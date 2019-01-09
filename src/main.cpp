#include "ThingerCore32.h"
#include "ThingerTaskController.h"

#include <Update.h>
#include <SPIFFS.h>

// memory allocator for thinger
protoson::dynamic_memory_allocator alloc;
protoson::memory_allocator& protoson::pool = alloc;

// variables for measuring OTA
unsigned long startOTA, stopOTA = 0;

// ThingerCore32 instance
ThingerCore32 thing;

void initThingerOTA(){

    thing["_OTA"] >> [](pson& out){
        out["enabled"] = true;
    };

    thing["_BOTA"] = [](pson& in, pson& out){
        if(in.is_empty()){
            out["success"] = false;
            return;
        }

        String firmware = in["firmware"];
        String version = in["version"];
        size_t size = in["size"];

        THINGER_DEBUG("OTA", "Received OTA request...");
        THINGER_DEBUG_VALUE("OTA", "Firmware: ", firmware);
        THINGER_DEBUG_VALUE("OTA", "Size: ", size);
        THINGER_DEBUG("OTA", "Initializing update...");

        bool init = Update.begin(size);

        // try to end previous updates
        if(!init){
            THINGER_DEBUG("OTA", "Cannot Init... Clearing previous upgrade?");
            Update.abort();
            init = Update.begin(size);
        }

        THINGER_DEBUG_VALUE("OTA", "Init OK: ", init);
        out["success"] = init;

        // remove user task handle while upgrading OTA
        if(init)
        {
            TaskController.stopTask(TaskController.LOOP);
            THINGER_DEBUG("OTA", "Waiting for firmware...");
            out["block_size"] = 8192;
            startOTA = millis();
        }
    };

    thing["_WOTA"] = [](pson& in, pson& out){
        if(!in.is_bytes()){
            out["success"] = false;
            return;
        }

        // get buffer
        size_t size = 0;
        const void * buffer = NULL;
        in.get_bytes(buffer, size);

        THINGER_DEBUG_VALUE("OTA", "Received OTA part (bytes): ", size);

        // write buffer
        auto start = millis();
        size_t written = Update.write((uint8_t*) buffer, size);
        auto stop = millis();

        THINGER_DEBUG_VALUE("OTA", "Wrote OK: ", size==written);
        THINGER_DEBUG_VALUE("OTA", "Elapsed time ms: ", stop-start);
        THINGER_DEBUG_VALUE("OTA", "Remaining: ", Update.remaining());

        out["success"] = written==size;
        out["written"] = written;
    };

    thing["_WOTA2"] << [](pson& in){
        if(!in.is_bytes()){
            return;
        }

        // get buffer
        size_t size = 0;
        const void * buffer = NULL;
        in.get_bytes(buffer, size);

        THINGER_DEBUG_VALUE("OTA", "Received OTA part (bytes): ", size);

        // write buffer
        auto start = millis();
        size_t written = Update.write((uint8_t*) buffer, size);
        auto stop = millis();

        THINGER_DEBUG_VALUE("OTA", "Wrote OK: ", size==written);
        THINGER_DEBUG_VALUE("OTA", "Elapsed time ms: ", stop-start);
        THINGER_DEBUG_VALUE("OTA", "Remaining: ", Update.remaining());
    };

    thing["_EOTA"] >> [](pson& out){
        THINGER_DEBUG("OTA", "Finishing...");

        bool result = Update.end();
        stopOTA = millis();

        THINGER_DEBUG_VALUE("OTA", "Update OK: ", result);
        THINGER_DEBUG_VALUE("OTA", "Error: ", Update.getError());
        THINGER_DEBUG_VALUE("OTA", "Total Time: ", stopOTA-startOTA);

        Serial.println(stopOTA-startOTA);

        out["success"] = result;
    };

    thing["_reboot"] = [](){
        ESP.restart();
    };
}

void thingerTask(void* pvParameters){
    for(;;){
        thing.handle();
        yield();
    }
}

void loopTask(void *pvParameters){
    setup();
    for(;;){
        loop();
        yield();
    }
}

extern "C" void app_main()
{
    initArduino();
    initThingerOTA();

    SPIFFS.begin();
    Serial.begin(115200);

    THINGER_DEBUG("CORE32", "Debug Enabled");
    TaskController.startTask(TaskController.THINGER);
    TaskController.startTask(TaskController.LOOP);
    TaskController.startTask(TaskController.WEB_CONFIG);
}