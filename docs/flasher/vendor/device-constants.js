// Stand-in for particle-usb's `@particle/device-constants` peer dependency,
// which npm publishes UNLICENSED. Only the fields particle-usb reads, only
// the Photon: the ids are the device's own USB descriptors.
const photon = {
  id: 6,
  name: 'photon',
  displayName: 'Photon',
  generation: 2,
  features: ['wifi', 'tcp'],
  usb: { vendorId: '0x2b04', productId: '0xc006' },
  dfu: { vendorId: '0x2b04', productId: '0xd006' },
};
const platforms = { photon };
const platformsById = { 6: photon };
const PlatformId = { PHOTON: 6 };
// edl-device.js (Tachyon only) touches these at call time, never for a Photon.
const linux = { machineIdToDeviceId() { throw new Error('unsupported platform'); } };
module.exports = { platforms, platformsById, PlatformId, linux };
