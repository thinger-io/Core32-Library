#ifndef THINGER_CORE_32_H
#define THINGER_CORE_32_H

#define _DEBUG_
#define THINGER_DO_NOT_INIT_MEMORY_ALLOCATOR

#include <WiFiClientSecure.h>
#include <ThingerWifi.h>
#include <vector>

class ThingerCore32 : public ThingerClient {

public:
    ThingerCore32() : ThingerClient(client_, user, device, device_credential),
        initialized_(false)
    {
        // initialize empty configuration
        user[0] = '\0';
        device[0] = '\0';
        device_credential[0] = '\0';
    }

    ~ThingerCore32(){

    }

    void on_wifi_disconnected();

protected:
    virtual bool network_connected();
    virtual bool connect_network();

private:

    bool load_credentials();

#ifndef _DISABLE_TLS_
    WiFiClientSecure client_;
#else
    WiFiClient client_;
#endif

    bool initialized_;
    char user[40];
    char device[40];
    char device_credential[40];
};

extern ThingerCore32 thing;

#endif