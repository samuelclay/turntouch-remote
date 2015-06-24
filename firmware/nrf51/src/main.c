/* Copyright 2015 Turn Touch. All rights reserved.
 * 
 * This is the main sequence for the Turn Touch Remote.
 * 
 * Includes:
 *   - 4 button GPIOs
 *   - Pin interrupts on buttons
 *   - Timer to watch for long presses to signal mode changes
 *   - Bluetooth Softdevice stack
 * 
 */

#include <stdbool.h>
#include <stdint.h>
#include "boards.h"
#include "bsp.h"
#include "app_timer.h"
#include "nrf_drv_gpiote.h"
#include "nordic_common.h"
#include "nrf_error.h"
#include "rtt.h"

#define APP_TIMER_PRESCALER      0                           /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS     (1 + BSP_APP_TIMERS_NUMBER) /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE  2                           /**< Size of timer operation queues. */

#define BUTTON_PREV_ID           0                           /**< Button used to switch the state. */
#define BUTTON_NEXT_ID           1                           /**< Button used to switch the state. */
#ifdef BSP_BUTTON_0
    #define PIN_IN BSP_BUTTON_0
#endif
#ifndef PIN_IN
    #error "Please indicate input pin"
#endif

#ifdef BSP_LED_0
    #define PIN_OUT BSP_LED_0
#endif
#ifndef PIN_OUT
    #error "Please indicate output pin"
#endif

bsp_indication_t actual_state =  BSP_INDICATE_FIRST;         /**< Currently indicated state. */

const char * indications_list[] = BSP_INDICATIONS_LIST;


void bsp_evt_handler(bsp_event_t evt)
{
    uint32_t err_code;
    rtt_print(0, "%sEvt handler: %s%X%s\n", RTT_CTRL_TEXT_YELLOW, RTT_CTRL_TEXT_BRIGHT_YELLOW, evt, RTT_CTRL_RESET);
    
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

    rtt_print(0, "%sButton press: %s%s%s\n", RTT_CTRL_TEXT_GREEN, RTT_CTRL_TEXT_BRIGHT_GREEN, indications_list[actual_state], RTT_CTRL_RESET);
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
}

void in_pin_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    nrf_drv_gpiote_out_toggle(PIN_OUT);
    rtt_print(0, "%sPin toggle: %s%d%s\n", RTT_CTRL_TEXT_YELLOW, RTT_CTRL_TEXT_BRIGHT_YELLOW, pin, RTT_CTRL_RESET);
}

/**
 * @brief Function for configuring: PIN_IN pin for input, PIN_OUT pin for output, 
 * and configures GPIOTE to give an interrupt on pin change.
 */
// static void gpio_init(void)
// {
//     ret_code_t err_code;
//
//     err_code = nrf_drv_gpiote_init();
//     APP_ERROR_CHECK(err_code);
//
//     nrf_drv_gpiote_out_config_t out_config = GPIOTE_CONFIG_OUT_SIMPLE(false);
//
//     err_code = nrf_drv_gpiote_out_init(PIN_OUT, &out_config);
//     APP_ERROR_CHECK(err_code);
//
//     nrf_drv_gpiote_in_config_t in_config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(true);
//     in_config.pull = NRF_GPIO_PIN_PULLUP;
//
//     err_code = nrf_drv_gpiote_in_init(PIN_IN, &in_config, in_pin_handler);
//     APP_ERROR_CHECK(err_code);
//
//     nrf_drv_gpiote_in_event_enable(PIN_IN, true);
// }

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    uint32_t err_code = NRF_SUCCESS;

    clock_initialization();
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, NULL);
    bsp_configuration();
    // gpio_init();
    rtt_print(0, "%s%sStarting up Turn Touch Remote%s\n", RTT_CTRL_TEXT_BRIGHT_YELLOW, RTT_CTRL_BG_BRIGHT_MAGENTA, RTT_CTRL_RESET);

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
