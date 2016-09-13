
#include "mmi_platform.h"
//#ifdef __NON_BLOCKING_FILE_MOVE_SUPPORT__
#include "kal_public_api.h"

#include "kal_public_defs.h" 
#include "stack_ltlcom.h"


#include "syscomp_config.h"
#include "task_config.h"        /* Task creation */

#include "stacklib.h"           /* Basic type for dll, evshed, stacktimer */
#include "stack_timer.h"        /* Stack timer */

#include "fat_fs.h"             /* file system */
///#endif /* __NON_BLOCKING_FILE_MOVE_SUPPORT__ */ 
#include "stack_ltlcom.h"
#include "FileManagerGProt.h"
#include "fmt_struct.h"
#include "stdafx.h"

#ifdef __DRM_SUPPORT__
#include "drm_gprot.h"
#endif

#include "kal_debug.h"
#include "Fs_gprot.h"
#include "stdlib.h"
//#include "String.h"
//#include "stdio.h"
#include <stdio.h>
#include <string.h>

#include "mqtt_type_priv.h"
#include "MQTTMTK.h"
#include "MQTTClient.h"
#include "MQTTcJSON.h"
#include "MQTTMd5.h"

#include "MQTTTask.h"
#include "MQTTTimer.h"
#include "MQTTTrace.h"

#include "dlinkmq_api.h"
#include "dlinkmq_mgr.h"

static stack_timer_struct g_mqtt_task_stack_timer;
static stack_timer_struct g_mqtt_task_stack_init_timer;
static stack_timer_struct g_mqtt_task_stack_reconnect_timer;
static stack_timer_struct g_mqtt_task_stack_ping_timer;
static stack_timer_struct g_mqtt_task_stack_http_connect_timer;
static stack_timer_struct g_mqtt_task_stack_client_connect_timer;

static int tryReconnect = 0;


Network g_upload_net;
static int g_upload_net_connect = 0;


dlinkmq_ext_callback cb_http_upload;




typedef int (*CALLBACK)(char *p);
void cbconn(int result,void * data);
int dlinkmq_upload(char* path, dlinkmq_ext_callback cb);
void cb_dlinkmq_gethostbyname(void *data);
void cb_dlinkmq_publish(we_int32 err_code, MQTTMessage *message);
we_int32 dlinkmq_parse_payload(we_int8 *payload);

typedef struct mqtt_cb_exec_msg
{
	LOCAL_PARA_HDR
	MQTTAsyncCallbackFunc cb;
	int result;
	void * data;
}mqtt_cb_exec_msg;

typedef struct{
	char buf[4096];
	int buf_size;
}http_soc_buf;
http_soc_buf soc_buf;
FS_HANDLE file_h;






we_handle g_pstDlinkmqMgr = NULL;
we_handle g_pstDlinkmqMsgHandle = NULL;
dlinkmq_device_info *g_device_info = NULL;
dlinkmq_on_receive *g_fun_cb = NULL;

static we_void dlinkmq_init_timmer();
static void mqtt_service_init_timer_start();

we_int32 dlinkmq_init_device_info(dlinkmq_device_info *device_info, dlinkmq_on_receive *fun_cb)
{
    we_int32 ret = DlinkMQ_ERROR_CODE_FAIL;
		
	if(!device_info || !fun_cb)
	{
		return ret;
	}
#if 0
	ret = DlinkmqMsg_Init((we_handle *)&g_pstDlinkmqMsgHandle);
	mqtt_fmt_print("---dlinkmq_init_device_info  DlinkmqMsg_Init ret=%d",ret);
	
	ret = DlinkmqMgr_Init((we_handle *)&g_pstDlinkmqMgr);
	mqtt_fmt_print("---dlinkmq_init_device_info  DlinkmqMgr_Init ret=%d",ret);

	DlinkmqMgr_SetData(g_pstDlinkmqMgr, device_info, fun_cb);
#endif
	g_device_info = device_info;
	g_fun_cb = fun_cb;

	dlinkmq_init_timmer();
   
	mqtt_fmt_print("---dlinkmq_init_device_info  end");

	return DlinkMQ_ERROR_CODE_SUCCESS;
}

static we_void dlinkmq_init_timmer()
{   
	
	mqtt_service_init_timer_start();

	stack_init_timer(&g_mqtt_task_stack_timer, "MQTT Timer", MOD_MQTT);
	stack_init_timer(&g_mqtt_task_stack_reconnect_timer,"MQTT Reconnect Timer",MOD_MQTT);

	stack_init_timer(&g_mqtt_task_stack_http_connect_timer,"MQTT HTTP Connect Timer",MOD_MQTT);
	stack_init_timer(&g_mqtt_task_stack_client_connect_timer,"MQTT Client Connect Timer",MOD_MQTT);
	stack_init_timer(&g_mqtt_task_stack_ping_timer,"MQTT Ping Connect Timer",MOD_MQTT);
	
}

we_void dlinkmq_httpconn_timer(we_uint8 start)
{
	mqtt_fmt_print("---mqtt dlinkmq_httpconn_timer  sart = %d", start);
	if(start){

		mqtt_fmt_print("---mqtt dlinkmq_httpconn_timer  stack_start_timer");
		stack_start_timer(&g_mqtt_task_stack_http_connect_timer, MQTT_STACK_HTTP_CONNECT_TIMEROUT_ID, KAL_TICKS_1_MIN);
	}else{

		mqtt_fmt_print("---mqtt dlinkmq_httpconn_timer stack_stop_timer");
		stack_stop_timer(&g_mqtt_task_stack_http_connect_timer);
	}
}

we_void dlinkmq_client_reconn_timer(we_uint8 start)
{
	mqtt_fmt_print("---mqtt dlinkmq_client_reconn_timer  sart = %d", start);
	if(start){

		mqtt_fmt_print("---mqtt dlinkmq_client_reconn_timer  stack_start_timer");
		stack_start_timer(&g_mqtt_task_stack_client_connect_timer, MQTT_STACK_CLIENT_CONNECT_TIMEROUT_ID, KAL_TICKS_1_MIN);
	}else{

		mqtt_fmt_print("---mqtt dlinkmq_client_reconn_timer stack_stop_timer");
		stack_stop_timer(&g_mqtt_task_stack_client_connect_timer);
	}
}

we_void dlinkmq_ping_conn_timer(we_uint8 start, kal_uint32 in_time)
{
	mqtt_fmt_print("---mqtt dlinkmq_ping_timer  sart = %d", start);
	if(start){

		mqtt_fmt_print("---mqtt dlinkmq_ping_timer  stack_start_timer");
		stack_start_timer(&g_mqtt_task_stack_ping_timer, MQTT_STACK_PING_CONNECT_TIMEROUT_ID, in_time);
	}else{

		mqtt_fmt_print("---mqtt dlinkmq_ping_timer stack_stop_timer");
		stack_stop_timer(&g_mqtt_task_stack_ping_timer);
	}
}



stack_timer_status_type dlinkmq_get_httpconn_timer_status()
{
	return g_mqtt_task_stack_http_connect_timer.timer_status;
}


void mqtt_cb_exec_with_data(MQTTAsyncCallbackFunc cb,int result,void *data){
	ilm_struct *msg = NULL;
	mqtt_cb_exec_msg *cb_msg = construct_local_para(sizeof(mqtt_cb_exec_msg), TD_CTRL);
	msg = allocate_ilm(stack_get_active_module_id());
	ASSERT(msg);
	ASSERT(cb_msg);

	mqtt_fmt_print("---mqtt_cb_exec_with_data  resultt=%d",result);
	cb_msg->cb = cb;
	cb_msg->result = result;
	cb_msg->data = data;
	msg->dest_mod_id = MOD_MQTT;
	msg->src_mod_id = stack_get_active_module_id();
	msg->local_para_ptr = (local_para_struct *)cb_msg;
	msg->msg_id = MSG_ID_MQTT_CB_EXEC;
	msg->peer_buff_ptr = NULL;
	msg->sap_id = INVALID_SAP;
	msg_send_ext_queue(msg);


}


void mqtt_cb_exec(MQTTAsyncCallbackFunc cb,int result){
	 mqtt_cb_exec_with_data(cb,result,NULL);
}



void dlinkmq_init_handle(void){
	we_int32 ret=0;
	
	ret = DlinkmqMsg_Init((we_handle *)&g_pstDlinkmqMsgHandle);
	mqtt_fmt_print("---dlinkmq_init_device_info  DlinkmqMsg_Init ret=%d",ret);
	
	ret = DlinkmqMgr_Init((we_handle *)&g_pstDlinkmqMgr);
	mqtt_fmt_print("---dlinkmq_init_device_info  DlinkmqMgr_Init ret=%d",ret);

	DlinkmqMgr_SetData(g_pstDlinkmqMgr, g_device_info, g_fun_cb);
	g_device_info = NULL;
	g_fun_cb = NULL;
	
	kal_prompt_trace(MOD_MQTT,"---dlinkmq_init_handle:%d",ret);
	
}
//static int mqtt_service_start()
int mqtt_service_start() 
{
	int ret=-1;

	mqtt_fmt_print("---mqtt_service_start mqtt_service_start");
	DlinkmqMsg_PostMsg(g_pstDlinkmqMsgHandle, E_MQ_MSG_MODULEID_MQTT, E_MQ_MSG_EVENTID_MQTT_INIT, 0, 0, 0, 0, NULL, NULL);

#if 0
	NewNetwork(&g_mqtt_net ,dispatchEvents);
	ret=ConnectNetwork(&g_mqtt_net, g_dmq_client.server, g_dmq_client.port);
	mqtt_fmt_print("---mqtt_service_start:%d",ret);
#endif
	return ret;
}

static void dlinkmq_service_init() 
{
	we_int ret=0;
	mqtt_fmt_print("---dlinkmq_service_init init");
	if(g_device_info && g_fun_cb)
	{
		dlinkmq_init_handle();
	}
	ret = DlinkmqMsg_SendMsg(g_pstDlinkmqMsgHandle, E_MQ_MSG_MODULEID_HTTP, E_MQ_MSG_EVENTID_NEW_HTTP, 0, 0, 0, 0, NULL, NULL);

	mqtt_fmt_print("---dlinkmq_service_init end  ret=%d",ret);
}




static void mqtt_service_stop()
{
	//mqtt_fmt_print("\nservice STOP!\n");
}


static void mqtt_service_init_timer_start()
{
	mqtt_fmt_print("-----------mqtt_service_init_timer_start");
	stack_init_timer(&g_mqtt_task_stack_init_timer, "MQTT INIT Timer", MOD_MQTT);

	#ifdef  __MTK_TARGET__
	stack_start_timer(&g_mqtt_task_stack_init_timer, MQTT_STACK_INIT_TIMER_ID, KAL_TICKS_1_MIN * 2);
	#else 
	stack_start_timer(&g_mqtt_task_stack_init_timer, MQTT_STACK_INIT_TIMER_ID, KAL_TICKS_3_SEC);
	#endif
	//stack_start_timer(&g_mqtt_task_stack_init_timer, MQTT_STACK_INIT_TIMER_ID, KAL_TICKS_30_SEC);
}


static void mqtt_service_init_timer_stop()
{
	stack_stop_timer(&g_mqtt_task_stack_init_timer);
}

static void mqtt_service_set_keepalive_timer(kal_uint8 start)
{
	if(start)
	{
		stack_start_timer(&g_mqtt_task_stack_timer, MQTT_STACK_TIMER_ID, KAL_TICKS_50_MSEC);//L?5L?paG?TICK,KAL_TICKS_5_SEC
	}
	else
	{
		stack_stop_timer(&g_mqtt_task_stack_timer);
	}
}

static void mqtt_service_set_reconnect_timer(kal_uint8 flag){
	if(flag){
		stack_start_timer(&g_mqtt_task_stack_reconnect_timer, MQTT_STACK_RECONNECT_TIMER_ID, KAL_TICKS_30_SEC);
	}else{
		stack_stop_timer(&g_mqtt_task_stack_reconnect_timer);
	}
}

void dlinkmq_service_set_keepalive_timer(kal_uint8 start)
{
	mqtt_service_set_keepalive_timer(start);
}

void dlinkmq_service_set_reconnect_timer(kal_uint8 start)
{
	mqtt_service_set_reconnect_timer(start);
}



void mqtt_app_sendmsg_to_task(msg_type iMsg)
{
	/*----------------------------------------------------------------*/
    /* Local Variables                                                */
    /*----------------------------------------------------------------*/
    ilm_struct *ilm_ptr = NULL;


	//mqtt_test_msg_t *msg_p = (mqtt_test_msg_t*)
	//		construct_local_para(sizeof(mqtt_test_msg_t), TD_CTRL);

	//msg_p->status = 1;

	
	mqtt_fmt_print("---mqtt_app_sendmsg_to_task:%d-----",iMsg);
	
    ilm_ptr = allocate_ilm(stack_get_active_module_id());
    ilm_ptr->msg_id = iMsg; /* Set the message id */
    ilm_ptr->peer_buff_ptr = NULL;                  /* there are no peer message */
	ilm_ptr->local_para_ptr = NULL;
	
    //ilm_ptr->local_para_ptr = (local_para_struct*)msg_p;                 /* there are no local message */
    
	#if 1
    ilm_ptr->src_mod_id  = stack_get_active_module_id();
    ilm_ptr->dest_mod_id = MOD_MQTT;
    ilm_ptr->sap_id = INVALID_SAP;
	msg_send_ext_queue(ilm_ptr);
	#else
	SEND_ILM(MOD_MQTT, MOD_MQTT, INVALID_SAP /*Add correct sap*/, ilm_ptr);
	#endif
}

void get_mqtt_test(void)
{
    /*----------------------------------------------------------------*/
    /* Local Variables                                                */
    /*----------------------------------------------------------------*/
    ilm_struct *ilm_ptr = NULL;


	//mqtt_test_msg_t *msg_p = (mqtt_test_msg_t*)
	//		construct_local_para(sizeof(mqtt_test_msg_t), TD_CTRL);

	//msg_p->status = 1;

	
	mqtt_fmt_print("---get_mqtt_test(): send message - MSG_ID_MQTT_STATUS-----");
	
    ilm_ptr = allocate_ilm(MOD_MQTT);
    ilm_ptr->msg_id = MSG_ID_MQTT_STATUS; /* Set the message id */
    ilm_ptr->peer_buff_ptr = NULL;                  /* there are no peer message */
	ilm_ptr->local_para_ptr = NULL;
	
    //ilm_ptr->local_para_ptr = (local_para_struct*)msg_p;                 /* there are no local message */
    
	#if 1
    ilm_ptr->src_mod_id  = MOD_MQTT;
    ilm_ptr->dest_mod_id = MOD_MQTT;
    ilm_ptr->sap_id = INVALID_SAP;
	msg_send_ext_queue(ilm_ptr);
	#else
	SEND_ILM(MOD_MQTT, MOD_MQTT, INVALID_SAP /*Add correct sap*/, ilm_ptr);
	#endif
}


static void cbMQTTAsyncYield(int result)
{
}

void dlinkmq_sock_notify_ind(void *inMsg)
{
    app_soc_notify_ind_struct *soc_notify = (app_soc_notify_ind_struct*) inMsg;

	E_DlinkmqMsgModuleId moduleId = E_MQ_MSG_MODULEID_BASE; 

    if(soc_notify == NULL)
    {
        return;
    }

	moduleId = DlinkmqMgr_GetEventIdBySockId(g_pstDlinkmqMgr, soc_notify->socket_id);


	DlinkmqMsg_SendMsg(g_pstDlinkmqMsgHandle, moduleId, E_MQ_MSG_EVENTID_SOC_NOTIFY_IND, 0, 0, 0, 0, (we_void*)inMsg, NULL);

	
#if 0
	if(soc_notify->socket_id==g_mqtt_net.my_socket  && soc_notify->socket_id!=0){
		dispatchEvents(soc_notify);
	}
	else if(soc_notify->socket_id==g_upload_net.my_socket  && soc_notify->socket_id!=0){

		mqtt_fmt_print("---g_upload_net soc_notify->event_type:%d",soc_notify->event_type);
		switch (soc_notify->event_type) {
		case SOC_READ:{
			http_recv_buf();
			break;
					  }
		case SOC_CLOSE:{
			mqtt_fmt_print("\r\n---g_upload_net -SOC_CLOSE\n");
			g_upload_net_connect=0;
			//发送文件中断开，需要重连
			if(soc_buf.buf_size){
				ConnectNetwork(&g_upload_net, upload_host, upload_port);
			}
			break;
					   }
		case SOC_WRITE:{

			http_send_buf();
			break;
					   }
		case SOC_ACCEPT:{

			break;
						}
		case SOC_CONNECT:{
			
			g_upload_net_connect = 1;
			if(soc_buf.buf_size){
				http_send_buf();
			}
			break;
						 }

		}
	}
	else{
		mqtt_fmt_print("---n_for_http soc_notify->event_type:%d",soc_notify->event_type);
		switch (soc_notify->event_type) {
		case SOC_READ:{
			http_recv_buf();
			break;
					  }
		case SOC_CLOSE:{
			mqtt_fmt_print("\r\n---n_for_http -SOC_CLOSE\n");
			//n_for_http断开，需要重连
			if(g_dmq_client.mqttstatus==MQTT_STATUS_INIT){
				mqtt_service_init();
			}
			break;
					   }
		case SOC_WRITE:{
			
			http_send_buf();
			break;
					   }
		case SOC_ACCEPT:{

			break;
						}
		case SOC_CONNECT:{
			//初始化时需要回调，然后连接MQTT
			if(g_dmq_client.mqttstatus==MQTT_STATUS_INIT){
				mqtt_cb_exec(cb_mqtt_service_init,SUCCESS);
			}
			break;
						 }

		}

	}
#endif

	
}

void cbYield(int result,void *data);

/*
#define LOCAL_PARA_HDR \
   kal_uint8	ref_count; \
   kal_uint16	msg_len;
*/


static void mqtt_reconnect(void);

static int dlinkmq_cb_msg_gethostbyname(app_soc_get_host_by_name_ind_struct *dns_msg);
 void dlinkmq_run(ilm_struct *ilm_ptr)
{
	if (NULL != ilm_ptr)
	{
		//mqtt_fmt_print("\n----------ilm_ptr->msg_id=%d\n",ilm_ptr->msg_id);
		switch(ilm_ptr->msg_id)
		{
		case MSG_ID_TIMER_EXPIRY:
			{
				stack_timer_struct *pStackTimerInfo=(stack_timer_struct *)ilm_ptr->local_para_ptr;
				St_DlinkmqMqtt *pstMqtt = DlinkmqMgr_GetMqtt(g_pstDlinkmqMgr);
				if (NULL != pStackTimerInfo)
				{
					//mqtt_fmt_print("\n----------MQTT timer: pStackTimerInfo->timer_indx=%d\n", pStackTimerInfo->timer_indx);
					if (MQTT_STACK_TIMER_ID == pStackTimerInfo->timer_indx)
					{
						if (pstMqtt && pstMqtt->mqttStatus == MQTT_STATUS_CONN)
						{
							cbYield(SUCCESS,NULL);
						} else if (pstMqtt)
						{

							mqtt_fmt_print("\n----------MQTT timer: MQTT_STACK_TIMER_ID status=%d\n", pstMqtt->mqttStatus);
						}
						
					} 
					else if (MQTT_STACK_INIT_TIMER_ID == pStackTimerInfo->timer_indx)
					{
						mqtt_fmt_print("\n--MQTT_STACK_INIT_TIMER_ID");
						//初始化
						dlinkmq_service_init();
						
						mqtt_service_init_timer_stop();

						//MQTTAsyncYield(&c, 1000, cbMQTTAsyncYield);
						//mqtt_service_set_keepalive_timer(1);
					}else if(MQTT_STACK_RECONNECT_TIMER_ID == pStackTimerInfo->timer_indx){

						mqtt_fmt_print("\n--MQTT_STACK_RECONNECT_TIMER_ID start");

						tryReconnect = 0;
						mqtt_reconnect();
					} else if (MQTT_STACK_HTTP_CONNECT_TIMEROUT_ID == pStackTimerInfo->timer_indx) {

						
						St_DlinkmqHttp *pstHttp = DlinkmqMgr_GetHttp(g_pstDlinkmqMgr);

						if (pstMqtt 
							&& pstMqtt->mqttStatus != MQTT_STATUS_INIT
							&& pstHttp
							&& pstHttp->isInitFin == TRUE)
						{
							mqtt_service_start();
						} else 
						{
							dlinkmq_service_init();
						}
						
						mqtt_fmt_print("---MQTT_STACK_HTTP&MQTT_CONNECT_TIMEROUT_ID http timeout-----");
						
					}
					else if (MQTT_STACK_CLIENT_CONNECT_TIMEROUT_ID == pStackTimerInfo->timer_indx)
					{
						Client *pstClient = DlinkmqMgr_GetClient(g_pstDlinkmqMgr);

						if(pstClient && pstClient->fnReadTimeout)
						{
							pstClient->fnReadTimeout(0, NULL);
						}
							
					}
					else if (MQTT_STACK_PING_CONNECT_TIMEROUT_ID == pStackTimerInfo->timer_indx)
					{
						Client *pstClient = DlinkmqMgr_GetClient(g_pstDlinkmqMgr);

						if(pstClient && pstClient->fnPingTimeout)
						{
							pstClient->fnPingTimeout(0, NULL);
						}
					}		
					else
					{
						mqtt_fmt_print("---MQTT timer: other's timer id-----");
					}
				}
			}
			break;
		case MSG_ID_MQTT_STATUS:
			
			{

				mqtt_fmt_print("---mqtt_dispatch()MSG_ID_MQTT_STATUS-----");
#if 0
				mqtt_service_connect();
#endif
			}
			break;
		case MSG_ID_APP_SOC_NOTIFY_IND:
			dlinkmq_sock_notify_ind((void *)(ilm_ptr->local_para_ptr));
			break;
		case MSG_ID_APP_SOC_GET_HOST_BY_NAME_IND:
			dlinkmq_cb_msg_gethostbyname((void *)(ilm_ptr->local_para_ptr));
			break;
		case MSG_ID_MQTT_CB_EXEC:{
			mqtt_cb_exec_msg *msg = (mqtt_cb_exec_msg *)ilm_ptr->local_para_ptr;
			MQTTAsyncCallbackFunc cb;
			int result;
			void *data;
			if (msg != NULL){
				cb = msg->cb;
				result = msg->result;
				data = msg->data;
				free_local_para(ilm_ptr->local_para_ptr);
				ilm_ptr->local_para_ptr = NULL;
				if(cb){
					cb(result,data);
				}
			}
			
			break;
		}
		default:
			break;
		}
	}
}




static int toStop;

void cbDisconnect(int result){

	St_DlinkmqMqtt *pstMqtt = DlinkmqMgr_GetMqtt(g_pstDlinkmqMgr);

	if (pstMqtt && pstMqtt->pstNetWork)
	{
		pstMqtt->pstNetWork->disconnect(pstMqtt->pstNetWork);
	}
	
}

static int yieldCount = 0;
static kal_uint32 lastYieldTime = 0;
//mqtt_service_set_keepalive_timer
void cbYield(int result,void* data){

	kal_uint32 ticks,currentTime;
	int inteval;
	static int minimumYieldInteval = 1000;

	Client *pstClient = DlinkmqMgr_GetClient(g_pstDlinkmqMgr);
	
	if(toStop){
		MQTTAsyncDisconnect(pstClient,NULL);
	//}else if(result == FAILURE){
		//mqtt_fmt_print("\n-----------------------------------yield FAIL!\n");
		//mqtt_service_connect();
	}else{
		
		kal_get_time(&ticks);
		currentTime = kal_ticks_to_milli_secs(ticks);
		inteval = currentTime - lastYieldTime;
		mqtt_fmt_print("\n---mqtt yield current:%d yield last:%d inteval:%d\n ",currentTime,lastYieldTime,inteval);
		
		if(inteval < minimumYieldInteval){
			mqtt_fmt_print("\n---mqtt skip yield!\n");
			mqtt_service_set_keepalive_timer(1);
			return;
		}
		yieldCount++;
		mqtt_fmt_print("\n---mqttt YIELD! count:%d\n",yieldCount);

		lastYieldTime = currentTime;
		MQTTAsyncYield(pstClient,100000,cbYield);
	}


}


void cbSubscribe(int result,void *data){

	Client *pstClient = DlinkmqMgr_GetClient(g_pstDlinkmqMgr);

	mqtt_fmt_print("---cbSubscribe:%d",result);


	if(result == 0){

		toStop = 0;
		yieldCount = 0;
		MQTTAsyncYield(pstClient,100000,cbYield);
	}else{
		mqtt_service_stop();
	}
	
}

void handleMessage(MessageData* md){
	mqtt_fmt_print("--handleMessage--len:%d--payload:%s-------",md->message->payloadlen,(char*)md->message->payload);

	if(md->message->payloadlen>13 && md->message->payloadlen<DLINKMQ_MQTT_BUF_SIZE){
		dlinkmq_parse_payload(md->message->payload);
	}
	//dlinkmq_parse_payload("#510200fd000611");
}

static int connectRetryCount = 0;

static void mqtt_reconnect(void){
	

	St_DlinkmqMqtt *pstMqtt = DlinkmqMgr_GetMqtt(g_pstDlinkmqMgr);

	mqtt_fmt_print("\n------connect retry count :%d\n",connectRetryCount);

	if (pstMqtt && pstMqtt->pstNetWork)
	{
		connectRetryCount++;
		//pstMqtt->pstNetWork->disconnect(pstMqtt->pstNetWork);	
		mqtt_service_start(); 
		//mqtt_service_connect();
		mqtt_service_set_reconnect_timer(1);
		tryReconnect = 1;
	}

	
}

void dlinkmq_ping_timeout(void){

	mqtt_fmt_print("---dlinkmq_ping_timeout dlinkmq_ping_timeout");
	DlinkmqMsg_PostMsg(g_pstDlinkmqMsgHandle, E_MQ_MSG_MODULEID_MQTT, E_MQ_MSG_EVENTID_MQTT_INIT, 0, 0, 0, 0, NULL, NULL);
	//mqtt_service_connect();
}


static void dlinkmq_gethostbyname(kal_int32 opid, kal_char * domain_name, kal_uint8 access_id);

void cbconn(int result,void *data){

	dlinkmq_on_receive *pstRecvInfo =  DlinkmqMgr_GetRecvInfo(g_pstDlinkmqMgr);
	Client *pstClient = DlinkmqMgr_GetClient(g_pstDlinkmqMgr);
	St_DlinkmqMqttSubPub *pstMqttSubPub =  DlinkmqMgr_GetMqttSubPubInfo(g_pstDlinkmqMgr);
	St_DlinkmqMqtt *pstMqtt = DlinkmqMgr_GetMqtt(g_pstDlinkmqMgr);

	if (pstClient == NULL
		|| pstMqtt == NULL)
	{
		return;
	}

	if(pstMqtt->mqttStatus != MQTT_STATUS_CONN)
	{
		mqtt_fmt_print("---cbconn: mqtt status is connected = %d",pstMqtt->mqttStatus);
		return;
	}

	mqtt_fmt_print("---cbconn:%d",result);

	if(pstRecvInfo->on_receive_init){

		pstRecvInfo->on_receive_init(result);
		//mqtt_cb_exec(pstRecvInfo->on_receive_init, result);
	}
	
	if(result==0){
		mqtt_service_set_reconnect_timer(0);
		connectRetryCount = 0;
		tryReconnect = 0;
		MQTTAsyncSubscribe(pstClient, pstMqttSubPub->pcSubTopic, QOS1,handleMessage,cbSubscribe);

	}else if (!tryReconnect){
		
		mqtt_reconnect();
	}

	
}
static U8 SockNotifyCb(void* inMsg){

}


static  we_int32 ASC2BYTE(const char* in_psz, long* out_plLen, char* out_pus)
{
    we_int32    lLen, lRet = 0;
    register char ch;
    register long   i, k = 0;
    
    if(!in_psz)
    {
        *out_plLen = 0;
        return 0;
    }
    else
        lLen = strlen(in_psz);
    
    if(lLen % 2)
    {
        //mqtt_fmt_print("\n----输入的ASCII串长度非偶数!");
        lRet = -1;
    }
    
    for(i = 0; i < lLen; i = i + 2)
    {
        if(in_psz[i] <= '9')
            ch = in_psz[i] & 15;            //  15 = 0x0F
        else
            ch = (in_psz[i] & 15) + 9;      //  15 = 0x0F, 9 = 0x09
        
        ch = ch << 4;
        if(in_psz[i+1] <= '9')
            ch |= in_psz[i+1] & 15;         //  15 = 0x0F
        else
            ch |= (in_psz[i+1] & 15) + 9;   //  15 = 0x0F
        
        out_pus[k] = ch;k++;
    }
    out_pus[k] = 0;
    if(out_plLen) *out_plLen = k;
	
    return lRet;
}

we_int32 hex2int(we_int8 *s)
{
	we_int32 i,t;
	we_int32 sum=0;
	for(i=0;s[i];i++){
		if(s[i]<='9')
			t=s[i]-'0';
		else  
			t=s[i]-'a'+10;

		sum=sum*16+t;
	}
	return sum;
}
we_int32 char2int(we_int8 *str){
    return atoi(str);
}
we_int32 dlinkmq_parse_payload(we_int8 *payload){

	we_int32 ret=0;

	#if 1
	dlinkmq_on_receive *pstRecvInfo =  DlinkmqMgr_GetRecvInfo(g_pstDlinkmqMgr);
	dlinkmq_datapoint datapoint;
	we_int8 *hex_temp=DLINKMQ_MALLOC(5);
	we_int8 *data=NULL;
	
	if(pstRecvInfo == NULL || hex_temp== NULL || payload[0]!='#' || strlen(payload)<13){
		DLINKMQ_FREE(hex_temp);
		return DlinkMQ_ERROR_CODE_PARAM;
	}
	
	strncpy( hex_temp,payload+1 , 2);
	datapoint.cmd_id=hex2int(hex_temp);

	strncpy( hex_temp,payload+3 , 2);
	datapoint.cmd_value_type=hex2int(hex_temp);

	strncpy( hex_temp,payload+5 , 4);
	datapoint.msg_id=hex2int(hex_temp);

	strncpy( hex_temp,payload+9 , 4);
	datapoint.cmd_value_len=hex2int(hex_temp);
	
	data=DLINKMQ_MALLOC(datapoint.cmd_value_len);
	if(data ==NULL){
		DLINKMQ_FREE(hex_temp);
		return DlinkMQ_ERROR_CODE_PARAM;
	}
	strncpy( data,payload+13 , datapoint.cmd_value_len );
	datapoint.cmd_value=data;
	

	DLINKMQ_FREE(hex_temp);
	DLINKMQ_FREE(data);
	pstRecvInfo->on_receive_datapoint(&datapoint);
#endif
	return ret;
}
we_int32 dlinkmq_stringify_payload(dlinkmq_datapoint *data, we_int8 *payload){
    	we_int32 ret = 0;
    	we_int8 *temp = DLINKMQ_MALLOC(5);
	//we_int32 cmd_value_len=strlen(data->cmd_value);
	
	if(temp == NULL || data->cmd_id>255 || data->msg_id>65535){
		DLINKMQ_FREE(temp);
		return DlinkMQ_ERROR_CODE_PARAM;
	}

	sprintf(payload, "#");

	sprintf(temp,"%02x",data->cmd_id);
	strcat(payload, temp);

	sprintf(temp,"%02x",data->cmd_value_type);
	strcat(payload, temp);

	sprintf(temp,"%04x",data->msg_id);
	strcat(payload, temp);

	sprintf(temp,"%04x",data->cmd_value_len);
	strcat(payload, temp);
	
	strcat(payload, data->cmd_value);
	strcat(payload, "\r\n");

	DLINKMQ_FREE(temp);
	mqtt_fmt_print("--dlinkmq_stringify_payload:%s",payload);
	return ret;
}
void cb_dlinkmq_publish(we_int32 err_code, MQTTMessage *message){
	dlinkmq_datapoint *datapoint;
	on_send_msg fun_cb;
	mqtt_fmt_print("---cb_dlinkmq_publish:%d",err_code);

	if(message && message->fun_cb && message->datapoint){
		datapoint=message->datapoint;
		fun_cb=message->fun_cb;

		DLINKMQ_FREE(message->payload);
		DLINKMQ_FREE(message);
		
		mqtt_cb_exec_with_data(fun_cb,err_code ,(void*)datapoint);
	}
	else {
		DLINKMQ_FREE(message->payload);
		DLINKMQ_FREE(message);
	}
}
we_int32 dlinkmq_publish(dlinkmq_datapoint *datapoint, on_send_msg fun_cb){
	we_int32 ret=0;

#if 1
	Client *pstClient = DlinkmqMgr_GetClient(g_pstDlinkmqMgr);
	St_DlinkmqMqttSubPub *pstMqttSubPub =  DlinkmqMgr_GetMqttSubPubInfo(g_pstDlinkmqMgr);
	MQTTMessage *message=NULL;
	we_int8 *payload=NULL;
	
 	if (pstClient == NULL || pstMqttSubPub == NULL)
	{
		return DlinkMQ_ERROR_CODE_FAIL;
	}
	
	if(!datapoint->cmd_value){
		mqtt_cb_exec_with_data(fun_cb,DlinkMQ_ERROR_CODE_PARAM ,(void*)datapoint);
		return DlinkMQ_ERROR_CODE_PARAM;
	}

	//payload=(we_int8*)DLINKMQ_MALLOC(datapoint->cmd_value_len+16);
	payload=(we_int8*)DLINKMQ_MALLOC(strlen(datapoint->cmd_value)+16);
	message=DLINKMQ_MALLOC(sizeof(MQTTMessage));
	if (payload==NULL || message==NULL)
	{
		return DlinkMQ_ERROR_CODE_PARAM;
	}

	ret=dlinkmq_stringify_payload(datapoint, payload);
	if(ret!=DlinkMQ_ERROR_CODE_SUCCESS){
		DLINKMQ_FREE(payload);
		DLINKMQ_FREE(message);
		return ret;
	}
	
	message->payload = payload; 
   	 message->payloadlen = strlen(payload);
   	message->qos = 1;
	message->id=datapoint->msg_id;
	message->datapoint=datapoint;
	message->fun_cb=fun_cb;
	
	
	MQTTAsyncPublish (pstClient, pstMqttSubPub->pcPubTopic, message,cb_dlinkmq_publish);
	
#endif

	return ret;
}


void cb_dlinkmq_gethostbyname(void *data){
	app_soc_get_host_by_name_ind_struct *dns_msg = NULL;
    dns_msg = (app_soc_get_host_by_name_ind_struct *)data;

	//mqtt_fmt_print("------cbGetHostByName");
}

static void dlinkmq_cb_gethostbyname(kal_int32 opid, kal_uint8 * addr,kal_uint8 addr_len){

#if 0
	char host[16];
	_snprintf(host,16, "%d.%d.%d.%d", addr[0],addr[1],addr[2],addr[3]);
	//mqtt_fmt_print("\n dlinkmq_cb_gethostbyname ~~~~~ opid:%d,IP len:%d,IP:%s\n",opid,addr_len,host);
	strcpy(g_dmq_client.server,host);
	g_dmq_client.mqttstatus=MQTT_STATUS_CONN;
	mqtt_service_start();

#endif
}

static int dlinkmq_cb_msg_gethostbyname(app_soc_get_host_by_name_ind_struct *dns_msg){
	kal_uint8 addr[4] = {0};
	kal_uint8 addr_len = 0;
	kal_int32 opid = dns_msg->request_id;

	mmi_frm_clear_protocol_event_handler(MSG_ID_APP_SOC_GET_HOST_BY_NAME_IND,NULL); 
	if(dns_msg->result == KAL_TRUE){
		dlinkmq_cb_gethostbyname(opid,dns_msg->addr,dns_msg->addr_len);
	}else{
		dlinkmq_cb_gethostbyname(opid,addr,addr_len);
	}
	return KAL_TRUE;
}

static void dlinkmq_gethostbyname(kal_int32 opid, kal_char * domain_name, kal_uint8 access_id){
	kal_uint8 addr[4] = {0};
	kal_uint8 addr_len = 0;
	int ret;
	kal_uint8 app_id = 0;
	kal_uint8 sim_id = 0;
	kal_uint32 data_account = 0;
	kal_uint32 account_id = 0;

	cbm_register_app_id(&app_id);
	data_account= cbm_encode_data_account_id(CBM_DEFAULT_ACCT_ID, (cbm_sim_id_enum)sim_id, 0, MMI_FALSE);
	account_id = cbm_encode_data_account_id(data_account, (cbm_sim_id_enum)sim_id, app_id, MMI_FALSE);

	
	ret = soc_gethostbyname(KAL_FALSE, 
                            MOD_MQTT, 
                            opid, 
                            domain_name, 
                            addr, 
                            &addr_len, 
                            0, 
                            account_id);

	//mqtt_fmt_print("\n MQTT_gethostbyname OPID:%d,domain:%s,access_id:%d,result:%d,account:%d\n",opid,domain_name,access_id,ret,account_id);
	if(ret == SOC_WOULDBLOCK){
		mmi_frm_set_protocol_event_handler(MSG_ID_APP_SOC_GET_HOST_BY_NAME_IND,dlinkmq_cb_msg_gethostbyname,KAL_FALSE);
		return;
	}
	 dlinkmq_cb_gethostbyname(opid,addr,addr_len);

}






void dlink_delay_ticks( kal_uint32 delay )
{
    kal_uint32 old_tick, new_tick;
    kal_get_time( &old_tick );

    while( 1 )
    {
        kal_get_time( &new_tick );
        if( new_tick - old_tick >= delay )
        {
            return;
        }
    }
}



static int cb_post_data(const char *json_data ,dlinkmq_ext_callback cb) {
    int ret = 0;
    cJSON *root;
    
    root = cJSON_Parse(json_data);
    if (root && cb) {
        int ret_size = cJSON_GetArraySize(root);
        if (ret_size >= 2) {
            ret=strcmp(cJSON_GetObjectItem(root,"msg")->valuestring,"success");
			if(ret==0){
				mqtt_cb_exec_with_data(cb, DlinkMQ_ERROR_CODE_SUCCESS, (void*)cJSON_GetObjectItem(root ,"url")->valuestring);
			}
			else
			{
				mqtt_cb_exec_with_data(cb, ret, (void*)cJSON_GetObjectItem(root,"msg")->valuestring);
			}
        } else
            ret = -1;
        
        cJSON_Delete(root);
    }
    return ret;
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
int char_in_string(char * p,char ch)
{
	char * q = p;
	while(* q != '\0')
	{
		if(ch == * q)
			return 0;

		q++;
	}
	return -1;
}
int http_post_data(char *hostname, int port, char *path, char *file_path, dlinkmq_ext_callback cb) {
	
    int ret = -1;
#if 0
    //int h=0;
	//char buf[800]={0};
    //char *tempb;
	char upload_head[] =
		"POST %s HTTP/1.1\r\n"
		"Host: %s:%d\r\n"
		"Connection: keep-alive\r\n"
		"Content-Type: multipart/form-data; boundary=%s\r\n"
		"Content-Length: %d\r\n\r\n"
		"--%s\r\n"
		"Content-Disposition: form-data; name=\"filename\"; filename=\"%s\"\r\n"
		"Content-Type: application/octet-stream;chartset=UTF-8\r\n\r\n";
	char upload_request[] = 
		"--%s\r\n"
		"Content-Disposition: form-data; name=\"filename\"; filename=\"%s\"\r\n"
		"Content-Type: application/octet-stream;chartset=UTF-8\r\n\r\n";
	char boundary[]="------------7788656end";
	char upload_end[50]={0};
	int content_length=0;


	//读文件
	char UnicodeName[520]={0};
	//WCHAR file_name[]=L"upload_txt.txt";
	
	// 有 '/' FS_Open方法会挂掉
	if(char_in_string(file_path, '/')==0){
		return DlinkMQ_ERROR_CODE_PARAM;
	}
	mmi_asc_to_ucs2(UnicodeName,file_path);

	if((file_h = FS_Open((const WCHAR *)UnicodeName , FS_READ_ONLY|FS_OPEN_SHARED)) >= 0)
	{
		ret=FS_GetFileSize(file_h,&soc_buf.buf_size);
		if(soc_buf.buf_size<=0){
			return DlinkMQ_ERROR_CODE_PARAM;
		}
	}
	cb_http_upload=cb;
	

	content_length += soc_buf.buf_size;
	content_length +=_snprintf(upload_end,50,"\r\n--%s--\r\n",boundary);
	memset(soc_buf.buf, 0, sizeof(soc_buf.buf));
	content_length +=_snprintf(soc_buf.buf,4096, upload_request, boundary, file_path);
	mqtt_fmt_print("---content_length:%d",content_length);

	_snprintf(soc_buf.buf,4096, upload_head, path, hostname, port, boundary, content_length, boundary, file_path);
	soc_buf.buf_size=strlen(soc_buf.buf);
	ret = soc_send(n_for_http.my_socket, soc_buf.buf, soc_buf.buf_size,0);
    if (ret < 0) {
		mqtt_fmt_print("--http_post_data-发送head失败:%d\n",ret);
        return DlinkMQ_ERROR_CODE_SUCCESS;
	}
	mqtt_fmt_print("--http_post_data-发送head-len:%d",ret);


	while(1){
		//dlink_delay_ticks(KAL_TICKS_1_SEC/5);
		memset(soc_buf.buf, 0, sizeof(soc_buf.buf));
		FS_Read(file_h, soc_buf.buf, 4096, &soc_buf.buf_size);
		if(soc_buf.buf_size==4096){
			ret = soc_send(n_for_http.my_socket, soc_buf.buf, soc_buf.buf_size,0);
			if (ret <= 0) {
				mqtt_fmt_print("--http_post_data-发送data失败:%d\n",ret);
				return DlinkMQ_ERROR_CODE_SUCCESS;
			}
			mqtt_fmt_print("--http_post_data-发送data-len:%d",ret);
		}
		else{

			ret = soc_send(n_for_http.my_socket, soc_buf.buf, soc_buf.buf_size,0);
			if (ret < 0) {
				mqtt_fmt_print("--http_post_data-发送data失败:%d\n",ret);
				return DlinkMQ_ERROR_CODE_SUCCESS;
			}
			mqtt_fmt_print("--http_post_data-发送data-len:%d",ret);
			FS_Close(file_h);
			break;
		}
	}
	
	ret = soc_send(n_for_http.my_socket, upload_end, strlen(upload_end),0);
	if (ret < 0) {
		mqtt_fmt_print("--http_post_data-发送boundary失败:%d\n",ret);
		return DlinkMQ_ERROR_CODE_SUCCESS;
	}
	mqtt_fmt_print("--http_post_data-发送boundary-len:%d",ret);


	//memset(buf,0,sizeof(buf));
	//while(1){
	//	dlink_delay_ticks(KAL_TICKS_1_SEC/3);
	//	h = soc_recv(n_for_http.my_socket, &buf, 600,0);
	//	if(h>0){
	//		tempb = strstr(buf, "\r\n\r\n");
	//		if (tempb) {
	//			mqtt_fmt_print("\n---data:%s",tempb);
	//			tempb += 4;
	//			ret = cb_post_data(tempb, cb);
	//		}
	//		else{
	//			mqtt_fmt_print("---读取soc_recv数据为空！\n");
	//			ret = 4;
	//		}
	//		break;
	//	}

	//}
#endif

    return DlinkMQ_ERROR_CODE_SUCCESS;
}
int http_send_buf(){
	int ret=0;

#if 0
	// mqtt/info
	if (g_dmq_client.mqttstatus==MQTT_STATUS_INIT && soc_buf.buf){
		ret = soc_send(n_for_http.my_socket, soc_buf.buf, soc_buf.buf_size,0);
		if(ret==SOC_NOTCONN){
			mqtt_service_init();
		}
		mqtt_fmt_print("---http_send_buf-len:%d",ret);
		if(ret>0){
			memset(soc_buf.buf, 0, sizeof(soc_buf.buf));
			soc_buf.buf_size=0;
		}
	}
	//upload/public
	else if(soc_buf.buf_size==0){
		ret=soc_send(g_upload_net.my_socket, "\r\n--------------7788656end--\r\n", strlen("\r\n--------------7788656end--\r\n"),0);
	}
	else{
		while(1){
			if( soc_buf.buf_size>0){
				ret = soc_send(g_upload_net.my_socket, soc_buf.buf, soc_buf.buf_size,0);
				if (ret <= 0) {
					mqtt_fmt_print("--http_send_buf-upload-发送data失败:%d\n",ret);
					if(ret == SOC_NOTCONN){
						mqtt_cb_exec_with_data(cb_http_upload, ret,NULL);
					}
					return ret;
				}
				mqtt_fmt_print("--http_send_buf-upload-发送data-len:%d\n",ret);
				memset(soc_buf.buf, 0, sizeof(soc_buf.buf));
				FS_Read(file_h, soc_buf.buf, 4096, &soc_buf.buf_size);
				if(soc_buf.buf_size==0){
					FS_Close(file_h);
					memset(soc_buf.buf, 0, sizeof(soc_buf.buf));
					soc_buf.buf_size=0;
					ret=soc_send(g_upload_net.my_socket, "\r\n--------------7788656end--\r\n", strlen("\r\n--------------7788656end--\r\n"),0);
					mqtt_fmt_print("--http_send_buf-upload-发送data-len:%d\n",ret);
					
					if(ret == SOC_NOTCONN){
						mqtt_cb_exec_with_data(cb_http_upload, ret,NULL);
					}

					return ret;
				}
			}
			else{
				ret=soc_send(g_upload_net.my_socket, "\r\n--------------7788656end--\r\n", strlen("\r\n--------------7788656end--\r\n"),0);
				mqtt_fmt_print("--http_send_buf-upload-发送data-len:%d",ret);
				
				if(ret == SOC_NOTCONN){
					mqtt_cb_exec_with_data(cb_http_upload, ret,NULL);
				}
				return ret;
			}
		}
	}
#endif
	return ret;
}
int http_recv_buf(){
	int ret=0;
	char *tempb;
#if 0
	if (g_dmq_client.mqttstatus==MQTT_STATUS_INIT){
		ret = soc_recv(n_for_http.my_socket, &soc_buf.buf, 600,0);
	}
	else{
		ret = soc_recv(g_upload_net.my_socket, &soc_buf.buf, 600,0);
	}
	if (ret > 0) {
		//取body
		tempb = strstr(soc_buf.buf, "\r\n\r\n");
		if (tempb) {
			mqtt_fmt_print("--http_recv_buf-data:%s",tempb);
			tempb += 4;

			if (g_dmq_client.mqttstatus==MQTT_STATUS_INIT){
				ret = pro_cb(tempb);
			}
			else{
				ret=cb_post_data(tempb,cb_http_upload);
			}
		}
		else{
			mqtt_fmt_print("---解析soc_recv数据为空");
			ret = 4;
		}
		memset(soc_buf.buf,0,sizeof(soc_buf.buf));
	}
	else{
		mqtt_fmt_print("---http_recv_buf-soc_recv读取数据为空:%d",ret);
	}

#endif

	return ret;
}

we_int32 dlinkmq_upload(char* file_path, dlinkmq_ext_callback cb)
{
    int ret= 0;
	char path[]="/oss/uploadPublic";
	char upload_head[] =
		"POST %s HTTP/1.1\r\n"
		"Host: %s:%d\r\n"
		"Connection: keep-alive\r\n"
		"Content-Type: multipart/form-data; boundary=%s\r\n"
		"Content-Length: %d\r\n\r\n"
		"--%s\r\n"
		"Content-Disposition: form-data; name=\"audio\"; filename=\"%s\"\r\n"
		"Content-Type: application/octet-stream;chartset=UTF-8\r\n\r\n";
	char upload_request[] = 
		"--%s\r\n"
		"Content-Disposition: form-data; name=\"audio\"; filename=\"%s\"\r\n"
		"Content-Type: application/octet-stream;chartset=UTF-8\r\n\r\n";
	char boundary[]="------------7788656end";
	char upload_end[50]={0};
	int content_length=0;


	//读文件
	char UnicodeName[520]={0};
	//WCHAR file_name[]=L"upload_txt.txt";

	// 有 '/' FS_Open方法会挂掉
	if(char_in_string(file_path, '/')==0){
		mqtt_fmt_print("\n---dlinkmq_upload-nofile");
		return DlinkMQ_ERROR_CODE_PARAM;
	}
	mmi_asc_to_ucs2(UnicodeName,file_path);

	if((file_h = FS_Open((const WCHAR *)UnicodeName , FS_READ_ONLY|FS_OPEN_SHARED)) >= 0)
	{
		ret=FS_GetFileSize(file_h,&soc_buf.buf_size);
		if(soc_buf.buf_size<=0){
			mqtt_fmt_print("\n---dlinkmq_upload-nofile");
			return DlinkMQ_ERROR_CODE_PARAM;
		}
	}
	cb_http_upload=cb;


	content_length += soc_buf.buf_size;
	content_length +=_snprintf(upload_end,50,"\r\n--%s--\r\n",boundary);
	memset(soc_buf.buf, 0, sizeof(soc_buf.buf));
	content_length +=_snprintf(soc_buf.buf,4096, upload_request, boundary, file_path);
	mqtt_fmt_print("\n---content_length:%d\n",content_length);

	_snprintf(soc_buf.buf,4096, upload_head, path, upload_host, upload_port, boundary, content_length, boundary, file_path);
	soc_buf.buf_size=strlen(soc_buf.buf);


	if (g_upload_net_connect==1)
	{
		http_send_buf();
	}
	else{
		NewNetwork(&g_upload_net ,dispatchEvents);
		ret=ConnectNetwork(&g_upload_net, upload_host, upload_port);
	}
	
    return ret;
}





