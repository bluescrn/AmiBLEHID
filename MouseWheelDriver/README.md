
TankMouse driver adapted for AmiBLEHID, which uses/abuses the CD32 controller protocol to get
extra button/wheel events to the Amiga.

Reads additional buttons from the mouse from a vblank intterup handler using the CD32 controller
protocol, and generates wheel/MMB events

Copy AmiBLEHIDMouseDriver to C: on the Amiga, and run it towards the end of Startup-Sequence with
C:AmiBLEHIDMouseDriver >NIL:

THANKS
======

Source code is based on the code by [nogginthenog](https://github.com/paulroberthill/AmigaPS2Mouse).
                                and [sq7bti](https://github.com/sq7bti/AmigaMouse/tree/tankmouse).

HOW IT WORKS?
=============

AmiBLEHID uses the CD32 button protocol to pass wheel and additional button data to the Amiga,
rather than attempting to implement the Cocolino protocol.

This protocol shifts out 7 bits of additional button data, usually used for the 7 buttons on a
CD32 controller. Reading 2 additional bits (always a 0 then a 1) after the 7 buttons is done to 
confirm a CD32 compatible
controller is present.

For now, the first button (yellow, bit 3) is the MMB and the L/R shoulder buttons (bits 5+6) are 
used for wheel down/up.

TROUBLESHOOTING
===============

In case you observed unexpected behavior, please run OS event monitor (e.g. [RawKeys](http://aminet.net/package/dev/moni/Rawkeys)) and verify if the input from mouse matches.
If desired, the direction of wheel mouse can be reversed by the argument option:

* VREV (abrev. V) - up/down

Lastly the middle mouse button enable option is also available, but does not make any use for TankMouse:
* NOMMB (abrev. N) - disable the use of middle mouse button


BUILDING THE CODE
===============
I used the Docker-based cross-compiling environment from https://github.com/AmigaPorts/docker-amiga-gcc,
which was straightforward to get running on Windows. You will also need the Amiga NDK

Partial testing in WinUAE is possible by reading buttons from a CD32 pad in port 1 instead of the actual
mouse in port 0 (See line ~40 of vbrserver.s)
