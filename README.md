# ESPhome component for Electriq 12000 BTU WiFi Smart AC

This custom component for ESPhome replaces the Tuya firmware within the ESP8266 wifi module found inside [Electriq](https://www.electriq.co.uk) branded air conditioning units for integration into Home Assistant. Developed on the 12000 Smart model, others may also be supported.

## Introduction

The Electriq 12000 BTU WiFi Smart AC with Heat Pump is a portable AC unit sold by (and a brand name of?) https://www.appliancesdirect.co.uk

It includes a Tuya-manufactured WiFi module that communicates via a serial protocol with unit's MCU. 'Smart' control is via the Tuya Smart app, via a Chinese companies cloud infrastructure. No thank-you!

While Tuya have a [well-documented protocol](https://developer.tuya.com) for incorporating their technology into third party products, supported by ESPhome and Tasmota (Tuya MCU), this AC unit doesn't utilise that protocol. While there are similarities, this protocol uses fixed-length messages without variable data fields, and a single byte header. Only the baud rate and checksum byte, and concept of heartbeat messages match the published protocol. Fortunately, this protocol was simple to reverse engineer.

Perhaps this is an older firmware, or a proprietary / lightweight protocol running alongside the documented one? I have two units both purchased during 2021. For the curious, the firmware's boot messages include the output:

```
[N]user_main.c:313 SDK version:2.1.1(317e50f)
[N]user_main.c:317 fireware info name:esp_gushang_aircon version:1.0.7
[N]user_main.c:319 tuya sdk compiled at May 20 2020 14:40:05
```
The protocol was examined using Sigrok/Pulseview and a [cheap logic analyser](https://hobbycomponents.com/testing/243-hobby-components-usb-8ch-24mhz-8-channel-logic-analyser).

## Protocol

9600 baud 8/N/1 5v UART.

Each byte position has a fixed purpose (unlike the published Tuya MCU protocol with varying data lengths).

*ESP > MCU*  
12 byte word

Two message types are sent. A heartbeat message is issued approx every 2 seconds. Only the header, version and checksum bytes are non-zero. This prompts the MCU to return its status.

The other message type commands the MCU into changing its operating mode, fan speed, set temperature, swing, Celsius or Fahrenheit display and sleep mode.

Byte 1 = Header, fixed 0xAA in both directions  
Byte 2 = Version(?) 0x02 for heartbeat, or 0x03 for command messages  
Byte 3 = Fan speed and mode. MSB represents fan speed and LSB the mode. Fan speed of 0x1n represents standby (off)  
Byte 4 = Swing, Celsius or Fahrenheit and Sleep mode  
Byte 5 = Target temperature  
Byte 6-8 = Unknown, 0x00  
Byte 9 = Unknown, 0x0B  
Byte 11 = Unknown, 0x00  
Byte 12 = Checksum (reminder of sum of all bytes / 256)  

*MCU > ESP*  
17 byte word

As above, with these differences:

Byte 2 = Version(?) 0x00  
Byte 9 = Temperature probe 1 (used as room temperature)  
Byte 10 = Temperature probe 2 (unused in Tuya app)  
Byte 11-12 = Unknown, 0x00  
Byte 13 = Action (Idle or active)  
Byte 14-16 = Unknown, 0x00  
Byte 17 = Checksum (reminder of sum of all bytes / 256)  

## Configuring Home Assistant

With ESPhome installed, create a new device of type ESP8266. Edit the device yaml file and paste in the contents of electriq-12000-ac.yaml, preserving the existing ota and ap passwords and device name. Copy the file electriq_ac.h into /config/esphome/ I use the "Terminal & SSH" addon to scp files around.

Click install > manual download, and ESPhome should compile and download the firmware image. The first installation has to be done physically, any subsequent builds can be transferred OTA.

## Initial flashing

This is where you need to break out the soldering iron :)

**Initial flashing is done with the module OUT of and DISCONNECTED FROM the air conditioning unit.**

Installed in my units are Tuya wifi modules model TYJW2S-5V-BL. Tuya has [good documentation](https://developer.tuya.com/en/docs/iot/wifijw2s5vmodule?id=K9605srhjahvz) on these. The programming pins are down the side, mine were unpopulated so I soldered a strip of pin headers for dupont cables. Connect TX, RX, GND 3.3v to a USB>TTL Serial converter (I use a cheap FTDI clone), ensuring it's set to 3.3v! While the MCU communications are 5v logic levels, applying 5v directly to the ESP chip will probably destroy it. Before plugging in the USB thus powering on the module, keep the reset button pressed. Plug in and release the button to put the ESP chip into bootloader mode.

Here is where you will want to learn from my mistake and BACK UP YOUR ORIGINAL FIRMWARE! I didn't, then I discovered this doesn't talk the published Tuya protocol, and that's pretty much why I now have two AC units... ;)

You can backup the firmware using [esptool.py](https://github.com/espressif/esptool) Ensure you download the version there, and don't rely on a version supplied by Ubuntu etc. I found that was lacking the 'stub' code required to read the flash memory. The command I used to backup the original flash is:

```
esptool.py -b 9600 read_flash 0 0x200000 tuya2M.bin
```
For some reason (probably that clone FTDI) I had to use 9600 baud else I got errors, but somehow writing at 115200 is just fine.

You may now flash the downloaded ESPhome firmware with your ESP tool-of-choice (mine is [Tasmotizer](https://github.com/tasmota/tasmotizer)). Reinstall the module.

Should you wish to restore the original Tuya firmware, you can, using:

```
esptool.py write_flash 0 tuya2M.bin
```

You can use the `verify_flash` option to ensure its correctly written back. However note that there's a unique ID embedded in this flash image. If you flash back to second device and pair that up, it will replace any existing pairing you had using the same flash image. Also be aware that flash image will contain your wifi password in clear text, if you had already paired it up.

## Usage

Once powered up, the device should register on your network and appear online within ESPhome's dashboard. You should be prompted to create a new device in home assistant, and create a default lovelace card. From here everything should work as you might expect. You can control the operating mode, fan speed, set temperature and swing mode from Home Assistant (I didn't yet bother implementing C/F switching, smart cool or sleep mode, but the UI would need modifying too). You should see the temperature readout (in Celsius), and the UI will report when the unit is 'off' vs when it is 'idle'. That is, turned on in heating or cooling modes but fan off (idle) while at set temperature.

## Further development

I should state somewhere, *I am not a programmer!* So while I've tried to keep the code clean and sane within my extremely limited abilities, I expect there's much room for improvement. I have no idea how to implement the "custom fan modes" so currently the only speeds available are 1, 3 and 5 (low/mid/high). There's also the possibility that due to who-knows-what, a command from Home Assistant might not make it to the module. I'm unsure why this *(only very rarely)* happens, but rather than checking for individual mode / fan / swing / temperature changes, perhaps the code could poll Home Assistant for all the states, and apply them where the desired state differs from the states reported by the MCU? Alternatively, this logic might be implemented in a HA automation too to ensure reported state == desired state.

Also interesting to note, these AC units have a silly design flaw in that while in heating mode, they don't overrun the fan when reaching the target temperature. The compressor and fan simply stop together (idle mode), and the latent heat in the evaporator causes the temperature to shoot up much higher than room temperature. Therefore the readout is misleading, and it's impossible to call for more heat without turning the temperature much higher, or simply waiting a number of minutes. The manufacturer should have designed the system to overrun the fan after turning off the compressor to remove the latent heat. The problem is noticeable within the Tuya app also, but the panel only displays the set temperature. Fortunately, we can now achieve this with a little automation, although that might be better left to an automation in Home Assistant to switch into fan mode for 10 seconds after switching state from heating to idle, then return to heat mode? Or perhaps and ESPhome lambda function in the yaml.