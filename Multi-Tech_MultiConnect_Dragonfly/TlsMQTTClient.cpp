#include <cyassl/ctaocrypt/types.h>
#include "TlsMQTTClient.hpp"


TlsMQTTClient::TlsMQTTClient() :
    tcp(NULL), ctx(NULL), ssl(NULL),
    mqttClient(new MQTTClient(*this)) {
}


int TlsMQTTClient::connect(
    const char* host, 
    const int port,
    const char* certificates,
    MQTTPacket_connectData& options
) 
{
    cleanupTransport();

    // create TCP transport
    tcp = new TCPSocketConnection();
    if (tcp->connect(host, port)) {
        logError("tcp connection failed");
        goto fail;
    }
    tcp->set_blocking(false, 50);

    // setup SSL context
    ctx = CyaSSL_CTX_new((CYASSL_METHOD *)CyaSSLv23_client_method());
    { //Localize pMethod array for less overall memory time-use
        SSLMethod peerMethod = certificates != \
            NULL ? (SSLMethod)(
                VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT
            ) : VERIFY_NONE;
            
        std::string pMethod;
        if(peerMethod == VERIFY_NONE) {
            pMethod = "not verify peer";
        } else if (peerMethod & VERIFY_PEER) {
            pMethod = "Verify peer if certificates available";
            //Load the CA certificate(s) 
            // (If using multiple, concatenate them in the buffer being passed)
            if (SSL_SUCCESS !=  CyaSSL_CTX_load_verify_buffer(
                                    ctx, 
                                    (const unsigned char*)certificates, 
                                    strlen(certificates), 
                                    SSL_FILETYPE_PEM)
                                ) {
                                    
                logError("Unable to load root certificates!");
                goto fail;
            }
        }
        logDebug("SSL connection set to %s", pMethod.c_str());
        
        //SSL_VERIFY_FAIL_IF_NO_PEER_CERT, VERIFY_NONE, SSL_VERIFY_PEER
        CyaSSL_CTX_set_verify(ctx, peerMethod, 0); 
    }

    // setup SSL operations
    ssl = CyaSSL_new(ctx);

    CyaSSL_set_using_nonblock(ssl, 1);
    CyaSSL_SetIOReadCtx(ssl, this);
    CyaSSL_SetIORecv(ctx, ioRecv);
    CyaSSL_SetIOWriteCtx(ssl, this);
    CyaSSL_SetIOSend(ctx, ioSend);

    // SSL connect
    while (true) {
        int ret = CyaSSL_connect(ssl);

        if (ret != SSL_SUCCESS) {
            int err = CyaSSL_get_error(ssl, ret);
    
            if (SSL_ERROR_WANT_READ == err ||
                SSL_ERROR_WANT_WRITE == err) {
                continue;
            }

            logError("SSL_connect failed ret %d", ret);

            char data[CYASSL_MAX_ERROR_SZ];
            char data_new[CYASSL_MAX_ERROR_SZ];
            strcpy(data_new, CyaSSL_ERR_error_string(err, data));
            if(!strcmp(data,data_new)) {
                logError("Error code [%d] is [%s]\r\n", err, data);
            } else {
                logError(
                    "Failed to get error code [%d], Reason: [%s]\r\n", 
                    err, 
                    data_new
                );
            }

            goto fail;
        } else {
            break;
        }
    }

    return mqttClient->connect(options);

// Cleanup handler
fail:
    cleanupTransport() ;
    return MQTT::FAILURE;
}

int TlsMQTTClient::publish(const char* topicName, MQTT::Message& message) 
{
    return mqttClient->publish(topicName, message);
}

int TlsMQTTClient::publish(
    const char* topicName, 
    void* payload, 
    size_t payloadlen, 
    enum MQTT::QoS qos, 
    bool retained
) 
{
    return mqttClient->publish(topicName, payload, payloadlen, qos, retained);
}

int TlsMQTTClient::publish(
    const char* topicName, 
    void* payload, size_t payloadlen, 
    unsigned short& id, 
    enum MQTT::QoS qos, 
    bool retained
) 
{
    return mqttClient->publish(
        topicName, 
        payload, 
        payloadlen, 
        id, 
        qos, 
        retained
    );
}

int TlsMQTTClient::subscribe(
    const char* topicFilter, 
    enum MQTT::QoS qos, 
    MessageHandler mh
) 
{
    return mqttClient->subscribe(topicFilter, qos, mh);
}

int TlsMQTTClient::unsubscribe(const char* topicFilter) 
{
    return mqttClient->unsubscribe(topicFilter);
}

int TlsMQTTClient::disconnect() 
{
    int r = mqttClient->disconnect();
    cleanupTransport();
    return r;
}

int TlsMQTTClient::yield(unsigned long timeout_ms) 
{
    return mqttClient->yield(timeout_ms);
}

bool TlsMQTTClient::isConnected() 
{
    return mqttClient->isConnected();
}

void TlsMQTTClient::cleanupTransport() {
    if (ssl) {
        logTrace("freeing ssl");
        CyaSSL_free(ssl) ;
        ssl = NULL ;
    }
    if (ctx) {
        logTrace("freeing ssl ctx");
        CyaSSL_CTX_free(ctx) ;
        ctx = NULL ;
    }
    if (tcp) {
        if (tcp->is_connected()) {
            logTrace("disconnect tcp");
            tcp->close();
        }
        logTrace("freeing tcp");
        delete tcp;
        tcp = NULL;
    }
}

TlsMQTTClient::~TlsMQTTClient() {
    cleanupTransport();
    delete mqttClient;
}

int TlsMQTTClient::read(
    unsigned char* data, 
    int max, 
    int timeout
) 
{
//    logTrace(
//        "TlsMQTTClient::read data %p max %d timeout %d", 
//        data, 
//        max, 
//        timeout
//    );
    Timer tmr;
    int bytes = 0;    
    int totalbytes = 0;
    
    tmr.start();
    do {
        bytes = CyaSSL_read(ssl, data, max);
        if (bytes < 0) {
            int err = CyaSSL_get_error(ssl, bytes);
            if (SSL_ERROR_WANT_READ == err) {
                continue;
            }
            logTrace("CyaSSL_read fail %d", err);
            return -1;
        }
        totalbytes += bytes;
    } while ( tmr.read_ms() < timeout && totalbytes < max);
    //logTrace("TlsMQTTClient::read totalbytes %d", totalbytes);
    return totalbytes;
}

int TlsMQTTClient::write(const unsigned char* data, int length, int timeout) {
//    logTrace(
//        "TlsMQTTClient::write data %p max %d timeout %d", 
//        data, 
//        length, 
//        timeout
//    );
    Timer tmr;
    int bytes = 0;    
    int totalbytes = 0;
    
    tmr.start();
    do {
        bytes = CyaSSL_write(ssl, data, length);
        if (bytes < 0) {
            int err = CyaSSL_get_error(ssl, bytes);
            if (SSL_ERROR_WANT_WRITE == err) {
                continue;
            }
            logTrace("CyaSSL_write fail %d", err);
            return -1;
        }
        totalbytes += bytes;
    } while (tmr.read_ms() < timeout && totalbytes < length);
    //logTrace("TlsMQTTClient::write totalbytes %d", totalbytes);
    return totalbytes;
}

int TlsMQTTClient::ioRecv(CYASSL* ssl, char *buf, int sz, void *ctx) {
    TlsMQTTClient* thiz = (TlsMQTTClient*) ctx;
    int n = thiz->tcp->receive(buf, sz);
    if (0 == n) {
        return CYASSL_CBIO_ERR_WANT_READ;
    } else if (n > 0) {
        return n;
    } else {
        return CYASSL_CBIO_ERR_GENERAL;
    }
}

int TlsMQTTClient::ioSend(CYASSL* ssl, char *buf, int sz, void *ctx) {
    TlsMQTTClient* thiz = (TlsMQTTClient*) ctx;
    int n = thiz->tcp->send(buf, sz);
    if (0 == n) {
        return CYASSL_CBIO_ERR_WANT_WRITE;
    } else if (n > 0) {
        return n;
    } else {
        return CYASSL_CBIO_ERR_GENERAL;
    }
}
