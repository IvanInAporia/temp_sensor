#pragma once

// When is a reading worth spending the radio on?
//
// Measuring is nearly free -- one HDC3020 conversion is ~110 uA for 13 ms --
// while reporting is not: the T3-3S has to cold-boot, associate and reach the
// cloud, which is seconds at tens of milliamps every single time. On one NiMH
// cell that ratio is the entire battery life, so the cycle measures on a fixed
// cadence and only reports when the temperature has actually moved.
//
// Two rules, and no others:
//
//   1. A spike must not buy a report. A single sample that jumps and comes
//      straight back is filtered out before the comparison, not after.
//   2. There is no heartbeat. A temperature that never moves never reports.
//      This is a deliberate choice (see REPORT_TEMP_DELTA_C10 for what it
//      costs) and the reason the very first reading after boot always reports:
//      without that, a device switched on in a stable room would never say
//      anything at all.
//
// Humidity and battery ride along on whatever report the temperature triggers.
// Neither can trigger one of its own -- humidity is far noisier than
// temperature and would undo rule 1, and the battery moves so slowly that any
// threshold worth having would take days to cross.
//
// Deciding and committing are two calls, not one. A report can fail -- the
// module may never reach the cloud -- and with no heartbeat a change dropped
// on a failed window would stay dropped until the temperature happened to move
// again. So ReportPolicy_Update() only decides; the value becomes "the last
// thing the cloud knows" at ReportPolicy_ReportSent(), and a window that got
// nowhere simply never calls it, leaving the next cycle to try again.

#include <stdbool.h>
#include <stdint.h>

/* How far the filtered temperature must move from the last reported value
 * before the radio comes up, in 0.1 C.
 *
 * 0.3 C is a little over the HDC3020's own +-0.2 C typical accuracy, so a
 * report means the temperature moved rather than that the sensor wandered.
 *
 * This is the knob for the battery-life / responsiveness trade, and with no
 * heartbeat it is also the knob for how long the Tuya app may show a stale
 * reading: in a room holding steady to within 0.3 C, nothing is sent, and the
 * cloud keeps displaying the last value until something changes. Lower it and
 * the device talks more and lives shorter.
 */
#define REPORT_TEMP_DELTA_C10 3

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
    int16_t window[REPORT_FILTER_WINDOW];
    uint8_t filled;            /* samples so far, saturating at the window size */
    uint8_t next;              /* ring write position */
    int16_t last_reported_c10; /* only meaningful once has_reported */
    bool    has_reported;
} ReportPolicy;

/* Empty window, nothing reported yet. */
void ReportPolicy_Init(ReportPolicy* policy);

/* Feed one measured temperature.
 *
 * Always writes the filtered temperature to `filtered_c10` -- the median once
 * the window has filled, the raw sample before that. That value is what should
 * be cached for the cloud whatever the return says, because the module asks
 * for the current readings on its own schedule and a pairing window can open
 * without a report being due.
 *
 * Returns true when the radio is worth bringing up for it.
 */
bool ReportPolicy_Update(ReportPolicy* policy, int16_t temp_c10, int16_t* filtered_c10);

/* Record that `reported_c10` actually reached the cloud. Only from here on is
 * it what the next comparison is made against.
 */
void ReportPolicy_ReportSent(ReportPolicy* policy, int16_t reported_c10);
