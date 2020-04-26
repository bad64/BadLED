[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

# DIY RGB LED system for mechanical switch based buttons

For all you lovely people who like to RGB their arcade stick, but also can't/don't want to use Sanwa buttons and Kaimana rings. (Not that there is anything wrong with those to begin with !)

# What currently works

* The firmware
* The desktop app

# What needs to be done

* The mobile app
* Release packages

# What should ideally be done

* Firmware
    * ~~Better out-of-box stick support (through reporting button names from the hardware), currently the software as-is only supports 8 button sticks with 4 joystick LEDs~~ Will likely not be done due to this project being basically tailor made for the Brook boards as the Nano breakout plugs directly onto the Brook header
* Software
    * Some way to set the `USE_LOOPBACK` and `LEDS_STATE` flags through the GUI
    * The ability to save and load color presets
* Documentation

# Releases

Coming SOON™

# How to use

* Download the Gerber files that you need, order the PCBs from wherever you want (or make them yourself if you can/want to)
* Download the desktop application (when it becomes available) and launch it
* Pick a button
* Assign RGB values to be used when the button is pressed and not pressed
* Repeat steps 2 to 4 as much as necessary
* Upload and make your fightstick visible from outer space

# And if I want to scrutinize every byte of your code instead of downloading precompiled stuff ?

Go ahead ! I won't stop you.  
The desktop app is set up to be compiled from a Python virtual environment, though nothing prevents you from running it as-is.

# (Probable) FAQs:

**Q: Why not use a normal Kaimana like everyone else ?**  
A: The Kaimana rings are pretty much only compatible with Sanwa buttons, and I run Crown 202c buttons, so it was that or hot glue. And shipping costs were a bit much.

**Q: Are the rings compatible with a Kaimana ?**  
A: They are ! They use WS2812B RGB LEDs, same as the normal Kaimana rings. (The firmware, however, might need some tweaking to work)

**Q: What about the Nano breakout ? Can I use it with normal Kaimana rings ?**  
A: Pretty sure you can ! They use the same LEDs and protocol.

**Q: On which buttons do the rings fit ?**  
A: They were measured so as to fit Crown 202c 30mm buttons. If you're willing to use hot glue, you can definitely put them under 30mm GamerFingers too, and they're likely too wide for Sanwas. I have no idea if they fit on Seimitsu buttons without hot glue however.

**Q: Why Python and not like C# or something else ? I hate interpreted languages !**  
Short A: Because Trolltech screwed up.  
Longer A: I originally wanted a cross-platform GUI since I work a lot on Linux, and having prior knowledge of Qt, decided to use that with some C++. However, what happened instead was that I discovered Trolltech made it semi-mandatory to sign up with an account in order to install the Qt SDK (~~or I could cut internet during installation, which I learned only after I was well underway with the Python version~~). I also found out that, for some reason, PyQt didn't have this problem, so I pretty much ran with it. (Also serial comms in C++ is a nightmare compared to doing it in Python)
