#include "ThingerWifiConfig.h"

ThingerWiFiConfig WiFiConfig;

#define NETWORKS_FILE "/config/networks.json"


ThingerWiFiConfig::ThingerWiFiConfig() :
        available_networks_(-1),
        current_network_(-1)
{

}

ThingerWiFiConfig::~ThingerWiFiConfig(){

}

int ThingerWiFiConfig::networks_count(){
    if(available_networks_!=-1) return available_networks_;
    DynamicJsonBuffer buffer;
    JsonArray& networks = get_networks(buffer);
    available_networks_ = networks.size();
    return available_networks_;
}

bool ThingerWiFiConfig::get_network(unsigned short index, String& ssid, String& pswd) {
    DynamicJsonBuffer buffer;
    JsonArray& networks = get_networks(buffer);
    if(index >= networks.size()) return false;
    ssid = (const String&) networks[index]["ssid"];
    pswd = (const String&) networks[index]["pswd"];
    return true;
}

JsonArray& ThingerWiFiConfig::get_networks(DynamicJsonBuffer& buffer) {
    File configFile = SPIFFS.open(NETWORKS_FILE, "r");
    if (!configFile) return buffer.createArray();
    JsonArray& networks = buffer.parseArray(configFile);
    configFile.close();
    //networks.printTo(Serial);
    return networks.success() ? networks : buffer.createArray();
}

bool ThingerWiFiConfig::add_network(const String& ssid, const String& pswd) {
    THINGER_DEBUG_VALUE("NETWORK", "Setting Network: ", ssid);

    DynamicJsonBuffer buffer;
    JsonArray& networks = get_networks(buffer);
    THINGER_DEBUG("NETWORK", "Current networks:");
#ifdef _DEBUG_
    networks.prettyPrintTo(Serial);
#endif

    // open file for writing
    File writeFile = SPIFFS.open(NETWORKS_FILE, "w");
    if(!writeFile) return false;

    bool update = false;
    for(auto & network : networks){
        if(network["ssid"]==ssid){
            THINGER_DEBUG("NETWORK", "Updating WiFi!");
            network["pswd"] = pswd;
            update = true;
            break;
        }
    }

    if(!update){
        JsonObject& network = buffer.createObject();
        network["ssid"] = ssid;
        network["pswd"] = pswd;
        networks.add(network);
        available_networks_++;
        current_network_ = available_networks_;
    }

    THINGER_DEBUG("NETWORK", "Writing to file...");
    networks.printTo(writeFile);
    writeFile.close();

    THINGER_DEBUG("NETWORK", "Updated WiFi Networks:");
#ifdef _DEBUG_
    networks.prettyPrintTo(Serial);
#endif
    return true;
}

bool ThingerWiFiConfig::remove_network(const String& ssid) {
    THINGER_DEBUG_VALUE("NETWORK", "Removing Network: ", ssid);

    DynamicJsonBuffer buffer;
    JsonArray& networks = get_networks(buffer);
    THINGER_DEBUG("NETWORK", "Current networks:");
#ifdef _DEBUG_
    networks.prettyPrintTo(Serial);
#endif

    bool modified = false;
    for(auto i=0; i<networks.size() && !modified; i++){
        if(networks[i]["ssid"]==ssid){
            networks.remove(i);
            modified = true;
        }
    }

    if(!modified){
        THINGER_DEBUG("NETWORK", "Network not found!");
        return false;
    }

    File writeFile = SPIFFS.open(NETWORKS_FILE, "w");
    if(!writeFile) return false;
    networks.printTo(writeFile);
    writeFile.close();

    available_networks_--;
    current_network_ = available_networks_;

    THINGER_DEBUG("NETWORK", "Updated WiFi Networks:");
#ifdef _DEBUG_
    networks.prettyPrintTo(Serial);
#endif
    return true;

}

bool ThingerWiFiConfig::wait_for_wifi(unsigned long timeout){
    unsigned long wifi_timeout = millis();
    while(WiFi.status() != WL_CONNECTED){
        if(millis() - wifi_timeout > timeout){
            THINGER_DEBUG("NETWORK", "Cannot connect to WiFi!");
            return false;
        }
        delay(100);
    }
    THINGER_DEBUG("NETWORK", "Connected to WiFi! Getting IP Address...");

    wifi_timeout = millis();
    while(WiFi.localIP() == INADDR_NONE){
        if(millis() - wifi_timeout > timeout){
            THINGER_DEBUG("NETWORK", "Cannot get IP Address!");
            return false;
        }
        delay(100);
    }
    THINGER_DEBUG_VALUE("NETWORK", "Got IP Address: ", WiFi.localIP());
    return true;
}

bool ThingerWiFiConfig::connect(const char* ssid, const char* password){
    THINGER_DEBUG_VALUE("NETWORK", "Connecting to network: ", ssid);
    THINGER_DEBUG_VALUE("NETWORK", "Connecting with password: ", password);
    //WiFi.persistent(false);
    WiFi.disconnect();
    //delay(100);
    //WiFi.config({ 0,0,0,0 }, { 0,0,0,0 }, { 0,0,0,0 }, { 0,0,0,0 });
    //WiFi.enableSTA(true);
    //delay(100);
    WiFi.begin(ssid, password);
    return wait_for_wifi();
}

bool ThingerWiFiConfig::connected(){
    return WiFi.status() == WL_CONNECTED && WiFi.localIP()!=INADDR_NONE;
}

bool ThingerWiFiConfig::disconnect(){
    return WiFi.disconnect();
}

bool ThingerWiFiConfig::onnect(){
    // try to connect to previous known WiFi in the first attempt
    if(current_network_ == -1){
        WiFi.enableSTA(true);
        wifi_config_t conf;
        esp_wifi_get_config(WIFI_IF_STA, &conf);
        const char* ssid = reinterpret_cast<const char*>(conf.sta.ssid);
        const char* password = reinterpret_cast<const char*>(conf.sta.password);
        if(ssid!=NULL && strlen(ssid)>0){
            THINGER_DEBUG("NETWORK", "Connecting to previous network...");
            if(connect(ssid, password)) return true;
        }
    }

    // still not connected
    if(current_network_ == -1){
        // iterate over known networks
        int available_networks = networks_count();
        if(available_networks<=0){
            THINGER_DEBUG("NETWORK", "No networks available...");
            if(!TaskController.isTaskRunning(TaskController.WEB_CONFIG)){
                TaskController.startTask(TaskController.WEB_CONFIG);
                TaskController.stopTask(TaskController.THINGER);
            }
            return false;
        }
        current_network_ = available_networks;
    }

    THINGER_DEBUG_VALUE("NETWORK", "Networks available: ", available_networks_);

    if(available_networks_ == 0) return false;

    if(current_network_==0) current_network_ = available_networks_;
    THINGER_DEBUG_VALUE("NETWORK", "Using network at index: ", current_network_-1);

    String ssid, pswd;
    if(!get_network(--current_network_, ssid, pswd)) return false;

    return connect(ssid.c_str(), pswd.c_str());
}