# AmiBLEHID - Work in progress

## What is it?

With minimal other components (essentially just a 74HC05 and the joystick port connector), an ESP32-H2 module can be connected to an Amiga joystick/mouse port, and allow a BLE mouse or controller to be used with the Amiga.

The H2 has no wifi, and only supports BLE (no Bluetooth Classic). Other ESP32 modules likely draw too much current (With the Waveshare ESP32-H2-Zero and my initial code, I measured 20-23mA with am mouse connected, and 26mA while scanning)

There is an existing project, [Unijoysticle](https://github.com/ricardoquesada/unijoysticle2), that uses an ESP32-WROOM32 and provides compatibility with far more devices by supporting Bluetooth Classic, but it's a relatively bulky board and requires external power. This is an attempt to build a smaller neater 'dongle' using the same approach, albeit with more limited functionality.

## Code

The code side of things is based around [NimBLE Arduino](https://docs.arduino.cc/libraries/nimble-arduino/) and [this HID report parsing cod](https://github.com/pasztorpisti/hid-report-parser)

## Limitations

BLE game controllers are very uncommon, the only common one that I'm aware of is the latest model of Xbox controller, which works well. Would love to know if there's any more good ones out there.
  
Only supports two joystick/mouse buttons. Supporting the CD32 button protocol would significantly complicate things. And using a single 74HC05 limits it to 6 outputs at the moment (4 directions, 2 buttons)

The USB-C port on the ESP32 module isn't currently supportedd for wired mice/controllers. Would be nice to support that, but the additional current draw may well be too much for the Amiga joystick port
