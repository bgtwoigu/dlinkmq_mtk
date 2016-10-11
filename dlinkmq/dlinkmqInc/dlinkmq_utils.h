#ifndef dlinkmq_utils_h
#define dlinkmq_utils_h

#include "dlinkmq_types.h"
#include "string.h"
#include "stdlib.h"

//#define dotlink_host    "192.168.1.53"
//#define dotlink_port    4004
#define dotlink_host    "139.224.11.153"
#define dotlink_port    4004
#define dotlink_path    "/mqtt/info"
#define upload_host    "139.224.11.153"
#define upload_port    8079

#define MQTT_STACK_TIMER_ID    (146)
#define MQTT_STACK_INIT_TIMER_ID    (147)
#define MQTT_STACK_RECONNECT_TIMER_ID    (148)

#define MQTT_STACK_HTTP_CONNECT_TIMEROUT_ID    (149)
#define MQTT_STACK_CLIENT_CONNECT_TIMEROUT_ID    (150)
#define MQTT_STACK_PING_CONNECT_TIMEROUT_ID    (151)
#define MQTT_STACK_UPLOAD_CONNECT_TIMEROUT_ID    (152)
#define MQTT_STACK_RECORD_AUDIO_TIMEROUT_ID    (153)


#define WE_SOCKET_MAX_TCP_RECV_BUFFER_SIZE 4096
#define WE_SOCKET_MAX_UPLOAD_BUFFER_SIZE 1024

#define DLINKMQ_MQTT_COMMAND_TIMEOUT 100000
#define DLINKMQ_MQTT_READBUF_SIZE 256
#define DLINKMQ_MQTT_BUF_SIZE 256

#define DLINKMQ_MQTT_keepAliveInterval (1*60*1000)


typedef void(*MQTTAsyncCallbackFunc)(int result, void * data);

we_int32 dlinkmq_data_A_To_B(we_int8 *data, we_int32 indexA, we_int32 A, we_int32 B, we_int8 *out_data);

int char_in_string(char * p,char ch);

#endif /* dlinkmq_utils_h */
