
#if !defined(__MQTT_TIMER_H__)
#define __MQTT_TIMER_H__


#include "mmi_platform.h"
#include "MQTTMTK.h"

typedef void (*mqtt_timer_cb_t)(void*);



typedef enum
{
	MQTT_TIMER_ID_NONE,
	MQTT_TIMER_ID_MQTT_REQ_TIMEOUT,
	MQTT_TIMER_ID_CONN_TIMEOUT,
	MQTT_TIMER_ID_SEND_TIMEOUT,
	MQTT_TIMER_ID_RECV_TIMEOUT,
	
	MQTT_TIMER_ID_RECONNECT,


	/* Imp: Please do not modify this, and last id */
	MQTT_MAX_TIMERS
}mqtt_timer_id_t;


#define MQTT_INVALID_TIMER_ID		((kal_uint16)MQTT_TIMER_ID_NONE)

void mqtt_frame_timer_init(void);

void mqtt_stop_timer(kal_uint16 timer_id);
kal_uint16 mqtt_start_timer(kal_uint16 timer_id, kal_uint32 milli_seconds, mqtt_timer_cb_t cb, void * arg);
int mqtt_timer_is_existed(kal_uint16 timer_id);

int mqtt_on_timer(kal_uint16 timer_index);


#endif /*__MQTT_TIMER_H__*/



