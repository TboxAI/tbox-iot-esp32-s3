#ifndef WIFI_BOARD_H
#define WIFI_BOARD_H

#include "board.h"
#include "blufi_util.h"

class WifiBoard : public Board {
protected:
    bool wifi_config_mode_ = false;
    virtual std::string GetBoardJson() override;
    virtual void EnterWifiConfigMode();

public:
    WifiBoard();
    void OnBluFiEventCallback(blufi_event_t event, const char* info);
    virtual std::string GetBoardType() override;
    virtual void StartNetwork() override;
    virtual Http* CreateHttp() override;
    virtual WebSocket* CreateWebSocket() override;
    virtual Mqtt* CreateMqtt() override;
    virtual Udp* CreateUdp() override;
    virtual const char* GetNetworkStateIcon() override;
    virtual void SetPowerSaveMode(bool enabled) override;
    virtual void ResetWifiConfiguration();
    virtual AudioCodec* GetAudioCodec() override { return nullptr; }
    virtual std::string GetDeviceStatusJson() override;

private:
    volatile bool wifi_connected_;
    std::string ssid_;
    std::string password_;
};

#endif // WIFI_BOARD_H
