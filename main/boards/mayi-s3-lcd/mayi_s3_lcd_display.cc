#include "mayi_s3_lcd_display.h"

#include <esp_log.h>

#include <algorithm>
#include <cstring>

#include "display/lcd_display.h"
#include "assets/lang_config.h"
#include <font_awesome_symbols.h>
#include "esp_system.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "sdkconfig.h"

#define GPIO_INPUT_PIN       GPIO_NUM_8
#define ADC_CHANNEL          ADC1_CHANNEL_0  // 假设 GPIO8 连接到 ADC1 的通道（ADC1_CH0）
#define VBUS_VOLTAGE         5.0              // 假设 VBUS 为 5V

#define TAG "MaYiS3LCDDisplay"
#include "otto_emoji_gif.h"

LV_FONT_DECLARE(font_awesome_30_4);

MayiS3LCDDisplay::MayiS3LCDDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                   int width, int height, int offset_x, int offset_y, bool mirror_x,
                                   bool mirror_y, bool swap_xy, DisplayFonts fonts)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                    fonts),
      emotion_gif_(nullptr),
      last_emotion_gif_desc_(nullptr) {
    ESP_LOGI(TAG, "MayiS3LCDDisplay initialized with width: %d, height: %d", width, height);
    SetupUI();
};

void MayiS3LCDDisplay::SetupHighTempWarningPopup() {
    // 创建高温警告弹窗
    high_temp_popup_ = lv_obj_create(lv_scr_act());  // 使用当前屏幕
    lv_obj_set_scrollbar_mode(high_temp_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(high_temp_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);
    lv_obj_align(high_temp_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(high_temp_popup_, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_radius(high_temp_popup_, 10, 0);
    
    // 创建警告标签
    high_temp_label_ = lv_label_create(high_temp_popup_);
    lv_label_set_text(high_temp_label_, "警告：温度过高");
    lv_obj_set_style_text_color(high_temp_label_, lv_color_white(), 0);
    lv_obj_center(high_temp_label_);
    
    // 默认隐藏
    lv_obj_add_flag(high_temp_popup_, LV_OBJ_FLAG_HIDDEN);
}

void MayiS3LCDDisplay::UpdateHighTempWarning(float chip_temp, float threshold) {
    if (high_temp_popup_ == nullptr) {
        ESP_LOGW("MayiS3LCDDisplay", "High temp popup not initialized!");
        return;
    }

    if (chip_temp >= threshold) {
        ShowHighTempWarning();
    } else {
        HideHighTempWarning();
    }
}

void MayiS3LCDDisplay::ShowHighTempWarning() {
    DisplayLockGuard lock(this);
    if (high_temp_popup_ && lv_obj_has_flag(high_temp_popup_, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(high_temp_popup_, LV_OBJ_FLAG_HIDDEN);
    }
}

void MayiS3LCDDisplay::HideHighTempWarning() {
    DisplayLockGuard lock(this);
    if (high_temp_popup_ && !lv_obj_has_flag(high_temp_popup_, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(high_temp_popup_, LV_OBJ_FLAG_HIDDEN);
    }
}

// 运行时逻辑函数
void replace_element(lv_obj_t* from, lv_obj_t* &to) {
    // 获取目标位置的坐标
    lv_coord_t x = lv_obj_get_x(to);
    lv_coord_t y = lv_obj_get_y(to);

    // 将 A 移动到 B 的位置
    lv_obj_set_pos(from, x, y);

    // 销毁 B
    lv_obj_del(to);
    to = nullptr;
    // 或隐藏 B
    // lv_obj_hide(to);
}

void MayiS3LCDDisplay::SetupUI() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);
    lv_obj_set_style_text_color(screen, current_theme_.text, 0);
    lv_obj_set_style_bg_color(screen, current_theme_.background, 0);

    // 创建 GIF 动画容器
    emotion_gif_ = lv_gif_create(screen);
    lv_obj_set_size(emotion_gif_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_border_width(emotion_gif_, 0, 0);
    lv_obj_set_style_bg_color(emotion_gif_, lv_color_white(), 0);
    // 重点：将 GIF 移动到背景层，使其不参与 flex 布局
    lv_obj_move_background(emotion_gif_);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, fonts_.text_font->line_height);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);
    // lv_obj_set_style_bg_color(status_bar_, current_theme_.background, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);

    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_style_border_width(content_, 0, 0); // 移除边框
    lv_obj_set_style_pad_bottom(content_, 5, 0);

    // 增加 20px 的底部填充，避免显示过下圆形屏上看不见
    lv_obj_set_style_pad_bottom(content_, 20, 0);
    lv_obj_set_style_bg_opa(content_, LV_OPA_TRANSP, 0);
    // lv_obj_set_style_bg_color(content_, current_theme_.chat_background, 0);
    lv_obj_set_style_border_color(content_, current_theme_.border, 0); // Border color for content

    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN); // 垂直布局（从上到下）
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER); // 子对象居中对齐，等距分布

    chat_message_label_ = lv_label_create(content_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_size(chat_message_label_, LV_HOR_RES * 0.7, fonts_.text_font->line_height * 2 + 10);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(chat_message_label_, current_theme_.text, 0);

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(network_label_, current_theme_.text, 0);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(notification_label_, current_theme_.text, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status_label_, current_theme_.text, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(mute_label_, current_theme_.text, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(battery_label_, current_theme_.text, 0);

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, current_theme_.low_battery, 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    // 为圆形屏幕微调
    lv_obj_set_size(status_bar_, LV_HOR_RES, fonts_.text_font->line_height * 2 + 10);
    lv_obj_set_style_layout(status_bar_, LV_LAYOUT_NONE, 0);
    lv_obj_set_style_pad_top(status_bar_, 10, 0);
    lv_obj_set_style_pad_bottom(status_bar_, 2, 0);
    
    // 针对圆形屏幕调整位置
    //      network  battery  mute     //
    //               status            //
    lv_obj_align(battery_label_, LV_ALIGN_TOP_MID, -2.5*fonts_.icon_font->line_height, 0);
    lv_obj_align(network_label_, LV_ALIGN_TOP_MID, -0.5*fonts_.icon_font->line_height, 0);
    lv_obj_align(mute_label_, LV_ALIGN_TOP_MID, 1.5*fonts_.icon_font->line_height, 0);
    
    lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_grow(status_label_, 0);
    lv_obj_set_width(status_label_, LV_HOR_RES * 0.75);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);

    lv_obj_align(notification_label_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_width(notification_label_, LV_HOR_RES * 0.75);
    lv_label_set_long_mode(notification_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);

    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(low_battery_popup_, lv_color_hex(0xFF0000), 0);
    lv_obj_set_width(low_battery_label_, LV_HOR_RES * 0.75);
    lv_label_set_long_mode(low_battery_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);

    ESP_LOGI(TAG, "MayiS3LCDDisplay finished SetupUI...");
}

void MayiS3LCDDisplay::UpdateGifIfNecessary(const lv_img_dsc_t* new_img_dsc) {
    if (new_img_dsc == nullptr) {
        ESP_LOGE(TAG, "UpdateGifIfNecessary: new_img_dsc is null");
        return;
    }

    if (new_img_dsc != last_emotion_gif_desc_) {
        ESP_LOGI(TAG, "Updating GIF from %p to %p", last_emotion_gif_desc_, new_img_dsc);
        lv_gif_set_src(emotion_gif_, new_img_dsc);
        last_emotion_gif_desc_ = const_cast<lv_img_dsc_t*>(new_img_dsc);
    }
}

void MayiS3LCDDisplay::SetEmotion(const char* emotion) {
    ESP_LOGI(TAG, "MayiS3LCDDisplay SetEmotion '%s'", emotion);
    if (!emotion || !emotion_gif_) {
        return;
    }

    DisplayLockGuard lock(this);

    const lv_img_dsc_t* emotion_img = otto_emoji_gif_get_by_name(emotion);
    if (emotion_img != NULL) {
        UpdateGifIfNecessary(emotion_img);
    } else {
        // 使用默认表情
        ESP_LOGW(TAG, "MayiS3LCDDisplay 找不到表情 '%s'， 设置为默认 neutral", emotion);
        UpdateGifIfNecessary(&neutral);
    }
}

void MayiS3LCDDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);

    if (chat_message_label_ == nullptr) {
        return;
    }

    if (content == nullptr || strlen(content) == 0) {
        lv_obj_add_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (role == nullptr || strcmp(role, "system") == 0) {
        // 系统消息
        lv_obj_set_style_text_color(chat_message_label_, current_theme_.system_text, 0);
    } else if (strcmp(role, "low_battery") == 0) {
        lv_obj_set_style_text_color(chat_message_label_, current_theme_.low_battery, 0);
    } else {
        // 默认文本颜色
        lv_obj_set_style_text_color(chat_message_label_, current_theme_.text, 0);
    }
    lv_label_set_text(chat_message_label_, content);
    lv_obj_clear_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
}
