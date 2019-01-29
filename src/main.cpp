#include <Update.h>
#include <SPIFFS.h>

#include "ThingerCore32.h"
#include "ThingerTaskController.h"

// memory allocator for thinger
protoson::dynamic_memory_allocator alloc;
protoson::memory_allocator& protoson::pool = alloc;

// variables for measuring OTA
unsigned long startOTA, stopOTA = 0;

// ThingerCore32 instance
ThingerCore32 thing;

void initThingerOTA(){

    thing["CORE"] >> [](pson& out){
        out["version"] = "3.2";
    };

    thing["CORE"]["CLAIM"] = [](pson& in, pson& out){
        if(in.is_empty()){
            in["user"] = thing.get_user();
            in["device"] = thing.get_device();
            in["credential"] = thing.get_device_credential();
        }else{
            if(thing.has_credentials()){
                out["success"] = false;
                out["message"] = "device is already associated";
            }else{
                THINGER_DEBUG("_CORE32", "Claiming device...")
                THINGER_DEBUG_VALUE("_CORE32", "Received user: ", (const char*) in["user"])
                THINGER_DEBUG_VALUE("_CORE32", "Received device: ", (const char*) in["device"])
                THINGER_DEBUG_VALUE("_CORE32", "Received credentials: ", (const char*) in["credential"])
                bool result = thing.set_credentials(in["user"], in["device"], in["credential"]);
                out["success"] = result;
                if(result){
                    THINGER_DEBUG("_CORE32", "Done! The device will reconnect now with new credentials.")
                    out["message"] = "claim succeed!";
                    thing.stop();
                }else{
                    out["message"] = "cannot write credentials";
                    THINGER_DEBUG("_CORE32", "Error while writing credentials!")
                }
            }
        }
    };

    thing["CORE"]["UNCLAIM"] = [](){
        THINGER_DEBUG("_CORE32", "Unclaiming device...")
        if(thing.remove_credentials()){
            THINGER_DEBUG("_CORE32", "Done! The device will reconnect now with default credentials.")
            thing.stop();
        }else{
            THINGER_DEBUG("_CORE32", "Error while removing credentials!")
        }
    };

    thing["CORE"]["BOTA"] = [](pson& in, pson& out){
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

    thing["CORE"]["WOTA"] = [](pson& in, pson& out){
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

    thing["CORE"]["WOTA2"] << [](pson& in){
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

    thing["CORE"]["EOTA"] >> [](pson& out){
        THINGER_DEBUG("OTA", "Finishing...");

        bool result = Update.end();
        stopOTA = millis();

        THINGER_DEBUG_VALUE("OTA", "Update OK: ", result);
        THINGER_DEBUG_VALUE("OTA", "Error: ", Update.getError());
        THINGER_DEBUG_VALUE("OTA", "Total Time: ", stopOTA-startOTA);

        Serial.println(stopOTA-startOTA);

        out["success"] = result;
    };

    thing["CORE"]["TASKS"] << [](pson& in){
        if(in.is_empty()){
            in[TaskController.taskName(TaskController.WEB_CONFIG)] = TaskController.isRunning(TaskController.WEB_CONFIG);
            in[TaskController.taskName(TaskController.LOOP)] = TaskController.isRunning(TaskController.LOOP);
            in[TaskController.taskName(TaskController.THINGER)] = TaskController.isRunning(TaskController.THINGER);
        }else{
            TaskController.setTaskState(TaskController.WEB_CONFIG, in[TaskController.taskName(TaskController.WEB_CONFIG)]);
            TaskController.setTaskState(TaskController.LOOP, in[TaskController.taskName(TaskController.LOOP)]);
            TaskController.setTaskState(TaskController.THINGER, in[TaskController.taskName(TaskController.THINGER)]);
        }
    };

    thing["CORE"]["REBOOT"] = [](){
        ESP.restart();
    };
}

void thingerTask(void* pvParameters){
    for(;;){
        thing.handle();
    }
}

void loopTask(void *pvParameters){
    setup();
    for(;;){
        loop();
    }
}

extern "C" void app_main(){
    initArduino();
    initThingerOTA();

#ifdef _DEBUG_
    Serial.begin(115200);
    THINGER_DEBUG("CORE32", "Debug Enabled");
#endif

    // TODO review to remove true in production.. it may erase all files if begin fails in some way
    if(!SPIFFS.begin(true)){
        THINGER_DEBUG("CORE32", "Error while initializing Filesystem! Stopping...");
        return;
    }

    TaskController.startTask(TaskController.LOOP);
    TaskController.startTask(TaskController.THINGER);
    //TaskController.startTask(TaskController.WEB_CONFIG);
}