#include "MQTTAsync.h"

#include <signal.h>
#include <memory.h>
#include <stdlib.h>

#if defined(WIN32)
#define sleep Sleep
#else

#include <unistd.h>
#include <openssl/hmac.h>
#include <openssl/bio.h>

#endif


volatile int connected = 0;
char *topic;
char *userName;
char *passWord;

int messageDeliveryComplete(void *context, MQTTAsync_token token) {
    /* not expecting any messages */
    printf("send message %d success\n", token);
    return 1;
}

int messageArrived(void *context, char *topicName, int topicLen, MQTTAsync_message *m) {
    /* not expecting any messages */
    printf("recv message from %s ,body is %s\n", topicName, (char *) m->payload);
    MQTTAsync_freeMessage(&m);
    MQTTAsync_free(topicName);
    return 1;
}

void onConnectFailure(void *context, MQTTAsync_failureData *response) {
    connected = 0;
    printf("connect failed, rc %d\n", response ? response->code : -1);
    MQTTAsync client = (MQTTAsync) context;
}

void onSubcribe(void *context, MQTTAsync_successData *response) {
    printf("subscribe success \n");
}

void onConnect(void *context, MQTTAsync_successData *response) {
    connected = 1;
    printf("connect success \n");
    MQTTAsync client = (MQTTAsync) context;
    //do sub when connect success
    MQTTAsync_responseOptions sub_opts = MQTTAsync_responseOptions_initializer;
    sub_opts.onSuccess = onSubcribe;
    int rc = 0;
    if ((rc = MQTTAsync_subscribe(client, topic, 1, &sub_opts)) != MQTTASYNC_SUCCESS) {
        printf("Failed to subscribe, return code %d\n", rc);
    }
}

void onDisconnect(void *context, MQTTAsync_successData *response) {
    connected = 0;
    printf("connect lost \n");
}

void onPublishFailure(void *context, MQTTAsync_failureData *response) {
    printf("Publish failed, rc %d\n", response ? -1 : response->code);
}

int success = 0;

void onPublish(void *context, MQTTAsync_successData *response) {
    printf("send success %d\n", ++success);
}


void connectionLost(void *context, char *cause) {
    connected = 0;
    MQTTAsync client = (MQTTAsync) context;
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    int rc = 0;

    printf("Connecting\n");
    conn_opts.MQTTVersion = MQTTVERSION_3_1_1;
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;
    conn_opts.username = userName;
    conn_opts.password = passWord;
    conn_opts.onSuccess = onConnect;
    conn_opts.onFailure = onConnectFailure;
    conn_opts.context = client;
    conn_opts.ssl = NULL;
    if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS) {
        printf("Failed to start connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }
}


int main(int argc, char **argv) {
    MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
    MQTTAsync client;
    //实例 ID，购买后从控制台获取
    char * instanceId = "XXXX";
    //测试收发消息的 Topic
    topic = "XXXX";
    //接入点域名，从控制台获取
    char *host = "XXXX.mqtt.aliyuncs.com";
    //客户端使用的 GroupID，从控制台申请
    char *groupId = "GID_XXXX";
    //客户端 ClientID 的后缀，由业务自行指定，只需要保证全局唯一即可
    char *deviceId = "XXXX";
    //账号 AccessKey，从账号控制台获取
    char *accessKey = "XXXX";
    //账号 SecretKey，从账号控制台获取
    char *secretKey = "XXXX";
    //使用的协议端口，默认 tcp 协议使用1883
    int port = 1883;
    int qos = 0;
    int cleanSession = 1;
    int rc = 0;
    char tempData[100];
    int len = 0;
    //ClientID要求使用 GroupId 和 DeviceId 拼接而成，长度不得超过64个字符
    char clientIdUrl[64];
    sprintf(clientIdUrl, "%s@@@%s", groupId, deviceId);
    //username和 Password 签名模式下的设置方法，参考文档 https://help.aliyun.com/document_detail/48271.html?spm=a2c4g.11186623.6.553.217831c3BSFry7
    HMAC(EVP_sha1(), secretKey, strlen(secretKey), clientIdUrl, strlen(clientIdUrl), tempData, &len);
    char resultData[100];
    int passWordLen = EVP_EncodeBlock((unsigned char *) resultData, tempData, len);
    resultData[passWordLen] = '\0';
    printf("passWord is %s", resultData);
    char userNameData[128];
    sprintf(userNameData,"Signature|%s|%s", accessKey, instanceId);
    userName = userNameData;
    passWord = resultData;
    //1.create client
    MQTTAsync_createOptions create_opts = MQTTAsync_createOptions_initializer;
    create_opts.sendWhileDisconnected = 0;
    create_opts.maxBufferedMessages = 10;
    char url[100];
    sprintf(url, "%s:%d", host, port);
    rc = MQTTAsync_createWithOptions(&client, url, clientIdUrl, MQTTCLIENT_PERSISTENCE_NONE, NULL, &create_opts);
    rc = MQTTAsync_setCallbacks(client, client, connectionLost, messageArrived, NULL);
    //2.connect to server
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    conn_opts.MQTTVersion = MQTTVERSION_3_1_1;
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = cleanSession;
    conn_opts.username = userName;
    conn_opts.password = passWord;
    conn_opts.onSuccess = onConnect;
    conn_opts.onFailure = onConnectFailure;
    conn_opts.context = client;
    conn_opts.ssl = NULL;
    conn_opts.automaticReconnect = 1;
    conn_opts.connectTimeout = 3;
    if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS) {
        printf("Failed to start connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }
    //3.publish msg
    MQTTAsync_responseOptions pub_opts = MQTTAsync_responseOptions_initializer;
    pub_opts.onSuccess = onPublish;
    pub_opts.onFailure = onPublishFailure;
    for (int i = 0; i < 1000; i++) {
        do {
            char data[100];
            sprintf(data, "hello mqtt demo");
            rc = MQTTAsync_send(client, topic, strlen(data), data, qos, 0, &pub_opts);
            sleep(1);
        } while (rc != MQTTASYNC_SUCCESS);
    }
    sleep(1000);
    disc_opts.onSuccess = onDisconnect;
    if ((rc = MQTTAsync_disconnect(client, &disc_opts)) != MQTTASYNC_SUCCESS) {
        printf("Failed to start disconnect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }
    while (connected)
        sleep(1);
    MQTTAsync_destroy(&client);
    return EXIT_SUCCESS;
}