#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <soc/efuse_reg.h>
#include <base64.h>

#include "ThingerCore32.h"
#include "ThingerWifiConfig.h"
#include "ThingerTaskController.h"

#define DEVICE_CONFIG_FILE "/cfg/device.json"
#define USER_FIELD "user"
#define DEVICE_FIELD "device"
#define CREDENTIAL_FIELD "credential"

ThingerCore32::ThingerCore32() :
        ThingerClient(client_, user, device, device_credential),
        initialized_(false)
{
    // initialize empty configuration
    user[0] = '\0';
    device[0] = '\0';
    device_credential[0] = '\0';
    semaphore_ = xSemaphoreCreateMutex();
}

ThingerCore32::~ThingerCore32(){

}

bool ThingerCore32::load_credentials() {
    DynamicJsonBuffer buffer;

    // read current credentials
    File configFile = SPIFFS.open(DEVICE_CONFIG_FILE, FILE_READ);
    if(configFile){
        THINGER_DEBUG("_CORE32", "Reading device config...");

        // read config file credentials
        JsonObject& coreConfig = buffer.parseObject(configFile);
        configFile.close();
        if(coreConfig.success()){
            // copy contents
            strcpy(user, coreConfig[USER_FIELD]);
            strcpy(device, coreConfig[DEVICE_FIELD]);
            strcpy(device_credential, coreConfig[CREDENTIAL_FIELD]);
            THINGER_DEBUG("_CORE32", "OK!");
            return true;
        }
    }

    THINGER_DEBUG("_CORE32", "No device config available. Loading default core credentials...");

    char identifier[129];
    sprintf(identifier,"%08X%08X%08X%08X",
            REG_READ(EFUSE_BLK3_RDATA0_REG),
            REG_READ(EFUSE_BLK3_RDATA1_REG),
            REG_READ(EFUSE_BLK3_RDATA2_REG),
            REG_READ(EFUSE_BLK3_RDATA3_REG));

    char credential[65];
    sprintf(credential,"%08X%08X",
            REG_READ(EFUSE_BLK3_RDATA4_REG),
            REG_READ(EFUSE_BLK3_RDATA5_REG));

    THINGER_DEBUG_VALUE("_CORE32", "User: ", "Core32");
    THINGER_DEBUG_VALUE("_CORE32", "Core ID: ", identifier);
    THINGER_DEBUG_VALUE("_CORE32", "Core Credential: ", credential);

    strcpy(user, "Core32");
    strcpy(device, identifier);
    strcpy(device_credential, credential);

    return true;
}

bool ThingerCore32::has_credentials(){
    return SPIFFS.exists(DEVICE_CONFIG_FILE);
}

bool ThingerCore32::set_credentials(const char* username_, const char* device_, const char* device_credential_) {
    File configFile = SPIFFS.open(DEVICE_CONFIG_FILE, FILE_WRITE);
    if (!configFile) return false;

    // create json and save to file
    DynamicJsonBuffer buffer;
    JsonObject& credential = buffer.createObject();
    credential[USER_FIELD] = username_;
    credential[DEVICE_FIELD] = device_;
    credential[CREDENTIAL_FIELD] = device_credential_;
    credential.printTo(configFile);
    configFile.close();

    return load_credentials();
}

bool ThingerCore32::remove_credentials() {
    if(!SPIFFS.remove(DEVICE_CONFIG_FILE)) return false;
    return load_credentials();
}

bool ThingerCore32::lock(){
    return xSemaphoreTake(semaphore_, ( TickType_t ) 10);
}

bool ThingerCore32::unlock(){
    return xSemaphoreGive(semaphore_);
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
    ThingerClient::stop();
}

void ThingerCore32::reconnect(){
    ThingerClient::stop();
}