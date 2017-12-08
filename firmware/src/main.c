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
#include <inttypes.h>
#include "app_error.h"
#include "app_scheduler.h"
#include "app_timer.h"
#include "app_timer_appsh.h"
#include "app_trace.h"
#include "ble.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_bas.h"
#include "ble_conn_params.h"
#include "ble_dfu.h"
#include "ble_dis.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "boards.h"
#include "bsp.h"
#include "bsp_btn_ble.h"
#include "button_status.h"
#include "device_manager.h"
#include "dfu_app_handler.h"
#include "nordic_common.h"
#include "nrf.h"
#include "nrf51_bitfields.h"
#include "nrf_delay.h"
#include "nrf_drv_gpiote.h"
#include "nrf_error.h"
#include "nrf_gpio.h"
#include "pstorage.h"
#include "rtt.h"
#include "softdevice_handler.h"
#include "main.h"


#ifdef DEBUG
#pragma message "---> DEBUG mode turned on!"
#endif

// ===========
// = Buttons =
// ===========

void bsp_evt_handler(bsp_event_t evt) {
    
    if (evt == BSP_EVENT_KEY_0 ||
        evt == BSP_EVENT_KEY_1 ||
        evt == BSP_EVENT_KEY_2 ||
        evt == BSP_EVENT_KEY_3 ||
        evt == BSP_EVENT_KEY_4 ||
        evt == BSP_EVENT_KEY_5 ||
        evt == BSP_EVENT_KEY_6 ||
        evt == BSP_EVENT_KEY_7) {
        uint32_t err_code;
        bool pushed = false;
        uint8_t button_state[BUTTON_STATUS_PACKET_BYTES] = {0xFF, 0x00};
        bool mode_change = false;
        switch (evt)
        {
            case BSP_EVENT_KEY_4:            
            case BSP_EVENT_KEY_5:
            case BSP_EVENT_KEY_6:
            case BSP_EVENT_KEY_7:
                rtt_print(0, "%sMode change #%X%s\n", RTT_CTRL_TEXT_BLUE, evt, RTT_CTRL_RESET);
            
                mode_change = true;
                rtt_print(0, "%sMode Change #%X: %s%X%X(%X)%s\n", RTT_CTRL_TEXT_BRIGHT_BLACK, evt, RTT_CTRL_TEXT_BLUE, button_state[0], button_state[1], mode_change, RTT_CTRL_RESET);
                break;
            default:
                break;
        }
    
        uint32_t num;
        for (num = 0; num < BUTTONS_NUMBER; num++) {
            app_button_is_pushed(num, &pushed);
            if (pushed) {
                button_state[0] ^= (1 << num);
                strncpy(m_last_button_state, button_state, 2);
                m_last_unconnected_button_start = NRF_RTC1->COUNTER;
                rtt_print(0, "%sButton down #%X=%X: %s%X%X(%d)%s\n", RTT_CTRL_TEXT_BRIGHT_BLACK, evt, BSP_EVENT_KEY_0+num, RTT_CTRL_TEXT_BLUE, button_state[0], button_state[1], mode_change, RTT_CTRL_RESET);
                switch (num) {
                    case 0:
                        LEDS_ON(BSP_LED_0_MASK);
                        break;
                    case 1:
                        LEDS_ON(BSP_LED_1_MASK);
                        break;
                    case 2:
                        LEDS_ON(BSP_LED_2_MASK);
                        break;
                    case 3:
                        LEDS_ON(BSP_LED_3_MASK);
                        break;
                    default:
                        break;
                }
            } else {
                rtt_print(0, "%sButton up #%X=%X: %s%X%X(%d)%s\n", RTT_CTRL_TEXT_BRIGHT_BLACK, evt, BSP_EVENT_KEY_0+num, RTT_CTRL_TEXT_BLUE, button_state[0], button_state[1], mode_change, RTT_CTRL_RESET);
                if (evt == BSP_EVENT_KEY_0+num) {
                    if (m_last_press == evt && !mode_change) {
                        button_state[0] ^= (1 << (num+4));
                        rtt_print(0, "%sDouble click! #%X=%X=%X: %s%X%X(%d)%s\n", RTT_CTRL_TEXT_BRIGHT_BLACK, m_last_press, evt, BSP_EVENT_KEY_0+num, RTT_CTRL_TEXT_BLUE, button_state[0], button_state[1], mode_change, RTT_CTRL_RESET);
                    } else if (!mode_change) {
                        rtt_print(0, "%sStoring double click #(%X) %X=%X: %s%X%X(%d)%s\n", RTT_CTRL_TEXT_BRIGHT_BLACK, m_last_press, evt, BSP_EVENT_KEY_0+num, RTT_CTRL_TEXT_BLUE, button_state[0], button_state[1], mode_change, RTT_CTRL_RESET);
                        m_last_press = (bsp_event_t)(BSP_EVENT_KEY_0+num);
                    }
                }
                // button_state[0] |= (1 << 0);
                switch (num) {
                    case 0:
                        LEDS_OFF(BSP_LED_0_MASK);
                        break;
                    case 1:
                        LEDS_OFF(BSP_LED_1_MASK);
                        break;
                    case 2:
                        LEDS_OFF(BSP_LED_2_MASK);
                        break;
                    case 3:
                        LEDS_OFF(BSP_LED_3_MASK);
                        break;
                    default:
                        break;
                }
            }            
        }


        // Mode change
        if (mode_change) {
            m_last_press = BSP_EVENT_NOTHING; // Ignore subsequent double click
            button_state[1] |= 0xFF;
            strncpy(m_last_button_state, button_state, 2);
            LEDS_ON(LEDS_MASK);
        }    
        
        if (!pushed) {
            m_dblclick_timer_start = NRF_RTC1->COUNTER;
            rtt_print(0, "m_doubleclick_timer_id: %X\n", m_doubleclick_timer_id);
            err_code = app_timer_start(m_doubleclick_timer_id, APP_TIMER_TICKS(DOUBLECLICK_DURATION, APP_TIMER_PRESCALER), NULL);
            rtt_print(0, "m_doubleclick app error: %d\n", err_code);
            // APP_ERROR_CHECK(err_code);
        }
        
        if (m_conn_handle != BLE_CONN_HANDLE_INVALID) {
            err_code = ble_buttonstatus_on_button_change(&m_buttonservice, button_state);
            if ((err_code != NRF_SUCCESS) &&
                (err_code != BLE_ERROR_INVALID_CONN_HANDLE) &&
                (err_code != NRF_ERROR_INVALID_STATE) &&
                (err_code != BLE_ERROR_NO_TX_BUFFERS) &&
                (err_code != NRF_ERROR_BUSY) &&
                (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING))
            {
                // Can ignore this error, just means that bluetooth is connected but nobody's listening yet
                rtt_print(0, "BLE_ERROR_GATTS_SYS_ATTR_MISSING (or similar)\n");
                // Can ignore this error, just means buffer capacity exceeded (too many button pushes)
                // rtt_print(0, "BLE_ERROR_NO_TX_BUFFERS\n");
                APP_ERROR_HANDLER(err_code);
            }
        } else {
            rtt_print(0, "%sIgnoring button, not connected: %s%X:%X\n", RTT_CTRL_TEXT_BRIGHT_BLACK, RTT_CTRL_TEXT_BLUE, button_state[0], button_state[1]);
            
            LEDS_ON(LEDS_MASK);
            // err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
            // rtt_print(0, "%sStarted BLE advertising fast%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
            // APP_ERROR_CHECK(err_code);
        }
    } else {
        rtt_print(0, "%sUnhandled bsp_evt: %s%X%s\n", RTT_CTRL_TEXT_RED, RTT_CTRL_TEXT_BRIGHT_RED, evt, RTT_CTRL_RESET);
    }
}

// ================
// = Initializers =
// ================

/**@brief Function for initializing low frequency clock.
 */
void clock_initialization()
{
    // https://devzone.nordicsemi.com/question/953/what-low-frequency-clock-sources-can-i-use/
    NRF_CLOCK->LFCLKSRC            = (CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos);
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_LFCLKSTART    = 1;

    // Wait for the low frequency clock to start
    while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
    {
        // Do nothing.
    }
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
}


void storage_init() {
    uint32_t err_code;
    
    err_code = pstorage_init();
    
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing bsp module.
 */
void bsp_configuration()
{
    // bsp_event_t startup_event;
    uint32_t err_code;

    err_code = bsp_init(BSP_INIT_LED | BSP_INIT_BUTTONS,
                        APP_TIMER_TICKS(100, APP_TIMER_PRESCALER),
                        bsp_evt_handler);
    APP_ERROR_CHECK(err_code);
    
    // err_code = bsp_btn_ble_init(NULL, &startup_event);
    // APP_ERROR_CHECK(err_code);
}

/**@brief Function for the Event Scheduler initialization.
 *
 * The scheduler is used for transferring execution from the interrupt context to the main context.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

// void in_pin_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
// {
//     nrf_drv_gpiote_out_toggle(PIN_OUT);
//     rtt_print(0, "%sPin toggle: %s%d%s\n", RTT_CTRL_TEXT_YELLOW, RTT_CTRL_TEXT_BRIGHT_YELLOW, pin, RTT_CTRL_RESET);
// }

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

// =================
// = Battery Level =
// =================

/**@brief ADC interrupt handler.
 * @details  This function will fetch the conversion result from the ADC, convert the value into
 *           percentage and send it to peer.
 */
void ADC_IRQHandler(void)
{
    if (NRF_ADC->EVENTS_END != 0)
    {
        uint8_t     adc_result;
        uint16_t    batt_lvl_in_milli_volts;
        uint8_t     percentage_batt_lvl;
        uint32_t    err_code;

        NRF_ADC->EVENTS_END     = 0;
        adc_result              = NRF_ADC->RESULT;
        NRF_ADC->TASKS_STOP     = 1;

        batt_lvl_in_milli_volts = ADC_RESULT_IN_MILLI_VOLTS(adc_result) ;//+ DIODE_FWD_VOLT_DROP_MILLIVOLTS;
        percentage_batt_lvl     = battery_level_in_percent(batt_lvl_in_milli_volts);

        err_code = ble_bas_battery_level_update(&m_bas, percentage_batt_lvl);
        if ((err_code != NRF_SUCCESS)
            &&
            (err_code != NRF_ERROR_INVALID_STATE)
            &&
            (err_code != BLE_ERROR_NO_TX_BUFFERS)
            &&
            (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING)) {
            APP_ERROR_HANDLER(err_code);
        }
    }
}

void battery_level_update(void)
{
    uint32_t err_code;

    // Configure ADC
    NRF_ADC->INTENSET   = ADC_INTENSET_END_Msk;
    NRF_ADC->CONFIG     = (ADC_CONFIG_RES_8bit                        << ADC_CONFIG_RES_Pos)     |
                          (ADC_CONFIG_INPSEL_SupplyOneThirdPrescaling << ADC_CONFIG_INPSEL_Pos)  |
                          (ADC_CONFIG_REFSEL_VBG                      << ADC_CONFIG_REFSEL_Pos)  |
                          (ADC_CONFIG_PSEL_Disabled                   << ADC_CONFIG_PSEL_Pos)    |
                          (ADC_CONFIG_EXTREFSEL_None                  << ADC_CONFIG_EXTREFSEL_Pos);
    NRF_ADC->EVENTS_END = 0;
    NRF_ADC->ENABLE     = ADC_ENABLE_ENABLE_Enabled;

    // Enable ADC interrupt
    err_code = sd_nvic_ClearPendingIRQ(ADC_IRQn);
    APP_ERROR_CHECK(err_code);

    err_code = sd_nvic_SetPriority(ADC_IRQn, NRF_APP_PRIORITY_LOW);
    APP_ERROR_CHECK(err_code);

    err_code = sd_nvic_EnableIRQ(ADC_IRQn);
    APP_ERROR_CHECK(err_code);

    NRF_ADC->EVENTS_END  = 0;    // Stop any running conversions.
    NRF_ADC->TASKS_START = 1;
}

uint32_t temperature_data_get(void)
{
    int32_t temp;
    uint32_t err_code;
    
    err_code = sd_temp_get(&temp);
    APP_ERROR_CHECK(err_code);
    
    temp = (temp / 4) * 100;
    
    int8_t exponent = -2;
    return ((exponent & 0xFF) << 24) | (temp & 0x00FFFFFF);
}

/**@brief Function for handling the Battery measurement timer timeout.
 *
 * @details This function will be called each time the battery level measurement timer expires.
 *
 * @param[in] p_context  Pointer used for passing some arbitrary information (context) from the
 *                       app_start_timer() call to the timeout handler.
 */
static void battery_level_meas_timeout_handler(void * p_context)
{
    UNUSED_PARAMETER(p_context);
    battery_level_update();
}

static void doubleclick_timeout_handler(void * p_context) {
    UNUSED_PARAMETER(p_context);
    uint32_t err_code;
    
    if ((NRF_RTC1->COUNTER - m_dblclick_timer_start) < APP_TIMER_TICKS(DOUBLECLICK_DURATION, APP_TIMER_PRESCALER)) {
        rtt_print(0, "%sDouble-click expiring... %s%X: %d < %d%s\n", RTT_CTRL_TEXT_GREEN, RTT_CTRL_TEXT_BRIGHT_GREEN, m_last_press, (NRF_RTC1->COUNTER - m_dblclick_timer_start), APP_TIMER_TICKS(DOUBLECLICK_DURATION, APP_TIMER_PRESCALER), RTT_CTRL_RESET);
        
        err_code = app_timer_stop(m_doubleclick_timer_id);
        APP_ERROR_CHECK(err_code);
        err_code = app_timer_start(m_doubleclick_timer_id, APP_TIMER_TICKS(DOUBLECLICK_DURATION, APP_TIMER_PRESCALER), NULL);
        APP_ERROR_CHECK(err_code);
    } else {
        rtt_print(0, "%sDouble-click expired: %s%X%s\n", RTT_CTRL_TEXT_GREEN, RTT_CTRL_TEXT_BRIGHT_GREEN, m_last_press, RTT_CTRL_RESET);
        m_last_press = BSP_EVENT_NOTHING;
    
        err_code = app_timer_stop(m_doubleclick_timer_id);
        APP_ERROR_CHECK(err_code);
    }
}

// =============
// = Bluetooth =
// =============

/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in] sys_evt  System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
{
    pstorage_sys_event_handler(sys_evt);
    ble_advertising_on_sys_evt(sys_evt);
}

/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if(p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);

        rtt_print(0, "%s%sConnection params failed: %s%X%s\n", RTT_CTRL_BG_BRIGHT_RED, RTT_CTRL_TEXT_BRIGHT_WHITE, RTT_CTRL_TEXT_BRIGHT_RED, p_evt, RTT_CTRL_RESET);
    }
}

/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
    rtt_print(0, "%s%sConnection params error: %s%X%s\n", RTT_CTRL_BG_BRIGHT_RED, RTT_CTRL_TEXT_BRIGHT_WHITE, RTT_CTRL_TEXT_BRIGHT_RED, nrf_error, RTT_CTRL_RESET);
}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, NULL);
    // SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_RC_250_PPM_4000MS_CALIBRATION, NULL);

    // Enable BLE stack 
    ble_enable_params_t ble_enable_params;
    memset(&ble_enable_params, 0, sizeof(ble_enable_params));
    ble_enable_params.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT;
    err_code = sd_ble_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    ble_gap_addr_t addr;

    err_code = sd_ble_gap_address_get(&addr);
    APP_ERROR_CHECK(err_code);
    sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &addr);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);
    
    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
    
    // rtt_print(0, "%sStarted bluetooth stack.%s\n", RTT_CTRL_TEXT_MAGENTA, RTT_CTRL_RESET);
}

/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    // /* YOUR_JOB: Use an appearance value matching the application's use case. 
    err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_GENERIC_REMOTE_CONTROL);
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
    
    // rtt_print(0, "%sBluetooth GAP connection params init.%s\n", RTT_CTRL_TEXT_MAGENTA, RTT_CTRL_RESET);
}

/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t      err_code;
    ble_advdata_t advdata;
    ble_uuid_t    adv_uuids[] = {
        {BLE_UUID_BATTERY_SERVICE,              BLE_UUID_TYPE_BLE},
        {BLE_UUID_DEVICE_INFORMATION_SERVICE,   BLE_UUID_TYPE_BLE},
        {BUTTONSERVICE_UUID_SERVICE,            BLE_UUID_TYPE_BLE}
    };

    // Build advertising data struct to pass into @ref ble_advertising_init.
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance      = true;
    advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED ;
    advdata.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
    advdata.uuids_complete.p_uuids  = adv_uuids;
    rtt_print(0, "%sAdvertising: %d services.\n", RTT_CTRL_TEXT_MAGENTA, sizeof(adv_uuids) / sizeof(adv_uuids[0]));
    
    ble_adv_modes_config_t options = {0};
    options.ble_adv_fast_enabled   = BLE_ADV_FAST_ENABLED;
    options.ble_adv_fast_interval  = APP_ADV_FAST_INTERVAL;
    options.ble_adv_fast_timeout   = APP_ADV_FAST_TIMEOUT;
    options.ble_adv_slow_enabled   = BLE_ADV_SLOW_ENABLED;
    options.ble_adv_slow_interval  = APP_ADV_SLOW_INTERVAL;
    options.ble_adv_slow_timeout   = APP_ADV_SLOW_TIMEOUT;
    
    err_code = ble_advertising_init(&advdata, NULL, &options, on_adv_evt, NULL);
    APP_ERROR_CHECK(err_code);
    
    // rtt_print(0, "%sStarted bluetooth advertising.%s\n", RTT_CTRL_TEXT_MAGENTA, RTT_CTRL_RESET);
}

/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
    
    // rtt_print(0, "%sConnection params initialized.%s\n", RTT_CTRL_TEXT_MAGENTA, RTT_CTRL_RESET);
}

/**@brief Function for initializing security parameters.
 */
static void sec_params_init(void)
{
    m_sec_params.bond         = SEC_PARAM_BOND;
    m_sec_params.mitm         = SEC_PARAM_MITM;
    m_sec_params.io_caps      = SEC_PARAM_IO_CAPABILITIES;
    m_sec_params.oob          = SEC_PARAM_OOB;
    m_sec_params.min_key_size = SEC_PARAM_MIN_KEY_SIZE;
    m_sec_params.max_key_size = SEC_PARAM_MAX_KEY_SIZE;
}


/**@brief Function for handling the Device Manager events.
 *
 * @param[in]   p_evt   Data associated to the device manager event.
 */
static uint32_t device_manager_evt_handler(dm_handle_t const    * p_handle,
                                           dm_event_t const     * p_event,
                                           ret_code_t           event_result)
{
    // rtt_print(0, ">> device_manager_evt_handler");
    APP_ERROR_CHECK(event_result);

    switch(p_event->event_id)
    {
        case DM_EVT_DEVICE_CONTEXT_LOADED: // Fall through.
        case DM_EVT_SECURITY_SETUP_COMPLETE:
            m_bonded_peer_handle = (*p_handle);
            break;
        case DM_EVT_LINK_SECURED:
            app_context_load(p_handle);
            break;
    }

    return NRF_SUCCESS;
}


/**@brief Function for the Device Manager initialization.
 *
 * @param[in] erase_bonds  Indicates whether bonding information should be cleared from
 *                         persistent storage during initialization of the Device Manager.
 */
static void device_manager_init()
{
    uint32_t               err_code;
    dm_init_param_t        init_param;
    dm_application_param_t  register_param;

    // Initialize peer device handle.
    err_code = dm_handle_initialize(&m_bonded_peer_handle);
    APP_ERROR_CHECK(err_code);
    
    // Initialize persistent storage module.
    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);

    err_code = dm_init(&init_param);
    APP_ERROR_CHECK(err_code);
    
    memset(&register_param.sec_param, 0, sizeof(ble_gap_sec_params_t));

    register_param.sec_param.bond         = SEC_PARAM_BOND;
    register_param.sec_param.mitm         = SEC_PARAM_MITM;
    register_param.sec_param.io_caps      = SEC_PARAM_IO_CAPABILITIES;
    register_param.sec_param.oob          = SEC_PARAM_OOB;
    register_param.sec_param.min_key_size = SEC_PARAM_MIN_KEY_SIZE;
    register_param.sec_param.max_key_size = SEC_PARAM_MAX_KEY_SIZE;
    register_param.evt_handler            = device_manager_evt_handler;
    register_param.service_type           = DM_PROTOCOL_CNTXT_GATT_SRVR_ID;

    err_code = dm_register(&m_app_handle, &register_param);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_DIRECTED:
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING_DIRECTED);
            APP_ERROR_CHECK(err_code);
            rtt_print(0, "%sAdvertising directed%s\n", RTT_CTRL_TEXT_BRIGHT_CYAN, RTT_CTRL_RESET);
            break;

        case BLE_ADV_EVT_FAST:
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            rtt_print(0, "%sAdvertising fast%s\n", RTT_CTRL_TEXT_BRIGHT_CYAN, RTT_CTRL_RESET);
            break;

        case BLE_ADV_EVT_FAST_WHITELIST:
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING_WHITELIST);
            APP_ERROR_CHECK(err_code);
            rtt_print(0, "%sAdvertising fast whitelist%s\n", RTT_CTRL_TEXT_BRIGHT_CYAN, RTT_CTRL_RESET);
            break;

        case BLE_ADV_EVT_SLOW:
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING_SLOW);
            APP_ERROR_CHECK(err_code);
            rtt_print(0, "%sAdvertising slow%s\n", RTT_CTRL_TEXT_BRIGHT_CYAN, RTT_CTRL_RESET);
            break;

        case BLE_ADV_EVT_IDLE:
            rtt_print(0, "%sAdvertising idle%s\n", RTT_CTRL_TEXT_BRIGHT_CYAN, RTT_CTRL_RESET);
            // sleep_mode_enter();
            err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
            rtt_print(0, "%sRestarted BLE advertising fast%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_ADV_EVT_WHITELIST_REQUEST:
        {
            // ble_gap_whitelist_t whitelist;
            // ble_gap_addr_t    * p_whitelist_addr[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
            // ble_gap_irk_t     * p_whitelist_irk[BLE_GAP_WHITELIST_IRK_MAX_COUNT];
            //
            // whitelist.addr_count = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;
            // whitelist.irk_count  = BLE_GAP_WHITELIST_IRK_MAX_COUNT;
            // whitelist.pp_addrs   = p_whitelist_addr;
            // whitelist.pp_irks    = p_whitelist_irk;
            //
            // err_code = dm_whitelist_create(&m_app_handle, &whitelist);
            // APP_ERROR_CHECK(err_code);
            //
            // err_code = ble_advertising_whitelist_reply(&whitelist);
            // APP_ERROR_CHECK(err_code);
            rtt_print(0, "%sAdvertising whitelist request, ignoring%s\n", RTT_CTRL_TEXT_BRIGHT_CYAN, RTT_CTRL_RESET);
            break;
        }

        default:
            break;
    }
}

// ====================
// = Bluetooth events =
// ====================


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the scheduler in the main loop after a BLE stack
 *          event has been received.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
	rtt_print(0, "%sBluetooth (conn: %X) event: %s%X%s\n", RTT_CTRL_TEXT_BLUE, p_ble_evt->evt.gap_evt.conn_handle, RTT_CTRL_TEXT_BRIGHT_BLUE, p_ble_evt->header.evt_id, RTT_CTRL_RESET);

    dm_ble_evt_handler(p_ble_evt);
    ble_conn_params_on_ble_evt(p_ble_evt);
    ble_dfu_on_ble_evt(&m_dfus, p_ble_evt);
    ble_bas_on_ble_evt(&m_bas, p_ble_evt);
    on_ble_evt(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);
    ble_buttonstatus_on_ble_evt(&m_buttonservice, p_ble_evt, &m_mem_block);
    
    if (p_ble_evt->header.evt_id == BLE_GATTS_EVT_WRITE) {
        send_last_unconnected_button_status();
    }
}

/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t                         err_code;
    static ble_gap_evt_auth_status_t m_auth_status;
    bool                             master_id_matches = false;
    ble_gap_sec_kdist_t *            p_distributed_keys;
    ble_gap_enc_info_t *             p_enc_info;
    ble_gap_irk_t *                  p_id_info;
    ble_gap_sign_info_t *            p_sign_info;
    //ble_gatts_evt_write_t *          p_evt_write;

    static ble_gap_enc_key_t         m_enc_key;           /**< Encryption Key (Encryption Info and Master ID). */
    static ble_gap_id_key_t          m_id_key;            /**< Identity Key (IRK and address). */
    static ble_gap_sign_info_t       m_sign_key;          /**< Signing Key (Connection Signature Resolving Key). */
    static ble_gap_sec_keyset_t      m_keys = {.keys_periph = {&m_enc_key, &m_id_key, &m_sign_key}};

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            rtt_print(0, "m_conn_handle: connected: %X\n", p_ble_evt->evt.gap_evt.conn_handle);
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

            /* YOUR_JOB: Uncomment this part if you are using the app_button module to handle button
                         events (assuming that the button events are only needed in connected
                         state). If this is uncommented out here,
                            1. Make sure that app_button_disable() is called when handling
                               BLE_GAP_EVT_DISCONNECTED below.
                            2. Make sure the app_button module is initialized. */
            err_code = app_button_enable();
            APP_ERROR_CHECK(err_code);
            rtt_print(0, "%s%sBluetooth connected.%s\n", RTT_CTRL_BG_BLUE, RTT_CTRL_TEXT_BRIGHT_YELLOW, RTT_CTRL_RESET);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            rtt_print(0, "m_conn_handle: invalid\n");
            m_conn_handle = BLE_CONN_HANDLE_INVALID;

            /* YOUR_JOB: Uncomment this part if you are using the app_button module to handle button
                         events. This should be done to save power when not connected
                         to a peer. */
            // err_code = app_button_disable();
            // APP_ERROR_CHECK(err_code);
            
            rtt_print(0, "%s%sBluetooth %sdisconnected.%s\n", RTT_CTRL_BG_BLUE, RTT_CTRL_TEXT_BRIGHT_RED, RTT_CTRL_TEXT_BRIGHT_RED, RTT_CTRL_RESET);
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            rtt_print(0, "m_conn_handle: sec params request");
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle,
                                                   BLE_GAP_SEC_STATUS_SUCCESS,
                                                   &m_sec_params,
                                                   &m_keys);
            APP_ERROR_CHECK(err_code);
            rtt_print(0, "%sBluetooth params request.%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle,
                                                 NULL,
                                                 0,
                                                 BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS | BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS);
            APP_ERROR_CHECK(err_code);
            rtt_print(0, "%sBluetooth sys attr missing.%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
            break;

        case BLE_GAP_EVT_AUTH_STATUS:
            m_auth_status = p_ble_evt->evt.gap_evt.params.auth_status;
            rtt_print(0, "%sBluetooth auth status.%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
            break;

        case BLE_GAP_EVT_SEC_INFO_REQUEST:
            master_id_matches  = memcmp(&p_ble_evt->evt.gap_evt.params.sec_info_request.master_id,
                                        &m_enc_key.master_id,
                                        sizeof(ble_gap_master_id_t)) == 0;
            p_distributed_keys = &m_auth_status.kdist_periph;

            p_enc_info  = (p_distributed_keys->enc  && master_id_matches) ? &m_enc_key.enc_info : NULL;
            p_id_info   = (p_distributed_keys->id   && master_id_matches) ? &m_id_key.id_info   : NULL;
            p_sign_info = (p_distributed_keys->sign && master_id_matches) ? &m_sign_key         : NULL;

            err_code = sd_ble_gap_sec_info_reply(m_conn_handle, p_enc_info, p_id_info, p_sign_info);
                APP_ERROR_CHECK(err_code);
            rtt_print(0, "%sBluetooth security info request.%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
            break;
        
        case BLE_EVT_TX_COMPLETE:
            rtt_print(0, "%sTX complete\n", RTT_CTRL_TEXT_BLUE);
            m_last_unconnected_button_start = 0;
            break;

        case BLE_GAP_EVT_CONN_PARAM_UPDATE:
            rtt_print(0, "%sConnection params update: %dms (0x%X)\n", RTT_CTRL_TEXT_BLUE, 
                         p_ble_evt->evt.gap_evt.params.conn_param_update.conn_params.min_conn_interval,
                         p_ble_evt->evt.gap_evt.params.conn_param_update.conn_params.min_conn_interval);   
            break;
            
        case BLE_GATTS_EVT_WRITE:
            //p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;
            //rtt_print(0, "%sGatt written to by client: %s%X\n", RTT_CTRL_TEXT_BLUE, RTT_CTRL_TEXT_BRIGHT_BLUE, p_evt_write);
            break;

        case BLE_EVT_USER_MEM_REQUEST:
        	rtt_print(0, "main -> BLE_EVT_USER_MEM_REQUEST\n");
            /*rtt_print(0, "%sBluetooth user mem request (%X)\n", RTT_CTRL_TEXT_BLUE, p_ble_evt->evt.gap_evt.conn_handle);
            m_mem_block.p_mem = &m_mem_queue[0];
            m_mem_block.len = MEM_BLOCK_SIZE;
            err_code = sd_ble_user_mem_reply(p_ble_evt->evt.gap_evt.conn_handle, &m_mem_block);
            APP_ERROR_CHECK(err_code);*/
            break;

        case BLE_EVT_USER_MEM_RELEASE:
        	rtt_print(0, "main -> BLE_EVT_USER_MEM_RELEASE\n");
            /*rtt_print(0, "%sBluetooth user mem release (%X, %X)\n", RTT_CTRL_TEXT_BLUE, p_ble_evt->evt.gap_evt.conn_handle, *((uint16_t *)m_mem_block.p_mem));
            if (p_ble_evt->evt.common_evt.params.user_mem_release.mem_block.p_mem == m_mem_block.p_mem) {
            
            }*/
            break;

        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
            rtt_print(0, "%sBluetooth RW authorize request: %X == %X/%x, %X\n", RTT_CTRL_TEXT_BLUE, p_ble_evt->evt.gatts_evt.params.write.op, BLE_GATTS_OP_PREP_WRITE_REQ, BLE_GATTS_OP_EXEC_WRITE_REQ_NOW, p_ble_evt->evt.gatts_evt.params.write.data[0]);
            m_rw_authorize_reply.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
            m_rw_authorize_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
            
            err_code = sd_ble_gatts_rw_authorize_reply(m_conn_handle, &m_rw_authorize_reply);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            rtt_print(0, "%sBluetooth event unhandled: %s%X(%d)%s\n", RTT_CTRL_TEXT_BLUE, RTT_CTRL_TEXT_BRIGHT_BLUE, p_ble_evt->header.evt_id, p_ble_evt->header.evt_len, RTT_CTRL_RESET);
            break;
    }
}

static void send_last_unconnected_button_status(void) {
    uint32_t err_code;
    rtt_print(0, "%ssend_last_unconnected_button_status 1: %s%d - %d < %d\n", RTT_CTRL_TEXT_BLUE, RTT_CTRL_TEXT_BRIGHT_BLUE, NRF_RTC1->COUNTER, m_last_unconnected_button_start, APP_TIMER_TICKS(UNCONNECTED_BUTTON_PRESS_DURATION, APP_TIMER_PRESCALER), RTT_CTRL_RESET);
    
    if (m_last_unconnected_button_start == 0) return;
    if ((NRF_RTC1->COUNTER - m_last_unconnected_button_start) < APP_TIMER_TICKS(UNCONNECTED_BUTTON_PRESS_DURATION, APP_TIMER_PRESCALER)) {
        rtt_print(0, "%ssend_last_unconnected_button_status 2: %s%d - %d < %d\n", RTT_CTRL_TEXT_BLUE, RTT_CTRL_TEXT_BRIGHT_BLUE, NRF_RTC1->COUNTER, m_last_unconnected_button_start, APP_TIMER_TICKS(UNCONNECTED_BUTTON_PRESS_DURATION, APP_TIMER_PRESCALER), RTT_CTRL_RESET);
        
        if (m_conn_handle != BLE_CONN_HANDLE_INVALID) {
            rtt_print(0, "%ssend_last_unconnected_button_status 3: %s%d - %d < %d\n", RTT_CTRL_TEXT_BLUE, RTT_CTRL_TEXT_BRIGHT_BLUE, NRF_RTC1->COUNTER, m_last_unconnected_button_start, APP_TIMER_TICKS(UNCONNECTED_BUTTON_PRESS_DURATION, APP_TIMER_PRESCALER), RTT_CTRL_RESET);
            
            err_code = ble_buttonstatus_on_button_change(&m_buttonservice, m_last_button_state);
            if ((err_code != NRF_SUCCESS) &&
                (err_code != BLE_ERROR_INVALID_CONN_HANDLE) &&
                (err_code != NRF_ERROR_INVALID_STATE) &&
                (err_code != BLE_ERROR_NO_TX_BUFFERS) &&
                (err_code != NRF_ERROR_BUSY) &&
                (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING))
            {
                // Can ignore this error, just means that bluetooth is connected but nobody's listening yet
                rtt_print(0, "BLE_ERROR_GATTS_SYS_ATTR_MISSING (or similar)\n");
                // Can ignore this error, just means buffer capacity exceeded (too many button pushes)
                // rtt_print(0, "BLE_ERROR_NO_TX_BUFFERS\n");
                APP_ERROR_HANDLER(err_code);
            }
            
            bsp_evt_handler(BSP_EVENT_KEY_0);
        } else {
            rtt_print(0, "%sIgnoring unconnected button, not connected (!!): %s\n", RTT_CTRL_TEXT_BRIGHT_BLACK, RTT_CTRL_TEXT_BLUE);
        }
    }
}

// =======
// = DFU =
// =======

/**@brief Function for stopping advertising.
 */
static void advertising_stop(void)
{
    uint32_t err_code;

    err_code = sd_ble_gap_adv_stop();
    APP_ERROR_CHECK(err_code);

    err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for loading application-specific context after establishing a secure connection.
 *
 * @details This function will load the application context and check if the ATT table is marked as 
 *          changed. If the ATT table is marked as changed, a Service Changed Indication
 *          is sent to the peer if the Service Changed CCCD is set to indicate.
 *
 * @param[in] p_handle The Device Manager handle that identifies the connection for which the context 
 *                     should be loaded.
 */
static void app_context_load(dm_handle_t const * p_handle)
{
    uint32_t                 err_code;
    static uint32_t          context_data;
    dm_application_context_t context;

    context.len    = sizeof(context_data);
    context.p_data = (uint8_t *)&context_data;

    err_code = dm_application_context_get(p_handle, &context);
    if (err_code == NRF_SUCCESS)
    {
        // Send Service Changed Indication if ATT table has changed.
        if ((context_data & (DFU_APP_ATT_TABLE_CHANGED << DFU_APP_ATT_TABLE_POS)) != 0)
        {
            err_code = sd_ble_gatts_service_changed(m_conn_handle, APP_SERVICE_HANDLE_START, BLE_HANDLE_MAX);
            if ((err_code != NRF_SUCCESS) &&
                (err_code != BLE_ERROR_INVALID_CONN_HANDLE) &&
                (err_code != NRF_ERROR_INVALID_STATE) &&
                (err_code != BLE_ERROR_NO_TX_BUFFERS) &&
                (err_code != NRF_ERROR_BUSY) &&
                (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING))
            {
                APP_ERROR_HANDLER(err_code);
            }
        }

        err_code = dm_application_context_delete(p_handle);
        APP_ERROR_CHECK(err_code);
    }
    else if (err_code == DM_NO_APP_CONTEXT)
    {
        // No context available. Ignore.
    }
    else
    {
        APP_ERROR_HANDLER(err_code);
    }
}


/**@brief Function for preparing for system reset.
 *
 * @details This function implements @ref dfu_app_reset_prepare_t. It will be called by 
 *          @ref dfu_app_handler.c before entering the bootloader/DFU.
 *          This allows the current running application to shut down gracefully.
 */
static void reset_prepare(void)
{
    uint32_t err_code;

    if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        // Disconnect from peer.
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        APP_ERROR_CHECK(err_code);
        err_code = bsp_indication_set(BSP_INDICATE_IDLE);
        APP_ERROR_CHECK(err_code);
    }
    else
    {
        // If not connected, the device will be advertising. Hence stop the advertising.
        advertising_stop();
    }

    err_code = ble_conn_params_stop();
    APP_ERROR_CHECK(err_code);

    nrf_delay_ms(500);
}

// ============
// = Services =
// ============

static void firmware_nickname_write_handler(ble_buttonservice_t *p_buttonservice, uint8_t* nickname,uint8_t length) {
     uint32_t err_code;

    rtt_print(0, "New nickname : %s\n", nickname);
	memset(m_nickname_storage,0,FIRMWARE_NICKNAME_MAX_LENGTH);
    memcpy(m_nickname_storage, nickname, length);

	// flash must be cleared before saving new data to it
	err_code = pstorage_clear(&m_flash_handle, 32);

	APP_ERROR_CHECK(err_code);

}

static void pstorage_callback_handler(pstorage_handle_t * handle,
                                      uint8_t             op_code,
                                      uint32_t            reason,
                                      uint8_t           * p_data,
                                      uint32_t            param_len)
{
		
		switch(op_code)
    {
        case PSTORAGE_STORE_OP_CODE: 
            if (reason != NRF_SUCCESS)
						{
								rtt_print(0, "pstorage store error: %x\n", reason);
						} else {
								rtt_print(0, "pstorage store success: \n");
								for (int i = 0; i <param_len; i++)
								{
										SEGGER_RTT_printf(0,"%02X ", p_data[i]);
								}
								SEGGER_RTT_printf(0,"\n");
								SEGGER_RTT_Write(0,(const char *)p_data,param_len);
								SEGGER_RTT_printf(0,"\n");
						}        
            break;
        case PSTORAGE_LOAD_OP_CODE:
            if (reason != NRF_SUCCESS)
						{
								rtt_print(0, "pstorage load error: %x\n", reason);
						} else {
								rtt_print(0, "pstorage load success: \n");
								for (int i = 0; i <param_len; i++)
								{
										SEGGER_RTT_printf(0,"%02X ", p_data[i]);
								}
								SEGGER_RTT_printf(0,"\n\r");
								SEGGER_RTT_Write(0,(const char *)p_data,param_len);
								SEGGER_RTT_printf(0,"\n\r");
						}
            break;
        case PSTORAGE_CLEAR_OP_CODE:
				if (reason != NRF_SUCCESS)
						{
								rtt_print(0, "pstorage clear error: %x\n", reason);
						} else {
								rtt_print(0, "pstorage clear success:\n");
								//save new nickname
                uint32_t err_code = pstorage_store(&m_flash_handle,
                       m_nickname_storage,
                       FIRMWARE_NICKNAME_MAX_LENGTH,
                       0);
								APP_ERROR_CHECK(err_code);
						}
            break;
        case PSTORAGE_UPDATE_OP_CODE:
                
            break;
        default:
                
            break;
    }
}


/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    uint32_t                err_code;
    ble_buttonstatus_init_t buttons_init;
    ble_bas_init_t          bas_init;
    ble_dis_init_t          dis_init;
    ble_dfu_init_t          dfus_init;
    pstorage_module_param_t params;
    
    // Button Status Service
    buttons_init.firmware_nickname_write_handler = firmware_nickname_write_handler;

    params.block_size   = FIRMWARE_NICKNAME_MAX_LENGTH;
    params.block_count  = 1;
    params.cb           = pstorage_callback_handler;    
    err_code = pstorage_register(&params, &m_flash_handle);
    APP_ERROR_CHECK(err_code);
    
    err_code = pstorage_load(m_nickname_storage, &m_flash_handle, FIRMWARE_NICKNAME_MAX_LENGTH, 0);
    APP_ERROR_CHECK(err_code);

    memset(buttons_init.nickname_str, 0, FIRMWARE_NICKNAME_MAX_LENGTH);
    memcpy(buttons_init.nickname_str, m_nickname_storage, FIRMWARE_NICKNAME_MAX_LENGTH);
    rtt_print(0, "Setting nickname: \n");
  
    // Button Service
    err_code = ble_buttonstatus_init(&m_buttonservice, &buttons_init);
    APP_ERROR_CHECK(err_code);

    // Battery Service
    memset(&bas_init, 0, sizeof(bas_init));
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_char_attr_md.cccd_write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_char_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&bas_init.battery_level_char_attr_md.write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_report_read_perm);

    bas_init.evt_handler          = NULL;
    bas_init.support_notification = true;
    bas_init.p_report_ref         = NULL;
    bas_init.initial_batt_level   = 100;

    err_code = ble_bas_init(&m_bas, &bas_init);
    APP_ERROR_CHECK(err_code);
    
    // Initialize Device Information Service
    memset(&dis_init, 0, sizeof(dis_init));
    ble_srv_ascii_to_utf8(&dis_init.manufact_name_str, MANUFACTURER_NAME);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&dis_init.dis_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&dis_init.dis_attr_md.write_perm);
    
    err_code = ble_dis_init(&dis_init);
    APP_ERROR_CHECK(err_code);
    
    // Initialize the Device Firmware Update Service.
    memset(&dfus_init, 0, sizeof(dfus_init));

    dfus_init.evt_handler   = dfu_event_handler;
    dfus_init.error_handler = dfu_error_handler;
    dfus_init.revision      = DFU_REVISION;

    err_code = ble_dfu_init(&m_dfus, &dfus_init);
    APP_ERROR_CHECK(err_code);

    dfu_app_reset_prepare_set(reset_prepare);
    dfu_app_dm_appl_instance_set(m_app_handle);
}

void dfu_event_handler(ble_dfu_t * p_dfu, ble_dfu_evt_t * p_evt) {
    rtt_print(0, "DFU event: %X\n", p_evt->ble_dfu_evt_type);
    dfu_app_on_dfu_evt(p_dfu, p_evt);
}

void dfu_error_handler(uint32_t nrf_error) {
    rtt_print(0, "\nDFU error event: %X\n", nrf_error);
    if ((nrf_error != NRF_SUCCESS) &&
        (nrf_error != BLE_ERROR_INVALID_CONN_HANDLE) &&
        (nrf_error != NRF_ERROR_INVALID_STATE) &&
        (nrf_error != BLE_ERROR_NO_TX_BUFFERS) &&
        (nrf_error != NRF_ERROR_BUSY) &&
        (nrf_error != BLE_ERROR_GATTS_SYS_ATTR_MISSING))
    {
        rtt_print(0, "Real BLE error!");
        // Can ignore this error, just means that bluetooth is connected but nobody's listening yet
        // rtt_print(0, "BLE_ERROR_GATTS_SYS_ATTR_MISSING (or similar)\n");
        // Can ignore this error, just means buffer capacity exceeded (too many button pushes)
        // rtt_print(0, "BLE_ERROR_NO_TX_BUFFERS\n");
        APP_ERROR_HANDLER(nrf_error);
    }
}

// =========
// = Power =
// =========


static void gpio_start(void) {
    NVIC_EnableIRQ(GPIOTE_IRQn);
    NRF_GPIOTE->INTENSET = GPIOTE_INTENSET_PORT_Set << GPIOTE_INTENSET_PORT_Pos;
}


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module.
 */
static void timers_init(void)
{
    uint32_t err_code;
    
    // Initialize timer module, making it use the scheduler
    // APP_TIMER_APPSH_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, true);
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);
    
    err_code = app_timer_create(&m_battery_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                battery_level_meas_timeout_handler);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_doubleclick_timer_id,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                doubleclick_timeout_handler);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for starting application timers.
 */
static void application_timers_start(void)
{
    uint32_t err_code;

    // Start application timers.
    err_code = app_timer_start(m_battery_timer_id, BATTERY_LEVEL_MEAS_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for the Power manager.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    rtt_print(0, "%sSleeping...%s\n", RTT_CTRL_TEXT_BRIGHT_RED, RTT_CTRL_RESET);

    uint32_t err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    uint32_t err_code;
    SEGGER_RTT_Init();
    
    rtt_print(0, "%s%sStarting up Turn Touch Remote...%s\n", RTT_CTRL_TEXT_BRIGHT_YELLOW,
                                                          RTT_CTRL_BG_BRIGHT_MAGENTA, 
                                                          RTT_CTRL_RESET);
    
    // Initialize buttons
    clock_initialization();
    rtt_print(0, "%sInitialized clock%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
    timers_init();
    rtt_print(0, "%sInitialized timers%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
    bsp_configuration();
    rtt_print(0, "%sInitialized BSP%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);

    // Initialize
    ble_stack_init();
    rtt_print(0, "%sInitialized BLE stack%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
    scheduler_init();
    rtt_print(0, "%sInitialized scheduler%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
    device_manager_init();
    rtt_print(0, "%sInitialized device manager%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
    gap_params_init();
    rtt_print(0, "%sInitialized GAP Params%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
    advertising_init();
    rtt_print(0, "%sInitialized advertising%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
    services_init();
    rtt_print(0, "%sInitialized services%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
    conn_params_init();
    rtt_print(0, "%sInitialized connection params%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
    sec_params_init();
    rtt_print(0, "%sInitialized security params%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
    battery_level_update();
    rtt_print(0, "%sInitialized battery level updater%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);

    // Start execution
    gpio_start();
    rtt_print(0, "%sStarted GPIO%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
    application_timers_start();
    rtt_print(0, "%sStarted application timers%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
    err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
    rtt_print(0, "%sStarted BLE advertising fast%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
    APP_ERROR_CHECK(err_code);
    err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
    APP_ERROR_CHECK(err_code);

    while (true)
    {
        app_sched_execute();
        power_manage();
    }
}


/**
 * @}
 */
