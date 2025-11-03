/*******************************************************************************
*
*               COPYRIGHT (c) 2021-2022 Broadlink Corporation
*                         All Rights Reserved
*
* The source code contained or described herein and all documents
* related to the source code ("Material") are owned by Broadlink
* Corporation or its licensors.  Title to the Material remains
* with Broadlink Corporation or its suppliers and licensors.
*
* The Material is protected by worldwide copyright and trade secret
* laws and treaty provisions. No part of the Material may be used,
* copied, reproduced, modified, published, uploaded, posted, transmitted,
* distributed, or disclosed in any way except in accordance with the
* applicable license agreement.
*
* No license under any patent, copyright, trade secret or other
* intellectual property right is granted to or conferred upon you by
* disclosure or delivery of the Materials, either expressly, by
* implication, inducement, estoppel, except in accordance with the
* applicable license agreement.
*
* Unless otherwise agreed by Broadlink in writing, you may not remove or
* alter this notice or any other notice embedded in Materials by Broadlink
* or Broadlink's suppliers or licensors in any way.
*
*******************************************************************************/

#ifndef __FCBLE_API_H
#define __FCBLE_API_H

#include "fcble_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
*  SDK initialization interface.
*
*  @info: device information (such as mac address/product type)
*
*  Return 0 on success, otherwise failed.
*/
int32_t fcble_init(fcble_dev_info_t * info);

/*
*  BLE frame process interface.
*
*  @pdu: BLE frame PDU (skip preamble and access address, drop the last CRC)
*  @pdu_len: BLE frame PDU length
*
*  Return 0 on success (protocol matched), otherwise failed.
*
*  Note: called by the BLE driver when a frame is received.
*/
int32_t fcble_ble_frame_handle(uint8_t * pdu, uint8_t pdu_len);

/*
*  SDK event process interface.
*
*  Note: called by the application layer every 15ms.
*/
void fcble_event_handle(void);

/*
*  Enter into pairing state, you can using APP to pairing it.
*
*  @timeout: pairing window (sec), if set to 0, then using default: 60s
*
*  Return 0 on success, otherwise failed.
*/
int32_t fcble_pairing_start(uint16_t timeout);

/*
*  Exit pairing state.
*
*  Return 0 on success, otherwise failed.
*/
int32_t fcble_pairing_stop(void);

/*
*  Event notify callback register interface.
*
*  @callback: custom callback
*
*  Return 0 on success, otherwise failed.
*/
int32_t fcble_event_notify_register(fcble_event_notify_cb_t callback, void *ctx);


/*
*  led groove packet send interface .
*  this interface needs to be called once every 100ms
*
*  @light_level: led lightness 0-127
*  @mode: classic or rock(0,1)
*  @color_change: whether need to change the RGB color.(0,1)
*
*  Return 0 on success, otherwise failed.
*/
int32_t fcble_led_groove_packet_send(uint8_t light_level, uint8_t mode, uint8_t color_change);

#ifdef __cplusplus
}
#endif

#endif

