#pragma once

const int maxMQTTpackageSize = 512;
const int maxMQTTMessageHandlers = 4;

// Handle JSON
#include <ArduinoJson.h>

#include "Arduino.h"
#include <LTask.h>
#include <LWiFi.h>
#include <LWiFiClient.h>
#include <LGPRS.h>

#include "linkit_aws_header.h"

/* Workaround to remove a Macro from mbedtls */
#ifdef connect
#undef connect
#endif

/*
 * The SyncLinkItHelper class encapsulates some of the more complex 
 * MQTT communications going on in this example.
 */
class SyncLinkItHelper {
public:
        SyncLinkItHelper(
                const int&      mqtt_port_in,
                const String&   mqtt_host_in,
                const String&   mqtt_client_id_in,
                const String&   root_ca_name_in,
                const String&   iot_ca_fname_in,
                const String&   iot_key_fname_in,
                const bool&     wifi_used_in,
                void            (*disc_in)(void)
        );

        // Subscribe to a topic and register a callback function
        // to handle incoming messages
        void subscribe_to_topic(
                char* topic, 
                int32_t (*callbackHandler)(MQTTCallbackParams) 
        );

        // Publish an Arduino string message to a topic
        void publish_to_topic(
                const char* topic, 
                const char* message 
        );
        
        // Publish a C string message to a topic
        void publish_to_topic(
                const char* topic, 
                const String& message
        );

        boolean start_mqtt(void* ctx);
        boolean wifiResolveDomainName(void *userData);
        boolean bearer_open(void* ctx);
        void handle_requests();
        
private:
        
        // Incoming message and connection bookeeping.
        static long                     connection;
        static long                     arrivedcount;

        // Twilio Sync Deployed Devices Related configuration
        String                          mqtt_host;
        const int                       mqtt_port;
        String                          mqtt_client_id;
        String                          root_ca_name;
        String                          iot_ca_fname;
        String                          iot_key_fname;
        bool                            wifi_used;

        // Pointer to our disconnect function
        void                            (*__disc)(void);
};

namespace linkitsync
{
        VMINT __wifiResolveCallback(vm_soc_dns_result *pDNS);
        void __bearer_callback(
                VMINT handle,
                VMINT event,
                VMUINT data_account_id,
                void *user_data
        );
}