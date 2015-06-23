/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

/** @file
 *
 * @defgroup ble_sdk_app_template_main main.c
 * @{
 * @ingroup ble_sdk_app_template
 * @brief Template project main file.
 *
 * This file contains a template for creating a new application. It has the code necessary to wakeup
 * from button, advertise, get a connection restart advertising on disconnect and if no new
 * connection created go back to system-off mode.
 * It can easily be used as a starting point for creating a new application, the comments identified
 * with 'YOUR_JOB' indicates where and how you can customize.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include "boards.h"
#include "bsp.h"
#include "app_timer.h"
#include "nordic_common.h"
#include "nrf_error.h"
#include "SEGGER_RTT.h"

#define APP_TIMER_PRESCALER      0                           /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS     (1 + BSP_APP_TIMERS_NUMBER) /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE  2                           /**< Size of timer operation queues. */

#define BUTTON_PREV_ID           0                           /**< Button used to switch the state. */
#define BUTTON_NEXT_ID           1                           /**< Button used to switch the state. */

bsp_indication_t actual_state =  BSP_INDICATE_FIRST;         /**< Currently indicated state. */

const char * indications_list[] = BSP_INDICATIONS_LIST;

void rtt_print(unsigned BufferIndex, const char * sFormat, ...) {
    char colorStringFormat[256];
    sprintf(colorStringFormat, " %s--->%s %s", RTT_CTRL_TEXT_MAGENTA, RTT_CTRL_RESET, sFormat);
    va_list ParamList;

    va_start(ParamList, sFormat);
    SEGGER_RTT_vprintf(BufferIndex, colorStringFormat, &ParamList);
}

/**@brief Function for handling bsp events.
 */
void bsp_evt_handler(bsp_event_t evt)
{
    uint32_t err_code;
    switch (evt)
    {
        case BSP_EVENT_KEY_0:

            if (actual_state != BSP_INDICATE_FIRST)
                actual_state--;
            else
                actual_state = BSP_INDICATE_LAST;
            break;

        case BSP_EVENT_KEY_1:

            if (actual_state != BSP_INDICATE_LAST)
                actual_state++;
            else
                actual_state = BSP_INDICATE_FIRST;
            break;

        default:
            return; // no implementation needed
    }

    err_code = bsp_indication_text_set(actual_state, indications_list[actual_state]);
    APP_ERROR_CHECK(err_code);
    printf("\r\n");
    rtt_print(0, "%sButton press: %s%s%s", RTT_CTRL_TEXT_GREEN, RTT_CTRL_TEXT_BRIGHT_GREEN, indications_list[actual_state], RTT_CTRL_RESET);
}


/**@brief Function for initializing low frequency clock.
 */
void clock_initialization()
{
    NRF_CLOCK->LFCLKSRC            = (CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos);
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_LFCLKSTART    = 1;

    while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
    {
        // Do nothing.
    }
}


/**@brief Function for initializing bsp module.
 */
void bsp_configuration()
{
    uint32_t err_code;

    err_code = bsp_init(BSP_INIT_LED | BSP_INIT_BUTTONS,
                        APP_TIMER_TICKS(100, APP_TIMER_PRESCALER),
                        bsp_evt_handler);
    APP_ERROR_CHECK(err_code);

    // err_code = bsp_buttons_enable( (1 << BUTTON_PREV_ID) | (1 << BUTTON_NEXT_ID) );
    // APP_ERROR_CHECK(err_code);
}


/**
 * @brief Function for application main entry.
 */
int main(void)
{
    uint32_t err_code = NRF_SUCCESS;

    clock_initialization();
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, NULL);
    bsp_configuration();
    rtt_print(0, "%s%sStarting up Turn Touch Remote%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_BG_BRIGHT_MAGENTA, RTT_CTRL_RESET);
    err_code = bsp_indication_text_set(actual_state,indications_list[actual_state]);
    APP_ERROR_CHECK(err_code);
    while (true)
    {
        // no implementation needed
    }
}


/**
 * @}
 */
