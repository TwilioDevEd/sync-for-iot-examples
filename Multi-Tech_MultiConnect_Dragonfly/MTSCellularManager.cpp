#include "MTSCellularManager.hpp"

MTSCellularManager::MTSCellularManager(
    const char* apn_
) :     apn(apn_), 
        io(NULL), 
        radio(NULL) 
{}


MTSCellularManager::~MTSCellularManager() {
    delete radio;
    delete io;
}

bool MTSCellularManager::init() {
    logInfo("Initializing Cellular Radio");

    io = new mts::MTSSerialFlowControl(
        RADIO_TX, 
        RADIO_RX, 
        RADIO_RTS, 
        RADIO_CTS
    );
    
    /* Radio default baud rate is 115200 */
    io->baud(115200);
    if (! io)
        return false;
    
    radio = mts::CellularFactory::create(io);
    radio2 = mts::CellularFactory::create(io);
    if (! radio)
        return false;
    
    /* 
     * Transport must be set properly before any TCPSocketConnection or 
     * UDPSocket objects are created
     */
    Transport::setTransport(radio);

    logInfo("setting APN");
    if (radio->setApn(apn) != MTS_SUCCESS) {
        logError("failed to set APN to \"%s\"", apn);
        return false;
    }

    logInfo("bringing up the link");
    if (! radio->connect()) {
        logError("failed to bring up the link");
        return false;
    } else {
        return true;
    }
}

void MTSCellularManager::uninit() {
    logInfo("finished - bringing down link");
    radio->disconnect();
    delete radio;
    delete io;
}