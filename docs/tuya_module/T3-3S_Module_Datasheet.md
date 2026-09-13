# T3-3S Module Datasheet

# T3-3S Module Datasheet
T3-3S module is a Wi-Fi 6 and Bluetooth Low Energy (LE) combo module developed by Tuya Smart. It is composed of a highly integrated radio-frequency identification (RFID) chip T3 and a few peripheral components. It can run in the station (STA) mode and access point (AP) mode.

## Overview

T3-3S has a built-in 32-bit MCU with a maximum running speed of 320 MHz, built-in 4 MB flash memory, and 640 KB SRAM. Various peripherals are available to support cloud connectivity, such as PWM, UART, and ADC. Up to five channels of 32-bit PWM output make the chip ideal for high-quality LED control.

### Features

- Built-in low-power 32-bit CPU that also acts as an application processor.
- Clock rate of 320 MHz.
- Operating voltage range: 2.0V to 3.6V.
- Peripherals: 5 PWM pins, 2 UARTs, and 1 ADC
- Wi-Fi connectivity
   - IEEE 802.11b/g/n/ax.
   - Channels 1 to 14 at 2.4 GHz.
   - Support WPA2, WPA2 PSK (AES), and WPA3 security modes.
   - The maximum output power is +17 dBm for IEEE 802.11b transmission.
   - Support STA, AP, STA + AP combo, and direct working modes.
   - Two pairing modes are supported, namely Bluetooth and access point (AP) mode. Both modes are suitable for pairing with Android and iOS mobile phones.
   - The onboard PCB antenna has a peak gain of 2.02 dBi.
   - Operating temperature range: -40°C to +85°C.
- Bluetooth connectivity
   - Bluetooth Core Specification v5.4.
   - The transmitter power in Bluetooth mode is 6 dBm.
   - Integral Wi-Fi and Bluetooth coexistence interface.
   - The onboard PCB antenna has a peak gain of 2.02 dBi.

### Scope of applications

- Smart building
- Smart home and electrical appliance
- Smart socket and light
- Industrial wireless control
- Baby monitor
- IP camera
- Smart bus

<!--
### 更新说明

| 更新日期 | 更新内容 | 更新后版本 |
| --- | --- | --- |
| 2024-4-3 | 新建文档 | V1.0.0 |

 -->

## Module interfaces
### Dimensions and footprint

The T3-3S dimensions are 16±0.35 mm (W) × 24±0.35 mm (L) × 2.8±0.15 mm (H). The figure below shows the dimensions of the T3-3S module.

<img src="https://images.tuyacn.com/content-platform/hestia/1712136791d014bff9ca9.PNG" width="400">



### Pin definition

| Pin No. | Symbol | I/O type | Description |
| ---- | ---- | ---- | ---- |
| 1 | NC | / | Not connected, in order to be compatible with other modules. |
| 2 | ADC | I | ADC pin, corresponding to Pin 12 on the IC. |
| 3 | CEN | I | Low-level reset pin, corresponding to Pin 26 on the IC. The module has been pulled up. |
| 4 | P48 | I/O | GPIO pin, corresponding to Pin 10 on the IC. |
| 5 | P24 | I/O | Support hardware PWM, corresponding to Pin 11 on the IC. |
| 6 | P32 | I/O | Support hardware PWM, corresponding to Pin 14 on the IC. |
| 7 | P34 | I/O | Support hardware PWM, corresponding to Pin 15 on the IC. |
| 8 | VBAT | P | 3.3V power pin. |
| 9 | GND | P | Ground pin. |
| 10 | P19 | I/O | GPIO pin, corresponding to Pin 38 on the IC. |
| 11 | TX1 | I/O | `UART_TX1` to print logs, corresponding to P0 (Pin 18) on the IC. |
| 12 | RX1 | I/O | `UART_RX1`, corresponding to P1 (Pin 17) on the IC. |
| 13 | P36 | I/O | Support hardware PWM, corresponding to Pin 16 on the IC. |
| 14 | P18 | I/O | Support hardware PWM, corresponding to Pin 37 on the IC. |
| 15 | RX0 | I/O | `UART_RX0` to receive user data, corresponding to P10 (Pin 20) on the IC. |
| 16 | TX0 | I/O | `UART_TX0` to send user data, corresponding to P11 (Pin 19) on the IC. |


:::info
- The maximum input level of the ADC port is 3.25V. It is recommended to place a 100 nF ground filter capacitor close to the ADC port.
- `P` indicates the power pin, `I/O` indicates the input and output pin, and `I` indicates the input pin.
- For the MCU integration solution, please refer to Tuya's design document [T3-X Series Modules](https://developer.tuya.com/cn/docs/iot/T3xSerialModuleUART?id=Kdqq3rq96qt6e).
:::

## Electrical parameters
### Absolute electrical parameters

| Parameter | Description | Minimum value | Maximum value | Unit |
| ---- | ---- | ---- | ---- | ---- |
| Ts | Storage temperature | -55 | 125 | °C |
| VBAT | Supply voltage | -0.3 | 3.9 | V |
| Electrostatic discharge voltage (human body model) | TAMB -25°C | \-4 | 4 | kV |
| Electrostatic discharge voltage (machine model) | TAMB -25°C | \-200 | 200 | V |

### Normal operating conditions

| Parameter | Description | Minimum value | Typical value | Maximum value | Unit |
| ---- | ---- | ---- | ---- | ---- | ---- |
| Ta | Operating temperature | -40 | - | 85 | °C |
| VBAT | Supply voltage | 2.0 | 3.3 | 3.6 | V |
| VOL | I/O low-level output | VSS | - | VSS + 0.3 | V |
| VOH | I/O high-level output | VBAT − 0.3 | \- | VBAT | V |
| Imax | I/O drive current | \- | 6 | 20 | mA |

### Radio frequency (RF) power

| Operating status | Mode | Rate | Transmit/Receive power | Average value | Peak (Typical) value | Unit |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Transmit | 802.11b | 11 Mbit/s | +17 dBm | 230 | 241 | mA |
| Transmit | 802.11g | 54 Mbit/s | +15 dBm | 200 | 230 | mA |
| Transmit | 802.11n | HT20 MCS7 | +14 dBm | 190 | 220 | mA |
| Transmit | 802.11ax | HE20 MCS7 | +14 dBm | 190 | 214 | mA |
| Receive | 802.11b | 11 Mbit/s | Continuous reception | 14 | 15 | mA |
| Receive | 802.11g | 54 Mbit/s | Continuous reception | 14 | 15 | mA |
| Receive | 802.11n | HT20 MCS7 | Continuous reception | 14 | 15 | mA |
| Receive | 802.11ax | HE20 MCS7 | Continuous reception | 14 | 15 | mA |

### Operating current

| The working mode. Valid values: | Status (Ta = 25°C) | Average value | Max (Typical) value | Unit |
| ---- | ---- | ---- | ---- | ---- |
| Quick pairing (Bluetooth) | The module is pairing over Bluetooth. The network status indicator blinks slowly. | 77 | 370 | mA |
| Quick pairing (AP) | The module is in AP mode. The network status indicator blinks slowly. | 105 | 380 | mA |
| Connected | The module is connected to the cloud. The network status indicator is steady on. | 48 | 300 | mA |
| Weakly connected | The connection between the module and the hotspot is intermittent. The network status indicator is steady on. | 140 | 380 | mA |
| Disconnected | The module is disconnected from the cloud. The network status indicator is steady off. | 43 | 370 | mA |
| Module disabled | The module's clock enable (CEN) pin is pulled down. | 330 | - | μA |

:::info
Low power long keepalive current: The typical value is 150 µA @ DTIM10.
:::

## RF parameters

### Basic RF features

| Parameter | Description |
| --- | --- |
| Operating frequency | 2.412 to 2.484 GHz |
| Wi-Fi standard | IEEE 802.11b/g/n/ax (channels 1–14) |
| Data transmission rate | <ul><li> IEEE 802.11b: 1, 2, 5.5, and 11 Mbit/s </li><li> IEEE 802.11g: 6, 9, 12, 18, 24, 36, 48, and 54 Mbit/s </li><li> IEEE 802.11n: HT20 MCS0-7      <li> IEEE 802.11n: HT40 MCS0-7           </li>   <li>  IEEE 802.11ax: HE20 MCS0-7</li>   </li><li>  IEEE 802.11ax: HE40 MCS0-7</li> </ul> |
| Antenna type | PCB antenna |

:::info
The above data module is powered by a 3.3 V regulated power supply with firmware v6.0.6. There are slight differences in test data under different test environment conditions.
:::

### Wi-Fi transmitter (TX) power

| Parameter | Minimum value | Typical value | Maximum value | Unit |
| ---- | ---- | ---- | ---- | ---- |
| RF average output power, 802.11b CCK mode, 11 Mbit/s | 15 | 17 | 19 | dBm |
| RF average output power, 802.11g OFDM mode, 54 Mbit/s | 13 | 15 | 17 | dBm |
| RF average output power, 802.11n OFDM mode, MCS7 (HT20) | 12 | 14 | 16 | dBm |
| RF average output power, 802.11n OFDM mode, MCS7 (HT40) |11 | 13 | 15 | dBm |
| RF average output power, 802.11ax OFDMA mode, MCS7 (HE20) | 12 | 14 | 16 | dBm |
| RF average output power, 802.11ax OFDMA mode, MCS7 (HE40) | 11 | 13 | 15 | dBm |
| Frequency error | -20 | \- | 20 | ppm |


### Wi-Fi transmitter (TX) EVM

| Parameter | Minimum value | Typical value | Maximum value | Unit |
| ---- | ---- | ---- | ---- | ---- |
| EVM, 802.11b CCK Mode, 1 Mbps | - | -17 | -10 | dB
| EVM, 802.11b CCK Mode, 11 Mbps | - | -17 | -10 | dB
| EVM, 802.11g OFDM Mode, 6 Mbps | - | -30 | -5 | dB
| EVM, 802.11g OFDM Mode, 54 Mbps | - | -31 | -25 | dB
| EVM, 802.11n HT20, MCS0 | - | -31 | -5 | dB
| EVM, 802.11n HT20, MCS7  | - | -32 | -27 | dB
| EVM, 802.11n HT40, MCS0 | - | -32 | -5 | dB
| EVM, 802.11n HT40, MCS7  | - | -33 | -27 | dB
| EVM, 802.11ax HE20, MCS0 | - | -31 | -5 | dB
| EVM, 802.11ax HE20, MCS7  | - | -32 | -28 | dB
| EVM, 802.11ax HE40, MCS0 | - | -31 | -5 | dB
| EVM, 802.11ax HE40, MCS7  | - | -32 | -28 | dB


### Wi-Fi receiver (RX) performance

| Parameter | Minimum value | Typical value | Maximum value | Unit |
| ---- | ---- | ---- | ---- | ---- |
| PER<8%, RX sensitivity, 802.11b, 1Mbps | \- | -97 | \- | dBm
| PER<8%,  RX sensitivity, 802.11b, 11 Mbit/s | \- | -88 | \- | dBm |
| PER<10%, RX sensitivity, 802.11g, 6Mbps | \- | -91 | \- | dBm |
| PER<10%, RX sensitivity, 802.11g, 54 Mbit/s | \- | -75 | \- | dBm |
| PER<10%, RX sensitivity, 802.11n, MCS0 ( HT20) | \- | -91 | \- | dBm
| PER<10%, RX sensitivity, 802.11n, MCS7 (HT20) | \- | -76 | \- | dBm |
| PER<10%, RX sensitivity, 802.11n, MCS0 (HT40) | \- | -88 | \- | dBm
| PER<10%, RX sensitivity, 802.11n, MCS7 (HT40) | \- | -73 | \- | dBm
| PER<10%, RX sensitivity, 802.11ax, MCS0 (HE20) | \- | -91| \- | dBm
| PER<10%, RX sensitivity, 802.11ax, MCS7 (HE20) | \- | -74 | \- | dBm
| PER<10%, RX sensitivity, 802.11ax, MCS0 (HE40) | \- | -88 | \- | dBm
| PER<10%, RX sensitivity, 802.11ax, MCS7 (HE40) | \- | -73 | \- | dBm



### Bluetooth transmitter (TX) performance

| Parameter | Minimum value | Typical value | Maximum value | Unit |
| ---- | ---- | ---- | ---- | ---- |
| Operating frequency | 2402 | \- | 2480 | MHz |
| Transmission rate over the air | \- | 1 | \- | Mbit/s |
| Transmission power | -20 | 6 | 20 | dBm |
| Frequency error | -150 | \- | 150 | KHz |

### Bluetooth receiver (RX) performance

| Parameter | Minimum value | Typical value | Maximum value | Unit |
| ---- | ---- | ---- | ---- | ---- |
| RX sensitivity | \- | -96 | \- | dBm |
| Max RF signal input | -10 | \- | \- | dBm |
| Intermodulation | \- | \- | -23 | dBm |
| Adjacent-channel rejection ratio | \- | 10 | \- | dB |

## Antenna information
### Antenna type

The T3-3S module uses an onboard PCB antenna.

### Antenna interference reduction

When a PCB antenna is used on a Wi-Fi module, we recommend that the module antenna is at least 15 mm away from other metal components. This can optimize the Wi-Fi performance.

Make sure that the enclosure surrounding the antenna is not traced or filled with copper. Otherwise, the RF performance might be degraded.

## Packing and production instructions
### Mechanical dimensions

Dimensions of the T3-3S PCB are 16±0.35 mm (W) × 24±0.35 mm (L) × 0.8±0.1 mm (H).

<img src="https://images.tuyacn.com/content-platform/hestia/1712136897bee84beadb2.PNG" height="400" style="display:inline;">

<img src="https://images.tuyacn.com/content-platform/hestia/171213691817489dbea21.PNG" height="400" style="display:inline;">


<img src="https://images.tuyacn.com/content-platform/hestia/1712806569d13e629ba4a.png" width="400">



The figure below shows the recommended footprint of the T3-3S module.

![image.png](https://images.tuyacn.com/content-platform/hestia/1725256187c6bf02f3808.png)

:::info
The area indicated as **keep-out** in the diagram above does not require tinning and should not have any traces routed through it.
:::

### Production instructions

1. If a PCB is designed to be SMT-packaged, package the module with the SMT. Complete soldering within 24 hours after the module is unpacked. Otherwise, we recommend that you place the module in a drying cupboard with a relative humidity level below 10%, or pack the module in vacuum again. Then, record the packing time and duration of exposure. The total exposure time cannot exceed 168 hours.
   - Instruments or devices required for the SMT process:
      - Surface mount system
      - SPI
      - Reflow soldering machine
      - Thermal profiler
      - AOI
   - Instruments or devices required for the baking process:
      - Cabinet oven
      - Electro-static discharge (ESD) protection and heat-resistant trays
      - ESD protection and heat-resistant gloves
2. A delivered module must meet the following storage requirements:
   - The moisture-proof bag must be placed in an environment where the temperature is below 40°C and the relative humidity is lower than 90%.
   - The shelf life of a dry-packaged product is 12 months from the date when the product is packaged and sealed.
   - A humidity indicator card (HIC) is put in the sealed package.

      <img alt="HIC" src="https://images.tuyacn.com/goat/20210410/2c61fd34d2a6464d8cbee05f63689786.png" width="">

3. The module needs to be baked in the following cases:
   - The vacuum packaging bag is damaged before unpacking.
   - After unpacking, no HIC is found in the packaging bag.
   - After unpacking, the HIC indicates a humidity level of 10% or higher. In this case, the circle turns pink on the HIC.
   - The total exposure time has lasted for over 168 hours since unpacking.
   - More than 12 months have passed since the first sealing of the bag.
4. The baking parameter settings are described below:
   - Baking temperature: 40°C for reel packaging with relative humidity ≤ 5%. And 125°C for tray packaging with relative humidity ≤ 5% (use the heat-resistant tray, rather than plastic containers).
   - Baking time: 168 hours for reel packaging and 12 hours for tray packaging.
   - Temperature for triggering an alert: 50°C for reel packaging and 135°C for tray packaging.
   - Production can begin after a module has cooled down to below 36°C under natural conditions.
   - If a module remains unused for over 168 hours after being baked, it needs to be baked again.
   - If a batch of modules is not baked after exposure for more than 168 hours, do not use reflow soldering to solder them. Because these modules are level-3 moisture-sensitive devices, they are very likely to get damp when exposed beyond the allowable time. In this case, if they are soldered at high temperatures, device failure or poor soldering performance might occur.
5. In the whole production process, take electrostatic discharge (ESD) protective measures.
6. To guarantee the pass rate, we recommend that you use the SPI and AOI to monitor the quality of solder paste printing and mounting.

### Recommended oven temperature curve

Refer to the recommended oven temperature curve for reflow soldering. The peak temperature is 245°C. Set the oven temperatures according to the following curve.

<img alt="Curve" src="https://images.tuyacn.com/goat/20210410/6517c2dffb5448e6ad520c1c3a23aa89.png" width="400">

- A: temperature axis
- B: time axis
- C: alloy liquidus temperature from 217°C to 220°C
- D: ramp-up slope from 1°C/s to 3°C/s
- E: keep a constant temperature from 150°C to 200°C for a time period of 60s to 120s
- F: temperature above liquidus temperature for 50s to 70s
- G: peak temperature from 235°C to 245°C
- H: ramp-down slope from 1°C/s to 4°C/s

:::important
The curve above is based on solder paste SAC305. For more information about other solder pastes, see the recommended oven temperature curve in the specified solder paste specifications.
:::


### Storage conditions

<img alt="Storage conditions" src="https://images.tuyacn.com/content-platform/hestia/1712806731bc8b04ed0cc.png" width="400">

## MOQ and packaging information

| Product model | MOQ (pcs) | Shipping packaging | Modules per reel | Reels per carton |
| --- | --- | ---- | --- | --- |
| T3-3S | 4,400 | Tape and reel | 1,100 | 4 |

## Appendix: Statement

**FCC Caution**: Any changes or modifications not expressly approved by the party responsible for compliance could void the user's authority to operate this device. The module is limited to installation in mobile or fixed applications.

This device complies with Part 15 of the FCC Rules. Operation is subject to the following two conditions: (1) This device may not cause harmful interference, and (2) this device must accept any interference received, including interference that may cause undesired operation.

**Note**: This device has been tested and found to comply with the limits for a Class B digital device, according to part 15 of the FCC Rules. These limits are designed to provide reasonable protection against harmful interference in a residential installation. This device generates, uses, and can radiate radio frequency energy and, if not installed and used following the instructions, may cause harmful interference to radio communications. However, there is no guarantee that interference will not occur in a particular installation.

If this device does cause harmful interference to radio or television reception, which can be determined by turning the device off and on, the user is encouraged to try to correct the interference by one or more of the following measures:

* Reorient or relocate the receiving antenna.
* Increase the separation between the device and receiver.
* Connect the device to an outlet on a circuit different from that to which the receiver is connected.
* Consult the dealer or an experienced radio/TV technician for help.

**Radiation Exposure Statement**

This device complies with FCC radiation exposure limits set forth for an uncontrolled rolled environment. This device should be installed and operated with a minimum distance of 20cm between the radiator and your body.

**Important Note**

This radio module must not be installed to co-locate and operate simultaneously with other radios in the host system except by following FCC multi-transmitter product procedures. Additional testing and device authorization may be required to operate simultaneously with other radios.

The availability of some specific channels and/or operational frequency bands are country dependent and are firmware programmed at the factory to match the intended destination. The firmware setting is not accessible to the end-user.

The host product manufacturer is responsible for compliance with any other FCC rules that apply to the host not covered by the modular transmitter grant of certification. The final host product still requires Part 15 Subpart B compliance testing with the modular transmitter installed. The separate approval is required for all other operating configurations including portable configurations with respect to Part 2.1093 and different antenna configurations.

The end-user manual shall include all required regulatory information/warnings as shown in this manual, including "This product must be installed and operated with a minimum distance of 20 cm between the radiator and user body". The OEM integrator is responsible for ensuring that the end-user has no manual instructions to remove or install the module.

This device has got an FCC ID: 2ANDL-T3-3S. The end product must be labeled in a visible area with the following: "Contains Transmitter Module FCC ID: 2ANDL-T3-3S".

This device is intended only for OEM integrators under the following conditions:

The antenna must be installed such that 20cm is maintained between the antenna and users, and the transmitter module may not be co-located with any other transmitter or antenna.

As long as the 2 conditions above are met, further transmitter tests will not be required. However, the OEM integrator is still responsible for testing their end-product for any additional compliance requirements required with this module installed.

**Declaration of Conformity European Notice**

<img alt="CE.png" src="https://images.tuyacn.com/goat/20200107/b9d02ed6d1b7477d8c87b6ce012fc4b9.png" width="100">

Hereby, Hangzhou Tuya Information Technology Co., Ltd declares that this module product is in compliance with essential requirements and other relevant provisions of Directive 2014/53/EU,2011/65/EU. A copy of the Declaration of Conformity can be found at https://www.tuya.com.

<img alt="Statement.png" src="https://images.tuyacn.com/goat/20200107/25b18f04cecd484db93c67930377b26d.png" width="100">

This product must not be disposed of as normal household waste, in accordance with the EU directive for waste electrical and electronic equipment (WEEE-2012/19/EU). Instead, it should be disposed of by returning it to the point of sale, or to a municipal recycling collection point.

The device could be used with a separation distance of 20cm from the human body.
