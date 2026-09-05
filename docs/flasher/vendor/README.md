# vendor/particle-usb.bundle.js

Browser build of [particle-usb](https://github.com/particle-iot/particle-usb)
**4.3.0** (Apache-2.0), exposing `window.ParticleUsb`. The DFU flow in
`app.js` (normal-mode connect, `enterDfuMode`, then a second `requestDevice`
for the re-enumerated DFU product id &mdash; skipped when that id is already
permitted) follows the pattern Particle's own browser restore tool uses
against this same library.

Rebuild:

```bash
npm init -y
npm i particle-usb@4.3.0 buffer@6.0.3 events@3.3.0
printf "export { Buffer } from 'buffer';\n" > buffer-shim.js
printf "export * from 'particle-usb';\n"    > entry.js
npx esbuild@0.24.0 entry.js \
  --bundle --format=iife --global-name=ParticleUsb --platform=browser \
  --inject:./buffer-shim.js --minify \
  --alias:@particle/device-constants=./device-constants.js \
  --outfile=particle-usb.bundle.js
```

`--platform=browser` honours particle-usb's `browser` field, which swaps
`src/usb-device-node.js` for `src/usb-device-webusb.js`; the native `usb`
package never reaches the bundle. `--inject` supplies the free `Buffer`
identifier the way upstream's `webpack.ProvidePlugin` does — a top-level
assignment in the entry runs too late and fails at call time in
`usb-device-webusb.js`. `events` must be installed explicitly; esbuild does not polyfill it.
`--alias` points particle-usb's `@particle/device-constants` peer dependency
at `device-constants.js` here: npm publishes that package `UNLICENSED`, so it
is not redistributed. The stand-in carries the Photon entry only (platform id,
generation, features, USB and DFU product ids), which is every field
particle-usb reads.

Bundled dependencies and their licences:

| Package                       | Version | Licence                            |
| ----------------------------- | ------- | ---------------------------------- |
| particle-usb                  | 4.3.0   | Apache-2.0                         |
| buffer                        | 6.0.3   | MIT                                |
| events                        | 3.3.0   | MIT                                |
| protobufjs, ip-address, jsbn, sprintf-js, base64-js, ieee754 | transitive | BSD-3-Clause / MIT |
