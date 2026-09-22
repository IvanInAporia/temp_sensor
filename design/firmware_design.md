# temp_sensor firmware — design

What the device does, how the code is arranged, and which decisions are load-bearing.

| | |
|---|---|
| Target | STM32L010F4P6, **16 KB flash / 2 KB RAM**, Cortex-M0+ |
| Board | temp_sensor v2 (EasyEDA Pro) — see [`../easyeda/review-notes.md`](../easyeda/review-notes.md) |
| Sensor | TI HDC3020 on I2C1, permanently powered |
| Radio | Tuya T3-3S Wi-Fi module on LPUART1, **power-gated** |
| Supply | One NiMH AA cell → TPS61021A boost → 3.3 V |

---

## 1. What it does

Every five minutes: read the cell, read the sensor, decide whether the temperature
has moved enough to be worth telling anyone, and if so bring up the Wi-Fi module,
report four datapoints, and put it away again. Then sleep.

A press of the Wi-Fi button opens the same window for pairing instead, and can
interrupt the sleep to do it.

**There is no heartbeat.** A temperature that never moves is never reported, and
the Tuya app keeps showing the last value it was given. This is deliberate — see
§4 — and it is the reason two other things in the code look the way they do: the
first reading after boot always reports, and a report is only "spent" once it has
actually reached the cloud.

## 2. Layout

```
libs/src/bsp/bsp.h          the portability boundary — one implementation per board
libs/src/sensor/            HDC3020 driver (trigger-on-demand, CRC-checked)
libs/src/battery/           NiMH cell voltage -> percent, and the 3-level DP enum
libs/src/report/            the spike filter and the "is this worth the radio" rule
libs/src/tuya_link/         the glue between the cycle and the vendor MCU SDK
temp_sensor_main/           the cycle itself
tuya/                       the vendor Tuya MCU SDK, ported (§7)

temp_sensor_STM32L010F4/    CubeMX project: HAL, startup, and
  Core/Src/bsp_stm32.c        >>> the only file that knows what the hardware is
libs/test/                  host GoogleTest suite (66 tests) over the same C
```

Everything above `bsp.h` compiles for both the target and the host, unchanged and
uncopied: the CubeMX project links `libs/src/*` and `temp_sensor_main/` in as
linked folders, and `libs/CMakeLists.txt` compiles the same files for the tests.
Only the two edges differ — the board (`bsp_stm32.c` vs `test/mocks/bsp_mock.c`)
and the vendor SDK (real vs `test/mocks/tuya_sdk_mock.c`).

**Porting to another board is writing one new file against `bsp.h`.** Nothing
above that header may include `main.h`, a HAL header or a CMSIS header, and no
pin number, peripheral handle or register name belongs on that side of it.

### The LPUART1 interrupt lives in `bsp_stm32.c`, on purpose

The Tuya SDK needs a byte-level receive interrupt armed for the whole time the
module is powered, and the `.ioc` does not enable the LPUART1 global interrupt —
so CubeMX generates neither the vector nor the NVIC call.

The obvious fix is to add it to the `.ioc` and hand-patch `stm32l0xx_it.c` and
`stm32l0xx_hal_msp.c` to match. **That was tried during this work and it silently
destroyed the receive path.** The project was regenerated, everything outside a
USER CODE block went with it, and the build still succeeded afterwards because
nothing references what went missing. The same regeneration also reverted LPUART1
and I2C1 from HSI16 to PCLK1, the I2C timing, the RTC prescaler and PA7's EXTI —
none of which any test or build could see.

So both halves live in [`bsp_stm32.c`](../temp_sensor_STM32L010F4/Core/Src/bsp_stm32.c),
which CubeMX never touches: `LPUART1_IRQHandler` overrides the weak vector in the
startup file, and the NVIC line is enabled once in `BSP_Init()`. If a future
`.ioc` revision ever does enable that interrupt, both definitions will exist and
the link will fail with a duplicate symbol — the loud version of the same problem,
and the right one to have.

**Do not "tidy" this into the generated files.**

## 3. The cycle is straight-line code

There is no scheduler, no task, no coroutine and no state machine.
[`temp_sensor_main.c`](../temp_sensor_main/temp_sensor_main.c) is a list of steps
in the order they happen, and a wait is a call that returns when the wait is over:

```c
bool pairing = PairingRequested();

uint16_t battery_mv = BSP_Battery_ReadMv();     /* before any load is switched on */

if (Hdc3020_Read(&reading))
{
    report_due = ReportPolicy_Update(&report_policy, reading.temp_c10, &temp_c10);
    Tuya_SetDps(temp_c10, reading.rh_pct, battery_pct, battery_state);
}

if (report_due || pairing)
{
    RunWifiWindow(report_due, pairing, temp_c10);
}

WaitForNextCycle();
```

This is affordable because **nothing on this device is concurrent.** One job per
wake; the sensor read is a blocking 15 ms; during a Wi-Fi window the module is the
only thing drawing current and there is nothing else that needs the CPU. The only
concurrency is the LPUART receive interrupt, which pushes one byte into the Tuya
SDK's ring buffer and re-arms itself.

What makes the blocking waits cheap is `Tuya_WaitUntil()`: it blocks, but it
services the SDK and idles the core on every pass, so "wait 25 seconds for the
cloud" costs interrupt wakeups rather than a spin.

**The rule that keeps this honest: every wait has a deadline.** Nothing may wait
on the module forever. It is a separate processor on the far end of a UART with no
reset line; if it never answers, the cycle ends and tries again in five minutes.

## 4. Why measuring and reporting are priced differently

One HDC3020 conversion is ~110 µA for 13 ms. One report is the T3-3S cold-booting,
associating and reaching the cloud — seconds at tens of milliamps, and on a single
NiMH cell feeding a 3.3 V boost, roughly three times that current *at the cell*.

That ratio is the whole battery life, so the cycle measures often and reports
rarely. The rule, in [`report_policy.h`](../libs/src/report/report_policy.h):

- **A spike must not buy a report.** A median of the last three samples is the
  whole filter — it rejects any single outlier outright, whatever its size. The
  cost is one sample of lag on a genuine step, which at a five-minute cadence
  means a real change is reported up to two periods after it starts.
- **Only temperature triggers.** Humidity is far noisier and would undo the first
  rule; the battery moves so slowly that any useful threshold would take days to
  cross. Both ride along on whatever report the temperature triggers.
- **`REPORT_TEMP_DELTA_C10` is 0.3 °C**, a little over the sensor's own ±0.2 °C
  typical accuracy, so a report means the temperature moved rather than that the
  sensor wandered. This is the knob for battery life vs responsiveness, and with
  no heartbeat it is also the knob for how stale the app may get.

### Deciding and committing are two calls

`ReportPolicy_Update()` decides; `ReportPolicy_ReportSent()` commits. A window that
never reached the cloud simply never commits, so the next cycle still finds the
change outstanding. Without a heartbeat there is nothing else that would ever
repair a dropped report — in a stable room it could be lost for good.

## 5. Power

All datasheet figures; none of this has been measured on the board yet.

| | |
|---|---|
| Stop, core + RTC on LSI | ~1 µA (datasheet) |
| **VREFINT buffer left on in Stop** | **3.0 µA** — the largest single item, so it is switched off before every sleep and brought back (3 ms) by the battery read |
| ADC in Stop | not a listed adder; the ADC's own regulator is left alone |
| T3-3S, connected | 48 mA average, 300 mA peak |

Three ordering rules in the code exist for power or for the hardware, not for
style:

1. **The cell is read before the Wi-Fi rail comes up.** A reading taken during a
   report measures the sag, not the charge.
2. **Power before drive, drive before power-off.** PA2 idles high push-pull;
   driving it into an unpowered module back-feeds VBAT through the module's input
   protection, and U5's QOD is unconnected so nothing would discharge that rail.
   `Wifi_RailOn`/`Wifi_RailOff` in `main.c` own this ordering.
3. **Every path out of a Wi-Fi window cuts the supply.** Leaving the module
   powered is the one mistake that empties the cell outright.

## 6. Flash and RAM

16 KB is the binding constraint and it is not comfortable. The bare CubeMX
skeleton alone was **14.1 KB at `-O0`**, which is why:

- **The Debug configuration is built `-Os -flto`, not `-O0`.** At `-O0` the image
  does not fit, with or without application code. Debugging is against optimised
  code; that is the trade this part forces.
- **`syscalls.c` and `sysmem.c` are excluded from the build.** They are CubeMX's
  newlib stubs, nothing calls them, and their `errno` reference pulled in
  `_impure_data` and newlib's stdio `__sf` — **388 bytes of RAM, 19 % of the
  part**, for functions that are never reached. `--specs=nosys.specs` provides the
  stubs the linker still wants.

Current figures (Debug, `-Os -flto`):

```
   text    data     bss     dec     hex
  12180       8    1584   13772    35cc
```

`bss` includes the linker script's 1024-byte `._user_heap_stack` reservation, so
static RAM is ~568 bytes and the stack has ~1480 available. Flash is 12.2 KB of
16 KB, leaving ~4.2 KB.

**If the link starts failing on `region RAM overflowed`, the first place to look
is `WIFI_UART_RECV_BUF_LMT` / `WIFI_DATA_PROCESS_LMT` in
[`../tuya/protocol.h`](../tuya/protocol.h)** — and the answer is not to shrink
them (§7).

## 7. The vendor Tuya SDK

`tuya/` is Tuya's generated MCU SDK for this product. Every edit made to it is
marked `PORTED:` with the reason. They are:

| File | Change |
|---|---|
| `protocol.c` | `uart_transmit_output` → `BSP_Wifi_TransmitByte`; `all_data_update` → `Tuya_ReportCachedDps`; `dp_download_handle` returned an **uninitialised** `ret` |
| `mcu_api.c` | three `#error` porting markers removed (reminders, not gaps) |
| `system.c` | `Queue_Read_Byte` returned an **uninitialised** local on the empty-queue path |
| `protocol.h` | receive/process buffers raised from 16/24 to 32/32 |
| `wifi.h` | dropped `#include "main.h"` — the SDK uses nothing from the board header, and including it made every file that includes `wifi.h` target-only, which is why none of it could be host-compiled |

The two buffer sizes are load-bearing and **both fail silently when too small**.
`uart_receive_input()` drops a byte without a word when the ring is full — no UART
error, nothing any counter can see — so the ring has to cover the longest gap
between `wifi_uart_service()` calls. The longest gap here is one outgoing DP
report: `uart_transmit_output` blocks a byte at a time, so a 13-byte frame holds
the loop for ~13 ms while the module is free to talk back. 32 gives 39 bytes,
~40 ms of wire time at 9600. Tuya's 16 gives 23 bytes, ~24 ms — under half a frame
of margin.

The remaining compiler warnings in `tuya/system.c` (`char*`/`unsigned char*`
signedness, two unused locals in a `#ifdef`-thinned switch) are untouched vendor
code and are left alone on purpose.

## 8. Tests

`libs/build_and_test.bat` — 66 GoogleTest cases, ~1.6 s, run from **PowerShell or
cmd, not Git Bash** (the preset pins MSYS2 UCRT64 and Git Bash's environment leaks
into it, killing every link step).

Time in the host BSP is explicit, not real: `BSP_GetTimeMs()` only moves when
something that takes time on the device takes time in the mock too. So a test
never waits, and a timeout expires for a reason the test chose.

`temp_sensor_main_test` runs the real cycle against the real sensor driver, gauge,
policy and Tuya glue, with only the board and the vendor SDK faked. What it
asserts is what only exists once the steps are in a line: when the radio comes up,
how long each wait is given, what is measured before the load is switched on, and
that the module is never left powered.

## 9. Things deliberately not done

- **No watchdog.** IWDG on this part maxes out around 26 s against a 5-minute
  sleep, so it would have to be re-armed on every RTC wake and would not cover the
  sleep itself. Worth revisiting if field units are seen to wedge.
- **No persistent settings.** All four DPs are report-only; nothing comes down
  from the cloud, so there is nothing to store. The part's 128 bytes of EEPROM are
  untouched.
- **No I2C bus recovery.** An MCU reset in the middle of a sensor read could in
  principle leave the HDC3020 holding SDA. Not observed; a nine-clock recovery in
  `BSP_Init` is the fix if it ever is.
- **No battery hysteresis.** The three-level DP is derived statelessly from the
  percentage. Reports are minutes to hours apart, so a level sitting exactly on a
  threshold alternates slowly rather than flapping. Add hysteresis if a bench
  discharge says otherwise.

## 10. The numbers that are still estimates

**The NiMH discharge curve in [`battery.c`](../libs/src/battery/battery.c) is a
datasheet-shaped estimate, not bench data.** It puts the curve in the right place
and gives the gauge a shape to interpolate along; a discharge run on the real board
should replace it. The table is written as anchors precisely so that is an edit and
not a rewrite, and `battery_test` pins the properties that must hold whatever the
numbers are (monotonic, clamped at both ends, most of the scale on the plateau).

Also unverified on hardware: the T3-3S boot and cloud-connect times that
`WIFI_ALIVE_TIMEOUT_MS` and `WIFI_CLOUD_TIMEOUT_MS` are sized from, and the
assumption that the SDK's `SMART_CONFIG` selects the module's Bluetooth pairing
mode (the T3-3S datasheet lists Bluetooth and AP, not the older EZ broadcast).
