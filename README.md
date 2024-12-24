# LoRaWAN®-LED-Christmas-Tree

## Introduction

This repo contains the software for my LoRaWAN®-LED Christmas Tree project.
The goal is to build a small christmas tree shaped PCB with Neopixel LEDs as decoration.
The tree can be configured via LoRaWAN® downlinks,
as it implements a class C device and sends the ambient temperature and humidity via uplinks.

## General user manual

To get started, compile and flash the firmware to your RAK3172. Next, copy the DevEUI, JoinEUI and AppKEY Printed to the console and add the Christmas Tree to your LNS. This is also a good time to add the payload de-/encoder for the LoRaWAN® Christmas tree to your LNS.

Insert some batteries and turn on the power switch. (SW1. The tree is on when the switch is pushed down.) SW2 is the reset button, and SW3 allows you to manually toggle through the modes described in the [downlink payload description](#mode) below.

When turned on the tree should join and send a message every 10 minutes (default).

## Powering

To get the most out of the Christmas Tree, I highly recommend using high capacity AA batteries, such as [these](https://online-batterien.de/a/varta-industrial-pro-mignon-aa-batterie-4006-2er-folie/400084) or [these](https://online-batterien.de/a/energizer-ultimate-lithium-l91-mignon-aa-batterie-4er-blister/401064).

## Sensor description

The Christmas Tree has an integrated Sensirion SHT41-AD1F-R2 temperature and humidity sensor.
[Link to the sensor.](https://sensirion.com/products/catalog/SHT41-AD1F)

Here are the sensor specifications:

| Humidity                          | Value     |
|-----------------------------------|-----------|
| Typ. relative humidity accuracy   | 1.8 %RH   |
| Operating relative humidity range | 0-100 %RH |

| Temperature                 | Value        |
|-----------------------------|--------------|
| Typ. temperature accuracy   | 0.2 °C       |
| Operating temperature range | -40 - 125 °C |

## LoRaWAN® informations

If you use the module lists in the BOM, the frequency is Europe 868 MHz.
If you want to use other frequencies, change the part number accordingly. A list of all RAK3172 modules can be found [here](https://docs.rakwireless.com/product-categories/wisduo/rak3172-module/datasheet#ordering-information).

The Christmas Tree uses LoRaWAN® Specification 1.0.3 and Regional Parameters version RP001 Regional Parameters 1.0.3 Revision A.
In addition ADR is enabled.

## Ordering information

If you want to rebuild the project, you can find the Gerber file in the [/fabrication](/fabrication) folder as well as the BOM and pick and place file.

## LoRaWAN® Payload Description
### Uplinks

| Byte           | Byte 0                                                                          | Byte 1                                                | Byte 2                     | Byte 3                    |
|:---------------|:--------------------------------------------------------------------------------|:------------------------------------------------------|:---------------------------|:--------------------------|
| Payload        | `0x01` or `0xFF`                                                                | `0x02`                                                | e.g. `0x00`                | e.g. `0x01`               |
| Description    | `0x01` -> Successfully read SHT 45 <br/> `0xFF` -> Error while reading the SHT45| The next two bytes contain the firmware Version (LSB) | High byte firmware version | Low byte firmware version |

| Byte           | Byte 4                                                     | Byte 5                | Byte 6               | Byte 7                                                 | Byte 8             | Byte 9            |
|:---------------|:-----------------------------------------------------------|:----------------------|:---------------------|:-------------------------------------------------------|:-------------------|:------------------|
| Payload        | `0x03`                                                     | e.g. `0x09`           | e.g. `0x39`          | `0x04`                                                 | e.g. `0x0D`        | e.g. `0x25`       |
| Description    | The next two bytes contain the, measured temperature (LSB) | High byte temperature | Low byte temperature | The next two bytes contain the measured humidity (LSB) | High byte humidity | Low byte humidity |

| Byte           | Byte 10                                            | Byte 11             | Byte 12     | Byte 13     | Byte 14             |
|:---------------|:---------------------------------------------------|:--------------------|:------------|:------------|:--------------------|
| Payload        | `0x05`                                             | e.g. `0x00`         | e.g. `0x00` | e.g. `0x40` | e.g. `0x40`         |
| Description    | The next 4 bytes contain the battery voltage (LSB) | MSB battery voltage | -           | -           | LSB battery voltage |

| Byte 15                                        | Byte 16         | Byte 17     | Byte 18     | Byte 19         |
|:-----------------------------------------------|:----------------|:------------|:------------|:----------------|
| `0x06`                                         | e.g. `0x00`     | e.g. `0x09` | e.g. `0x27` | e.g. `0xC0`     |
| The next 4 bytes contain the tx interval (LSB) | MSB tx interval | -           | -           | LSB tx interval |

| Byte 20                                         | Byte 21      | Byte 22                                             | Byte 23            | Byte 24            |
|:------------------------------------------------|:-------------|:----------------------------------------------------|:-------------------|:-------------------|
| `0x07`                                          | e.g. `0x07`  | `0x08`                                              | e.g. `0x3A`        | e.g. `0x98`        |
| The next byte contains the current working mode | Working mode | The next two bytes contain the max brightness (LSB) | MSB max brightness | LSB max brightness |

### Downlinks

#### TX Interval
Set the transmission interval for status messages.
`0x00` + the transmission interval, you want to set in LSB format. <br/>
The value can be between 60 and 3600 seconds.

#### Max Brightness
Set the max brightness for the LEDs. This value will be used for all modes. <br/>
Note that when a static color is displayed, the brightness will only change, if the mode is reapplied.

`0x01` + the max brightness, you want to set in LSB format. <br/>
The value can be between 1 and 65535, although testing showed that a value of 15000 is perfectly adequate. <br/>
Note that the higher this value is, the higher will be the current consumption and therefore the battery will last shorter. <br/>

#### Mode
There are 12 working modes for the christmas tree.
0. Display the colors set via downlink command `0x03` (set individual LED colors).
1. Display the colors set via downlink command `0x04` (set all LED colors at once).
2. Display the custom animation, set via the downlink command `0x05`.
3. Set the LED color to red.
4. Set the LED color to green.
5. Set the LED color to blue.
6. Set the LED color to yellow.
7. Set the LED color to amber.
8. Set the LED color to magenta.
9. Set the LED color to cyan.
10. Show animation 1.
11. Show animation 2.
12. Show animation 3.

Send `0x02` + the mode index from above as one byte.

#### Individual LED control
This command allows you to control the color of an idividual LED. <br/>
Send `0x03` followed by one byte for the LED index (0-15) and three bytes. One for red, one for green and one for blue. So the values can be from `0` to `255` or from `0x00` to `0xFF`.

Note that, if you used the command `0x04` before `0x03` the frame from `0x04` will be copied! <br/>
So if for example, you've set all LEDs to yellow via `0x04` and now want to set LED 0 to blue and all other LEDs to black, simple send `0x04 0x00 0x00 0x00`
And then: `0x03 0x00 0x00 0x00 0xFF`.

#### Set all LEDs
The commans `0x04` allows you to set all LEDs via a single command. <br/>
Simply send `0x04` followed by one byte for red, one for green and one for blue. <br/>
So the values can be from `0` to `255` or from `0x00` to `0xFF`. <br/>

#### Set a custom animation

To make it easier to create custom animations, I've created a web tool to do that, you can find it here: [https://jan-olegiebel.github.io/LoRaWAN-LED-Christmas-Tree/tools/frameGeneratorTool/frameGeneratorTool.html](https://jan-olegiebel.github.io/LoRaWAN-LED-Christmas-Tree/tools/frameGeneratorTool/frameGeneratorTool.html)

When you're done, just click the Export button and send the generated data to the Christmas Tree.
Note that the order in which you send the frames should be exactly the same as in the frame generator.

This command `0x05` allows users to set a custom animation, consisting of one or multiple frames (1-6). <br/>
Because of LoRaWAN package size limitations frame counts larger than 2 need to be send independently. <br/>
Byte 0 will tell how many frames will be send by the following command. <br/>
E.g. if you want to send 3 frames, packet one will have byte 1 set to 03 and packet two will have byte 1 set to 1. <br/>

The data package uses the following structrure: <br/>
Byte 0: command `0x05` <br/>
Byte 1: number of frames that remain to send (1-6) e.g. `0x02` <br/>

The next bytes represents the frames. Every frame consists of 50 bytes. <br/>
Byte 0 of the frame data sets the frame delay in seconds. <br/>
Byte 1 the frame fade time in seconds. <br/>

The following 48 bytes contain the color information. Every 3 bytes contain the color information ref, green, blue for one LED. So one byte per color. <br/>

The frames can be added after another to create the animation. <br/>

To make the creation of animation easier, I've programmed a simple tool, you can find in the tools folder. <br/>

## Contributing

You're welcome to make contributions to this project!
Just fork the repo and create a pull request.

## License

This project is released under the Apache 2.0 License.

Copyright 2024 Jan-Ole Giebel

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

## Additional infos
LoRaWAN® is a mark used under license from the LoRa Alliance®.
