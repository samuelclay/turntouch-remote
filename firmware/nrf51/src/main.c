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
#include "app_error.h"
#include "app_scheduler.h"
#include "app_timer.h"
#include "app_timer_appsh.h"
#include "app_trace.h"
#include "ble.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "boards.h"
#include "bsp.h"
#include "button_status.h"
#include "nordic_common.h"
#include "nrf.h"
#include "nrf51_bitfields.h"
#include "nrf_drv_gpiote.h"
#include "nrf_error.h"
#include "nrf_gpio.h"
#include "rtt.h"
#include "softdevice_handler.h"


#define APP_TIMER_PRESCALER      0                           /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS     (2 + BSP_APP_TIMERS_NUMBER) /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE  4                           /**< Size of timer operation queues. */

// #define BUTTON_PREV_ID           0                           /**< Button used to switch the state. */
// #define BUTTON_NEXT_ID           1                           /**< Button used to switch the state. */
// #ifdef BSP_BUTTON_0
//     #define PIN_IN BSP_BUTTON_0
// #endif
// #ifndef PIN_IN
//     #error "Please indicate input pin"
// #endif
//
// #ifdef BSP_LED_0
//     #define PIN_OUT BSP_LED_0
// #endif
// #ifndef PIN_OUT
//     #error "Please indicate output pin"
// #endif


#define IS_SRVC_CHANGED_CHARACT_PRESENT 0                                           /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#define DEVICE_NAME                     "Turn Touch Remote"                           /**< Name of device. Will be included in the advertising data. */

#define APP_ADV_FAST_INTERVAL           40                                          /**< The advertising interval (in units of 0.625 ms). The default value corresponds to 25 ms. */
#define APP_ADV_SLOW_INTERVAL           3200                                        /**< Slow advertising interval (in units of 0.625 ms). The default value corresponds to 2 seconds. */
#define APP_ADV_FAST_TIMEOUT            180                                         /**< The advertising time-out in units of seconds. */
#define APP_ADV_SLOW_TIMEOUT            180                                         /**< The advertising time-out in units of seconds. */
#define ADV_INTERVAL_FAST_PERIOD        30                                          /**< The duration of the fast advertising period (in seconds). */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(100, UNIT_1_25_MS)            /**< Minimum acceptable connection interval (0.5 seconds). */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(200, UNIT_1_25_MS)           /**< Maximum acceptable connection interval (1 second). */
#define SLAVE_LATENCY                   4                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds). */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)  /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER) /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define SEC_PARAM_BOND                  1                                           /**< Perform bonding. */
#define SEC_PARAM_MITM                  0                                           /**< Man In The Middle protection not required. */
#define SEC_PARAM_IO_CAPABILITIES       BLE_GAP_IO_CAPS_NONE                        /**< No I/O capabilities. */
#define SEC_PARAM_OOB                   0                                           /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE          7                                           /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE          16                                          /**< Maximum encryption key size. */

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

// YOUR_JOB: Modify these according to requirements (e.g. if other event types are to pass through
//           the scheduler).
#define SCHED_MAX_EVENT_DATA_SIZE       sizeof(app_timer_event_t)                   /**< Maximum size of scheduler events. Note that scheduler BLE stack events do not contain any data, as the events are being pulled from the stack in the event handler. */
#define SCHED_QUEUE_SIZE                10                                          /**< Maximum number of events in the scheduler queue. */

static ble_gap_sec_params_t             m_sec_params;                               /**< Security requirements for this application. */
static uint16_t                         m_conn_handle = BLE_CONN_HANDLE_INVALID;    /**< Handle of the current connection. */
static ble_buttonservice_t              m_buttonservice;

static void on_adv_evt(ble_adv_evt_t ble_adv_evt);
static void sleep_mode_enter(void);

// ===========
// = Buttons =
// ===========

void bsp_evt_handler(bsp_event_t evt) {
    rtt_print(0, "%sButton handler: %s%X%s\n", RTT_CTRL_TEXT_YELLOW, RTT_CTRL_TEXT_BRIGHT_YELLOW, evt, RTT_CTRL_RESET);
    bool pushed;
    switch (evt)
    {
        case BSP_EVENT_KEY_0:

            app_button_is_pushed(0, &pushed);
            if (pushed) {
                LEDS_ON(BSP_LED_0_MASK);
            } else {
                LEDS_OFF(BSP_LED_0_MASK);
            }
            break;

        case BSP_EVENT_KEY_1:

            app_button_is_pushed(1, &pushed);
            if (pushed) {
                LEDS_ON(BSP_LED_1_MASK);
            } else {
                LEDS_OFF(BSP_LED_1_MASK);
            }   
            break;

        case BSP_EVENT_KEY_2:

            app_button_is_pushed(2, &pushed);
            if (pushed) {
                LEDS_ON(BSP_LED_2_MASK);
            } else {
                LEDS_OFF(BSP_LED_2_MASK);
            }   
            break;

        case BSP_EVENT_KEY_3:

            app_button_is_pushed(3, &pushed);
            if (pushed) {
                LEDS_ON(BSP_LED_3_MASK);
            } else {
                LEDS_OFF(BSP_LED_3_MASK);
            }   
            break;
        
        case BSP_EVENT_KEY_4:
        case BSP_EVENT_KEY_5:
        case BSP_EVENT_KEY_6:
        case BSP_EVENT_KEY_7:

            LEDS_ON(LEDS_MASK);
            break;
        
        default:
            return; // no implementation needed
    }
    
    

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
    ble_advertising_on_sys_evt(sys_evt);
}

/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t                         err_code;
    static ble_gap_evt_auth_status_t m_auth_status;
    bool                             master_id_matches;
    ble_gap_sec_kdist_t *            p_distributed_keys;
    ble_gap_enc_info_t *             p_enc_info;
    ble_gap_irk_t *                  p_id_info;
    ble_gap_sign_info_t *            p_sign_info;

    static ble_gap_enc_key_t         m_enc_key;           /**< Encryption Key (Encryption Info and Master ID). */
    static ble_gap_id_key_t          m_id_key;            /**< Identity Key (IRK and address). */
    static ble_gap_sign_info_t       m_sign_key;          /**< Signing Key (Connection Signature Resolving Key). */
    static ble_gap_sec_keyset_t      m_keys = {.keys_periph = {&m_enc_key, &m_id_key, &m_sign_key}};

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
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
            rtt_print(0, "%sBluetooth connected.%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            m_conn_handle = BLE_CONN_HANDLE_INVALID;

            /* YOUR_JOB: Uncomment this part if you are using the app_button module to handle button
                         events. This should be done to save power when not connected
                         to a peer. */
            err_code = app_button_disable();
            APP_ERROR_CHECK(err_code);
            rtt_print(0, "%sBluetooth %sdisconnected.%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_TEXT_BRIGHT_RED, RTT_CTRL_RESET);
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
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

        default:
            // No implementation needed.
            rtt_print(0, "%sBluetooth event unhandled: %d%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, p_ble_evt->header.evt_id, RTT_CTRL_RESET);
            break;
    }
}

/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the scheduler in the main loop after a BLE stack
 *          event has been received.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    on_ble_evt(p_ble_evt);
    ble_conn_params_on_ble_evt(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);

    ble_buttonstatus_on_ble_evt(&m_buttonservice, p_ble_evt);
    
    rtt_print(0, "%sBluetooth event: %s%X%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_TEXT_BRIGHT_GREEN, p_ble_evt->header, RTT_CTRL_RESET);
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

    // Enable BLE stack 
    ble_enable_params_t ble_enable_params;
    memset(&ble_enable_params, 0, sizeof(ble_enable_params));
    ble_enable_params.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT;
    err_code = sd_ble_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // ble_gap_addr_t addr;
    //
    // err_code = sd_ble_gap_address_get(&addr);
    // APP_ERROR_CHECK(err_code);
    // sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &addr);
    // APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);
    
    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
    
    rtt_print(0, "%sStarted bluetooth stack.%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
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
    
    rtt_print(0, "%sBluetooth GAP connection params init.%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
}

/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t      err_code;
    ble_advdata_t advdata;

    ble_uuid_t m_adv_uuids[] = {{BUTTONSERVICE_UUID_SERVICE, BLE_UUID_TYPE_BLE}};         /**< Universally unique service identifiers. */

    // Build advertising data struct to pass into @ref ble_advertising_init.
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance      = true;
    advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    advdata.uuids_complete.p_uuids  = m_adv_uuids;

    ble_adv_modes_config_t options = {0};
    options.ble_adv_fast_enabled   = BLE_ADV_FAST_ENABLED;
    options.ble_adv_fast_interval  = APP_ADV_FAST_INTERVAL;
    options.ble_adv_fast_timeout   = APP_ADV_FAST_TIMEOUT;
    options.ble_adv_slow_enabled   = BLE_ADV_SLOW_ENABLED;
    options.ble_adv_slow_interval  = APP_ADV_SLOW_INTERVAL;
    options.ble_adv_slow_timeout   = APP_ADV_SLOW_TIMEOUT;

    err_code = ble_advertising_init(&advdata, NULL, &options, on_adv_evt, NULL);
    APP_ERROR_CHECK(err_code);
    
    rtt_print(0, "%sStarted bluetooth advertising.%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
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
    
    rtt_print(0, "%sConnection params initialized.%s\n", RTT_CTRL_TEXT_BRIGHT_BLUE, RTT_CTRL_RESET);
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
            sleep_mode_enter();
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

static void firmware_nickname_write_handler(ble_buttonservice_t *p_buttonservice, uint8_t nickname) {
    rtt_print(0, "%s%sNew nickname: %s%X%s", RTT_CTRL_BG_BLUE, RTT_CTRL_TEXT_BRIGHT_WHITE,
              RTT_CTRL_TEXT_BRIGHT_GREEN, nickname, RTT_CTRL_RESET);
}

/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    uint32_t err_code;
    ble_buttonstatus_init_t init;
    
    init.firmware_nickname_write_handler = firmware_nickname_write_handler;
    
    err_code = ble_buttonstatus_init(&m_buttonservice, &init);
    APP_ERROR_CHECK(err_code);
}

// =========
// = Power =
// =========

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module.
 */
static void timers_init(void)
{
    // Initialize timer module, making it use the scheduler
    APP_TIMER_APPSH_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);
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

    rtt_print(0, "%s%sStarting up Turn Touch Remote%s\n", RTT_CTRL_TEXT_BRIGHT_YELLOW,
                                                          RTT_CTRL_BG_BRIGHT_MAGENTA, 
                                                          RTT_CTRL_RESET);
    
    // Initialize buttons
    // clock_initialization();
    timers_init();
    bsp_configuration();

    // Initialize
    ble_stack_init();
    scheduler_init();

    gap_params_init();
    advertising_init();
    services_init();
    conn_params_init();
    sec_params_init();

    // Start execution
    err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
    // err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
    // APP_ERROR_CHECK(err_code);

    while (true)
    {
        app_sched_execute();
        // power_manage();
    }
}


/**
 * @}
 */
