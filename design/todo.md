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

Suggested order: 1a → 1b → 1c → 2 → 4. Items 3 and 5 don't depend on anything.

---

## 1a. Back off after failed windows

- [ ] Done

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

- [ ] Done

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

**Check on hardware.** What state does a module in anti-misoperation mode (`CONFIG_MODE_SPECIAL`)
report after a pairing window times out with no previous network? It should be 0x00/0x01/0x06,
not 0x02.

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

**Measure first.** These timeouts are guesses until measured. The only data is the
first-pairing capture: about 3 s from first heartbeat to first status, 5.4 s from 0x02 to
0x03 and 3.7 s from 0x03 to 0x04. A **cold boot of an already-paired module** with this
firmware hasn't been timed (firmware_design.md section 10). Log the timestamps of each state
change over 10–20 cold boots and set each timeout to about 2× the worst value seen.
---

## 2. Limit how often temperature reports go out

- [ ] Done

**Problem.** Reporting whenever the temperature moves 0.3 °C keeps the reading within
0.3 °C, but the number of reports per day has no limit. A heating/cooling cycle that swings
0.5–1 °C every 20–40 min can cross 0.3 °C several times per cycle, which adds up to 100+
reports a day. Before starting, check the actual reports per day in the Tuya app history.
That number also tests the field-run guess (see the Eneloop notes) that the battery is
draining faster than expected.

**Design.** Two thresholds, both measured against the last committed value:
- change ≥ 1.0 °C → report now
- change ≥ 0.3 °C → report only if at least 30 min have passed since the last committed
  report

This caps small-change reports at 48 a day while big changes still arrive within one or two
samples. When the hold-off ends, the current filtered value goes out, so the 0.3 °C error
bound still holds after it.

**Where.** [`report_policy.h`](../libs/src/report/report_policy.h) /
[`report_policy.c`](../libs/src/report/report_policy.c):
- Add `REPORT_TEMP_DELTA_FAST_C10` (10) and `REPORT_HOLDOFF_MS` (30 min).
- `ReportPolicy_Update()` needs the current time. `ReportPolicy_ReportSent()` stores the
  commit time.
- Rewrite the header's "two rules" comment.

Add tests in [`report_policy_test.cpp`](../libs/test/report_policy_test/report_policy_test.cpp):
the fast path, a small change held back, a small change released after the hold-off, a sawtooth
input capped at one report per hold-off, and the first report after boot (never held back).

## 3. Report a battery-state drop once (High → Middle, Middle → Low)

- [ ] Done

**Problem.** Battery state only goes out with a temperature report, so a device in a steady
room may never tell the app the battery is low.

**Decision.** No heartbeat. Instead, a **drop** in battery state triggers one report by
itself. That is two extra reports per charge cycle. A rise never triggers anything.

**Design.**
- Make the reported state only ever go down until reset. Keep the lowest state confirmed
  since boot and report that instead of the current reading. The app then never sees
  High → Middle → High flapping, and the two drop alerts fire exactly once each. This also
  replaces the "no battery hysteresis" item in firmware_design.md section 9.
- Accept a lower state only after it shows up on **3 consecutive cycles**. A single bad
  reading (the BSP reads 0 mV as 0 % → Low) must not latch Low until the next reset.
- Report when the confirmed state is below the last committed state. Commit it with the
  rest of the window. A battery-triggered report ignores the hold-off from item 2.
- Swapping the battery resets the MCU, which clears the latch.
- Battery % stays a raw reading. Revisit if relaxation bounce in the app becomes annoying.

**Where.** Either a small state holder in [`battery.c`](../libs/src/battery/battery.c) (the
header already says hysteresis would need "a caller-held previous value") or next to the
report policy. Wire it in through `temp_sensor_main.c`. Add tests for the 3-cycle
confirmation, both drop alerts, no alert on a rise, and a single 0 mV glitch ignored.

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

- [ ] Done

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

**Check first.** Does T3 module firmware 3.0.93 answer 0x22? If it ignores the command,
this item reduces to the fallback and isn't worth the code. Test it with the Tuya MCU
simulator or a logic capture before writing anything else.
