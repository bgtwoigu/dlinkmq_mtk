#ifndef dlinkmq_msg_h
#define dlinkmq_msg_h

#include "dlinkmq_api.h"
#include "dlinkmq_utils.h"

typedef enum
{
	E_MQ_MSG_MODULEID_BASE = -1,

	E_MQ_MSG_MODULEID_MGR = 0,
	E_MQ_MSG_MODULEID_HTTP,
	E_MQ_MSG_MODULEID_MQTT,
	E_MQ_MSG_MODULEID_UPLOAD,
	E_MQ_MSG_MODULEID_COUNT        /* The count of module ID */
} E_DlinkmqMsgModuleId;

typedef enum
{
	E_MQ_MSG_EVENTID_BASE = -1,


	E_MQ_MSG_EVENTID_HTTP_BASE = 0,
	E_MQ_MSG_EVENTID_NEW_HTTP,
	E_MQ_MSG_EVENTID_SOC_NOTIFY_IND,

	E_MQ_MSG_EVENTID_UPLOAD_BASE = 0,
	E_MQ_MSG_EVENTID_NEW_UPLOAD,

	E_MQ_MSG_EVENTID_MGR_BASE = 0,
	E_MQ_MSG_EVENTID_MGR_HTTP_RECONNECT,

	E_MQ_MSG_EVENTID_MQTT_BASE,
	E_MQ_MSG_EVENTID_MQTT_INIT,

	E_MQ_MSG_EVENTID_MAX        /* The count of module ID */
} E_DlinkmqMsgEventId;

#define DLINKMQ_MSG_MAX_MOD_NUM E_MQ_MSG_MODULEID_COUNT

typedef void (*FN_CbFreePrivateData)(we_void *pvParam1, we_void *pvParam2);

typedef struct tagSt_MQMsg
{
	we_uint32       uiMsgType;
	we_int32        iParam1;
	we_int32        iParam2;
	we_uint32       uiParam1;
	we_uint32       uiParam2;
	we_void         *pvParam1;
	we_void         *pvParam2;
	FN_CbFreePrivateData pfncbFreePrivateData;
} St_MQMsg;

typedef we_int Fn_DlinkmqMsgPorcess( we_handle, St_MQMsg * );

typedef struct tagSt_MQMsgNode{
	we_uint32 uiTgtModID;
	struct tagSt_MQMsgNode *pstNext;
	struct tagSt_MQMsg *pstMQMsg;
}St_MQMsgNode;


typedef struct tagSt_DlinkmqMsgHandle
{

	Fn_DlinkmqMsgPorcess *apcbMsgProcFunc[DLINKMQ_MSG_MAX_MOD_NUM];
	void *apvPrivData[DLINKMQ_MSG_MAX_MOD_NUM];
	struct tagSt_MQMsgNode *pstMsgNodeHead;
	struct tagSt_MQMsgNode *pstMsgNodeTail;


}St_DlinkmqMsgHandle, *P_St_DlinkmqMsgHandle;

we_int DlinkmqMsg_Init(we_handle *phDlinkmqMsgHandle);

we_int DlinkmqMsg_DispatchMsg(we_handle hDlinkmqMsgHandle);

we_bool DlinkmqMsg_HasMsg(we_handle hDlinkmqMsgHandle);


we_int DlinkmqMsg_RegisterProcess
(
 we_handle               hMsgHandle,
 E_DlinkmqMsgModuleId       eModId,
 Fn_DlinkmqMsgPorcess   *cbProcessFun,
 we_handle               hMyselfHandle
 );

we_int DlinkmqMsg_DeregisterProcess
(
 we_handle               hMsgHandle,
 E_DlinkmqMsgModuleId       eModId
 );

we_int DlinkmqMsg_PostMsg
(
 we_handle            hMsgHandle,
 E_DlinkmqMsgModuleId    eDstModId,
 we_int32            iMsgType,
 we_int32             iParam1,
 we_int32             iParam2,
 we_uint32            uiParam1,
 we_uint32            uiParam2,
 void                *pvParam1,
 void                *pvParam2
 );

we_int DlinkmqMsg_PrependMsg
(
 we_handle hMsgHandle,        
 E_DlinkmqMsgModuleId eTgtModID, 
 we_uint32 uiMsgID,           
 we_int32 iParam1,             
 we_int32 iParam2,
 we_uint32 uiParam1,
 we_uint32 uiParam2,
 void *pvParam1,
 void *pvParam2
 );


we_int DlinkmqMsg_SendMsg
(
 we_handle            hMsgHandle,
 E_DlinkmqMsgModuleId     eDstModId,
 we_int32            iMsgType,
 we_int32             iParam1,
 we_int32             iParam2,
 we_uint32            uiParam1,
 we_uint32            uiParam2,
 void                *pvParam1,
 void                *pvParam2
 );

we_void DlinkmqMsg_Destroy(we_handle phDlinkmqMsgHandle);


#endif /* dlinkmq_msg_h */