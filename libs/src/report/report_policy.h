#pragma once

// When is a reading worth spending the radio on?
//
// Measuring is nearly free -- one HDC3020 conversion is ~110 uA for 13 ms --
// while reporting is not: the T3-3S has to cold-boot, associate and reach the
// cloud, which is seconds at tens of milliamps every single time. On one NiMH
// cell that ratio is most of the battery life, so the cycle measures on a fixed
// cadence and reports only when there is something worth saying.
//
// A spike must not buy a report. A single sample that jumps and comes straight
// back is filtered out before anything is compared, not after.
//
// Then three rules, each against the last report that reached the cloud: how
// far the filtered temperature has moved from it, and how long ago it was.
//
//   moved 1.0 C or more   report now
//   moved 0.3 C or more   report once 30 min have passed (the hold-off)
//   4 h have passed       report, whatever the change
//
// The hold-off keeps a room that keeps changing from costing a report on every
// 0.3 C. A heating cycle swinging half a degree every 20 min would otherwise
// send 144 a day; small changes now go out at most every 30 min, 48 a day,
// while a big one still goes out on the sample that confirms it. When the
// hold-off ends it is the current reading that goes out, so the app is back
// within 0.3 C after it.
//
// The 4 h report is what tells a steady room from a dead device -- the app
// shows the device offline between windows, so the cloud log is the only sign
// of life -- and what gets the battery state out of a room that never changes.
// It costs at most 6 reports a day, and only in a room that otherwise sends
// next to nothing. It needs a good reading like any other report, so a dead
// sensor goes silent rather than repeating its last value. (Not "heartbeat":
// in tuya_link that is the protocol's cmd 0x00.)
//
// The first reading after boot always reports, hold-off or not. Without it a
// new device would not show up in the app for 4 h.
//
// Humidity and battery ride along on whatever report the temperature triggers.
// Neither can trigger one of its own -- humidity is far noisier than
// temperature and would undo the spike filter, and the battery moves so slowly
// that any threshold worth having would take days to cross. The 4 h report
// carries both out of a steady room.
//
// Deciding and committing are two calls, not one. A report can fail -- the
// module may never reach the cloud -- and a change dropped on a failed window
// would wait for the next 4 h report, far too late for a change worth sending.
// So ReportPolicy_Update() only decides; the value and its time become "the
// last thing the cloud knows" at ReportPolicy_ReportSent(), and a window that
// got nowhere simply never calls it, leaving the change outstanding for the
// next window. (When that is, after a failure, is the cycle's call: it backs
// off -- temp_sensor_main.c.)
//
// Time is in milliseconds on whatever clock the caller keeps, the same one for
// both calls; the cycle passes its schedule (temp_sensor_main.c says why).
// It may wrap.

#include <stdbool.h>
#include <stdint.h>

/* How far the filtered temperature must move from the last reported value to
 * be worth a report once the hold-off has passed, in 0.1 C.
 *
 * 0.3 C is a little over the HDC3020's own +-0.2 C typical accuracy, so a
 * report means the temperature moved rather than that the sensor wandered.
 * It is also how far off the app may stay for longer than the hold-off: a
 * smaller change waits for the 4 h report.
 *
 * Lower it and the device talks more and lives shorter -- in a room that keeps
 * drifting, up to the 48 a day the hold-off allows.
 */
#define REPORT_TEMP_DELTA_C10 3

/* A move this big goes out at once, hold-off or not, in 0.1 C: a window opened
 * in winter, the heating coming on. Big enough that a heating cycle's
 * half-degree swings stay under it.
 *
 * This is the most the app may be off during a hold-off.
 */
#define REPORT_TEMP_DELTA_FAST_C10 10

/* How long after a report a smaller move has to wait, in ms. It caps the
 * small-change reports at 48 a day, whatever the room does, and it is how late
 * a change under REPORT_TEMP_DELTA_FAST_C10 can reach the app.
 */
#define REPORT_HOLDOFF_MS (30u * 60u * 1000u)

/* The longest the device stays silent while it can read its sensor, in ms.
 * Past this it reports whatever the temperature did. At most 6 reports a day,
 * and only in a room steady enough to send no others.
 */
#define REPORT_MAX_SILENCE_MS (4u * 60u * 60u * 1000u)

/* Number of samples the spike filter holds. Three is the smallest window with
 * a median, and a median of three is the whole filter: it rejects any single
 * outlier outright, whatever its size, and passes everything else through
 * untouched.
 *
 * The cost is one sample of lag on a genuine step -- 5 minutes at the cycle
 * cadence -- because a real change looks exactly like a spike until the sample
 * after it confirms it. For room temperature that is the right trade; for a
 * quantity that can legitimately step, it would not be.
 */
#define REPORT_FILTER_WINDOW 3

typedef struct {
    int16_t  window[REPORT_FILTER_WINDOW];
    uint8_t  filled;            /* samples so far, saturating at the window size */
    uint8_t  next;              /* ring write position */
    int16_t  last_reported_c10; /* only meaningful once has_reported */
    bool     has_reported;
    uint32_t last_reported_ms;  /* when, on the caller's clock; only meaningful once has_reported */
} ReportPolicy;

/* Empty window, nothing reported yet. */
void ReportPolicy_Init(ReportPolicy* policy);

/* Feed one measured temperature, taken at `now_ms`.
 *
 * Always writes the filtered temperature to `filtered_c10` -- the median once
 * the window has filled, the raw sample before that. That value is what should
 * be cached for the cloud whatever the return says, because the module asks
 * for the current readings on its own schedule and a pairing window can open
 * without a report being due.
 *
 * Returns true when the radio is worth bringing up for it.
 */
bool ReportPolicy_Update(ReportPolicy* policy, int16_t temp_c10, uint32_t now_ms, int16_t* filtered_c10);

/* Record that `reported_c10` actually reached the cloud, at `now_ms`. Only
 * from here on is it what the next comparison is made against, and the time
 * the hold-off and the 4 h are counted from.
 */
void ReportPolicy_ReportSent(ReportPolicy* policy, int16_t reported_c10, uint32_t now_ms);
