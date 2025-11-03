#pragma once

#include <libs/gif/lv_gif.h>

#include "display/lcd_display.h"
#include "otto_emoji_gif.h"

/**
 * @brief Mayi S3 LCD 显示实现
 * 继承LcdDisplay，添加GIF表情支持
 */
class MayiS3LCDDisplay : public SpiLcdDisplay {
public:
    /**
     * @brief 构造函数，参数与SpiLcdDisplay相同
     */
    MayiS3LCDDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                     int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                     bool swap_xy, DisplayFonts fonts);

    virtual ~MayiS3LCDDisplay() = default;

    // 重写表情设置方法
    virtual void SetEmotion(const char* emotion) override;

    // 重写聊天消息设置方法
    virtual void SetChatMessage(const char* role, const char* content) override;

    void SetupHighTempWarningPopup();
    void UpdateHighTempWarning(float chip_temp, float threshold = 85.0f);
    void ShowHighTempWarning();
    void HideHighTempWarning();

protected:
    /**
     * @brief 设置UI界面
     * 重写父类方法，添加GIF表情容器
     */
    void SetupUI();
    void UpdateGifIfNecessary(const lv_img_dsc_t* new_img_dsc);

private:
    lv_obj_t* emotion_gif_;  ///< GIF表情组件
    void * last_emotion_gif_desc_;
    lv_obj_t* high_temp_popup_ = nullptr;  // 高温警告弹窗
    lv_obj_t* high_temp_label_ = nullptr;  // 高温警告标签

    // 表情映射
    struct EmotionMap {
        const char* name;
        const lv_img_dsc_t* gif;
    };
};