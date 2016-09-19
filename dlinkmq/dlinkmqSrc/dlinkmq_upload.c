#include "dlinkmq_upload.h"
#include "dlinkmq_error.h"
#include "dlinkmq_utils.h"
#include "dlinkmq_mgr.h"
#include "MQTTMd5.h"
#include "MQTTcJSON.h"

extern we_handle g_pstDlinkmqMsgHandle;
extern we_handle g_pstDlinkmqMgr;

we_int DlinkmqUpload_ConnectNetWork(we_handle hDlinkmqUploadHandle);
we_int DlinkmqUpload_SockNotifyInd(we_handle hDlinkmqUploadHandle, we_void *pvMsg);
we_void DlinkmqUpload_DestroyNetwork(we_handle hDlinkmqUploadHandle, we_bool isReconnect);
extern we_void mqtt_cb_exec_with_data(MQTTAsyncCallbackFunc cb,int result,void *data);
extern we_void dlinkmq_upload_conn_timer(we_uint8 start);
we_void DlinkmqUpload_RecvBuffer(we_handle hDlinkmqUploadHandle);
we_void DlinkmqUpload_PostBuffToSever(we_handle hDlinkmqUploadHandle);
we_int DlinkmqUpload_GetPostHeaders(we_handle hDlinkmqUploadHandle);
we_int DlinkmqUpload_DeleteUploadParams(we_handle hDlinkmqUploadHandle);
we_void DlinkmqUpload_UploadFile(we_handle hDlinkmqUploadHandle);


static we_int32 DlinkmqUpload_Process(we_handle hDlinkmqUploadHandle, St_MQMsg *pstMsg)
{
	we_int32 ret = DlinkMQ_ERROR_CODE_FAIL;
	St_DlinkmqUpload *pstUpload = (St_DlinkmqUpload *)hDlinkmqUploadHandle;

	if (pstUpload == NULL
		|| pstMsg == NULL)
	{
		return ret;
	}

	switch(pstMsg->uiMsgType)
	{

	case E_MQ_MSG_EVENTID_NEW_UPLOAD:
		if(pstUpload != NULL && pstUpload->status==DlinkmqUpload_Status_CONNECTING)
		{
			return  DlinkMQ_ERROR_CODE_SUCCESS;
		}
		if(pstUpload !=NULL && pstUpload->pstNetWork != NULL && pstUpload->status==DlinkmqUpload_Status_CONNECT)
		{
			return  DlinkMQ_ERROR_CODE_SUCCESS;
		}
		ret = DlinkmqUpload_ConnectNetWork(hDlinkmqUploadHandle);
		break;

	case E_MQ_MSG_EVENTID_SOC_NOTIFY_IND:
		ret = DlinkmqUpload_SockNotifyInd(hDlinkmqUploadHandle, pstMsg->pvParam1);
		break;

	default:
		break;

	}


	return ret;

}

we_int DlinkmqUpload_Init(we_handle *phDlinkmqUploadHandle)
{
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;

	St_DlinkmqUpload *pstUpload = (St_DlinkmqUpload *)DLINKMQ_MALLOC(sizeof(St_DlinkmqUpload));

	if (pstUpload == NULL) {

		return ret;
	}

	pstUpload->status = DlinkmqUpload_Status_INIT;

	DlinkmqMsg_RegisterProcess(g_pstDlinkmqMsgHandle, E_MQ_MSG_MODULEID_UPLOAD, DlinkmqUpload_Process, (we_handle)pstUpload);


	ret = DlinkMQ_ERROR_CODE_SUCCESS;
	*phDlinkmqUploadHandle = pstUpload;


	return ret;
}

we_int DlinkmqUpload_ConnectNetWork(we_handle hDlinkmqUploadHandle)
{
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;
	int netRet = DlinkMQ_ERROR_CODE_FAIL;

	St_DlinkmqUpload *pstUpload = (St_DlinkmqUpload *)hDlinkmqUploadHandle;

	mqtt_fmt_print("---DlinkmqUpload_ConnectNetWork start");

	if (pstUpload == NULL)
	{
		return ret;
	}

	if(pstUpload->pstNetWork != NULL) //之前连接还未销毁
	{
		mqtt_fmt_print("---DlinkmqUpload_ConnectNetWork before DlinkmqUpload_DestroyNetwork");
		DlinkmqUpload_DestroyNetwork(hDlinkmqUploadHandle, FALSE);
	}

	pstUpload->status = DlinkmqUpload_Status_CONNECTING;
	mqtt_fmt_print("---DlinkmqUpload_ConnectNetWork before DlinkmqNetwork_Init");

	ret = DlinkmqNetwork_Init((we_handle *)(&pstUpload->pstNetWork));

	mqtt_fmt_print("---DlinkmqUpload_ConnectNetWork after DlinkmqNetwork_Init ret=%d", ret);

	if (pstUpload->pstNetWork != NULL) 
	{
		NewNetwork(pstUpload->pstNetWork, NULL);

		//1. 设置timer ,等待connect返回

		dlinkmq_upload_conn_timer(1);

		//2. 连接网络

		mqtt_fmt_print("---DlinkmqUpload_ConnectNetWork before ConnectNetwork");
		netRet = ConnectNetwork(pstUpload->pstNetWork, upload_host, upload_port);
		mqtt_fmt_print("---DlinkmqUpload_ConnectNetWork after ConnectNetwork netRet=%d", netRet);

		mqtt_fmt_print("---DlinkmqUpload_Init ConnectNetwork :%d", netRet);

		if (netRet == DlinkMQ_ERROR_CODE_SOC_CREAT
			|| netRet == DlinkMQ_ERROR_CODE_SOC_CONN
			|| netRet == DlinkMQ_ERROR_CODE_GET_HOSTNAME) //需要重新连接
		{
			DlinkmqUpload_DestroyNetwork(hDlinkmqUploadHandle, TRUE);

		} else if (netRet == DlinkMQ_ERROR_CODE_SUCCESS
			|| netRet == DlinkMQ_ERROR_CODE_WOULDBLOCK) { //需要设置timer, timer连接超时，需要重新连接


		} else { //其他内存错误。

			DlinkmqUpload_DestroyNetwork(hDlinkmqUploadHandle, FALSE);
			return ret;
		}

		ret = DlinkMQ_ERROR_CODE_SUCCESS;
		
	} 


	mqtt_fmt_print("---DlinkmqUpload_ConnectNetWork end");
	

	return ret;

}

we_void DlinkmqUpload_DestroyNetwork(we_handle hDlinkmqUploadHandle, we_bool isReconnect)
{
	St_DlinkmqUpload *pstUpload = (St_DlinkmqUpload *)hDlinkmqUploadHandle;


	if(pstUpload == NULL)
	{
		return;
	}

	dlinkmq_upload_conn_timer(0);

	DlinkmqNetwork_Destroy(pstUpload->pstNetWork);
	if(pstUpload->pSendBuff != NULL)
	{
		DLINKMQ_FREE(pstUpload->pSendBuff);
	}
	pstUpload->pstNetWork = NULL;
	pstUpload->pSendBuff = NULL;
	pstUpload->iBuffSize = 0;
	pstUpload->iSentSize = 0;
	pstUpload->status = DlinkmqUpload_Status_INIT;

	if(pstUpload->file != NULL)
	{
		FS_Close(pstUpload->file);
		pstUpload->file = NULL;
	}


	if (isReconnect == TRUE)//重连
	{
		mqtt_fmt_print("----DlinkmqUpload_DestroyNetwork reconnect");
		DlinkmqMsg_PostMsg(g_pstDlinkmqMsgHandle, E_MQ_MSG_MODULEID_HTTP, E_MQ_MSG_EVENTID_NEW_HTTP, 0, 0, 0, 0, NULL, NULL);
	}

	
}

we_int DlinkmqUpload_SockNotifyInd(we_handle hDlinkmqUploadHandle, we_void *pvMsg)
{
	we_int ret = DlinkMQ_ERROR_CODE_SUCCESS;
	app_soc_notify_ind_struct *soc_notify = (app_soc_notify_ind_struct*) pvMsg;
	St_DlinkmqUpload *pstUpload = (St_DlinkmqUpload *)hDlinkmqUploadHandle;
	we_int iResult  = DlinkMQ_ERROR_CODE_FAIL;


	if(soc_notify == NULL
		|| pstUpload == NULL)
	{
		return DlinkMQ_ERROR_CODE_FAIL;
	}

	iResult = soc_notify->result;

	mqtt_fmt_print("---n_for_upload soc_notify->event_type:%d",soc_notify->event_type);

	switch (soc_notify->event_type) 
	{
		case SOC_READ:
		{
			dlinkmq_upload_conn_timer(0);
			DlinkmqUpload_RecvBuffer(hDlinkmqUploadHandle);
		}
			break;
		case SOC_CLOSE:
		{
 			mqtt_fmt_print("\r\n---n_for_upload -SOC_CLOSE\n");
			pstUpload->status=DlinkmqUpload_Status_INIT;

			if(pstUpload->paths != NULL)
			{
				DlinkmqUpload_DestroyNetwork(hDlinkmqUploadHandle,TRUE);
			}
			else
			{
				DlinkmqUpload_DestroyNetwork(hDlinkmqUploadHandle,FALSE);
			}	
				
			
		}
			break;
		case SOC_WRITE:
		{
			dlinkmq_upload_conn_timer(0);
			DlinkmqUpload_PostBuffToSever(hDlinkmqUploadHandle);
		}
			break;
		case SOC_CONNECT:
		{
			if (iResult == KAL_TRUE) 
			{
				pstUpload->status = DlinkmqUpload_Status_CONNECT;
				dlinkmq_upload_conn_timer(0);

				mqtt_fmt_print("\r\n---n_for_upload -SOC_CONNECT success \n");
				DlinkmqUpload_UploadFile(hDlinkmqUploadHandle);
				
			} 
			else 
			{
				pstUpload->status = DlinkmqUpload_Status_INIT;
				mqtt_fmt_print("\r\n---n_for_upload -SOC_CONNECT failed reconnect \n");
				DlinkmqUpload_DestroyNetwork(hDlinkmqUploadHandle,TRUE);
			}

			
			
			break;
		}

	}


	return ret;

}

we_void DlinkmqUpload_Destroy(we_handle hDlinkmqUploadHandle)
{
	St_DlinkmqUpload *pstUpload = (St_DlinkmqUpload *)hDlinkmqUploadHandle;

	if(pstUpload != NULL) {

		DlinkmqUpload_DestroyNetwork(hDlinkmqUploadHandle, FALSE);
		DLINKMQ_FREE(pstUpload);
	}
}


we_int DlinkmqUpload_ParseJson(we_handle hDlinkmqUploadHandle, we_char *json_data){
	St_DlinkmqUpload *pstUpload = (St_DlinkmqUpload *)hDlinkmqUploadHandle;
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;
	cJSON *root = NULL;

	if(pstUpload == NULL || !pstUpload->paths->cb){
		return ret;
	}
    root = cJSON_Parse(json_data);
    if (root) {
        int ret_size = cJSON_GetArraySize(root);
        if (ret_size >= 2) {
            ret=strcmp(cJSON_GetObjectItem(root,"msg")->valuestring,"success");
			if(ret==0){
				mqtt_cb_exec_with_data(pstUpload->paths->cb, DlinkMQ_ERROR_CODE_SUCCESS, (void*)cJSON_GetObjectItem(root ,"url")->valuestring);
			}
			else
			{
				mqtt_cb_exec_with_data(pstUpload->paths->cb, ret, (void*)cJSON_GetObjectItem(root,"msg")->valuestring);
			}
        } else
            ret = DlinkMQ_ERROR_CODE_FAIL;
        
        cJSON_Delete(root);
    }
    return ret;


}	
we_void DlinkmqUpload_RecvBuffer(we_handle hDlinkmqUploadHandle)
{
	St_DlinkmqUpload *pstUpload = (St_DlinkmqUpload *)hDlinkmqUploadHandle;
	we_char *pTempBuffer = NULL;
	we_int iRecvNum = 0;
	we_char *pJsonStr = NULL;
	we_char *pLineStr = "\r\n\r\n";
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;

	if (pstUpload == NULL
		|| (pstUpload && pstUpload->pstNetWork == NULL))
	{
		return;
	}

	pTempBuffer = DLINKMQ_MALLOC(WE_SOCKET_MAX_UPLOAD_BUFFER_SIZE);
	iRecvNum = mtk_read(pstUpload->pstNetWork, pTempBuffer, WE_SOCKET_MAX_UPLOAD_BUFFER_SIZE, 0);

	mqtt_fmt_print("-----DlinkmqUpload_RecvBuffer:iRecvNum:%d", iRecvNum);

	if (iRecvNum > 0)
	{
		pJsonStr = strstr(pTempBuffer, pLineStr);

		if (pJsonStr)
		{
			pJsonStr += strlen(pLineStr);
			mqtt_fmt_print("-----DlinkmqUpload_RecvBuffer:pJsonStr:%s", pJsonStr);

			ret = DlinkmqUpload_ParseJson(hDlinkmqUploadHandle, pJsonStr);

			if (ret == DlinkMQ_ERROR_CODE_SUCCESS)
			{
				mqtt_fmt_print("---DlinkmqUpload_RecvBuffer: DlinkmqMsg_PostMsg");
				
				DlinkmqUpload_DeleteUploadParams(hDlinkmqUploadHandle);

				//还有文件需要继续上传
				if(pstUpload->paths != NULL){
					DlinkmqUpload_UploadFile(hDlinkmqUploadHandle);
				}
				else
				{
					DlinkmqUpload_DestroyNetwork(hDlinkmqUploadHandle, FALSE);
				}
				
			} else {
				DlinkmqUpload_DestroyNetwork(hDlinkmqUploadHandle, TRUE);
			}
		}

	}
	else if(SOC_WOULDBLOCK == iRecvNum)
	{
		dlinkmq_upload_conn_timer(1);
	}
	else
	{    
		DlinkmqUpload_DestroyNetwork(hDlinkmqUploadHandle, TRUE);
	}    
	DLINKMQ_FREE(pTempBuffer);
}
we_void DlinkmqUpload_UploadFile(we_handle hDlinkmqUploadHandle)
{
	St_DlinkmqUpload *pstUpload = (St_DlinkmqUpload *)hDlinkmqUploadHandle;
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;
	
	if(pstUpload->paths == NULL || pstUpload->status != DlinkmqUpload_Status_CONNECT)
	{
		return;
	}
	
	ret = DlinkmqUpload_GetPostHeaders(hDlinkmqUploadHandle);

	if (ret == DlinkMQ_ERROR_CODE_SUCCESS)
	{
		DlinkmqUpload_PostBuffToSever(hDlinkmqUploadHandle);
	}


}	
we_void DlinkmqUpload_PostBuffToSever(we_handle hDlinkmqUploadHandle)
{
	St_DlinkmqUpload *pstUpload = (St_DlinkmqUpload *)hDlinkmqUploadHandle;

	we_int iSendNum = 0;

	if (pstUpload == NULL ||  pstUpload->pSendBuff == NULL)
	{
		return;
	}

	while(pstUpload->iBuffSize > 0)
	{
		iSendNum = mtk_write(pstUpload->pstNetWork, pstUpload->pSendBuff + pstUpload->iSentSize, pstUpload->iBuffSize,0);
		mqtt_fmt_print("--DlinkmqUpload_PostBuffToSever-upload--len:%d\n",iSendNum);
		if (iSendNum > 0) {
			pstUpload->iSentSize += iSendNum;
			pstUpload->iBuffSize -= iSendNum;
				
			if (pstUpload->iBuffSize == 0 && pstUpload->file)
			{
				memset(pstUpload->pSendBuff, 0, sizeof(pstUpload->pSendBuff));
				FS_Read(pstUpload->file, pstUpload->pSendBuff, WE_SOCKET_MAX_UPLOAD_BUFFER_SIZE, &pstUpload->iBuffSize);
				pstUpload->iSentSize = 0;

				if(pstUpload->iBuffSize == 0){
					FS_Close(pstUpload->file);
					pstUpload->file = NULL;
						
					pstUpload->iBuffSize = sprintf(pstUpload->pSendBuff,"%s","\r\n--------------7788656end--\r\n");
				}
			}

					
		} 
		else if (SOC_WOULDBLOCK == iSendNum) {//设置超时

			dlinkmq_upload_conn_timer(1);
			
			return;
		} else {//重连

			DlinkmqUpload_DestroyNetwork(hDlinkmqUploadHandle, TRUE);

			return;
		}
				
	} 

	DLINKMQ_FREE(pstUpload->pSendBuff);
	pstUpload->pSendBuff = NULL;
	pstUpload->iSentSize = 0;
	pstUpload->iBuffSize = 0;
	

	return;
}

static U16 mmi_asc_to_ucs2(S8 *pOutBuffer, S8 *pInBuffer)
{
	/*----------------------------------------------------------------*/
	/* Local Variables                                                */
	/*----------------------------------------------------------------*/
	S16 count = -1;

	/*----------------------------------------------------------------*/
	/* Code Body                                                      */
	/*----------------------------------------------------------------*/
	while (*pInBuffer != '\0')
	{
		pOutBuffer[++count] = *pInBuffer;
		pOutBuffer[++count] = 0;
		pInBuffer++;
	}

	pOutBuffer[++count] = '\0';
	pOutBuffer[++count] = '\0';
	return count + 1;
}
we_int DlinkmqUpload_GetPostHeaders(we_handle hDlinkmqUploadHandle)
{
	St_DlinkmqUpload *pstUpload = (St_DlinkmqUpload *)hDlinkmqUploadHandle;
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;
	we_char path[]="/oss/uploadPublic";
	we_char upload_head[] =
		"POST %s HTTP/1.1\r\n"
		"Host: %s:%d\r\n"
		"Connection: keep-alive\r\n"
		"Content-Type: multipart/form-data; boundary=%s\r\n"
		"Content-Length: %d\r\n\r\n"
		"--%s\r\n"
		"Content-Disposition: form-data; name=\"audio\"; filename=\"%s\"\r\n"
		"Content-Type: application/octet-stream;chartset=UTF-8\r\n\r\n";
	we_char upload_request[] = 
		"--%s\r\n"
		"Content-Disposition: form-data; name=\"audio\"; filename=\"%s\"\r\n"
		"Content-Type: application/octet-stream;chartset=UTF-8\r\n\r\n";
	we_char boundary[]="------------7788656end";
	we_char upload_end[33]={0};
	we_int content_length=0;
	we_char UnicodeName[512]={0};
	we_int file_size = 0;
	

	//
	if(pstUpload->pSendBuff!=NULL || pstUpload->paths == NULL || pstUpload->paths->path ==NULL){
		return ret;
	}

	mmi_asc_to_ucs2(UnicodeName,pstUpload->paths->path);

	if((pstUpload->file= FS_Open((const WCHAR *)UnicodeName , FS_READ_ONLY|FS_OPEN_SHARED)) >= 0)
	{
		ret=FS_GetFileSize(pstUpload->file,&file_size);
		if(file_size<=0){
			mqtt_fmt_print("\n--DlinkmqUpload_GetPostHeaders-dlinkmq_upload-nofile");
			return DlinkMQ_ERROR_CODE_PARAM;
		}
	}


	content_length += file_size;
	content_length +=_snprintf(upload_end,33,"\r\n--%s--\r\n",boundary);
	
	pstUpload->pSendBuff = DLINKMQ_MALLOC(WE_SOCKET_MAX_UPLOAD_BUFFER_SIZE);
	if(pstUpload->pSendBuff == NULL){
		return DlinkMQ_ERROR_CODE_DISK_FULL;
	}
	
	content_length += sprintf(pstUpload->pSendBuff, upload_request, boundary, pstUpload->paths->path);
	mqtt_fmt_print("\n--DlinkmqUpload_GetPostHeaders-content_length:%d\n",content_length);

	pstUpload->iBuffSize = sprintf(pstUpload->pSendBuff, upload_head, path, upload_host, upload_port, boundary, content_length, boundary, pstUpload->paths->path);
	pstUpload->iSentSize = 0;


	return DlinkMQ_ERROR_CODE_SUCCESS;
}

we_int DlinkmqUpload_AddUploadParams(we_handle hDlinkmqUploadHandle, we_char* file_path, dlinkmq_ext_callback cb){
	St_DlinkmqUpload *pstUpload = (St_DlinkmqUpload *)hDlinkmqUploadHandle;
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;
	St_DlinkmqUploadPaths*lastPaths = NULL;
	St_DlinkmqUploadPaths*tempPaths = NULL;

	if(char_in_string(file_path, '/')==0){
		mqtt_fmt_print("\n---DlinkmqUpload_AddUploadParams-nofile");
		return DlinkMQ_ERROR_CODE_PARAM;
	}

	if(pstUpload->paths == NULL){
		pstUpload->paths = DLINKMQ_MALLOC(sizeof(St_DlinkmqUploadPaths));
		if(pstUpload->paths == NULL)
		{
			return DlinkMQ_ERROR_CODE_DISK_FULL;
		}

		pstUpload->paths->cb = cb;
		pstUpload->paths->path = file_path;
		pstUpload->paths->next =NULL;
		pstUpload->paths->tail = pstUpload->paths;
	}
	else{
		//加到队列尾部
		#if 1

		St_DlinkmqUploadPaths *pstTail = NULL;

		tempPaths = DLINKMQ_MALLOC(sizeof(St_DlinkmqUploadPaths));
		if(tempPaths == NULL)
		{
			return DlinkMQ_ERROR_CODE_DISK_FULL;
		}
		tempPaths->cb = cb;
		tempPaths->path = file_path;
		tempPaths->next = NULL;

		pstTail = pstUpload->paths->tail;
		
		pstTail->next = tempPaths;
		pstUpload->paths->tail = tempPaths;

		#else

		
		lastPaths = pstUpload-> paths;

		while(lastPaths->next){
			lastPaths = lastPaths->next;
		}

		tempPaths = DLINKMQ_MALLOC(sizeof(St_DlinkmqUploadPaths));
		tempPaths->cb = cb;
		tempPaths->path = file_path;
		tempPaths->next = NULL;

		lastPaths->next = tempPaths;
		


		#endif

	}

	return DlinkMQ_ERROR_CODE_SUCCESS;
}
we_int DlinkmqUpload_DeleteUploadParams(we_handle hDlinkmqUploadHandle){
	St_DlinkmqUpload *pstUpload = (St_DlinkmqUpload *)hDlinkmqUploadHandle;
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;
	St_DlinkmqUploadPaths*tempPaths = pstUpload->paths;

	if(pstUpload->paths == NULL){
		return ret;
	}
	
	if(pstUpload->paths->next ==NULL){
		pstUpload->paths = NULL;
	}
	else{
		pstUpload->paths = pstUpload->paths->next;
	}

	DLINKMQ_FREE(tempPaths);

	return DlinkMQ_ERROR_CODE_SUCCESS;
}

