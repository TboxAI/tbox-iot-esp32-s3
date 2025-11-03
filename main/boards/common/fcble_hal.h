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

#ifndef __FCBLE_HAL_H
#define __FCBLE_HAL_H

#include <stdint.h>
#include <stdio.h>
#if 0
#include "dna_libc.h"
#endif

//#define fcble_hal_printf printf
#define fcble_hal_printf(...) NULL

//extern uint32_t fcble_sys_ticks;

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t fcble_hal_system_ticks(void);
extern int32_t fcble_hal_nvm_write(uint16_t addr, uint8_t *data, uint8_t len);
extern int32_t fcble_hal_nvm_read(uint16_t addr, uint8_t *buf, uint8_t len);
extern int32_t fcble_hal_ble_init(uint8_t channel);
extern int32_t fcble_hal_ble_frame_send(uint8_t *pdu, uint8_t pdu_len);
extern void fcble_hal_get_roll_code(uint8_t *roll_code);

#ifdef __cplusplus
}
#endif

#endif
