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
report temperature, humidity and three battery datapoints (percentage, level, and
whether the cell looks damaged), plus one with the lowest the cell was pulled to
in the windows before (§5), and put it away again. Then sleep.

A press of the Wi-Fi button opens the same window for pairing instead, and can
interrupt the sleep to do it. A device that isn't paired opens no report windows
at all until the button pairs it (§4).

**There is no heartbeat.** A temperature that never moves is never reported, and
the Tuya app keeps showing the last value it was given. This is deliberate — see
§4 — and it is the reason two other things in the code look the way they do: the
first reading after boot always reports, and a report is only "spent" once it has
actually reached the cloud.

## 2. Layout

```
libs/src/bsp/bsp.h          the portability boundary — one implementation per board
libs/src/sensor/            HDC3020 driver (trigger-on-demand, CRC-checked)
libs/src/battery/           NiMH cell voltage and sag -> percent, level, damaged
libs/src/report/            the spike filter and the "is this worth the radio" rule
libs/src/tuya_link/         the glue between the cycle and the vendor MCU SDK
temp_sensor_main/           the cycle itself
tuya/                       the vendor Tuya MCU SDK, ported (§7)

temp_sensor_STM32L010F4/    CubeMX project: HAL, startup, and
  Core/Src/bsp_stm32.c        >>> the only file that knows what the hardware is
libs/test/                  host GoogleTest suite (119 tests) over the same C
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

The ADC interrupt behind the sag monitor (§5) lives there for the same reason:
`ADC1_IRQHandler` is defined in `bsp_stm32.c`, and its NVIC line is enabled by the
monitor's start and disabled by its halt, never left on.

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
reset line; if it never answers, the cycle ends and a later one tries again (§4).

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

### Backing off after failed windows

A failed window is the most expensive thing the device does: about 29 s with the
module powered (~4 s to alive, then the 25 s cloud timeout). With the router down
every window fails, and retrying the outstanding report on every cycle would keep
the module powered ~10 % of the time — a flat cell in three or four days. So
[`temp_sensor_main.c`](../temp_sensor_main/temp_sensor_main.c) retries twice and
then pauses:

| Failed report windows in a row | Next report window no earlier than |
|---|---|
| 1 or 2 | 5 min after the failed one (`REPORT_RETRY_MS`) |
| 3 | 2 h after it (`REPORT_PAUSE_MS`) |

- **Only a window that reaches the cloud gives the retries back.** The try at the
  end of a pause gets none of its own, so a long outage costs one window every
  2 h, about 12 a day. The retries after boot follow the same rules.
- **A held-back report is not lost.** Nothing was committed, so it is still
  outstanding when the backoff runs out. The cycle keeps measuring and caching
  throughout.
- **The Wi-Fi button always opens a window, but not a "retry now".** It opens a
  pairing window. One that reaches the cloud clears the backoff; one that doesn't
  changes nothing, because the module spent it waiting for a phone and never tried
  the router.
- **The Reset button is "retry now".** The backoff lives in RAM, so after a reset
  the first cycle reports.
- **Times, not cycles**, so a shorter cycle period cannot shorten the retry. They
  are measured on the schedule (`next_cycle_ms`), not off the clock. A cycle
  starts a few ms after its slot, by a different few each time, and a retry timed
  off the clock would miss the next cycle about half the time.

### Not reporting while unpaired

A module that was never paired, or was removed in the app, has no network to try,
so a report window would wait out the whole cloud timeout for nothing. Until the
first report after boot lands, the policy asks for one on every cycle, so the
backoff alone would still spend about 12 of those windows a day.

The module's first state report says whether it is paired. `SMART_CONFIG_STATE`
(0x00), `AP_STATE` (0x01) and `SMART_AND_AP_STATE` (0x06) mean it is waiting to
be paired (`Tuya_IsUnpaired()`). A router that is down looks different: the
module reports 0x02 and gets the backoff instead.

- **A report window that finds the module unpaired powers off at once**, about
  4 s in rather than 29, and sets `module_unpaired`.
- **While it is set, no report window opens**, including the retries after boot.
  The cycle keeps measuring and caching, so a pairing window has current values
  to push.
- **Only a pairing window that reaches the cloud clears it.** One that nobody
  finishes leaves it set. A pairing window is never cut short this way, since
  waiting to be paired is what it is for.
- **It lives in RAM.** After a reset or a battery swap, the first report window
  finds out again, at the cost of one ~4 s window. No EEPROM needed.

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
4. **The sag monitor never runs into a sleep.** The window stops it once the
   supply is off, and `BSP_McuSleep` halts it first anyway (below).

### The sag monitor: the cell's minimum under load

The cell pin is the boost's input, after the holder. During a window the module's
transmit bursts pull it down. On the office board's Eneloop the scope shows 1.30 V
dipping to 0.96 V in short peaks. A boost draws roughly constant power, so the
input collapses once the cell and its path can no longer deliver it, no matter how
much charge is left. How deep the dips go is the early warning for that.

The dips are too short for a reading from thread context to catch, so the BSP
samples them from an interrupt
([`BSP_Battery_StartSagMonitor`](../libs/src/bsp/bsp.h)):

- **The ADC runs continuously** over the cell pin and VREFINT, back to back in one
  scan. Its end-of-conversion interrupt keeps the pair with the lowest
  cell/VREFINT ratio.
  - Each sample carries its own reference because the failing case is exactly when
    the boost stops holding 3.3 V, and a sagging VDDA reads a sagging cell high.
- **Wait mode** holds each conversion until the last result has been read. So a
  late interrupt leaves a gap in the sampling, never a torn pair. That is also why
  its priority is the lowest there is.
- **Rate:** about 86 µs per result (VREFINT's 160.5-cycle sampling, 4×
  oversampling), so a pair every ~171 µs, or ~11,700 interrupts a second, and
  every one ends a `BSP_Idle`. Invisible next to the module's tens of milliamps.
  A dip much shorter than ~100 µs is averaged rather than caught.
- **Coverage:** the window starts the monitor before the supply comes up, so the
  inrush counts, and stops it after the supply is off. `BSP_McuSleep` halts it
  before Stop whatever the caller did. The halt disables the NVIC line first, then
  the ADC interrupt, the conversion, continuous and wait mode, and VREFINT's
  channel, leaving the ADC as `BSP_Battery_ReadMv` expects it.

The result goes out as **DP 101** (`battery_min_voltage`, mV) on the *next*
report. That report carries the lowest value across every window since the last
one that reached the cloud, failed windows included. A window whose sag browned
the module out is the one most worth knowing about, and the one that could not say
so itself. Checked against the scope on 2026-11-03: 945 mV reported against a
961 mV scope minimum, with the cell resting at 1.286 V. A minimum reads slightly low
by nature, since it also picks up the deepest noise spike. A window's own minimum is
not reported in that window: the module
queries every DP twice per window, and a value that changed mid-window would
put two different entries into the cloud log. The first window after a reset has
nothing to report and sends no DP 101. Nor does a minimum of 1000 mV or above:
the boost still has plenty of margin there, and the DP would only be noise in the
app's history (the threshold is in
[`tuya_link.c`](../libs/src/tuya_link/tuya_link.c)).

**The DP has to exist on the Tuya platform:** ID 101, Value, report only,
0..3300, step 1, scale 0, unit mV. See the `PORTED` note in
[`protocol.h`](../tuya/protocol.h).

### The gauge under load

A cell can have plenty of charge and still be unable to power a report, so the
sag feeds the battery percentage and a damaged flag
([`battery.h`](../libs/src/battery/battery.h)).

**Collapse voltage.** The boost draws roughly constant power, so at the bottom of
a burst `P·R = (Vrest − Vmin)·Vmin`, with no current measurement needed. The path
delivers the most at Vmin = Vrest/2, and the cell stops powering reports at a
resting voltage of `Vc = 2·√(P·R)`. Two cells measured on the office board are the
reference points:

| Cell | Vrest | Vmin | Vc |
|---|---|---|---|
| Healthy Eneloop | 1.286 V | 0.945 V | ~1.135 V |
| Failed Eneloop | 1.29 V | 0.58 V | past the limit: below Vrest/2 |

The failed cell's limit matches the 1.28 V it stopped working at on the front board.
Three details in the code:

- **A sag to half the resting voltage or below reads as collapsing at the resting
  voltage.** P·R turns back down below that point, so at face value a deeper sag
  would read as a healthier cell.
- **No square root.** `Vc ≈ 600 + P·R/600` mV is the tangent at 1.2 V. It only ever
  reads high, by at most 4 mV between 1.10 and 1.30 V and 9 mV at 1.05 and 1.35 V.
- **A resting reading above 2 V gives no sample.** No AA rests that high, so it is a
  bad reference read. Taken as a cell's first window, it would read 0 % and damaged
  for weeks.

**Trend.** Each window gives one Vc sample, set against that cycle's resting voltage.
Failed windows count too. The first sample on a cell sets the trend outright; after
that each one is weighed at 1/4, and its pull is capped at ±60 mV first. So no single
window moves the trend more than 15 mV: one window as deep as the failed cell's
takes the healthy cell from 63 % to 61 %, not to 0 %. A sustained change gets there
in about 7 windows. The price of taking the first window whole: if that one is an
outlier as deep as the failed cell's, a healthy cell reads 0 % for four windows and
damaged for six. The trend lives in RAM and is cleared on reset. A swap the MCU
rides through on the 3.3 V rail's ~100 µF is caught by the resting voltage rising
50 mV or more since the last cycle, which a cell in use never does. That also clears
DP 101's carried minimum.

**Percentage.** The 0 % point moves up from 1.10 V to Vc + 50 mV, and the curve above
it is stretched back over 0–100. So a full cell still reads 100, and 0 % arrives
while reports still get out. Today's healthy office cell reads 63 % rather than the
plain curve's 67 %. The failed cell reads 0 %, Low.

**DP 102 `battery_damaged`** is true from a trend of 1210 mV up, midway between the
two cells. Vc never exceeds the resting voltage, so only a cell still resting at
1.21 V or more can trip it: an ordinary cell running flat reads empty, not damaged.
It is sent false as well, so a good cell clears it in the app. Recharging doesn't
hide a bad cell. If the failed cell kept its resistance at 1.39 V, it would sag to
only ~0.97 V, about what the healthy cell does today, yet still collapse at ~1.28 V.
The minimum alone can't tell the two apart.

**The DP has to exist on the Tuya platform:** ID 102, identifier `battery_damaged`,
Bool, report only.

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
  13504       8    1624   15136    3b20
```

`bss` includes the linker script's 1024-byte `._user_heap_stack` reservation, so
static RAM is ~608 bytes and the stack has ~1440 available. Flash is 13.5 KB of
16 KB, leaving ~2.8 KB. (When `-Os` gave 12.5 KB, the same tree at `-O2` was
13.6 KB, so expect about 1.1 KB more at `-O2`.)

**If the link starts failing on `region RAM overflowed`, the first place to look
is `WIFI_UART_RECV_BUF_LMT` / `WIFI_DATA_PROCESS_LMT` in
[`../tuya/protocol.h`](../tuya/protocol.h)** — and the answer is not to shrink
them (§7).

## 7. The vendor Tuya SDK

`tuya/` is Tuya's generated MCU SDK for this product: the **standard** Wi-Fi MCU
protocol, v2.6.2. Not the low-power one — the module firmware Tuya's burning tool
flashes for this PID (3.0.93) speaks the standard protocol, and the two share a
frame format but not a command set. A low-power SDK talking to it ignores the
module's opening heartbeat, the module never gets past it, and the window times
out with nothing on the wire but heartbeats. That is how the first bring-up
failed.

Every edit made to the SDK is marked `PORTED:` with the reason. They are:

| File | Change |
|---|---|
| `protocol.c` | `uart_transmit_output` → `BSP_Wifi_TransmitByte`; `all_data_update` → `Tuya_ReportCachedDps` |
| `protocol.h` | receive/process buffers raised from 16/24 to 32/32; `WIFI_TEST_ENABLE` off; notes on `CONFIG_MODE` and the send buffer |
| `mcu_api.c` | four `#error` porting markers removed; `wifi_uart_service`'s fill level hoisted so `wifi_protocol_init` resets it; the oversize-frame branch fixed (it allowed a frame the buffer cannot hold, and left the bad header in place to be found again on every pass — **a wedged parser for the rest of the window**); `mcu_reset_wifi` forgets the cached Wi-Fi state |
| `system.h` | `rx_buf_in` / `rx_buf_out` made `volatile` pointers — they are shared with the receive interrupt, and only the bytes they point at were |

The ring size is load-bearing and **fails silently when too small**.
`uart_receive_input()` drops a byte without a word when the ring is full — no UART
error, nothing any counter can see — so the ring has to cover the longest gap
between `wifi_uart_service()` calls. The longest gap here is `all_data_update()`:
`uart_transmit_output` blocks a byte at a time, so six DP frames (84 bytes) hold
the loop for ~88 ms while the module is free to send a heartbeat or a status
change. Those are 7–8 bytes each; 32 gives a 39-byte ring. The process buffer is
margin rather than a cliff now that an oversize frame is discarded.

**Pairing** is the reset command (`0x04`, `mcu_reset_wifi`), not "reset and select
mode" (`0x05`): the T3-3S pairs over Bluetooth or AP, and `0x04` lets it use both.
The module restarts on it, which is why the SDK has to forget its cached state —
otherwise a module that was online when the button was pressed reads as "paired"
the instant the command goes out. The product runs Tuya's anti-misoperation mode
(`"m":2`): an unpaired reset goes back to the old network after three minutes, the
same three minutes the pairing window waits.

The remaining compiler warnings in `tuya/` (a misleadingly indented
`tuya_strncpy`, an unused local in a `#ifdef`-thinned switch) are untouched vendor
code and are left alone on purpose.

## 8. Tests

`libs/build_and_test.bat` — 119 GoogleTest cases, ~2.6 s, run from **PowerShell or
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
- **No persistent settings.** All six DPs are report-only; nothing comes down
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
datasheet-shaped estimate, not bench data,** except for the full anchor. That one
moved from 1.40 V to 1.32 V on field data: three cells all read 1.32 V on a DMM a
few days off the charger, and at 1.40 V no cell ever read 100 %. The plateau
probably reads low too. The office Eneloop read 71 % at 1.28 V after 24 days, when
the battery budget says it had used only ~8 %. The curve puts things in the right
place and gives the gauge a shape to interpolate along; a discharge run on the
real board should replace it. The table is written as anchors precisely so that is an edit and
not a rewrite, and `battery_test` pins the properties that must hold whatever the
numbers are (monotonic, clamped at both ends, most of the scale on the plateau).

Also not yet measured with this firmware: the T3-3S boot and cloud-connect times
that `WIFI_ALIVE_TIMEOUT_MS` and `WIFI_CLOUD_TIMEOUT_MS` are sized from. The only
data so far is a Tuya MCU-simulator capture of a first pairing: ~3 s from the
module's first heartbeat to its first state report, and ~9 s from "configured" to
"cloud" — which is why the alive timeout is 15 s.

Not yet checked on hardware either: what state the module reports on the boot after
a pairing window nobody finished. Not reporting while unpaired (§4) relies on two
things:

- **A module with no network** should come back in a pairing state (0x00, 0x01 or
  0x06), not 0x02. If it reports 0x02, it looks like a down router and gets the
  backoff: about 12 full 29 s windows a day instead of none.
- **A module that was paired** must not. The pairing window cuts power at about
  the moment anti-misoperation mode would restore the old network. Tuya documents
  that a power cycle after a reset and before re-pairing also restores it. If this
  module firmware doesn't, a stray press would leave a paired device silent until
  someone pairs it again.
