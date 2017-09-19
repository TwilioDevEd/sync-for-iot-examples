#pragma once

#include <ssl.h>
#include <mbed.h>
#include <mtsas.h>
#include <TCPSocketConnection.h>
#include <MQTTClient.h>
#include <MQTTmbed.h>

class TlsMQTTClient {
public:
    TlsMQTTClient();
    ~TlsMQTTClient();

private:
    typedef MQTT::Client<
        TlsMQTTClient, 
        Countdown, 
        1024 /* MAX_MQTT_PACKET_SIZE */
    > MQTTClient;

    // MQTT Operations
public:
    /** MQTT Connect - send an MQTT connect packet down the network and wait for a Connack
     *  The nework object must be connected to the network endpoint before calling this
     *  @param host - host of the mqtt server
     *  @param port - port of the mqtt server
     *  @param certificates - CA certs to be verified, can be NULL to bypass CA checking(not recommended)
     *  @param options - connect options
     *  @return success code -
     */
    int connect(
        const char* host, 
        const int port,
        const char* certificates,
        MQTTPacket_connectData& options
    );

    /** MQTT Publish - send an MQTT publish packet and wait for all acks to complete for all QoSs
     *  @param topic - the topic to publish to
     *  @param message - the message to send
     *  @return success code -
     */
    int publish(
        const char* topicName, 
        MQTT::Message& message
    );
   
    /** MQTT Publish - send an MQTT publish packet and wait for all acks to complete for all QoSs
     *  @param topic - the topic to publish to
     *  @param payload - the data to send
     *  @param payloadlen - the length of the data
     *  @param qos - the QoS to send the publish at
     *  @param retained - whether the message should be retained
     *  @return success code -
     */
    int publish(
        const char* topicName, 
        void* payload, 
        size_t payloadlen, 
        enum MQTT::QoS qos = MQTT::QOS0, 
        bool retained = false
    );
  
    /** MQTT Publish - send an MQTT publish packet and wait for all acks to complete for all QoSs
     *  @param topic - the topic to publish to
     *  @param payload - the data to send
     *  @param payloadlen - the length of the data
     *  @param id - the packet id used - returned 
     *  @param qos - the QoS to send the publish at
     *  @param retained - whether the message should be retained
     *  @return success code -
     */
    int publish(
        const char* topicName, 
        void* payload, 
        size_t payloadlen, 
        unsigned short& id, 
        enum MQTT::QoS qos = MQTT::QOS1, 
        bool retained = false
    );

    typedef void (*MessageHandler)(MQTT::MessageData&);

    /** MQTT Subscribe - send an MQTT subscribe packet and wait for the suback
     *  @param topicFilter - a topic pattern which can include wildcards
     *  @param qos - the MQTT QoS to subscribe at
     *  @param mh - the callback function to be invoked when a message is received for this subscription
     *  @return success code -
     */
    int subscribe(
        const char* topicFilter, 
        enum MQTT::QoS qos, 
        MessageHandler mh
    );

    /** MQTT Unsubscribe - send an MQTT unsubscribe packet and wait for the unsuback
     *  @param topicFilter - a topic pattern which can include wildcards
     *  @return success code -
     */
    int unsubscribe(const char* topicFilter);

    /** MQTT Disconnect - send an MQTT disconnect packet, and clean up any state
     *  @return success code -
     */
    int disconnect();

    /** A call to this API must be made within the keepAlive interval to keep the MQTT connection alive
     *  yield can be called if no other MQTT operation is needed.  This will also allow messages to be
     *  received.
     *  @param timeout_ms the time to wait, in milliseconds
     *  @return success code - on failure, this means the client has disconnected
     */
    int yield(unsigned long timeout_ms = 1000L);

    /** Is the client connected?
     *  @return flag - is the client connected or not?
     */
    bool isConnected();

    // Network interface for MQTT::Client
    int read(unsigned char* data, int max, int timeout = -1);
    int write(const unsigned char* data, int length, int timeout = -1);

private:
    void cleanupTransport();
    static int ioRecv(CYASSL* ssl, char *buf, int sz, void *ctx);
    static int ioSend(CYASSL* ssl, char *buf, int sz, void *ctx);
    TCPSocketConnection* tcp;
    CYASSL_CTX* ctx;
    CYASSL *ssl;
    MQTTClient* mqttClient;
};
