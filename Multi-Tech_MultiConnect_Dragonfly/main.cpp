#include <mbed.h>
#include <mtsas.h>
#include <ssl.h>
#include <MbedJSONValue.h>
#include <string>
#include "MTSCellularManager.hpp"
#include "TlsMQTTClient.hpp"
#include "certificates.hpp"

/* 
 *  Sync Settings
 *  
 *  Enter a Sync Key & Password, your document unique name, 
 *  and the device name
 */
char* sync_key                          = "KYXXXXXXXXXXXXXXXXXXXX";
char* sync_password                     = "SECRET_HERE";
char* sync_document                     = "sync/docs/BoardLED";
char* sync_device_name                  = "MultiConnect Dragonfly";

/* Sync server and MQTT setup; you probably don't have to change these. */
const char* mqtt_server                 = "mqtt-sync.us1.twilio.com";
const uint16_t mqtt_port                = 8883;
const uint16_t maxMQTTpackageSize       = 512;

TlsMQTTClient client = TlsMQTTClient();
DigitalOut led(D7);
DigitalOut bc_nce(PB_2);
const uint8_t MQTT_HEARTBEAT = 15;

/* 
 * Our Twilio Connected Devices message handling callback.  This is passed as a 
 * callback function when we subscribe to the document, and any messages will 
 * appear here.
 */
void callback(MQTT::MessageData& data) 
{
    if (data.message.payloadlen > maxMQTTpackageSize) {
        return;
    }
    char buf[maxMQTTpackageSize + 1];
    strncpy(buf, (char*)data.message.payload, data.message.payloadlen);
    buf[data.message.payloadlen] = '\0';
    
    logDebug("Received new update %s", buf);
    /* JSON Parse 'led' */
    MbedJSONValue parser;
    parse(parser, buf);
    
    /* The parser will segfault and reset the board if "led" isn't contained. */
    if (parser.hasMember("led")) {
        std::string led_str;
        led_str = parser["led"].get<std::string>();
        
        if (led_str.compare("ON") == 0) {
            logDebug("Turning LED ON");
            led = 0; // Active LOW
        } else {
            logDebug("Turning LED OFF");
            led = 1; // Active LOW
        }
    }
}


/* 
 * This function connects to Sync via MQTT. We connect using the key, password, 
 * and device name defined as constants above, and checks the server
 * certificate.
 * 
 * If everything works, we subscribe to the document topic and return.
 */
void connect_mqtt()
{
    MQTTPacket_connectData conn_data = MQTTPacket_connectData_initializer;
    conn_data.clientID.cstring = sync_device_name;
    conn_data.username.cstring = sync_key;
    conn_data.password.cstring = sync_password;
    int rc = client.connect(
        mqtt_server, 
        mqtt_port, 
        MQTT_GATEWAY_PROD_ROOT_CA_PEM, 
        conn_data
    );
    logInfo("MQTT connect result: %d", rc);
    
    rc = client.subscribe(
        "sync/docs/BoardLED", 
        MQTT::QOS1, 
        callback
    );
    logInfo("MQTT subscription result: %d", rc);
}


/* 
 * Very basic device loop - all we do is reconnect when disconnected
 */
void loop()
{
    if (client.isConnected()) {
        client.yield(MQTT_HEARTBEAT/2.0);
        wait(MQTT_HEARTBEAT);
    } else {
        wait(MQTT_HEARTBEAT*10);
        connect_mqtt();
    }
    
    // Here's an example of publishing from the MultiConnect Dragonfly.
    // Uncomment until the end of the function to send a 'msg' back to Sync
    // every 5 cycles through the loop! (5*MQTT_HEARTBEAT seconds)
    /*
    static uint32_t ticks = 0;
    MQTT::Message message;
    char buf[maxMQTTpackageSize] = "{\"msg\":\"Ahoy!\",\"led\":\"ON\"}";
    message.qos = MQTT::QOS1;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf) + 1;
    if (ticks++ > 4) {
            logInfo("Sending ON message to Twilio!");
            client.publish(
                    sync_document, 
                    message
            );
            ticks = 0;
    }
    */
}


/* 
 * In main, we configure our LEDs, connect to Twilio Programmable Wireless,
 * and initialize CyaSSL. We then connect our MQTT client for the first time.
 *
 * When done, we pass control to the MQTT loop, which handles yield()s.
 */
int main() 
{
    led = 1;  // Active LOW
    bc_nce = 1;
    Serial debug(USBTX, USBRX);
    debug.baud(115200);
    mts::MTSLog::setLogLevel(mts::MTSLog::TRACE_LEVEL);

    // Be sure your SIM is registered in the Twilio console.
    // https://www.twilio.com/console/wireless/sims/
    logInfo("Initializing Twilio Programmable Wireless");
    MTSCellularManager cellularManager("wireless.twilio.com");   
    if (! cellularManager.init()) {
        while (true) {
            logError("failed to initialize cellular radio"); wait(10);  
        }
    }
    CyaSSL_Init();
    connect_mqtt();

    /* We're done; pass off control to the loop */
    while (1) {
       loop();
    }
}