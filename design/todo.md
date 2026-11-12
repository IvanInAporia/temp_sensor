# Report policy and Wi-Fi window: todo

Agreed 2026-10-10 from a review of the temperature processing and the decision of when to
transmit. Battery life comes first, but the app should still show an accurate reading
promptly.

**Cost model behind every item.** One report costs about 0.5 mAh at the cell (estimated
from the Eneloop field run), so a charge holds roughly 4,000 reports. Reports per day set
the battery life almost by themselves. A sensor reading costs tens of thousands of times
less than a report.

Each item is meant to be one session. When an item is done, update
[`firmware_design.md`](firmware_design.md) (sections 4, 9 and 10 cover most of this) and
tick it off here.

Suggested order: 1a → 1b → 1c → 2 → 4. Item 5 doesn't depend on anything. Item 3 was
dropped on 2026-10-11; item 2's 4 h report covers it. Item 6 came out of the kitchen
board's silence on 2026-11-11 and was done the same day, together with item 5.

The checks that need the board, the Tuya app or the MCU simulator are collected in
[Hardware and manual checks](#hardware-and-manual-checks) at the end, to do once the code
changes are in.

---

## 1a. Back off after failed windows

- [x] Done

**Problem.** A window that never reaches the cloud leaves the report uncommitted, so the
next cycle tries again five minutes later. The same happens after boot until the first
report gets through. If the router is down, each attempt keeps the module powered for about
29 s (about 4 s to alive, then the 25 s `WIFI_CLOUD_TIMEOUT_MS`), roughly 10 % of the time
at tens of mA. That empties the cell in about 3–4 days.

**Design.** Retry twice, then pause for 2 h. If the router is still down after three tries,
it is likely to stay down for a while.
- Keep two values: a count of failed report windows in a row, and the earliest time the
  next report window may open.
- After a failure, add 1 to the count. If the count is below 3, the next window may open
  5 min later (the next cycle at today's period). At 3 or more, it may open 2 h later.
- The count stops going up at 3 and resets only on a success. So when the 2 h runs out, the
  device tries once, and if that fails it pauses again. A long outage costs one window
  every 2 h, about 12 a day.
- While waiting, keep measuring and keep caching DPs, but open no report window.
- The 5 min gap between retries is a time, not "the next cycle". Item 4 would otherwise
  shrink it to 1–2 min, which is too short to wait out a router that is rebooting.
- Compare times with unsigned subtraction, like `WaitForNextCycle`.
  `BSP_GetTimeMs()` wraps after about 49.7 days.
- The same rules cover the retries after boot (today they repeat every cycle until the
  first report gets through).
- The Wi-Fi button still always opens a window, but it is not a "retry now": it puts the
  module into pairing mode. A pairing window that reaches the cloud counts as a success.
- The way to retry now is the board's **Reset** button (NRST). Both values live in RAM, so
  after a reset the device reports on its first cycle.

**Where.** [`temp_sensor_main.c`](../temp_sensor_main/temp_sensor_main.c) (`RunWifiWindow`
has to return whether the window succeeded). Add tests in
[`temp_sensor_main_test.cpp`](../libs/test/temp_sensor_main_test/temp_sensor_main_test.cpp):
- retries 5 min apart, then a 2 h pause
- one failure after the pause starts another pause
- a success resets both values
- the same rules after boot
- the button opens a window during a pause

## 1b. Leave an unpaired module unpowered until the button is pressed

- [x] Done

**Problem.** A device that was never paired, or was removed in the app, still opens a
report window on every cycle and waits out the full cloud timeout.

**What the protocol gives us.** The module's first network-status report (cmd 0x03) says
whether it is paired. `SMART_CONFIG_STATE` (0x00), `AP_STATE` (0x01) and
`SMART_AND_AP_STATE` (0x06) mean it is unpaired and in pairing mode. In the simulator
capture (`temp_sensor_provision_with_mcu_simulation.txt`), an unpaired module reported
`AP_STATE` about 3 s after its first heartbeat.

**Design.**
- Add `Tuya_IsUnpaired()` (or return the state) next to `Tuya_IsAlive()` in
  [`tuya_link.c`](../libs/src/tuya_link/tuya_link.c).
- If a report window (not a pairing window) comes alive in a pairing state, power off right
  away and set an `unpaired` flag in RAM. While the flag is set, open no report windows at
  all, including the "keep reporting until the first one lands" retries after boot. Keep
  measuring and caching so a pairing window has current values to push.
- Clear the flag only when a pairing window reaches the cloud.
- After a battery swap (MCU reset) the device learns the state again with one short window
  of about 4 s. No EEPROM needed.

**Changed 2026-11-11 (item 6):** the flag is gone. A window that finds the module unpaired
now starts the backoff's 2 h pause, without spending a quick retry, and the device checks
back at the end of every pause. A latch that only Reset or a pairing could clear was one
of the three ways the kitchen board could have gone silent on 11-11.

**Check on hardware:** the state an unfinished pairing window leaves, in
[Hardware and manual checks](#hardware-and-manual-checks).

## 1c. Detect router-down early with staged deadlines

- [ ] Done

**Problem.** "Router down" and "still associating" look the same to the MCU: the module
reports 0x02 (`WIFI_NOT_CONNECTED`) and stays there. The protocol has no "access point not
found" state, so the only way to detect a down router is to stop waiting sooner.

**Design.** Replace the single cloud wait with one deadline per step, each starting when the
previous step finished:

| Stage | Waits for | Proposed timeout |
|---|---|---|
| Alive | any status report | 8 s from power-on |
| Router | 0x03 `WIFI_CONNECTED` | 10 s from alive |
| Cloud | 0x04 `WIFI_CONN_CLOUD` | 8 s from router |

With these numbers, a down router gives up about 14 s after power-on instead of 29 s, and a
down internet connection about 22 s after.

**These timeouts are guesses.** Implement with them; the cold-boot timing in
[Hardware and manual checks](#hardware-and-manual-checks) sets the final values. Until then,
a timeout that is too short shows up as report windows failing on a working network.

---

## 2. Limit how often temperature reports go out, and report at least every 4 h

- [x] Done 2026-11-07. Three changes from the plan below, agreed before implementing:
  - Any window that reaches the cloud commits this cycle's reading, pairing windows
    included, unless the sensor read failed. A pairing window pushes the current reading
    too, and without the commit the app could show a value up to ~1 °C off for up to 4 h.
  - The time since the last report stops growing at 4 h (like `ReportBackoffOver`), so it
    can't wrap. This replaces the comment about the 49.7-day wrap.
  - Numbers kept at 1.0 °C / 30 min after replaying the week's cloud logs: front board
    84 → ≈44 reports a day, kitchen 12 → 10, office 16 → 15 (firmware_design.md §4).
    A 60 min hold-off would have given the front ≈34. Retune after the post-flash count.

**Problem.** Two opposite problems, both about the time since the last report.
- *Too many.* Reporting whenever the temperature moves 0.3 °C keeps the reading within
  0.3 °C, but the number of reports per day has no limit. A heating/cooling cycle that
  swings 0.5–1 °C every 20–40 min can cross 0.3 °C several times per cycle, which adds up
  to 100+ reports a day. How many actually go out is one of the
  [Hardware and manual checks](#hardware-and-manual-checks).
- *Too few.* A room that holds within 0.3 °C (little ventilation, say) sends nothing for
  hours, or at all. The app can't tell a steady room from a dead device, and the battery
  state never goes out either.

**Design.** Three rules, all measured against the last committed report (its value and its
time):
- change ≥ 1.0 °C → report now
- change ≥ 0.3 °C → report only if at least 30 min have passed since the last committed
  report
- 4 h or more since the last committed report → report, whatever the change

The first two cap small-change reports at 48 a day while big changes still arrive within one
or two samples. When the hold-off ends, the current filtered value goes out, so the 0.3 °C
error bound still holds after it.

The third rule (the "quiet-room report") reverses the "no heartbeat" decision from the
2026-10-10 review:
- It costs at most 6 reports a day (about 3 mAh), and only in a steady room, where the
  device otherwise sends almost nothing. A room that keeps changing never goes 4 h without
  a report, so it adds nothing there. The Eneloop run logged 6–10 reports a day, so in that
  room it would only fill the long quiet gaps.
- It is an ordinary report. It sends the current filtered value and commits it, which
  restarts the 4 h and moves the 0.3 °C reference. The backoff (1a) and the unpaired flag
  (1b) apply to it with no special case. During a router outage, a steady room now pays the
  same ~12 failed windows a day a busy room already does; 1c makes each one cheaper.
- It needs a good sensor reading, like any report. This comes for free with the rule
  living in `ReportPolicy_Update()`, which a failed read never calls. A dead sensor goes
  silent instead of re-sending old values, so no new log entry for more than about 4 h
  means something is wrong.
- The app shows the device offline between windows, so the cloud log is the only sign of
  life. Checked: the log does show repeated values, so a report with an unchanged value
  is visible there.
- It carries the battery state, so a low battery reaches the app within about 4 h. That
  replaces item 3.
- Don't call it "heartbeat" in code. That already means the Tuya protocol's cmd 0x00 in
  `tuya_link` and the docs.
- Considered, not doing: sending only the DPs that changed (or just temperature) on a
  quiet-room report. It saves nothing. The module asks for every DP itself (cmd 0x08) twice
  per window, and `all_data_update()` answers with all four, so all four go out anyway.
  The three skipped frames are about 15 bytes each, tens of ms against a window of about
  15 s that is mostly boot and association. Answering the module's queries with fewer
  DPs too would go against what the SDK expects, for under 1% of a window.

**Time.** Use the schedule time 1a uses (`next_cycle_ms`), for both the decision and the
commit, not `BSP_GetTimeMs()`. A cycle starts a few ms after its slot, by a different few
each time, so clock time would push the 30 min hold-off to the next cycle about half the
time. Use unsigned subtraction, as everywhere else. After 49.7 days without a commit (only
during a long outage) the elapsed time wraps, which at worst delays one report. Say so in a
comment.

**Where.** [`report_policy.h`](../libs/src/report/report_policy.h) /
[`report_policy.c`](../libs/src/report/report_policy.c):
- Add `REPORT_TEMP_DELTA_FAST_C10` (10), `REPORT_HOLDOFF_MS` (30 min) and
  `REPORT_MAX_SILENCE_MS` (4 h).
- `ReportPolicy_Update()` needs the current time. `ReportPolicy_ReportSent()` stores the
  commit time.
- Rewrite the header's "two rules" comment and the `REPORT_TEMP_DELTA_C10` comment (which
  says it is also the knob for how stale the app may get).
- Reword every "no heartbeat" reason: `report_policy.c` (first report after boot),
  `temp_sensor_main.c` (the commit in `RunWifiWindow`), and
  [`firmware_design.md`](firmware_design.md) sections 1 and 4. The code those comments
  explain stays as it is; only the reason changes. A dropped report would now be repaired
  within 4 h rather than never, which is still too late, and without the first report after
  boot a new device would stay invisible for 4 h.

Add tests in [`report_policy_test.cpp`](../libs/test/report_policy_test/report_policy_test.cpp):
the fast path, a small change held back, a small change released after the hold-off, a sawtooth
input capped at one report per hold-off, the first report after boot (never held back), a
steady input reporting once every 4 h, and any report restarting the 4 h.
`SaysNothingWhileTheTemperatureHolds` feeds 100 samples (more than 8 h at 5 min), so it has to
stay under 4 h, and its "No heartbeat" section heading has to go. In
[`temp_sensor_main_test.cpp`](../libs/test/temp_sensor_main_test/temp_sensor_main_test.cpp):
reword the comment on `ADroppedReportIsRetriedOnTheNextCycle`, and add a steady room opening a
report window after 4 h and a failing sensor never opening one.

## 3. Report a battery-state drop once (dropped)

- [x] Dropped 2026-10-11

**Why dropped.** The problem was that battery state only went out with a temperature report,
so a device in a steady room might never tell the app the battery is low. Item 2's 4 h report
now sends it at least every 4 h, which is soon enough for a low-battery warning.

Without the drop alert, the latch and the 3-cycle confirmation have nothing to protect. They
existed so each alert fired exactly once and a single 0 mV reading couldn't latch Low until
the next reset. The reported state stays a fresh reading on every report, and one bad reading
is corrected by the next report. The "no battery hysteresis" item in firmware_design.md
section 9 stays as it is.

## 4. Take readings every 1–2 min instead of 5 (after item 2)

- [ ] Done

**Why.** A reading costs tens of thousands of times less than a report. Faster readings cut
the delay before a real change is reported from up to about 10 min (one period plus one for
the median filter to confirm it) to about 2–4 min. **This depends on item 2.** Without the
hold-off, faster readings catch more heating/cooling swings and send more reports.

**Where.**
- `CYCLE_PERIOD_MS` in [`temp_sensor_main.c`](../temp_sensor_main/temp_sensor_main.c).
- Rewrite the comment on `WIFI_PAIRING_TIMEOUT_MS`. It says the 3-minute pairing window
  stays under the cycle period, which stops being true. The rescheduling in
  `WaitForNextCycle` already handles a window longer than the period; add a test that pins
  this down.
- `kCyclePeriodMs` in `temp_sensor_main_test.cpp` and any test that assumes 5 min.
- Any time-based constants from items 1a and 2 must not be counted in cycles.

**Optional.** The HDC3020's 15 ms conversion wait is a busy `HAL_Delay`
(`BSP_DelayMs` in [`bsp_stm32.c`](../temp_sensor_STM32L010F4/Core/Src/bsp_stm32.c)). At a
1-min period, an idle wait (WFI) or a stop-mode wait would save a little more. Low priority.

## 5. Synchronous DP report: commit on the module's ack and power off right away

- [x] Done 2026-11-11, in firmware_design.md §4 "Delivered means the module said so". Two
  changes from the design below. **One** synchronous report, not four: the SDK's notes
  (protocol.h, item 14) allow no second before the first is answered, and the module takes
  up to 5 s. So the temperature goes last and its answer covers the plain reports sent
  ahead of it. And the fallback is learned once per run: a module that has never answered
  gets the old rule and isn't asked again until a reset; one that has answered before
  and then doesn't is a failed window. The ack wait is 6 s, past the module's own 5 s.

**Why.** Today a report counts as sent as soon as the cloud is connected, followed by a
fixed 2 s `WIFI_DRAIN_MS`. With synchronous reporting (cmd 0x22, module answers 0x23 with
the result), "sent" means the module confirmed delivery. The window can also power off as
soon as the ack arrives instead of waiting the fixed 2 s.

**Design.**
- Enable `MCU_DP_UPLOAD_SYN` in [`tuya/protocol.h`](../tuya/protocol.h). Implement
  `get_upload_syn_result()` in [`tuya/protocol.c`](../tuya/protocol.c), which currently
  hits an `#error` placeholder, so it sets a flag that `tuya_link` can read.
- Use the `mcu_dp_*_update_syn()` variants only for the cycle's explicit push. Leave the
  answers to the module's own status queries (`all_data_update`) asynchronous. The capture
  shows the module asking once at 0x03 and again at 0x04, so the data may already be
  delivered by then; the sync push is what confirms it.
- The SDK sends one 0x22 frame per DP, so wait for all four acks (or at least temperature's).
  Use a short timeout; if it runs out, fall back to today's behavior (commit on
  cloud-connected).
- `RunWifiWindow` commits only on a successful ack, then powers off.

**Check on hardware:** whether module firmware 3.0.93 answers 0x22 at all, in
[Hardware and manual checks](#hardware-and-manual-checks).

## 6. No state that only the Reset button clears

- [x] Done 2026-11-11

**Why.** On 2026-11-11 every board lost the cloud from about 00:35 to 07:50 (a
house-wide outage). The others came back by themselves within their 2 h pause. The
kitchen board made contact once at about 07:53 (an offline event in the Tuya log with no
online and no DPs), then nothing until its Reset button was pressed at 16:42, when it
reported at once. Its firmware predates item 2, so part of the silence may simply have
been a steady room. But the firmware had three states that only a reset clears, and the
cloud log can't say which, if any, it was in:
- the unpaired latch (item 1b), set by one unpaired answer from the module,
- an I2C bus left busy, after which every read fails and no report is ever due,
- a hang or HardFault: no watchdog, and both fault handlers spun forever.

**Done.**
- Unpaired is re-checked every 2 h (item 1b's note).
- A failed sensor read resets the I2C controller and clocks out a sensor holding SDA
  (`BSP_Sensor_RecoverBus`). So does every boot.
- An IWDG watchdog (18–40 s depending on LSI). It is fed by `Tuya_WaitUntil` and through
  the sleep in 10 s Stops, and only when time really passed. `HardFault_Handler` and
  `Error_Handler` reset the board instead of spinning, and mark the reset as a fault.
- DP 104 `diagnostics`, a line of text that says what happened since the last delivered
  report: why the board restarted, cycles run, failed windows, what state the module was
  in, and failed sensor reads. Kept in the RTC backup registers, so the report after the
  Reset button says what came before it.
- `tools/tuya_logs` shows DP 104 as the `diag` column.

**Checks on hardware:** item 6, in [Hardware and manual checks](#hardware-and-manual-checks).

---

## Hardware and manual checks

Do these once the code changes above are in. Each needs the board, the Tuya app or the Tuya
MCU simulator, so they are batched here rather than interrupting the items. Each says what
to change if the result is wrong.

### 1b: the state an unfinished pairing window leaves

- [ ] Done

Also in firmware_design.md section 10. Not reporting while unpaired relies on the state the
module reports on the boot after a pairing window nobody finished:
- **No previous network.** A module in anti-misoperation mode (`CONFIG_MODE_SPECIAL`) should
  come back in a pairing state, 0x00/0x01/0x06, not 0x02. If it reports 0x02, the unpaired
  flag never sets and the device gets the backoff instead: about 12 full windows a day
  rather than none.
- **Previously paired.** The module must come back on its old network. The pairing window
  cuts power at about the moment the module would restore it. Tuya says a power cycle
  restores it too; if 3.0.93 doesn't, a stray press silences the device until it is paired
  again.

### 1c: cold-boot timing of a paired module

- [ ] Done

The staged timeouts are guesses. The only data is the first-pairing capture: about 3 s from
first heartbeat to first status, 5.4 s from 0x02 to 0x03 and 3.7 s from 0x03 to 0x04. A
**cold boot of an already-paired module** with this firmware hasn't been timed
(firmware_design.md section 10). Log the timestamps of each state change over 10–20 cold
boots and set each timeout to about 2× the worst value seen.

### 2: reports per day in the Tuya app history

- [ ] Done

**Before: done 2026-11-07**, from the Tuya cloud log (`tools/tuya_logs`). Over the full days
11-02 to 11-06 the old firmware sent: front 78–105 reports a day, kitchen 4–17, office 10–28.
At the front board's rate the battery budget gives a cell about 6 weeks. Replaying that week
through item 2's rules gives about 44, 10 and 15 a day.

**After:** flash item 2 and fetch the logs again after a few days (the free tier keeps 7
days). Expect no more than 48 small-change reports a day, no gap longer than about 4 h while
the sensor reads, and counts near the replayed ones. If the front board stays well above 44,
the room moves faster than the interpolated replay assumed; the hold-off
(`REPORT_HOLDOFF_MS`) is the knob.

### 5: does module firmware 3.0.93 answer 0x22?

- [ ] Done

The code is in (item 5), with a fallback for a module that doesn't answer. To tell which
one this is, capture one report window's UART (logic analyser on PA2/PA3), or time the
module's rail on the scope:
- **It answers:** a frame with command byte 0x23 and one data byte (`55 AA vv 23 00 01 01 cs`,
  `01` taken, `00` refused) comes back after the 0x22. The rail drops right after it, with
  no 2 s tail.
- **It doesn't:** no 0x23. The first window of a run stays up about 6 s after the push,
  and later windows have the 2 s tail.

If it never answers, item 5's code costs one 6 s wait per boot and about 100 bytes, and
can come out. If it answers 0 (refused) on a healthy network, look at the frame before
deciding anything.

### 6: the platform, the Reset button and the watchdog

- [ ] Done

1. **Add DP 104 on the platform** before flashing: ID 104, identifier `diagnostics`,
   String, report only, max length 40. Without it the cloud drops the DP and nothing else
   changes. Then fetch the logs: the first report after flashing should carry
   `r=pin c=1 ...` or `r=sw c=1 ...`, depending on how the debugger resets.
2. **The backup registers survive the Reset button.** Let a board run an hour, press
   Reset, fetch the logs: expect `r=pin` with `c` around 12, not 1. If `c=1`, the
   registers are being cleared at boot and the record is useless after a reset. Check
   `HAL_RCCEx_PeriphCLKConfig`'s RTC domain reset first.
3. **Sleep current unchanged.** The 10 s Stop chunks should add tens of nA to the ~120 µA
   at the cell. The module plays no part in this: it is off during sleep, and the chunks
   and the watchdog live in `BSP_McuSleep` and `BSP_Init`. So a sleep-only build like the
   11-03 one, with the module never powered, measures exactly what changed. That build
   must sleep through `BSP_McuSleep`, not call `HAL_PWR_EnterSTOPMode` directly, or the
   watchdog resets it every ~28 s. If the current moved by more than a few µA, look at
   what `SystemClock_Config` waits on after each wake.
4. **The watchdog fires.** In a scratch build only, add `while (1) {}` after the third
   cycle. The board should reset within 20–40 s and the next report read `r=wdg`.
5. **The bus recovery.** Briefly short SDA to ground with a jumper during a cycle. Expect
   the next DP 104 to show `s=1` and readings to resume on the next cycle.

### Sag monitor: DP 101 against the scope

- [x] Done 2026-11-03. With the office Eneloop resting at 1.286 V (DMM), DP 101 read 945 mV and
  the scope's minimum was 961 mV: 16 mV low, so it catches the dips. The gap is within the
  minimum's noise bias plus the scope's DC accuracy. Caveat: DP 101 belongs to the window
  before the one that sends it, so make sure the scope captured that window.

The sag monitor (firmware_design.md section 5) reports the lowest cell voltage under load
as DP 101, one report late. First add the DP on the Tuya platform: ID 101, Value, report
only, 0..3300, step 1, scale 0, unit mV. Without it the cloud drops the value.

Then scope the board input through one window and compare its minimum with the DP 101
the next report carries. The scope showed 0.96 V on the office Eneloop at 1.30 V.
- **Within a few tens of mV:** the monitor catches the dips, and its numbers can be used
  for a loaded-voltage battery gauge.
- **Well above the scope:** the dips are shorter than the ~86 µs a result takes. Drop
  the oversampling for the monitor, or move to DMA.
- **Well below the scope:** suspect noise. A minimum over tens of thousands of samples also
  finds the deepest noise spike and the boost's input ripple. Compare with the scope's
  bandwidth limit off.

### Gauge under load and DP 102 (battery_damaged)

- [ ] Done

The battery percentage now moves its 0 % point up to just above the cell's collapse
voltage, and DP 102 flags a cell that sags far more than a healthy one (firmware_design.md
section 5). First add the DPs on the Tuya platform:
- ID 102, identifier battery_damaged, Bool, report only.
- ID 103, identifier battery_avg_min_voltage, Value, report only, 0..3300, step 1, scale 0,
  unit mV.

**Calibrate from DP 103 first.** Since 2026-11-06 the gauge goes by the averaged minimum
(DP 103), not the lowest single reading (DP 101). Every threshold was set from the single
reading: 1210 mV damaged, the 50 mV margin, and the healthy reference of 945 mV. On the
scope, a brand-new cell's single reading sagged further than the failed cell's (a 0.49 V
spike, with bursts to ~0.8 V; 0.40 V on a brand-new Duracell). So:
- Collect a few days of DP 101 and DP 103 side by side on a brand-new or known-good cell,
  and on the failed Eneloop if it is still around.
- Set DAMAGED_COLLAPSE_MV and COLLAPSE_MARGIN_MV from where the cells fall on DP 103. If
  they don't separate, drop DP 102.
- If the brand-new cell's DP 103 still reads damaged, the averaging time is too short for
  the bursts (SAG_AVERAGE_SHIFT), or the supply path really is that bad. In that case,
  measure the holder (below). This already happened once: at 1/8 (~1.4 ms), a brand-new
  Duracell read 470-750 mV and damaged from its second report, so the weight is now 1/128
  (~22 ms). The 470 mV readings fit a dip ~2-3 ms long at 0.40 V, not the ~1 ms seen on the
  scope; if the next DP 103 values are still low, check how long that dip really lasts.

Then check, one report late each time:
- **Healthy office Eneloop:** about 63 % at 1.286 V, DP 102 false. This assumed 945 mV
  and is to be redone with DP 103.
- **The failed Eneloop on the office board:** 0 % and Low from the third report after it
  goes in, DP 102 true. A window counts only once the next one confirms it.
- **The supply path:** on 2026-11-06 a brand-new cell dipped as far as the failed one. With
  the scope on the cell's own ends (C1) and on the board's input (C2) through one window,
  the gap between them during the bursts is the holder and its leads. Also find out what
  changed on the office board since October, when the same board read 0.96 V.
- **The failed Eneloop recharged:** DP 102 should still be true if its resistance stays
  high, even though its DP 101 looks like a healthy cell's. If it reads false, its
  resistance was high only because it was nearly flat.
- **A freshly charged healthy cell:** 100 %, which the old curve never showed.
- **Variation between windows:** a few days of DP 101 on one cell. The trend's 60 mV step
  limit assumes about ±20 mV; set it to about three times what DP 101 shows.
- **Front board:** the Duracell in its holder. If a healthy cell trips DP 102 there, the
  holder accounts for the sag, and DP 102 is flagging the holder rather than the cell.
