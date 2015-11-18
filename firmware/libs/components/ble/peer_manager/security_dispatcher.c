/* Copyright (C) 2015 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */


#include "security_dispatcher.h"

#include <string.h>
#include "ble.h"
#include "ble_gap.h"
#include "ble_conn_state.h"
#include "peer_manager_types.h"
#include "peer_database.h"
#include "id_manager.h"


#define MAX_REGISTRANTS 3                               /**< The number of user that can register with the module. */

#define MODULE_INITIALIZED (m_smd.n_registrants > 0)    /**< Expression which is true when the module is initialized. */

/**@brief Macro for verifying that the module is initialized. It will cause the function to return
 *        @ref NRF_ERROR_INVALID_STATE if not.
 */
#define VERIFY_MODULE_INITIALIZED()     \
do                                      \
{                                       \
    if (!MODULE_INITIALIZED)            \
    {                                   \
        return NRF_ERROR_INVALID_STATE; \
    }                                   \
} while(0)


/**@brief Macro for verifying that the module is initialized. It will cause the function to return
 *        if not.
 *
 * @param[in] param  The variable to check if is NULL.
 */
#define VERIFY_PARAM_NOT_NULL(param)    \
do                                      \
{                                       \
    if (param == NULL)                  \
    {                                   \
        return NRF_ERROR_NULL;          \
    }                                   \
} while(0)


typedef struct
{
    smd_evt_handler_t evt_handlers[MAX_REGISTRANTS];
    uint8_t           n_registrants;
    pm_peer_id_t      pending_bond_central;
    pm_peer_id_t      pending_bond_peripheral;
} smd_t;

static smd_t m_smd;

static void internal_state_reset(smd_t * smd)
{
    memset(smd, 0, sizeof(smd_t));
    smd->pending_bond_central    = PM_PEER_ID_INVALID;
    smd->pending_bond_peripheral = PM_PEER_ID_INVALID;
}


/**@brief Function for storing the fact that a new bond is being made.
 *
 * @details This specifies that there is no bonding data yet stored for this peer. The role is
 *          needed because the SoftDevice supports two simultaneous bonding procedures, one per role.
 *
 * @param[in]  role     The role of the local device in the bonding procedure.
 * @param[in]  peer_id  ID of the device being bonded with.
 */
static void pending_bond_push(uint8_t role, pm_peer_id_t peer_id)
{
    if (role == BLE_GAP_ROLE_CENTRAL)
    {
        m_smd.pending_bond_central = peer_id;
    }
    else
    {
        m_smd.pending_bond_peripheral = peer_id;
    }
}


/**@brief Function for reading and clearing the record of whether a new bond is being made.
 *
 * @details If the bonding fails, and this function returns true this peer ID must be freed. The
 *          role is needed because the SoftDevice supports two simultaneous bonding procedures, one
 *          per role.
 *
 * @param[in]  role     The role of the local device in the bonding procedure.
 * @param[in]  peer_id  ID of the device being bonded with.
 *
 * @return     Whether a new bond was/is being made. I.E. whether @ref pending_bond_push has been
 *             called with the same parameters.
 */
static bool pending_bond_pop(uint8_t role, pm_peer_id_t peer_id)
{
    if (peer_id == PM_PEER_ID_INVALID)
    {
        return false;
    }

    if ((role == BLE_GAP_ROLE_CENTRAL) && (m_smd.pending_bond_central == peer_id))
    {
        m_smd.pending_bond_central = PM_PEER_ID_INVALID;
        return true;
    }
    else if ((role == BLE_GAP_ROLE_PERIPH) && (m_smd.pending_bond_peripheral == peer_id))
    {
        m_smd.pending_bond_peripheral = PM_PEER_ID_INVALID;
        return true;
    }
    else
    {
        return false;
    }
}


static void evt_send(smd_evt_t * p_event)
{
    for (int i = 0; i < m_smd.n_registrants; i++)
    {
        m_smd.evt_handlers[i](p_event);
    }
}


static void sec_start_send(uint16_t conn_handle, pm_sec_procedure_t procedure)
{
    smd_evt_t evt =
    {
        .evt_id = SMD_EVT_SEC_PROCEDURE_START,
        .conn_handle = conn_handle,
        .params = {.sec_procedure_start = {.procedure = procedure}}
    };
    evt_send(&evt);
}


/**@brief Event handler for events from the peer_database module.
 *
 * @param[in]  p_event   The event that has happened.
 */
static void pdb_evt_handler(pdb_evt_t const * p_event)
{
    if ((p_event->evt_id == PDB_EVT_WRITE_BUF_STORED) && (p_event->data_id == PM_PEER_DATA_ID_BONDING))
    {
        smd_evt_t evt =
        {
            .evt_id = SMD_EVT_BONDING_INFO_STORED,
            .conn_handle = im_conn_handle_get(p_event->peer_id),
            .params = {.bonding_info_stored =
            {
                .peer_id = p_event->peer_id,
            }}
        };
        evt_send(&evt);
    }
}


/**@brief Function for processing the @ref BLE_GAP_EVT_SEC_PARAMS_REQUEST event from the SoftDevice.
 *
 * @param[in]  p_gap_evt  The event from the SoftDevice.
 */
static void sec_params_request_process(ble_gap_evt_t * p_gap_evt)
{
    smd_evt_t evt =
    {
        .evt_id = SMD_EVT_PARAMS_REQ,
        .conn_handle = p_gap_evt->conn_handle
    };
    evt_send(&evt);
    return;
}


/**@brief Function for processing the @ref BLE_GAP_EVT_SEC_INFO_REQUEST event from the SoftDevice.
 *
 * @param[in]  p_gap_evt  The event from the SoftDevice.
 */
static void sec_info_request_process(ble_gap_evt_t * p_gap_evt)
{
    ret_code_t                 err_code;
    ble_gap_enc_info_t const * p_enc_info = NULL;
    pm_peer_data_flash_t       peer_data;
    pm_peer_id_t               peer_id = im_peer_id_get_by_master_id(&p_gap_evt->params.sec_info_request.master_id);
    smd_evt_t evt;

    evt.conn_handle = p_gap_evt->conn_handle;

    if (peer_id != PM_PEER_ID_INVALID)
    {
        err_code = pdb_read_buf_get(peer_id, PM_PEER_DATA_ID_BONDING, &peer_data, NULL);

        if (err_code == NRF_SUCCESS)
        {
            // There is stored bonding data for this peer.
            ble_gap_enc_key_t const * p_existing_key = &peer_data.data.p_bonding_data->own_ltk;

            if (peer_data.data.p_bonding_data->own_role == BLE_GAP_ROLE_CENTRAL)
            {
                // Role has changed since bonding.
                p_existing_key = &peer_data.data.p_bonding_data->peer_ltk;
            }

            if (im_master_id_is_valid(&p_existing_key->master_id))
            {
                p_enc_info = &p_existing_key->enc_info;
            }
        }
    }

    // All return values from the following can be safely ignored.
    err_code = sd_ble_gap_sec_info_reply(p_gap_evt->conn_handle, p_enc_info, NULL, NULL);

    if (err_code != NRF_SUCCESS)
    {
        evt.evt_id                        = SMD_EVT_ERROR_UNEXPECTED;
        evt.params.error_unexpected.error = err_code;

        evt_send(&evt);
    }
    else if (p_enc_info == NULL)
    {
        evt.evt_id                                  = SMD_EVT_LINK_ENCRYPTION_FAILED;
        evt.params.link_encryption_failed.error     = PM_SEC_ERROR_CODE_PIN_OR_KEY_MISSING;
        evt.params.link_encryption_failed.error_src = BLE_GAP_SEC_STATUS_SOURCE_LOCAL;

        evt_send(&evt);
    }
    else
    {
        sec_start_send(p_gap_evt->conn_handle, PM_LINK_SECURED_PROCEDURE_ENCRYPTION);
    }
    return;
}


/**@brief Function for processing the @ref BLE_GAP_EVT_SEC_REQUEST event from the SoftDevice.
 *
 * @param[in]  p_gap_evt  The event from the SoftDevice.
 */
static void sec_request_process(ble_gap_evt_t * p_gap_evt)
{
    smd_evt_t evt =
    {
        .evt_id = SMD_EVT_SLAVE_SECURITY_REQ,
        .conn_handle = p_gap_evt->conn_handle,
        .params =
        {
            .slave_security_req =
            {
                .bond = p_gap_evt->params.sec_request.bond,
                .mitm = p_gap_evt->params.sec_request.mitm,
            }
        }
    };
    evt_send(&evt);
    return;
}


/**@brief Function for processing the @ref BLE_GAP_EVT_AUTH_STATUS event from the SoftDevice, when
 *        the auth_status is success.
 *
 * @param[in]  p_gap_evt  The event from the SoftDevice.
 */
static void auth_status_success_process(ble_gap_evt_t * p_gap_evt)
{
    ret_code_t           err_code      = NRF_SUCCESS;
    uint8_t              role          = ble_conn_state_role(p_gap_evt->conn_handle);
    pm_peer_id_t         peer_id       = im_peer_id_get_by_conn_handle(p_gap_evt->conn_handle);
    ble_gap_sec_kdist_t  kdist_periph  = p_gap_evt->params.auth_status.kdist_periph;
    ble_gap_sec_kdist_t  kdist_central = p_gap_evt->params.auth_status.kdist_central;
    ble_gap_sec_kdist_t  kdist_own     = (role == BLE_GAP_ROLE_CENTRAL) ?  kdist_central : kdist_periph;
    ble_gap_sec_kdist_t  kdist_peer    = (role == BLE_GAP_ROLE_CENTRAL) ?  kdist_periph  : kdist_central;

    if (role == BLE_GAP_ROLE_INVALID)
    {
        /* Unlikely, but maybe possible? */
        return;
    }

    pending_bond_pop(role, peer_id);

    err_code = pdb_write_buf_store(peer_id, PM_PEER_DATA_ID_BONDING);
    if (err_code != NRF_SUCCESS)
    {
        /* Unexpected */
        smd_evt_t error_evt =
        {
            .evt_id      = SMD_EVT_ERROR_BONDING_INFO,
            .conn_handle = p_gap_evt->conn_handle,
            .params      =
            {
                .error_bonding_info =
                {
                    .peer_id        = peer_id,
                    .error          = err_code,
                }
            }
        };
        evt_send(&error_evt);
    }

    smd_evt_t pairing_success_evt =
    {
        .evt_id      = SMD_EVT_PAIRING_SUCCESS,
        .conn_handle = p_gap_evt->conn_handle,
        .params      =
        {
            .pairing_success =
            {
                .bonded        = p_gap_evt->params.auth_status.bonded,
                .mitm          = p_gap_evt->params.auth_status.sm1_levels.lv3,
                .kdist_own     = kdist_own,
                .kdist_peer    = kdist_peer,
            }
        }
    };
    evt_send(&pairing_success_evt);
    return;
}


/**@brief Function for cleaning up after a failed pairing procedure.
 *
 * @param[in]  role     Our role in the pairing procedure. See @ref BLE_GAP_ROLES.
 * @param[in]  peer_id  The peer id used in the pairing procedure.
 */
static void pairing_failure(uint8_t role, pm_peer_id_t peer_id)
{
    if(pending_bond_pop(role, peer_id))
    {
        // The peer_id was created during the procedure, and should be freed, because no data is stored under it.
        pdb_peer_free(peer_id);
    }
    else
    {
        pdb_write_buf_release(peer_id, PM_PEER_DATA_ID_BONDING);
    }

    return;
}


/**@brief Function for processing the @ref BLE_GAP_EVT_DISCONNECT event from the SoftDevice.
 *
 * @param[in]  p_gap_evt  The event from the SoftDevice.
 */
static void disconnect_process(ble_gap_evt_t * p_gap_evt)
{
    uint8_t       role    = ble_conn_state_role(p_gap_evt->conn_handle);
    pm_peer_id_t  peer_id = im_peer_id_get_by_conn_handle(p_gap_evt->conn_handle);

    if (   (peer_id != PM_PEER_ID_INVALID)
        && (p_gap_evt->params.disconnected.reason == BLE_HCI_CONN_TERMINATED_DUE_TO_MIC_FAILURE)
        && (pending_bond_pop(role, peer_id)))
    {
//        smd_evt_t     evt     =
//        {
//            .evt_id = SMD_EVT_PAIRING_FAIL,
//            .conn_handle = p_gap_evt->conn_handle,
//            .params =
//            {
//                .pairing_failed =
//                {
//                    .auth_status = ?
//                    .error_src   = ?
//                }
//            }
//        };

        pairing_failure(role, peer_id);

//        evt_send(&evt);
    }
}



/**@brief Function for processing the @ref BLE_GAP_EVT_AUTH_STATUS event from the SoftDevice, when
 *        the auth_status is failure.
 *
 * @param[in]  p_gap_evt  The event from the SoftDevice.
 */
static void auth_status_failure_process(ble_gap_evt_t * p_gap_evt)
{
    uint8_t       role    = ble_conn_state_role(p_gap_evt->conn_handle);
    pm_peer_id_t  peer_id = im_peer_id_get_by_conn_handle(p_gap_evt->conn_handle);
    smd_evt_t     evt     =
    {
        .evt_id = SMD_EVT_PAIRING_FAIL,
        .conn_handle = p_gap_evt->conn_handle,
        .params =
        {
            .pairing_failed =
            {
                .auth_status = p_gap_evt->params.auth_status.auth_status,
                .error_src   = p_gap_evt->params.auth_status.error_src,
            }
        }
    };

    pairing_failure(role, peer_id);

    evt_send(&evt);
}


/**@brief Function for processing the @ref BLE_GAP_EVT_AUTH_STATUS event from the SoftDevice.
 *
 * @param[in]  p_gap_evt  The event from the SoftDevice.
 */
static void auth_status_process(ble_gap_evt_t * p_gap_evt)
{
    switch(p_gap_evt->params.auth_status.auth_status)
    {
        case BLE_GAP_SEC_STATUS_SUCCESS:
            auth_status_success_process(p_gap_evt);
            break;

        case BLE_GAP_SEC_STATUS_PDU_INVALID:
            /* Ignore, because they don't have anything to do with an ongoing procedure. */
            break;

        default:
            auth_status_failure_process(p_gap_evt);
            break;
    }
}


/**@brief Function for processing the @ref BLE_GAP_EVT_CONN_SEC_UPDATE event from the SoftDevice.
 *
 * @param[in]  p_gap_evt  The event from the SoftDevice.
 */
static void conn_sec_update_process(ble_gap_evt_t * p_gap_evt)
{
    smd_evt_t evt;

    evt.conn_handle = p_gap_evt->conn_handle;

    if (ble_conn_state_encrypted(p_gap_evt->conn_handle))
    {
        evt.evt_id = SMD_EVT_LINK_ENCRYPTION_UPDATE;
        evt.params.link_encryption_update.mitm_protected
                                = ble_conn_state_mitm_protected(p_gap_evt->conn_handle);
    }
    else
    {
        evt.evt_id                                  = SMD_EVT_LINK_ENCRYPTION_FAILED;
        evt.params.link_encryption_failed.error     = PM_SEC_ERROR_CODE_PIN_OR_KEY_MISSING;
        evt.params.link_encryption_failed.error_src = BLE_GAP_SEC_STATUS_SOURCE_REMOTE;
    }

    evt_send(&evt);
}


ret_code_t smd_register(smd_evt_handler_t evt_handler)
{
    ret_code_t err_code = NRF_SUCCESS;
    if (evt_handler == NULL)
    {
        err_code = NRF_ERROR_NULL;
    }
    else
    {
        if (!MODULE_INITIALIZED)
        {
            err_code = pdb_register(pdb_evt_handler);
            if (err_code == NRF_SUCCESS)
            {
                internal_state_reset(&m_smd);
            }
        }
        if ((err_code == NRF_SUCCESS))
        {
            if ((m_smd.n_registrants < MAX_REGISTRANTS))
            {
                m_smd.evt_handlers[m_smd.n_registrants++] = evt_handler;
            }
            else
            {
                err_code = NRF_ERROR_NO_MEM;
            }
        }
    }
    return err_code;
}


ret_code_t smd_params_reply(uint16_t conn_handle, ble_gap_sec_params_t * p_sec_params)
{
    VERIFY_MODULE_INITIALIZED();

    uint8_t              role = ble_conn_state_role(conn_handle);
    pm_peer_id_t         peer_id = PM_PEER_ID_INVALID;
    ret_code_t           err_code = NRF_SUCCESS;
    uint8_t              sec_status = BLE_GAP_SEC_STATUS_SUCCESS;
    ble_gap_sec_keyset_t sec_keyset;

    memset(&sec_keyset, 0, sizeof(ble_gap_sec_keyset_t));

    if (role == BLE_GAP_ROLE_INVALID)
    {
        return BLE_ERROR_INVALID_CONN_HANDLE;
    }

    if (p_sec_params == NULL)
    {
        // NULL params means reject pairing.
        sec_status = BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP;
    }
    else if (p_sec_params->bond)
    {
        // Bonding is to be performed, prepare to receive bonding data.
        pm_peer_data_t       peer_data;

        peer_id = im_peer_id_get_by_conn_handle(conn_handle);

        if (peer_id == PM_PEER_ID_INVALID)
        {
            // Peer is unknown to us, allocate a new peer ID for it.
            peer_id = pdb_peer_allocate();
            if (peer_id != PM_PEER_ID_INVALID)
            {
                pending_bond_push(role, peer_id);
            }
            else
            {
                err_code = NRF_ERROR_INTERNAL;
            }
        }

        if (err_code == NRF_SUCCESS)
        {
            // Peer ID is OK, retrieve memory buffer to receive data.
            err_code = pdb_write_buf_get(peer_id, PM_PEER_DATA_ID_BONDING, 1, &peer_data);
            if (err_code == NRF_SUCCESS)
            {
                peer_data.data.p_bonding_data->own_role = role;

                if (role == BLE_GAP_ROLE_CENTRAL)
                {
                    sec_keyset.keys_central.p_enc_key = &peer_data.data.p_bonding_data->own_ltk;
                    sec_keyset.keys_periph.p_enc_key  = &peer_data.data.p_bonding_data->peer_ltk;
                    sec_keyset.keys_periph.p_id_key   = &peer_data.data.p_bonding_data->peer_id;
                    im_ble_addr_get(conn_handle, &peer_data.data.p_bonding_data->peer_id.id_addr_info); // Will be overwritten if ID is shared.
                }
                else
                {
                    sec_keyset.keys_periph.p_enc_key  = &peer_data.data.p_bonding_data->own_ltk;
                    sec_keyset.keys_central.p_enc_key = &peer_data.data.p_bonding_data->peer_ltk;
                    sec_keyset.keys_central.p_id_key  = &peer_data.data.p_bonding_data->peer_id;
                    im_ble_addr_get(conn_handle, &peer_data.data.p_bonding_data->peer_id.id_addr_info); // Will be overwritten if ID is shared.
                }

                // Buffer is OK, reserve room in flash for the data.
                err_code = pdb_write_buf_store_prepare(peer_id, PM_PEER_DATA_ID_BONDING);
            }
        }
    }
    else
    {
        // Pairing only, no action needed.
    }

    if (err_code == NRF_SUCCESS)
    {
        // Everything OK, reply to SoftDevice. If an error happened, the user is given an
        // opportunity to change the parameters and retry the call.
        if (role == BLE_GAP_ROLE_CENTRAL)
        {
            err_code = sd_ble_gap_sec_params_reply(conn_handle, sec_status, NULL, &sec_keyset);
        }
        else
        {
            err_code = sd_ble_gap_sec_params_reply(conn_handle, sec_status, p_sec_params, &sec_keyset);

            if ((p_sec_params != NULL) && (err_code == NRF_SUCCESS))
            {
                pm_sec_procedure_t procedure = p_sec_params->bond
                                               ? PM_LINK_SECURED_PROCEDURE_BONDING
                                               : PM_LINK_SECURED_PROCEDURE_PAIRING;
                sec_start_send(conn_handle, procedure);
            }
        }
    }

    if(pending_bond_pop(role, peer_id))
    {
        // New peer ID created, either report it or free it.
        if (err_code == NRF_SUCCESS)
        {
            pending_bond_push(role, peer_id);
            im_new_peer_id(conn_handle, peer_id);
        }
        else
        {
            pdb_peer_free(peer_id);
        }
    }
    return err_code;
}


static ret_code_t link_secure_central_existing_peer(uint16_t               conn_handle,
                                                    ble_gap_sec_params_t * p_sec_params,
                                                    bool                   force_repairing,
                                                    pm_peer_id_t           peer_id,
                                                    pm_sec_procedure_t   * procedure)
{
    pm_peer_data_flash_t      peer_data;
    pm_peer_data_t            dummy_peer_data;
    ret_code_t                err_code;
    ble_gap_enc_key_t const * p_existing_key = NULL;


    err_code = pdb_read_buf_get(peer_id, PM_PEER_DATA_ID_BONDING, &peer_data, NULL);

    if (err_code == NRF_SUCCESS)
    {
        p_existing_key = &(peer_data.data.p_bonding_data->peer_ltk);
        if (peer_data.data.p_bonding_data->own_role == BLE_GAP_ROLE_PERIPH)
        {
            // Role has changed since bonding.
            p_existing_key = &(peer_data.data.p_bonding_data->own_ltk);
        }
    }

    if     (!force_repairing
         && (err_code == NRF_SUCCESS)
         && (p_existing_key != NULL)
         && (im_master_id_is_valid(&(p_existing_key->master_id))) /* There is an existing LTK stored. */
         && (p_existing_key->enc_info.auth >= p_sec_params->mitm)) /* The requested security is at or below the existing level. */
    {
        err_code = sd_ble_gap_encrypt(conn_handle, &(p_existing_key->master_id), &(p_existing_key->enc_info));

        *procedure = PM_LINK_SECURED_PROCEDURE_ENCRYPTION;
    }
    else if ((err_code == NRF_SUCCESS) || (err_code == NRF_ERROR_NOT_FOUND))
    {
        /* Re-pairing is needed, because there is no LTK available or the existing key is not
           secure enough */
        err_code = NRF_SUCCESS;

        if (p_sec_params->bond)
        {
            err_code = pdb_write_buf_get(peer_id, PM_PEER_DATA_ID_BONDING, 1, &dummy_peer_data);
            if (err_code == NRF_SUCCESS)
            {
                err_code = pdb_write_buf_store_prepare(peer_id, PM_PEER_DATA_ID_BONDING);
            }
        }

        if (err_code == NRF_SUCCESS)
        {
            err_code = sd_ble_gap_authenticate(conn_handle, p_sec_params);
        }

        if (err_code != NRF_SUCCESS)
        {
            ret_code_t err_code_release = pdb_write_buf_release(peer_id, PM_PEER_DATA_ID_BONDING);
            if ((err_code_release != NRF_SUCCESS) && (err_code_release != NRF_ERROR_NOT_FOUND))
            {
                err_code = NRF_ERROR_INTERNAL;
            }
        }
    }

    return err_code;
}


static ret_code_t link_secure_central_new_peer(uint16_t               conn_handle,
                                               ble_gap_sec_params_t * p_sec_params)
{
    pm_peer_id_t    peer_id = pdb_peer_allocate();
    pm_peer_data_t  dummy_peer_data;
    ret_code_t      err_code;

    if (peer_id != PM_PEER_ID_INVALID)
    {
        err_code = pdb_write_buf_get(peer_id, PM_PEER_DATA_ID_BONDING, 1, &dummy_peer_data);
        if (err_code == NRF_SUCCESS)
        {
            err_code = pdb_write_buf_store_prepare(peer_id, PM_PEER_DATA_ID_BONDING);
        }

        if (err_code == NRF_SUCCESS)
        {
            err_code = sd_ble_gap_authenticate(conn_handle, p_sec_params);
        }

        if (err_code != NRF_SUCCESS)
        {
            ret_code_t err_code_release = pdb_write_buf_release(peer_id, PM_PEER_DATA_ID_BONDING);
            if ((err_code_release != NRF_SUCCESS) && (err_code_release != NRF_ERROR_NOT_FOUND))
            {
                err_code = NRF_ERROR_INTERNAL;
            }
            pdb_peer_free(peer_id);
        }
        else
        {
            pending_bond_push(BLE_GAP_ROLE_CENTRAL, peer_id);
            im_new_peer_id(conn_handle, peer_id);
        }
    }
    else
    {
        err_code = NRF_ERROR_INTERNAL;
    }

    return err_code;
}


static ret_code_t link_secure_central(uint16_t               conn_handle,
                                      ble_gap_sec_params_t * p_sec_params,
                                      bool                   force_repairing)
{
    ret_code_t           err_code;
    pm_peer_id_t         peer_id;

    if (p_sec_params == NULL)
    {
        return sd_ble_gap_authenticate(conn_handle, NULL);
    }

    pm_sec_procedure_t procedure = p_sec_params->bond ? PM_LINK_SECURED_PROCEDURE_BONDING
                                                      : PM_LINK_SECURED_PROCEDURE_PAIRING;

    peer_id = im_peer_id_get_by_conn_handle(conn_handle);

    if (peer_id != PM_PEER_ID_INVALID)
    {
        // There is already data in flash for this peer.
        err_code = link_secure_central_existing_peer(conn_handle,
                                                     p_sec_params,
                                                     force_repairing,
                                                     peer_id,
                                                     &procedure);
    }
    else if (p_sec_params->bond)
    {
        // New peer is required.
        err_code = link_secure_central_new_peer(conn_handle, p_sec_params);
    }
    else
    {
        err_code = sd_ble_gap_authenticate(conn_handle, p_sec_params);
    }

    if (err_code == NRF_SUCCESS)
    {
        sec_start_send(conn_handle, procedure);
    }

    return err_code;
}


ret_code_t smd_link_secure(uint16_t               conn_handle,
                           ble_gap_sec_params_t * p_sec_params,
                           bool                   force_repairing)
{
    VERIFY_MODULE_INITIALIZED();

    uint8_t role = ble_conn_state_role(conn_handle);

    switch (role)
    {
        case BLE_GAP_ROLE_CENTRAL:
            return link_secure_central(conn_handle,
                                       p_sec_params,
                                       force_repairing);

        case BLE_GAP_ROLE_PERIPH:
            VERIFY_PARAM_NOT_NULL(p_sec_params);
            return sd_ble_gap_authenticate(conn_handle, p_sec_params);

        default:
            return BLE_ERROR_INVALID_CONN_HANDLE;
    }
}


void smd_ble_evt_handler(ble_evt_t * p_ble_evt)
{
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_DISCONNECTED:
            disconnect_process(&(p_ble_evt->evt.gap_evt));
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            sec_params_request_process(&(p_ble_evt->evt.gap_evt));
            break;

        case BLE_GAP_EVT_SEC_INFO_REQUEST:
            sec_info_request_process(&(p_ble_evt->evt.gap_evt));
            break;

        case BLE_GAP_EVT_SEC_REQUEST:
            sec_request_process(&(p_ble_evt->evt.gap_evt));
            break;

        case BLE_GAP_EVT_AUTH_STATUS:
            auth_status_process(&(p_ble_evt->evt.gap_evt));
            break;

        case BLE_GAP_EVT_CONN_SEC_UPDATE:
            conn_sec_update_process(&(p_ble_evt->evt.gap_evt));
            break;
    };
}

