/**	
 * \file            gsm_network.c
 * \brief           Network API
 */

/*
 * Copyright (c) 2018 Tilen Majerle
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file is part of GSM-AT library.
 *
 * Author:          Tilen MAJERLE <tilen@majerle.eu>
 */
#include "gsm/gsm_private.h"
#include "gsm/gsm_network.h"
#include "gsm/gsm_mem.h"

#if GSM_CFG_NETWORK || __DOXYGEN__

/**
 * \brief           Attach to network and active PDP context
 * \param[in]       apn: APN name
 * \param[in]       user: User name to attach. Set to `NULL` if not used
 * \param[in]       pass: User password to attach. Set to `NULL` if not used
 * \param[in]       evt_fn: Callback function called when command has finished. Set to `NULL` when not used
 * \param[in]       evt_arg: Custom argument for event callback function
 * \param[in]       blocking: Status whether command should be blocking or not
 * \return          \ref gsmOK on success, member of \ref gsmr_t enumeration otherwise
 */
gsmr_t
gsm_network_attach(const char* apn, const char* user, const char* pass,
                    const gsm_api_cmd_evt_fn evt_fn, void* const evt_arg, const uint32_t blocking) {
    GSM_MSG_VAR_DEFINE(msg);

    GSM_MSG_VAR_ALLOC(msg);
    GSM_MSG_VAR_SET_EVT(msg);
    GSM_MSG_VAR_REF(msg).cmd_def = GSM_CMD_NETWORK_ATTACH;
#if GSM_CFG_CONN
    GSM_MSG_VAR_REF(msg).cmd = GSM_CMD_CIPSTATUS;
#endif /* GSM_CFG_CONN */
    GSM_MSG_VAR_REF(msg).msg.network_attach.apn = apn;
    GSM_MSG_VAR_REF(msg).msg.network_attach.user = user;
    GSM_MSG_VAR_REF(msg).msg.network_attach.pass = pass;

    return gsmi_send_msg_to_producer_mbox(&GSM_MSG_VAR_REF(msg), gsmi_initiate_cmd, 200000);
}

/**
 * \brief           Detach from network
 * \param[in]       evt_fn: Callback function called when command has finished. Set to `NULL` when not used
 * \param[in]       evt_arg: Custom argument for event callback function
 * \param[in]       blocking: Status whether command should be blocking or not
 * \return          \ref gsmOK on success, member of \ref gsmr_t enumeration otherwise
 */
gsmr_t
gsm_network_detach(const gsm_api_cmd_evt_fn evt_fn, void* const evt_arg, const uint32_t blocking) {
    GSM_MSG_VAR_DEFINE(msg);

    GSM_MSG_VAR_ALLOC(msg);
    GSM_MSG_VAR_SET_EVT(msg);
    GSM_MSG_VAR_REF(msg).cmd_def = GSM_CMD_NETWORK_DETACH;
#if GSM_CFG_CONN
    /* GSM_MSG_VAR_REF(msg).cmd = GSM_CMD_CIPSTATUS; */
#endif /* GSM_CFG_CONN */

    return gsmi_send_msg_to_producer_mbox(&GSM_MSG_VAR_REF(msg), gsmi_initiate_cmd, 60000);
}

/**
 * \brief           Check network PDP status
 * \param[in]       evt_fn: Callback function called when command has finished. Set to `NULL` when not used
 * \param[in]       evt_arg: Custom argument for event callback function
 * \param[in]       blocking: Status whether command should be blocking or not
 * \return          \ref gsmOK on success, member of \ref gsmr_t enumeration otherwise
 */
gsmr_t
gsm_network_check_status(const gsm_api_cmd_evt_fn evt_fn, void* const evt_arg, const uint32_t blocking) {
    GSM_MSG_VAR_DEFINE(msg);

    GSM_MSG_VAR_ALLOC(msg);
    GSM_MSG_VAR_SET_EVT(msg);
    GSM_MSG_VAR_REF(msg).cmd_def = GSM_CMD_CIPSTATUS;

    return gsmi_send_msg_to_producer_mbox(&GSM_MSG_VAR_REF(msg), gsmi_initiate_cmd, 60000);
}

/**
 * \brief           Copy IP address from internal value to user variable
 * \param[out]      ip: Pointer to output IP variable
 * \return          \ref gsmOK on success, member of \ref gsmr_t enumeration otherwise
 */
gsmr_t
gsm_network_copy_ip(gsm_ip_t* ip) {
    if (gsm_network_is_attached()) {
        gsm_core_lock();
        GSM_MEMCPY(ip, &gsm.m.network.ip_addr, sizeof(*ip));
        gsm_core_unlock();
        return gsmOK;
    }
    return gsmERR;
}

/**
 * \brief           Check if device is attached to network and PDP context is active
 * \return          `1` on success, `0` otherwise
 */
uint8_t
gsm_network_is_attached(void) {
    uint8_t res;
    gsm_core_lock();
    res = GSM_U8(gsm.m.network.is_attached);
    gsm_core_unlock();
    return res;
}

/**
 * \brief           Set PDP context
 * \param[in]       pdp_type: "IP" string
 * \param[in]       apn: APN name
 * \param[in]       evt_fn: Callback function called when command has finished. Set to `NULL` when not used
 * \param[in]       evt_arg: Custom argument for event callback function
 * \param[in]       blocking: Status whether command should be blocking or not
 * \return          \ref gsmOK on success, member of \ref gsmr_t enumeration otherwise
 */
gsmr_t
gsm_network_pdp(const char* pdp_type, const char* apn, const gsm_api_cmd_evt_fn evt_fn,
        void* const evt_arg, const uint32_t blocking) {
    GSM_MSG_VAR_DEFINE(msg);

    GSM_MSG_VAR_ALLOC(msg);
    GSM_MSG_VAR_SET_EVT(msg);
#if GSM_CFG_CONN
    GSM_MSG_VAR_REF(msg).cmd = GSM_CMD_CIPSGTXT;
#endif /* GSM_CFG_CONN */
    GSM_MSG_VAR_REF(msg).msg.pdp_ctx.pdp_type = pdp_type;
    GSM_MSG_VAR_REF(msg).msg.pdp_ctx.apn = apn;
    
    return gsmi_send_msg_to_producer_mbox(&GSM_MSG_VAR_REF(msg), gsmi_initiate_cmd, 200000);
}

#endif /* GSM_CFG_NETWORK || __DOXYGEN__ */

gsmr_t
gsm_reg_check(const gsm_api_cmd_evt_fn evt_fn, void* const evt_arg, const uint32_t blocking) {
    GSM_MSG_VAR_DEFINE(msg);

    GSM_MSG_VAR_ALLOC(msg);
    GSM_MSG_VAR_SET_EVT(msg);
    GSM_MSG_VAR_REF(msg).cmd = GSM_CMD_CREG_GET;

    return gsmi_send_msg_to_producer_mbox(&GSM_MSG_VAR_REF(msg), gsmi_initiate_cmd, 60000);
}

gsmr_t
gsm_network_pause_transfer(const gsm_api_cmd_evt_fn evt_fn, void* const evt_arg, const uint32_t blocking) {
    GSM_MSG_VAR_DEFINE(msg);

    GSM_MSG_VAR_ALLOC(msg);
    GSM_MSG_VAR_SET_EVT(msg);
    GSM_MSG_VAR_REF(msg).cmd = GSM_CMD_PPP;

    return gsmi_send_msg_to_producer_mbox(&GSM_MSG_VAR_REF(msg), gsmi_initiate_cmd, 60000);
}


/**
 * \brief           Read RSSI signal from network operator
 * \param[out]      rssi: RSSI output variable. When set to `0`, RSSI is not valid
 * \param[in]       evt_fn: Callback function called when command has finished. Set to `NULL` when not used
 * \param[in]       evt_arg: Custom argument for event callback function
 * \param[in]       blocking: Status whether command should be blocking or not
 * \return          \ref gsmOK on success, member of \ref gsmr_t enumeration otherwise
 */
gsmr_t
gsm_network_rssi(int16_t* rssi,
                    const gsm_api_cmd_evt_fn evt_fn, void* const evt_arg, const uint32_t blocking) {
    GSM_MSG_VAR_DEFINE(msg);

    GSM_MSG_VAR_ALLOC(msg);
    GSM_MSG_VAR_SET_EVT(msg);
    GSM_MSG_VAR_REF(msg).cmd_def = GSM_CMD_CSQ_GET;
    GSM_MSG_VAR_REF(msg).msg.csq.rssi = rssi;

    return gsmi_send_msg_to_producer_mbox(&GSM_MSG_VAR_REF(msg), gsmi_initiate_cmd, 120000);
}

/**
 * \brief           Get network registration status
 * \return          Member of \ref gsm_network_reg_status_t enumeration
 */
gsm_network_reg_status_t
gsm_network_get_reg_status(void) {
    gsm_network_reg_status_t ret;
    gsm_core_lock();
    ret = gsm.m.network.status;
    gsm_core_unlock();
    return ret;
}
