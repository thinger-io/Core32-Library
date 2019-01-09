#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <FS.h>

#include "ThingerCore32.h"
#include "ThingerWifiConfig.h"

bool ThingerCore32::load_credentials() {
    File configFile = SPIFFS.open("/config/credentials.json", "r");
    if (!configFile) return false;
    DynamicJsonBuffer buffer;
    JsonObject& coreConfig = buffer.parseObject(configFile);
    if (!coreConfig.success()) return false;
    configFile.close();
    strcpy(user, coreConfig["user"]);
    strcpy(device, coreConfig["device"]);
    strcpy(device_credential, coreConfig["credential"]);
    return true;
}

bool ThingerCore32::network_connected(){
    return WiFiConfig.connected();
}

bool ThingerCore32::connect_network(){
    if(!initialized_){
        if(load_credentials()){
            initialized_ = true;
            THINGER_DEBUG("_CONFIG", "Loaded Initial Config");
        }else{
            THINGER_DEBUG("_CONFIG", "Cannot Read Config!");
        }
    }

    return initialized_ && WiFiConfig.connect();
}

void ThingerCore32::on_wifi_disconnected(){
    ThingerClient::disconnected();
}