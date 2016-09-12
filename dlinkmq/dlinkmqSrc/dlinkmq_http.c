#include "dlinkmq_http.h"
#include "dlinkmq_error.h"
#include "dlinkmq_utils.h"
#include "dlinkmq_mgr.h"
#include "MQTTMd5.h"
#include "MQTTcJSON.h"

extern we_handle g_pstDlinkmqMsgHandle;
extern we_handle g_pstDlinkmqMgr;

we_int DlinkmqHttp_ConnectNetWork(we_handle hDlinkmqHttpHandle);
we_int DlinkmqHttp_SockNotifyInd(we_handle hDlinkmqHttpHandle, we_void *pvMsg);
int MQTTClient_setup_with_pid_and_did(char* pid, char *did, char *productSecret);
we_int DlinkmqHttp_GetHttpJson(we_char* pid, we_char *did, we_char *productSecret, we_char **ppJsonData);
we_void DlinkmqHttp_PostJsonToSeverSendBuff(we_handle hDlinkmqHttpHandle);
we_void DlinkmqHttp_DestroyNetwork(we_handle hDlinkmqHttpHandle, we_bool isReconnect);
extern we_void dlinkmq_httpconn_timer(we_uint8 start);
we_void DlinkmqHttp_RecvBuffer(we_handle hDlinkmqHttpHandle);
static int parseJsonToSaveInfo(const char *json_data);

static we_int32 DlinkmqHttp_Process(we_handle hDlinkmqHttpHandle, St_MQMsg *pstMsg)
{
	we_int32 ret = DlinkMQ_ERROR_CODE_FAIL;
	St_DlinkmqHttp *pstHttp = (St_DlinkmqHttp *)hDlinkmqHttpHandle;

	if (pstHttp == NULL
		|| pstMsg == NULL)
	{
		return ret;
	}

	switch(pstMsg->uiMsgType)
	{

	case E_MQ_MSG_EVENTID_NEW_HTTP:
		ret = DlinkmqHttp_ConnectNetWork(hDlinkmqHttpHandle);
		break;

	case E_MQ_MSG_EVENTID_SOC_NOTIFY_IND:
		ret = DlinkmqHttp_SockNotifyInd(hDlinkmqHttpHandle, pstMsg->pvParam1);
		break;

	default:
		break;

	}


	return ret;

}

we_int DlinkmqHttp_Init(we_handle *phDlinkmqHttpHandle)
{
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;

	St_DlinkmqHttp *pstHttp = (St_DlinkmqHttp *)DLINKMQ_MALLOC(sizeof(St_DlinkmqHttp));

	if (pstHttp == NULL) {

		return ret;
	}

	pstHttp->isInitFin = FALSE;

	DlinkmqMsg_RegisterProcess(g_pstDlinkmqMsgHandle, E_MQ_MSG_MODULEID_HTTP, DlinkmqHttp_Process, (we_handle)pstHttp);


	ret = DlinkMQ_ERROR_CODE_SUCCESS;
	*phDlinkmqHttpHandle = pstHttp;


	return ret;
}

we_int DlinkmqHttp_ConnectNetWork(we_handle hDlinkmqHttpHandle)
{
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;
	int netRet = DlinkMQ_ERROR_CODE_FAIL;

	St_DlinkmqHttp *pstHttp = (St_DlinkmqHttp *)hDlinkmqHttpHandle;

	mqtt_fmt_print("---DlinkmqHttp_ConnectNetWork start");

	if (pstHttp == NULL)
	{
		return ret;
	}

	if(pstHttp->pstNetWork != NULL) //之前连接还未销毁
	{
		mqtt_fmt_print("---DlinkmqHttp_ConnectNetWork before DlinkmqHttp_DestroyNetwork");
		DlinkmqHttp_DestroyNetwork(hDlinkmqHttpHandle, FALSE);
	}


	mqtt_fmt_print("---DlinkmqHttp_ConnectNetWork before DlinkmqNetwork_Init");

	ret = DlinkmqNetwork_Init((we_handle *)(&pstHttp->pstNetWork));

	mqtt_fmt_print("---DlinkmqHttp_ConnectNetWork after DlinkmqNetwork_Init ret=%d", ret);

	if (pstHttp->pstNetWork != NULL) 
	{
		NewNetwork(pstHttp->pstNetWork, NULL);

		//1. 设置timer ,等待connect返回

		dlinkmq_httpconn_timer(1);

		//2. 连接网络

		mqtt_fmt_print("---DlinkmqHttp_ConnectNetWork before ConnectNetwork");
		netRet = ConnectNetwork(pstHttp->pstNetWork, dotlink_host, dotlink_port);
		mqtt_fmt_print("---DlinkmqHttp_ConnectNetWork after ConnectNetwork netRet=%d", netRet);

		mqtt_fmt_print("---DlinkmqHttp_Init ConnectNetwork :%d", netRet);

		if (netRet == DlinkMQ_ERROR_CODE_SOC_CREAT
			|| netRet == DlinkMQ_ERROR_CODE_SOC_CONN
			|| netRet == DlinkMQ_ERROR_CODE_GET_HOSTNAME) //需要重新连接
		{
			DlinkmqHttp_DestroyNetwork(hDlinkmqHttpHandle, TRUE);

		} else if (netRet == DlinkMQ_ERROR_CODE_SUCCESS
			|| netRet == DlinkMQ_ERROR_CODE_WOULDBLOCK) { //需要设置timer, timer连接超时，需要重新连接


		} else { //其他内存错误。

			DlinkmqHttp_DestroyNetwork(hDlinkmqHttpHandle, FALSE);
			return ret;
		}

		ret = DlinkMQ_ERROR_CODE_SUCCESS;
		
	} 


	mqtt_fmt_print("---DlinkmqHttp_ConnectNetWork end");
	

	return ret;

}

we_void DlinkmqHttp_DestroyNetwork(we_handle hDlinkmqHttpHandle, we_bool isReconnect)
{
	St_DlinkmqHttp *pstHttp = (St_DlinkmqHttp *)hDlinkmqHttpHandle;


	if(pstHttp == NULL)
	{
		return;
	}

	dlinkmq_httpconn_timer(0);

	DlinkmqNetwork_Destroy(pstHttp->pstNetWork);
	DLINKMQ_FREE(pstHttp->pSendBuff);
	pstHttp->pstNetWork = NULL;
	pstHttp->pSendBuff = NULL;
	pstHttp->iBuffSize = 0;
	pstHttp->iSentSize = 0;


	if (pstHttp->isInitFin == FALSE && isReconnect == TRUE)//重连
	{
		mqtt_fmt_print("----DlinkmqHttp_DestroyNetwork reconnect");
		DlinkmqMsg_PostMsg(g_pstDlinkmqMsgHandle, E_MQ_MSG_MODULEID_HTTP, E_MQ_MSG_EVENTID_NEW_HTTP, 0, 0, 0, 0, NULL, NULL);
	}

	
}

we_int DlinkmqHttp_SockNotifyInd(we_handle hDlinkmqHttpHandle, we_void *pvMsg)
{
	we_int ret = DlinkMQ_ERROR_CODE_SUCCESS;
	app_soc_notify_ind_struct *soc_notify = (app_soc_notify_ind_struct*) pvMsg;
	St_DlinkmqHttp *pstHttp = (St_DlinkmqHttp *)hDlinkmqHttpHandle;
	we_int iResult  = DlinkMQ_ERROR_CODE_FAIL;


	if(soc_notify == NULL
		|| pstHttp == NULL)
	{
		return DlinkMQ_ERROR_CODE_FAIL;
	}

	iResult = soc_notify->result;

	mqtt_fmt_print("---n_for_http soc_notify->event_type:%d",soc_notify->event_type);

	switch (soc_notify->event_type) 
	{
		case SOC_READ:
		{
			dlinkmq_httpconn_timer(0);
			DlinkmqHttp_RecvBuffer(hDlinkmqHttpHandle);
		}
			break;
		case SOC_CLOSE:
		{
 			mqtt_fmt_print("\r\n---n_for_http -SOC_CLOSE\n");
			DlinkmqHttp_DestroyNetwork(hDlinkmqHttpHandle,TRUE);
		}
			break;
		case SOC_WRITE:
		{
			dlinkmq_httpconn_timer(0);
			DlinkmqHttp_PostJsonToSeverSendBuff(hDlinkmqHttpHandle);
		}
			break;
		case SOC_CONNECT:
		{

			if (iResult == KAL_TRUE) 
			{
				
				dlinkmq_device_info *pstDinfo = DlinkmqMgr_GetDeviceInfo(g_pstDlinkmqMgr);

				dlinkmq_httpconn_timer(0);

				mqtt_fmt_print("\r\n---n_for_http -SOC_CONNECT success \n");
				if (pstDinfo != NULL)
				{
					we_char *pJsonData = NULL;
					DlinkmqHttp_GetHttpJson(pstDinfo->product_id, pstDinfo->device_id, pstDinfo->product_secret, &pJsonData);

					if (pJsonData != NULL)
					{
						ret = DlinkmqHttp_PostJsonToSever(hDlinkmqHttpHandle, pJsonData);

						DLINKMQ_FREE(pJsonData);
					}
				}
			} 
			else 
			{
				mqtt_fmt_print("\r\n---n_for_http -SOC_CONNECT failed reconnect \n");
				DlinkmqHttp_DestroyNetwork(hDlinkmqHttpHandle,TRUE);
			}

			
			
			break;
		}

	}


	return ret;

}

we_void DlinkmqHttp_Destroy(we_handle hDlinkmqHttpHandle)
{
	St_DlinkmqHttp *pstHttp = (St_DlinkmqHttp *)hDlinkmqHttpHandle;

	if(pstHttp != NULL) {

		DlinkmqHttp_DestroyNetwork(hDlinkmqHttpHandle, FALSE);
		DLINKMQ_FREE(pstHttp);
	}
}


static int parseJsonToSaveInfo(const char *json_data) 
{
    int ret = DlinkMQ_ERROR_CODE_FAIL;
	St_DlinkmqMqttReg stMqttReg = {0};

    cJSON *root = NULL;
    
    root = cJSON_Parse(json_data);
    if (root) {
        int ret_size = cJSON_GetArraySize(root);
        if (ret_size >= 4) {
			char temp[30]={0};

			stMqttReg.pcMqttCliendId = cJSON_GetObjectItem(root,"client")->valuestring;
			stMqttReg.pcMqttUsername = cJSON_GetObjectItem(root,"username")->valuestring;
			stMqttReg.pcMqttPassword = cJSON_GetObjectItem(root,"password")->valuestring;

			
			dlinkmq_data_A_To_B(cJSON_GetObjectItem(root,"host")->valuestring,1,':',':',temp);
			stMqttReg.iMqttPort = atoi(temp);

			memset(temp, 0, sizeof(temp));
			dlinkmq_data_A_To_B(cJSON_GetObjectItem(root,"host")->valuestring,0,':',':',temp);

			stMqttReg.pcMqttServer = temp;


			DlinkmqMgr_SetMqttRegInfo(g_pstDlinkmqMgr, &stMqttReg);

			ret = DlinkMQ_ERROR_CODE_SUCCESS;


			
/*						dlinkmq_gethostbyname(0,temp,0);*/

        } 
        
        cJSON_Delete(root);
    }

    return ret;
}


we_void DlinkmqHttp_RecvBuffer(we_handle hDlinkmqHttpHandle)
{
	St_DlinkmqHttp *pstHttp = (St_DlinkmqHttp *)hDlinkmqHttpHandle;
	we_char *pTempBuffer = NULL;
	we_int iRecvNum = 0;
	we_char *pJsonStr = NULL;
	we_char *pLineStr = "\r\n\r\n";
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;

	if (pstHttp == NULL
		|| (pstHttp && pstHttp->pstNetWork == NULL))
	{
		return;
	}

	pTempBuffer = DLINKMQ_MALLOC(WE_SOCKET_MAX_TCP_RECV_BUFFER_SIZE);
	iRecvNum = mtk_read(pstHttp->pstNetWork, pTempBuffer, WE_SOCKET_MAX_TCP_RECV_BUFFER_SIZE, 0);

	mqtt_fmt_print("-----DlinkmqHttp_RecvBuffer:iRecvNum:%d", iRecvNum);

	if (iRecvNum > 0)
	{
		pJsonStr = strstr(pTempBuffer, pLineStr);

		if (pJsonStr)
		{
			pJsonStr += strlen(pLineStr);
			mqtt_fmt_print("-----DlinkmqHttp_RecvBuffer:pJsonStr:%s", pJsonStr);

			ret = parseJsonToSaveInfo(pJsonStr);

			if (ret == DlinkMQ_ERROR_CODE_SUCCESS)
			{
				pstHttp->isInitFin = TRUE;
				mqtt_fmt_print("---DlinkmqHttp_RecvBuffer: DlinkmqMsg_PostMsg");
				DlinkmqMsg_PostMsg(g_pstDlinkmqMsgHandle, E_MQ_MSG_MODULEID_MQTT, E_MQ_MSG_EVENTID_MQTT_INIT, 0, 0, 0, 0, NULL, NULL);

				DlinkmqHttp_DestroyNetwork(hDlinkmqHttpHandle, FALSE);
			} else {
				DlinkmqHttp_DestroyNetwork(hDlinkmqHttpHandle, TRUE);
			}
		}

	}
	else if(SOC_WOULDBLOCK == iRecvNum)
	{
		dlinkmq_httpconn_timer(1);
	}
	else if(iRecvNum == 0 )
	{
		DlinkmqHttp_DestroyNetwork(hDlinkmqHttpHandle, TRUE);
	}
	else
	{    
		DlinkmqHttp_DestroyNetwork(hDlinkmqHttpHandle, TRUE);
	}    
	DLINKMQ_FREE(pTempBuffer);
}

we_void DlinkmqHttp_PostJsonToSeverSendBuff(we_handle hDlinkmqHttpHandle)
{
	St_DlinkmqHttp *pstHttp = (St_DlinkmqHttp *)hDlinkmqHttpHandle;

	we_int iSendNum = 0;

	if (pstHttp == NULL
		|| (pstHttp && pstHttp->pSendBuff == NULL))
	{
		return;
	}

	mqtt_fmt_print("\n---mqtt post json -buffer:%s, len=%d",pstHttp->pSendBuff, pstHttp->iBuffSize);

	while(pstHttp->iBuffSize > 0)
	{
		iSendNum = mtk_write(pstHttp->pstNetWork, pstHttp->pSendBuff + pstHttp->iSentSize, pstHttp->iBuffSize, 0); 

		mqtt_fmt_print("--- mqtt DlinkmqHttp_PostJsonToSeverSendBuff iSendNum = %d",iSendNum);

		if (iSendNum > 0)
		{
			pstHttp->iSentSize += iSendNum;
			pstHttp->iBuffSize -= iSendNum;

		} else if (SOC_WOULDBLOCK == iSendNum) {//设置超时

			dlinkmq_httpconn_timer(1);
			return;
		} else {//重连
			DlinkmqHttp_DestroyNetwork(hDlinkmqHttpHandle, TRUE);

			return;
		}
	}  
	pstHttp->iSentSize = 0;
	pstHttp->iBuffSize = 0;
	DLINKMQ_FREE(pstHttp->pSendBuff);
	pstHttp->pSendBuff = NULL;

	return;
}


we_int DlinkmqHttp_PostJsonToSever(we_handle hDlinkmqHttpHandle, we_char *json_data)
{
	
    int ret = DlinkMQ_ERROR_CODE_FAIL;
	int len=0;
	int json_len=0;
	int portLen = 0;
	int jsondataLen = 0;
	char *hostname = dotlink_host;
	char *path = dotlink_path;
	char portStr[30] = {0};

	char post_data[]=
		"POST %s HTTP/1.1\r\n"
		"Host: %s:%d\r\n"
		"Accept: application/json\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: %lu\n\n"
		"%s";

	St_DlinkmqHttp *pstHttp = (St_DlinkmqHttp *)hDlinkmqHttpHandle;

	if (pstHttp == NULL
		|| json_data == NULL)
	{
		return ret;
	}

	if (pstHttp->pSendBuff != NULL)
	{
		DLINKMQ_FREE(pstHttp->pSendBuff);
	}

	sprintf(portStr, "%d", dotlink_port);

	portLen = strlen(portStr);
	json_len=strlen(json_data);


	memset(portStr, 0, 30);
	sprintf(portStr, "%d", json_len);
	jsondataLen = strlen(portStr);

	len= json_len + strlen(hostname) + portLen +strlen(path) + strlen(post_data) + jsondataLen;
	pstHttp->pSendBuff = DLINKMQ_MALLOC(len);

	if (pstHttp->pSendBuff == NULL)
	{
		return ret;
	}

	pstHttp->iBuffSize = sprintf(pstHttp->pSendBuff, post_data, path, hostname, dotlink_port, json_len, json_data);
	pstHttp->iSentSize = 0;

	DlinkmqHttp_PostJsonToSeverSendBuff(hDlinkmqHttpHandle);

    return ret;
}

we_int DlinkmqHttp_GetHttpJson(we_char* pid, we_char *did, we_char *productSecret, we_char **ppJsonData)
{
	char *pjson_data = NULL;
	char json_keys[]="{\"did\": \"%s\", \"pid\": \"%s\", \"sign\": \"%s\"}";
	char formt_keys[] = "%sdid%spid%s%s";
	char sign[34]={0};
	MD5_CTX md5;
	int len = 0;
	int len1 = 0;
	int ret = DlinkMQ_ERROR_CODE_FAIL;
	we_int i = 0;
	unsigned char *encrypt;
	unsigned char decrypt[16]={0};

	if (pid == NULL || did==NULL || productSecret==NULL){
		return ret;
	}
	len = strlen(did) + strlen(productSecret)*2 + strlen(pid) + strlen(formt_keys);


	encrypt = DLINKMQ_MALLOC(len);

	if (encrypt == NULL)
	{
		return ret;
	}

	MD5Init(&md5);

	len = sprintf(encrypt,formt_keys,productSecret, did, pid ,productSecret);
	MD5Update(&md5,encrypt,len);
	MD5Final(&md5,decrypt);

	for(i = 0; i < 16; i++)
	{
		char tempStr[5] = {0};

		sprintf(tempStr, "%02x", decrypt[i]);
		strcat(sign, tempStr);
	}

	len1 = strlen(json_keys);
	len = strlen(did) + strlen(pid) +strlen(sign) + len1;

	pjson_data = DLINKMQ_MALLOC(len);

	if (pjson_data == NULL)
	{
		DLINKMQ_FREE(encrypt);
		return ret;
	}
	sprintf(pjson_data, json_keys, did, pid, sign);

	mqtt_fmt_print("\n---mqtt json -DATA:%s, len=%d",pjson_data, len);

	ret = DlinkMQ_ERROR_CODE_SUCCESS;

	*ppJsonData = pjson_data;

	DLINKMQ_FREE(encrypt);
	return ret;
}

