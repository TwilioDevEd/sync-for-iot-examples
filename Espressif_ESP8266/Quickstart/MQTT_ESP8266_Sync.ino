#include "ESP8266WiFi.h"
#include "WiFiClientSecure.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

/* WiFi SSID and Password */
const char* ssid                        = "YOUR_SSID";
const char* password                    = "WIFI_PASSWORD";

/*
 *  Sync Settings
 *
 *  Enter a Sync Key & Password, your document unique name,
 *  and the device name
 */
const char* sync_key                    = "KYXXXXXXXXXXXXXXXXXXXX";
const char* sync_password               = "XXXXXXXXXXXXXXXXXXXXXX";
const char* sync_document               = "sync/docs/BoardLED";
const char* sync_device_name            = "ESP8266";

/* Sync server and MQTT setup; you probably don't have to change these. */
const char* mqtt_server                 = "mqtt-sync.us1.twilio.com";
const uint16_t mqtt_port                = 8883;
const uint16_t maxMQTTpackageSize       = 512;

void callback(char *, byte*, unsigned int);
WiFiClientSecure espClient;
PubSubClient client(mqtt_server, mqtt_port, callback, espClient);

/*
 * Only use the fingerprint if you have a certificate rotation strategy in place.
 * It can be enabled by setting the use_fingerprint boolean to 'true'
 *
 * SHA1 Fingerprint valid as of August 2017, but if it expires:
 * On *NIX systems with openssl installed, you can check the fingerprint with
 *
 * echo | openssl s_client -connect mqtt-sync.us1.twilio.com:8883 | openssl x509 -fingerprint
 *
 * ... and look for the 'SHA1 Fingerprint' line.
 */
const char* fingerprint         = \
        "CE:7D:D3:B3:A0:73:BF:B6:90:B3:99:B7:7F:80:F6:81:16:F1:C0:78";
const bool use_fingerprint      = false;


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
                digitalWrite(BUILTIN_LED, LOW);
                Serial.println("LED turned on!");
        } else {
                digitalWrite(BUILTIN_LED, HIGH);
                Serial.println("LED turned off!");
        }
}

/*
 * This function connects to Sync via MQTT. We connect using the key, password, and
 * device name defined as constants above, and immediately check the server's
 * certificate fingerprint (if desired).
 *
 * If everything works, we subscribe to the document topic and return.
 */
void connect_mqtt()
{
        while (!client.connected()) {
                Serial.println("Attempting to connect to Twilio Sync...");
                if (client.connect(sync_device_name, sync_key, sync_password)) {
                        /* Verify you are talking to Twilio */
                        if (!use_fingerprint || espClient.verify(fingerprint, mqtt_server)) {
                                Serial.print("Connected!  Subscribing to "); Serial.println(sync_document);
                                client.subscribe(sync_document);
                        } else {
                                Serial.println("Certificate mismatch!  Check fingerprint.");
                                client.disconnect();
                                while(1);
                        }
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
        pinMode(BUILTIN_LED, OUTPUT);
        digitalWrite(BUILTIN_LED, HIGH); // Active LOW LED

        Serial.begin(115200);
        WiFi.begin(ssid, password);

        while (WiFi.status() != WL_CONNECTED) {
                delay(1000);
                Serial.print(".");
        }

        randomSeed(micros());

        Serial.print("\nWiFi connected!  IP address: ");
        Serial.println(WiFi.localIP());
}

/* Our loop constantly checks we are still connected.  On disconnects we try again. */
void loop()
{
        if (!client.connected()) {
                connect_mqtt();
        }
        client.loop();

        // Here's an example of publishing from the ESP8266.
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
