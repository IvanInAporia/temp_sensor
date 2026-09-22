#pragma once

// The application: one measurement cycle every five minutes, forever.
//
// Board-agnostic -- everything it touches is either a libs/src module or the
// BSP -- so the whole of it runs on the host test harness against bsp_mock.

/* Once, after BSP_Init(). */
void TempSensorInit(void);

/* One complete cycle: measure, decide, maybe report, then sleep until the next
 * one is due. Returns when it is time to run again.
 *
 * Exposed separately from TempSensorRun() so the host tests can step the
 * device one cycle at a time.
 */
void TempSensorRunCycle(void);

/* Cycle forever. Never returns. */
void TempSensorRun(void);
