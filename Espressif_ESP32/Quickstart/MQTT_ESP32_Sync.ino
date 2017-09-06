/* License: MIT */
#include "WiFiClientSecure.h"
#include "certificates.hpp"
#include <PubSubClient.h>
#include <ArduinoJson.h>

/* LED Pin for your ESP32 */
#define LED_PIN 23

/* WiFi SSID and Password */
const char* ssid                        = "YOUR_SSID";
const char* password                    = "WIFI_PASSWORD";

/* 
 *  Sync Settings
 *  
 *  Enter your document's unique name and a device name.
 */
const char* sync_document               = "sync/docs/BoardLED";
const char* sync_device_name            = "ESP32_Dev_Board";

/* Sync server and MQTT setup; you probably don't have to change these. */
const char* mqtt_server                 = "mqtt-sync.us1.twilio.com";
const uint16_t mqtt_port                = 8883;
const uint16_t maxMQTTpackageSize       = 512;

WiFiClientSecure espClient;
PubSubClient client(mqtt_server, mqtt_port, espClient);


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
        String led_command           = root["led"];

        if (led_command == "ON") {
                digitalWrite(LED_PIN, HIGH);
                Serial.println("LED turned on!");
        } else {
                digitalWrite(LED_PIN, LOW);
                Serial.println("LED turned off!");
        }
}


/* 
 * This function connects to Sync via MQTT. We connect using the certificates and 
 * device name defined as constants above, and immediately check the server's 
 * certificate fingerprint.
 * 
 * If everything works, we subscribe to the document topic and return.
 */
void connect_mqtt() 
{
        while (!client.connected()) {
                Serial.println("Attempting to connect to Twilio Sync...");
                if (client.connect(sync_device_name)) {
                        Serial.print("Connected!  Subscribing to "); Serial.println(sync_document);
                        client.setCallback(callback);
                        client.subscribe(sync_document);
                } else {
                        Serial.print("failed, rc=");
                        Serial.print(client.state());
                        delay(10000);
                }
        }
}


/* In setup, we configure our LED for output, turn it off, and connect to WiFi */
void setup() 
{
        Serial.begin(115200);
        WiFi.begin(ssid, password);

        pinMode(LED_PIN, OUTPUT);
        digitalWrite(LED_PIN, LOW);

        while (WiFi.status() != WL_CONNECTED) {
                delay(1000);
                Serial.print(".");
        }

        randomSeed(micros());
        Serial.print("\nWiFi connected!  IP address: ");
        Serial.println(WiFi.localIP());

        // Set the root CA (to validate you are talking to Twilio) and the client key and cert
        espClient.setCACert(root_cert);
        espClient.setCertificate(client_cert);
        espClient.setPrivateKey(client_key);
}


/* Our loop constantly checks we are still connected.  On disconnects we try again. */
void loop() 
{
        if (!client.connected()) {
                connect_mqtt();
        }
        client.loop();
                
        // Here's an example of publishing from the ESP32.
        // Uncomment until the end of the function to send a 'msg' back to Sync
        // every 2 minutes!
        /*
        static uint32_t now = millis();
        if (millis() > now + (2*(1000*60))) {
                Serial.println("Sending 2 minute ON message to Twilio!");
                client.publish(
                        sync_document, 
                        "{\"msg\":\"Ahoy!\",\"led\":\"ON\"}\0"
                );
                now = millis();
        }
        */
}