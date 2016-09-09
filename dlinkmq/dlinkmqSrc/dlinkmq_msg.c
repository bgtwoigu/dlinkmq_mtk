#include "dlinkmq_msg.h"
#include "MQTTMTK.h"

extern void mqtt_cb_exec(MQTTAsyncCallbackFunc cb,int result);
extern we_handle g_pstDlinkmqMsgHandle;
we_int DlinkmqMsg_Init(we_handle *phDlinkmqMsgHandle)
{
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;
	St_DlinkmqMsgHandle *pstMqMsgHandle = (St_DlinkmqMsgHandle *)DLINKMQ_MALLOC(sizeof(St_DlinkmqMsgHandle));

	if (pstMqMsgHandle == NULL) {

		return ret;
	}

	ret = DlinkMQ_ERROR_CODE_SUCCESS;


	*phDlinkmqMsgHandle = pstMqMsgHandle;


	return ret;
}

we_int DlinkmqMsg_DispatchMsg(we_handle hDlinkmqMsgHandle)
{
	St_DlinkmqMsgHandle *pstMqMsgHandle = (St_DlinkmqMsgHandle *)hDlinkmqMsgHandle;
	St_MQMsg *pstMQMsg = NULL;
	St_MQMsgNode *pstMQMsgNode = NULL;
	we_int32 iRet = DlinkMQ_ERROR_CODE_FAIL;

	if (NULL == pstMqMsgHandle)
	{
		return iRet;
	}

	if (NULL == pstMqMsgHandle->pstMsgNodeHead)
	{
		return DlinkMQ_ERROR_CODE_SUCCESS;
	}

	pstMQMsgNode = pstMqMsgHandle->pstMsgNodeHead;
	pstMqMsgHandle->pstMsgNodeHead = pstMQMsgNode->pstNext;

	if (NULL == pstMqMsgHandle->pstMsgNodeHead)
	{
		pstMqMsgHandle->pstMsgNodeTail = NULL;
	}

	pstMQMsg = pstMQMsgNode->pstMQMsg;

	if (NULL == pstMqMsgHandle->apcbMsgProcFunc[pstMQMsgNode->uiTgtModID])
	{
		DLINKMQ_FREE(pstMQMsgNode);
		DLINKMQ_FREE(pstMQMsg);

		return iRet;
	}
	iRet = (pstMqMsgHandle->apcbMsgProcFunc[pstMQMsgNode->uiTgtModID])(
			pstMqMsgHandle->apvPrivData[pstMQMsgNode->uiTgtModID],
			pstMQMsg); 


	DLINKMQ_FREE(pstMQMsgNode);

	if (pstMQMsg->pfncbFreePrivateData != NULL)
	{
		pstMQMsg->pfncbFreePrivateData(pstMQMsg->pvParam1, pstMQMsg->pvParam2);
	}
	DLINKMQ_FREE(pstMQMsg);
		

	return iRet;

}

we_bool DlinkmqMsg_HasMsg(we_handle hDlinkmqMsgHandle)
{
	St_DlinkmqMsgHandle *pstMQMsgHandle = (St_DlinkmqMsgHandle *)hDlinkmqMsgHandle;


	if(NULL == pstMQMsgHandle)
	{
		return FALSE;
	}

	if(NULL == pstMQMsgHandle->pstMsgNodeHead)
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}


we_int DlinkmqMsg_RegisterProcess
(
 we_handle               hMsgHandle,
 E_DlinkmqMsgModuleId       eModuleID,
 Fn_DlinkmqMsgPorcess   *pcbMsgProcFunc,
 we_handle               pvPrivData
 )
{
	St_DlinkmqMsgHandle *pstMQMsgHandle = (St_DlinkmqMsgHandle *)hMsgHandle;

	if(NULL == pstMQMsgHandle ||NULL == pcbMsgProcFunc || NULL == pvPrivData )
	{
		return DlinkMQ_ERROR_CODE_FAIL;
	}

	if(eModuleID < E_MQ_MSG_MODULEID_MGR || eModuleID >= E_MQ_MSG_MODULEID_COUNT)
	{
		return DlinkMQ_ERROR_CODE_FAIL;
	}


	pstMQMsgHandle->apcbMsgProcFunc[eModuleID] = pcbMsgProcFunc;
	pstMQMsgHandle->apvPrivData[eModuleID] = pvPrivData;

	return DlinkMQ_ERROR_CODE_SUCCESS;
}
we_int DlinkmqMsg_DeregisterProcess
(
 we_handle               hMsgHandle,
 E_DlinkmqMsgModuleId       eModuleID
 )
{
	St_DlinkmqMsgHandle *pstMQMsgHandle = (St_DlinkmqMsgHandle *)hMsgHandle;

	if(NULL == pstMQMsgHandle)
	{
		return DlinkMQ_ERROR_CODE_FAIL;
	}

	if(eModuleID < E_MQ_MSG_MODULEID_MGR || eModuleID >= E_MQ_MSG_MODULEID_COUNT)
	{
		return DlinkMQ_ERROR_CODE_FAIL;
	}

	pstMQMsgHandle->apcbMsgProcFunc[eModuleID] = NULL;
	pstMQMsgHandle->apvPrivData[eModuleID] = NULL;

	return DlinkMQ_ERROR_CODE_SUCCESS;
}



we_void DlinkmqMsg_RunMsg(int result, we_void *data) 
{
	DlinkmqMsg_DispatchMsg(g_pstDlinkmqMsgHandle);
}

we_int DlinkmqMsg_PostMsg
(
 we_handle hMsgHandle,                
 E_DlinkmqMsgModuleId eTgtModID,             
 we_int32 iMsgID,                   
 we_int32 iParam1,                        
 we_int32 iParam2,
 we_uint32 uiParam1,
 we_uint32 uiParam2,
 we_void *pvParam1,
 we_void *pvParam2
 )
{
	St_DlinkmqMsgHandle *pstMQMsgHandle = (St_DlinkmqMsgHandle *)hMsgHandle;
	St_MQMsgNode *pstMQMsgNode = NULL;
	St_MQMsg *pstMQMsg = NULL;


	if(NULL == pstMQMsgHandle)
	{
		return DlinkMQ_ERROR_CODE_FAIL;
	}

	if(eTgtModID < E_MQ_MSG_MODULEID_MGR || eTgtModID > E_MQ_MSG_MODULEID_COUNT)
	{
		return DlinkMQ_ERROR_CODE_FAIL;
	}

	pstMQMsgNode = (St_MQMsgNode *)DLINKMQ_MALLOC(sizeof(St_MQMsgNode));
	if(NULL == pstMQMsgNode)
	{
		return DlinkMQ_ERROR_CODE_FAIL;
	}
	pstMQMsg = (St_MQMsg *)DLINKMQ_MALLOC(sizeof(St_MQMsg));

	if(NULL == pstMQMsg)
	{
		DLINKMQ_FREE(pstMQMsgNode);
		return DlinkMQ_ERROR_CODE_FAIL;
	}


	pstMQMsg->iParam1 = iParam1;
	pstMQMsg->iParam2 = iParam2;
	pstMQMsg->uiParam1 = uiParam1;
	pstMQMsg->uiParam2 = uiParam2;
	pstMQMsg->pvParam1 = pvParam1;
	pstMQMsg->pvParam2 = pvParam2;
	pstMQMsg->uiMsgType = (we_uint32)iMsgID;

	pstMQMsgNode->pstMQMsg = pstMQMsg;
	pstMQMsgNode->uiTgtModID = (we_uint32)eTgtModID;
	pstMQMsgNode->pstNext = NULL;

	/* Add message to the tail of message queue */
	if(pstMQMsgHandle->pstMsgNodeTail == NULL)
	{
		pstMQMsgHandle->pstMsgNodeHead = pstMQMsgNode;
		pstMQMsgHandle->pstMsgNodeTail = pstMQMsgNode;
	}
	else
	{
		pstMQMsgHandle->pstMsgNodeTail->pstNext = pstMQMsgNode;
		pstMQMsgHandle->pstMsgNodeTail = pstMQMsgNode;
	}

	//MQ_SendResumePrimitive( g_hMQHandle );

	mqtt_cb_exec(DlinkmqMsg_RunMsg, DlinkMQ_ERROR_CODE_SUCCESS);

	return DlinkMQ_ERROR_CODE_SUCCESS;
}

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
 )
{
	St_DlinkmqMsgHandle *pstMQMsgHandle = (St_DlinkmqMsgHandle *)hMsgHandle;
	St_MQMsgNode *pstMQMsgNode = NULL;
	St_MQMsg *pstMQMsg = NULL;


	if(NULL == pstMQMsgHandle)
	{
		return DlinkMQ_ERROR_CODE_FAIL;
	}

	if(eTgtModID < E_MQ_MSG_MODULEID_MGR || eTgtModID >= E_MQ_MSG_MODULEID_COUNT)
	{
		return DlinkMQ_ERROR_CODE_FAIL;
	}

	pstMQMsgNode = (St_MQMsgNode *)DLINKMQ_MALLOC(sizeof(St_MQMsgNode));
	if (NULL == pstMQMsgNode)
	{
		return DlinkMQ_ERROR_CODE_FAIL;
	}
	pstMQMsg = (St_MQMsg *)DLINKMQ_MALLOC(sizeof(St_MQMsg));
	if (NULL == pstMQMsg)
	{
		DLINKMQ_FREE(pstMQMsgNode);
		return DlinkMQ_ERROR_CODE_FAIL;
	}

	pstMQMsg->iParam1   = iParam1;
	pstMQMsg->iParam2   = iParam2;
	pstMQMsg->uiParam1  = uiParam1;
	pstMQMsg->uiParam2  = uiParam2;
	pstMQMsg->pvParam1  = pvParam1;
	pstMQMsg->pvParam2  = pvParam2;
	pstMQMsg->uiMsgType = uiMsgID;

	pstMQMsgNode->pstMQMsg = pstMQMsg;
	pstMQMsgNode->uiTgtModID = (we_uint32)eTgtModID;
	pstMQMsgNode->pstNext = NULL;

	if(pstMQMsgHandle->pstMsgNodeTail == NULL)
	{
		pstMQMsgHandle->pstMsgNodeHead = pstMQMsgNode;
		pstMQMsgHandle->pstMsgNodeTail = pstMQMsgNode;
	}
	else
	{   pstMQMsgNode->pstNext = pstMQMsgHandle->pstMsgNodeHead;
		pstMQMsgHandle->pstMsgNodeHead = pstMQMsgNode;
	}

	//MQ_SendResumePrimitive( g_hMQHandle );

	return DlinkMQ_ERROR_CODE_SUCCESS;
}


we_int DlinkmqMsg_SendMsg
(
 we_handle            hMsgHandle,
 E_DlinkmqMsgModuleId     eTgtModID,
 we_int32            iMsgID,
 we_int32             iParam1,
 we_int32             iParam2,
 we_uint32            uiParam1,
 we_uint32            uiParam2,
 void                *pvParam1,
 void                *pvParam2
 )
{
	St_DlinkmqMsgHandle *pstMQMsgHandle = (St_DlinkmqMsgHandle *)hMsgHandle;
	St_MQMsg stMQMsg;


	if(NULL == pstMQMsgHandle)
	{
		mqtt_fmt_print("---DlinkmqMsg_SendMsg  NULL == pstMQMsgHandle");
		return DlinkMQ_ERROR_CODE_FAIL;
	}

	if(eTgtModID < E_MQ_MSG_MODULEID_MGR || eTgtModID >= E_MQ_MSG_MODULEID_COUNT)
	{
		mqtt_fmt_print("---DlinkmqMsg_SendMsg  eTgtModID < E_MQ_MSG_MODULEID_MGR || eTgtModID >= E_MQ_MSG_MODULEID_COUNT");
		return DlinkMQ_ERROR_CODE_FAIL;
	}

	stMQMsg.uiMsgType = (we_uint32)iMsgID;
	stMQMsg.iParam1 = iParam1;
	stMQMsg.iParam2 = iParam2;
	stMQMsg.uiParam1 = uiParam1;
	stMQMsg.uiParam2 = uiParam2;
	stMQMsg.pvParam1 = pvParam1;
	stMQMsg.pvParam2 = pvParam2;

	/* Call the message process directly */
	if(NULL == pstMQMsgHandle->apcbMsgProcFunc[eTgtModID])
	{        
		mqtt_fmt_print("---DlinkmqMsg_SendMsg  NULL == pstMQMsgHandle->apcbMsgProcFunc[eTgtModID]");
		return DlinkMQ_ERROR_CODE_FAIL;
	}
	(we_void)(pstMQMsgHandle->apcbMsgProcFunc[eTgtModID])(
		pstMQMsgHandle->apvPrivData[eTgtModID],
		&stMQMsg 
		);
	return DlinkMQ_ERROR_CODE_SUCCESS;
}


we_void DlinkmqMsg_Destroy(we_handle phDlinkmqMsgHandle)
{
	St_DlinkmqMsgHandle *pstMQMsgHandle = (St_DlinkmqMsgHandle *)phDlinkmqMsgHandle;
	St_MQMsgNode *pstMQMsgNode = NULL;


	if(NULL == pstMQMsgHandle)
	{
		return;
	}
	
	while(pstMQMsgHandle->pstMsgNodeHead)
	{
		pstMQMsgNode = pstMQMsgHandle->pstMsgNodeHead;
		pstMQMsgHandle->pstMsgNodeHead = pstMQMsgNode->pstNext;

		if (pstMQMsgNode->pstMQMsg->pfncbFreePrivateData != NULL)
		{
			pstMQMsgNode->pstMQMsg->pfncbFreePrivateData(pstMQMsgNode->pstMQMsg->pvParam1, pstMQMsgNode->pstMQMsg->pvParam2);
		}
		DLINKMQ_FREE(pstMQMsgNode->pstMQMsg);
		DLINKMQ_FREE(pstMQMsgNode);
	}

	DLINKMQ_FREE(pstMQMsgHandle);
}