
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


#ifdef __DRM_SUPPORT__
#include "drm_gprot.h"
#endif

#include "kal_debug.h"
#include "mdi_audio.h"
#include "xml_def.h"
#include "stdlib.h"
#include "OslMemory_Int.h"
#include "Med_utility.h"
#include "stdio.h"
#include <string.h>


#include "dlinkmq_api.h"
#include "dlinkmq_miaoxin.h"


void upload_data(void *data);
extern void dlink_delay_ticks( kal_uint32 delay );
void send_msg_from_datapoint_list();
void device_to_server_public(we_int32 cmd_id, we_void *data, we_int32 data_type);

typedef struct d_datapoint_list{

	dlinkmq_datapoint *pData;
	struct d_datapoint_list *tail;
	struct d_datapoint_list *next;

}datapoint_list;


kal_timerid uploadTimer;
miaoxin_settings settings_info;
we_int8 isSending = 0;
static we_int32 connect_status;

datapoint_list *g_datapoint_list = NULL;


void *miaoxin_med_malloc(size_t len){
	void * result = med_alloc_ext_mem(len);
	if(result){
		memset(result,0,len);
	}
	return result;
}
void miaoxin_med_free(void *buf){
	med_free_ext_mem(&buf);
}

void datapoint_list_add(dlinkmq_datapoint *datapoint){

	datapoint_list *dataList = NULL;
	
	if (datapoint == NULL) 
	{
		return;
	}

	if (g_datapoint_list == NULL) {
		g_datapoint_list = miaoxin_med_malloc(sizeof(datapoint_list));
		g_datapoint_list->pData = datapoint;
		g_datapoint_list->next = NULL;
		g_datapoint_list->tail = g_datapoint_list;
	} else {

		dataList = miaoxin_med_malloc(sizeof(datapoint_list));
		dataList->pData = datapoint;
		dataList->next = NULL;

		g_datapoint_list->tail ->next = dataList;
		g_datapoint_list->tail = dataList;
	
	
	}
}

void datapoint_list_remove_head()
{

	datapoint_list *tempList = g_datapoint_list;

	if (g_datapoint_list == NULL) {

		return;
	}

	if (tempList->next == NULL) {

		g_datapoint_list = NULL;

	} else {

		g_datapoint_list = g_datapoint_list->next;

	}

	miaoxin_med_free(tempList->pData->cmd_value);
	miaoxin_med_free(tempList->pData);
	miaoxin_med_free(tempList);
}


void cb_dlinkmq_upload(we_int32 err_code, void *data){
	we_int8 *file_info=NULL;
	printf("\n-------cb_dlinkmq_upload  --result:%d --data:%s",err_code,(we_int8*)data);
	kal_prompt_trace(MOD_MQTT,"---cb_dlinkmq_upload  --result:%d --data:%s",err_code,(we_int8*)data);

	//上传文件成功，上报音频文件信息
	if(err_code==DlinkMQ_ERROR_CODE_SUCCESS){
		file_info=miaoxin_med_malloc(strlen(data)+5);
		_snprintf(file_info,strlen(data)+5,"%s,%d",(we_int8*)data,settings_info.record_time);
		device_to_server_public(MIAOXIN_CMD_ID_UP_AUDIO,file_info, DATA_TYPE_STRING);
		miaoxin_med_free(file_info);
	}
}

void cb_miaoxin_aud_record_and_upload(mdi_result result, void* data){
	printf("\n-------cb_miaoxin_aud_record_and_upload  --result:%d --data:%s",result,(we_int8*)data);
	kal_prompt_trace(MOD_MQTT,"---cb_miaoxin_aud_record_and_upload  --result:%d  --data:%s",result, (we_int8*)data);

	if(result==MDI_AUDIO_SUCCESS){

		//上传音频文件到服务器
		if(dlinkmq_upload("upload\\upload_wav.wav", cb_dlinkmq_upload)==0){
			//上传接口调用成功
		}

	}
}

void miaoxin_audio_cmd(){
	WCHAR file_name[]=L"file_name_aud.amr";
	we_int32 ret=-1;
	we_int32 size_limit=20*1024;
	we_int32 time_limit=settings_info.record_time*KAL_TICKS_1_SEC;
	mdi_ext_callback cb= cb_miaoxin_aud_record_and_upload;
	/*
		录制音频操作，得到音频文件地址
	*/
	ret=mdi_audio_start_record_with_limit(file_name,MDI_FORMAT_AMR,0,cb,file_name,size_limit,time_limit);
	if(ret==MDI_AUDIO_SUCCESS){
		//录音操作成功
		device_to_server_public(MIAOXIN_CMD_ID_DOWN_AUDIO,(we_void*)DlinkMQ_ERROR_CODE_SUCCESS,DATA_TYPE_BOOL);
	}
	else{
		//录音开始失败时，根据返回结果ret作相应处理
		device_to_server_public(MIAOXIN_CMD_ID_DOWN_AUDIO,(we_void*)DlinkMQ_ERROR_CODE_DATA,DATA_TYPE_BOOL);
	}

}


void upload_location_data(void *timerData){
	//获取地理位置信息，上报数据
	device_to_server_public(MIAOXIN_CMD_ID_UP_LOCATION_RT,"220414,134652,A,22.571707,N,113.8613968,E,0.1,0.0,100,7,60,90,1000,50,0000,4,1,460,0,9360,4082,131,9360,4092,148,9360,4091,143,9360,4153,141",DATA_TYPE_STRING);
}

kal_timerid location_timer;
void close_upload_location_data(void *timerData){
	printf("\n-------close_upload_location_data ");
	kal_prompt_trace(MOD_MQTT,"---close_upload_location_data ");
	if (location_timer)
	{
		//kal_cancel_timer取消不了？
		//kal_cancel_timer(location_timer);

		kal_set_timer(location_timer, upload_location_data, NULL, 5*KAL_TICKS_1_SEC, 0);
	}
}

void miaoxin_location_cmd(){
	kal_timerid close_location_timer;
	close_location_timer=kal_create_timer("close_location_timer");
	kal_set_timer(close_location_timer, close_upload_location_data, NULL, settings_info.location_time*KAL_TICKS_1_SEC, 0);
	//kal_set_timer(close_location_timer, close_upload_location_data, NULL, 20*KAL_TICKS_1_SEC, 0);

	//实时定位timer，暂定时间间隔5S
	if(location_timer==NULL){
		location_timer=kal_create_timer("location_timer");
	}
	kal_set_timer(location_timer, upload_location_data, NULL, 5*KAL_TICKS_1_SEC, 1);

	//回复操作结果
	device_to_server_public(MIAOXIN_CMD_ID_DOWN_LOCATION_RT,(we_void*)DlinkMQ_ERROR_CODE_SUCCESS,DATA_TYPE_BOOL);
}


we_int32 miaoxin_Data_A_To_B(we_int8 *data, we_int32 indexA, we_int32 A, we_int32 B, we_int8 *out_data){
	we_int32 ret=-1;
	we_int32 dataIndex=0;
	we_int32 numA=0;
	we_int32 i;
	we_int32 data_len=strlen(data);

	for(i=0; i<data_len; i++){
		if(numA==indexA && data[i]==B){
			return 0;
		}
		if(numA==indexA){
			out_data[dataIndex]=data[i];
			dataIndex++;

			if(i==data_len-1){
				return 0;
			}
		}
		if(data[i]==A){
			numA++;
		}

	}
	return ret;
}
we_int32 count_str_same(we_int8 * p,we_int8 ch)
{
	we_int8 * q = p;
	we_int32 m = 0;
	while(* q != '\0')
	{
		if(ch == * q)
			m++;
		q++;
	}
	return m;
}
we_int32 miaoxin_setting_parse(we_int8 *cmd_value){
	we_int8 temp[40]={0};

	printf("\n-------miaoxin_setting_parse:%s",cmd_value);
	kal_prompt_trace(MOD_MQTT,"----miaoxin_setting_parse:%s",cmd_value);
	if(strlen(cmd_value)>40 || count_str_same(cmd_value,',')<4){
		return DlinkMQ_ERROR_CODE_PARAM;
	}
	if(miaoxin_Data_A_To_B(cmd_value,0,',',',',temp)==0){
		settings_info.record_time=atoi(temp);
	}

	memset(temp, 0, sizeof(temp));
	if(miaoxin_Data_A_To_B(cmd_value,1,',',',',temp)==0){
		settings_info.LED_time=atoi(temp);
	}

	memset(temp, 0, sizeof(temp));
	if(miaoxin_Data_A_To_B(cmd_value,2,',',',',temp)==0){
		settings_info.battery_val=atoi(temp);
	}

	memset(temp, 0, sizeof(temp));
	if(miaoxin_Data_A_To_B(cmd_value,3,',',',',temp)==0){
		settings_info.upload_time=atoi(temp);
	}

	memset(temp, 0, sizeof(temp));
	if(miaoxin_Data_A_To_B(cmd_value,4,',',',',temp)==0){
		settings_info.location_time=atoi(temp);
	}
	return 0;
}

void miaoxin_setting_cmd(we_int8 *cmd_value){
	WCHAR filename[]=L"miaoxin_settings.txt";
	FS_HANDLE file_h;
	we_int32 write_len;
	if(miaoxin_setting_parse(cmd_value)==0){
		//数据正常，写入文件
		FS_Delete((const WCHAR *)filename);
		if((file_h = FS_Open((const WCHAR *)filename , FS_READ_WRITE|FS_OPEN_SHARED|FS_CREATE)) >= 0){
			FS_Write(file_h,cmd_value,strlen(cmd_value),&write_len);
		}
		
		//修改上报数据的timer
		if(uploadTimer){
			kal_cancel_timer(uploadTimer);
			kal_set_timer(uploadTimer, upload_data, NULL, settings_info.upload_time*KAL_TICKS_1_SEC, 1);
		}

		device_to_server_public(MIAOXIN_CMD_ID_DOWN_SETTING,(we_void*)DlinkMQ_ERROR_CODE_SUCCESS,DATA_TYPE_BOOL);
	}
	else{
		device_to_server_public(MIAOXIN_CMD_ID_DOWN_SETTING,(we_void*)DlinkMQ_ERROR_CODE_DATA,DATA_TYPE_BOOL);
	}
}


void xml_app_read_start_element(void *no_used, const char *el, const char **attr, S32 error)
{
	printf("\n---xml_app_read_start_element --el:%s --attr:%s",el,attr);
}

void xml_app_read_end_element(void *data, const kal_char *el, kal_int32 error)
{

	printf("\n---xml_app_read_end_element --el:%s",el);
}

void xml_app_read_data_element(void *resv, const kal_char *el, const kal_char *data, kal_int32 len, kal_int32 error)
{
	printf("\n---xml_app_read_data_element --el:%s --data:%s",el, data);
	if(strcmp(el,"record_time")==0){
		settings_info.record_time=atoi(data);
	}
	else if(strcmp(el,"LED_time")==0){
		settings_info.LED_time=atoi(data);
	}
	else if(strcmp(el,"battery_val")==0){
		settings_info.battery_val=atoi(data);
	}
	else if(strcmp(el,"upload_time")==0){
		settings_info.upload_time=atoi(data);
	}
}

////把miaoxin_setting放入xml文件
//void miaoxin_setting_init(){
//	WCHAR filename[]=L"miaoxin_settings.xml";
//	XML_PARSER_STRUCT  xml_app_parser;
//	FS_HANDLE file_h;
//	we_int32 ret;
//	we_int8 xml_buf[]=
//		"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n"
//		"<set i=\"0\" version=\"0.0\" info=\"miaoxin_settings\">\r\n"
//		"<record_time>10</record_time>\r\n"
//		"<LED_time>300</LED_time>\r\n"
//		"<battery_val>15</battery_val>\r\n"
//		"<upload_time>120</upload_time>\r\n"
//		"</set>\r\n";
//
//	xml_new_parser(&xml_app_parser);
//	//xml_register_element_handler(&xml_app_parser,xml_app_read_start_element,xml_app_read_end_element);
//	xml_register_data_handler(&xml_app_parser, xml_app_read_data_element);
//	ret=xml_parse(&xml_app_parser,(kal_wchar*) filename);
//	printf("\n\n----xml_parse :%d-----\n",ret);
//
//	//xml文件不存在
//	if(ret==XML_RESULT_FILE_NOT_FOUND){
//		if((file_h = FS_Open((const WCHAR *)filename , FS_READ_WRITE|FS_OPEN_SHARED|FS_CREATE)) >= 0)
//		{
//			FS_Write(file_h,&xml_buf,strlen(xml_buf),&ret);
//		}
//	}
//	xml_stop_parse(&xml_app_parser);//释放用于分析的buffer等资源
//	xml_close_parser(&xml_app_parser);
//}


//把miaoxin_setting放入TXT文件
void miaoxin_setting_init(){
	WCHAR filename[]=L"miaoxin_settings.txt";
	we_int8 set_buf[]="10,300,15,30,300";	//这里为默认数据（record_time,LED_time,battery_val,upload_time,location_time）
	we_int8 file_data[40]={0};
	FS_HANDLE file_h;
	we_int32 read_len;
	we_int32 len;
	we_int32 ret;
	if((file_h = FS_Open((const WCHAR *)filename , FS_READ_WRITE|FS_OPEN_SHARED|FS_CREATE)) >= 0){
		FS_GetFileSize(file_h,&len);
		if(len>0 && len<=40){
			FS_Read(file_h, file_data, len, &read_len);
			if(read_len>0){
				if(miaoxin_setting_parse(file_data)==0){
					return;
				}
			}
		}

		ret=FS_Write(file_h,&set_buf,strlen(set_buf),&len);
		miaoxin_setting_parse(set_buf);
		
	}
}

void miaoxin_LED_cmd(void *timerData){
	device_to_server_public(MIAOXIN_CMD_ID_UP_LED, "",DATA_TYPE_BOOL);
}
//LED灯 timmer， 在LED等开启后调用此接口
void miaoxin_LED_cmd_timer_set(){
	kal_timerid LED_timer;
	LED_timer=kal_create_timer("LED_timer");
	kal_set_timer(LED_timer, miaoxin_LED_cmd, NULL, settings_info.LED_time*KAL_TICKS_1_SEC, 0);

}





void upload_data(void *timerData){
	we_int32 battery_val=80;   //设备电量值

	printf("\n---------------upload_data");
	kal_prompt_trace(MOD_MQTT,"---upload_data");
	

	//模拟上报-----//
	device_to_server_public(MIAOXIN_CMD_ID_UP_SLEEP,"300,100",DATA_TYPE_STRING);	//睡眠质量
	device_to_server_public(MIAOXIN_CMD_ID_UP_BATTERY,(we_void*)80,DATA_TYPE_NUMBER);	//电量值
	device_to_server_public(MIAOXIN_CMD_ID_UP_LOCATION_RT,"220414,134652,A,22.571707,N,113.8613968,E,0.1,0.0,100,7,60,90,1000,50,0000,4,1,460,0,9360,4082,131,9360,4092,148,9360,4091,143,9360,4153,141",DATA_TYPE_STRING);	//地理位置信息
	device_to_server_public(MIAOXIN_CMD_ID_UP_DIET,(we_void*)210,DATA_TYPE_NUMBER);	//饮食
	device_to_server_public(MIAOXIN_CMD_ID_UP_STEP,(we_void*)666,DATA_TYPE_NUMBER);	//运动步数
	device_to_server_public(MIAOXIN_CMD_ID_UP_TEMP,"36.5",DATA_TYPE_STRING);	//温度

	if(battery_val<=settings_info.battery_val){
		device_to_server_public(MIAOXIN_CMD_ID_UP_BATTERY,(we_void*)14,DATA_TYPE_NUMBER);
	}

}



//发送指令回调
void on_send_message(we_int32 err_code, dlinkmq_datapoint *datapoint){
	printf("\r\n------on_send_message-err_code:%d  --cmd_id:%d  --cmd_value:%s",err_code,datapoint->cmd_id,datapoint->cmd_value);
	kal_prompt_trace(MOD_MQTT,"---on_send_message:%d  --cmd_id:%d  --cmd_value:%s",err_code,datapoint->cmd_id,datapoint->cmd_value);

	isSending = 0;

	if(err_code==0){

		//处理消息队列
		datapoint_list_remove_head();

		//检查队列继续发送。
		send_msg_from_datapoint_list();

	}else{
		//发送失败
		send_msg_from_datapoint_list();
	}
}


void send_msg_from_datapoint_list(){

	//连接状态正常时发送
	if(connect_status==0){
		if (g_datapoint_list  != NULL && isSending == 0) {
			dlinkmq_publish(g_datapoint_list->pData, on_send_message);
			isSending = 1;
		}
		
	}
}



void device_to_server_public(we_int32 cmd_id, we_void *data, we_int32 data_type)
{
	dlinkmq_datapoint *datapoint = NULL;
	we_int32 ticks = 0;
	we_int32 dataLen = 0;

	if (data == NULL && data_type==DATA_TYPE_STRING) 
	{
		return;
	}
	

	datapoint=miaoxin_med_malloc(sizeof(dlinkmq_datapoint));

	if (datapoint == NULL) {
		return;
	}

	kal_get_time(&ticks);
	
	datapoint->msg_id=ticks;
	datapoint->cmd_id=cmd_id;

	if (data_type == DATA_TYPE_BOOL) {

		dataLen = 1; 
		datapoint->cmd_value=miaoxin_med_malloc(dataLen);
		sprintf(datapoint->cmd_value,"%01d",data);
	} else if (data_type == DATA_TYPE_NUMBER) {

		//dataLen = 4; 
		//datapoint->cmd_value=miaoxin_med_malloc(dataLen);
		//sprintf(datapoint->cmd_value,"%d",data);


		datapoint->cmd_value=miaoxin_med_malloc(10);
		dataLen=sprintf(datapoint->cmd_value,"%d",data);
	} else if  (data_type == DATA_TYPE_STRING) {
		dataLen = strlen(data) + 1; 
		datapoint->cmd_value=miaoxin_med_malloc(dataLen);
		sprintf(datapoint->cmd_value,"%s",data);
	}

	if (datapoint->cmd_value == NULL) {
		miaoxin_med_free(datapoint);
		return;
	}

	//memcpy(datapoint->cmd_value, data, dataLen);
	
	
	datapoint->cmd_value_len = dataLen;
	datapoint->cmd_value_type=data_type;
	
	datapoint_list_add(datapoint);


	//发送数据到服务器

	send_msg_from_datapoint_list();
}



//设备初始化回调
void on_receive_init(we_int32 err_code){
	//测试文件上传接口
	//dlinkmq_upload("upload\\upload_jpg.jpg", cb_dlinkmq_upload);

	printf("\n---------------on_receive_init:%d",err_code);
	kal_prompt_trace(MOD_MQTT,"---on_receive_init:%d",err_code);
	connect_status=err_code;

	//设备初始化成功后
	if(err_code==0 && uploadTimer==NULL){
		//初始化miaoxin_settings
		miaoxin_setting_init();

		//设timer定时上报数据
		//uploadTimer=kal_create_timer("uploadTimer");
		////kal_set_timer(uploadTimer, upload_data, NULL, settings_info.upload_time*KAL_TICKS_1_SEC, 1);
		//kal_set_timer(uploadTimer, upload_data, NULL, 5*KAL_TICKS_1_SEC, 1);

	}

}


//从服务器接收到指令监听
void on_receive_msg(dlinkmq_datapoint *datapoint){
	we_int32 result=0;
	if (datapoint->cmd_value)
	{
		printf("\n-------on_receive_msg  --cmd_id:%d  --cmd_value:%s",datapoint->cmd_id,datapoint->cmd_value);
		kal_prompt_trace(MOD_MQTT,"---on_receive_msg  --cmd_id:%d  --cmd_value:%s",datapoint->cmd_id,datapoint->cmd_value);
	}
	else{
		printf("\n-------on_receive_msg  --cmd_id:%d  --cmd_value:",datapoint->cmd_id);
		kal_prompt_trace(MOD_MQTT,"---on_receive_msg  --cmd_id:%d  --cmd_value:",datapoint->cmd_id);
	}

	//根据数据点数据操作设备
	switch(datapoint->cmd_id){
		
		case MIAOXIN_CMD_ID_DOWN_CALL:
			
			//设备CALL到手机，电话号码： datapoint->cmd_value
			/*
			*
			*/
			//根据拨打结果回复指令
			device_to_server_public(MIAOXIN_CMD_ID_DOWN_CALL,DlinkMQ_ERROR_CODE_SUCCESS,DATA_TYPE_BOOL);

			break;
		case MIAOXIN_CMD_ID_DOWN_AUDIO:

			//录制音频上传、回复设备操作结果
			//miaoxin_audio_cmd();
			device_to_server_public(MIAOXIN_CMD_ID_DOWN_AUDIO,(we_void*)DlinkMQ_ERROR_CODE_SUCCESS,DATA_TYPE_BOOL);

			dlinkmq_upload("upload\\upload_amr.amr", cb_dlinkmq_upload);
			//device_to_server_public(MIAOXIN_CMD_ID_UP_AUDIO,"57b4255070066a3821556b46,13",DATA_TYPE_STRING);

			break;
		case MIAOXIN_CMD_ID_DOWN_LOCATION_RT:
			
			//执行实时定位指令,回复操作结果
			miaoxin_location_cmd();


			break;
		case MIAOXIN_CMD_ID_DOWN_TEMP:

			//执行获取温度操作,获取成功回复数据，失败回复空或者再次获取成功后回复
			device_to_server_public(MIAOXIN_CMD_ID_DOWN_TEMP,"37.2",DATA_TYPE_STRING);  //温度度数:35度


			break;
		case MIAOXIN_CMD_ID_DOWN_STEP:

			//执行获取步数操作,获取成功回复数据，失败回复空或者再次获取成功后回复
			device_to_server_public(MIAOXIN_CMD_ID_DOWN_STEP,(we_void*)778,DATA_TYPE_NUMBER);	//运动步数:778步


			break;
		case MIAOXIN_CMD_ID_DOWN_SLEEP:

			//执行获取睡眠质量操作,获取成功回复数据，失败回复空或者再次获取成功后回复
			device_to_server_public(MIAOXIN_CMD_ID_DOWN_SLEEP,"400,60",DATA_TYPE_STRING);	//深度睡眠:300分钟,浅度睡眠:100分钟


			break;
		case MIAOXIN_CMD_ID_DOWN_DIET:

			//执行获取饮食数据操作,获取成功回复数据，失败回复空或者再次获取成功后回复
			device_to_server_public(MIAOXIN_CMD_ID_DOWN_DIET,(we_void*)233,DATA_TYPE_NUMBER);


			break;
		case MIAOXIN_CMD_ID_DOWN_BATTERY:

			//执行获取电量值操作,获取成功回复数据，失败回复空或者再次获取成功后回复
			device_to_server_public(MIAOXIN_CMD_ID_DOWN_BATTERY,"2,40",DATA_TYPE_STRING);	//未充电状态,剩余电量30%


			break;
		case MIAOXIN_CMD_ID_DOWN_LOCATION:

			//执行获取地理位置信息操作,获取成功回复数据，失败回复空或者再次获取成功后回复
			device_to_server_public(MIAOXIN_CMD_ID_DOWN_LOCATION,"220414,134652,A,30.457,N,114.422,E,0.1,0.0,100,7,60,90,1000,50,0000,4,1,460,0,9360,4082,131,9360,4092,148,9360,4091,143,9360,4153,141",DATA_TYPE_STRING);


			break;
		case MIAOXIN_CMD_ID_DOWN_SETTING:
			
			//数据设置指令,将数据存储,回复操作结果
			//....
			//
			miaoxin_setting_cmd(datapoint->cmd_value);


			break;
		case MIAOXIN_CMD_ID_DOWN_SETTING_LED:

			//数据设置指令,将数据存储,回复操作结果
			//....
			//
			device_to_server_public(MIAOXIN_CMD_ID_DOWN_SETTING_LED,(we_void*)DlinkMQ_ERROR_CODE_SUCCESS,DATA_TYPE_BOOL);

			break;
		case MIAOXIN_CMD_ID_DOWN_SETTING_BATTERY:

			//数据设置指令,将数据存储,回复操作结果
			//....
			//
			device_to_server_public(MIAOXIN_CMD_ID_DOWN_SETTING_BATTERY,(we_void*)DlinkMQ_ERROR_CODE_SUCCESS,DATA_TYPE_BOOL);

			break;
		case MIAOXIN_CMD_ID_DOWN_SETTING_UPLOAD:

			//数据设置指令,将数据存储,回复操作结果
			//....
			//
			device_to_server_public(MIAOXIN_CMD_ID_DOWN_SETTING_UPLOAD,(we_void*)DlinkMQ_ERROR_CODE_SUCCESS,DATA_TYPE_BOOL);

			break;
		case MIAOXIN_CMD_ID_DOWN_SETTING_STEP:

			//数据设置指令,将数据存储,回复操作结果
			//....
			//
			device_to_server_public(MIAOXIN_CMD_ID_DOWN_SETTING_STEP,(we_void*)DlinkMQ_ERROR_CODE_SUCCESS,DATA_TYPE_BOOL);

			break;

		default:
			break;

	}

	


}




extern void dlinkmq_run(ilm_struct *ilm_ptr);

//task 入口函数
static void mqtt_main(task_entry_struct *task_entry_ptr)
{
    	ilm_struct current_ilm;
    	kal_uint32 my_index;
	dlinkmq_device_info device_info;
	dlinkmq_on_receive fun_cb;
	we_int32 ret=0;
	kal_prompt_trace(MOD_MQTT,"---mqtt_main");

    	kal_get_my_task_index(&my_index);
	stack_set_active_module_id(my_index, MOD_MQTT);

	strcpy(device_info.device_id,"222222");
	strcpy(device_info.product_id,"b1e57467c92140e299022deb808cdd24");
	strcpy(device_info.product_secret,"2c15e161b5064d32ba6a6f664fbcde15");
	//接受消息监听方法
	fun_cb.on_receive_datapoint=on_receive_msg;
	//设备初始化回调
	fun_cb.on_receive_init=on_receive_init;
	
	//初始化设备接口
	ret=dlinkmq_init_device_info(&device_info,  &fun_cb);
	kal_prompt_trace(MOD_MQTT,"---dlinkmq_init_device_info:%d",ret);
	

    while (1)
    {
        receive_msg_ext_q_for_stack(task_info_g[task_entry_ptr->task_indx].task_ext_qid, &current_ilm);
        stack_set_active_module_id(my_index, current_ilm.dest_mod_id);
		
		//SDK运行主函数
		dlinkmq_run(&current_ilm);
		kal_prompt_trace(MOD_MQTT,"---mqtt_run");
		
		free_ilm(&current_ilm);

/*
		//消息队列有datapoint，发送消息
		if (is_publish_datapoint_list==0 && datapoint_list_info.datapoint_num>0)
		{
			printf("\n------datapoint_list_info.datapoint_num:%d  ",datapoint_list_info.datapoint_num);
			kal_prompt_trace(MOD_MQTT,"---datapoint_list_info.datapoint_num:%d  ",datapoint_list_info.datapoint_num);
			is_publish_datapoint_list=1;
			publish_datapoint_list();
		}

		*/

    }
	
}
kal_bool mqtt_create(comptask_handler_struct **handle)
{
    static const comptask_handler_struct ms_handler_info = 
    {
        mqtt_main,  /* task entry function */
        NULL,       /* task initialization function */
        NULL,           /* task configuration function */
        NULL,      /* task reset handler */
        NULL,           /* task termination handler */
    };

	kal_prompt_trace(MOD_MQTT,"---mqtt_create");

    *handle = (comptask_handler_struct*) & ms_handler_info;
    return KAL_TRUE;
}
