#include <ArduinoJson.h>
#include <MQTT-TLS.h>
#include "certificate.h"

/*  License: MIT
 *  Sync Settings
 *  
 *  Enter a Sync Key & Password, your document unique name, 
 *  and a device name.
 */
const char* sync_key                    = "KYxxxxxxxxxxxx";
const char* sync_password               = "SECRET_PASSWORD_HERE";
const char* sync_document               = "sync/docs/BoardLED";
const char* sync_device_name            = "Particle_Electron";

/* LED Setting - on the Electron, the LED is Pin 7 */
const uint8_t LED_PIN                    = 7;

/* Sync server and MQTT setup; you probably don't have to change these. */
char* mqtt_server                       = "mqtt-sync.us1.twilio.com";
uint16_t mqtt_port                      = 8883;
const uint16_t maxMQTTpackageSize       = 512;

void callback(char* topic, byte* payload, unsigned int length);
MQTT client(
    mqtt_server, 
    mqtt_port, 
    callback
);

/* 
 * Our Twilio Connected Devices message handling callback.  This is passed as a 
 * callback function when we subscribe to the document, and any messages will 
 * appear here.
 */
void callback(char* topic, byte* payload, unsigned int length) 
{
        std::unique_ptr<char []> msg(new char[length+1]());
        memcpy (msg.get(), payload, length);

        Serial.print("Message arrived on topic "); Serial.println(msg.get());
        
        StaticJsonBuffer<maxMQTTpackageSize> jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(msg.get());
        const char* led_command_raw = root["led"];
        String led_command(led_command_raw);

        if (led_command == "ON") {
                digitalWrite(LED_PIN, HIGH);
                Serial.println("LED turned on!");
        } else {
                digitalWrite(LED_PIN, LOW);
                Serial.println("LED turned off!");
        }
}

/* 
 * This function connects to Sync via MQTT. We connect using the device keys
 * from above, and immediately check the server's certificate fingerprint.
 * 
 * If everything works, we subscribe to the document topic and return.
 */
void connect_mqtt()
{
    // Ensure we are talking to Twilio
    client.enableTls(root_ca, sizeof(root_ca));
    
    // Connect to Sync
    Serial.println("Connecting to Sync...");
    Serial.println(client.connect(
        "Particle_Electron",
        sync_key,
        sync_password
    ));
    
    if (client.isConnected()) {
        client.subscribe(sync_document);
        Serial.print("Subscribed to "); Serial.println(sync_document);
    }
}

/* In setup, we configure our LED, enabled serial, and connect to Sync */
void setup() 
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    Serial.begin(115200);
    connect_mqtt();
}

/* 
 * Our loop constantly checks we are still connected and runs the 
 * heartbeat/yield function.  
 */
void loop() 
{
    if (client.isConnected()) {
        client.loop();
    } else {
        delay(1000);
        connect_mqtt();
    }
    delay(1000);
    // Here's an example of publishing from the Particle Electron.
    // Uncomment until the end of the function to send a 'msg' back to Sync
    // every 2 minutes!

    static uint32_t now = millis();
    if (millis() > now + (2*(1000*60))) {
        Serial.println("Sending 2 minute ON message to Twilio!");
        client.publish(
            sync_document, 
            "{\"msg\":\"Ahoy World!\",\"led\":\"ON\"}"
        );
        now = millis();
    }
}