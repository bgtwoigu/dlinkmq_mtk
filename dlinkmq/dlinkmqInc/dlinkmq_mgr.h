#ifndef dlinkmq_mgr_h
#define dlinkmq_mgr_h

#include "dlinkmq_api.h"
#include "dlinkmq_utils.h"
#include "MQTTClient.h"
#include "dlinkmq_http.h"
#include "dlinkmq_mqtt.h"
#include "dlinkmq_upload.h"
#include "dlinkmq_msg.h"


typedef struct tagSt_DlinkmqMqttReg
{

	we_char *pcMqttCliendId;
	we_char *pcMqttUsername;
	we_char *pcMqttPassword;
	we_char *pcMqttServer;
	we_int	iMqttPort;
}St_DlinkmqMqttReg;

typedef struct tagSt_DlinkmqMqttSubPub
{
	we_char *pcPubTopic;
	we_char *pcSubTopic;
}St_DlinkmqMqttSubPub;

typedef struct tagSt_DlinkmqMgr
{
	St_DlinkmqMqttSubPub stMqttSubPub;
	
	St_DlinkmqMqttReg stMqttReg;

	Client  *pstMqClient;
	
	St_DlinkmqHttp *pstDlinkmqHttp;

	St_DlinkmqMqtt *pstDlinkmqMqtt;

	St_DlinkmqUpload*pstDlinkmqUpload;
	
	dlinkmq_device_info stDeviceInfo;
	dlinkmq_on_receive stRecvFunCB;
	
	
	
	
}St_DlinkmqMgr, *P_St_DlinkmqMgr;

we_int DlinkmqMgr_Init(we_handle *phDlinkmqMgrHandle);

we_void DlinkmqMgr_SetData(we_handle hDlinkmqMgrHandle, dlinkmq_device_info *pstDeviceInfo, dlinkmq_on_receive *pstRecvFunCB);

E_DlinkmqMsgModuleId DlinkmqMgr_GetEventIdBySockId(we_handle hDlinkmqMgrHandle, we_int sockId);

we_void DlinkmqMgr_Destroy(we_handle hDlinkmqMgrHandle);

dlinkmq_device_info * DlinkmqMgr_GetDeviceInfo(we_handle hDlinkmqMgrHandle);
dlinkmq_on_receive * DlinkmqMgr_GetRecvInfo(we_handle hDlinkmqMgrHandle);

we_void DlinkmqMgr_SetMqttRegInfo(we_handle hDlinkmqMgrHandle, St_DlinkmqMqttReg *pstMqttReg);

St_DlinkmqMqttReg * DlinkmqMgr_GetMqttRegInfo(we_handle hDlinkmqMgrHandle);

St_DlinkmqMqttSubPub * DlinkmqMgr_GetMqttSubPubInfo(we_handle hDlinkmqMgrHandle);

Client  * DlinkmqMgr_GetClient(we_handle hDlinkmqMgrHandle);

St_DlinkmqMqtt  * DlinkmqMgr_GetMqtt(we_handle hDlinkmqMgrHandle);

St_DlinkmqHttp  * DlinkmqMgr_GetHttp(we_handle hDlinkmqMgrHandle);

St_DlinkmqUpload* DlinkmqMgr_GetUpload(we_handle hDlinkmqMgrHandle);


#endif /* dlinkmq_mgr_h */
