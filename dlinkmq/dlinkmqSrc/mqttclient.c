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

#include "MQTTClient.h"

#include "soc_consts.h"
#include "kal_general_types.h"
#include "MQTTTrace.h"

extern void get_mqtt_test(void);
extern  void mqtt_app_sendmsg_to_task(msg_type iMsg);
extern void mqtt_cb_exec(MQTTAsyncCallbackFunc cb,int result);
extern void cb_mqtt_service_init(int result,void *data);
extern void mqtt_cb_exec_with_data(MQTTAsyncCallbackFunc cb,int result,void *data);
extern int mqtt_service_start();
//extern Client c;



void NewMessageData(MessageData* md, MQTTString* aTopicName, MQTTMessage* aMessgage) {
    md->topicName = aTopicName;
    md->message = aMessgage;
}


int getNextPacketId(Client *c) {
    return c->next_packetid = (c->next_packetid == MAX_PACKET_ID) ? 1 : c->next_packetid + 1;
}


int sendPacket(Client* c, int length, Timer* timer)
{
    int rc = FAILURE,
    sent = 0;
    
    while (sent < length && !expired(timer))
    {
        rc = c->ipstack->mqttwrite(c->ipstack, &c->buf[sent], length, left_ms(timer));
        if (rc < 0)  // there was an error writing the data
            break;
        sent += rc;
    }
    if (sent == length)
    {
        countdown(&c->ping_timer, c->keepAliveInterval); // record the fact that we have successfully sent the packet
        rc = SUCCESS;
    }
    else
        rc = FAILURE;
    return rc;
}







void MQTTClient(Client* c, Network* network, unsigned int command_timeout_ms, unsigned char* buf, size_t buf_size, unsigned char* readbuf, size_t readbuf_size)
{
    int i;
    c->ipstack = network;
    
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
        c->messageHandlers[i].topicFilter = 0;
    c->command_timeout_ms = command_timeout_ms;
    c->buf = buf;
    c->buf_size = buf_size;
    c->readbuf = readbuf;
    c->readbuf_size = readbuf_size;
    c->isconnected = 0;
    c->ping_outstanding = 0;
    c->defaultMessageHandler = NULL;
    InitTimer(&c->ping_timer);
}

typedef enum{readStatusReady,readStatusDecodingPacket,readStatusReading} readStatus;

static readStatus currentReadStatus = readStatusReady;

int decodePacket(Client* c, int* value, int timeout)
{
    unsigned char i;
    int multiplier = 1;
    int len = 0;
    const int MAX_NO_OF_REMAINING_LENGTH_BYTES = 4;
    
    
    *value = 0;
    do
    {
        int rc = MQTTPACKET_READ_ERROR;
        
        if (++len > MAX_NO_OF_REMAINING_LENGTH_BYTES)
        {
            rc = MQTTPACKET_READ_ERROR; /* bad data */
            goto exit;
        }
        rc = c->ipstack->mqttread(c->ipstack, &i, 1, timeout);
        if (rc != 1)
            goto exit;
        *value += (i & 127) * multiplier;
        multiplier *= 128;
    } while ((i & 128) != 0);
exit:
    return len;
}


int readPacket(Client* c, Timer* timer)
{
    int rc = FAILURE;
    MQTTHeader header = {0};
    int len = 0;
    int rem_len = 0;
    
    /* 1. read the header byte.  This has the packet type in it */
    if (c->ipstack->mqttread(c->ipstack, c->readbuf, 1, left_ms(timer)) != 1)
        goto exit;
    
    len = 1;
    /* 2. read the remaining length.  This is variable in itself */
    decodePacket(c, &rem_len, left_ms(timer));
    len += MQTTPacket_encode(c->readbuf + 1, rem_len); /* put the original remaining length back into the buffer */
    
    /* 3. read the rest of the buffer using a callback to supply the rest of the data */
    if (rem_len > 0 && (c->ipstack->mqttread(c->ipstack, c->readbuf + len, rem_len, left_ms(timer)) != rem_len))
        goto exit;
    
    header.byte = c->readbuf[0];
    rc = header.bits.type;
exit:
    return rc;
}










// assume topic filter and name is in correct format
// # can only be at end
// + and # can only be next to separator
char isTopicMatched(char* topicFilter, MQTTString* topicName)
{
    char* curf = topicFilter;
    char* curn = topicName->lenstring.data;
    char* curn_end = curn + topicName->lenstring.len;
    
    while (*curf && curn < curn_end)
    {
        if (*curn == '/' && *curf != '/')
            break;
        if (*curf != '+' && *curf != '#' && *curf != *curn)
            break;
        if (*curf == '+')
        {   // skip until we meet the next separator, or end of string
            char* nextpos = curn + 1;
            while (nextpos < curn_end && *nextpos != '/')
                nextpos = ++curn + 1;
        }
        else if (*curf == '#')
            curn = curn_end - 1;    // skip until end of string
        curf++;
        curn++;
    };
    
    return (curn == curn_end) && (*curf == '\0');
}


int deliverMessage(Client* c, MQTTString* topicName, MQTTMessage* message)
{
    int i;
    int rc = FAILURE;
    
    // we have to find the right message handler - indexed by topic
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
    {
        if (c->messageHandlers[i].topicFilter != 0 && (MQTTPacket_equals(topicName, (char*)c->messageHandlers[i].topicFilter) ||
                                                       isTopicMatched((char*)c->messageHandlers[i].topicFilter, topicName)))
        {
            if (c->messageHandlers[i].fp != NULL)
            {
                MessageData md;
                NewMessageData(&md, topicName, message);
                c->messageHandlers[i].fp(&md);
                rc = SUCCESS;
            }
        }
    }
    
    if (rc == FAILURE && c->defaultMessageHandler != NULL)
    {
        MessageData md;
        NewMessageData(&md, topicName, message);
        c->defaultMessageHandler(&md);
        rc = SUCCESS;
    }
    
    return rc;
}





int keepalive(Client* c)
{
    int rc = FAILURE;
    
    if (c->keepAliveInterval == 0)
    {
        rc = SUCCESS;
        goto exit;
    }
    
    if (expired(&c->ping_timer))
    {
        if (!c->ping_outstanding)
        {
            Timer timer;
            int len;
            InitTimer(&timer);
            countdown_ms(&timer, 1000);
            len = MQTTSerialize_pingreq(c->buf, c->buf_size);
            if (len > 0 && (rc = sendPacket(c, len, &timer)) == SUCCESS) // send the ping packet
                c->ping_outstanding = 1;
        }
    }
    
exit:
    return rc;
}






int cycle(Client* c, Timer* timer)
{
    // read the socket, see what work is due
    unsigned short packet_type = readPacket(c, timer);
    
    
    
    int len = 0,
    rc = SUCCESS;
    
    switch (packet_type)
    {
        case CONNACK:
        case PUBACK:
        case SUBACK:
            break;
        case PUBLISH:
        {
            MQTTString topicName;
            MQTTMessage msg;
            if (MQTTDeserialize_publish((unsigned char*)&msg.dup, (int*)&msg.qos, (unsigned char*)&msg.retained, (unsigned short*)&msg.id, &topicName,
                                        (unsigned char**)&msg.payload, (int*)&msg.payloadlen, c->readbuf, c->readbuf_size) != 1)
                goto exit;
            deliverMessage(c, &topicName, &msg);
            if (msg.qos != QOS0)
            {
                if (msg.qos == QOS1)
                    len = MQTTSerialize_ack(c->buf, c->buf_size, PUBACK, 0, msg.id);
                else if (msg.qos == QOS2)
                    len = MQTTSerialize_ack(c->buf, c->buf_size, PUBREC, 0, msg.id);
                if (len <= 0)
                    rc = FAILURE;
                else
                    rc = sendPacket(c, len, timer);
                if (rc == FAILURE)
                    goto exit; // there was a problem
            }
            break;
        }
        case PUBREC:
        {
            unsigned short mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = FAILURE;
            else if ((len = MQTTSerialize_ack(c->buf, c->buf_size, PUBREL, 0, mypacketid)) <= 0)
                rc = FAILURE;
            else if ((rc = sendPacket(c, len, timer)) != SUCCESS) // send the PUBREL packet
                rc = FAILURE; // there was a problem
            if (rc == FAILURE)
                goto exit; // there was a problem
            break;
        }
        case PUBCOMP:
            break;
        case PINGRESP:
            //ping_timer_dispose();
            break;
    }
    keepalive(c);
exit:
    if (rc == SUCCESS)
        rc = packet_type;
    return rc;
}


int MQTTYield(Client* c, int timeout_ms)
{
    int rc = SUCCESS;
    Timer timer;
    
    InitTimer(&timer);
    countdown_ms(&timer, timeout_ms);
    while (!expired(&timer))
    {
        
        if (cycle(c, &timer) == FAILURE)
        {
            rc = FAILURE;
            break;
        }
    }
    
    return rc;
}


// only used in single-threaded mode where one command at a time is in process
int waitfor(Client* c, int packet_type, Timer* timer)
{
    int rc = FAILURE;
    
    do
    {
        if (expired(timer))
            break; // we timed out
    }
    while ((rc = cycle(c, timer)) != packet_type);
    
    return rc;
}


int MQTTConnect(Client* c, MQTTPacket_connectData* options)
{
    Timer connect_timer;
    int rc = FAILURE;
    MQTTPacket_connectData default_options = MQTTPacket_connectData_initializer;
    int len = 0;
    
    InitTimer(&connect_timer);
    countdown_ms(&connect_timer, c->command_timeout_ms);
    
    if (c->isconnected) // don't send connect packet again if we are already connected
        goto exit;
    
    if (options == 0)
        options = &default_options; // set default options if none were supplied
    
    c->keepAliveInterval = options->keepAliveInterval;
    countdown(&c->ping_timer, c->keepAliveInterval);
    if ((len = MQTTSerialize_connect(c->buf, c->buf_size, options)) <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &connect_timer)) != SUCCESS)  // send the connect packet
        goto exit; // there was a problem
    
    // this will be a blocking call, wait for the connack
    if (waitfor(c, CONNACK, &connect_timer) == CONNACK)
    {
        unsigned char connack_rc = 255;
        char sessionPresent = 0;
        if (MQTTDeserialize_connack((unsigned char*)&sessionPresent, &connack_rc, c->readbuf, c->readbuf_size) == 1)
            rc = connack_rc;
        else
            rc = FAILURE;
    }
    else
        rc = FAILURE;
    
exit:
    if (rc == SUCCESS)
        c->isconnected = 1;
    return rc;
}


int MQTTSubscribe(Client* c, const char* topicFilter, enum QoS qos, messageHandler messageHandler)
{
    int rc = FAILURE;
    Timer timer;
    int len = 0;
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;
    
    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);
    
    if (!c->isconnected)
        goto exit;
    
    len = MQTTSerialize_subscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic, (int*)&qos);
    if (len <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit;             // there was a problem
    
    if (waitfor(c, SUBACK, &timer) == SUBACK)      // wait for suback
    {
        int count = 0, grantedQoS = -1;
        unsigned short mypacketid;
        if (MQTTDeserialize_suback(&mypacketid, 1, &count, &grantedQoS, c->readbuf, c->readbuf_size) == 1)
            rc = grantedQoS; // 0, 1, 2 or 0x80
        if (rc != 0x80)
        {
            int i;
            for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
            {
                if (c->messageHandlers[i].topicFilter == 0)
                {
                    c->messageHandlers[i].topicFilter = topicFilter;
                    c->messageHandlers[i].fp = messageHandler;
                    rc = 0;
                    break;
                }
            }
        }
    }
    else
        rc = FAILURE;
    
exit:
    return rc;
}


int MQTTUnsubscribe(Client* c, const char* topicFilter)
{
    int rc = FAILURE;
    int len = 0;
    Timer timer;
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;
    
    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);
    
    if (!c->isconnected)
        goto exit;
    
    if ((len = MQTTSerialize_unsubscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic)) <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit; // there was a problem
    
    if (waitfor(c, UNSUBACK, &timer) == UNSUBACK)
    {
        unsigned short mypacketid;  // should be the same as the packetid above
        if (MQTTDeserialize_unsuback(&mypacketid, c->readbuf, c->readbuf_size) == 1)
            rc = 0;
    }
    else
        rc = FAILURE;
    
exit:
    return rc;
}


int MQTTPublish(Client* c, const char* topicName, MQTTMessage* message)
{
    int rc = FAILURE;
    int len = 0;
    Timer timer;
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicName;
    
    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);
    
    if (!c->isconnected)
        goto exit;
    
    if (message->qos == QOS1 || message->qos == QOS2)
        message->id = getNextPacketId(c);
    
    len = MQTTSerialize_publish(c->buf, c->buf_size, 0, message->qos, message->retained, message->id,
                                topic, (unsigned char*)message->payload, message->payloadlen);
    if (len <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit; // there was a problem
    
    if (message->qos == QOS1)
    {
        if (waitfor(c, PUBACK, &timer) == PUBACK)
        {
            unsigned short mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = FAILURE;
        }
        else
            rc = FAILURE;
    }
    else if (message->qos == QOS2)
    {
        if (waitfor(c, PUBCOMP, &timer) == PUBCOMP)
        {
            unsigned short mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = FAILURE;
        }
        else
            rc = FAILURE;
    }
    
exit:
    return rc;
}


int MQTTDisconnect(Client* c)
{
    int rc = FAILURE;
    Timer timer;     // we might wait for incomplete incoming publishes to complete
    int len = MQTTSerialize_disconnect(c->buf, c->buf_size);
    
    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);
    
    if (len > 0)
        rc = sendPacket(c, len, &timer);            // send the disconnect packet
    
    c->isconnected = 0;
    return rc;
}


//#pragma mark - async methods

//#pragma mark - dispatch notify
//typedef struct
//{
//    kal_uint8       ref_count;
//    kal_uint16      msg_len;
//    kal_int8        socket_id;    /* socket ID */
//    soc_event_enum  event_type;   /* soc_event_enum */
//    kal_bool        result;       /* notification result. KAL_TRUE: success, KAL_FALSE: error */
//    soc_error_enum  error_cause;  /* used only when EVENT is close/connect */
//    kal_int32       detail_cause; /* refer to ps_cause_enum if error_cause is
//                                   * SOC_BEARER_FAIL */
//} app_soc_notify_ind_struct;


void sendEventDoNext(void);
void decodeEventDoNext(void);
void readEventDoNext(void);

static int devToStopFlag = 0;


kal_uint8 dispatchEvents(void * event){
    soc_event_enum type;
	app_soc_notify_ind_struct *notify = (app_soc_notify_ind_struct *)event;
    if (notify == NULL) {
        return 1;
    }
    type = notify->event_type;
	printf("\n---dispatchEvents notify event : %d",type);
	kal_prompt_trace(MOD_MQTT,"---dispatchEvents soc_notify->event_type:%d",type);
    switch (type) {
		case SOC_READ:{
			//printf("\r\n---n  SOC_READ\n");
            if (currentReadStatus == readStatusDecodingPacket) {
                //printf("\ndecode next");
                decodeEventDoNext();
            }
            if (currentReadStatus == readStatusReading) {
                //printf("\nread next");
                readEventDoNext();
            }
            break;
        }
        case SOC_CLOSE:{
			//mqtt socket close
            //printf("\r\n---n  SOC_CLOSE\n");
			kal_prompt_trace(MOD_MQTT,"\r\n---n  SOC_CLOSE\n");
			mqtt_service_start();
            break;
        }
        case SOC_WRITE:{
            sendEventDoNext();
            break;
        }
        case SOC_ACCEPT:{
            
            break;
        }
        case SOC_CONNECT:{
			mqtt_app_sendmsg_to_task(MSG_ID_MQTT_STATUS);

			
            break;
        }
            
    }
	return 0;
};




typedef struct sendPacketEvent{
    MQTTAsyncCallbackFunc cb;
    Client *c;
    int length;
    Timer *timer;
    int sent;
    int result;
}sendPacketEvent;


//#pragma mark - send packet


static sendPacketEvent *currentSendingEvent = NULL;
static kal_uint32 lastSentTime_ms = 0;

static void MQTTAsyncConnectSendPacketCheck(int,void *);

static void sendEventFinish(int result){
    MQTTAsyncCallbackFunc cb;
    if (!currentSendingEvent) {
        return;
    }

    cb = currentSendingEvent->cb;
    DLINKMQ_FREE(currentSendingEvent);
    currentSendingEvent = NULL;

	if(cb){
		mqtt_cb_exec(cb,result);
	}
    
}

static void sendEventDoNext(void){
    int rc,length,sent;
    Client *c;
    Timer *timer;
    if (currentSendingEvent == NULL) {
        return;
    }
    c = currentSendingEvent->c;
    length = currentSendingEvent->length;
    timer = currentSendingEvent->timer;
    sent = currentSendingEvent->sent;
    if (expired(timer)) {
       // sendEventFinish(FAILURE);
        //return;
    }
    rc = c->ipstack->mqttwrite(c->ipstack, &c->buf[sent], length, left_ms(timer));
    mqtt_fmt_print("\n----------------------------mqttwrite-->len:%d",rc);
    if (rc >= 0) {
        sent += rc;
    }
    if (rc == -1){
        sendEventFinish(FAILURE);
        return;
    }
    if (rc == -2) {
        return;
    }
    if (sent == length) {
        sendEventFinish(SUCCESS);
    }else{
        sendEventDoNext();
    }
}

static void asyncSendPacket(Client *c,int length,Timer *timer,MQTTAsyncCallbackFunc cb){
    if (currentSendingEvent) {
		/*
        if (cb) {
            mqtt_cb_exec(cb,FAILURE);
        }
        return;
		*/
		sendEventFinish(FAILURE);
    }
    currentSendingEvent = (sendPacketEvent *)DLINKMQ_MALLOC(sizeof(sendPacketEvent));
    currentSendingEvent->cb = cb;
    currentSendingEvent->length = length;
    currentSendingEvent->timer = timer;
    currentSendingEvent->c = c;
    currentSendingEvent->sent = 0;
    currentSendingEvent->result = 0;
    sendEventDoNext();
}

//#pragma mark - read packet
typedef struct readEvent{
    Client *c;
    unsigned char *buffer;
    int len;
    int timeout_ms;
    Timer *timer;
    MQTTAsyncCallbackFunc cb;
    int result;
}readEvent;





static readEvent *currentReadEvent = NULL;
static const kal_uint32 readEventTimeoutInteval = 800;
static kal_timerid readEventTimer = NULL;






static void readEventFinish(kal_bool isTimeout){
    MQTTAsyncCallbackFunc cb;
    int result;

	mqtt_fmt_print("---mqtt readEventFinish readEventFinish start");
	
    if (!currentReadEvent) {
		mqtt_fmt_print("---mqtt readEventFinish readEventFinish currentReadEvent is NULL");
        return;
    }
	if(readEventTimer && isTimeout == KAL_FALSE){

		mqtt_fmt_print("---mqtt readEventFinish before kal_cancel_timer readEventTimer ");
		kal_cancel_timer(readEventTimer);

		mqtt_fmt_print("---mqtt readEventFinish after kal_cancel_timer readEventTimer ");
		//readEventTimer = NULL;
	}

    cb = currentReadEvent->cb;

	mqtt_fmt_print("---mqtt readEventFinish  currentReadEvent->cb");
    result = currentReadEvent->result;

	mqtt_fmt_print("---mqtt readEventFinish  currentReadEvent->result");
	
    DLINKMQ_FREE(currentReadEvent->timer);

	
	mqtt_fmt_print("---mqtt readEventFinish  DLINKMQ_FREE currentReadEvent->timer = 0x%x", currentReadEvent->timer);
	
    DLINKMQ_FREE(currentReadEvent);

mqtt_fmt_print("---mqtt readEventFinish  DLINKMQ_FREE currentReadEvent =0x%x ", currentReadEvent);
	
    currentReadEvent = NULL;
    currentReadStatus = readStatusReady;

	mqtt_fmt_print("---mqtt readEventFinish  currentReadStatus is  readStatusReady");
	
    if (cb )
    {
    	mqtt_fmt_print("---mqtt readEventFinish before mqtt_cb_exec mqtt_cb_exec ");
        mqtt_cb_exec(cb,result);
	mqtt_fmt_print("---mqtt readEventFinish after mqtt_cb_exec mqtt_cb_exec ");
    }

	mqtt_fmt_print("---mqtt readEventFinish readEventFinish end");
    
}
static void readEventTimeout(void * timerData){

	mqtt_fmt_print("---mqtt readEventTimeout readEventTimeout");
	//readEventTimer = NULL;
    if (!currentReadEvent) {

	mqtt_fmt_print("---mqtt readEventTimeout currentReadEvent is NULL");
        return;
    }
	currentReadEvent->result = FAILURE;

	mqtt_fmt_print("---mqtt readEventTimeout before readEventFinish");
	readEventFinish(KAL_TRUE);
	mqtt_fmt_print("---mqtt readEventTimeout after readEventFinish");


}
static int tmpCount=0;
static void readEventDoNext(void){
    int rc;
	Client *c;
	mqtt_fmt_print("---mqtt readEventDoNext");
    if (!currentReadEvent) {
        return;
    }
    if (expired(currentReadEvent->timer)) {
        //currentReadEvent->result = -1;
       // readEventFinish();
       // return;
    }

    c = currentReadEvent->c;

   // mqtt_fmt_print("\n---mqtt will read next,current length:%d,total length:%d\n,buffer:%x",currentReadEvent->result,currentReadEvent->len,*(currentReadEvent->buffer));
   mqtt_fmt_print("\n---mqtt will read next");
    rc = c->ipstack->mqttread(c->ipstack, currentReadEvent->buffer, currentReadEvent->len, currentReadEvent->timeout_ms);
	mqtt_fmt_print("\n---mqtt will read finish rc = %d", rc);
   //mqtt_fmt_print("\n---mqtt read finish,buffer:%x\n",*(currentReadEvent->buffer));
    if (rc >= 0) {
		currentReadEvent->buffer += rc;
        currentReadEvent->result += rc;
        if (currentReadEvent->result == currentReadEvent->len) {
            readEventFinish(KAL_FALSE);
            return;
		}else if(rc == 0){
		mqtt_fmt_print("\n---mqtt read fail!\n");
			currentReadEvent->result = -1;
			readEventFinish(KAL_FALSE);
		}
    }
    if(rc == -1){
        currentReadEvent->result = -1;
        readEventFinish(KAL_FALSE);
        return;
    }
    if(rc == -2){
        mqtt_fmt_print("\n----mqtt read event would block");
        return;
    }
    readEventDoNext();
}



static void asyncRead(Client *c,unsigned char *buffer,int len,int timeout_ms,MQTTAsyncCallbackFunc cb){
    
    if (currentReadEvent || currentReadStatus != readStatusReady) {
	mqtt_fmt_print("---mqtt asyncRead -1");
        if (cb) {
            mqtt_cb_exec(cb,-1);
        }
        return;
    }
    currentReadEvent = (readEvent *)DLINKMQ_MALLOC(sizeof(readEvent));

	mqtt_fmt_print("---mqtt asyncRead currentReadEvent = 0x%x", currentReadEvent);
    currentReadEvent->c = c;
    currentReadEvent->len = len;
    currentReadEvent->buffer = buffer;
    currentReadEvent->timer = (Timer *)DLINKMQ_MALLOC(sizeof(Timer));

	mqtt_fmt_print("---mqtt asyncRead  currentReadEvent->timer = 0x%x",  currentReadEvent->timer);
    currentReadEvent->result = 0;
    InitTimer(currentReadEvent->timer);
    countdown_ms(currentReadEvent->timer, timeout_ms);
    currentReadEvent->cb = cb;
    currentReadStatus = readStatusReading;
	//readEventTimer = kal_create_timer("MQTTClientReadEventTimer");
	if(readEventTimer == NULL){
		
		readEventTimer = kal_create_timer("MQTTClientReadEventTimer");

		mqtt_fmt_print("---mqtt kal_create_timer readEventTimer");
	}
mqtt_fmt_print("---mqtt kal_set_timer readEventTimer");
	kal_set_timer(readEventTimer,readEventTimeout,NULL,kal_milli_secs_to_ticks(readEventTimeoutInteval),0);
    readEventDoNext();
}





typedef struct decodePacketEvent{
    Client *c;
    int *value;
    MQTTAsyncCallbackFunc cb;
    unsigned char buffer;
    int multiplier;
    int len;
    int timeout;
    Timer *timer;
}decodePacketEvent;

static decodePacketEvent *currentDecodeEvent = NULL;
static void decodeEventFinish(void){
    MQTTAsyncCallbackFunc cb;
    int result;
    if (!currentDecodeEvent) {
        return;
    }
	//printf("\ndecode finish\n");
    cb = currentDecodeEvent->cb;
    result = currentDecodeEvent->len;
    DLINKMQ_FREE(currentDecodeEvent->timer);
    DLINKMQ_FREE(currentDecodeEvent);
    currentDecodeEvent = NULL;
    currentReadStatus = readStatusReady;

    if (cb)
    {
        mqtt_cb_exec(cb,result);
		return;

    }
    
}
static void asyncDecodePacket(Client *c, int* value, int timeout, MQTTAsyncCallbackFunc cb){
    if (currentDecodeEvent || currentReadStatus != readStatusReady) {
        if (cb) {
            mqtt_cb_exec(cb,0);
        }
        return;
    }
    currentDecodeEvent = (decodePacketEvent *)DLINKMQ_MALLOC(sizeof(decodePacketEvent));
    
    *value = 0;
    currentDecodeEvent->len = 0;
    currentDecodeEvent->c = c;
    currentDecodeEvent->value = value;
    currentDecodeEvent->multiplier = 1;
    currentDecodeEvent->timer = DLINKMQ_MALLOC(sizeof(Timer));
    InitTimer(currentDecodeEvent->timer);
    countdown(currentDecodeEvent->timer, timeout);
    currentDecodeEvent->cb = cb;
    currentDecodeEvent->timeout = timeout;
    
    currentReadStatus = readStatusDecodingPacket;
    
    decodeEventDoNext();
}


static void decodeEventDoNext(void){
    int rc;
    const int MAX_NO_OF_REMAINING_LENGTH_BYTES = 4;
    Client *c = currentDecodeEvent->c;
    if(!currentDecodeEvent){
        return;
    }
    if(expired(currentDecodeEvent->timer)){
       // decodeEventFinish();
        //return;
    }
    if (++currentDecodeEvent->len > MAX_NO_OF_REMAINING_LENGTH_BYTES){
        decodeEventFinish();
        return;
    }
    rc = c->ipstack->mqttread(c->ipstack, &currentDecodeEvent->buffer, 1, currentDecodeEvent->timeout);
    if (rc == 1) {
        *currentDecodeEvent->value += (currentDecodeEvent->buffer & 127) * currentDecodeEvent->multiplier;
        currentDecodeEvent->multiplier *= 128;
        if ((currentDecodeEvent->buffer & 128) != 0) {
            decodeEventDoNext();
        }else{
            decodeEventFinish();
        }
    }else if(rc == -2){
        return;
    }else{
        decodeEventFinish();
    }
}

typedef struct readPacketEvent{
    Client *c;
    Timer *timer;
    int result;
    MQTTHeader *header;
    int len;
    int rem_len;
    int step;
    MQTTAsyncCallbackFunc cb;
}readPacketEvent;

static readPacketEvent *currentReadPacketEvent = NULL;

static void readPacketEventFinish(void){
    MQTTAsyncCallbackFunc cb;
    int result;
    if (!currentReadPacketEvent) {
        return;
    }
	
    cb = currentReadPacketEvent->cb;
    result = currentReadPacketEvent->result;

    DLINKMQ_FREE(currentReadPacketEvent->header);
    DLINKMQ_FREE(currentReadPacketEvent);
    currentReadPacketEvent = NULL;
    if(cb){
        mqtt_cb_exec(cb,result);
    }
}
static void cbAsyncReadPacketStep3(int result,void *data){
    if (!currentReadPacketEvent) {
        return;
    }
    if (result == currentReadPacketEvent->rem_len) {
        currentReadPacketEvent->header->byte = currentReadPacketEvent->c->readbuf[0];
        currentReadPacketEvent->result = currentReadPacketEvent->header->bits.type;
    }
    //readEventFinish();
    readPacketEventFinish();
    
}
static void cbAsyncReadPacketStep2(int result,void *data){
    if (!currentReadPacketEvent) {
        return;
    }
	//printf("\nread packet step 2,result:%d\n b:\n",result,currentReadPacketEvent->c->readbuf[1]);
    currentReadPacketEvent->len += MQTTPacket_encode(currentReadPacketEvent->c->readbuf +1, currentReadPacketEvent->rem_len);

    if (currentReadPacketEvent->rem_len <= 0) {
        currentReadPacketEvent->header->byte = currentReadPacketEvent->c->readbuf[0];
        currentReadPacketEvent->result = currentReadPacketEvent->header->bits.type;
        readPacketEventFinish();
        return;
    }
    asyncRead(currentReadPacketEvent->c, currentReadPacketEvent->c->readbuf + currentReadPacketEvent->len, currentReadPacketEvent->rem_len, left_ms(currentReadPacketEvent->timer), cbAsyncReadPacketStep3);
    
}
static void cbAsyncReadPacketStep1(int result,void *data){
    if (!currentReadPacketEvent) {
        return;
    }
	//printf("\nread packet step 1,result:%d b:\n",result,currentReadPacketEvent->c->readbuf[1]);
    if (result != 1) {
        readPacketEventFinish();
        return;
    }
    asyncDecodePacket(currentReadPacketEvent->c, &currentReadPacketEvent->rem_len, left_ms(currentReadPacketEvent->timer), cbAsyncReadPacketStep2);
}

void asyncReadPacket(Client *c,Timer *timer,MQTTAsyncCallbackFunc cb){
    if (currentReadPacketEvent){
        if (cb) {
            mqtt_cb_exec(cb,FAILURE);
        }
        return ;
    }
    memset(c->readbuf,0,c->readbuf_size);
    currentReadPacketEvent = (readPacketEvent *)DLINKMQ_MALLOC(sizeof(readPacketEvent));
    currentReadPacketEvent->c = c;
    currentReadPacketEvent->timer = timer;
    currentReadPacketEvent->len = 1;
    currentReadPacketEvent->result = FAILURE;
    
    currentReadPacketEvent->header = (MQTTHeader *)DLINKMQ_MALLOC(sizeof(MQTTHeader));
    currentReadPacketEvent->header->byte = 0;
    currentReadPacketEvent->rem_len = 0;
    currentReadPacketEvent->step = 1;
    currentReadPacketEvent->cb = cb;
     mqtt_fmt_print("\n---mqtt begin read\n");
    asyncRead(c, c->readbuf, 1, left_ms(timer),cbAsyncReadPacketStep1);
    
}



//#pragma mark - keep alive;

static Client* keepAliveClient = NULL;
static MQTTAsyncCallbackFunc keepAliveCB = NULL;
void cbAsyncKeepAlive(int result,void *data){

	kal_uint32 ticks;
    MQTTAsyncCallbackFunc cb;


	//mqtt_cb_exec(cb,0);






    if (keepAliveClient != NULL && result == SUCCESS) {
        //keepAliveClient->ping_outstanding = 1;
        keepAliveClient = NULL;
		kal_get_time(&ticks);  
		lastSentTime_ms = kal_ticks_to_milli_secs(ticks);
    }
    if (keepAliveCB) {
        cb = keepAliveCB;
        keepAliveCB = NULL;
        mqtt_cb_exec(cb,SUCCESS);
    }
    
    
}
void dlinkmq_ping_timeout(void);
static kal_timerid ping_timeout_timer;
static void ping_timer_timeout(void *data){
	//printf("\nPING TIMEOUT!!!\n");
	dlinkmq_ping_timeout();
}

static void ping_timer_dispose(void){
	if(ping_timeout_timer){
		kal_cancel_timer(ping_timeout_timer);
	}
}

static void ping_timer_init(kal_uint32 ms){
	ping_timer_dispose();
	if(ping_timeout_timer == NULL){
		ping_timeout_timer = kal_create_timer("PingTimeoutTimer");
	}
	
	kal_set_timer(ping_timeout_timer,ping_timer_timeout,NULL,kal_milli_secs_to_ticks(ms),0);
}


void asyncKeepAlive(Client *c,MQTTAsyncCallbackFunc cb){
    int len,tmp;
	int shouldPing = 0;
	kal_uint32 ticks,currentTime_ms,sentInteval;

	//mqtt_cb_exec(cb,0);

    if (c->keepAliveInterval == 0) {
        if (cb) {
            mqtt_cb_exec(cb,SUCCESS);
        }
        return;
    }
	keepAliveClient = c;
     keepAliveCB = cb;
	kal_get_time(&ticks);  
	currentTime_ms = kal_ticks_to_milli_secs(ticks);

	sentInteval = currentTime_ms - lastSentTime_ms;
	//tmp = sentInteval - c->keepAliveInterval;

	tmp = sentInteval - 100;
	
	if(tmp >= 0){
		shouldPing = 1;

	}
	//mqtt_fmt_print("\ncurrent time:%d\n",currentTime_ms);
	
	
	//shouldPing = 1;
    if (shouldPing == 1 && !c->ping_outstanding) {
		
        Timer timer;
        InitTimer(&timer);
        countdown_ms(&timer, 1000);
		//mqtt_fmt_print("\nwill ping client\n");
        len = MQTTSerialize_pingreq(c->buf, c->buf_size);
        if (len <= 0) {
			if (cb) {
				//mqtt_fmt_print("\n- 2.5--01");
                mqtt_cb_exec(cbAsyncKeepAlive,0);
            }
            return;
		}
		//mqtt_fmt_print("\n- 2.5--04");

		mqtt_fmt_print("\n---mqtt ping client");
		c->ping_outstanding = 1;
		ping_timer_init(c->keepAliveInterval / 2);
		asyncSendPacket(c, len, &timer, cbAsyncKeepAlive);
		//mqtt_fmt_print("\n- 2.5--05");
        return;
    }
	//mqtt_fmt_print("\n skip ping,next ping time:%d",-tmp);
    if (cb) {
        mqtt_cb_exec(cb,SUCCESS);
    }
}

//#pragma mark - cycle

typedef struct cycleEvent{
    Client* c;
    Timer* timer;
    MQTTAsyncCallbackFunc cb;
    int result;
    int packet_type;
}cycleEvent;


static cycleEvent *currentCycle = NULL;

void cycleFinish(int ignored,void* data){
    MQTTAsyncCallbackFunc cb;
    int result;
    if (!currentCycle) {
        return;
    }
    if (currentCycle->result == SUCCESS){
        currentCycle->result = currentCycle->packet_type;
    }
    cb = currentCycle->cb;
    result = currentCycle->result;
    DLINKMQ_FREE(currentCycle);
    currentCycle = NULL;
    if (cb)
    {
		//printf("\nCYCLE finish with result:%d\n",result);
        mqtt_cb_exec(cb,result);
    }
    
}



 

void cycleGetResult(int result){
    currentCycle->result = result;
    if (result == FAILURE) {
        cycleFinish(0,NULL);
    }else{
        asyncKeepAlive(currentCycle->c,cycleFinish);
    }
}
 void cycleGetResultCB(int result,void *data){
	cycleGetResult(result);
 }
void cyclePubRecResultTransition(int result,void *data){
    if (result != SUCCESS) {
        result = FAILURE;
    }
    cycleGetResult(result);
    
}


void cyclePacketTypeDispatch(int packet_type,void* data){
    int len;
    if (!currentCycle) {
        return;
    }
    //mqtt_fmt_print("\nread packet:%d\n",packet_type);
    len = 0;
    //mqtt_fmt_print("\nrrrrrrrrr:%x\n",currentCycle->c->readbuf[1]);
    currentCycle->packet_type = packet_type;
    switch (packet_type)
    {
        case CONNACK:
        case PUBACK:
        case SUBACK:{
            break;
        }
        case PUBLISH:{
            MQTTString topicName;
            MQTTMessage msg;
            if (MQTTDeserialize_publish((unsigned char*)&msg.dup, (int*)&msg.qos, (unsigned char*)&msg.retained, (unsigned short*)&msg.id, &topicName,
                                        (unsigned char**)&msg.payload, (int*)&msg.payloadlen, currentCycle->c->readbuf, currentCycle->c->readbuf_size) != 1){
                cycleGetResult(SUCCESS);
                return;
            }
            deliverMessage(currentCycle->c, &topicName, &msg);
            if (msg.qos != QOS0)
            {
                if (msg.qos == QOS1){
                    len = MQTTSerialize_ack(currentCycle->c->buf, currentCycle->c->buf_size, PUBACK, 0, msg.id);
                }else if (msg.qos == QOS2){
                    len = MQTTSerialize_ack(currentCycle->c->buf, currentCycle->c->buf_size, PUBREC, 0, msg.id);
                }
                if (len <= 0){
                    cycleGetResult(FAILURE);
                }else{
                    asyncSendPacket(currentCycle->c, len, currentCycle->timer, cycleGetResultCB);
                }
                return;
                
            }
            break;
        }
        case PUBREC:{
            unsigned short mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, currentCycle->c->readbuf, currentCycle->c->readbuf_size) != 1){
                cycleGetResult(FAILURE);
                return;
            }
            if ((len = MQTTSerialize_ack(currentCycle->c->buf, currentCycle->c->buf_size, PUBREL, 0, mypacketid)) <= 0){
                cycleGetResult(FAILURE);
                return;
            }
            asyncSendPacket(currentCycle->c, len, currentCycle->timer, cyclePubRecResultTransition);
            return;
        }
        case PUBCOMP:{
            break;
        }
        case PINGRESP:{
			static long lastPingTime = 0;
			kal_uint32 ticks,currentTime_ms,sentInteval;

			kal_get_time(&ticks);  
			currentTime_ms = kal_ticks_to_milli_secs(ticks);


			if (lastPingTime != 0) {

				sentInteval = currentTime_ms - lastPingTime;
				mqtt_fmt_print("\n----mqtt PING  PING RESP!!  currentTime_ms = %ld, sentInteval = %ld\n", currentTime_ms, sentInteval);
			} 

			lastPingTime = currentTime_ms;
			
			
			ping_timer_dispose();
            currentCycle->c->ping_outstanding = 0;
            break;
        }
    }
    cycleGetResult(SUCCESS);
}

void asyncCycle(Client* c, Timer* timer,MQTTAsyncCallbackFunc cb){
    if (currentCycle) {
        mqtt_cb_exec(cb,FAILURE);
        return;
    }
    currentCycle = (cycleEvent *)DLINKMQ_MALLOC(sizeof(cycleEvent));
    currentCycle->c = c;
    currentCycle->timer = timer;
    currentCycle->cb = cb;
     mqtt_fmt_print("\n---mqtt cycle-read packet\n");
    asyncReadPacket(c, timer, cyclePacketTypeDispatch);
}

//#pragma mark - wait for
typedef struct waitForEvent{
    Client *c;
    int packet_type;
    Timer *timer;
    MQTTAsyncCallbackFunc cb;
}waitForEvent;
static waitForEvent *currentWaitEvent = NULL;

void waitFinish(int result){
    MQTTAsyncCallbackFunc cb;
    if (!currentWaitEvent) {
        return;
    }

    cb = currentWaitEvent->cb;
    DLINKMQ_FREE(currentWaitEvent);
    currentWaitEvent = NULL;
    if(cb){
        mqtt_cb_exec(cb,result);
    }
}

void waitNext(int result,void *data){
    if (!currentWaitEvent) {
        return;
    }
    if (result == FAILURE)
    {
        waitFinish(FAILURE);
        return;
    }
    
    if (expired(currentWaitEvent->timer)) {
       // waitFinish(FAILURE);
       // return;
    }
    if (result == currentWaitEvent->packet_type) {
        waitFinish(result);
        return;
    }
    asyncCycle(currentWaitEvent->c, currentWaitEvent->timer, waitNext);
}

void asyncWaitFor(Client* c, int packet_type, Timer* timer,MQTTAsyncCallbackFunc cb){
    if (currentWaitEvent) {
        if (cb) {
            mqtt_cb_exec(cb,FAILURE);
        }
        return;
    }
    currentWaitEvent = (waitForEvent *)DLINKMQ_MALLOC(sizeof(waitForEvent));
    currentWaitEvent->c = c;
    currentWaitEvent->packet_type = packet_type;
    currentWaitEvent->timer = timer;
    currentWaitEvent->cb = cb;
    if (expired(timer)) {
      // waitFinish(FAILURE);
        //return;
    }
    asyncCycle(c, timer, waitNext);
}

//#pragma mark - Yield

typedef struct  MQTTYieldEvent{
    Client *c;
    Timer *timer;
    MQTTAsyncCallbackFunc cb;
}MQTTYieldEvent;

static MQTTYieldEvent *currentYieldEvent = NULL;
void MQTTYieldEventFinish(result){
    MQTTAsyncCallbackFunc cb;
    if (!currentYieldEvent) {
        return;
    }
    cb = currentYieldEvent->cb;
    DLINKMQ_FREE(currentYieldEvent->timer);
    DLINKMQ_FREE(currentYieldEvent);
    currentYieldEvent = NULL;
    if (cb) {
        mqtt_cb_exec(cb,result);
    }
}




void MQTTYieldEventDoNext(int result,void* data){

    if (!currentYieldEvent) {
        return;
    }

	MQTTYieldEventFinish(result);
	/*
    if (result == FAILURE) {
        MQTTYieldEventFinish(FAILURE);
        return;
    }
	printf("yield cycle start!");
    asyncCycle(currentYieldEvent->c, currentYieldEvent->timer, MQTTYieldEventDoNext);
	*/
}

void MQTTAsyncYield(Client* c, int timeout_ms,MQTTAsyncCallbackFunc cb){
    if (currentYieldEvent) {
        if (cb) {
            mqtt_cb_exec(cb,FAILURE);
        }
        return;
    }

    currentYieldEvent = (MQTTYieldEvent *)DLINKMQ_MALLOC(sizeof(MQTTYieldEvent));
    currentYieldEvent->c = c;
    currentYieldEvent->cb = cb;
    currentYieldEvent->timer = (Timer *)DLINKMQ_MALLOC(sizeof(Timer));
    InitTimer(currentYieldEvent->timer);
    countdown_ms(currentYieldEvent->timer, timeout_ms);
    //MQTTYieldEventDoNext(SUCCESS);
	mqtt_fmt_print("\n---mqtt CYCLE begin!\n");
	asyncCycle(currentYieldEvent->c, currentYieldEvent->timer, MQTTYieldEventDoNext);
}

//#pragma mark - connect

typedef struct MQTTConnectAction{
    Client *c;
    Timer *connectTimer;
    MQTTAsyncCallbackFunc cb;
}MQTTConnectAction;

static MQTTConnectAction *currentConnectAction = NULL;


void MQTTAsyncConnectFinish(int result){
    MQTTAsyncCallbackFunc cb;
    if (!currentConnectAction) {
        return;
    }
    if (result == SUCCESS) {
        currentConnectAction->c->isconnected = 1;
    }
    cb = currentConnectAction->cb;
    DLINKMQ_FREE(currentConnectAction->connectTimer);
    DLINKMQ_FREE(currentConnectAction);
    currentConnectAction = NULL;
    if (cb) {
        mqtt_cb_exec(cb,result);
    }
}


void MQTTAsyncConnectWaitForConnack(int result,void *data){
    unsigned char connack_rc = 255;
    char sessionPresent = 0;
    if (!currentConnectAction) {
        return;
    }
    if (result != CONNACK) {
        MQTTAsyncConnectFinish(FAILURE);
		return;
    }

    if (MQTTDeserialize_connack((unsigned char*)&sessionPresent, &connack_rc, currentConnectAction->c->readbuf, currentConnectAction->c->readbuf_size) == 1){
        MQTTAsyncConnectFinish(connack_rc);
    }else{
        MQTTAsyncConnectFinish(FAILURE);
    }
}

void MQTTAsyncConnectSendPacketCheck(int result,void *data){
    if (!currentConnectAction) {
        return;
    }
    if (result != SUCCESS) {
        MQTTAsyncConnectFinish(FAILURE);
        return;
    }
    asyncWaitFor(currentConnectAction->c, CONNACK, currentConnectAction->connectTimer, MQTTAsyncConnectWaitForConnack);
}

void MQTTAsyncConnect(Client *c, MQTTPacket_connectData* options,MQTTAsyncCallbackFunc cb){
    MQTTPacket_connectData default_options = MQTTPacket_connectData_initializer;
    int len = 0;
	if( !c || c->isconnected){
		if (cb) {
            mqtt_cb_exec(cb,FAILURE);
        }
        return;
	}
    if (currentConnectAction ) {
		DLINKMQ_FREE(currentConnectAction->connectTimer);
		DLINKMQ_FREE(currentConnectAction);
		currentConnectAction = NULL;
		if(currentSendingEvent){
			DLINKMQ_FREE(currentSendingEvent);
			currentSendingEvent = NULL;
		}
    }
    
    
    if (options == 0){
        options = &default_options;
    }
    c->keepAliveInterval = options->keepAliveInterval;
    countdown(&c->ping_timer, c->keepAliveInterval);
    if ((len = MQTTSerialize_connect(c->buf, c->buf_size, options)) <= 0){
        if (cb) {
            mqtt_cb_exec(cb,FAILURE);
        }
        return;
    }
    currentConnectAction = (MQTTConnectAction *)DLINKMQ_MALLOC(sizeof(MQTTConnectAction));
    currentConnectAction->connectTimer = (Timer *)DLINKMQ_MALLOC(sizeof(Timer));
    InitTimer(currentConnectAction->connectTimer);
    countdown_ms(currentConnectAction->connectTimer, c->command_timeout_ms);
    currentConnectAction->c = c;
    currentConnectAction->cb = cb;
    asyncSendPacket(c, len, currentConnectAction->connectTimer, MQTTAsyncConnectSendPacketCheck);
}

//#pragma mark - subscribe

typedef struct MQTTSubscribeAction{
    Client *c;
    Timer *subscribeTimer;
    const char* topicFilter;
    messageHandler messageHandler;
    MQTTAsyncCallbackFunc cb;
}MQTTSubscribeAction;

static MQTTSubscribeAction *currentSubscribeAction = NULL;

void MQTTAsyncSubscribeFinish(int result){
    MQTTAsyncCallbackFunc cb;
    if (!currentSubscribeAction) {
        return;
    }
    cb = currentSubscribeAction->cb;
    DLINKMQ_FREE(currentSubscribeAction->subscribeTimer);
    DLINKMQ_FREE(currentSubscribeAction);
    currentSubscribeAction = NULL;
    if (cb) {
        mqtt_cb_exec(cb,result);
    }
}


void MQTTAsyncSubscribeWaitForSuback(int result,void *data){
    unsigned short mypacketid;
    int rc,count,grantedQoS;
    if (!currentSubscribeAction) {
        return;
    }
    if (result != SUBACK) {
        MQTTAsyncSubscribeFinish(FAILURE);
        return;
    }
    rc = FAILURE;
    count = 0, grantedQoS = -1;
    
    if (MQTTDeserialize_suback(&mypacketid, 1, &count, &grantedQoS, currentSubscribeAction->c->readbuf, currentSubscribeAction->c->readbuf_size) == 1){
        rc = grantedQoS; // 0, 1, 2 or 0x80
    }
    if (rc != 0x80)
    {
        int i;
        for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
        {
            if (currentSubscribeAction->c->messageHandlers[i].topicFilter == 0)
            {
                
                currentSubscribeAction->c->messageHandlers[i].topicFilter = currentSubscribeAction->topicFilter;
                currentSubscribeAction->c->messageHandlers[i].fp = currentSubscribeAction->messageHandler;
                rc = 0;
                break;
            }
        }
    }
    MQTTAsyncSubscribeFinish(rc);
    
}

void MQTTAsyncSubscribeSendPacketCheck(int result,void *data){
    if (!currentSubscribeAction) {
        return;
    }
    if (result != SUCCESS) {
        MQTTAsyncSubscribeFinish(FAILURE);
        return;
    }
    asyncWaitFor(currentSubscribeAction->c, SUBACK, currentSubscribeAction->subscribeTimer, MQTTAsyncSubscribeWaitForSuback);
}


void MQTTAsyncSubscribe(Client* c, const char* topicFilter, enum QoS qos, messageHandler messageHandler,MQTTAsyncCallbackFunc cb){
    int len = 0;
    MQTTString topic = MQTTString_initializer;
    if (currentSubscribeAction || !c->isconnected) {
        if (cb) {
            mqtt_cb_exec(cb,FAILURE);
        }
        return;
    }
    
    topic.cstring = (char *)topicFilter;
    currentSubscribeAction = (MQTTSubscribeAction *)DLINKMQ_MALLOC(sizeof(MQTTSubscribeAction));
    currentSubscribeAction->subscribeTimer = (Timer *)DLINKMQ_MALLOC(sizeof(Timer));
    currentSubscribeAction->topicFilter = topicFilter;
    currentSubscribeAction->messageHandler = messageHandler;
	currentSubscribeAction->c = c;
	currentSubscribeAction->cb = cb;
    InitTimer(currentSubscribeAction->subscribeTimer);
    countdown_ms(currentSubscribeAction->subscribeTimer, c->command_timeout_ms);
    len = MQTTSerialize_subscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic, (int*)&qos);
    if (len <= 0) {
        if (cb) {
            mqtt_cb_exec(cb,FAILURE);
        }
        return;
    }
    asyncSendPacket(c, len, currentSubscribeAction->subscribeTimer, MQTTAsyncSubscribeSendPacketCheck);
}



//#pragma mark - unsubscribe


typedef struct MQTTUnsubscribeAction{
    Client *c;
    Timer *unsubscribeTimer;
    MQTTAsyncCallbackFunc cb;
}MQTTUnsubscribeAction;

static MQTTUnsubscribeAction *currentUnsubscribeAction = NULL;

void MQTTAsyncUnsubscribeFinish(int result){
    MQTTAsyncCallbackFunc cb;
    if (!currentUnsubscribeAction) {
        return;
    }
    cb = currentUnsubscribeAction->cb;
    DLINKMQ_FREE(currentUnsubscribeAction->unsubscribeTimer);
    DLINKMQ_FREE(currentUnsubscribeAction);
    currentUnsubscribeAction = NULL;
    if (cb) {
        mqtt_cb_exec(cb,result);
    }
}


void MQTTAsyncUnsubscribeWaitForUnsuback(int result,void *data){
    int rc = FAILURE;
    unsigned short mypacketid;
    if (!currentUnsubscribeAction) {
        return;
    }
    if (result != UNSUBACK) {
        MQTTAsyncUnsubscribeFinish(FAILURE);
        return;
    }
    if (MQTTDeserialize_unsuback(&mypacketid, currentUnsubscribeAction->c->readbuf, currentUnsubscribeAction->c->readbuf_size) == 1){
        result = 0;
    }
    MQTTAsyncUnsubscribeFinish(rc);
    
}

void MQTTAsyncUnsubscribeSendPacketCheck(int result,void *data){
    if (!currentUnsubscribeAction) {
        return;
    }
    if (result != SUCCESS) {
        MQTTAsyncUnsubscribeFinish(FAILURE);
        return;
    }
    asyncWaitFor(currentUnsubscribeAction->c, SUBACK, currentUnsubscribeAction->unsubscribeTimer, MQTTAsyncUnsubscribeWaitForUnsuback);
}


void MQTTAsyncUnsubscribe(Client* c, const char* topicFilter,MQTTAsyncCallbackFunc cb){
    MQTTString topic = MQTTString_initializer;
    int len = 0;
    if (currentUnsubscribeAction || !c->isconnected) {
        if (cb) {
            mqtt_cb_exec(cb,FAILURE);
        }
        return;
    }
    
    topic.cstring = (char *)topicFilter;
    currentUnsubscribeAction = (MQTTUnsubscribeAction *)DLINKMQ_MALLOC(sizeof(MQTTUnsubscribeAction));
    currentUnsubscribeAction->unsubscribeTimer = (Timer *)DLINKMQ_MALLOC(sizeof(Timer));
    currentUnsubscribeAction->c = c;
    InitTimer(currentUnsubscribeAction->unsubscribeTimer);
    countdown_ms(currentUnsubscribeAction->unsubscribeTimer, c->command_timeout_ms);
    len = MQTTSerialize_unsubscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic);
    if (len <= 0) {
        if (cb) {
            mqtt_cb_exec(cb,FAILURE);
        }
        return;
    }
    asyncSendPacket(c, len, currentUnsubscribeAction->unsubscribeTimer, MQTTAsyncUnsubscribeSendPacketCheck);
}

//#pragma mark - publish

typedef struct MQTTPublishAction{
    Client* c;
    int waitType;
    MQTTMessage* message;
    Timer *publishTimer;
    MQTTAsyncCallbackFunc cb;
}MQTTPublishAction;

static MQTTPublishAction *currentPublishAction = NULL;

void MQTTAsyncPublishFinish(int result){
    MQTTAsyncCallbackFunc cb;
	MQTTMessage *msg;
    if(!currentPublishAction){
        return;
    }
    cb = currentPublishAction->cb;
	msg=currentPublishAction->message;
    DLINKMQ_FREE(currentPublishAction->publishTimer);
    DLINKMQ_FREE(currentPublishAction);
    currentPublishAction = NULL;
    if (cb) {
        //mqtt_cb_exec(cb,result);
		mqtt_cb_exec_with_data(cb,result,msg);
		//cb(result,msg);
    }
}


void MQTTAsyncPublishWait(int result,void *data){
    unsigned short mypacketid;
    unsigned char dup, type;
    if (!currentPublishAction) {
        return;
    }
    if (result != currentPublishAction->waitType) {
        MQTTAsyncPublishFinish(FAILURE);
        return;
    }
    if (MQTTDeserialize_ack(&type, &dup, &mypacketid, currentPublishAction->c->readbuf, currentPublishAction->c->readbuf_size) != 1){
        MQTTAsyncPublishFinish(FAILURE);
        return;
    }
    MQTTAsyncPublishFinish(SUCCESS);
}

void MQTTAsyncPublishSendPacketCheck(int result,void *data){
    if (!currentPublishAction) {
        return;
    }
    if (result != SUCCESS) {
        MQTTAsyncPublishFinish(FAILURE);
        return;
    }
    if (result == SUCCESS) {
        MQTTAsyncPublishFinish(SUCCESS);
        return;
    }
    if (currentPublishAction->message->qos == QOS1) {
        currentPublishAction->waitType = PUBACK;
        asyncWaitFor(currentPublishAction->c, currentPublishAction->waitType, currentPublishAction->publishTimer, MQTTAsyncPublishWait);
        return;
    }
    if (currentPublishAction->message->qos == QOS2) {
        currentPublishAction->waitType = PUBCOMP;
        asyncWaitFor(currentPublishAction->c, currentPublishAction->waitType, currentPublishAction->publishTimer, MQTTAsyncPublishWait);
        return;
    }
}

void MQTTAsyncPublish(Client* c, const char* topicName, MQTTMessage* message,MQTTAsyncCallbackFunc cb){
    int len = 0;
    MQTTString topic = MQTTString_initializer;
    if (!c->isconnected || currentPublishAction){
        if (cb) {
            //mqtt_cb_exec(cb,FAILURE);
            //cb(FAILURE,message);
			mqtt_cb_exec_with_data(cb,-2,message);
            //cb(-2,message);
        }
        return;
    }
    topic.cstring = (char *)topicName;
    if (message->qos == QOS1 || message->qos == QOS2){
        message->id = getNextPacketId(c);
    }
    len = MQTTSerialize_publish(c->buf, c->buf_size, 0, message->qos, message->retained, message->id,
                                topic, (unsigned char*)message->payload, message->payloadlen);
    if(len <= 0){
        if (cb) {
            //mqtt_cb_exec(cb,FAILURE);
            mqtt_cb_exec_with_data(cb,FAILURE,message);
        }
        return;
    }
    currentPublishAction = (MQTTPublishAction *)DLINKMQ_MALLOC(sizeof(MQTTPublishAction));
    currentPublishAction->message = message;
    currentPublishAction->publishTimer = (Timer *)DLINKMQ_MALLOC(sizeof(Timer));
	currentPublishAction->c = c;
	currentPublishAction->cb = cb;
    InitTimer(currentPublishAction->publishTimer);
    countdown_ms(currentPublishAction->publishTimer, c->command_timeout_ms);
    asyncSendPacket(c, len, currentPublishAction->publishTimer, MQTTAsyncPublishSendPacketCheck);
}

//#pragma mark - disconnect

typedef struct MQTTDisconnectAction{
    Client* c;
    MQTTAsyncCallbackFunc cb;
}MQTTDisconnectAction;

static MQTTDisconnectAction *currentDisconnectAction = NULL;

void MQTTAsyncDisconnectSendPacketCheck(int result,void *data){
    MQTTAsyncCallbackFunc cb;
    if (!currentDisconnectAction) {
        return;
    }
    currentDisconnectAction->c->isconnected = 0;
    cb = currentDisconnectAction->cb;
    DLINKMQ_FREE(currentDisconnectAction);
    currentDisconnectAction = NULL;
    if (cb) {
        mqtt_cb_exec(cb,SUCCESS);

    }
}


void MQTTAsyncDisconnect(Client *c,MQTTAsyncCallbackFunc cb){
    int len;
    Timer timer;
    if (currentDisconnectAction) {
        if (cb) {
            mqtt_cb_exec(cb,FAILURE);
        }
        return;
    }
    
    len = MQTTSerialize_disconnect(c->buf, c->buf_size);
    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);
    if (len > 0){
		currentDisconnectAction = (MQTTDisconnectAction *)DLINKMQ_MALLOC(sizeof(MQTTDisconnectAction));
		currentDisconnectAction->c = c;
		currentDisconnectAction->cb = cb;
        asyncSendPacket(c, len, &timer, MQTTAsyncDisconnectSendPacketCheck);
    }else{
        if (cb) {
            mqtt_cb_exec(cb,FAILURE);
        }
    }
}



we_int DlinkmqClient_Init(we_handle *phDlinkmqClientHandle)
{
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;
	we_int netRet = DlinkMQ_ERROR_CODE_FAIL;

	Client *pstClient = (Client *)DLINKMQ_MALLOC(sizeof(Client));

	if (pstClient == NULL) {

		return ret;
	}


	ret = DlinkMQ_ERROR_CODE_SUCCESS;

	*phDlinkmqClientHandle = pstClient;


	return ret;
}

we_void DlinkmqClient_Destroy(we_handle hDlinkmqClientHandle)
{
	Client *pstClient = (Client *)hDlinkmqClientHandle;

	if(pstClient != NULL) {

		DLINKMQ_FREE(pstClient);
	}
}