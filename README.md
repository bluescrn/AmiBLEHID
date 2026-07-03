# AmiBLEHID

Work in progress, but now fairly functional. Needs a better name.

Latest iteration, v4 PCB with extra diodes to enable experimental CD32 protocol support

![Pic of prototype](/assets/images/prototype4.jpeg)

## What is it?

With minimal other components (essentially just a [74HC05](https://www.build-electronic-circuits.com/7400-series-integrated-circuits/74hc05-74ls05/) and the joystick port connector), an ESP32-H2 module can be connected to an Amiga joystick/mouse port, and allow a BLE mouse or controller to be used with the Amiga.

There is an existing project, [Unijoysticle](https://github.com/ricardoquesada/unijoysticle2), that uses an ESP32-WROOM32 and provides compatibility with far more devices by supporting Bluetooth Classic, but it's a relatively bulky board and I believe it requires external power. This is an attempt to build a smaller neater single-port 'dongle' using the same approach, albeit with more limited functionality.

The H2 has no wifi, and only supports BLE (no Bluetooth Classic). Other ESP32 modules likely draw too much current. With the latest revision, I measured power consumption as between 22mA and 34mA, depending on the amount of BLE activity.

![Measuring power consumption](/assets/images/powerconsumption.jpeg)

## Features

- Mouse speed control (cycle 5 levels via button on board, or hold middle mouse button)
- Mouse emulation using right analog stick when using a controller and Amiga joystick port 1
- 'Up-to-jump' mode - Maps the 2nd button to joystick up
- Experimental CD32 protocol support (7 buttons)
 
## Code

The code side of things is based around [NimBLE Arduino](https://docs.arduino.cc/libraries/nimble-arduino/) and [this HID report parsing code](https://github.com/pasztorpisti/hid-report-parser), and can be built in the Arduino IDE

Had to add a little bodge to the HID parser to read the hat switch (d-pad) properly, as the neutral position of a hat switch (usually 0 or -1 depending on device) is outside the logical_min/max range of the axis, and the parser was ignoring out-of-range values.

## Limitations

BLE game controllers are very uncommon, the only common+good ones that I'm aware of are the latest model of Xbox controller and the 8BitDo Ultimate Wireless 2.
  
~~Only supports two joystick/mouse buttons~~. Now has experimental CD32 protocol support. Functional enough for casual gaming, but a little bit glitch (dropped/spurious button presses).

The USB-C port on the ESP32 module isn't currently supported for wired mice/controllers. Would be nice to support wired controllers, but the additional current draw is likely to be too much for the Amiga joystick port

## Tested+Compatible devices

Amigas
- Only tested on an A1200. Have tested with two of them and so far it seems OK (see [this thread](https://eab.abime.net/showthread.php?t=98463) for some discussion of joystick port current limits)
- Probably won't fit an A600. Space around the joystick ports looks very tight? (could use a joystick extension cable maybe, but that defeats the goal of a small+neat dongle)

Controllers:
- Xbox Series S/X controller (this is the version with USB-C and the extra 'share' button in the centre. Previous versions aren't BLE)
- 8BitDo Ultimate 2 Wireless

Mice:
- KeyChron M3 Wireless Mouse
- Logitech MX Anywhere 3
- Razer Atheris

## Status LED
Fast blue blinking - BLE pairing (Hold reset button for ~3 secs until both LEDs flash orange to enter pairing mode)
Slow blue blinking - BLE scanning/connecting (to paired device)
Red - BLE scanning timed out, not connected
Cyan   - Mouse connected
Green  - Controller connected, Regular 2-button mode
Purple - Controller connected, CD32 mode (when the Amiga software is treating it as a CD32 controller)

Status LED brightness will increase slightly when a mouse button or controller button A/B is held. Mostly to show it's connected. (Might be able to use it to approximately measure latency by pointing a camera at it and recording the LED and button being pressed?)

## Mode LED
With a mouse connected, the mode LED will cycle between green (slowest) and red (fastest) as you cycle through mouse speed (middle mouse button, or Mode button on the board)

With a controller connected, the mode LED will switch between green (normal) and yellow (up-to-jump mode) when the mode is switched (start button or Mode button). Up-to-jump mode won't work if the controller is being read as a CD32 pad.

## CD32 Protocol

A real CD32 controller contains a shift register chip, used to output the state of 7 buttons serially, when pin 9 is pulled low and pin 6 (Fire 1) is pulsed as a clock. During this process the button state is output to pin 9 (Fire 2)

There is an initial attempt to support this. The status LED will switch from green to purple when CD32 button reading is detected (pin 9 changing level).

But emulating the shift register behaviour in code requires quite responsive interrupts, and regular ESP32 interrupts (written in C and via the Arduino ESP32 framework) on the H2 (single-core 96MHz RISC-V) aren't quite fast/stable enough for this. While it's mostly functional, there are currently some dropped and spurious button inputs picked up due to the interrupts being delayed.

In theory, this should be resolvable in software by moving the codebase to ESP-IDF and using a high-priority interrupt - but this requires the interrupt handler to be written in assembly language. But currently I've got no experience with ESP-IDF or RISC-V assembly.

I've been testing CD32 support with [Joytest_CP](https://aminet.net/package/driver/input/joytest_CP) and also with [Cecconoid](https://thalamusdigital.itch.io/cecconoid-amiga).

([JoyPortTest](https://aminet.net/package/driver/input/JoyPortTest.lha) doesn't pick up CD32 buttons. Not sure why, maybe it's trying to read too fast as I'm using an A1200 with a PiStorm?)

## USB controllers?

The ESP32-H2 board has a USB-C connector. It would be nice if it could also support wired USB controller, but I don't think the ESP32-H2 can function as a USB host, and if it could, power draw would likely be too much (especially controllers with rumble motors and RGB lighting). So don't try plugging anything in when it's connected to the Amiga unless you're connecting it to a PC for programming/debug output.

## Mouse Wheels?

Currently only 2 mouse buttons are supported, via the Fire1+Fire2 pins. I'm somewhat aware of atttempts to support mouse wheels on Amiga, and seen references to the Cocolino protocol, but no real documentation of it.

One thought I'd had is that it would make sense to re-use the well-understood CD32 protocol to output additional mouse data, that gives 7 bits polled once per frame. Maybe use 4 bits as a signed value for the wheel movement, leaving 3 bits free for additional buttons. Or just treating wheel up/down as buttons if one 'wheel tick' every 1/30th sec turns out to be enough?

But that'd require a custom driver, likely a modified version of the [TankMouse]( https://aminet.net/package/driver/input/TankMouse) driver

But there's no point attempting any of that unless I can get more robust CD32 support working by using high-priority interrupts...

## BOM

|Qty|Component|
|--|----|
|1x|[Waveshare ESP32-H2 Zero](https://www.waveshare.com/esp32-h2-zero.htm)|
|1x|74HC05 open-collector hex inverter (SOIC)|
|2x|Micro tactile pushbutton switch (4.5x4.5x4.5mm)|
|2x|WS2812B RGB LEDs (3535 size)|
|1x|9-pin female D-Sub connector (solder bucket connections)|
|2x|1N4148 diodes (SOD-123)|
|1x|470 ohm resistor (3216 Metric)|

The LEDs are likely the trickiest part to source as the larger 5050 size are more common. I'd bought a load from AliExpress for another project so already had some.


