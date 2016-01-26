/* Copyright 2015 Turn Touch. All rights reserved.
 * 
 * Header for Turn Touch Remote
 */

#include "app_timer.h"
#include "ble_advertising.h"
#include "ble_bas.h"
#include "ble_dfu.h"
#include "button_status.h"
#include "device_manager.h"
#include "pstorage.h"

#define APP_TIMER_PRESCALER      0                           /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE  4                           /**< Size of timer operation queues. */
#define APP_FEATURE_NOT_SUPPORTED            BLE_GATT_STATUS_ATTERR_APP_BEGIN + 2                    /**< Reply when unsupported features are requested. */

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

#define IS_SRVC_CHANGED_CHARACT_PRESENT 1                                           /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#define DEVICE_NAME                     "Turn Touch Remote"                         /**< Name of device. Will be included in the advertising data. */
#define MANUFACTURER_NAME               "Turn Touch"                                /**< Manufacturer. Will be passed to Device Information Service. */

#define APP_ADV_FAST_INTERVAL           40                                          /**< The advertising interval (in units of 0.625 ms). The default value corresponds to 25 ms. */
#define APP_ADV_SLOW_INTERVAL           3200                                        /**< Slow advertising interval (in units of 0.625 ms). The default value corresponds to 2 seconds. */
#define APP_ADV_FAST_TIMEOUT            30                                          /**< The advertising time-out in units of seconds. */
#define APP_ADV_SLOW_TIMEOUT            86400                                       /**< The advertising time-out in units of seconds. */
#define ADV_INTERVAL_FAST_PERIOD        30                                          /**< The duration of the fast advertising period (in seconds). */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(100, UNIT_1_25_MS)            /**< Minimum acceptable connection interval (0.5 seconds). */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(200, UNIT_1_25_MS)            /**< Maximum acceptable connection interval (1 second). */
#define SLAVE_LATENCY                   4                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)  /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER) /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */
#define BATTERY_LEVEL_MEAS_INTERVAL     APP_TIMER_TICKS(60*60*1000, APP_TIMER_PRESCALER) /**< Battery level measurement interval (ticks). */
#define VBAT_MAX_IN_MV                  3300

#define SEC_PARAM_BOND                  1                                           /**< Perform bonding. */
#define SEC_PARAM_MITM                  0                                           /**< Man In The Middle protection not required. */
#define SEC_PARAM_IO_CAPABILITIES       BLE_GAP_IO_CAPS_NONE                        /**< No I/O capabilities. */
#define SEC_PARAM_OOB                   0                                           /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE          7                                           /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE          16                                          /**< Maximum encryption key size. */
#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define SCHED_MAX_EVENT_DATA_SIZE       sizeof(app_timer_event_t)                   /**< Maximum size of scheduler events. Note that scheduler BLE stack events do not contain any data, as the events are being pulled from the stack in the event handler. */
#define SCHED_QUEUE_SIZE                10                                          /**< Maximum number of events in the scheduler queue. */

#define DFU_APP_DATA_RESERVED           0x1000                                      /**< Preserve app data during DFU. 0x1000 is 4 pages (4k) */
#define DFU_REV_MAJOR                   0x00                                        /** DFU Major revision number to be exposed. */
#define DFU_REV_MINOR                   0x04                                        /** DFU Minor revision number to be exposed. */
#define DFU_REVISION                    ((DFU_REV_MAJOR << 8) | DFU_REV_MINOR)      /** DFU Revision number to be exposed. Combined of major and minor versions. */
#define APP_SERVICE_HANDLE_START        0x000C                                      /**< Handle of first application specific service when when service changed characteristic is present. */
#define BLE_HANDLE_MAX                  0xFFFF                                      /**< Max handle value in BLE. */

#define ADC_REF_VOLTAGE_IN_MILLIVOLTS        1200                                      /**< Reference voltage (in milli volts) used by ADC while doing conversion. */
#define ADC_PRE_SCALING_COMPENSATION         3                                         /**< The ADC is configured to use VDD with 1/3 prescaling as input. And hence the result of conversion is to be multiplied by 3 to get the actual value of the battery voltage.*/
#define DIODE_FWD_VOLT_DROP_MILLIVOLTS       270                                       /**< Typical forward voltage drop of the diode (Part no: SD103ATW-7-F) that is connected in series with the voltage supply. This is the voltage drop when the forward current is 1mA. Source: Data sheet of 'SURFACE MOUNT SCHOTTKY BARRIER DIODE ARRAY' available at www.diodes.com. */
#define DOUBLECLICK_DURATION                300                                     /**< Time in ms for double-click to register individual clicks */

// STATIC_ASSERT(IS_SRVC_CHANGED_CHARACT_PRESENT);                                     /** When having DFU Service support in application the Service Changed Characteristic should always be present. */


/**@brief Macro to convert the result of ADC conversion in millivolts.
 *
 * @param[in]  ADC_VALUE   ADC result.
 * @retval     Result converted to millivolts.
 */
#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE)\
        ((((ADC_VALUE) * ADC_REF_VOLTAGE_IN_MILLIVOLTS) / 255) * ADC_PRE_SCALING_COMPENSATION)

static ble_gap_sec_params_t             m_sec_params;                               /**< Security requirements for this application. */
static uint16_t                         m_conn_handle = BLE_CONN_HANDLE_INVALID;    /**< Handle of the current connection. */
static ble_buttonservice_t              m_buttonservice;                            /**< Struct for pressed and held buttons */
static dm_application_instance_t        m_app_handle;                               /**< Application identifier allocated by device manager. */
static ble_bas_t                        m_bas;                                      /**< Structure used to identify the battery service. */
static dm_handle_t                      m_bonded_peer_handle;                       /**< Device reference handle to the current bonded central. */
static ble_user_mem_block_t             m_mem_block;                                /**< Memory block structure, used during a BLE_EVT_USER_MEM_REQUEST event. */
static ble_gatts_rw_authorize_reply_params_t    m_rw_authorize_reply;               /**< Authorize reply structure, used during BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST event. */
static uint8_t                          m_nickname_storage[FIRMWARE_NICKNAME_MAX_LENGTH]; /**< Memory block for nickname */
static pstorage_handle_t                m_flash_handle;                             /**< Handle to pstorage */
static ble_dfu_t                        m_dfus;                                     /**< Structure used to identify the DFU service. */
static bsp_event_t                      m_last_press;                               /**< Most recent button to be pressed, for dblclick */
static uint32_t                         m_dblclick_timer_start;                     /**< RTC counter ticks, stored to be checked in timers */
APP_TIMER_DEF(m_battery_timer_id);
APP_TIMER_DEF(m_doubleclick_timer_id);

static void on_adv_evt(ble_adv_evt_t ble_adv_evt);
static void sleep_mode_enter(void);
static void ble_evt_dispatch(ble_evt_t * p_ble_evt);
static void on_ble_evt(ble_evt_t * p_ble_evt);
static void app_context_load(dm_handle_t const * p_handle);
static void dfu_event_handler(ble_dfu_t * p_dfu, ble_dfu_evt_t * p_evt);
static void dfu_error_handler(uint32_t nrf_error);
