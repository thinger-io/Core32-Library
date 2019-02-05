#ifndef THINGER_CORE_32_H
#define THINGER_CORE_32_H

#define THINGER_DO_NOT_INIT_MEMORY_ALLOCATOR

#include <WiFiClientSecure.h>
#include <ThingerWifi.h>

class ThingerCore32 : public ThingerClient {

public:
    ThingerCore32();

    ~ThingerCore32(){

    }

    void on_wifi_disconnected();


    /**
     * Method that will force a reconnect to the server
     */
    void reconnect();

    /**
     * Get the current user
     * @return
     */
    const char* get_user(){
        return user;
    }

    /**
     * Get the current device id
     * @return
     */
    const char* get_device(){
        return device;
    }

    /**
     * Get the current device credential
     * @return
     */
    const char* get_device_credential(){
        return device_credential;
    }

    /**
     * Load device credentials, from file to memory
     * @return
     */
    bool load_credentials();

    /**
     * Check if the device has configured credentials
     * @return
     */
    bool has_credentials();

    /**
     * Modify the device credentials, both in memory and file
     * @param user
     * @param device
     * @param credentials
     * @return
     */
    bool set_credentials(const char* user, const char* device, const char* credentials);

    /**
     * Remove current credentials
     * @param user
     * @param device
     * @param credentials
     * @return
     */
    bool remove_credentials();


    /**
     * Lock the usage of client object
     * @return true if it safe to use the client object
     */
    bool lock();

    /**
     * Unlock the usage of client object
     * @return true if the client was released
     */
    bool unlock();


protected:

    /**
     * Implementation for ThingerClient class, that will check if the device is connected to network
     * @return
     */
    virtual bool network_connected();

    /**
     * Implementation for ThingerClient class, that will connect to the network
     * @return
     */
    virtual bool connect_network();


private:

#ifndef _DISABLE_TLS_
    WiFiClientSecure client_;
#else
    WiFiClient client_;
#endif

    bool initialized_;
    char user[40];
    char device[40];
    char device_credential[40];
    SemaphoreHandle_t semaphore_;
};

extern ThingerCore32 thing;

#endif