
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





//#define dotlink_host    "192.168.1.53"
//#define dotlink_host    "202.108.22.5"
#define dotlink_host    "139.224.11.153"
#define dotlink_port    4004
#define dotlink_path    "/mqtt/info"
#define upload_host    "139.224.11.153"
#define upload_port    8079

#define MQTT_TIMER_ENABLED         (1)
#if MQTT_TIMER_ENABLED

#define MQTT_STACK_TIMER_ID    (146)
#define MQTT_STACK_INIT_TIMER_ID    (147)
#define MQTT_STACK_RECONNECT_TIMER_ID    (148)

static stack_timer_struct g_mqtt_task_stack_timer;
static stack_timer_struct g_mqtt_task_stack_init_timer;
static stack_timer_struct g_mqtt_task_stack_reconnect_timer;

static int tryReconnect = 0;
int g_mqtt_timer_cnt = 0;
kal_int32 mqtt_lk_time=KAL_TICKS_30_SEC;

#endif


Network g_mqtt_net;
Network g_upload_net;
static int g_upload_net_connect = 0;
Network n_for_http;
Client  c;
static char pub_topic[55];
static char sub_topic[55];
dlinkmq_ext_callback cb_http_upload;
dlinkmq_on_receive on_receive_info;
dlinkmq_device_info device_info;


typedef int (*CALLBACK)(char *p);
static void mqtt_service_connect();
void cbconn(int result,void * data);
int MQTTClient_setup_with_pid_and_did(char* pid, char *did, char *productSecret);
int dlinkmq_upload(char* path, dlinkmq_ext_callback cb);
void cb_dlinkmq_gethostbyname(void *data);
void cb_dlinkmq_publish(we_int32 err_code, MQTTMessage *message);

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


void mqtt_cb_exec_with_data(MQTTAsyncCallbackFunc cb,int result,void *data){
	ilm_struct *msg = NULL;
	mqtt_cb_exec_msg *cb_msg = construct_local_para(sizeof(mqtt_cb_exec_msg), TD_CTRL);
	msg = allocate_ilm(stack_get_active_module_id());
	ASSERT(msg);
	ASSERT(cb_msg);
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


//static int mqtt_service_start()
int mqtt_service_start() 
{
	int ret=-1;

	NewNetwork(&g_mqtt_net ,dispatchEvents);
	ret=ConnectNetwork(&g_mqtt_net, c.server, c.port);
	mqtt_fmt_print("---mqtt_service_start:%d",ret);

	return ret;
}
void cb_mqtt_service_init(int result,void * data){
	int ret=-1;

	//测试上传接口
	//dlinkmq_upload("upload_png.png", NULL);

	/*while(ret){

		ret=MQTTClient_setup_with_pid_and_did(device_info.product_id, device_info.device_id, device_info.product_secret);
		mqtt_fmt_print("---MQTTClient_setup_with_pid_and_did:%d",ret);

		c.mqttstatus=MQTT_STATUS_CONN;
		mqtt_service_start();
	}*/


	ret=MQTTClient_setup_with_pid_and_did(device_info.product_id, device_info.device_id, device_info.product_secret);
	mqtt_fmt_print("---cb_mqtt_service_init:%d",ret);

}
static int mqtt_service_init() 
{
	int ret=-1;

	NewNetwork(&n_for_http ,dispatchEvents);
	ret=ConnectNetwork(&n_for_http, dotlink_host, dotlink_port);
	mqtt_fmt_print("---mqtt_service_init:%d",ret);

	return ret;
}


static unsigned char buf[256];
static unsigned char readbuf[256];

static void mqtt_service_connect()
{
 		int rc = 0;
		/*unsigned char *buf=mqtt_med_malloc(25000);
		unsigned char *readbuf=mqtt_med_malloc(25000);*/
		MQTTPacket_connectData data = MQTTPacket_connectData_initializer;


    	MQTTClient(&c, &g_mqtt_net, 100000, buf, 256, readbuf, 256);
    
    	
    	data.willFlag = 0;
    	data.MQTTVersion = 3;
    	data.clientID.cstring = c.clientid;
    	data.username.cstring = c.username;
    	data.password.cstring = c.password;
    	data.keepAliveInterval = 1* 60 *1000;
    	data.cleansession = 1;
    
    
    	MQTTAsyncConnect(&c, &data, cbconn);
		mqtt_fmt_print("---mqtt_service_connect-->socketId:%d\n",c.ipstack->my_socket);

}

static void mqtt_service_stop()
{
	//mqtt_fmt_print("\nservice STOP!\n");
}


static void mqtt_service_init_timer_start()
{
	stack_init_timer(&g_mqtt_task_stack_init_timer, "MQTT INIT Timer", MOD_MQTT);
	//stack_start_timer(&g_mqtt_task_stack_init_timer, MQTT_STACK_INIT_TIMER_ID, KAL_TICKS_500_MSEC);
	stack_start_timer(&g_mqtt_task_stack_init_timer, MQTT_STACK_INIT_TIMER_ID, KAL_TICKS_1_MIN*2);
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

static kal_bool dlinkmq_init_timmer()
{   

#if MQTT_TIMER_ENABLED
	
	mqtt_service_init_timer_start();

	stack_init_timer(&g_mqtt_task_stack_timer, "MQTT Timer", MOD_MQTT);
	stack_init_timer(&g_mqtt_task_stack_reconnect_timer,"MQTT Reconnect Timer",MOD_MQTT);
	
#endif

    return KAL_TRUE;
}



static kal_bool  mqtt_reset(task_indx_type task_indx)
{
    return KAL_TRUE;
}


void mqtt_init_framework(void)
{
	// todo ...
	
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

void mqtt_sock_notify_ind(void *inMsg)
{
    app_soc_notify_ind_struct *soc_notify = (app_soc_notify_ind_struct*) inMsg;

    if(soc_notify==NULL )
    {
        return;
    }
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
			if(c.mqttstatus==MQTT_STATUS_INIT){
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
			if(c.mqttstatus==MQTT_STATUS_INIT){
				mqtt_cb_exec(cb_mqtt_service_init,SUCCESS);
			}
			break;
						 }

		}

	}



	
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
		#if MQTT_TIMER_ENABLED
			{
				stack_timer_struct *pStackTimerInfo=(stack_timer_struct *)ilm_ptr->local_para_ptr;
				if (NULL != pStackTimerInfo)
				{
					//mqtt_fmt_print("\n----------MQTT timer: pStackTimerInfo->timer_indx=%d\n", pStackTimerInfo->timer_indx);
					if (MQTT_STACK_TIMER_ID == pStackTimerInfo->timer_indx)
					{
						/*g_mqtt_timer_cnt ++;
						if (100 > g_mqtt_timer_cnt)
						{
							stack_start_timer(&g_mqtt_task_stack_timer, MQTT_STACK_TIMER_ID, KAL_TICKS_1_SEC);
							//get_mqtt_test();
						}
						else
						{
							stack_stop_timer(&g_mqtt_task_stack_timer);
							mqtt_fmt_print("---MQTT timer: STOP-----");
						}*/

						//mqtt_service_set_keepalive_timer(1);
						//mqtt_fmt_print("\ntimer\n");
						cbYield(SUCCESS,NULL);
					} 
					else if (MQTT_STACK_INIT_TIMER_ID == pStackTimerInfo->timer_indx)
					{
						mqtt_fmt_print("\n--MQTT_STACK_INIT_TIMER_ID");
						//初始化
						mqtt_service_init();
						//直接连接mqtt测试
						//strcpy(c.clientid, "5020365161-000000000003");
						//strcpy(c.username, "EmJ4S6fQnt");
						//strcpy(c.password, "hjACCWFbT7");
						////strcpy(c.server,"120.26.77.175");
						//strcpy(c.server,"192.168.1.53");
						//c.port=1886;
						//c.mqttstatus=MQTT_STATUS_CONN;
						//mqtt_service_start();

						mqtt_service_init_timer_stop();

						//MQTTAsyncYield(&c, 1000, cbMQTTAsyncYield);
						//mqtt_service_set_keepalive_timer(1);
					}else if(MQTT_STACK_RECONNECT_TIMER_ID == pStackTimerInfo->timer_indx){
						tryReconnect = 0;
						mqtt_reconnect();
					}
					else
					{
						mqtt_fmt_print("---MQTT timer: other's timer id-----");
					}
				}
			}
		#endif
			break;
		case MSG_ID_MQTT_STATUS:
			
			{

				mqtt_fmt_print("---mqtt_dispatch()MSG_ID_MQTT_STATUS-----");
				mqtt_service_connect();
			}
			break;
		case MSG_ID_APP_SOC_NOTIFY_IND:
			mqtt_sock_notify_ind((void *)(ilm_ptr->local_para_ptr));
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
	g_mqtt_net.disconnect(&g_mqtt_net);
}

static int yieldCount = 0;
static kal_uint32 lastYieldTime = 0;
//mqtt_service_set_keepalive_timer
void cbYield(int result,void* data){
	kal_uint32 ticks,currentTime;
	int inteval;
	static int minimumYieldInteval = 1000;
	
	if(toStop){
		MQTTAsyncDisconnect(&c,NULL);
	//}else if(result == FAILURE){
		//mqtt_fmt_print("\n-----------------------------------yield FAIL!\n");
		//mqtt_service_connect();
	}else{
		
		kal_get_time(&ticks);
		currentTime = kal_ticks_to_milli_secs(ticks);
		inteval = currentTime - lastYieldTime;
		//mqtt_fmt_print("\nyield current:%d yield last:%d inteval:%d\n ",currentTime,lastYieldTime,inteval);
		
		if(inteval < minimumYieldInteval){
			//mqtt_fmt_print("\nskip yield!\n");
			mqtt_service_set_keepalive_timer(1);
			return;
		}
		yieldCount++;
		//mqtt_fmt_print("\nYIELD! count:%d\n",yieldCount);

		lastYieldTime = currentTime;
		MQTTAsyncYield(&c,100000,cbYield);
	}

}


void cbSubscribe(int result,void *data){
	mqtt_fmt_print("---cbSubscribe:%d",result);
	if(result == 0){

		toStop = 0;
		yieldCount = 0;
		MQTTAsyncYield(&c,100000,cbYield);
	}else{
		mqtt_service_stop();
	}
	
}

void handleMessage(MessageData* md){
	mqtt_fmt_print("--handleMessage--len:%d--payload:%s",md->message->payloadlen,(char*)md->message->payload);

	if(md->message->payloadlen>13 && md->message->payloadlen<256){
		dlinkmq_parse_payload(md->message->payload);
	}
	//dlinkmq_parse_payload("#510200fd000611");
}

static int connectRetryCount = 0;

static void mqtt_reconnect(void){
	mqtt_fmt_print("\n------connect retry count :%d\n",connectRetryCount);
	connectRetryCount++;
	g_mqtt_net.disconnect(&g_mqtt_net);	
	mqtt_service_start(); 
	//mqtt_service_connect();
	mqtt_service_set_reconnect_timer(1);
	tryReconnect = 1;
}

void dlinkmq_ping_timeout(void){
	mqtt_service_connect();
}


static void dlinkmq_gethostbyname(kal_int32 opid, kal_char * domain_name, kal_uint8 access_id);

void cbconn(int result,void *data){
	mqtt_fmt_print("---cbconn:%d",result);

	if(on_receive_info.on_receive_init){
		mqtt_cb_exec(on_receive_info.on_receive_init,result);
	}
	
	if(result==0){
		mqtt_service_set_reconnect_timer(0);
		connectRetryCount = 0;
		tryReconnect = 0;
		MQTTAsyncSubscribe(&c, sub_topic, QOS1,handleMessage,cbSubscribe);

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
    dlinkmq_datapoint datapoint;
	we_int8 *hex_temp=MQTTMalloc(5);
	we_int8 *data;


	if(payload[0]!='#' || strlen(payload)<13){
		MQTTFree(hex_temp);
		MQTTFree(data);
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
	
	data=MQTTMalloc(datapoint.cmd_value_len);
	strncpy( data,payload+13 , datapoint.cmd_value_len );
	datapoint.cmd_value=data;
	

	MQTTFree(hex_temp);
	MQTTFree(data);
	on_receive_info.on_receive_datapoint(&datapoint);
	return ret;
}
we_int32 dlinkmq_stringify_payload(dlinkmq_datapoint *data, we_int8 *payload){
    	we_int32 ret=0;
    	we_int8 temp[10]={0};
	//we_int32 cmd_value_len=strlen(data->cmd_value);
	
	if(data->cmd_id>255 || data->msg_id>65535){
		return DlinkMQ_ERROR_CODE_PARAM;
	}

	sprintf(payload, "#");

	_snprintf(temp,2,"%02x",data->cmd_id);
	strcat(payload, temp);

	_snprintf(temp,2,"%02x",data->cmd_value_type);
	strcat(payload, temp);

	_snprintf(temp,4,"%04x",data->msg_id);
	strcat(payload, temp);

	_snprintf(temp,4,"%04x",data->cmd_value_len);
	strcat(payload, temp);
	
	strcat(payload, data->cmd_value);
	strcat(payload, "\r\n");

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

		mqtt_med_free(message->payload);
		mqtt_med_free(message);
		
		mqtt_cb_exec_with_data(message->fun_cb,err_code ,(void*)message->datapoint);
	}
	else {
		mqtt_med_free(message->payload);
		mqtt_med_free(message);
	}
}
we_int32 dlinkmq_publish(dlinkmq_datapoint *datapoint, on_send_msg fun_cb){
	we_int32 ret=0;
	MQTTMessage *message=NULL;
	we_int8 *payload=NULL;

	
	if(!datapoint->cmd_value){
		mqtt_cb_exec_with_data(fun_cb,DlinkMQ_ERROR_CODE_PARAM ,(void*)datapoint);
		return DlinkMQ_ERROR_CODE_PARAM;
	}

	//payload=(we_int8*)mqtt_med_malloc(datapoint->cmd_value_len+16);
	payload=(we_int8*)mqtt_med_malloc(strlen(datapoint->cmd_value)+16);
	message=mqtt_med_malloc(sizeof(MQTTMessage));
	if (payload==NULL || message==NULL)
	{
		return DlinkMQ_ERROR_CODE_PARAM;
	}

	ret=dlinkmq_stringify_payload(datapoint, payload);
	if(ret!=DlinkMQ_ERROR_CODE_SUCCESS){
		mqtt_med_free(payload);
		mqtt_med_free(message);
		return ret;
	}
	
	message->payload = payload; 
   	 message->payloadlen = strlen(payload);
   	message->qos = 1;
	message->id=datapoint->msg_id;
	message->datapoint=datapoint;
	message->fun_cb=fun_cb;
	

	MQTTAsyncPublish (&c, pub_topic, message,cb_dlinkmq_publish);
	
	

	return ret;
}


void cb_dlinkmq_gethostbyname(void *data){
	app_soc_get_host_by_name_ind_struct *dns_msg = NULL;
    dns_msg = (app_soc_get_host_by_name_ind_struct *)data;

	//mqtt_fmt_print("------cbGetHostByName");
}

static void dlinkmq_cb_gethostbyname(kal_int32 opid, kal_uint8 * addr,kal_uint8 addr_len){
	char host[16];
	_snprintf(host,16, "%d.%d.%d.%d", addr[0],addr[1],addr[2],addr[3]);
	//mqtt_fmt_print("\n dlinkmq_cb_gethostbyname ~~~~~ opid:%d,IP len:%d,IP:%s\n",opid,addr_len,host);
	strcpy(c.server,host);
	c.mqttstatus=MQTT_STATUS_CONN;
	mqtt_service_start();
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
we_int32 dlinkmq_data_A_To_B(we_int8 *data, we_int32 indexA, we_int32 A, we_int32 B, we_int8 *out_data){
	we_int32 ret=-1;
	we_int32 dataIndex=0;
	we_int32 numA=0;
	we_int32 i;

	if (strlen(data)>30){
		return ret;
	}

	for(i=0; i<strlen(data); i++){
		if(numA==indexA && data[i]==B){
			return 0;
		}
		if(numA==indexA){
			out_data[dataIndex]=data[i];
			dataIndex++;

			if(i==strlen(data)-1){
				return 0;
			}
		}
		if(data[i]==A){
			numA++;
		}

	}
	return ret;
}

static int pro_cb(const char *json_data) {
    int ret = 0;
    cJSON *root;
    
    root = cJSON_Parse(json_data);
    if (root) {
        int ret_size = cJSON_GetArraySize(root);
        if (ret_size >= 4) {
			//char *host;
			//char *port;
			char temp[30]={0};
            strcpy(c.clientid, cJSON_GetObjectItem(root,"client")->valuestring);
            strcpy(c.username, cJSON_GetObjectItem(root,"username")->valuestring);
            strcpy(c.password, cJSON_GetObjectItem(root,"password")->valuestring);

			//编译出错
			//host = strtok(cJSON_GetObjectItem(root,"host")->valuestring,":");
			//port = strtok(NULL,":");
			//c.port=atoi(port);
			//dlinkmq_gethostbyname(0,host,0);

			dlinkmq_data_A_To_B(cJSON_GetObjectItem(root,"host")->valuestring,1,':',':',temp);
			c.port=atoi(temp);

			memset(temp, 0, sizeof(temp));
			dlinkmq_data_A_To_B(cJSON_GetObjectItem(root,"host")->valuestring,0,':',':',temp);
			dlinkmq_gethostbyname(0,temp,0);

			/*
			c.port = 1886;
			strcpy(c.server,"120.26.77.175");
			//c.port = 1885;
			//strcpy(c.server,"192.168.1.53");
			//strcpy(c.username, "liupf");
			//strcpy(c.password, "U2FsdGVkX181GNsTpzW0LO7aJ2jHgBA1Ah8FagYop1m7vKd6Y8hn1Ug7n9lu+sY0hSDJDM9FkDajZO3HegQ6ImQjZA8v7UVM");
			c.mqttstatus=MQTT_STATUS_CONN;
			mqtt_service_start();
			*/
        } else
            ret = -1;
        
        cJSON_Delete(root);
    }
    return ret;
}


int http_post_json(char *json_data, char *hostname, kal_uint16 port, char *path, CALLBACK cb) {
	
    int ret = -1;
	char buf[300]={0};
	char temp[50]={0};
	//int h;
	//char recvBuf[600]={0};
    //char *tempb;


    _snprintf(temp,50, "POST %s HTTP/1.1", path);
    strcat(buf, temp);
    strcat(buf, "\r\n");
    _snprintf(temp,50, "Host: %s:%d", hostname, port),
    strcat(buf, temp);
    strcat(buf, "\r\n");
    strcat(buf, "Accept: application/json\r\n");
    strcat(buf, "Content-Type: application/json\r\n");
    strcat(buf, "Content-Length: ");
    _snprintf(temp,50, "%lu", strlen(json_data)),
    strcat(buf, temp);
    strcat(buf, "\n\n");
    strcat(buf, json_data);
    
    ret = soc_send(n_for_http.my_socket, buf, strlen(buf),0);
    if (ret < 0) {
		
		mqtt_fmt_print("---http_post_json:%d",ret);
        if (ret==SOC_WOULDBLOCK)
		{
			soc_buf.buf_size=strlen(buf);
			_snprintf(soc_buf.buf,4096,buf);
        }
	if(ret==SOC_NOTCONN){
		mqtt_service_init();
	}
        return ret;
	}
	mqtt_fmt_print("---http_post_json:%d",ret);

	//dlink_delay_ticks(KAL_TICKS_1_SEC/2);
	//h = soc_recv(n_for_http.my_socket, &recvBuf, 600,0);
	//if (h > 0) {
	//	//取body
	//	tempb = strstr(recvBuf, "\r\n\r\n");
	//	if (tempb) {
	//		mqtt_fmt_print("\n---data:%s",tempb);
	//		mqtt_fmt_print("---data:%s",tempb);
	//		tempb += 4;
	//		ret = cb(tempb);
	//	}
	//	else{
	//		mqtt_fmt_print("解析soc_recv数据为空！\n");
	//		mqtt_fmt_print("---解析soc_recv数据为空");
	//		ret = 4;
	//	}
	//}
	//else{
	//	mqtt_fmt_print("soc_recv读取数据为空:%d\n",h);
	//	mqtt_fmt_print("---soc_recv读取数据为空:%d",h);
	//}

    return ret;
}
int MQTTClient_setup_with_pid_and_did(char* pid, char *did, char *productSecret)
{
	char json_data[150]={0};
	char sign[34]={0};
	MD5_CTX md5;
	int i;
	int ret=-1;
	unsigned char encrypt[150];
	unsigned char decrypt[16]={0};

	if (pid == NULL || did==NULL || productSecret==NULL){
		return 1;
	}

	//md5签名
	MD5Init(&md5);
	_snprintf(encrypt,150,"%sdid%spid%s%s",productSecret, did, pid ,productSecret);
	MD5Update(&md5,encrypt,strlen((char *)encrypt));
	MD5Final(&md5,decrypt);
	for(i=0;i<16;i++)
	{
		sprintf(sign,"%s%02x",sign,decrypt[i]);
	}
	_snprintf(json_data,150,"{\"did\": \"%s\", \"pid\": \"%s\", \"sign\": \"%s\"}", did, pid, sign);
	mqtt_fmt_print("\n---MQTTClient_setup-DATA:%s",json_data);

	ret = http_post_json(json_data, dotlink_host, dotlink_port, dotlink_path, pro_cb);


	return ret;
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


    return DlinkMQ_ERROR_CODE_SUCCESS;
}
int http_send_buf(){
	int ret=0;

	// mqtt/info
	if (c.mqttstatus==MQTT_STATUS_INIT && soc_buf.buf){
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
	return ret;
}
int http_recv_buf(){
	int ret=0;
	char *tempb;

	if (c.mqttstatus==MQTT_STATUS_INIT){
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

			if (c.mqttstatus==MQTT_STATUS_INIT){
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



we_int32 dlinkmq_init_device_info(dlinkmq_device_info *device_in, dlinkmq_on_receive *pFun_cb){
    	we_int32 ret=0;
	//we_int32 topic_len=0;
   
	if(!device_in || !pFun_cb){
        return DlinkMQ_ERROR_CODE_PARAM;
    	}
	device_info=*device_in;
	//topic_len=strlen(device_info.product_id)+strlen(device_info.device_id)+15;
	//pub_topic=mqtt_med_malloc(topic_len);
	//sub_topic=mqtt_med_malloc(topic_len);
	_snprintf(pub_topic,55,"device/%s/%s/set",device_info.product_id,device_info.device_id);
	_snprintf(sub_topic,55,"device/%s/%s/get",device_info.product_id,device_info.device_id);

    	on_receive_info.on_receive_datapoint = pFun_cb->on_receive_datapoint;
	on_receive_info.on_receive_init = pFun_cb->on_receive_init;
	

	c.mqttstatus=MQTT_STATUS_INIT;
	dlinkmq_init_timmer();
   
	mqtt_fmt_print("---dlinkmq_init_device_info");
    return ret;
}


//
//we_int32 dlinkmq_stop(){
//	we_int32 ret=0;
//	
//	if(c.buf && c.readbuf){
//		mqtt_med_free(c.buf);
//		mqtt_med_free(c.readbuf);
//	}
//
//
//	return ret;
//}




