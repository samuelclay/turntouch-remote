/* Copyright 2015 Turn Touch. All rights reserved.
 * 
 * Header for button status service for the Turn Touch Remote.
 * 
 */

#ifndef BLE_BUTTONSTATUS_H__
#define BLE_BUTTONSTATUS_H__

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"

#define BUTTONSERVICE_UUID_BASE {0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00}
#define BUTTONSERVICE_UUID_SERVICE                  0x1523
#define BUTTONSERVICE_UUID_BUTTONSTATUS_CHAR        0x1525
#define BUTTONSERVICE_UUID_FIRMWARE_NICKNAME_CHAR   0x1526

#define FIRMWARE_NICKNAME_MAX_LENGTH                32

// Forward declaration of the ble_buttonservice_t type. 
typedef struct ble_buttonservice_s ble_buttonservice_t;

typedef void (*ble_buttonservice_firmware_nickname_write_handler_t) (ble_buttonservice_t * p_buttonservice, uint8_t nickname);

typedef struct
{
    ble_buttonservice_firmware_nickname_write_handler_t firmware_nickname_write_handler;                    /**< Event handler to be called when Firmware Nickname characteristic is written. */
} ble_buttonstatus_init_t;

/**@brief Button Status Service structure. This contains various status information for the service. */
typedef struct ble_buttonservice_s
{
    uint16_t                    service_handle;
    ble_gatts_char_handles_t    firmware_nickname_char_handles;
    ble_gatts_char_handles_t    button_status_char_handles;
    uint8_t                     uuid_type;
    uint16_t                    conn_handle;
    ble_buttonservice_firmware_nickname_write_handler_t firmware_nickname_write_handler;
} ble_buttonservice_t;

/**@brief Function for initializing the Button Service.
 *
 * @param[out]  p_buttonservice       Button Service status structure. This structure will have to be supplied by
 *                          the application. It will be initialized by this function, and will later
 *                          be used to identify this particular service instance.
 * @param[in]   p_buttonservice_init  Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on successful initialization of service, otherwise an error code.
 */
uint32_t ble_buttonstatus_init(ble_buttonservice_t * p_buttonservice, const ble_buttonstatus_init_t * p_buttonservice_init);

/**@brief Function for handling the Application's BLE Stack events.
 *
 * @details Handles all events from the BLE stack of interest to the Button Service.
 *
 *
 * @param[in]   p_buttonservice      LED Button Service structure.
 * @param[in]   p_ble_evt  Event received from the BLE stack.
 */
void ble_buttonstatus_on_ble_evt(ble_buttonservice_t * p_buttonservice, ble_evt_t * p_ble_evt);

/**@brief Function for sending a button state notification.
 */
uint32_t ble_buttonstatus_on_button_change(ble_buttonservice_t * p_buttonservice, uint8_t button_state);

#endif // BLE_BUTTONSTATUS_H__

