#if !defined(__MQTT_TASK_H__)
#define __MQTT_TASK_H__

#include "MQTTMTK.h"
#include "MQTTClient.h"

#define MQTT_PACK_RECV_BUF_SIZE	(4096)
#define MQTT_PACK_SEND_BUF_SIZE	(4096)
#define MQTT_PACK_RSP_BUF_SIZE	(4096)

#define MQTT_APP_RECV_BUF_SIZE	(4096)
#define MQTT_APP_SEND_BUF_SIZE	(4096)
#define MQTT_APP_RSP_BUF_SIZE	(4096)



//---------------------------------------------
//---------------------------------------------


//---------------------------------------------
//---------------------------------------------
int mqtt_data_is_sending(void);

int mqtt_is_busy(void);
int mqtt_st_is_busy(void);

int mqtt_has_pending_data_package(void);

//---------------------------------------------
//---------------------------------------------

void mqtt_task_async_linke_build(void);
void mqtt_task_async_link_rebuild(void);
void mqtt_task_async_link_built(void);
void mqtt_task_async_active_service(void);
void mqtt_task_async_ask_idle(void);
void mqtt_task_async_soc_close_notify(void);
void mqtt_task_async_mqtt_pause(void);
void mqtt_task_async_mqtt_stop(void);

void mqtt_task_delay_reconnect(kal_uint32 timeout);

//---------------------------------------------
//---------------------------------------------



#endif

