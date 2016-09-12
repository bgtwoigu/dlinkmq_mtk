
#include "dlinkmq_mqtt.h"
#include "dlinkmq_error.h"
#include "dlinkmq_msg.h"
#include "dlinkmq_mgr.h"

extern we_handle g_pstDlinkmqMsgHandle;
extern we_handle g_pstDlinkmqMgr;

extern we_void dlinkmq_httpconn_timer(we_uint8 start);
extern stack_timer_status_type dlinkmq_get_httpconn_timer_status();
we_int DlinkmqMqtt_SockNotifyInd(we_handle hDlinkmqMqttHandle, we_void *pvMsg);
we_void DlinkmqMqtt_DestroyNetwork(we_handle hDlinkmqMqttHandle, we_bool isReconnect);
we_int DlinkmqMqtt_ConnectNetWork(we_handle hDlinkmqMqttHandle);
we_void DlinkmqMqtt_ServiceInit(we_handle hDlinkmqMqttHandle);

static void FN_MQTTAsyncConnect_CB(int result,void *data);

static we_int32 DlinkmqMqtt_Process(we_handle hDlinkmqMqttHandle, St_MQMsg *pstMsg)
{
	we_int32 ret = DlinkMQ_ERROR_CODE_FAIL;
	St_DlinkmqMqtt *pstMqtt = (St_DlinkmqMqtt *)hDlinkmqMqttHandle;

	if (pstMqtt == NULL
		|| pstMsg == NULL)
	{
		return ret;
	}

	switch(pstMsg->uiMsgType)
	{

	case E_MQ_MSG_EVENTID_MQTT_INIT:
		ret = DlinkmqMqtt_ConnectNetWork(hDlinkmqMqttHandle);
		break;

	case E_MQ_MSG_EVENTID_SOC_NOTIFY_IND:
		ret = DlinkmqMqtt_SockNotifyInd(hDlinkmqMqttHandle, pstMsg->pvParam1);
		break;

	default:
		break;

	}


	return ret;

}

we_int DlinkmqMqtt_Init(we_handle *phDlinkmqMqttHandle)
{
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;

	St_DlinkmqMqtt *pstMqtt = (St_DlinkmqMqtt *)DLINKMQ_MALLOC(sizeof(St_DlinkmqMqtt));

	if (pstMqtt == NULL) {

		return ret;
	}

	DlinkmqMsg_RegisterProcess(g_pstDlinkmqMsgHandle, E_MQ_MSG_MODULEID_MQTT, DlinkmqMqtt_Process, (we_handle)pstMqtt);

	pstMqtt->mqttStatus = MQTT_STATUS_INIT;

	ret = DlinkMQ_ERROR_CODE_SUCCESS;

	*phDlinkmqMqttHandle = pstMqtt;


	return ret;
}

we_void DlinkmqMqtt_Destroy(we_handle hDlinkmqMqttHandle)
{
	St_DlinkmqMqtt *pstMqtt = (St_DlinkmqMqtt *)hDlinkmqMqttHandle;

	if(pstMqtt != NULL) {

		DlinkmqMqtt_DestroyNetwork(hDlinkmqMqttHandle, FALSE);
		DLINKMQ_FREE(pstMqtt);
	}
}

we_int DlinkmqMqtt_ConnectNetWork(we_handle hDlinkmqMqttHandle)
{
	St_DlinkmqMqtt *pstMqtt = (St_DlinkmqMqtt *)hDlinkmqMqttHandle;
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;
	we_int netRet = DlinkMQ_ERROR_CODE_FAIL;
	St_DlinkmqMqttReg *pstMqttReg = DlinkmqMgr_GetMqttRegInfo(g_pstDlinkmqMgr);;

	if (pstMqtt == NULL
		|| pstMqttReg == NULL) {

		return ret;
	}


	if(pstMqtt->pstNetWork != NULL) //之前连接还未销毁
	{
		mqtt_fmt_print("---DlinkmqMqtt_ConnectNetWork first DlinkmqMqtt_DestroyNetwork");
		DlinkmqMqtt_DestroyNetwork(hDlinkmqMqttHandle, FALSE);
	}

	ret = DlinkmqNetwork_Init((we_handle *)(&pstMqtt->pstNetWork));

	if (pstMqtt->pstNetWork != NULL) 
	{
		NewNetwork(pstMqtt->pstNetWork, NULL);

		//1. 设置timer ,等待connect返回

		dlinkmq_httpconn_timer(1);

		 
		//2. 连接网络
		netRet = ConnectNetwork(pstMqtt->pstNetWork, pstMqttReg->pcMqttServer, pstMqttReg->iMqttPort);

		pstMqtt->mqttStatus = MQTT_STATUS_CONNING;

		mqtt_fmt_print("---DlinkmqMqtt_ConnectNetWork ConnectNetwork :%d", netRet);

		if (netRet == DlinkMQ_ERROR_CODE_SOC_CREAT
			|| netRet == DlinkMQ_ERROR_CODE_SOC_CONN
			|| netRet == DlinkMQ_ERROR_CODE_GET_HOSTNAME) //需要重新连接
		{

			mqtt_fmt_print("---DlinkmqMqtt_ConnectNetWork need connect :%d", netRet);
			DlinkmqMqtt_DestroyNetwork(hDlinkmqMqttHandle, TRUE);

		} else if (netRet == DlinkMQ_ERROR_CODE_SUCCESS
			|| netRet == DlinkMQ_ERROR_CODE_WOULDBLOCK) { //需要设置timer, timer连接超时，需要重新连接

			mqtt_fmt_print("---DlinkmqMqtt_ConnectNetWork waiting for connect :%d", netRet);


		} else { //其他内存错误。

			mqtt_fmt_print("---DlinkmqMqtt_ServiceInit err netRet :%d", netRet);
			DlinkmqMqtt_DestroyNetwork(hDlinkmqMqttHandle, FALSE);
			return ret;
		}

		ret = DlinkMQ_ERROR_CODE_SUCCESS;

	} 



	return ret;

}

we_void DlinkmqMqtt_DestroyNetwork(we_handle hDlinkmqMqttHandle, we_bool isReconnect)
{
	St_DlinkmqMqtt *pstMqtt = (St_DlinkmqMqtt *)hDlinkmqMqttHandle;


	if(pstMqtt == NULL)
	{
		return;
	}

	dlinkmq_httpconn_timer(0);

	DlinkmqNetwork_Destroy(pstMqtt->pstNetWork);

	pstMqtt->pstNetWork = NULL;

	DLINKMQ_FREE(pstMqtt->pcMqttBuf);
	DLINKMQ_FREE(pstMqtt->pcMqttReadBuf);

	
	if (pstMqtt->mqttStatus != MQTT_STATUS_RECONN && isReconnect == TRUE)//重连
	{
		mqtt_fmt_print("---DlinkmqMqtt_DestroyNetwork reconnect");

		pstMqtt->mqttStatus = MQTT_STATUS_RECONN;
		DlinkmqMsg_PostMsg(g_pstDlinkmqMsgHandle, E_MQ_MSG_MODULEID_MQTT, E_MQ_MSG_EVENTID_MQTT_INIT, 0, 0, 0, 0, NULL, NULL);
	} 
	else 
	{
		pstMqtt->mqttStatus = MQTT_STATUS_DESTROY;

	}

	

}

we_int DlinkmqMqtt_SockNotifyInd(we_handle hDlinkmqMqttHandle, we_void *pvMsg)
{
	we_int ret = DlinkMQ_ERROR_CODE_SUCCESS;
	app_soc_notify_ind_struct *soc_notify = (app_soc_notify_ind_struct*) pvMsg;
	St_DlinkmqMqtt *pstMqtt = (St_DlinkmqMqtt *)hDlinkmqMqttHandle;
	we_int iResult  = DlinkMQ_ERROR_CODE_FAIL;


	if(soc_notify == NULL
		|| pstMqtt == NULL)
	{
		return DlinkMQ_ERROR_CODE_FAIL;
	}

	iResult = soc_notify->result;

	mqtt_fmt_print("---n_for_mqtt soc_notify->event_type:%d",soc_notify->event_type);

	switch (soc_notify->event_type) 
	{
	case SOC_READ:
		{
			dlinkmq_httpconn_timer(0);
			dispatchEvents(soc_notify);
		}
		break;
	case SOC_CLOSE:
		{
			mqtt_fmt_print("\r\n---n_for_mqtt -SOC_CLOSE\n");

			if (dlinkmq_get_httpconn_timer_status() != STACK_TIMER_RUNNING) 
			{
				mqtt_fmt_print("\r\n---n_for_mqtt -SOC_CLOSE  reconnect \n");
				DlinkmqMqtt_DestroyNetwork(hDlinkmqMqttHandle,TRUE);
			}		
			
		}
		break;
	case SOC_WRITE:
		{
			dlinkmq_httpconn_timer(0);
			dispatchEvents(soc_notify);
		}
		break;
	case SOC_CONNECT:
		{
			if (iResult == KAL_TRUE) 
			{
				mqtt_fmt_print("\r\n---n_for_mqtt -SOC_CONNECT success \n");
				dlinkmq_httpconn_timer(0);
				pstMqtt->mqttStatus = MQTT_STATUS_CONN;
				DlinkmqMqtt_ServiceInit(hDlinkmqMqttHandle);
			} 
			else 
			{
				mqtt_fmt_print("\r\n---n_for_mqtt -SOC_CONNECT failed reconnect \n");
				DlinkmqMqtt_DestroyNetwork(hDlinkmqMqttHandle,TRUE);
			}
			break;
		}

	}


	return ret;

}



we_void DlinkmqMqtt_ServiceInit(we_handle hDlinkmqMqttHandle)
{
	St_DlinkmqMqtt *pstMqtt = (St_DlinkmqMqtt *)hDlinkmqMqttHandle;
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	Client *pstClient = DlinkmqMgr_GetClient(g_pstDlinkmqMgr);
	St_DlinkmqMqttReg *psMqttReg = DlinkmqMgr_GetMqttRegInfo(g_pstDlinkmqMgr);

	if (pstMqtt == NULL
		|| pstClient == NULL
		|| (pstMqtt && pstMqtt->pstNetWork == NULL)
		|| psMqttReg == NULL)
	{
		mqtt_fmt_print("---DlinkmqMqtt_ServiceInit--> param error");
		return;
	}

	pstMqtt->pcMqttBuf = DLINKMQ_MALLOC(DLINKMQ_MQTT_BUF_SIZE);
	pstMqtt->pcMqttReadBuf = DLINKMQ_MALLOC(DLINKMQ_MQTT_READBUF_SIZE);

	if (pstMqtt->pcMqttBuf == NULL
		|| pstMqtt->pcMqttReadBuf == NULL)
	{
		return;
	}


	MQTTClient(pstClient, pstMqtt->pstNetWork, DLINKMQ_MQTT_COMMAND_TIMEOUT, pstMqtt->pcMqttBuf, DLINKMQ_MQTT_BUF_SIZE, pstMqtt->pcMqttReadBuf, DLINKMQ_MQTT_READBUF_SIZE);
    
    	
    data.willFlag = 0;
    data.MQTTVersion = 3;
    data.clientID.cstring = psMqttReg->pcMqttCliendId;
    data.username.cstring = psMqttReg->pcMqttUsername;
    data.password.cstring = psMqttReg->pcMqttPassword;
    data.keepAliveInterval = DLINKMQ_MQTT_keepAliveInterval;
    data.cleansession = 1;
    
    //1.要设置超时timer

	//2.连接
   	MQTTAsyncConnect(pstClient, &data, FN_MQTTAsyncConnect_CB);
	mqtt_fmt_print("---DlinkmqMqtt_ServiceInit-->socketId:%d\n",pstClient->ipstack->my_socket);
}

extern void cbconn(int result,void *data);
static void FN_MQTTAsyncConnect_CB(int result,void *data)
{
	cbconn(result, data);

}