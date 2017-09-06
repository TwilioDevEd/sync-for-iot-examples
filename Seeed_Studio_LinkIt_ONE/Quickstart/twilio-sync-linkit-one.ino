/*
 * Subscribe to a Twilio Sync Deployed Devices Document using a LinkIt ONE
 * over WiFi or Twilio Wireless with GPRS.

 * This code owes much thanks to MediaTek Labs, who have a skeleton up here:
 * 
 * https://github.com/MediaTek-Labs/aws_mbedtls_mqtt
 *
 * You'll need to install it to run our code.
 * 
 * License: This code, MIT, AWS Code: Apache (http://aws.amazon.com/apache2.0)
 */
#include <ArduinoJson.h>
#include <LTask.h>
#include <LWiFi.h>
#include <LWiFiClient.h>
#include <LGPRS.h>
#include "SyncLinkItHelper.hpp"

/* CONFIGURATION:
 *  
 * This should point at the ROOT Certificate, CLIENT Certificate,
 * and unencrypted CLIENT Key.  You can unencrypt it with (*NIX):
 *
 * openssl rsa -in  CYxxxxxxxxxx.key -out client.key
 * 
 * Name the three files root.crt, root.crt, and client.key
 * 
 * Then, flip the outer switch to 'MS' and upload the three to
 * the LinkIt's root directory.
 */
String           SYNC_ROOT_CA_FILENAME        = "root.crt";
String           SYNC_CERTIFICATE_FILENAME    = "client.crt";
String           SYNC_PRIVATE_KEY_FILENAME    = "client.key";

/* Should we use WiFi or GPRS?  'true' for WiFi, 'false' for GPRS */
boolean          WIFI_USED                    = true;

/* 
 *  Optional Settings.  
 *  You probably do not need to change these unless you change the Doc name. 
 */
const String twilio_topic                     = "sync/docs/BoardLED";
const int mqtt_tls_port                       = 8883;
String SYNC_MQTT_HOST                         = "mqtt-sync.us1.twilio.com";
String SYNC_MQTT_CLIENT_ID                    = "LinkIt_ONE";
const uint8_t LED_BUILTIN                     = 13;

/* Friendly WiFi Network details go here.  Auth choices:
 * LWIFI_OPEN, LWIFI_WPA, LWIFI_WEP
 */
#define WIFI_AP "YOUR_SSID"
#define WIFI_PASSWORD "WIFI_PASSWORD"
#define WIFI_AUTH LWIFI_WPA

/* 
 *  Twilio GPRS Settings here - you should not have to change these to 
 *  use a Twilio Programmable Wireless SIM Card.  Make sure your card 
 *  is registered, provisioned, and activated if you have issues.
 */
#define GPRS_APN "wireless.twilio.com"
#define GPRS_USERNAME NULL
#define GPRS_PASSWORD NULL

typedef int32_t mqtt_callback(MQTTCallbackParams);

// Global Twilio Lambda helper
void disconnect_function();
SyncLinkItHelper helper(
        mqtt_tls_port,
        SYNC_MQTT_HOST,
        SYNC_MQTT_CLIENT_ID,
        SYNC_ROOT_CA_FILENAME,
        SYNC_CERTIFICATE_FILENAME,
        SYNC_PRIVATE_KEY_FILENAME,
        WIFI_USED,
        disconnect_function
);

struct MQTTSub {
        char* topic;
        mqtt_callback* callback; 
};

/* 
 * Our Sync message handling callback.  This is passed as a callback function
 * when we subscribe to the Document, and will handle any incoming messages
 * on that topic (e.g., LED ON or OFF)
 */
int32_t handle_callback(MQTTCallbackParams params)
{     
        MQTTMessageParams message = params.MessageParams;

        char msg[message.PayloadLen+1];
        memcpy (msg,message.pPayload,message.PayloadLen);
        StaticJsonBuffer<maxMQTTpackageSize> jsonBuffer;

        Serial.print("Incoming message from Twilio:");
        Serial.println(msg);
        
        JsonObject& root = jsonBuffer.parseObject(msg);
        String led           = root["led"];
        if (led == "ON") {
                digitalWrite(LED_BUILTIN, HIGH);
        } else {
                digitalWrite(LED_BUILTIN, LOW);          
        }

        return 0;
}

/* 
 *  Main setup function.
 *  
 *  Here we connect to either GPRS or WiFi, then Deployed Devices.  We then 
 *  subscribe to the Document and listen for updates.
 */
void setup()
{
        LTask.begin();
        pinMode(LED_BUILTIN, OUTPUT);
        
        Serial.begin(115200);
        while(!Serial) {
                // Busy wait on Serial Monitor connection.
                delay(100);
        }

  
        if (WIFI_USED){
                LWiFi.begin();
                Serial.println("Connecting to AP");
                Serial.flush();
                while (!LWiFi.connect(
                                WIFI_AP, 
                                LWiFiLoginInfo(WIFI_AUTH, WIFI_PASSWORD)
                      )
                ) {
                        Serial.print(".");
                        Serial.flush();
                        delay(500);
                }
        } else {  
                Serial.println("Connecting to GPRS");
                Serial.flush();
                while (!LGPRS.attachGPRS(
                                GPRS_APN, 
                                GPRS_USERNAME, 
                                GPRS_PASSWORD
                      )
                ) {
                        Serial.println(".");
                        Serial.flush();
                        delay(500);
                }
        }

        Serial.println("Connected to the internet!");
        VMINT port = mqtt_tls_port;
        CONNECT_PORT = port;
        
        LTask.remoteCall(&__wifi_dns, (void*)SYNC_MQTT_HOST.c_str());
        LTask.remoteCall(&__bearer_open, NULL);
        LTask.remoteCall(&__mqtt_start, NULL);

        MQTTSub sub_struct;
        sub_struct.topic = const_cast<char*>(twilio_topic.c_str());
        sub_struct.callback = handle_callback;
        
        LTask.remoteCall(&__sub_mqtt, (void*)&sub_struct);
}

/*  
 *   Every time through the main loop we will spin up a new thread on the 
 *   LinkIt to perform our watchdog tasks in the background.  Insert everything
 *   you want to perform over and over again until infinity (or power 
 *   loss!) here.
*/
boolean main_thread(void* user_data) 
{
        Serial.flush();
        helper.handle_requests();
        delay(1000);

        // Example on how to publish to a Sync Document
        // (uncomment lines between the /* */)
        /*
        static uint32_t counter = 0;
        ++counter;
        if (counter == 10) {
                Serial.println("Sending message to Twilio.");
                helper.publish_to_topic(twilio_topic.c_str(), "{\"msg\":\"Ahoy!\",\"led\":\"ON\"}\0");
                counter = 0;
        }
        */
}

/*
 * This example creates a new thread which calls main_thread() every 3 seconds.
 */
void loop() 
{
        Serial.flush();
        LTask.remoteCall(main_thread, NULL);
        delay(3000);
}

/* 
 *  Trampolines/thunks are the easiest way to pass these class methods.  
 *  We don't have std::bind() or boost::bind()!
 */
inline boolean __bearer_open(void* ctx) 
{ return helper.bearer_open(ctx); }

inline boolean __mqtt_start(void* ctx)  
{ return helper.start_mqtt(ctx); }

inline boolean __wifi_dns(void* ctx)    
{ return helper.wifiResolveDomainName(ctx); }

inline boolean __sub_mqtt(void *ctx)   
{ 
        MQTTSub* sub_struct = (MQTTSub*)ctx; char* topic = sub_struct->topic;
        mqtt_callback* callback = sub_struct->callback;
        helper.subscribe_to_topic(topic, callback);
        return true;
}

/* MQTT Disconnect callback function */
void disconnect_function() 
{
        Serial.println("Oh no, we disconnected!"); while(1);
}

/* Workaround to remove a Macro from mbedtls */
#ifdef connect
#undef connect
#endif