#ifndef THINGER_WIFI_CONFIG_H
#define THINGER_WIFI_CONFIG_H

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <FS.h>
#include <esp_wifi.h>
#include "ThingerTaskController.h"

#define WIFI_CONNECTION_TIMEOUT 20000

class ThingerWiFiConfig{

public:
    ThingerWiFiConfig();

    ~ThingerWiFiConfig();

    /**
     * Return the number of configured wifi networks
     * @return
     */
    int networks_count();

    /**
     * Read the network at provided index, and store the result in the given parameters
     * @param index
     * @param ssid
     * @param pswd
     * @return
     */
    bool get_network(unsigned short index, String& ssid, String& pswd);

    /**
     * Read all configured networks and return the array
     * @param buffer
     * @return
     */
    JsonArray& get_networks(DynamicJsonBuffer& buffer);

    /**
     * Add a new network to the configuration. If the network already exists, it updates its password
     * @param ssid
     * @param pswd
     * @return
     */
    bool add_network(const String& ssid, const String& pswd);

    /**
     * Remove a network from the configuration given its SSID
     * @param ssid
     * @return
     */
    bool remove_network(const String& ssid);


    bool connected();

    bool disconnect();

    bool connect();

protected:

    /**
     * This method waits for WiFi connection
     * @param timeout
     * @return
     */
    bool wait_for_wifi(unsigned long timeout = WIFI_CONNECTION_TIMEOUT);



private:
    int available_networks_;
    int current_network_;
};

extern ThingerWiFiConfig WiFiConfig;

#endif