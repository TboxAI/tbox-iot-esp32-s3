#ifndef XIAOZHI_IBEACON_H
#define XIAOZHI_IBEACON_H
#include <functional>
#include "esp_timer.h"
using pairing_status_cb_t = std::function<void(bool)>;

class ibeacon {
  public:
    virtual ~ibeacon() {};
    virtual esp_err_t StartPairing() = 0;
    virtual esp_err_t StopPairing() = 0;
    virtual void OnPairingStatus(pairing_status_cb_t cb) = 0;
    /**
      *  @light_level: led lightness 0-127
      *  @mode: classic or rock(0,1)
      *  @color_change: whether need to change the RGB color.(0,1)
      */
    virtual bool ControlPeerDevice(uint8_t light_level, uint8_t mode, uint8_t color_change) = 0;
};

#endif //XIAOZHI_IBEACON_H