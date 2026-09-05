# vendor/particle-usb.bundle.js

Browser build of [particle-usb](https://github.com/particle-iot/particle-usb)
**4.3.0** (Apache-2.0), exposing `window.ParticleUsb`.

Rebuild:

```bash
npm init -y
npm i particle-usb@4.3.0 @particle/device-constants@4.1.1 buffer@6.0.3 events@3.3.0
printf "export { Buffer } from 'buffer';\n" > buffer-shim.js
printf "export * from 'particle-usb';\n"    > entry.js
npx esbuild@0.24.0 entry.js \
  --bundle --format=iife --global-name=ParticleUsb --platform=browser \
  --inject:./buffer-shim.js --minify \
  --outfile=particle-usb.bundle.js
```

`--platform=browser` honours particle-usb's `browser` field, which swaps
`src/usb-device-node.js` for `src/usb-device-webusb.js`; the native `usb`
package never reaches the bundle. `--inject` supplies the free `Buffer`
identifier the way upstream's `webpack.ProvidePlugin` does — a top-level
assignment in the entry runs too late and fails at call time in
`usb-device-webusb.js`. `events` and the `@particle/device-constants` peer
dependency must be installed explicitly; esbuild polyfills neither.

Bundled dependencies and their licences:

| Package                       | Version | Licence                            |
| ----------------------------- | ------- | ---------------------------------- |
| particle-usb                  | 4.3.0   | Apache-2.0                         |
| @particle/device-constants    | 4.1.1   | `UNLICENSED` as published by Particle |
| buffer                        | 6.0.3   | MIT                                |
| events                        | 3.3.0   | MIT                                |
| protobufjs, ip-address, jsbn, sprintf-js, base64-js, ieee754 | transitive | BSD-3-Clause / MIT |
