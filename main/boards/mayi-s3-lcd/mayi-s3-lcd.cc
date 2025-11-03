
#include "wifi_board.h"
#include "audio_codecs/vb6824_audio_codec.h"
#include "display/lcd_display.h"
#include "led/single_led.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/spi_common.h>

#include "power_save_timer.h"
#include "assets/lang_config.h"
#include <esp_lcd_st77916.h>
#include "mayi_s3_lcd_display.h"
#include "mayi_s3_pm.h"
#include "bt42_ibeacon.h"

#define TAG "MaYiS3LcdBoard"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

static const st77916_lcd_init_cmd_t vendor_specific_init_new[] = {
    {0xDE, (uint8_t []){0x00}, 1, 0},
    {0xDF, (uint8_t []){0x98, 0x55}, 2, 0},
    {0xB2, (uint8_t []){0x1F}, 1, 0},
    {0xB7, (uint8_t []){0x01, 0x2D, 0x01, 0x55}, 4, 0},
    {0xBB, (uint8_t []){0x1B, 0x64, 0xC4, 0x0E, 0x3E, 0xF5}, 6, 0},
    {0xBC, (uint8_t []){0x03, 0x20, 0xF3, 0xC0}, 4, 0},
    {0xC0, (uint8_t []){0x22, 0xA1}, 2, 0},
    {0xC3, (uint8_t []){0x00, 0x02, 0x2A, 0x0B, 0x08, 0x48, 0x08, 0x04, 0x62, 0x30, 0x30}, 11, 0},
    {0xC4, (uint8_t []){0x40, 0x00, 0xAD, 0x68, 0x43, 0x07, 0x04, 0x16, 0x43, 0x07, 0x04}, 11, 0},
    {0xC8, (uint8_t []){
        0x3F, 0x31, 0x28, 0x25, 0x25, 0x27, 0x22, 0x22, 0x20, 0x1F, 0x1C, 0x12, 0x0F, 0x0B, 0x02, 0x02,
        0x3F, 0x31, 0x28, 0x25, 0x25, 0x27, 0x22, 0x22, 0x20, 0x1F, 0x1C, 0x12, 0x0F, 0x0B, 0x02, 0x02
    }, 32, 0},
    {0xD3, (uint8_t []){0x28, 0x13}, 2, 0},
    {0xD9, (uint8_t []){0x00, 0x00, 0xFF, 0x00, 0xF0, 0x00}, 6, 0},
    {0xDE, (uint8_t []){0x01}, 1, 0},
    {0xB7, (uint8_t []){0x13, 0xE7, 0x64, 0x39, 0x06, 0x36, 0x19, 0x1C}, 8, 0},
    {0xBE, (uint8_t []){0x00}, 1, 0},
    {0xC1, (uint8_t []){0x00, 0x4A, 0x80}, 3, 0},
    {0xC2, (uint8_t []){0x00, 0x16, 0xDA, 0xE7}, 4, 0},
    {0xC7, (uint8_t []){0x00, 0x00, 0x00, 0x38, 0x08, 0x08, 0x00, 0x01}, 8, 0}, 
    {0xC8, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x15, 0x3B}, 6, 0},
    {0xC9, (uint8_t []){0x00, 0x16, 0x06, 0x04, 0x0A}, 5, 0},
    {0xCA, (uint8_t []){0x08, 0x35, 0x16, 0x1F, 0x1F}, 5, 0},
    {0xCB, (uint8_t []){0x01, 0x16, 0x07, 0x05, 0x0B}, 5, 0},
    {0xCC, (uint8_t []){0x09, 0x35, 0x16, 0x1F, 0x1F}, 5, 0},
    {0xCD, (uint8_t []){0x01, 0x16, 0x09, 0x0B, 0x05}, 5, 0},
    {0xCE, (uint8_t []){0x07, 0x15, 0x1F, 0x16, 0x1F}, 5, 0},
    {0xCF, (uint8_t []){0x00, 0x16, 0x08, 0x0A, 0x04}, 5, 0},
    {0xD0, (uint8_t []){0x06, 0x15, 0x1F, 0x16, 0x1F}, 5, 0},
    {0xD1, (uint8_t []){0x02, 0x30}, 2, 0},
    {0xD2, (uint8_t []){0x02, 0x03, 0x52, 0xDF, 0xDD}, 5, 0},
    {0xD3, (uint8_t []){0x3B, 0x04, 0x48}, 3, 0},
    {0xD5, (uint8_t []){0x10, 0x10, 0x07, 0x07, 0x0F, 0x94, 0x26}, 7, 0},
    {0xD6, (uint8_t []){0x00, 0x00, 0x40}, 3, 0}, 
    {0xD7, (uint8_t []){0x00, 0x00, 0x20}, 3, 0},
    {0xDE, (uint8_t []){0x02}, 1, 0}, 
    {0xB6, (uint8_t []){0x1C}, 1, 0},
    {0xDE, (uint8_t []){0x00}, 1, 0},
    {0x4D, (uint8_t []){0x00}, 1, 0},
    {0x4E, (uint8_t []){0x00}, 1, 0},
    {0x4F, (uint8_t []){0x00}, 1, 0},
    {0x4C, (uint8_t []){0x01}, 1, 10},
    {0x4C, (uint8_t []){0x00}, 1, 0},
    {0x2A, (uint8_t []){0x00, 0x00, 0x01, 0x67}, 4, 0},
    {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0x67}, 4, 0}, 
    {0x35, (uint8_t []){0x00}, 1, 0}, 
    {0x11, (uint8_t []){0x00}, 1, 120},
    {0x29, (uint8_t []){0x00}, 1, 10},
};

class MayiS3LCDBoard : public WifiBoard {
private:
    Button boot_button_;
    MayiS3LCDDisplay* display_;
    VbAduioCodec audio_codec_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
    BT42IBeacon ibeacon_;

    void InitializePowerManager() {
        power_manager_ = new PowerManager(GPIO_NUM_8);
        // 注意: 目前测不到主板温度，先关掉高温报警
        // power_manager_->OnTemperatureChanged([this](float chip_temp) {
        //     display_->UpdateHighTempWarning(chip_temp);
        // });
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            ESP_LOGI(TAG, "PM: is charging: %d", is_charging);
            auto &app = Application::GetInstance();
            if (app.GetDeviceState() != kDeviceStateWifiConfiguring) {
                display_->SetChatMessage("system",  is_charging ? Lang::Strings::CHARGING : "");
            }
            power_save_timer_->SetEnabled(!is_charging);
        });
        power_manager_->OnLowBatteryStatusChanged([this](bool is_low_battery) {
            if (is_low_battery) {
                ESP_LOGW(TAG, "PM: Low battery detected");
                display_->SetChatMessage("low_battery", Lang::Strings::LOW_ENERGY_PROMPT);
                display_->SetEmotion("sleepy");
            }
            /*
            else {
                ESP_LOGI(TAG, "PM: Battery level is sufficient");
                display_->SetChatMessage("system", "电量充足");
                display_->SetEmotion("happy");
            }
            */
        });
    }

    void InitializePowerSaveTimer() {
        gpio_set_direction(POWER_KRRP_GPIO, GPIO_MODE_OUTPUT);
        gpio_set_pull_mode(POWER_KRRP_GPIO, GPIO_PULLUP_ONLY);
        gpio_set_level(POWER_KRRP_GPIO, 1);

        power_save_timer_ = new PowerSaveTimer(240, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGW(TAG, "Enabling sleep mode");
            display_->SetChatMessage("system", "");
            display_->SetEmotion("sad");
            GetBacklight()->SetBrightness(10);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            ESP_LOGW(TAG, "Enabling sleep mode");
            // gpio_set_level(POWER_KRRP_GPIO, 1);
            display_->SetChatMessage("system", "");
            display_->SetEmotion("neutral");
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGW(TAG, "Shutting down");
            gpio_set_level(POWER_KRRP_GPIO, 0);
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            if (audio_codec_.InOtaMode(1) == true) {
                ESP_LOGI(TAG, "OTA mode, do not enter chat");
                return;
            }
            auto &app = Application::GetInstance();
            app.ToggleChatState();
        });
        boot_button_.OnPressRepeat([this](uint16_t count) {
            if(count >= 3){
                if (audio_codec_.InOtaMode(1) == true) {
                    ESP_LOGI(TAG, "OTA mode, do not enter chat");
                    return;
                }
                ResetWifiConfiguration();
            }
        });
        boot_button_.OnLongPress([this]() {
            gpio_set_level(POWER_KRRP_GPIO, 0);
        });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
    }

    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize QSPI bus");
        const spi_bus_config_t bus_config = {                                                           
            .data0_io_num = QSPI_PIN_NUM_LCD_DATA0,                                     
            .data1_io_num = QSPI_PIN_NUM_LCD_DATA1,                                     
            .sclk_io_num = QSPI_PIN_NUM_LCD_PCLK,                                    
            .data2_io_num = QSPI_PIN_NUM_LCD_DATA2,                                     
            .data3_io_num = QSPI_PIN_NUM_LCD_DATA3,                                     
            .max_transfer_sz = DISPLAY_WIDTH * 80 * sizeof(uint16_t),                        
        };
        ESP_ERROR_CHECK(spi_bus_initialize(QSPI_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));
    }

    void Initializest77916Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        const esp_lcd_panel_io_spi_config_t io_config = {                                                           \
            .cs_gpio_num = QSPI_PIN_NUM_LCD_CS,                                      
            .dc_gpio_num = -1,                                      
            .spi_mode = 0,                                          
            .pclk_hz = 80 * 1000 * 1000,                            
            .trans_queue_depth = 10,                                
            .on_color_trans_done = NULL,                              
            .user_ctx = NULL,                                     
            .lcd_cmd_bits = 32,                                     
            .lcd_param_bits = 8,                                    
            .flags = {                                              
                .quad_mode = true,                                  
            },                                                      
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)QSPI_LCD_HOST, &io_config, &panel_io));
    
        st77916_vendor_config_t vendor_config = {
            .flags = {
                .use_qspi_interface = 1,
            },
        };
        vendor_config.init_cmds = vendor_specific_init_new;
        vendor_config.init_cmds_size = sizeof(vendor_specific_init_new) / sizeof(st77916_lcd_init_cmd_t);

        ESP_LOGD(TAG, "Install LCD driver");
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = QSPI_PIN_NUM_LCD_RST, // Shared with Touch reset
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = 16,
            .vendor_config = &vendor_config,
        };
        esp_lcd_new_panel_st77916(panel_io, &panel_config, &panel);

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);
        esp_lcd_panel_init(panel);
 
        display_ = new MayiS3LCDDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_16_4,
                                        .icon_font = &font_awesome_16_4,
                                        .emoji_font = font_emoji_64_init(),
                                    });
        display_->SetupHighTempWarningPopup();
    }

    ibeacon* GetIBeacon() override {
        return &ibeacon_;
    }

public:
    MayiS3LCDBoard() : boot_button_(BOOT_BUTTON_GPIO,false,3000), audio_codec_(CODEC_TX_GPIO, CODEC_RX_GPIO){    
        InitializePowerManager();
        InitializePowerSaveTimer();      
        InitializeButtons();
        InitializeSpi();
        Initializest77916Display();
        GetBacklight()->RestoreBrightness();
        // InitializeIot();

        audio_codec_.OnWakeUp([this](const std::string& command) {
            ESP_LOGI(TAG, "audio_codec_.OnWakeUp() command: %s", command.c_str());
            if (command == std::string(vb6824_get_wakeup_word())){
                if(Application::GetInstance().GetDeviceState() != kDeviceStateListening){
                    Application::GetInstance().WakeWordInvoke(command);
                }
            } else if (command == "开始配网") {
                ResetWifiConfiguration();
            }
        });
    }
    
    virtual AudioCodec* GetAudioCodec() override {
        return &audio_codec_;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(MayiS3LCDBoard);