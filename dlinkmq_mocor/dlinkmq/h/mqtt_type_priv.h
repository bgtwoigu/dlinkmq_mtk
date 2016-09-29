

#if !defined(__MQTT_TYPE_PRIV_H__)
#define __MQTT_TYPE_PRIV_H__


#include "kal_public_defs.h"



typedef struct {
	LOCAL_PARA_HDR
	kal_int32 status;
}mqtt_test_msg_t;

typedef struct
{
	kal_int8 linked;
	kal_int8 actived;
	kal_int8 is_busy;
	kal_int8 padding;
}mqtt_status_t;


typedef struct {
	LOCAL_PARA_HDR
	mqtt_status_t status;
}mqtt_status_msg_t;







#endif //  __MQTT_TYPE_PRIV_H__



