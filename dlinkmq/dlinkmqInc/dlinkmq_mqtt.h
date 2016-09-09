#ifndef dlinkmq_mqtt_h
#define dlinkmq_mqtt_h


#include "dlinkmq_types.h"
#include "MQTTMTK.h"

typedef struct tagSt_DlinkmqMqtt
{
	Network *pstNetWork;

	we_uint8 *pcMqttBuf;
	we_uint8 *pcMqttReadBuf;

	we_int mqttStatus;

}St_DlinkmqMqtt, *P_St_DlinkmqMqtt;

we_int DlinkmqMqtt_Init(we_handle *phDlinkmqMqttHandle);

we_void DlinkmqMqtt_Destroy(we_handle hDlinkmqMqttHandle);

#endif /* dlinkmq_mqtt_h */