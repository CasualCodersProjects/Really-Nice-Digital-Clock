# Digital Clock Hardware

## Notice:
There is a hardware change between the 8 clocks sold on Tindie and these. The original production clocks from tindie are:  
- Missing a copper trace between the 3v3 line and the 3v3 output of the LDO. This is corrected with a hand-assembled solder bridge.  
- The I2C lines are shifted right by one pin. The tindie version uses GPIO21 for SDA and GPIO33 for SCL.  
- The Tindie clocks have a manually soldered bodge to move one of the binary coded digits off GPIO 46, which is an input only strapping pin.

## Description
This is an internet connected Clock which synchronizes with NTP servers to perpetually serve an accurate time. It is unable to keep very good time by itself. As such, it requires a stable, consistent network connection to show reliable time. With that, it should always be acctute within a second or so. To improve accuracy, you could modify the firmware to check every hour or half hour instead of once per day.  

It's built using BCD Decoders and 1.5" Seven segment displays for a nice clean look. It also utilizes an ambient light sensor to automatically brighten and dim to match the room light.  

This clock serves it's own webpage for configuration. Upon first boot, it activates an access point ssid `NiceClock`, password `MinesBigger`. Connect to the default gateway `192.168.4.1` to configure the clock and provide network credentials. Once credentials are provided, it should instantly connect and acquire the network time.  
To view the device's IP address, press the boot button on the rear.

## Hacking
This device openly exposes both a TTL UART and JTAG port on the rear of te board, near the ESP Module. You can also use the USB Port to program and upload code. While holding the boot button, then press reset. This enters the ESP32 into bootloader mode, and allows you to reflash new firmware or filesystem to the device.