#pragma once
#include <mbed.h>
#include <mtsas.h>

class MTSCellularManager {
public:
    MTSCellularManager(const char* apn_);
    ~MTSCellularManager();

    bool init();
    void uninit();

private:
    // An APN is required for GSM radios.
    const char* apn;
    /* 
     * The MTSSerialFlowControl object represents the physical serial link 
     * between the processor and the cellular radio.
     */
    mts::MTSSerialFlowControl* io;
    
    /* The Cellular object represents the cellular radio. */
    mts::Cellular* radio;
    mts::Cellular* radio2;
};
