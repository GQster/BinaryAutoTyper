# BinaryAutoTyper
#### A little desk gizmo that converts key strokes to binary then back to ASCII for your delight! 



Currently working on the [CAD](https://cad.onshape.com/documents/a23f21f7142f74b01f919613/w/19e0e9d361a04868ba1185da/e/0e3bc4b0d31ba26d3ed0fc04?renderMode=0&uiState=678027edc156f43ca85e5772)








![Alt Text](imgs/finished.jpg)

[RP2040 Info](https://www.waveshare.com/wiki/RP2040-Zero)

### BOM:
  - 1 rp2040-zero
  - 2 Mechanical keyboard switches
  - 2 key caps
  - some wire
  - 3D printed housing

### TOOLS:
  - Solder
  - Soldering iron

### Software needed:
  - [Circuit Python](https://circuitpython.org/board/waveshare_rp2040_zero/   ) ([how to install](https://learn.adafruit.com/getting-started-with-raspberry-pi-pico-circuitpython/circuitpython))
  - Serial terminal for testing ([online version I used](https://www.serialterminal.com/advanced_terminal/src/html/index.html))



### Steps:
- Install circuitpython to the RP2040
- Copy the `adafruit_debouncer.py` & `adafruit_ticks.py` to the root folder. ( I got ticks from [here](https://github.com/adafruit/Adafruit_CircuitPython_Ticks/blob/main/adafruit_ticks.py) and debouncer from [here](https://github.com/adafruit/Adafruit_CircuitPython_Debouncer/blob/main/adafruit_debouncer.py) ) (Did this as I couldnt get the lib for these to work)
- In the adafruit-circuitpython*****.zip [I got from here](https://circuitpython.org/libraries), copy the `lib/adafruit_hid` folder to the rp2040's lib folder.
- Copy the `code.py` [file](https://github.com/GQster/rp240ZeroMacroPad/blob/master/code.py) to the root of the rp2040



**Wire it up:**
- Solder one side of each key to a GND pin (I used pin 2)
- Solder the other pin of each key to a GPIO pin. I used 2, and 3 (avoiding the UART pins). Make sure you update the code with your pins:


`PINS = (
    board.GP2,
    board.GP3
)`
![Alt Text](imgs/internals.jpg)



To change what the keys do you will want to edit :

    KEYMAP = (
        ("Paste", [MODIFIER, Keycode.V]),
        ("Copy", [MODIFIER, Keycode.C]),
        ("Select all", [MODIFIER, Keycode.A])
    )

[Here is the documentation for the key codes](https://docs.circuitpython.org/projects/hid/en/latest/)



## NOTE:
I was haveing issues importing the libraries correctly and didn't want to spend the time to fix it. So I just coppied the `adafruit_ticks` and `adafruit_debounce` files into `code.py`. This work fine, but is messy. 



### References:
I took inspairation (and the stl files) from [this instructable](https://www.instructables.com/Macropad-for-Keyboard-Shortcuts/) but swapped to a RP2040 as it is smaller and I had a few already.
