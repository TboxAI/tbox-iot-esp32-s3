#ifndef XIAOZHI_BT42IBeacon_H
#define XIAOZHI_BT42IBeacon_H
#include "esp_timer.h"
#include <cstdint>
#include "ibeacon.h"

class BT42IBeacon : public ibeacon {
    public:
    BT42IBeacon();
    ~BT42IBeacon();
    virtual esp_err_t StartPairing() override;
    virtual esp_err_t StopPairing() override;
    virtual void OnPairingStatus(pairing_status_cb_t cb) override;

    /**
      *  @light_level: led lightness 0-127
      *  @mode: classic or rock(0,1)
      *  @color_change: whether need to change the RGB color.(0,1)
      */
    virtual bool ControlPeerDevice(uint8_t light_level, uint8_t mode, uint8_t color_change) override;

    private:
    /**
     * @brief C 风格的回调函数 "trampoline"。
     * 
     * esp_timer 会调用这个静态函数，我们将 this 指针作为参数传递给它，
     * 然后它会调用我们真正的成员函数 onPairingTimer()。
     * 
     * @param arg 指向 IBeacon 实例的指针 (this)。
     */
    static void pairing_timer_callback_trampoline(void* arg);
    static void fcble_event_notify_cb(uint8_t type, uint8_t * data, uint8_t len, void *ctx);

    /**
     * @brief 定时器到期时要执行的实际处理函数。
     * 
     * 这是每 15ms 被调用的目标函数。
     */
    
    void OnPairingTimer();
    void OnFcBleEventNotify(uint8_t type, uint8_t * data, uint8_t len);
    esp_timer_handle_t pairing_timer_handle_; // 定时器句柄
    pairing_status_cb_t pairing_status_cb_; // 配对状态回调
    int timer_count_ = 0;
};

#endif //XIAOZHI_BT42IBeacon_H