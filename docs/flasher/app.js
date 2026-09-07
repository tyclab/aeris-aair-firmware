'use strict';

const USB_VENDOR = 0x2b04;
const PID_NORMAL = 0xc006;
const PID_DFU = 0xd006;
const PLATFORM_PHOTON = 6;
const DFU_ALT_INTERNAL_FLASH = 0;

const FLASH = {
  system_part1: 0x08020000,
  system_part2: 0x08060000,
  application: 0x080a0000,
};

// The only gate on a DFU write target. The bootloader at 0x08000000 must never be one:
// a wrong bootloader bricks the unit beyond DFU recovery.
function checkedAddress(addr) {
  if (!Object.values(FLASH).includes(addr)) {
    throw new Error(`refusing to write to 0x${addr.toString(16).padStart(8, '0')}`);
  }
  return addr;
}

// 14400 baud flips the unit into DFU and 28800 into listening mode; anything else is plain serial.
const SERIAL_BAUD = 9600;

const $ = (id) => document.getElementById(id);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

/* ---------------------------------------------------------------- log */

function log(message, kind) {
  const line = document.createElement('p');
  if (kind) {
    line.className = kind;
  }
  const stamp = document.createElement('time');
  stamp.textContent = new Date().toLocaleTimeString([], { hour12: false });
  line.append(stamp, document.createTextNode(message));
  $('log').append(line);
  $('log').scrollTop = $('log').scrollHeight;
}

/* -------------------------------------------------------------- steps */

const STATES = {
  waiting: 'waiting',
  active: 'working',
  done: 'done',
  error: 'failed',
  skipped: 'skipped',
};

function step(name, status) {
  const li = document.querySelector(`.steps li[data-step="${name}"]`);
  li.dataset.status = status;
  li.querySelector('.state').textContent = STATES[status];
}

function resetSteps() {
  document.querySelectorAll('.steps li').forEach((li) => {
    li.dataset.status = 'waiting';
    li.querySelector('.state').textContent = STATES.waiting;
  });
}

/* ----------------------------------------------------------- manifest */

let manifest = { versions: [] };

async function loadManifest() {
  const res = await fetch('manifest.json', { cache: 'no-store' });
  if (!res.ok) {
    throw new Error(`manifest.json: HTTP ${res.status}`);
  }
  manifest = await res.json();
  const select = $('version');
  manifest.versions.forEach((v, i) => {
    const opt = document.createElement('option');
    opt.value = String(i);
    opt.textContent = `${v.name} (Device OS ${v.device_os})`;
    select.append(opt);
  });
  log(`manifest loaded, ${manifest.versions.length} version(s)`);
}

async function fetchBinary(path) {
  const res = await fetch(path, { cache: 'no-store' });
  if (!res.ok) {
    throw new Error(`${path}: HTTP ${res.status}`);
  }
  return new Uint8Array(await res.arrayBuffer());
}

/* -------------------------------------------------------------- flash */

function setProgress(label, done, total) {
  $('progress').hidden = false;
  const pct = total > 0 ? Math.min(100, Math.round((done / total) * 100)) : 0;
  $('progress-fill').style.width = `${pct}%`;
  $('progress-text').textContent = `${label} ${pct}%`;
}

function progressHandler(label) {
  let phase = '';
  let total = 0;
  let done = 0;
  return (ev) => {
    switch (ev.event) {
      case 'start-erase':
        phase = 'erasing';
        total = ev.bytes;
        done = 0;
        break;
      case 'start-download':
        phase = 'writing';
        total = ev.bytes;
        done = 0;
        break;
      case 'erased':
      case 'downloaded':
        done += ev.bytes;
        break;
      case 'complete-download':
        done = total;
        break;
      case 'failed-download':
        phase = 'failed';
        break;
      default:
        return;
    }
    setProgress(`${label} ${phase}`, done, total);
  };
}

async function writePart(dfuDev, name, addr, data, leave) {
  log(`writing ${name}: ${data.length} bytes at 0x${addr.toString(16)}`);
  await dfuDev.writeOverDfu(data, {
    altSetting: DFU_ALT_INTERNAL_FLASH,
    startAddr: checkedAddress(addr),
    noErase: false,
    leave,
    progress: progressHandler(name),
  });
  log(`${name} written`, 'ok');
}

async function findPermittedDfuDevice(deviceId) {
  const devices = await navigator.usb.getDevices();
  return devices.find(
    (d) =>
      d.vendorId === USB_VENDOR &&
      d.productId === PID_DFU &&
      (!deviceId || (d.serialNumber || '').toLowerCase() === deviceId),
  );
}

function waitForClick(button) {
  return new Promise((resolve) => button.addEventListener('click', resolve, { once: true }));
}

async function collectParts() {
  const version = manifest.versions[Number($('version').value)];
  const parts = [];

  if ($('advanced').checked) {
    if (!version) {
      throw new Error('system parts need a manifest version selected');
    }
    parts.push({
      name: 'system-part1',
      addr: FLASH.system_part1,
      data: await fetchBinary(version.system_part1),
    });
    parts.push({
      name: 'system-part2',
      addr: FLASH.system_part2,
      data: await fetchBinary(version.system_part2),
    });
  }

  const own = $('own-bin').files[0];
  if (own) {
    parts.push({
      name: own.name,
      addr: FLASH.application,
      data: new Uint8Array(await own.arrayBuffer()),
    });
  } else {
    if (!version) {
      throw new Error('no firmware selected');
    }
    parts.push({
      name: version.name,
      addr: FLASH.application,
      data: await fetchBinary(version.app),
    });
  }
  return parts;
}

async function flash() {
  resetSteps();
  $('progress').hidden = true;
  $('flash').disabled = true;

  let dfuNative = null;
  let deviceId = null;

  try {
    const parts = await collectParts();

    // A unit left in DFU by an earlier run is already permitted, so neither chooser is needed.
    dfuNative = await findPermittedDfuDevice(null);

    if (dfuNative) {
      log('unit already in DFU mode');
      step('connect', 'skipped');
      step('dfu', 'skipped');
      step('authorize', 'done');
    } else {
      step('connect', 'active');
      const native = await navigator.usb.requestDevice({
        filters: [{ vendorId: USB_VENDOR, productId: PID_NORMAL }],
      });
      const dev = await window.ParticleUsb.openNativeUsbDevice(native, {});
      if (dev.platformId !== PLATFORM_PHOTON) {
        await dev.close();
        throw new Error(`not a Photon: platform id ${dev.platformId}`);
      }
      deviceId = dev.id;
      log(`connected to Photon ${deviceId}`);
      step('connect', 'done');

      step('dfu', 'active');
      if (dev.isInDfuMode) {
        log('device is already in DFU mode');
        step('dfu', 'skipped');
      } else {
        await dev.enterDfuMode({ noReconnectWait: true });
        await sleep(2000);
        log('device rebooted into DFU mode');
        step('dfu', 'done');
      }
      try {
        await dev.close();
      } catch (err) {
        log(`closing the normal-mode handle: ${err.message}`);
      }

      step('authorize', 'active');
      for (let i = 0; i < 6 && !dfuNative; i++) {
        dfuNative = await findPermittedDfuDevice(deviceId);
        if (!dfuNative) {
          await sleep(500);
        }
      }
      if (!dfuNative) {
        log('the DFU device needs its own authorisation, use the button below');
        $('authorize').hidden = false;
        await waitForClick($('authorize'));
        $('authorize').hidden = true;
        dfuNative = await navigator.usb.requestDevice({
          filters: [{ vendorId: USB_VENDOR, productId: PID_DFU }],
        });
      }
      step('authorize', 'done');
    }

    step('write', 'active');
    const dfuDev = await window.ParticleUsb.openNativeUsbDevice(dfuNative, {});
    if (!dfuDev.isInDfuMode || dfuDev.platformId !== PLATFORM_PHOTON) {
      await dfuDev.close();
      throw new Error('the selected device is not a Photon in DFU mode');
    }

    for (let i = 0; i < parts.length; i++) {
      const part = parts[i];
      const last = i === parts.length - 1;
      await writePart(dfuDev, part.name, part.addr, part.data, last);
    }
    step('write', 'done');
    step('leave', 'done');
    log('flash complete, the unit is rebooting', 'ok');

    try {
      await dfuDev.close();
    } catch (err) {
      log(`the device re-enumerated before close: ${err.message}`);
    }
  } catch (err) {
    document.querySelectorAll('.steps li[data-status="active"]').forEach((li) => {
      step(li.dataset.step, 'error');
    });
    $('authorize').hidden = true;
    log(err.message, 'err');
  } finally {
    $('flash').disabled = false;
  }
}

/* ---------------------------------------------------------- provision */

let port = null;
let reader = null;
let streamClosed = null;
let rx = '';

async function readLoop() {
  const decoder = new TextDecoderStream();
  streamClosed = port.readable.pipeTo(decoder.writable);
  reader = decoder.readable.getReader();
  try {
    for (;;) {
      const { value, done } = await reader.read();
      if (done) {
        return;
      }
      rx += value;
    }
  } finally {
    reader.releaseLock();
  }
}

async function send(text) {
  const writer = port.writable.getWriter();
  try {
    await writer.write(new TextEncoder().encode(text));
  } finally {
    writer.releaseLock();
  }
}

async function waitForRx(pattern, timeoutMs) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const match = rx.match(pattern);
    if (match) {
      return match;
    }
    await sleep(100);
  }
  return null;
}

function serialButtons(connected) {
  $('serial-connect').disabled = connected;
  $('provision').disabled = !connected;
  $('ask-ip').disabled = !connected;
  $('serial-disconnect').disabled = !connected;
}

async function serialConnect() {
  try {
    port = await navigator.serial.requestPort({ filters: [{ usbVendorId: USB_VENDOR }] });
    await port.open({ baudRate: SERIAL_BAUD });
    rx = '';
    readLoop().catch((err) => log(`serial read stopped: ${err.message}`, 'err'));
    serialButtons(true);
    log(`serial port open at ${SERIAL_BAUD} baud`, 'ok');
  } catch (err) {
    log(err.message, 'err');
  }
}

async function serialDisconnect() {
  try {
    if (reader) {
      await reader.cancel();
    }
    if (streamClosed) {
      await streamClosed.catch(() => {});
    }
    await port.close();
    log('serial port closed');
  } catch (err) {
    log(err.message, 'err');
  } finally {
    port = null;
    reader = null;
    streamClosed = null;
    serialButtons(false);
  }
}

function result(text, kind) {
  const el = $('prov-result');
  el.textContent = '';
  el.className = `result ${kind}`;
  el.append(document.createTextNode(text));
  return el;
}

async function provision() {
  const fields = [
    $('ssid').value,
    $('wifi-pass').value,
    $('mqtt-host').value,
    $('mqtt-port').value,
    $('mqtt-user').value,
    $('mqtt-pass').value,
    $('topic-root').value,
    $('device-id').value,
  ];
  if (fields.some((f) => f === '')) {
    result('every field is required', 'err');
    return;
  }
  if ($('ota-pass').value !== '') {
    fields.push($('ota-pass').value); // optional ninth field; without it Wi-Fi updates stay off
  }
  // Carries the Wi-Fi, MQTT and update secrets: never logged, never echoed into the page.
  const line = `PROV\t${fields.join('\t')}\n`;

  $('provision').disabled = true;
  try {
    for (let attempt = 1; attempt <= 5; attempt++) {
      rx = '';
      await send('x'); // leave listening mode; its console owns the same port
      await sleep(300);
      await send('\n'); // flush any stray characters out of the unit's line buffer
      await sleep(300);
      await send(line);
      log(`provisioning line sent, attempt ${attempt}`);

      const match = await waitForRx(/PROV (OK|ERR)([^\r\n]*)/, 8000);
      if (match && match[1] === 'OK') {
        log('PROV OK, the unit is rebooting', 'ok');
        result('PROV OK — the unit is rebooting onto your network.', 'ok');
        return;
      }
      if (match) {
        log(`PROV ERR${match[2]}`, 'err');
        result(`PROV ERR${match[2]}`, 'err');
        return;
      }
      log(`attempt ${attempt}: no reply`);
    }
    result('Gave up after 5 attempts — is the unit in setup mode on this port?', 'err');
  } catch (err) {
    log(err.message, 'err');
    result(err.message, 'err');
  } finally {
    $('provision').disabled = false;
  }
}

async function askIp() {
  $('ask-ip').disabled = true;
  try {
    rx = '';
    await send('x'); // leave listening mode; its console owns the same port
    await sleep(300);
    await send('\n'); // x is unframed, so flush it out of the unit's line buffer
    await sleep(300);
    await send('IP?\n');
    const match = await waitForRx(/(\d{1,3}(?:\.\d{1,3}){3})/, 8000);
    if (!match) {
      log('no address reported', 'err');
      result('No address reported — is the unit on the network yet?', 'err');
      return;
    }
    const url = `http://${match[1]}/`;
    log(`unit reports ${match[1]}`, 'ok');
    const link = document.createElement('a');
    link.href = url;
    link.textContent = url;
    link.rel = 'noreferrer';
    result('Unit address: ', 'ok').append(link);
  } catch (err) {
    log(err.message, 'err');
  } finally {
    $('ask-ip').disabled = false;
  }
}

/* --------------------------------------------------------------- boot */

function main() {
  if (!navigator.usb || !navigator.serial) {
    $('unsupported').hidden = false;
    $('flash-card').hidden = true;
    $('prov-card').hidden = true;
    return;
  }

  $('flash').addEventListener('click', flash);
  $('serial-connect').addEventListener('click', serialConnect);
  $('serial-disconnect').addEventListener('click', serialDisconnect);
  $('provision').addEventListener('click', provision);
  $('ask-ip').addEventListener('click', askIp);

  loadManifest().catch((err) => log(err.message, 'err'));
}

main();
