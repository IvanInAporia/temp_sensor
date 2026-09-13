# STM32L010F4P6 (TSSOP20) — Condensed Reference

Distilled from **DS12323 Rev 3 (August 2019)**, `stm32l010f4.pdf`, restricted to the
**TSSOP20** package (`STM32L010F4P6`). Figures, timing diagrams and consumption curves are
omitted — consult the PDF for those. Register-level detail is in **RM0451**.

---

## 1. Part identification

| Field | Value |
|---|---|
| Ordering code | `STM32L010F4P6` (`TR` suffix = tape & reel) |
| Package code | `P` = TSSOP20, 6.5 × 4.4 mm, 0.65 mm pitch, 169 mils |
| Flash code | `4` = 16 Kbyte |
| Temp code | `6` = industrial, −40 to +85 °C |
| Top marking | `32L010F4P6` + `Y WW R` (date code / revision code) |
| Compliance | ECOPACK2 |

---

## 2. Core, memory and clocking (software-relevant)

- **Core:** Arm Cortex-M0+, 32-bit, 2-stage pipeline, von Neumann, single-cycle multiplier,
  0.95 DMIPS/MHz, **f_max 32 MHz** (AHB = APB1 = APB2 max 32 MHz).
- **NVIC:** up to 32 maskable channels, **4 priority levels**, NMI, tail-chaining.
- **Debug:** Serial Wire only (SW-DP) — `SWDIO` = PA13, `SWCLK` = PA14. No JTAG, no ITM trace.
- **Memory:** 16 Kbyte Flash, **2 Kbyte SRAM** (0 wait state), **128 byte data EEPROM**,
  4 Kbyte system memory (bootloader), 32 user/factory option bytes, 20-byte backup registers
  (5 × 32-bit, survive system reset and Standby).
  Memory map / peripheral base addresses: see RM0451.
- **Readout protection:** Level 0 (none), Level 1 (readout protected — Flash unreadable if debug
  is connected or boot-in-RAM is selected), Level 2 (chip readout protected, SWD and boot-in-RAM
  permanently disabled). Write/readout protection granularity 4 Kbyte.
- **CRC unit:** configurable generator polynomial and size.
- **Unique ID:** 96-bit. **VREFINT_CAL** (raw ADC value @ 25 °C, V_DDA = 3.0 V) at
  **0x1FF8 0078 – 0x1FF8 0079**.
- **DMA1:** 5 channels usable (7-channel controller), memory↔memory / peripheral,
  circular mode; serves SPI, I2C, USART, LPUART, TIM2/TIM21, ADC.

### Clock sources

| Source | Frequency | Accuracy / notes |
|---|---|---|
| **HSE** | 0–32 MHz | **Bypass (external clock) only** on this device — no crystal oscillator supported. Applied to `PA0-CK_IN`. Duty 45–55 %, V_IH ≥ 0.7 V_DD, rise/fall ≤ 20 ns, C_in 2.6 pF |
| **HSI16** | 16 MHz RC | ±1 % @ 25 °C / 3.0 V; ±2 % over −10…85 °C; −5.45/+3.25 % over full range and 1.8–3.6 V. Startup 3.7 µs typ, I_DD 100 µA typ |
| **MSI** | 65.5 / 131 / 262 / 524 kHz, 1.05 / 2.1 / 4.2 MHz | ±0.5 % after factory cal; startup 30 µs (range 0) … 3.5 µs (range 6); I_DD 0.75–15 µA |
| **LSI** | 37 kHz typ (26–56 kHz) | Drives IWDG; startup ≤ 200 µs, I_DD 400 nA |
| **LSE** | 32.768 kHz crystal | On `PC14-OSC32_IN` / `PC15-OSC32_OUT`; g_m 0.5–2.7 µA/V via `LSEDRV[1:0]`; startup 2 s typ |
| **PLL** | in 2–24 MHz, out 2–32 MHz | Lock 115 µs typ / 160 µs max; jitter ±600 ps; I_DDA 220 µA, I_DD 120 µA |

- **CSS** is available on **LSE only** (not on HSE).
- **MCO** output available on **PA9** (AF0) in TSSOP20 (PA8 is not bonded).
- MSI can be trimmed to ±0.5 % against LSE when an LSE is present.
- **LSE layout rule:** do **not** place an external resistor between OSC32_IN and OSC32_OUT
  (explicitly forbidden). Keep the crystal and its load caps as close to the pins as possible.

### Voltage scaling vs. CPU frequency

| Range | V_CORE | `VOS[1:0]` | Frequency (wait states) |
|---|---|---|---|
| Range 1 | 1.8 V | 01 | 32 kHz–16 MHz (0 ws), 16–32 MHz (1 ws) |
| Range 2 | 1.5 V | 10 | 32 kHz–8 MHz (0 ws), 8–16 MHz (1 ws) |
| Range 3 | 1.2 V | 11 | 32 kHz–4.2 MHz (0 ws) |

> For 1.8 V ≤ V_DD ≤ 2.0 V, a frequency change must not exceed ×4 in one step, with ≥ 5 µs
> between steps (e.g. 4.2 → 16 MHz, wait 5 µs, then 16 → 32 MHz).

### Boot

`BOOT0` = **pin 1 (PB9-BOOT0, input only)** together with option bits `nBOOT0`, `nBOOT1`,
`nBOOT_SEL`. Boot from Flash / system memory / SRAM. The built-in bootloader speaks
**USART2 (PA2/PA3)** or **SPI1 (PA4–PA7)** and is active on empty devices (empty-check
mechanism). See AN2606. BOOT0/PB9 max input frequency: 10 kHz (1.8–2.7 V), 5 MHz (2.7–3.6 V).

---

## 3. Peripheral set on STM32L010F4 (TSSOP20)

| Peripheral | Present | TSSOP20 notes |
|---|---|---|
| GPIO | 16 I/O | 14 are 5 V-tolerant (FT); PB9-BOOT0 is input-only, PC15 is TC (3.3 V) |
| 12-bit ADC | 1 | ADC_IN0…IN7 and IN9 are bonded out (see pin table). Table 1 of the datasheet quotes "7 channels" for the F4 variant, which does not match its own pin table — verify against the pin table |
| TIM2 | 16-bit, 4 ch, up/down, DMA, encoder + hall | prescaler 1…65536 |
| TIM21 | 16-bit, 2 ch, up/down, no DMA | can be clocked from LSE for a CPU-independent timebase |
| LPTIM1 | 16-bit, runs in Stop | clock: LSE / LSI / HSI / APB or external input; pulse/PWM, encoder, glitch filter; **can wake the MCU from Stop** |
| SysTick | 24-bit downcounter | — |
| IWDG | 12-bit + 8-bit prescaler on LSI | runs in Stop and Standby; HW or SW start via option byte |
| WWDG | 7-bit, on the bus clock | early-warning interrupt |
| RTC | calendar, 2 alarms, periodic wakeup, 1 ppm calibration, timestamp, 2 tamper pins | tamper / TS / RTC_OUT available on PA0 and PA2 |
| I2C1 | 7/10-bit, Sm 100 kbit/s, Fm 400 kbit/s, SMBus 2.0 / PMBus 1.1, PEC, DMA | **no Fast-mode-plus**; independent clock domain → **wakeup from Stop on address match** |
| SPI1 | up to 16 Mbit/s, 8/16-bit frames, HW CRC, DMA | **no I2S**, no TI mode |
| USART2 | up to 4 Mbit/s, CTS/RTS/DE (RS485), DMA, multiprocessor, single-wire half-duplex | **not** implemented: synchronous mode, smartcard, IrDA, LIN, Modbus, auto-baud, receiver timeout, dual clock domain |
| LPUART1 | half-duplex, CTS/RTS, multiprocessor, DMA | wakes from Stop up to 46 kbaud; 9600 baud from LSE alone |
| CRC | configurable polynomial | — |

**EXTI:** 23 lines — 16 configurable GPIO lines (`EXTIx` selects among ports A/B/C for pin *x*)
plus 7 fixed lines for RTC / USART / I2C / LPUART / LPTIM events. Minimum detected pulse
width **8 ns**.

**Interconnect matrix (autonomous, no CPU):** TIMx → TIMx trigger; RTC → TIM21 / LPTIM1 trigger
(LPTIM1 also in Stop); clock source → TIMx for RC measurement/trimming; GPIO → TIMx / LPTIM1
input & trigger, and GPIO → ADC conversion trigger.

---

## 4. TSSOP20 pinout

Top view, pin 1 top-left: pins 1–10 down the left side, 11–20 up the right side.

| Pin | Name (after reset) | Type | I/O struct. | Alternate functions | Additional functions |
|---:|---|---|---|---|---|
| 1 | **PB9-BOOT0** | I | B | — | — |
| 2 | **PC14-OSC32_IN** | I/O | FT | — | OSC32_IN |
| 3 | **PC15-OSC32_OUT** | I/O | TC | — | OSC32_OUT |
| 4 | **NRST** | I/O | RST | — | reset in / internal reset out, active low |
| 5 | **VDDA** | S | — | analog supply | — |
| 6 | **PA0-CK_IN** | I/O | TTa | USART2_RX, LPTIM1_IN1, TIM2_CH1, USART2_CTS, TIM2_ETR, LPUART1_RX | ADC_IN0, RTC_TAMP2 / **WKUP1** / CK_IN |
| 7 | **PA1** | I/O | FT | EVENTOUT, LPTIM1_IN2, TIM2_CH2, I2C1_SMBA, USART2_RTS, TIM21_ETR, LPUART1_TX | ADC_IN1 |
| 8 | **PA2** | I/O | TTa | TIM21_CH1, TIM2_CH3, USART2_TX, LPUART1_TX | ADC_IN2, RTC_TAMP3 / RTC_TS / RTC_OUT / **WKUP3** |
| 9 | **PA3** | I/O | FT | TIM21_CH2, TIM2_CH4, USART2_RX, LPUART1_RX | ADC_IN3 |
| 10 | **PA4** | I/O | TTa | SPI1_NSS, LPTIM1_IN1, LPTIM1_ETR, I2C1_SCL, USART2_CK, TIM2_ETR, LPUART1_TX | ADC_IN4 |
| 11 | **PA5** | I/O | TTa | SPI1_SCK, LPTIM1_IN2, TIM2_ETR, TIM2_CH1 | ADC_IN5 |
| 12 | **PA6** | I/O | FT | SPI1_MISO, LPTIM1_ETR, LPUART1_CTS, EVENTOUT | ADC_IN6 |
| 13 | **PA7** | I/O | FT | SPI1_MOSI, LPTIM1_OUT, USART2_CTS, TIM21_ETR, EVENTOUT | ADC_IN7 |
| 14 | **PB1** | I/O | FT | USART2_CK, SPI1_MOSI, LPTIM1_IN1, LPUART1_RTS, TIM2_CH4 | ADC_IN9, **VREF_OUT** |
| 15 | **VSS** | S | — | digital + analog ground | — |
| 16 | **VDD** | S | — | digital supply | — |
| 17 | **PA9** | I/O | FT | MCO, I2C1_SCL, LPTIM1_OUT, USART2_TX, TIM21_CH2 | — |
| 18 | **PA10** | I/O | FT | TIM21_CH1, I2C1_SDA, RTC_REFIN, USART2_RX, TIM2_CH3 | — |
| 19 | **PA13** | I/O | FT | **SWDIO**, LPTIM1_ETR, I2C1_SDA, SPI1_SCK, LPUART1_RX | — |
| 20 | **PA14** | I/O | FT | **SWCLK**, LPTIM1_OUT, I2C1_SMBA, USART2_TX, SPI1_MISO, LPUART1_TX | — |

**Legend** — `S` supply, `I` input only, `I/O` bidirectional; `FT` 5 V-tolerant,
`TTa` 3.3 V-tolerant and directly connected to the ADC, `TC` standard 3.3 V,
`B` BOOT0 structure, `RST` reset structure.
Unless otherwise noted, **all I/Os are analog inputs during and after reset**.

**Not bonded on TSSOP20** (present on LQFP32): PA8, PA11, PA12, PA15, PB0, PB3, PB4, PB5, PB6,
PB7, plus the second VDD and second VSS. Consequences: **no VREF_PVD_IN** (PB7), **no ADC_IN8**
(PB0), only **one MCO pin** (PA9), and no separate VSSA pin — *VSSA is internally connected to
VSS in all packages*.

### Alternate-function numbers (`GPIOx_AFR`) for TSSOP20 pins

| Pin | AF0 | AF1 | AF2 | AF3 | AF4 | AF5 | AF6 |
|---|---|---|---|---|---|---|---|
| PA0 | USART2_RX | LPTIM1_IN1 | TIM2_CH1 | — | USART2_CTS | TIM2_ETR | LPUART1_RX |
| PA1 | EVENTOUT | LPTIM1_IN2 | TIM2_CH2 | I2C1_SMBA | USART2_RTS | TIM21_ETR | LPUART1_TX |
| PA2 | TIM21_CH1 | — | TIM2_CH3 | — | USART2_TX | — | LPUART1_TX |
| PA3 | TIM21_CH2 | — | TIM2_CH4 | — | USART2_RX | — | LPUART1_RX |
| PA4 | SPI1_NSS | LPTIM1_IN1 | LPTIM1_ETR | I2C1_SCL | USART2_CK | TIM2_ETR | LPUART1_TX |
| PA5 | SPI1_SCK | LPTIM1_IN2 | TIM2_ETR | — | — | TIM2_CH1 | — |
| PA6 | SPI1_MISO | LPTIM1_ETR | — | — | LPUART1_CTS | — | EVENTOUT |
| PA7 | SPI1_MOSI | LPTIM1_OUT | — | — | USART2_CTS | TIM21_ETR | EVENTOUT |
| PA9 | MCO | I2C1_SCL | LPTIM1_OUT | — | USART2_TX | TIM21_CH2 | — |
| PA10 | TIM21_CH1 | I2C1_SDA | RTC_REFIN | — | USART2_RX | TIM2_CH3 | — |
| PA13 | SWDIO | LPTIM1_ETR | — | I2C1_SDA | — | SPI1_SCK | LPUART1_RX |
| PA14 | SWCLK | LPTIM1_OUT | — | I2C1_SMBA | USART2_TX | SPI1_MISO | LPUART1_TX |
| PB1 | USART2_CK | SPI1_MOSI | LPTIM1_IN1 | — | LPUART1_RTS | TIM2_CH4 | — |

(AF column headers: AF0 SPI1/USART2/TIM21/EVENTOUT/SYS_AF · AF1 SPI1/I2C1/LPTIM ·
AF2 LPUART1/LPTIM/TIM2/EVENTOUT/SYS_AF · AF3 I2C1/EVENTOUT · AF4 I2C1/USART2/LPUART1/EVENTOUT ·
AF5 SPI1/TIM2/TIM21 · AF6 LPUART1/EVENTOUT.)

### Signal availability summary (TSSOP20 only)

| Signal | Available on |
|---|---|
| I2C1_SCL | PA4 (AF3), PA9 (AF1) |
| I2C1_SDA | PA10 (AF1), PA13 (AF3) |
| I2C1_SMBA | PA1 (AF3), PA14 (AF3) |
| SPI1 NSS / SCK / MISO / MOSI | PA4 / PA5, PA13 / PA6, PA14 / PA7, PB1 |
| USART2_TX | PA2, PA9, PA14 (all AF4) |
| USART2_RX | PA0 (AF0), PA3, PA10 (AF4) |
| LPUART1_TX | PA1, PA2, PA4, PA14 (AF6) |
| LPUART1_RX | PA0, PA3, PA13 (AF6) |
| SWD | PA13 (SWDIO), PA14 (SWCLK) |
| MCO | PA9 (AF0) |
| Standby wakeup pins | **WKUP1 = PA0**, **WKUP3 = PA2** |
| RTC tamper / timestamp / RTC_OUT | PA0 (TAMP2), PA2 (TAMP3, TS, OUT) |
| VREF_OUT | PB1 |

> **PA13/PA14 carry SWD.** If they are reassigned, keep a recovery path (BOOT0 on pin 1 →
> system bootloader) or debug access is lost.

---

## 5. Absolute maximum ratings

| Symbol | Rating | Min | Max | Unit |
|---|---|---|---|---|
| V_DD−V_SS | External main supply (incl. V_DDA) | −0.3 | 4.0 | V |
| V_IN | on FT pins | V_SS − 0.3 | V_DD + 4.0 | V |
| V_IN | on TC pins | V_SS − 0.3 | 4.0 | V |
| V_IN | on BOOT0 | V_SS | V_DD + 4.0 | V |
| V_IN | any other pin | V_SS − 0.3 | 4.0 | V |
| \|ΔV_DDA−V_DDx\| | V_DDA vs V_DD difference | — | 300 | mV |
| \|ΔV_SS\| | between ground pins | — | 50 | mV |
| I_VDD / I_VSS | total into / out of all supply lines | — | 105 | mA |
| I_IO | sunk / sourced by any I/O or control pin | — | 16 / −16 | mA |
| ΣI_IO(PIN) | total sunk / sourced by all I/Os | — | 90 / −90 | mA |
| I_INJ(PIN) | injected current on FT, RST, B pins | −5 | +0 | mA |
| I_INJ(PIN) | injected current on TC pin | −5 | +5 | mA |
| ΣI_INJ(PIN) | total injected current | — | ±25 | mA |
| T_STG | storage temperature | −65 | +150 | °C |
| T_J | junction temperature | — | 150 | °C |

**ESD / latch-up:** HBM 2000 V (class 2), CDM 500 V (class C4), latch-up class II level A @ 85 °C.
**EMS:** V_FESD level 3B (IEC 61000-4-2), V_EFTB level 4A (IEC 61000-4-4).
**EMI:** peak −22 dBµV (0.1–30 MHz), −7 dBµV (30–130 MHz), −12 dBµV (130 MHz–1 GHz), SAE level 1.

> Positive current injection is **not possible** on PA0, PA4, PA5 (and PA11/PA12/PC15 on other
> packages). ST recommends a **Schottky diode to ground** on analog pins that may see negative
> injection — negative injection on a standard analog pin significantly degrades the accuracy of
> a conversion in progress on *another* channel.

---

## 6. Operating conditions

| Symbol | Parameter | Min | Max | Unit |
|---|---|---|---|---|
| f_HCLK, f_PCLK1, f_PCLK2 | bus clocks | 0 | 32 | MHz |
| V_DD | supply | 1.8 | 3.6 | V |
| V_DDA | analog supply — **same voltage as V_DD** (≤ 300 mV difference) | 1.8 | 3.6 | V |
| V_IN (FT, RST) | 2.0 V ≤ V_DD ≤ 3.6 V | −0.3 | 5.5 | V |
| V_IN (FT, RST) | 1.8 V ≤ V_DD ≤ 2.0 V | −0.3 | 5.2 | V |
| V_IN (BOOT0) | — | 0 | 5.5 | V |
| V_IN (TC) | — | −0.3 | V_DD + 0.3 | V |
| P_D | power dissipation @ T_A = 85 °C, **TSSOP20** | — | **270** | mW |
| T_A | ambient temperature | −40 | 85 | °C |
| T_J | junction temperature | −40 | 105 | °C |

**Thermal:** Θ_JA (TSSOP20, 169 mils) = **74 °C/W**; T_Jmax = T_Amax + (P_Dmax × Θ_JA).
To sustain an input above V_DD + 0.3 V on an FT pin, the internal pull-up/pull-down must be
disabled.

### Reset and supply supervision

| Symbol | Parameter | Min | Typ | Max | Unit |
|---|---|---|---|---|---|
| t_VDD | V_DD rise/fall rate, BOR enabled | 0 | — | ∞ | µs/V |
| t_VDD | V_DD rise rate, BOR disabled | 0 | — | 1000 | µs/V |
| t_VDD | V_DD fall rate, BOR disabled | 20 | — | ∞ | µs/V |
| t_RSTTEMPO | reset temporization | — | 2 | 3.3 | ms |
| V_POR/PDR | POR/PDR threshold, falling / rising | 1.0 / 1.3 | 1.5 / 1.5 | 1.8 / 1.8 | V |
| V_BOR0 | falling / rising | 1.67 / 1.69 | 1.70 / 1.76 | 1.74 / 1.80 | V |
| V_BOR1 | falling / rising | 1.87 / 1.96 | 1.93 / 2.03 | 1.97 / 2.07 | V |
| V_BOR2 | falling / rising | 2.22 / 2.31 | 2.30 / 2.41 | 2.35 / 2.44 | V |
| V_BOR3 | falling / rising | 2.45 / 2.54 | 2.55 / 2.66 | 2.60 / 2.70 | V |
| V_BOR4 | falling / rising | 2.68 / 2.78 | 2.80 / 2.90 | 2.85 / 2.95 | V |
| V_hyst | BOR0 / all other thresholds | — | 40 / 100 | — | mV |

BOR is active at power-on and guarantees proper operation from 1.8 V regardless of the ramp
shape. The threshold is selected by option bytes (5 levels, 1.8 to 3 V) and BOR can be disabled
permanently. **No external reset circuit is required.** V_REFINT can be switched off in Stop
mode to cut consumption.

### Internal reference voltage

| Symbol | Parameter | Min | Typ | Max | Unit |
|---|---|---|---|---|---|
| V_REFINT | internal reference, −40…85 °C | 1.202 | 1.224 | 1.242 | V |
| t_VREFINT | startup time | — | 2 | 3 | ms |
| A_VREF_MEAS | accuracy of the factory-measured value | — | — | ±5 | mV |
| TCoeff | temperature coefficient | — | 25 | 100 | ppm/°C |
| T_S_vrefint | ADC sampling time when reading V_REFINT | 5 | 10 | — | µs |
| T_ADC_BUF | reference-buffer startup for ADC | — | — | 10 | µs |
| I_BUF_ADC | reference-buffer consumption (ADC) | — | 13.5 | 25 | µA |
| I_VREF_OUT | VREF_OUT output current (for < 1 % deviation) | — | — | **1** | µA |
| C_VREF_OUT | VREF_OUT output load | — | — | 50 | pF |

V_REFINT is internally wired to **ADC_IN17** and is the only way to measure V_DD — there is no
external VREF+ pin. Factory calibration value at `0x1FF8 0078`.

---

## 7. Current consumption (typical, V_DD = 3.0 V unless stated)

| Mode | Condition | Typ | Max |
|---|---|---|---|
| Run, from Flash | Range 1, HSE 32 MHz | 5.3 mA | 6.5 mA |
| Run, from Flash | Range 2, HSI 16 MHz | 2.2 mA | 2.6 mA |
| Run, from Flash | Range 3, MSI 4.2 MHz | 505 µA | 560 µA |
| Run, from Flash | Range 3, MSI 65 kHz | 34.5 µA | 54 µA |
| Sleep, Flash off | Range 1, 32 MHz | 1350 µA | 1700 µA |
| Sleep, Flash off | Range 3, MSI 65 kHz | 15.5 µA | 32 µA |
| Low-power run (from RAM, Flash off) | MSI 65 kHz, f_HCLK 32 kHz, ≤ 25 °C | 5.7 µA | 8.1 µA |
| Low-power run (from Flash) | MSI 65 kHz, f_HCLK 32 kHz, ≤ 25 °C | 17 µA | 19.5 µA |
| Low-power sleep | MSI 65 kHz, Flash off, ≤ 25 °C | 2.5 µA | — |
| Low-power sleep | MSI 65 kHz, Flash on, ≤ 25 °C | 13 µA | 19 µA |
| **Stop** | −40…25 °C | **0.34 µA** | 0.99 µA |
| Stop | 55 °C / 85 °C | 0.43 / 0.94 µA | 1.9 / 4.2 µA |
| **Standby**, IWDG + LSI off | −40…25 °C | **0.23 µA** | 0.6 µA |
| Standby, IWDG + LSI off | 55 °C / 85 °C | 0.25 / 0.36 µA | 0.7 / 1.7 µA |
| Standby, IWDG + LSI on | −40…25 °C | 0.8 µA | 1.6 µA |

Headline figures at V_DD = 1.8 V: Stop 0.29 µA (no RTC) / 0.54 µA (with RTC), Standby 0.1 µA
(no RTC) / 0.41 µA (with RTC). At 3.0 V: 0.34 / 0.67 and 0.23 / 0.53 µA.

**Transients:** wakeup from Stop 0.1 mA (MSI 65 kHz) … 1 mA (HSI); wakeup from Standby 0.5 mA
(fast wakeup set) / 0.12 mA (fast wakeup disabled); NRST held low 0.21 mA; power-up with BOR on
0.23 mA. **Flash/EEPROM program or erase:** 500 µA average (peak 1.5 mA typ, 2.5 mA max)
for 3.28 ms typ.

### Peripheral adders (typ, µA/MHz of f_HCLK, V_DD = 3.0 V, 25 °C)

| Peripheral | Range 1 | Range 2 | Range 3 | LP run/sleep |
|---|---|---|---|---|
| I2C1 | 11 | 8.2 | 6.8 | 8.9 |
| SPI1 | 4.5 | 3.5 | 2.9 | 3.6 |
| USART2 | 8.5 | 6.8 | 5.4 | 7.1 |
| LPUART1 | 8.3 | 7.2 | 5.4 | 7.2 |
| LPTIM1 | 14 | 11 | 8.7 | 11 |
| TIM2 / TIM21 | 10.5 / 6.8 | 8.5 / 6.1 | 6.4 / 4.5 | 8.5 / 5.6 |
| ADC1 | 5 | 3.9 | 3.3 | 4 |
| GPIOA / GPIOB / GPIOC | 7.6 / 5.1 / 1.1 | 6.3 / 4.1 / 0.7 | 4.9 / 3.2 / 0.6 | 6.5 / 4 / 0.8 |
| DMA1 | 5.3 | 4.2 | 3.5 | 4.8 |
| WWDG | 2.5 | 2 | 1.6 | 2 |
| SYSCFG / DBGMCU | 2.5 / 1.7 | 2.4 / 1.7 | 1.6 / 1.1 | 2.3 / 1.4 |
| **All enabled** | 96 | 80 | 62 | 88 |

**In Stop / Standby (µA typ, 25 °C, at V_DD = 1.8 V / 3.0 V):** BOR 0.6 / 1.0 ·
V_REFINT — / 3.0 · LSE low drive 0.1 / 0.19 · RTC 0.025 / 0.03 · LPUART1 0.01 / — ·
LPTIM1 with 1 MHz input 8 / 9. LPTIM1 cannot operate in Standby.

---

## 8. Low-power modes and wakeup

Seven modes: Sleep, Low-power run, Low-power sleep, Stop (with / without RTC),
Standby (with / without RTC).

- **Stop:** RAM, registers and RTC retained; PLL, HSE, MSI and HSI off (a peripheral with wakeup
  capability can request HSI on demand); regulator in low-power mode. Wakeup from any EXTI line:
  any GPIO, RTC alarm / tamper / timestamp / wakeup, or USART / I2C / LPUART / LPTIM events.
  USART and LPUART reception works in Stop (start-bit wakeup); I2C address detection works in
  Stop and wakes HSI during reception.
- **Standby:** regulator off, whole V_CORE domain unpowered, RAM and registers **lost** except
  the standby circuitry (wakeup logic, IWDG, RTC, LSI, LSE, backup registers, `RCC_CSR`).
  Wakeup only from NRST, IWDG reset, a rising edge on a **WKUP pin (PA0 = WKUP1, PA2 = WKUP3
  on TSSOP20)**, or an RTC alarm / tamper / timestamp / wakeup event.
- RTC and IWDG (and their clock sources) are **not** stopped automatically on entering
  Stop or Standby.
- **RAM/register retention:** V_DD ≥ 1.8 V in Stop or under reset.

| Wakeup | Condition | Typ | Max |
|---|---|---|---|
| Sleep | f_HCLK = 32 MHz | 7 | 8 CPU cycles |
| Low-power sleep | 262 kHz, Flash on / off | 7 / 9 | 8 / 10 CPU cycles |
| Stop, regulator in Run mode | f_HSI = 16 MHz | 5.1 µs | 7 µs |
| Stop, regulator in LP mode | MSI 4.2 MHz, Range 1 | 5 µs | 8 µs |
| Stop, regulator in LP mode | MSI 65 kHz | 196 µs | 260 µs |
| Stop, HSI kept running in Stop | f_HSI = 16 MHz | 3.25 µs | 7 µs |
| **Standby** | MSI 2.1 MHz, FWU = 1 | 65 µs | 130 µs |
| Standby | MSI 2.1 MHz, FWU = 0 | 2.2 ms | 3 ms |

Clock used on wakeup: Sleep → the clock in use before entry; Stop → MSI (range as configured),
HSI16 or HSI16/4; Standby → MSI at 2.1 MHz.

---

## 9. I/O characteristics (PCB-relevant)

| Symbol | Parameter | Min | Typ | Max | Unit |
|---|---|---|---|---|---|
| V_IL | TC, FT, RST I/Os | — | — | 0.3 V_DD | V |
| V_IL | BOOT0 | — | — | 0.14 V_DD | V |
| V_IH | all I/Os except BOOT0 | 0.7 V_DD | — | — | V |
| V_hys | Schmitt hysteresis, standard I/Os | — | 10 % V_DD (≥ 200 mV) | — | V |
| I_lkg | leakage, V_SS ≤ V_IN ≤ V_DD | — | — | ±50 | nA |
| I_lkg | leakage, V_DD ≤ V_IN ≤ 5 V (FT) | — | — | 200 | nA |
| R_PU / R_PD | internal pull-up / pull-down | 25 | 45 | 65 | kΩ |
| C_IO | pin capacitance | — | 5 | — | pF |

All I/Os are CMOS and TTL compliant. **Output drive:** ±8 mA nominal, ±15 mA with the relaxed
levels below.

| Condition | V_OL max | V_OH min |
|---|---|---|
| I_IO = ±8 mA, 2.7–3.6 V (CMOS) | 0.4 V | V_DD − 0.4 V |
| I_IO = +8 / −6 mA, 2.7–3.6 V (TTL) | 0.4 V | 2.4 V |
| I_IO = ±15 mA, 2.7–3.6 V | 1.3 V | V_DD − 1.3 V |
| I_IO = ±4 mA, 1.8–3.6 V | 0.45 V | V_DD − 0.45 V |

**Output speed (`OSPEEDR[1:0]`), C_L = 50 pF unless noted:**

| `OSPEEDR` | f_max @ 2.7–3.6 V | f_max @ 1.8–2.7 V | t_r/t_f @ 2.7–3.6 V | t_r/t_f @ 1.8–2.7 V |
|---|---|---|---|---|
| 00 | 400 kHz | 100 kHz | 125 ns | 320 ns |
| 01 | 2 MHz | 0.6 MHz | 30 ns | 65 ns |
| 10 | 10 MHz | 2 MHz | 13 ns | 28 ns |
| 11 | 35 MHz (C_L = 30 pF) | 10 MHz | 6 ns | 17 ns |

Use the slowest `OSPEEDR` that still meets timing — it directly reduces EMI and switching
current. EXTI detects pulses ≥ 8 ns.

### NRST pin

| Symbol | Parameter | Min | Typ | Max |
|---|---|---|---|---|
| V_IL(NRST) | input low | — | — | 0.3 V_DD |
| V_IH(NRST) | input high | 0.39 V_DD + 0.59 | — | — |
| V_OL(NRST) | I_OL = 2 mA, 2.7–3.6 V | — | — | 0.4 V |
| V_hys(NRST) | Schmitt hysteresis | — | 10 % V_DD (≥ 200 mV) | — |
| R_PU | internal pull-up | 25 kΩ | 45 kΩ | 65 kΩ |
| V_F(NRST) | filtered (ignored) pulse | — | — | 50 ns |
| V_NF(NRST) | guaranteed-detected pulse | 350 ns | — | — |

**Recommended:** a **0.1 µF capacitor from NRST to ground, placed as close to the pin as
possible** (protects against parasitic resets). No external pull-up or reset IC is needed —
the pin has a permanent internal pull-up and the device has POR/PDR/BOR.

---

## 10. ADC (12-bit, oversampling to 16-bit)

| Symbol | Parameter | Min | Typ | Max | Unit |
|---|---|---|---|---|---|
| V_DDA | analog supply with ADC on | 1.8 | — | 3.6 | V |
| f_ADC | ADC clock | 0.14 | — | 16 | MHz |
| f_S | sampling rate | 0.01 | — | **1.14** | Msps |
| f_TRIG | external trigger frequency | — | — | 941 | kHz |
| V_AIN | conversion voltage range | 0 | — | V_DDA | V |
| R_AIN | external input impedance | — | — | 50 | kΩ |
| R_ADC | sampling switch resistance | — | — | 1 | kΩ |
| C_ADC | internal sample-and-hold capacitor | — | — | 8 | pF |
| t_S | sampling time | 1.5 | — | 239.5 | 1/f_ADC |
| t_CONV | total conversion incl. sampling | 0.875 | — | 10.81 | µs @ f_ADC 16 MHz |
| t_CAL | calibration time | — | 83 | — | 1/f_ADC (5.2 µs on HSI16) |
| t_STAB | power-up time | 0 | 0 | 1 | µs |
| I_DDA(ADC) | on V_DDA, 1.14 Msps / 10 ksps | — | 200 / 40 | — | µA |

**Accuracy** (after internal calibration, 1.8–3.6 V, any voltage range):
E_T ≤ 4 LSB, E_O ≤ 2.5 LSB, E_G ≤ 2 LSB, E_L ≤ 2.5 LSB, E_D ≤ 1.5 LSB.
ENOB 11 bits typ (12.1 bits with ×256 oversampling), SNR 68 dB (76 dB oversampled),
SINAD 67.8 dB, THD −81 dB typ.

**Fast channels:** PA0 (IN0), PA4 (IN4), PA5 (IN5). All other channels are standard and carry an
extra protection resistance that limits the allowable source impedance at low V_DD.

**Max source impedance R_AIN at f_ADC = 16 MHz (kΩ):**

| t_S (cycles) | t_S (µs) | Fast ch. | Std, V_DD > 2.7 V | > 2.4 V | > 2.0 V | > 1.8 V |
|---|---|---|---|---|---|---|
| 1.5 | 0.09 | 0.5 | < 0.1 | — | — | — |
| 3.5 | 0.22 | 1 | 0.2 | < 0.1 | — | — |
| 7.5 | 0.47 | 2.5 | 1.7 | 1.5 | < 0.1 | — |
| 12.5 | 0.78 | 4 | 3.2 | 3 | 1 | — |
| 19.5 | 1.22 | 6.5 | 5.7 | 5.5 | 3.5 | — |
| 39.5 | 2.47 | 13 | 12.2 | 12 | 10 | — |
| 79.5 | 4.97 | 27 | 26.2 | 26 | 24 | < 0.1 |
| 160.5 | 10.03 | 50 | 49.2 | 49 | 47 | 32 |

- **Calibrate after every power-up.**
- Auto-shutdown keeps the ADC powered off except during the active conversion phase.
- Board parasitic capacitance (~7 pF pad plus PCB) degrades accuracy — keep analog traces short,
  or reduce f_ADC.
- Hardware oversampler up to ×256 → 16-bit result (see AN2668).
- Analog watchdog with programmable thresholds; TIMx events can trigger conversions;
  DMA-serviceable; single-shot or scan mode.

---

## 11. Communication interface timing

**I2C** — meets the I2C-bus specification for **Standard mode (100 kbit/s)** by design when the
I2CCLK frequency is above 2 MHz. SDA/SCL are **not true open-drain**: in open-drain mode the PMOS
to V_DDIO is disabled but still present, so **do not pull SDA/SCL above V_DD**. The analog filter
suppresses spikes narrower than 50 ns and passes those wider than 100 ns.

**SPI** — maximum f_SCK:

| Voltage range | Master | Slave receiver | Slave transmitter |
|---|---|---|---|
| Range 1 | 16 MHz | 16 MHz | 12 MHz (1.71–3.6 V) / 16 MHz (2.7–3.6 V) |
| Range 2 | 8 MHz | 8 MHz | 8 MHz (2.7–3.6 V) |
| Range 3 | 2 MHz | 2 MHz | 2 MHz |

Master mode, Range 1: data input setup t_su(MI) ≥ 3 ns, hold t_h(MI) ≥ 3.5 ns, data output valid
t_v(MO) ≤ 6 ns, output hold t_h(MO) ≥ 3 ns. SCK duty 30–70 % in slave mode.
Measurement points are at 0.3 V_DD / 0.7 V_DD.

**Timers:** resolution 1 t_TIMxCLK (31.25 ns @ 32 MHz); external clock on CH1–CH4 up to
f_TIMxCLK/2 (16 MHz @ 32 MHz); 16-bit counters; maximum count 65536 × 65536
(134.2 s @ 32 MHz).

**Flash / EEPROM endurance:** program memory 10 kcycles, EEPROM 100 kcycles; data retention
30 years at 85 °C after full cycling. Program/erase time 3.28 ms typ, 3.94 ms max.
Operating voltage for read / write / erase: 1.8–3.6 V.

---

## 12. Package and footprint (TSSOP20)

**TSSOP20 — 20-lead thin shrink small outline, 6.5 × 4.4 mm body, 0.65 mm pitch, 169 mils.**

| Symbol | Min | Typ | Max (mm) |
|---|---|---|---|
| A (overall height) | — | — | 1.200 |
| A1 (standoff) | 0.050 | — | 0.150 |
| A2 (body thickness) | 0.800 | 1.000 | 1.050 |
| b (lead width) | 0.190 | — | 0.300 |
| c (lead thickness) | 0.090 | — | 0.200 |
| D (body length) | 6.400 | 6.500 | 6.600 |
| E (overall width incl. leads) | 6.200 | 6.400 | 6.600 |
| E1 (body width) | 4.300 | 4.400 | 4.500 |
| e (pitch) | — | 0.650 | — |
| L (foot length) | 0.450 | 0.600 | 0.750 |
| L1 | — | 1.000 | — |
| k (lead angle) | 0° | — | 8° |
| aaa (coplanarity) | — | — | 0.100 |

D excludes mold flash / protrusions (≤ 0.15 mm per side); E1 excludes interlead flash
(≤ 0.25 mm per side).

**Recommended footprint (mm):** 20 pads of **0.40 × 1.35 mm** on **0.65 mm pitch**;
**7.10 mm** outer land span, **4.40 mm** gap between the inner edges of the two pad rows;
each pad row spans **6.25 mm** (= 9 × 0.65 + 0.40).

---

## 13. PCB design checklist

1. **Supplies:** V_DD (pin 16) and V_DDA (pin 5) must be fed from the same source, within 300 mV,
   1.8–3.6 V. A single V_SS (pin 15) serves both — **VSSA is internally bonded**, there is no
   separate analog ground pin on this package.
2. **Decoupling** (datasheet power-supply scheme): **100 nF per V_DD pin plus one 1–10 µF bulk**
   on V_DD, and **100 nF + 1 µF** on V_DDA. Place the 100 nF caps against pins 16 and 5 with the
   shortest possible return loop to pin 15.
3. **NRST (pin 4):** 100 nF to ground, close to the pin. Nothing else needed.
4. **BOOT0 (pin 1):** input only. Tie low (or rely on the option-byte configuration) for normal
   Flash boot; provide a pull-up or jumper if the USART2/SPI1 system bootloader is wanted as a
   recovery/programming path.
5. **SWD:** keep PA13/PA14 (pins 19/20) accessible; route SWCLK/SWDIO short with a ground return.
6. **LSE (pins 2/3):** crystal and load caps as close as possible, ground-guarded; **no series
   resistor between OSC32_IN and OSC32_OUT**. If no RTC crystal is used, PC14/PC15 are ordinary
   I/Os — note PC15 is TC (3.3 V only, **not** 5 V-tolerant).
7. **HSE:** only a **square-wave external clock** into PA0-CK_IN is supported — no crystal
   oscillator circuit on this device. HSI16 (±1 %) or MSI is usually sufficient and saves parts.
8. **Analog:** keep ADC traces short and low-impedance (see the R_AIN table); add a Schottky diode
   to ground on any analog pin that could inject negative current; PA0/PA4/PA5 cannot take
   positive injection at all.
9. **5 V tolerance:** every I/O on TSSOP20 is FT except **PC15 (TC)**, the supply pins and NRST.
   Disable the internal pull-up/pull-down on any FT pin held above V_DD + 0.3 V.
10. **Current budget:** ≤ 16 mA per I/O, ≤ 90 mA total across all I/Os, ≤ 105 mA through the
    supply pins. Θ_JA = 74 °C/W and the package allows 270 mW at T_A = 85 °C.

---

*Source: STMicroelectronics DS12323 Rev 3, "STM32L010F4/K4 — Value line ultra-low-power 32-bit MCU
Arm Cortex-M0+, 16-Kbyte Flash memory, 2-Kbyte SRAM, 128-byte EEPROM, ADC", August 2019.
Companion documents: RM0451 (reference manual), AN2606 (bootloader), AN2867 (oscillator design),
AN2668 (ADC oversampling), AN1709 / AN1015 (EMC).*
