/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander/Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/

#ifndef __MQTT_CLIENT_C_
#define __MQTT_CLIENT_C_

#include "MQTTPacket.h"
#include "MQTTConnect.h"
#include "MQTTMTK.h" //Platform specific implementation header file
#include "dlinkmq_api.h"
#include "dlinkmq_utils.h"

#define MAX_PACKET_ID 65535
#define MAX_MESSAGE_HANDLERS 5

enum QoS { QOS0, QOS1, QOS2 };

// all failure return codes must be negative
enum returnCode { TIMEOUT = -3, BUFFER_OVERFLOW = -2, FAILURE = -1, SUCCESS = 0 };
enum mqttStatus {
	MQTT_STATUS_INIT = 0, 
	MQTT_STATUS_INITING, 
	MQTT_STATUS_CONN,
	MQTT_STATUS_CONNING, 
	MQTT_STATUS_RECONN,
	MQTT_STATUS_SUB, 
	MQTT_STATUS_SUBING, 
	MQTT_STATUS_DESTROY,
	MQTT_STATUS_RUN
};

void NewTimer(St_Timer*);

//typedef struct MQTTMessage MQTTMessage;

//typedef struct MessageData MessageData;

typedef struct MQTTMessage
{
    enum QoS qos;
    char retained;
    char dup;
    unsigned short id;
    void *payload;
    size_t payloadlen;
	dlinkmq_datapoint *datapoint;
	on_send_msg fun_cb;
}MQTTMessage;

typedef struct MessageData
{
    MQTTMessage* message;
    MQTTString* topicName;
}MessageData;

typedef void (*messageHandler)(MessageData*);

typedef struct Client Client;

int MQTTConnect (Client*, MQTTPacket_connectData*);
int MQTTPublish (Client*, const char*, MQTTMessage*);
int MQTTSubscribe (Client*, const char*, enum QoS, messageHandler);
int MQTTUnsubscribe (Client*, const char*);
int MQTTDisconnect (Client*);
int MQTTYield (Client*, int);

void setDefaultMessageHandler(Client*, messageHandler);

void MQTTClient(Client*, Network*, unsigned int, unsigned char*, size_t, unsigned char*, size_t);

struct Client {

// 	U8  app_id;
// 	U32	nwt_account_id; 
	
    unsigned int next_packetid;
    unsigned int command_timeout_ms;
    size_t buf_size, readbuf_size;
    unsigned char *buf;
    unsigned char *readbuf;
    unsigned int keepAliveInterval;
    char ping_outstanding;
    int isconnected;
    
    struct MessageHandlers
    {
        const char* topicFilter;
        void (*fp) (MessageData*);
    } messageHandlers[MAX_MESSAGE_HANDLERS];      // Message handlers are indexed by subscription topic
    
    void (*defaultMessageHandler) (MessageData*);
    
    Network* ipstack;
    St_Timer ping_timer;

	MQTTAsyncCallbackFunc fnReadTimeout;
	MQTTAsyncCallbackFunc fnPingTimeout;
};

#define DefaultClient {0, 0, 0, 0, NULL, NULL, 0, 0, 0}

//#pragma mark - async method


we_int DlinkmqClient_Init(we_handle *phDlinkmqClientHandle);

we_void DlinkmqClient_Destroy(we_handle hDlinkmqClientHandle);
we_void DlinkmqClient_DestroyMqttAll(we_handle hDlinkmqClientHandle);



void MQTTAsyncConnect (Client*, MQTTPacket_connectData*,MQTTAsyncCallbackFunc cb);
void MQTTAsyncPublish (Client*, const char*, MQTTMessage*,MQTTAsyncCallbackFunc cb);
void MQTTAsyncSubscribe (Client*, const char*, enum QoS, messageHandler,MQTTAsyncCallbackFunc cb);
void MQTTAsyncUnsubscribe (Client*, const char*,MQTTAsyncCallbackFunc cb);
void MQTTAsyncDisconnect (Client*,MQTTAsyncCallbackFunc cb);
void MQTTAsyncYield (Client*, int,MQTTAsyncCallbackFunc cb);
kal_uint8 dispatchEvents(void * event);

#endif
