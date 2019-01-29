#include <WiFi.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>

#include "ThingerCore32.h"
#include "ThingerWifiConfig.h"

#ifndef THINGER_DEVICE_SSID
    #define THINGER_DEVICE_SSID "CORE_32"
#endif

#ifndef THINGER_DEVICE_SSID_PSWD
    #define THINGER_DEVICE_SSID_PSWD "CORE_32"
#endif

class CaptiveRequestHandler : public AsyncWebHandler {
public:
    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}

    bool canHandle(AsyncWebServerRequest *request){
        Serial.println(request->url());
        //request->addInterestingHeader("ANY");
        return true;
    }

    void handleRequest(AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("text/html");
        response->print("<!DOCTYPE html><html><head><title>Captive Portal</title></head><body>");
        response->print("<p>This is out captive portal front page.</p>");
        response->printf("<p>You were trying to reach: http://%s%s</p>", request->host().c_str(), request->url().c_str());
        response->printf("<p>Try opening <a href='http://%s'>this link</a> instead</p>", WiFi.softAPIP().toString().c_str());
        response->print("</body></html>");
        request->send(response);
    }
};

void webConfigTask(void *pvParameters){
    const byte DNS_PORT = 53;
    IPAddress apIP(192, 168, 1, 1);
    DNSServer dnsServer;
    AsyncWebServer server(80);

    dnsServer.start(DNS_PORT, "*", apIP);

    WiFi.enableAP(true);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    String apName("CORE32_");
    String mac = WiFi.macAddress().substring(12);
    mac.remove(2,1);

    apName += mac;
    WiFi.softAP(apName.c_str());

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

    server.onNotFound([](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_OPTIONS) {
            AsyncWebServerResponse *response = request->beginResponse(200);
            response->addHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept, Authorization");
            response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS, DELETE");
            request->send(response);
        } else {
            request->send(404);
        }
    });

    //server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);//only when requested from AP

    server.serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html");


    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest * request) {
        request->redirect("/index.html");
    });

    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest * request) {
        request->redirect("/index.html");
    });

    server.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest * request) {
        request->redirect("/index.html");
    });

    server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
        DynamicJsonBuffer buffer;
        JsonArray& wifis = buffer.createArray();
        int n = WiFi.scanComplete();
        if(n == -2){
            THINGER_DEBUG("NETWORK", "Scanning Networks...");
            WiFi.scanNetworks(true);
        } else if(n){
            for (int i = 0; i < n; ++i){
                JsonObject& ssid = buffer.createObject();
                ssid["ssid"] = WiFi.SSID(i);
                ssid["rssi"] = WiFi.RSSI(i);
                ssid["bssid"] = WiFi.BSSIDstr(i);
                ssid["chan"] = WiFi.channel(i);
                ssid["auth"] = WiFi.encryptionType(i);
                wifis.add(ssid);
            }
            WiFi.scanDelete();
            if(WiFi.scanComplete() == -2){
                WiFi.scanNetworks(true);
            }
        }
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        wifis.printTo(*response);
        request->send(response);
    });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
        DynamicJsonBuffer buffer;
        JsonObject& ssid = buffer.createObject();
        ssid["ssid"] = WiFi.SSID();
        ssid["ip"] = WiFi.localIP().toString();
        ssid["netmask"] = WiFi.subnetMask().toString();
        ssid["gw"] = WiFi.gatewayIP().toString();
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        ssid.printTo(*response);
        request->send(response);
    });

    AsyncCallbackJsonWebHandler* connectHandler = new AsyncCallbackJsonWebHandler("/connect", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject& network = json.as<JsonObject>();
        THINGER_DEBUG_VALUE("NETWORK", "Received SSID: ", (const char*) network["ssid"]);
        THINGER_DEBUG_VALUE("NETWORK", "Received Password: ", (const char*) network["pswd"]);

        // add the network, so it will be available for the next reconnection
        bool result = WiFiConfig.add_network(network["ssid"], network["pswd"]);

        // send ok or error...
        request->send(result ? 200: 500);

        if(result){
            // disconnect...
            WiFiConfig.disconnect();

            // force device reconnection
            thing.reconnect();
        }
    });
    server.addHandler(connectHandler);

    AsyncCallbackJsonWebHandler* disconnectHandler = new AsyncCallbackJsonWebHandler("/disconnect", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject& jsonObj = json.as<JsonObject>();
        const String& ssid = jsonObj["ssid"];

        if(ssid==WiFi.SSID()){
            THINGER_DEBUG("NETWORK", "Disconnecting current Network...");
            WiFiConfig.disconnect();
            // TODO MOVE INSIDE DISCONNECT??
            thing.on_wifi_disconnected();
        }

        bool result = WiFiConfig.remove_network(jsonObj["ssid"]);
        request->send(result ? 200 : 404);
    });
    server.addHandler(disconnectHandler);

    server.begin();


    for(;;){
        dnsServer.processNextRequest();
        delay(1000);
    }
}