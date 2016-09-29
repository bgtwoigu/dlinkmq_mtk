#include "sci_types.h"
#include "sci_api.h"
#include "sci_log.h"
#include "os_api.h"
#include "window_parse.h"
#include "Datatype.h"
#include "mmk_kbd.h"
#include "mmi_position.h"
#include "mmi_appmsg.h"
#include "mmipub.h"
#include "mmiphone_export.h"
#include "IN_Message.h"
#include "socket_api.h"
#include "socket_types.h"

#include "mqtt_type_priv.h"
#include "MQTTMTK.h"
#include "MQTTClient.h"
#include "MQTTcJSON.h"
#include "MQTTMd5.h"
#include "MQTTTimer.h"
#include "MQTTTrace.h"

#include "dlinkmq_utils.h"
#include "dlinkmq_api.h"
#include "dlinkmq_mgr.h"

BLOCK_ID g_dlinkmq_task_id; 

static uint8 g_mqtt_task_stack_timer;
static uint8 g_mqtt_task_stack_init_timer;
static uint8 g_mqtt_task_stack_reconnect_timer;
static uint8 g_mqtt_task_stack_ping_timer;
static uint8 g_mqtt_task_stack_http_connect_timer;
static uint8 g_mqtt_task_stack_upload_connect_timer;
static uint8 g_mqtt_task_stack_client_connect_timer;

//static int tryReconnect = 0;


Network g_upload_net;
static int g_upload_net_connect = 0;



typedef int (*CALLBACK)(char *p);
void cbconn(int result,void * data);
int dlinkmq_upload(char* path, dlinkmq_ext_callback cb);
void cb_dlinkmq_gethostbyname(void *data);
void cb_dlinkmq_publish(we_int32 err_code, MQTTMessage *message);
we_int32 dlinkmq_parse_payload(we_int8 *payload);
void cbYield(int result,void *data);
static void mqtt_reconnect(void);
static int dlinkmq_cb_msg_gethostbyname(app_soc_get_host_by_name_ind_struct *dns_msg);

typedef struct mqtt_cb_exec_msg
{
	MQTTAsyncCallbackFunc cb;
	int result;
	void * data;
}mqtt_cb_exec_msg;







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


void mqtt_cb_exec_with_data(MQTTAsyncCallbackFunc cb,int result,void *data){
	mqtt_cb_exec_msg *cb_msg = NULL;
	if(cb == NULL)
	{
		return;	
	}

	SCI_CREATE_SIGNAL(cb_msg,DLINKMQ_SIG_CB_EXEC,sizeof(mqtt_cb_exec_msg),g_dlinkmq_task_id)
	cb_msg->cb = cb;
	cb_msg->result = result;
	cb_msg->data = data;
	SCI_SEND_SIGNAL(cb_msg,g_dlinkmq_task_id); 
}


void mqtt_cb_exec(MQTTAsyncCallbackFunc cb,int result){
	 mqtt_cb_exec_with_data(cb,result,NULL);
}

void mqtt_app_sendmsg_to_task(int msg_id)
{

	mqtt_cb_exec_msg *msg = NULL;

	SCI_CREATE_SIGNAL(msg,msg_id,sizeof(mqtt_cb_exec_msg),g_dlinkmq_task_id)
	SCI_SEND_SIGNAL(msg,g_dlinkmq_task_id); 
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
	
	mqtt_fmt_print("---dlinkmq_init_handle:%d",ret);
	
}
//static int mqtt_service_start()
int mqtt_service_start() 
{
	int ret=-1;

	mqtt_fmt_print("---mqtt_service_start mqtt_service_start");
	DlinkmqMsg_PostMsg(g_pstDlinkmqMsgHandle, E_MQ_MSG_MODULEID_MQTT, E_MQ_MSG_EVENTID_MQTT_INIT, 0, 0, 0, 0, NULL, NULL);

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



void dlinkmq_timer_cb( uint8  timer_id, uint32 param)
{
	St_DlinkmqMqtt *pstMqtt = DlinkmqMgr_GetMqtt(g_pstDlinkmqMgr);
	if (g_mqtt_task_stack_timer == timer_id)
	{
		if (pstMqtt && pstMqtt->mqttStatus == MQTT_STATUS_CONN)
		{
			cbYield(SUCCESS,NULL);
		} else if (pstMqtt)
		{

			mqtt_fmt_print("\n--------dlinkmq_run--MQTT timer: MQTT_STACK_TIMER_ID status=%d\n", pstMqtt->mqttStatus);
		}
						
	} 
	else if (g_mqtt_task_stack_init_timer == timer_id)
	{
		mqtt_fmt_print("\n--MQTT_STACK_INIT_TIMER_ID");
		//初始化
		dlinkmq_service_init();
		
		mqtt_service_init_timer_stop();

		//MQTTAsyncYield(&c, 1000, cbMQTTAsyncYield);
		//mqtt_service_set_keepalive_timer(1);
	}else if(g_mqtt_task_stack_reconnect_timer == timer_id){

		mqtt_fmt_print("\n--MQTT_STACK_RECONNECT_TIMER_ID start");

		//tryReconnect = 0;
		mqtt_reconnect();
	} else if (g_mqtt_task_stack_http_connect_timer == timer_id)
	{
			
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
	else if (g_mqtt_task_stack_client_connect_timer == timer_id)
	{
		Client *pstClient = DlinkmqMgr_GetClient(g_pstDlinkmqMgr);

		if(pstClient && pstClient->fnReadTimeout)
		{
			mqtt_fmt_print("---mqtt client connect timeout, call readtimeout-----");
			pstClient->fnReadTimeout(0, NULL);
		}

		mqtt_fmt_print("---mqtt client connect timeout-----");
							
	}
	else if (g_mqtt_task_stack_ping_timer == timer_id)
	{
		Client *pstClient = DlinkmqMgr_GetClient(g_pstDlinkmqMgr);

		if(pstClient && pstClient->fnPingTimeout)
		{
			mqtt_fmt_print("---mqtt ping connect timeout, call ping timeout-----");
			pstClient->fnPingTimeout(0, NULL);
		}
		mqtt_fmt_print("---mqtt ping connect timeout-----");
	}
	else if (g_mqtt_task_stack_upload_connect_timer == timer_id) {

		//重连UPLOAD
						
		mqtt_fmt_print("---MQTT_STACK_UPLOAD&MQTT_STACK_UPLOAD_CONNECT_TIMEROUT_ID upload timeout-----");
		DlinkmqMsg_PostMsg(g_pstDlinkmqMsgHandle, E_MQ_MSG_MODULEID_UPLOAD, E_MQ_MSG_EVENTID_NEW_UPLOAD, 0, 0, 0, 0, NULL, NULL);
						
	}		
	else
	{
		mqtt_fmt_print("---MQTT timer: other's timer id-----");
	}
}


static we_void dlinkmq_init_timmer()
{   
	
	mqtt_service_init_timer_start();

	g_mqtt_task_stack_timer = MMK_CreateTimer(20000, 0);
	g_mqtt_task_stack_reconnect_timer = MMK_CreateTimer(20000, 0);
	g_mqtt_task_stack_http_connect_timer = MMK_CreateTimer(20000, 0);
	g_mqtt_task_stack_client_connect_timer = MMK_CreateTimer(20000, 0);
	g_mqtt_task_stack_ping_timer = MMK_CreateTimer(20000, 0);
	g_mqtt_task_stack_upload_connect_timer = MMK_CreateTimer(20000, 0);
	
}

we_void dlinkmq_httpconn_timer(we_uint8 start)
{
	mqtt_fmt_print("---mqtt dlinkmq_httpconn_timer  sart = %d", start);
	if(start){

		mqtt_fmt_print("---mqtt dlinkmq_httpconn_timer  stack_start_timer");
		//stack_start_timer(&g_mqtt_task_stack_http_connect_timer, MQTT_STACK_HTTP_CONNECT_TIMEROUT_ID, KAL_TICKS_1_MIN);
		MMK_StartTimerCallback(g_mqtt_task_stack_http_connect_timer, 60*1000, dlinkmq_timer_cb, NULL, 0);
	}else{

		mqtt_fmt_print("---mqtt dlinkmq_httpconn_timer stack_stop_timer");
		MMK_StopTimer(g_mqtt_task_stack_http_connect_timer);
	}
}

we_void dlinkmq_upload_conn_timer(we_uint8 start)
{
	mqtt_fmt_print("---upload dlinkmq_upload_conn_timer  sart = %d", start);
	if(start){

		mqtt_fmt_print("---upload dlinkmq_upload_conn_timer   stack_start_timer");
		//stack_start_timer(&g_mqtt_task_stack_upload_connect_timer, MQTT_STACK_UPLOAD_CONNECT_TIMEROUT_ID, KAL_TICKS_1_MIN);
		MMK_StartTimerCallback(g_mqtt_task_stack_upload_connect_timer, 60*1000, dlinkmq_timer_cb, NULL, 0);
	}else{

		mqtt_fmt_print("---upload dlinkmq_upload_conn_timer  stack_stop_timer");
		MMK_StopTimer(g_mqtt_task_stack_upload_connect_timer);
	}
}

we_void dlinkmq_client_reconn_timer(we_uint8 start)
{
	mqtt_fmt_print("---mqtt dlinkmq_client_reconn_timer  sart = %d", start);
	if(start){

		mqtt_fmt_print("---mqtt dlinkmq_client_reconn_timer  stack_start_timer");
		//stack_start_timer(&g_mqtt_task_stack_client_connect_timer, MQTT_STACK_CLIENT_CONNECT_TIMEROUT_ID, KAL_TICKS_1_MIN);
		MMK_StartTimerCallback(g_mqtt_task_stack_client_connect_timer, 60*1000, dlinkmq_timer_cb, NULL, 0);
	}else{

		mqtt_fmt_print("---mqtt dlinkmq_client_reconn_timer stack_stop_timer");
		MMK_StopTimer(g_mqtt_task_stack_client_connect_timer);
	}
}

we_void dlinkmq_ping_conn_timer(we_uint8 start, kal_uint32 in_time)
{
	mqtt_fmt_print("---mqtt dlinkmq_ping_timer  sart = %d", start);
	if(start){

		mqtt_fmt_print("---mqtt dlinkmq_ping_timer  stack_start_timer");
		//stack_start_timer(&g_mqtt_task_stack_ping_timer, MQTT_STACK_PING_CONNECT_TIMEROUT_ID, in_time);
		MMK_StartTimerCallback(g_mqtt_task_stack_ping_timer, in_time, dlinkmq_timer_cb, NULL, 0);
	}else{

		mqtt_fmt_print("---mqtt dlinkmq_ping_timer stack_stop_timer");
		MMK_StopTimer(g_mqtt_task_stack_ping_timer);
	}
}



//stack_timer_status_type dlinkmq_get_httpconn_timer_status()
//{
//	return g_mqtt_task_stack_http_connect_timer.timer_status;
//}
BOOLEAN dlinkmq_get_httpconn_timer_status()
{
	return MMK_IsTimerActive(g_mqtt_task_stack_http_connect_timer);
}

static void mqtt_service_stop()
{
	//mqtt_fmt_print("\nservice STOP!\n");
}

static void mqtt_service_init_timer_start()
{
	mqtt_fmt_print("-----------mqtt_service_init_timer_start");
	//stack_start_timer(&g_mqtt_task_stack_init_timer, MQTT_STACK_INIT_TIMER_ID, KAL_TICKS_30_SEC);
	g_mqtt_task_stack_init_timer = MMK_CreateTimer(20000, 0);
	MMK_StartTimerCallback(g_mqtt_task_stack_init_timer, 20000, dlinkmq_timer_cb, NULL, 0);
}


static void mqtt_service_init_timer_stop()
{
	MMK_StopTimer(g_mqtt_task_stack_init_timer);
}

static void mqtt_service_set_keepalive_timer(kal_uint8 start)
{
	if(start)
	{
		MMK_StartTimerCallback(g_mqtt_task_stack_timer, 200, dlinkmq_timer_cb, NULL, 0);
	}
	else
	{
		MMK_StopTimer(g_mqtt_task_stack_timer);
	}
}

static void mqtt_service_set_reconnect_timer(kal_uint8 flag){
	if(flag){
		MMK_StartTimerCallback(g_mqtt_task_stack_reconnect_timer, 60*1000, dlinkmq_timer_cb, NULL, 0);
	}else{
		MMK_StopTimer(g_mqtt_task_stack_reconnect_timer);
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


static void cbMQTTAsyncYield(int result)
{
}





void dlinkmq_sock_notify_ind(void *inMsg)
{
    	xSignalHeaderRec *soc_notify = (xSignalHeaderRec*) inMsg;
	E_DlinkmqMsgModuleId moduleId = E_MQ_MSG_MODULEID_BASE; 

    if(soc_notify == NULL)
    {
        return;
    }

	moduleId = DlinkmqMgr_GetEventIdBySockId(g_pstDlinkmqMgr, ((SOCKET_CONNECT_EVENT_IND_SIG_T*)soc_notify) ->socket_id);


	DlinkmqMsg_SendMsg(g_pstDlinkmqMsgHandle, moduleId, E_MQ_MSG_EVENTID_SOC_NOTIFY_IND, 0, 0, 0, 0, (we_void*)inMsg, NULL);
	
}
void dlinkmq_run(void *msg)
{
	xSignalHeaderRec *psig;
	if(msg == NULL)
	{
		return;
	}
	psig = (xSignalHeaderRec *)msg;

	SCI_TRACE_ID(TRACE_INFO,"--dlinkmq_run--psig->SignalCode:%d",(uint8*)"",psig->SignalCode);
    switch(psig->SignalCode)   
    {  
        case SOCKET_READ_EVENT_IND:
		case SOCKET_READ_BUFFER_STATUS_EVENT_IND:
		case SOCKET_WRITE_EVENT_IND:
		case SOCKET_CONNECT_EVENT_IND:
		case SOCKET_CONNECTION_CLOSE_EVENT_IND:
		case SOCKET_ACCEPT_EVENT_IND:
		case SOCKET_FULL_CLOSED_EVENT_IND:
		case SOCKET_ASYNC_GETHOSTBYNAME_CNF:
			dlinkmq_sock_notify_ind((void *)psig);	
			
			break;  

		case DLINKMQ_SIG_CB_EXEC:
			mqtt_cb_exec_msg *msg = (mqtt_cb_exec_msg *)psig;
			MQTTAsyncCallbackFunc cb;
			int result;
			void *data;
			St_DlinkmqMqtt *pstMqtt = DlinkmqMgr_GetMqtt(g_pstDlinkmqMgr);

			if (msg != NULL){
				cb = msg->cb;
				result = msg->result;
				data = msg->data;
				if(cb){

					if (pstMqtt 
						&&(pstMqtt->mqttStatus == MQTT_STATUS_RECONN
						|| pstMqtt->mqttStatus == MQTT_STATUS_DESTROY))
					{
						if (cb != pstMqtt->pFnReconn)
						{
							return;
						}

					}
					cb(result,data);
				}
			}
			
		default:  
					
			break;
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
	mqtt_fmt_print("\n--cbYield-result:%d",result);
	
	if(toStop)
	{
		MQTTAsyncDisconnect(pstClient,NULL);
	}
	else if(result == FAILURE){
		dlinkmq_service_init();
	}
	else
	{
		
		kal_get_time(&ticks);
		currentTime = kal_ticks_to_milli_secs(ticks);
		inteval = currentTime - lastYieldTime;
		mqtt_fmt_print("\n---mqtt yield current:%d yield last:%d inteval:%d\n ",currentTime,lastYieldTime,inteval);
		
		if(inteval < minimumYieldInteval){
			mqtt_fmt_print("\n---mqtt skip yield!\n");
			mqtt_service_set_keepalive_timer(0);
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


static void mqtt_reconnect(void){
	

	St_DlinkmqMqtt *pstMqtt = DlinkmqMgr_GetMqtt(g_pstDlinkmqMgr);

	mqtt_fmt_print("\n---mqtt connect retry");

	if (pstMqtt && pstMqtt->pstNetWork)
	{
		//pstMqtt->pstNetWork->disconnect(pstMqtt->pstNetWork);	
		mqtt_service_start(); 
		//mqtt_service_connect();
		mqtt_service_set_reconnect_timer(1);
		//tryReconnect = 1;
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
		//connectRetryCount = 0;
		//tryReconnect = 0;
		MQTTAsyncSubscribe(pstClient, pstMqttSubPub->pcSubTopic, QOS1,handleMessage,cbSubscribe);

	}else {
	
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

we_int32 dlinkmq_upload(char* file_path, dlinkmq_ext_callback cb)
{
    	int ret= DlinkMQ_ERROR_CODE_FAIL;
	St_DlinkmqUpload*pstUpload = DlinkmqMgr_GetUpload(g_pstDlinkmqMgr);
	
	if(!file_path || !&cb || !pstUpload)
	{
		return ret;
	}
	
	ret = DlinkmqUpload_AddUploadParams((we_handle)pstUpload, file_path, cb);
	if(ret != DlinkMQ_ERROR_CODE_SUCCESS){
		return ret;
	}
	
	DlinkmqMsg_PostMsg(g_pstDlinkmqMsgHandle, E_MQ_MSG_MODULEID_UPLOAD, E_MQ_MSG_EVENTID_NEW_UPLOAD, 0, 0, 0, 0, NULL, NULL);
	
    	return ret;
}





