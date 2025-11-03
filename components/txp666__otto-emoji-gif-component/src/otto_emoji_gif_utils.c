/**
 * @file otto_emoji_gif_utils.c
 * @brief Otto机器人GIF表情资源组件辅助函数实现
 */

#include <string.h>

#include "otto_emoji_gif.h"

// 表情映射表
typedef struct {
    const char* name;
    const lv_image_dsc_t* gif;
} emotion_map_t;

// 外部声明的GIF资源
extern const lv_image_dsc_t amazed;
extern const lv_image_dsc_t happy;
extern const lv_image_dsc_t neutral;
extern const lv_image_dsc_t sad;
extern const lv_image_dsc_t amazed;
extern const lv_image_dsc_t offline;
extern const lv_image_dsc_t like;
extern const lv_image_dsc_t neutral;
extern const lv_image_dsc_t thinking;
extern const lv_image_dsc_t network_setup;

// 表情映射表
static const emotion_map_t emotion_maps[] = {
    {"amazed", &amazed},
    {"confused", &amazed},
    {"shocked", &amazed},
    {"happy", &happy},
    {"neutral", &neutral},
    {"sleepy", &sad},    
    {"amazed", &amazed},    
    {"sad", &sad},
    {"offline", &offline},
    {"thinking", &thinking},
    {"network_setup", &network_setup}, // 网络设置态

      // 积极/开心类表情 -> happy
    {"happy", &happy},
    {"laughing", &happy},
    {"funny", &happy},
    {"loving", &like},
    {"confident", &happy},
    {"winking", &happy},
    {"cool", &happy},
    {"delicious", &happy},
    {"kissy", &happy},
    {"silly", &happy},

    // 悲伤类表情 -> sad
    {"sad", &sad},
    {"angry", &sad},
    {"embarrassed", &sad},
    {"crying", &sad},
    {NULL, NULL}  // 结束标记
};

const char* otto_emoji_gif_get_version(void) {
    return "1.0.2";
}

int otto_emoji_gif_get_count(void) {
    return 6;
}

const lv_image_dsc_t* otto_emoji_gif_get_by_name(const char* name) {
    if (name == NULL) {
        return NULL;
    }

    for (int i = 0; emotion_maps[i].name != NULL; i++) {
        if (strcmp(emotion_maps[i].name, name) == 0) {
            return emotion_maps[i].gif;
        }
    }

    return NULL;  // 未找到
}
