/* Copyright 2015 Turn Touch. All rights reserved.
 * 
 * This is button status service for the Turn Touch Remote.
 * 
 * Service includes:
 *   - Characteristic for all four buttons' status (released, active, pressed, held)
 * 
 */

#include <string.h>
#include "nordic_common.h"
#include "ble_srv_common.h"
#include "button_status.h"
#include "rtt.h"
#include "app_error.h"



#define MEM_BLOCK_SIZE                  54                                          /**< Maximum length (an example) for write long, according to https://devzone.nordicsemi.com/documentation/nrf51/5.1.0/html/a01222.html */
static uint8_t                          m_mem_queue[MEM_BLOCK_SIZE];                /**< Memory block for p_mem_block */


void ExtractMemBlockData(ble_user_mem_block_t * p_mem_block,uint8_t* Buf,uint8_t *len);


// ====================
// = Bluetooth Events =
// ====================


/**@brief Function for handling the Connect event.
 *
 * @param[in]   p_buttonservice     Button Service status structure.
 * @param[in]   p_ble_evt           Event received from the BLE stack.
 */
static void on_connect(ble_buttonservice_t * p_buttonservice, ble_evt_t * p_ble_evt)
{
    rtt_print(0, "on_connect: %X\n", p_ble_evt->evt.gap_evt.conn_handle);
    p_buttonservice->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
}


/**@brief Function for handling the Disconnect event.
 *
 * @param[in]   p_buttonservice Button Service status structure.
 * @param[in]   p_ble_evt       Event received from the BLE stack.
 */
static void on_disconnect(ble_buttonservice_t * p_buttonservice, ble_evt_t * p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    p_buttonservice->conn_handle = BLE_CONN_HANDLE_INVALID;
}


/**@brief Function for handling the Write event.
 *
 * @param[in]   p_buttonservice Button Service status structure.
 * @param[in]   p_ble_evt       Event received from the BLE stack.
 */
static void on_write(ble_buttonservice_t * p_buttonservice, ble_evt_t * p_ble_evt, 
                     ble_user_mem_block_t * p_mem_block)
{
    ble_gatts_evt_write_t * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;
    rtt_print(0, "%son_write: %shandler=%X-%X (len:%d [4]:%X [8]:%X offset:%X) / %X/%X-%X\n", RTT_CTRL_TEXT_GREEN, RTT_CTRL_TEXT_BRIGHT_GREEN, p_evt_write->handle, *((uint16_t *)p_mem_block->p_mem), p_ble_evt->header.evt_len, *(&p_evt_write->data[4]), *(&p_evt_write->data[8]), p_evt_write->offset, p_buttonservice->button_status_char_handles.value_handle, p_buttonservice->firmware_nickname_char_handles.value_handle, p_buttonservice->firmware_nickname_char_handles.cccd_handle);
    
    if (p_evt_write->handle == p_buttonservice->button_status_char_handles.value_handle)
    {
        rtt_print(0, "on_write, button status");        
    }
    else if (p_evt_write->handle == p_buttonservice->firmware_nickname_char_handles.value_handle)
    {
    	rtt_print(0, "on_write short:\r");
		SEGGER_RTT_Write(0,(char *)p_evt_write->data,p_evt_write->len);
		SEGGER_RTT_printf(0, "\r");
		p_buttonservice->firmware_nickname_write_handler(p_buttonservice, p_evt_write->data,p_evt_write->len);
    }
    else if(p_evt_write->op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW)
    {
    	rtt_print(0, "on_write long:\r ");
        if(*((uint16_t *)p_mem_block->p_mem) == p_buttonservice->firmware_nickname_char_handles.value_handle)
        {
        	uint8_t buf[MEM_BLOCK_SIZE];
        	uint8_t length;
        	ExtractMemBlockData(p_mem_block,buf,&length);
        	p_buttonservice->firmware_nickname_write_handler(p_buttonservice, buf,length);
        }
    }
    /*else if (((p_evt_write->handle == p_buttonservice->firmware_nickname_char_handles.value_handle) ||
                (p_ble_evt->header.evt_id == BLE_EVT_USER_MEM_RELEASE && 
                 *((uint16_t *)p_mem_block->p_mem) == p_buttonservice->firmware_nickname_char_handles.value_handle)) &&
               (p_buttonservice->firmware_nickname_write_handler != NULL)) {
        rtt_print(0, "on_write, firmware: %X(%d) / %X\n", p_evt_write->handle, *p_evt_write->data,
                 p_buttonservice->firmware_nickname_char_handles.value_handle);
				rtt_print(0, "on_write, firmware: ");
				SEGGER_RTT_Write(0,p_evt_write->data,p_evt_write->len);
				SEGGER_RTT_printf(0, "\n");
        p_buttonservice->firmware_nickname_write_handler(p_buttonservice, p_evt_write->data);
    }*/
}

//
// mem block layout
// http://developer.nordicsemi.com/nRF51_SDK/nRF51_SDK_v7.x.x/doc/7.0.1/s110/html/a01075.html
//
void ExtractMemBlockData(ble_user_mem_block_t * p_mem_block,uint8_t* Buf,uint8_t *len)
{
	uint16_t Handle;
	uint16_t Offset;
	uint16_t Length;
	uint8_t *p;

	p=p_mem_block->p_mem;

	Handle = *(uint16_t*)p;
	*len=0;
	while(Handle !=BLE_GATT_HANDLE_INVALID)
	{
		Handle = *(uint16_t*)p;
		Offset = *(uint16_t*)(p+2);
		Length = *(uint16_t*)(p+4);
		p+=6;
		*len+=Length;
		for(int i=0;i<Length;i++)
		{
			Buf[Offset+i]=*p++;
		}
	}
}

void ble_buttonstatus_on_ble_evt(ble_buttonservice_t * p_buttonservice, ble_evt_t * p_ble_evt, 
                                 ble_user_mem_block_t * p_mem_block)
{
	uint32_t err_code;
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_buttonservice, p_ble_evt);
            break;
            
        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_buttonservice, p_ble_evt);
            break;
            
        case BLE_GATTS_EVT_WRITE:
            on_write(p_buttonservice, p_ble_evt, p_mem_block);
            break;
        
        case BLE_GAP_EVT_CONN_PARAM_UPDATE:
        case BLE_EVT_TX_COMPLETE:
            break;

        case BLE_EVT_USER_MEM_REQUEST:
        	rtt_print(0, "service -> BLE_EVT_USER_MEM_REQUEST\r");
        	p_mem_block->p_mem = &m_mem_queue[0];
			p_mem_block->len = MEM_BLOCK_SIZE;
			err_code = sd_ble_user_mem_reply(p_ble_evt->evt.gap_evt.conn_handle, p_mem_block);
			APP_ERROR_CHECK(err_code);
        	break;

        case BLE_EVT_USER_MEM_RELEASE:
        	rtt_print(0, "service -> BLE_EVT_USER_MEM_RELEASE\r");
            //on_write(p_buttonservice, p_ble_evt, p_mem_block);
            break;
            
        case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
            on_write(p_buttonservice, p_ble_evt, p_mem_block);
            break;
            
        default:
            // No implementation needed.
            rtt_print(0, "%sUnhandled ble buttonstatus ble event: %s%X\r", RTT_CTRL_TEXT_YELLOW, RTT_CTRL_TEXT_BRIGHT_BLUE, p_ble_evt->header.evt_id);
            break;
    }
}

// ===================
// = Characteristics =
// ===================

static uint32_t button_status_char_add(ble_buttonservice_t *p_buttonservice, const ble_buttonstatus_init_t *p_buttonservice_init) {
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
    
    memset(&cccd_md, 0, sizeof(cccd_md));
    
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;
    
    memset(&char_md, 0, sizeof(char_md));
    
    char_md.char_props.read   = 1;
    char_md.char_props.notify = 1;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = &cccd_md;
    char_md.p_sccd_md         = NULL;
    
    ble_uuid.type = p_buttonservice->uuid_type;
    ble_uuid.uuid = BUTTONSERVICE_UUID_BUTTONSTATUS_CHAR;
    
    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 0;
    
    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_len     = sizeof(uint16_t);
    attr_char_value.init_offs    = 0;
    attr_char_value.max_len      = sizeof(uint16_t);
    attr_char_value.p_value      = NULL;
    
    return sd_ble_gatts_characteristic_add(p_buttonservice->service_handle, &char_md,
                                           &attr_char_value,
                                           &p_buttonservice->button_status_char_handles);
}

static uint32_t firmware_nickname_char_add(ble_buttonservice_t * p_buttonservice, const ble_buttonstatus_init_t * p_buttonservice_init)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
        
    memset(&char_md, 0, sizeof(char_md));
    char_md.char_props.read   = 1;
    char_md.char_props.write  = 1;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = NULL;
    char_md.p_sccd_md         = NULL;
    
    ble_uuid.type = p_buttonservice->uuid_type;
    ble_uuid.uuid = BUTTONSERVICE_UUID_FIRMWARE_NICKNAME_CHAR;
    
    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 0;
    
    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_len     = sizeof(p_buttonservice->nickname_str);
    attr_char_value.init_offs    = 0;
    attr_char_value.max_len      = 32;
    attr_char_value.p_value      = (uint8_t *)p_buttonservice->nickname_str;

    rtt_print(0, "At firmware_nickname_char_add 0: %s/%X (%X)\n", attr_char_value.p_value, p_buttonservice->nickname_str, sizeof(p_buttonservice->nickname_str));
    
    return sd_ble_gatts_characteristic_add(p_buttonservice->service_handle, 
                                           &char_md,
                                           &attr_char_value,
                                           &p_buttonservice->firmware_nickname_char_handles);
}

uint32_t ble_buttonstatus_init(ble_buttonservice_t * p_buttonservice, 
                               const ble_buttonstatus_init_t * p_buttonservice_init) {
    uint32_t err_code;
    ble_uuid_t ble_uuid;
    
    // Initialize service structure
    p_buttonservice->conn_handle = BLE_CONN_HANDLE_INVALID;
    p_buttonservice->firmware_nickname_write_handler = p_buttonservice_init->firmware_nickname_write_handler;
    memcpy(p_buttonservice->nickname_str, p_buttonservice_init->nickname_str, 32);
    
    // Add service
    ble_uuid128_t base_uuid = {BUTTONSERVICE_UUID_BASE};
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_buttonservice->uuid_type);
    if (err_code != NRF_SUCCESS) {
        return err_code;
    }
    
    ble_uuid.type = p_buttonservice->uuid_type;
    ble_uuid.uuid = BUTTONSERVICE_UUID_SERVICE;
    
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_buttonservice->service_handle);
    if (err_code != NRF_SUCCESS) {
        return err_code;
    }
    
    err_code = button_status_char_add(p_buttonservice, p_buttonservice_init);
    if (err_code != NRF_SUCCESS) {
        return err_code;
    }

    err_code = firmware_nickname_char_add(p_buttonservice, p_buttonservice_init);
    if (err_code != NRF_SUCCESS) {
        return err_code;
    }
    
    return NRF_SUCCESS;
}

uint32_t ble_buttonstatus_on_button_change(ble_buttonservice_t * p_buttonservice, uint8_t button_state[]) {
    ble_gatts_hvx_params_t params;
    uint16_t len = BUTTON_STATUS_PACKET_BYTES * sizeof(uint8_t);
    
    memset(&params, 0, sizeof(params));
    params.type = BLE_GATT_HVX_NOTIFICATION;
    params.handle = p_buttonservice->button_status_char_handles.value_handle;
    params.p_data = button_state;
    params.p_len = &len;

    rtt_print(0, "%sButton change: %s%X%X (%X / %X-%d)%s\n", RTT_CTRL_TEXT_YELLOW, RTT_CTRL_TEXT_BRIGHT_YELLOW, button_state[0], button_state[1], p_buttonservice->conn_handle, *params.p_data, len, RTT_CTRL_RESET);
    
    return sd_ble_gatts_hvx(p_buttonservice->conn_handle, &params);
}
