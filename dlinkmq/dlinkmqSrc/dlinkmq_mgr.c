#include "dlinkmq_mgr.h"
#include "dlinkmq_msg.h"
#include "MQTTMd5.h"


extern we_handle g_pstDlinkmqMsgHandle;

we_int DlinkmqMgr_GetHttpJson(we_char* pid, we_char *did, we_char *productSecret, we_char **ppJsonData);

static we_int32 DlinkmqMgr_Process(we_handle hDlinkmqMgrHandle, St_MQMsg *pstMsg)
{
	we_int32 ret = DlinkMQ_ERROR_CODE_FAIL;
	St_DlinkmqMgr *pstMgr = (St_DlinkmqMgr *)hDlinkmqMgrHandle;

	if (pstMgr == NULL
		|| pstMsg == NULL)
	{
		return ret;
	}

	switch(pstMsg->uiMsgType)
	{

	case E_MQ_MSG_EVENTID_MGR_HTTP_RECONNECT:
		{
			
		}
		break;

	default:
		break;

	}


	return ret;

}

we_int DlinkmqMgr_Init(we_handle *phDlinkmqMgrHandle)
{
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;

	St_DlinkmqMgr *pstMgr = (St_DlinkmqMgr *)DLINKMQ_MALLOC(sizeof(St_DlinkmqMgr));

	if (pstMgr == NULL) {

		return ret;
	}

	DlinkmqMsg_RegisterProcess(g_pstDlinkmqMsgHandle, E_MQ_MSG_MODULEID_MGR, DlinkmqMgr_Process, (we_handle)pstMgr);

	ret = DlinkmqClient_Init((we_handle *)(&pstMgr->pstMqClient));
	ret = DlinkmqHttp_Init((we_handle *)(&pstMgr->pstDlinkmqHttp));
	ret = DlinkmqMqtt_Init((we_handle *)(&pstMgr->pstDlinkmqMqtt));
	ret = DlinkmqUpload_Init((we_handle *)(&pstMgr->pstDlinkmqUpload));

	*phDlinkmqMgrHandle = pstMgr;


	return ret;
}

we_void DlinkmqMgr_SetData(we_handle hDlinkmqMgrHandle, dlinkmq_device_info *pstDeviceInfo, dlinkmq_on_receive *pstRecvFunCB)
{
	St_DlinkmqMgr *pstMgr = (St_DlinkmqMgr *)hDlinkmqMgrHandle;
	we_int len = 0;
	we_char *pcDeviceStr = "device/";
	we_char *pcGetStr = "/get";
	we_char *pcSetStr = "/set";

	if (pstMgr == NULL
		|| pstDeviceInfo == NULL
		|| pstRecvFunCB == NULL)
	{
		return;
	}



	memcpy(&pstMgr->stDeviceInfo, pstDeviceInfo, sizeof(dlinkmq_device_info));
	memcpy(&pstMgr->stRecvFunCB, pstRecvFunCB, sizeof(dlinkmq_on_receive));

	len = strlen(pcDeviceStr) + strlen(pcSetStr) + 2 + strlen(pstDeviceInfo->product_id) + strlen(pstDeviceInfo->device_id);

	pstMgr->stMqttSubPub.pcPubTopic = (we_char *)DLINKMQ_MALLOC(len);

	if (pstMgr->stMqttSubPub.pcPubTopic == NULL) 
	{
		return;
	}

	sprintf(pstMgr->stMqttSubPub.pcPubTopic, "device/%s/%s/set", pstDeviceInfo->product_id,pstDeviceInfo->device_id);


	len = strlen(pcDeviceStr) + strlen(pcGetStr) + 2 + strlen(pstDeviceInfo->product_id) + strlen(pstDeviceInfo->device_id);

	pstMgr->stMqttSubPub.pcSubTopic= (we_char *)DLINKMQ_MALLOC(len);

	if (pstMgr->stMqttSubPub.pcSubTopic == NULL) 
	{
		goto ErrorExit;
	}

	sprintf(pstMgr->stMqttSubPub.pcSubTopic, "device/%s/%s/get", pstDeviceInfo->product_id,pstDeviceInfo->device_id);


/*	pstMgr->pstMqClient->mqttstatus = MQTT_STATUS_INIT;*/


	return;

	ErrorExit:
		DLINKMQ_FREE(pstMgr->stMqttSubPub.pcPubTopic);
		DLINKMQ_FREE(pstMgr->stMqttSubPub.pcSubTopic);
	
}

E_DlinkmqMsgModuleId DlinkmqMgr_GetEventIdBySockId(we_handle hDlinkmqMgrHandle, we_int sockId)
{
	E_DlinkmqMsgModuleId moduleId = E_MQ_MSG_MODULEID_BASE;

	St_DlinkmqMgr *pstMgr = (St_DlinkmqMgr *)hDlinkmqMgrHandle;

	if (pstMgr == NULL
		|| sockId < 0) 
	{
		return moduleId;
	}

	if (pstMgr->pstDlinkmqHttp 
		&& pstMgr->pstDlinkmqHttp->pstNetWork
		&& pstMgr->pstDlinkmqHttp->pstNetWork->my_socket == sockId)
	{
		moduleId = E_MQ_MSG_MODULEID_HTTP;
	} 
	else if (pstMgr->pstDlinkmqMqtt 
		&& pstMgr->pstDlinkmqMqtt->pstNetWork
		&& pstMgr->pstDlinkmqMqtt->pstNetWork->my_socket == sockId)
	{
		moduleId = E_MQ_MSG_MODULEID_MQTT;
	}
	else if (pstMgr->pstDlinkmqUpload 
		&& pstMgr->pstDlinkmqUpload->pstNetWork
		&& pstMgr->pstDlinkmqUpload->pstNetWork->my_socket == sockId)
	{
		moduleId = E_MQ_MSG_MODULEID_UPLOAD;
	}

	return moduleId;
}


we_void DlinkmqMgr_Destroy(we_handle hDlinkmqMgrHandle)
{
	St_DlinkmqMgr *pstMgr = (St_DlinkmqMgr *)hDlinkmqMgrHandle;
	

	if(pstMgr != NULL) {


	
		DLINKMQ_FREE(pstMgr->stMqttSubPub.pcPubTopic);
		DLINKMQ_FREE(pstMgr->stMqttSubPub.pcSubTopic);

		DLINKMQ_FREE(pstMgr->stMqttReg.pcMqttCliendId);
		DLINKMQ_FREE(pstMgr->stMqttReg.pcMqttUsername);
		DLINKMQ_FREE(pstMgr->stMqttReg.pcMqttPassword);
		DLINKMQ_FREE(pstMgr->stMqttReg.pcMqttServer);

		
		DlinkmqHttp_Destroy(pstMgr->pstDlinkmqHttp);
		DlinkmqMqtt_Destroy(pstMgr->pstDlinkmqMqtt);
		DlinkmqClient_Destroy(pstMgr->pstMqClient);

		DLINKMQ_FREE(pstMgr);
	}

	
}
	

we_void DlinkmqMgr_SetMqttRegInfo(we_handle hDlinkmqMgrHandle, St_DlinkmqMqttReg *pstMqttReg)
{
	St_DlinkmqMgr *pstMgr = (St_DlinkmqMgr *)hDlinkmqMgrHandle;
	we_int len = 0;

	if (pstMgr == NULL
		|| pstMqttReg == NULL)
	{
		return;
	}

	if (pstMqttReg->pcMqttCliendId)
	{
		len = strlen(pstMqttReg->pcMqttCliendId);
		pstMgr->stMqttReg.pcMqttCliendId = DLINKMQ_MALLOC(len + 1);

		if (pstMgr->stMqttReg.pcMqttCliendId)
		{
			memcpy(pstMgr->stMqttReg.pcMqttCliendId, pstMqttReg->pcMqttCliendId, len);
		}
	}
	
	if (pstMqttReg->pcMqttUsername)
	{
		len = strlen(pstMqttReg->pcMqttUsername);
		pstMgr->stMqttReg.pcMqttUsername = DLINKMQ_MALLOC(len + 1);

		if (pstMgr->stMqttReg.pcMqttUsername)
		{
			memcpy(pstMgr->stMqttReg.pcMqttUsername, pstMqttReg->pcMqttCliendId, len);
		}
	}

	if (pstMqttReg->pcMqttPassword)
	{
		len = strlen(pstMqttReg->pcMqttPassword);
		pstMgr->stMqttReg.pcMqttPassword = DLINKMQ_MALLOC(len + 1);

		if (pstMgr->stMqttReg.pcMqttPassword)
		{
			memcpy(pstMgr->stMqttReg.pcMqttPassword, pstMqttReg->pcMqttPassword, len);
		}
	}

	if (pstMqttReg->pcMqttServer)
	{
		len = strlen(pstMqttReg->pcMqttServer);
		pstMgr->stMqttReg.pcMqttServer = DLINKMQ_MALLOC(len + 1);

		if (pstMgr->stMqttReg.pcMqttServer)
		{
			memcpy(pstMgr->stMqttReg.pcMqttServer, pstMqttReg->pcMqttServer, len);
		}
	}

	pstMgr->stMqttReg.iMqttPort = pstMqttReg->iMqttPort;
}




dlinkmq_device_info * DlinkmqMgr_GetDeviceInfo(we_handle hDlinkmqMgrHandle)
{

	St_DlinkmqMgr *pstMgr = (St_DlinkmqMgr *)hDlinkmqMgrHandle;

	if (pstMgr == NULL)
	{
		return NULL;
	}

	return &pstMgr->stDeviceInfo;
}

St_DlinkmqMqttReg * DlinkmqMgr_GetMqttRegInfo(we_handle hDlinkmqMgrHandle)
{

	St_DlinkmqMgr *pstMgr = (St_DlinkmqMgr *)hDlinkmqMgrHandle;

	if (pstMgr == NULL)
	{
		return NULL;
	}

	return &pstMgr->stMqttReg;
}

Client  * DlinkmqMgr_GetClient(we_handle hDlinkmqMgrHandle)
{
	St_DlinkmqMgr *pstMgr = (St_DlinkmqMgr *)hDlinkmqMgrHandle;

	if (pstMgr == NULL)
	{
		return NULL;
	}

	return pstMgr->pstMqClient;
}

dlinkmq_on_receive * DlinkmqMgr_GetRecvInfo(we_handle hDlinkmqMgrHandle)
{
	St_DlinkmqMgr *pstMgr = (St_DlinkmqMgr *)hDlinkmqMgrHandle;

	if (pstMgr == NULL)
	{
		return NULL;
	}

	return &pstMgr->stRecvFunCB;
}

St_DlinkmqMqttSubPub * DlinkmqMgr_GetMqttSubPubInfo(we_handle hDlinkmqMgrHandle)
{
	St_DlinkmqMgr *pstMgr = (St_DlinkmqMgr *)hDlinkmqMgrHandle;

	if (pstMgr == NULL)
	{
		return NULL;
	}

	return &pstMgr->stMqttSubPub;
}

St_DlinkmqMqtt  * DlinkmqMgr_GetMqtt(we_handle hDlinkmqMgrHandle)
{

	St_DlinkmqMgr *pstMgr = (St_DlinkmqMgr *)hDlinkmqMgrHandle;

	if (pstMgr == NULL)
	{
		return NULL;
	}

	return pstMgr->pstDlinkmqMqtt;
}

St_DlinkmqHttp  * DlinkmqMgr_GetHttp(we_handle hDlinkmqMgrHandle)
{
	St_DlinkmqMgr *pstMgr = (St_DlinkmqMgr *)hDlinkmqMgrHandle;

	if (pstMgr == NULL)
	{
		return NULL;
	}

	return pstMgr->pstDlinkmqHttp;
}

St_DlinkmqUpload* DlinkmqMgr_GetUpload(we_handle hDlinkmqMgrHandle)
{
	St_DlinkmqMgr *pstMgr = (St_DlinkmqMgr *)hDlinkmqMgrHandle;

	if (pstMgr == NULL)
	{
		return NULL;
	}

	return pstMgr->pstDlinkmqUpload;
}

