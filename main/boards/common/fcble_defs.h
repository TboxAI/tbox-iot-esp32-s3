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

#ifndef __FCBLE_DEFS_H
#define __FCBLE_DEFS_H

#include <stdint.h>

#define CUSTOM_SCENE_ID_LEN             3
#define CUSTOM_SCENE_CMD_LEN            11
#define TLV_TYPE_SCENE_CONFIG           TLV_TYPE_HEART_BEAT /* 设置自定义场景*/
#define TLV_TYPE_SCENE_TRIGGER          TLV_TYPE_CLK_UPLOAD /* 执行自定义场景*/
#define ABILITY_ALL_SUPPORT             \
    (ABILITY_RM_SUPPORT | ABILITY_SCHED_SUPPORT | ABILITY_DELAY_SUPPORT | \
     ABILITY_SCENE_SUPPORT | ABILITY_SLOW_SUPPORT | ABILITY_SCENE_MODE_SUPPORT | \
     ABILITY_BRIGHTNESS_MIN_LEVEL0 | ABILITY_1024_GRAY_LEVEL_SUPPORT | ABILITY_CUSTOM_SCENE_EXCLUDE_SUPPORT | \
     ABILITY_TEMP_GROUP_SUPPORT)

/* id: group_id */
#define FLOOR_ROOM_MAX_NUM              32    /* MUST set to 2 ^ N */
#define WHOLE_HOUSE_ID                  0
#define IS_FLOOR_ID(id)                 (!((id) % FLOOR_ROOM_MAX_NUM))
#define IS_WHOLE_HOUSE_ID(id)           ((id) == WHOLE_HOUSE_ID)
#define GET_FLOOR_NUM(id)               ((id) / FLOOR_ROOM_MAX_NUM)
#define GET_ROOM_NUM(id)                ((id) % FLOOR_ROOM_MAX_NUM)

/* TYPE:1~TYPE:6 */
typedef enum fastcon_msg_type {
    MSG_TYPE_PROBE_REQ = 0,
	MSG_TYPE_DISC_REQ = 1,
    MSG_TYPE_DISC_RES,
    MSG_TYPE_UP_REQ,
    MSG_TYPE_UP_RES,
    MSG_TYPE_CTRL_REQ,
    MSG_TYPE_CTRL_RES,
    MSG_TYPE_RM,
} fastcon_msg_type_e;

/* TLV type (sub_type) */
typedef enum fastcon_tlv_type {
    TLV_TYPE_NULL = 0,
	TLV_TYPE_SET_GP_ADDR = 1,
    TLV_TYPE_CTRL_BY_ADDR,
    TLV_TYPE_CTRL_BY_LOC, /*基于品类和位置的控制，上报时复用*/
    TLV_TYPE_HEART_BEAT,  /*心跳*/
    TLV_TYPE_SET_TEMP_GROUP, /*设置临时Group，用于批量控制*/

    /*--------------自定义TLV类型------------------*/
    TLV_TYPE_GRPCTL, /*音乐律动，按short addr索引group*/
    TLV_TYPE_BATCH_CTRL, /*多设备控制*/
    TLV_TYPE_MODE_SET, /*模式设置(如全彩渐变/全彩跳变等)*/
    TLV_TYPE_CLK_SET,    /*定时下发*/
    TLV_TYPE_CLK_GET = 10,
    TLV_TYPE_CLK_UPLOAD,
    TLV_TYPE_GP_MODE_SET, /*基于group的模式设置(如全彩渐变/全彩跳变等)*/
	TLV_TYPE_RM_CLEAN = 13,
	TLV_TYPE_DELAY_CTRL,        /* 用于实现起床/睡眠等情景模式*/
	TLV_TYPE_GROUP_DELAY_CTRL,  /* 同上，基于组*/
} fastcon_tlv_type_e;

/* RM TLV type */
typedef enum fastcon_rm_tlv_type {
    RM_TLV_TYPE_PAIRING_REQ = 0,
    RM_TLV_TYPE_PAIRING_RSP,
    RM_TLV_TYPE_CTRL_REQ,
    RM_TLV_TYPE_DELAY_WORK,
    RM_TLV_TYPE_DEL_REQ,
    RM_TLV_MODE_PAUSE,
    RM_TLV_ABS_BRIGHTNESS = 6,
    RM_TLV_ABS_COLOR,
    RM_TLV_REL_BRIGHTNESS,
    RM_TLV_REL_CCT,
    RM_TLV_DELAY_TIME,
    RM_TLV_STATUS_SAVE,
    RM_TLV_STATUS_RESTORE,
    RM_TLV_STEPLESS_SET,
    RM_TLV_ABS_COLOR_WITH_ON,
	RM_TLV_MODE_SET,
    RM_TLV_MAX,
} fastcon_rm_tlv_type_e;

typedef enum dev_ability_type {
    ABILITY_NONE_SUPPORT                    = 0,          /* 不支持任何能力*/
    ABILITY_RM_SUPPORT                      = (1u << 0),  /* 支持遥控器*/
    ABILITY_SCHED_SUPPORT                   = (1u << 1),  /* 支持定时*/
    ABILITY_DELAY_SUPPORT                   = (1u << 2),  /* 支持倒计时*/
    ABILITY_SCENE_SUPPORT                   = (1u << 3),  /* 支持自定义场景*/
    ABILITY_SLOW_SUPPORT                    = (1u << 4),  /* 支持渐变*/
    ABILITY_SCENE_MODE_SUPPORT              = (1u << 5),  /* 支持起床/ 睡眠等情景模式*/
    ABILITY_BRIGHTNESS_MIN_LEVEL0           = (0u << 6),  /* 支持最低亮度到1% */
    ABILITY_BRIGHTNESS_MIN_LEVEL1           = (1u << 6),  /* 支持最低亮度到5% */
    ABILITY_BRIGHTNESS_MIN_LEVEL2           = (2u << 6),  /* 支持最低亮度到10% */
    ABILITY_BRIGHTNESS_MIN_LEVEL3           = (3u << 6),  /* 支持最低亮度到15% */
    ABILITY_1024_GRAY_LEVEL_SUPPORT         = (1u << 8),  /* 支持1024灰度*/
    ABILITY_CUSTOM_SCENE_EXCLUDE_SUPPORT    = (1u << 9),  /* 支持场景例外*/
    ABILITY_TEMP_GROUP_SUPPORT              = (1u << 10), /* 支持临时Group */
    ABILITY_STATUS_REPORT_SUPPORT           = (1u << 11), /* 支持状态查询及上报*/
    ABILITY_GATEWAY_SUPPORT                 = (1u << 12), /* 支持网关能力*/
} dev_ability_type_e;

/* Event notify type */
typedef enum fastcon_notify_type {
    FCBLE_EVENT_NOTIFY_TEST_START = 0,
    FCBLE_EVENT_NOTIFY_TEST_DONE,
    FCBLE_EVENT_NOTIFY_CONFIG_DONE,
    FCBLE_EVENT_NOTIFY_GROUP_CHANGE,
    FCBLE_EVENT_NOTIFY_ADDR_CHANGE,
    FCBLE_EVENT_NOTIFY_RM_BIND,
    FCBLE_EVENT_NOTIFY_RM_CLEAN,
    FCBLE_EVENT_NOTIFY_TIME_SYNC,
    FCBLE_EVENT_NOTIFY_TEMP_GROUP_SET,
    FCBLE_EVENT_NOTIFY_STATUS_REPORT_REQ,
    FCBLE_EVENT_NOTIFY_DATA_REPORT,

	FCBLE_EVENT_NOTIFY_BYPASS_REQ,
} fastcon_notify_type_e;

typedef enum {
    CUSTOM_SCENE_SET = 0,
    CUSTOM_SCENE_DELETE,
    CUSTOM_SCENE_SET_DEFAULT,
    CUSTOM_SCENE_EXCLUDE,
} custom_scene_opcode_e;

typedef struct scene_set_info {
    uint8_t short_addr;
    uint8_t type;
} scene_set_info_t;

typedef struct scene_set_head {
    scene_set_info_t info;
    uint8_t scene_id[CUSTOM_SCENE_ID_LEN];
} scene_set_head_t;

typedef struct scene_set_req {
    scene_set_head_t head;
    uint8_t cmd[];
} scene_set_req_t;

typedef struct timing_info {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t week;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} timing_info_t;

typedef struct sched_info {
    uint8_t	hour : 5;
    uint8_t enable : 1;	/* 是否有效 (repeat为0，不循环执行，执行一次之后，该位变成0*/
    uint8_t onoff : 1;	/* 开关状态*/
    uint8_t repeat : 1;	/* 是否只执行1次 */
    uint8_t	min : 6;
    uint8_t res : 2;	/* 保留 */
} sched_info_t;

typedef struct sched_list {
    sched_info_t sched_info[4];
} sched_list_t;

typedef struct lc_time {
    uint8_t	hour : 5;
    uint8_t	week : 3;  
    uint8_t	min : 6;  /* min + hour*60 ，当天的分钟值*/
    uint8_t	adj : 1; /* 是否被手机正确的校准小时 、分钟值*/
	uint8_t	res : 1;
} lc_time_t;

/* 定时任务*/
typedef struct sched_set {
    uint8_t short_addr; /* short_addr为1~255*/
    lc_time_t lc_time;	/* 用于时间校准*/
    sched_list_t sched_list;
} sched_set_t;

typedef struct delay_work_req {
    uint16_t delay_time;    /* 延迟时间，单位秒*/
    uint8_t start_pwm;      /* 起始亮度PWM */
    uint8_t interval;       /* 执行间隔，单位秒*/
    int8_t step;            /* 步长(正负) */
    uint8_t count;          /* 执行次数*/
    uint8_t cold;           /* 冷光分量*/
    uint8_t warm;           /* 暖光分量*/
} delay_work_req_t;

/* Report type list */
typedef enum fcble_report_type {
    REPORT_TYPE_DEV_STATUS = 0,
} fcble_report_type_e;

typedef struct fcble_report_req {
    uint8_t delay_max; /* 在[0 - delay_max]区间进行延迟上报，如果delay_max等于0，表示立即上报，以jiffies为单位, delay_max + 1必须符合2^N次方对齐*/
    uint8_t res;        /* 预留，目前固定为0 */
    uint8_t start_addr; /* 设备mask中首个设备（即mask[0].BIT0）的短地址*/
    uint8_t mask[13]; /* 设备掩码: 13字节，一个包最多13 * 8 = 104个设备*/
} fcble_report_req_t;

typedef struct fcble_cfg_info {
	uint8_t short_addr;
    uint8_t group_id;
	uint8_t key[4];
} fcble_cfg_info_t;

typedef struct fcble_dev_info {
    uint8_t * license;  /* 设备License */
    uint8_t did[6];     /* 设备MAC */
    uint16_t pid;       /* 产品类型*/
    uint32_t ability;   /* 支持的能力集合*/
    uint8_t plat_id;    /* 平台ID (自定义) */
    uint8_t vendor_id;  /* 厂商ID (自定义) */
    uint8_t soft_ver;   /* 软件版本号*/
    uint8_t res;        /* 保留字段(未使用) */
} fcble_dev_info_t;

/*
@type: fastcon_msg_type_e
@sub_type: fastcon_tlv_type_e / fastcon_rm_tlv_type_e
*/
typedef int32_t (* fcble_data_handle_cb_t)(
    uint8_t type, uint8_t sub_type, uint8_t * data, uint8_t len);

/*
@type: fastcon_notify_type_e
*/
typedef void (* fcble_event_notify_cb_t)(
    uint8_t type, uint8_t * data, uint8_t len, void *ctx);

/*
* 检查group_id是否为设备所在楼层的楼层ID(含全屋)
* 返回true表示检查通过。
*/
uint8_t check_floor(uint8_t group_id);

/*
* 检查group_id是否为全屋、设备所在房间/楼层/临时组等
* 返回true表示检查通过。
*/
uint8_t check_group(uint8_t group_id);

#endif

