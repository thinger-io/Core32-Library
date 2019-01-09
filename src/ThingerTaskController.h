#ifndef THINGER_TASK_CONTROLLER_H
#define THINGER_TASK_CONTROLLER_H

#include <FreeRTOS.h>
#include "ThingerCore32.h"

extern void thingerTask(void*);
extern void webConfigTask(void*);
extern void loopTask(void*);

class ThingerTaskController{

public:

    enum Task{
        THINGER = 0,
        LOOP = 1,
        WEB_CONFIG = 2
    };

private:

    typedef struct{
        Task task_;
        const char* taskName_;
        void (*taskFunction_)(void*);
        TaskHandle_t taskHandler_ = NULL;

        void set(Task task, const char* taskName, void (*taskFunction)(void*)){
            task_ = task;
            taskName_ = taskName;
            taskFunction_ = taskFunction;
        }

    } TaskDefinition;

public:

    ThingerTaskController()
    {
        taskHandlers_[THINGER].set(THINGER, "ThingerTask", thingerTask);
        taskHandlers_[LOOP].set(LOOP, "LoopTask", loopTask);
        taskHandlers_[WEB_CONFIG].set(WEB_CONFIG, "WebConfigTask", webConfigTask);
    }

    bool startTask(Task task){
        TaskDefinition& taskDef = taskHandlers_[task];
        if(taskDef.taskHandler_==NULL){
            THINGER_DEBUG_VALUE("CORE_OS", "Starting Task: ", taskDef.taskName_);
            auto cpu = 1; //task == WEB_CONFIG ? 0 : 1;
            xTaskCreatePinnedToCore(taskDef.taskFunction_, taskDef.taskName_, 8192, NULL, 1, &(taskDef.taskHandler_), cpu);
            return true;
        }
        return false;
    }

    bool stopTask(Task task){
        TaskDefinition& taskDef = taskHandlers_[task];
        if(taskDef.taskHandler_!=NULL){
            THINGER_DEBUG_VALUE("CORE_OS", "Stopping Task: ", taskDef.taskName_);
            vTaskDelete( taskDef.taskHandler_ );
            taskDef.taskHandler_ = NULL;
            return true;
        }
        return false;
    }

    bool isTaskRunning(Task task){
        return taskHandlers_[task].taskHandler_ != NULL;
    }

private:
    TaskDefinition taskHandlers_[3];
};

extern ThingerTaskController TaskController;

#endif