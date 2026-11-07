/**
  ******************************************************************************
  * @file    bsp_stm32.c
  * @brief   BSP implementation for the temp_sensor v2 board
  *          (STM32L010F4P6 + HDC3020 + Tuya T3-3S).
  ******************************************************************************
  *
  * This is the one file that knows what the hardware is. Everything above
  * ../../../libs/src/bsp/bsp.h is board-agnostic and compiles for the host test
  * harness unchanged; porting to another board means writing another one of
  * these and nothing else.
  *
  * It is deliberately NOT in main.c. CubeMX owns main.c and rewrites it on
  * every regeneration, preserving only the USER CODE blocks -- which is a poor
  * place for four hundred lines that must not be lost. The two calls CubeMX
  * does have to make (BSP_Init and the application entry) are one line each in
  * their USER CODE blocks.
  *
  ******************************************************************************
  */

#include "bsp.h"

#include "battery.h"
#include "main.h"
#include "stm32l0xx_ll_adc.h"

/* CubeMX-owned peripheral handles and clock setup, from main.c. */
extern ADC_HandleTypeDef  hadc;
extern I2C_HandleTypeDef  hi2c1;
extern UART_HandleTypeDef hlpuart1;
extern RTC_HandleTypeDef  hrtc;

void SystemClock_Config(void);

/* HAL's millisecond counter. There is no API to move it, and it has to be
 * moved: SysTick is suspended across a Stop, so without this every sleep would
 * be invisible to HAL_GetTick() and the cycle's schedule would stand still. */
extern __IO uint32_t uwTick;

/* --- Tunables ------------------------------------------------------------- */

/* Generous against transfers of at most 6 bytes at 100 kHz (~0.6 ms). This is
 * not a pacing timeout, it is the "the sensor is not answering" timeout, and it
 * is only reached when the part or the bus is broken. */
#define I2C_TIMEOUT_MS 50u

/* One byte at 9600 8N1 is 1.04 ms. */
#define WIFI_TX_TIMEOUT_MS 10u

/* The wakeup timer runs off ck_spre (1 Hz) for anything a second or longer,
 * which covers every sleep this application asks for in a single arming -- up
 * to 65536 s. The alternative, RTCCLK/16, would need eleven wakeups for a
 * five-minute sleep and eleven clock re-inits with them. */
#define WAKEUP_SECONDS_MIN_MS 1000u

/* RTCCLK/16 with RTCCLK = LSI. The .ioc records LSI at 37 kHz, so 2312.5 Hz;
 * truncated because the counter is an integer and LSI's own tolerance (several
 * percent over temperature) dwarfs the half-hertz. Only used for sub-second
 * sleeps, which this application does not currently ask for. */
#define WAKEUP_DIV16_HZ 2312u

/* WUTR is 16 bits, so one ck_spre arming covers 65536 seconds. */
#define WAKEUP_MAX_SECONDS 65536u

#define MS_PER_DAY (24u * 60u * 60u * 1000u)

/* The battery channel. The cell is wired straight to PA6 with no divider
 * (schematic net `adc_batt`), which is safe precisely because it is the boost
 * converter's INPUT: one NiMH cell never approaches the 3.3 V rail the ADC
 * references. So the pin voltage is the cell voltage, with no ratio to undo. */
#define ADC_BATTERY_CHANNEL ADC_CHANNEL_6

/* MX_ADC_Init() runs the ADC at 10 bits with 4x oversampling and a matching
 * 2-bit right shift, so a result is still 0..1023 full scale. */
#define ADC_RESOLUTION_LL LL_ADC_RESOLUTION_10B

/* MX_ADC_Init() selects 79.5 ADC cycles, which at f_ADC = PCLK2/2 = 8 MHz is
 * 9.9 us -- just under the 10 us the datasheet requires for VREFINT
 * (T_S_vrefint). 160.5 cycles is 20 us. Sampling time is a single global
 * register on the L0, so it is bumped only around the VREFINT conversion and
 * put straight back. */
#define VREFINT_SAMPLING_TIME ADC_SAMPLETIME_160CYCLES_5

#define ADC_POLL_TIMEOUT_MS 10u

/* The lowest there is (SysTick shares it). The sag monitor's interrupt is the
 * one that can always wait: in wait mode the converter holds the next result
 * until this one is read, so a late interrupt costs a gap in the sampling and
 * never a wrong value -- see ADC1_IRQHandler. */
#define SAG_MONITOR_IRQ_PRIORITY 3u

/* --- State ---------------------------------------------------------------- */

/* Latched at BSP_Init before anything can clear it. */
static uint32_t reset_flags;

/* Written by the EXTI interrupt, read and cleared by the application. */
static volatile bool button_event;

/* Written from thread context (the BSP_Wifi_* setters) and read from the UART
 * interrupt, so volatile: without it the compiler is free to cache one of
 * these across a setter under -Os -flto. */
static volatile BSP_void_callback_t wifi_rx_callback;
static volatile BSP_void_callback_t wifi_rx_error_callback;

/* Both external channels sit in the sequencer after MX_ADC_Init(), and the
 * scan converts everything selected. Each read therefore has to deselect the
 * others first -- see ReadAdcChannel. */
static const uint32_t kAdcChannels[] = {
    ADC_BATTERY_CHANNEL,
    ADC_CHANNEL_VREFINT,
};

#define ADC_CHANNEL_COUNT (sizeof(kAdcChannels) / sizeof(kAdcChannels[0]))

/* The sag monitor's samples so far, kept as raw pairs rather than millivolts:
 * the interrupt only has to compare and average, and the conversion to
 * millivolts happens once, in BSP_Battery_StopSagMonitor.
 *
 * Written only by ADC1_IRQHandler while the monitor runs, and touched from
 * thread context only while its interrupt is off. Not volatile, because it is
 * handed to battery.c by pointer: the barriers in BSP_Battery_StartSagMonitor
 * and in the NVIC disable keep the compiler from moving an access across the
 * interrupt being turned on or off. */
static Battery_SagSamples sag_samples;

/* The cell half of the pair in progress. */
static volatile uint16_t sag_pair_cell_raw;

/* Thread context only. */
static bool     sag_running;
static uint32_t sag_saved_sampling_time;

static void SagMonitorHalt(void);

/* --- Init ----------------------------------------------------------------- */

/* --- The LPUART1 interrupt lives here, not in the CubeMX files -------------
 *
 * The Tuya SDK needs a byte-level receive interrupt that stays armed for the
 * whole time the module is powered, and the .ioc does not enable the LPUART1
 * global interrupt -- so neither the vector nor the NVIC call is generated.
 *
 * The obvious fix is to add it to the .ioc and hand-patch stm32l0xx_it.c and
 * stm32l0xx_hal_msp.c to match what CubeMX would emit. That was tried, and it
 * is how the Tuya link silently lost its receive path once already: anything
 * outside a USER CODE block is discarded the next time the project is
 * regenerated, and a build still succeeds afterwards because nothing
 * references what went missing. The failure is invisible until the module is
 * on the bench not answering.
 *
 * So both halves live in this file instead, which CubeMX never touches:
 *
 *   - LPUART1_IRQHandler overrides the weak vector in the startup file.
 *   - The NVIC line is enabled once in BSP_Init, below, and left enabled. It
 *     costs nothing while the module is unpowered: Wifi_RailOff de-inits the
 *     peripheral, so there is no source to interrupt from.
 *
 * If a future .ioc revision ever does enable the LPUART1 interrupt, this file
 * and the generated one will both define the handler and the link will fail
 * with a duplicate symbol -- which is the loud version of the same problem,
 * and the right one to have.
 */
void LPUART1_IRQHandler(void)
{
  HAL_UART_IRQHandler(&hlpuart1);
}

void BSP_Init(void)
{
  /* Before anything else: HAL_Init() does not clear these, but a later reset
     would, and the reason is wanted for the whole run. */
  reset_flags = RCC->CSR;
  __HAL_RCC_CLEAR_RESET_FLAGS();

#ifndef DEBUG
  /* CubeIDE's "Debug in low power modes" sets DBG_SLEEP/STOP/STANDBY, and only
     a power-on reset clears them -- not the reset button, not a reflash. Left
     set, Stop keeps the core clocks running and draws milliamps instead of
     microamps, so a board last flashed with the debugger attached drains its
     cell until someone power-cycles it. Release clears them itself. The cost:
     a debugger attached to a Release build drops off at the first Stop.
     DBGMCU writes need its APB clock; put that back the way it was found. */
  bool dbgmcu_clk_was_on = __HAL_RCC_DBGMCU_IS_CLK_ENABLED();

  __HAL_RCC_DBGMCU_CLK_ENABLE();
  HAL_DBGMCU_DisableDBGSleepMode();
  HAL_DBGMCU_DisableDBGStopMode();
  HAL_DBGMCU_DisableDBGStandbyMode();

  if (!dbgmcu_clk_was_on)
  {
    __HAL_RCC_DBGMCU_CLK_DISABLE();
  }
#endif

  /* Priority 0, the same as the button's EXTI and the RTC. Above SysTick
     (priority 3) deliberately: a tick may wait, a byte arriving at 9600 into a
     single-byte receive may not. */
  HAL_NVIC_SetPriority(LPUART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(LPUART1_IRQn);

  /* MX_RTC_Init() arms a ~21 s wakeup timer, because the .ioc has to name some
     period for the peripheral to be "configured" at all. BSP_McuSleep sets its
     own on every call, so that one is only an interrupt arriving in the middle
     of the first report window. Take it down. */
  (void) HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);

  button_event = false;

  /* Known-off state. main.c already parks the Wi-Fi rail before calling here;
     doing it again is free and makes this function stand on its own. */
  BSP_Wifi_PowerOff();
  BSP_Spare_Set(false);
}

/* --- Time, idling and sleep ----------------------------------------------- */

uint32_t BSP_GetTimeMs(void)
{
  return HAL_GetTick();
}

void BSP_DelayMs(uint32_t ms)
{
  HAL_Delay(ms);
}

void BSP_Idle(void)
{
  /* Plain WFI: clocks and peripherals stay up, so an armed UART receive is
     still armed and the core is back in a handful of cycles.
     SysTick is running here, which is what gives bsp.h's promise of a bounded
     return: even with the module silent this cannot park for more than 1 ms,
     so a condition satisfied between the caller's test and this call costs at
     most one tick rather than the whole timeout.

     NO_SLEEP (defined in the Debug configuration) turns this into a plain
     return, and the caller's wait loop into a spin. With the debugger
     attached, the first interrupt to wake the core from this WFI after the
     Wi-Fi rail comes up HardFaults: the core drops SP for the exception frames
     but they never reach SRAM, and the return pops whatever was there before.
     Without the debugger, or without the WFI, it does not happen -- so it is
     a debug-in-low-power artefact, not a firmware bug, but it makes the
     report window undebuggable. Release keeps the WFI; current measured on a
     Debug build during a window is not representative. */
#ifndef NO_SLEEP
  __WFI();
#endif
}

/* Milliseconds since midnight, from the RTC calendar.
 *
 * The RTC is the only clock that runs through a Stop, so it is the only thing
 * that can say how long one actually lasted -- which matters because the
 * button can end a sleep early. HAL_RTC_GetTime would do the same job and drag
 * in the date API with it; this is the whole of what is needed.
 */
static uint32_t RtcMsOfDay(void)
{
  /* Shadow registers. Reading SSR locks TR and DR until DR is read, so the
     order is fixed and DR has to be read even though the date is not wanted --
     skipping it would leave the calendar frozen. */
  uint32_t ssr = RTC->SSR;
  uint32_t tr  = RTC->TR;
  (void) RTC->DR;

  uint32_t hours   = ((((tr & RTC_TR_HT)  >> RTC_TR_HT_Pos)  * 10u) + ((tr & RTC_TR_HU)  >> RTC_TR_HU_Pos));
  uint32_t minutes = ((((tr & RTC_TR_MNT) >> RTC_TR_MNT_Pos) * 10u) + ((tr & RTC_TR_MNU) >> RTC_TR_MNU_Pos));
  uint32_t seconds = ((((tr & RTC_TR_ST)  >> RTC_TR_ST_Pos)  * 10u) + ((tr & RTC_TR_SU)  >> RTC_TR_SU_Pos));

  /* SSR counts DOWN from PREDIV_S to 0 across one second. */
  uint32_t prediv_s = (RTC->PRER & RTC_PRER_PREDIV_S);
  uint32_t sub_ms   = ((prediv_s - ssr) * 1000u) / (prediv_s + 1u);

  return (((((hours * 60u) + minutes) * 60u) + seconds) * 1000u) + sub_ms;
}

/* Difference between two RtcMsOfDay() readings, handling the midnight wrap.
 * Plain unsigned subtraction would not: the counter rolls at 86,400,000, not
 * at 2^32.
 */
static uint32_t RtcElapsedMs(uint32_t from_ms, uint32_t to_ms)
{
  return (to_ms >= from_ms) ? (to_ms - from_ms) : ((to_ms + MS_PER_DAY) - from_ms);
}

uint32_t BSP_McuSleep(uint32_t sleep_time_ms)
{
  /* Never into a sleep with the sag monitor running: the converter and its
     interrupt would stay armed through it. The cycle stops the monitor at the
     end of every window; this is what holds even on a path out of a window
     that forgot. First, because the halt's HAL calls poll HAL_GetTick(), which
     stops below. */
  if (sag_running)
  {
    SagMonitorHalt();
  }

  if (sleep_time_ms == 0u)
  {
    return 0u;
  }

  uint32_t wakeup_clock;
  uint32_t wakeup_counter;

  if (sleep_time_ms >= WAKEUP_SECONDS_MIN_MS)
  {
    uint32_t seconds = sleep_time_ms / 1000u;

    if (seconds > WAKEUP_MAX_SECONDS)
    {
      seconds = WAKEUP_MAX_SECONDS;
    }

    /* The flag is set (WUTR + 1) clock periods after arming, so a whole number
       of seconds is counter = seconds - 1. */
    wakeup_clock   = RTC_WAKEUPCLOCK_CK_SPRE_16BITS;
    wakeup_counter = seconds - 1u;
  }
  else
  {
    wakeup_clock   = RTC_WAKEUPCLOCK_RTCCLK_DIV16;
    wakeup_counter = (sleep_time_ms * WAKEUP_DIV16_HZ) / 1000u;
  }

  /* Armed BEFORE the tick is suspended, on purpose: the HAL polls the RTC's
     write-allowed flag against HAL_GetTick(), so with SysTick stopped its
     timeout can never expire and a flag that never sets becomes a hang inside
     the HAL rather than a return. */
  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, wakeup_counter, wakeup_clock) != HAL_OK)
  {
    /* The RTC wakeup is the only thing that brings the core back from Stop on
       a cycle with nobody pressing the button, so entering Stop without it
       would park the device until someone did.

       Burn the interval awake instead. It costs milliamps where the design
       budgets microamps -- days of battery rather than months -- but the
       device keeps measuring and reporting, which is worth more than the
       charge, and the current draw is something a bench ammeter shows
       immediately. */
    HAL_Delay(sleep_time_ms);

    return sleep_time_ms;
  }

  uint32_t entered_ms = RtcMsOfDay();

  /* SysTick would wake the core every millisecond and make the whole exercise
     pointless. */
  HAL_SuspendTick();

  /* The reference buffer is 3.0 uA in Stop at 3 V -- against a Stop budget of
     roughly 1 uA for the core plus RTC, it is by far the largest item in the
     sleep budget and nothing is converting anyway. BSP_Battery_ReadMv brings
     it back, at the cost of its 3 ms start-up, once per cycle.
     The ADC's own regulator is left alone: the datasheet's Stop-mode adder
     table lists VREFINT, BOR, LSE, RTC, LPUART1 and LPTIM1, and no ADC term. */
  HAL_ADCEx_DisableVREFINT();

  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

  /* Wakes on the RTC wakeup timer or on the button's EXTI line. Both are
     configured as EXTI sources, which is what lets them fire with the core
     stopped and every clock off. */
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

  /* Stop always exits onto MSI, whatever was running before. Everything
     downstream -- the LPUART's baud rate, the I2C timing, the ADC clock -- is
     computed for HSI16, so this is not optional and not deferrable. */
  SystemClock_Config();

  HAL_ResumeTick();

  /* After the tick is back, for the same reason the arming was done before it
     went away: this one polls the write-allowed flag too. The wakeup timer
     auto-reloads, so left armed it would keep interrupting through the whole
     of the next cycle. */
  (void) HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);

  uint32_t slept_ms = RtcElapsedMs(entered_ms, RtcMsOfDay());

  /* SysTick did not run while the core was stopped, so HAL_GetTick() is behind
     by exactly this much. Folding it back in is what keeps BSP_GetTimeMs() a
     usable wall clock across cycles -- the cycle schedule is computed from it
     (temp_sensor_main.c, WaitForNextCycle). */
  uwTick += slept_ms;

  return slept_ms;
}

void BSP_McuReset(void)
{
  NVIC_SystemReset();
}

BSP_ResetType BSP_GetResetReason(void)
{
  /* Checked in order of specificity. A pin reset sets PINRSTF alongside
     nothing else, and is reported as None: it is the debugger or the reset
     button, which is not an event worth reporting. */
  if ((reset_flags & RCC_CSR_LPWRRSTF) != 0u)
  {
    return BSP_Reset_LowPower;
  }

  if ((reset_flags & RCC_CSR_IWDGRSTF) != 0u)
  {
    return BSP_Reset_IWatchdog;
  }

  if ((reset_flags & RCC_CSR_SFTRSTF) != 0u)
  {
    return BSP_Reset_Software;
  }

  if ((reset_flags & RCC_CSR_PORRSTF) != 0u)
  {
    return BSP_Reset_PowerOn;
  }

  return BSP_Reset_None;
}

/* --- Sensor bus ----------------------------------------------------------- */

bool BSP_Sensor_I2cWrite(uint8_t address, const uint8_t* data, uint16_t len)
{
  /* HAL takes the address already shifted into bits 7:1. */
  return (HAL_I2C_Master_Transmit(&hi2c1, (uint16_t) (address << 1), (uint8_t*) data, len,
                                  I2C_TIMEOUT_MS) == HAL_OK);
}

bool BSP_Sensor_I2cRead(uint8_t address, uint8_t* data, uint16_t len)
{
  return (HAL_I2C_Master_Receive(&hi2c1, (uint16_t) (address << 1), data, len,
                                 I2C_TIMEOUT_MS) == HAL_OK);
}

/* --- Wi-Fi module --------------------------------------------------------- */

void BSP_Wifi_PowerOn(void)
{
  /* Wifi_RailOn (main.c) closes the load switch, waits out its turn-on time,
     and only then hands PA2/PA3 to LPUART1. The order is the point: PA2 idles
     high push-pull, and driving it into an unpowered module back-feeds VBAT
     through the module's input protection -- with U5's QOD unconnected,
     nothing would discharge that rail afterwards. */
  Wifi_RailOn();
}

void BSP_Wifi_PowerOff(void)
{
  /* And the inverse: Wifi_RailOff de-inits the UART, which leaves PA2/PA3 in
     analog mode -- no drive, no Schmitt trigger, no leakage path -- before the
     switch opens. */
  Wifi_RailOff();
}

void BSP_Wifi_TransmitByte(uint8_t value)
{
  (void) HAL_UART_Transmit(&hlpuart1, &value, 1u, WIFI_TX_TIMEOUT_MS);
}

void BSP_Wifi_Receive(uint8_t* received_byte, BSP_void_callback_t cb)
{
  wifi_rx_callback = cb;

  (void) HAL_UART_Receive_IT(&hlpuart1, received_byte, 1u);
}

void BSP_Wifi_AbortReceive(void)
{
  wifi_rx_callback = NULL;

  (void) HAL_UART_AbortReceive(&hlpuart1);
}

void BSP_Wifi_SetRxErrorCallback(BSP_void_callback_t cb)
{
  wifi_rx_error_callback = cb;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart)
{
  if (huart->Instance == LPUART1)
  {
    BSP_void_callback_t cb = wifi_rx_callback;

    if (cb != NULL)
    {
      cb();
    }
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart)
{
  if (huart->Instance != LPUART1)
  {
    return;
  }

  /* HAL splits UART errors in two and only reports one of them here in a way
     that matters. A framing, noise or parity error is "non-blocking": the byte
     is still delivered, the receive stays armed, and the completion callback
     re-arms as usual -- calling the error hook for one of those would try to
     arm on top of a live receive and simply be refused as busy. An overrun is
     "blocking": UART_EndRxTransfer runs, the completion callback never fires,
     and the arming loop is broken until something restarts it.
     RxState is what tells the two apart. */
  if ((huart->RxState == HAL_UART_STATE_READY) && (wifi_rx_error_callback != NULL))
  {
    wifi_rx_error_callback();
  }
}

bool BSP_Wifi_TakeButtonEvent(void)
{
  /* Read and clear together. Without the mask, a press landing between the two
     would be cleared without ever being reported -- and the next chance to
     notice it is five minutes away. */
  __disable_irq();

  bool event = button_event;

  button_event = false;

  __enable_irq();

  return event;
}

bool BSP_Wifi_ButtonIsPressed(void)
{
  /* Switch to ground against the MCU's internal pull-up, so pressed reads low
     (easyeda/review-notes.md 6.2). */
  return (HAL_GPIO_ReadPin(IN_WIFI_BUTTON_GPIO_Port, IN_WIFI_BUTTON_Pin) == GPIO_PIN_RESET);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == IN_WIFI_BUTTON_Pin)
  {
    /* The falling edge only. Everything else about the press -- debounce, how
       long it was held -- is the application's, which reads the level through
       BSP_Wifi_ButtonIsPressed once it is running again. */
    button_event = true;
  }
}

/* --- Battery -------------------------------------------------------------- */

/* One conversion on one channel.
 *
 * MX_ADC_Init() puts both channels in the sequencer and the L0's scan converts
 * everything selected, in channel order, so a conversion started without
 * touching CHSELR would convert both. Deselecting the others is therefore part
 * of reading one. Deselecting ADC_CHANNEL_VREFINT also clears ADC_CCR_VREFEN,
 * which keeps the reference path off while the battery pin is read.
 */
static uint32_t ReadAdcChannel(uint32_t channel)
{
  ADC_ChannelConfTypeDef config = {0};

  for (unsigned i = 0u; i < ADC_CHANNEL_COUNT; i++)
  {
    if (kAdcChannels[i] == channel)
    {
      continue;
    }

    config.Channel = kAdcChannels[i];
    config.Rank    = ADC_RANK_NONE;

    if (HAL_ADC_ConfigChannel(&hadc, &config) != HAL_OK)
    {
      return 0u;
    }
  }

  config.Channel = channel;
  config.Rank    = ADC_RANK_CHANNEL_NUMBER;

  if (HAL_ADC_ConfigChannel(&hadc, &config) != HAL_OK)
  {
    return 0u;
  }

  if (HAL_ADC_Start(&hadc) != HAL_OK)
  {
    return 0u;
  }

  uint32_t value = 0u;

  if (HAL_ADC_PollForConversion(&hadc, ADC_POLL_TIMEOUT_MS) == HAL_OK)
  {
    value = HAL_ADC_GetValue(&hadc);
  }

  /* Always stopped: the next read reconfigures the channel, and
     HAL_ADC_ConfigChannel is only legal with no conversion in flight. */
  (void) HAL_ADC_Stop(&hadc);

  return value;
}

/* The supply the ADC is actually referenced to, in millivolts, or 0 if it
 * could not be measured.
 *
 * There is no external reference on this board: VREF+ is tied to VDDA, so a
 * raw count is a fraction of the supply and means nothing until the supply is
 * known. VREFINT is a fixed ~1.22 V that ST characterises per die at
 * VDDA = 3.0 V and stores in ROM, so converting it backwards gives the supply:
 *
 *     VDDA = 3.0 V * VREFINT_CAL / VREFINT_measured
 *
 * The boost holds VDDA at a regulated 3.3 V, so unlike a board running straight
 * off its cell this does not move between readings -- but measuring it is still
 * what makes the battery reading a voltage rather than a ratio, and it costs
 * one extra conversion.
 */
static uint32_t MeasureVddaMv(void)
{
  /* Switched off before every Stop, so it has to come back. Blocks up to 3 ms
     (t_VREFINT) the first time after a sleep, and returns immediately when the
     buffer is already up. */
  if (HAL_ADCEx_EnableVREFINT() != HAL_OK)
  {
    return 0u;
  }

  /* SMPR may only be written with no conversion running, which is the case
     here: every read above ends in HAL_ADC_Stop. */
  uint32_t saved_sampling_time = READ_BIT(hadc.Instance->SMPR, ADC_SMPR_SMPR);

  MODIFY_REG(hadc.Instance->SMPR, ADC_SMPR_SMPR, VREFINT_SAMPLING_TIME);

  uint32_t vrefint_raw = ReadAdcChannel(ADC_CHANNEL_VREFINT);

  MODIFY_REG(hadc.Instance->SMPR, ADC_SMPR_SMPR, saved_sampling_time);

  if (vrefint_raw == 0u)
  {
    return 0u;
  }

  return __LL_ADC_CALC_VREFANALOG_VOLTAGE(vrefint_raw, ADC_RESOLUTION_LL);
}

uint16_t BSP_Battery_ReadMv(void)
{
  uint32_t vdda_mv = MeasureVddaMv();

  if (vdda_mv == 0u)
  {
    /* No usable reference, so no usable reading. Reported as 0, which the
       gauge maps to 0 % -- the direction that cannot make a flat cell look
       charged (battery.c). */
    return 0u;
  }

  uint32_t raw = ReadAdcChannel(ADC_BATTERY_CHANNEL);

  /* No divider to undo: the cell is the boost's input and sits below VDDA by
     construction, so the pin voltage is the cell voltage. */
  return (uint16_t) __LL_ADC_CALC_DATA_TO_VOLTAGE(vdda_mv, raw, ADC_RESOLUTION_LL);
}

/* --- Sag monitor -----------------------------------------------------------
 *
 * On the scope the cell's minimum during a window is a short dip on each
 * transmit burst, so a reading taken from thread context would almost always
 * land between them. Instead the converter runs continuously for the whole
 * window, and its end-of-conversion interrupt keeps the lowest pair and the
 * lowest point of a ~22 ms running average (Battery_SagSamples, battery.h).
 *
 * Every sample is a pair, the cell and then VREFINT, back to back in one scan,
 * and the pair is what makes it a voltage. MeasureVddaMv's single VDDA reading
 * would not do here: a cell that can no longer power the module is exactly when
 * the boost stops holding 3.3 V, and a sagging reference reads a sagging cell
 * as higher than it is.
 *
 * Wait mode (CFGR1.WAIT) holds each conversion until the previous result has
 * been read. So the interrupt can never fall a result behind, and a pair can
 * never be torn across two scans.
 *
 * Cost: the pair shares VREFINT's 160.5-cycle sampling time (the L0 has one
 * SMPR), so with 4x oversampling a result takes ~86 us -- a pair every
 * ~171 us, ~11,700 interrupts a second. Each one also ends a BSP_Idle, so the
 * window's wait loop polls that often too. Neither is visible next to the
 * module's tens of milliamps, and none of it outlives the window.
 *
 * The vector is defined here for the same reason LPUART1's is (top of this
 * file): the .ioc does not enable the ADC interrupt, and the NVIC line is
 * handled by the start and the halt below rather than by generated code.
 */
void ADC1_IRQHandler(void)
{
  uint32_t isr = hadc.Instance->ISR;

  if ((isr & ADC_ISR_EOC) == 0u)
  {
    return;
  }

  /* Clears EOC, and in wait mode is what lets the next conversion start. */
  uint16_t raw = (uint16_t) hadc.Instance->DR;

  if ((isr & ADC_ISR_EOS) == 0u)
  {
    /* The cell: channel 6 converts before VREFINT (17) in a forward scan. */
    sag_pair_cell_raw = raw;
    return;
  }

  hadc.Instance->ISR = ADC_ISR_EOS;

  /* VREFINT closes the pair. Keeping the lowest pair and the running average
     is battery.c's arithmetic: a handful of shifts, adds and multiplies, well
     inside the ~171 us before the next pair. */
  Battery_SagSamplesAdd(&sag_samples, sag_pair_cell_raw, raw);
}

/* Put back everything BSP_Battery_StartSagMonitor changed, the interrupt
 * first so it cannot run while the rest comes down. Leaves the converter the
 * way BSP_Battery_ReadMv leaves it: stopped, disabled, single conversion, the
 * configured sampling time, VREFINT deselected.
 */
static void SagMonitorHalt(void)
{
  HAL_NVIC_DisableIRQ(ADC1_IRQn);
  __HAL_ADC_DISABLE_IT(&hadc, ADC_IT_EOC);

  (void) HAL_ADC_Stop(&hadc);

  /* Both writable only with no conversion running, which the stop ensured. */
  CLEAR_BIT(hadc.Instance->CFGR1, ADC_CFGR1_CONT | ADC_CFGR1_WAIT);
  MODIFY_REG(hadc.Instance->SMPR, ADC_SMPR_SMPR, sag_saved_sampling_time);

  /* Clears ADC_CCR_VREFEN with it, as ReadAdcChannel's deselect does. */
  ADC_ChannelConfTypeDef config = {0};

  config.Channel = ADC_CHANNEL_VREFINT;
  config.Rank    = ADC_RANK_NONE;

  (void) HAL_ADC_ConfigChannel(&hadc, &config);

  HAL_NVIC_ClearPendingIRQ(ADC1_IRQn);

  sag_running = false;
}

void BSP_Battery_StartSagMonitor(void)
{
  if (sag_running)
  {
    return;
  }

  /* Marked running before anything is changed, so a failure anywhere below is
     undone by the same halt that ends a run. */
  sag_running             = true;
  sag_saved_sampling_time = READ_BIT(hadc.Instance->SMPR, ADC_SMPR_SMPR);
  sag_pair_cell_raw       = 0u;
  Battery_SagSamplesReset(&sag_samples);

  /* Already up from this cycle's BSP_Battery_ReadMv, so this returns at once.
     It stays up for the whole window: only BSP_McuSleep takes it down. */
  if (HAL_ADCEx_EnableVREFINT() != HAL_OK)
  {
    SagMonitorHalt();
    return;
  }

  /* Both channels in the sequence this time, the opposite of ReadAdcChannel. */
  ADC_ChannelConfTypeDef config = {0};

  config.Rank = ADC_RANK_CHANNEL_NUMBER;

  for (unsigned i = 0u; i < ADC_CHANNEL_COUNT; i++)
  {
    config.Channel = kAdcChannels[i];

    if (HAL_ADC_ConfigChannel(&hadc, &config) != HAL_OK)
    {
      SagMonitorHalt();
      return;
    }
  }

  MODIFY_REG(hadc.Instance->SMPR, ADC_SMPR_SMPR, VREFINT_SAMPLING_TIME);
  SET_BIT(hadc.Instance->CFGR1, ADC_CFGR1_CONT | ADC_CFGR1_WAIT);

  __HAL_ADC_ENABLE_IT(&hadc, ADC_IT_EOC);
  HAL_NVIC_SetPriority(ADC1_IRQn, SAG_MONITOR_IRQ_PRIORITY, 0);
  HAL_NVIC_ClearPendingIRQ(ADC1_IRQn);

  /* A compiler barrier as well: the samples were reset above through plain,
     non-volatile stores, and the NVIC enable has no barrier of its own. */
  __DSB();
  HAL_NVIC_EnableIRQ(ADC1_IRQn);

  if (HAL_ADC_Start(&hadc) != HAL_OK)
  {
    SagMonitorHalt();
  }
}

/* A raw pair to millivolts, or 0 for no pair. */
static uint16_t SagPairToMv(Battery_AdcPair pair)
{
  if (pair.vrefint == 0u)
  {
    return 0u;
  }

  uint32_t vdda_mv = __LL_ADC_CALC_VREFANALOG_VOLTAGE(pair.vrefint, ADC_RESOLUTION_LL);

  return (uint16_t) __LL_ADC_CALC_DATA_TO_VOLTAGE(vdda_mv, pair.cell, ADC_RESOLUTION_LL);
}

BSP_SagReading BSP_Battery_StopSagMonitor(void)
{
  if (sag_running)
  {
    SagMonitorHalt();
  }

  /* The interrupt is off -- the halt's NVIC disable is also a compiler
     barrier -- so the samples can no longer change under us. */
  BSP_SagReading reading = {
    SagPairToMv(Battery_SagSamplesLowest(&sag_samples)),
    SagPairToMv(Battery_SagSamplesLowestAverage(&sag_samples)),
  };

  /* Taken: a second stop reports nothing rather than this run again. */
  Battery_SagSamplesReset(&sag_samples);

  return reading;
}

/* --- Spare I/O ------------------------------------------------------------ */

void BSP_Spare_Set(bool on)
{
  HAL_GPIO_WritePin(OUT_SPARE_GPIO_Port, OUT_SPARE_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
