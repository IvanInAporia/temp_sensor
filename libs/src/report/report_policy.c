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
}

bool ReportPolicy_Update(ReportPolicy* policy, int16_t temp_c10, int16_t* filtered_c10)
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
         * against and no threshold that could be crossed. Report, and keep
         * reporting every cycle until one of them gets through: without a
         * heartbeat this is the only thing that puts a device in a stable room
         * on the map at all.
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
         * be a single outlier, and rule 1 says an outlier does not buy a
         * report. Wait for the window. */
        return false;
    }

    return (AbsDiff(*filtered_c10, policy->last_reported_c10) >= REPORT_TEMP_DELTA_C10);
}

void ReportPolicy_ReportSent(ReportPolicy* policy, int16_t reported_c10)
{
    if (policy == 0)
    {
        return;
    }

    policy->last_reported_c10 = reported_c10;
    policy->has_reported      = true;
}
