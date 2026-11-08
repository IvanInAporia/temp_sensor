#include "report_policy.h"

/* Median of the three samples in the window, without sorting them: the median
 * is whichever value is neither the smallest nor the largest, which three
 * comparisons settle. Sorting a three-element array in place would also work
 * and would destroy the ring order.
 */
static int16_t Median3(const int16_t v[REPORT_FILTER_WINDOW])
{
    int16_t a = v[0];
    int16_t b = v[1];
    int16_t c = v[2];

    if (a > b)
    {
        int16_t t = a;
        a = b;
        b = t;
    }

    if (b > c)
    {
        b = c;
    }

    return (a > b) ? a : b;
}

static uint16_t AbsDiff(int16_t x, int16_t y)
{
    /* Widened before subtracting. Over the sensor's -40..125 C range in 0.1 C
     * units the difference cannot actually overflow an int16 -- but it costs
     * nothing to say so here rather than leave it as something a reader has to
     * reconstruct from the sensor's range. */
    int32_t diff = (int32_t) x - (int32_t) y;

    return (uint16_t) ((diff < 0) ? -diff : diff);
}

void ReportPolicy_Init(ReportPolicy* policy)
{
    if (policy == 0)
    {
        return;
    }

    for (uint8_t i = 0u; i < REPORT_FILTER_WINDOW; i++)
    {
        policy->window[i] = 0;
    }

    policy->filled            = 0u;
    policy->next              = 0u;
    policy->last_reported_c10 = 0;
    policy->has_reported      = false;
    policy->last_reported_ms  = 0u;
}

bool ReportPolicy_Update(ReportPolicy* policy, int16_t temp_c10, uint32_t now_ms, int16_t* filtered_c10)
{
    if ((policy == 0) || (filtered_c10 == 0))
    {
        return false;
    }

    policy->window[policy->next] = temp_c10;
    policy->next                 = (uint8_t) ((policy->next + 1u) % REPORT_FILTER_WINDOW);

    if (policy->filled < REPORT_FILTER_WINDOW)
    {
        policy->filled++;
    }

    /* The filter has nothing to reject until it has three samples, so before
     * that the raw sample is the best estimate available. */
    bool window_full = (policy->filled >= REPORT_FILTER_WINDOW);

    *filtered_c10 = window_full ? Median3(policy->window) : temp_c10;

    if (!policy->has_reported)
    {
        /* Nothing has ever reached the cloud, so there is nothing to compare
         * against and no hold-off to wait out. Report, and keep asking every
         * cycle until one gets through (how often the cycle actually tries is
         * its own backoff): without this a device switched on in a steady room
         * would not show up in the app until the 4 h report.
         *
         * If this first sample happens to be a spike it corrects itself once
         * the window fills, which is a far smaller problem than starting up
         * silent. */
        return true;
    }

    if (!window_full)
    {
        /* Reported at least once and the window is still refilling -- which
         * only happens after a reset. Any difference visible now could still
         * be a single outlier, and an outlier does not buy a report. Wait for
         * the window. */
        return false;
    }

    /* Unsigned subtraction, as in the cycle: right across a wrap of the clock,
     * as long as less than one wrap has passed. */
    uint32_t since_ms = now_ms - policy->last_reported_ms;

    if (since_ms >= REPORT_MAX_SILENCE_MS)
    {
        /* Brought up to 4 h behind now, so the time since cannot grow far
         * enough to wrap. Left where it was, it would keep growing through a
         * long outage, or while nobody pairs the device, and some 49.7 days on
         * it would wrap round to a few minutes and hold the report back for up
         * to 4 h. Past 4 h nothing reads it as anything but "4 h or more".
         *
         * Only a sensor that fails for all of those 49.7 days -- this is never
         * called while it does -- could still let it wrap, and the cost is the
         * same one late report. */
        policy->last_reported_ms = now_ms - REPORT_MAX_SILENCE_MS;

        return true;
    }

    uint16_t moved_c10 = AbsDiff(*filtered_c10, policy->last_reported_c10);

    if (moved_c10 >= REPORT_TEMP_DELTA_FAST_C10)
    {
        return true;
    }

    return (moved_c10 >= REPORT_TEMP_DELTA_C10) && (since_ms >= REPORT_HOLDOFF_MS);
}

void ReportPolicy_ReportSent(ReportPolicy* policy, int16_t reported_c10, uint32_t now_ms)
{
    if (policy == 0)
    {
        return;
    }

    policy->last_reported_c10 = reported_c10;
    policy->last_reported_ms  = now_ms;
    policy->has_reported      = true;
}
