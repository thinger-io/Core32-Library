#include <WiFi.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <FS.h>

#include "ThingerCore32.h"
#include "ThingerWifiConfig.h"

#define CONFIG_FILE "/networks.pson"

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
        String json = "[";
        int n = WiFi.scanComplete();
        if(n == -2){
            WiFi.scanNetworks(true);
        } else if(n){
            for (int i = 0; i < n; ++i){
                if(i) json += ",";
                json += "{";
                json += "\"ssid\":\""+WiFi.SSID(i)+"\"";
                json += ",\"rssi\":"+String(WiFi.RSSI(i));
                json += ",\"bssid\":\""+WiFi.BSSIDstr(i)+"\"";
                json += ",\"chan\":"+String(WiFi.channel(i));
                json += ",\"auth\":"+String(WiFi.encryptionType(i));
                json += "}";
            }
            WiFi.scanDelete();
            if(WiFi.scanComplete() == -2){
                WiFi.scanNetworks(true);
            }
        }
        json += "]";
        request->send(200, "application/json", json);
    });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{";
        json += "\"ssid\":\""+WiFi.SSID()+"\"";
        json += ",\"ip\":\""+WiFi.localIP().toString()+"\"";
        json += ",\"netmask\":\""+WiFi.subnetMask().toString()+"\"";
        json += ",\"gw\":\""+WiFi.gatewayIP().toString()+"\"";
        json += "}";
        request->send (200, "application/json", json);
    });

    AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/connect", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject& network = json.as<JsonObject>();
        THINGER_DEBUG_VALUE("NETWORK", "Received SSID: ", (const String&) network["ssid"]);
        THINGER_DEBUG_VALUE("NETWORK", "Received Password: ", (const String&) network["pswd"]);
        // add the network, so it will be available for the next reconnection
        WiFiConfig.add_network(network["ssid"], network["pswd"]);
        // send ok...
        request->send(200);
        // disconnect...
        WiFiConfig.disconnect();
        thing.on_wifi_disconnected();
    });
    server.addHandler(handler);

    AsyncCallbackJsonWebHandler* handlerm = new AsyncCallbackJsonWebHandler("/disconnect", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject& jsonObj = json.as<JsonObject>();
        const String& ssid = jsonObj["ssid"];

        if(ssid==WiFi.SSID()){
            THINGER_DEBUG("NETWORK", "Disconnecting current Network...");
            WiFiConfig.disconnect();
            thing.on_wifi_disconnected();
        }

        if(WiFiConfig.remove_network(jsonObj["ssid"])){
            request->send(200);
        }else{
            request->send(404);
        }

    });
    server.addHandler(handlerm);

    server.begin();


    for(;;){
        dnsServer.processNextRequest();
    }
}