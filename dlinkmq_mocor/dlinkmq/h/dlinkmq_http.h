#ifndef dlinkmq_http_h
#define dlinkmq_http_h


#include "dlinkmq_types.h"
#include "MQTTMTK.h"
#include "MQTTTrace.h"
#include "dlinkmq_msg.h"

typedef struct tagSt_DlinkmqHttp
{
	Network *pstNetWork;
	we_char *pSendBuff;
	we_int iSentSize;
	we_int iBuffSize;
	we_bool isInitFin; //初始化完成

}St_DlinkmqHttp, *P_St_DlinkmqHttp;

we_int DlinkmqHttp_Init(we_handle *phDlinkmqHttpHandle);

we_void DlinkmqHttp_Destroy(we_handle hDlinkmqHttpHandle);

we_int DlinkmqHttp_PostJsonToSever(we_handle hDlinkmqHttpHandle, we_char *json_data);

#endif /* dlinkmq_http_h */