#ifndef THINGER_CONSOLE_H
#define THINGER_CONSOLE_H

#define THINGER_DO_NOT_INIT_MEMORY_ALLOCATOR
#include <Print.h>
#include <ThingerClient.h>

#define BUFFER_SIZE 128

class ThingerConsole : public Print{

public:
    ThingerConsole(ThingerClient& client);
    virtual ~ThingerConsole();
    virtual size_t write(uint8_t value);

private:
    ThingerClient& client_;
    size_t index_;
    uint8_t buffer_[BUFFER_SIZE+1];
};

extern ThingerConsole Console;

#endif