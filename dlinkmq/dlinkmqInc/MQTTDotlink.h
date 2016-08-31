

#if !defined(__MQTT_Dotlink_H__)
#define __MQTT_Dotlink_H__

#include "MQTTcJSON.h"
#include "MQTTMd5.h"
#include "MQTTMTK.h"
#include "MMIDataType.h"
#include "Soc_api.h"
#include "Custom_mmi_default_value.h"
#include "App_datetime.h"
#include "App2soc_struct.h"
#include "OslMemory_Int.h"
#include "In.h"


 typedef  struct PRO_info{
	char client_id[200];
	char username[200];
	char password[200];
	char device_id[200];
	char host[200];
}PRO_info ;

/*
code 应用id（pid）;
order_id 发送这条指令的唯一id，可以和timestamp类似;
did 设备id;
data_points 指令数组的JSON结构体;
result 操作结果。0为成功,-1为失败;
resp 操作描述。比如"操作成功";
timestamp 时间戳;
*/
 typedef  struct payload_info{
	char code[30];
	long order_id;
	long did;
	//cJSON *data_points;
	int result;
	char resp[128];
	long timestamp;
}payload_info ;

/*
key 指令类型;
value 指令内容。指令类容非数组时，不会再解析value，直接使用value;
valueLen 指令内容长度。指令内容是语音文件编码时可能会用到;
cJSON_value 指令内容的JSON结构体;
*/
typedef  struct data_point_info{
	char key[100];
	char value[25000];
	int valueLen;
	cJSON *cJSON_value;
} data_point_info;

/*
SOS指令内容;
*/
 typedef  struct data_SOS_info{
	long SOS1;
	long SOS2;
	long SOS3;
}data_SOS_info ;

/*
IP指令内容;
*/
 typedef  struct data_IP_info{
	char IP[30];
	int port;
}data_IP_info ;

/*
SILENCETIME指令内容;
*/
 typedef  struct data_SILENCETIME_info{
	long time1;
	long time2;
	long time3;
	long time4;
} data_SILENCETIME_info;

/*
REMIND指令内容;
*/
 typedef  struct data_REMIND_info{
	long time1;
	long time2;
	long time3;
} data_REMIND_info;

/*
PHB指令内容;
*/
typedef  struct data_PHB_info{
	char name1[20];
	long phone1;
	char name2[20];
	long phone2;
	char name3[20];
	long phone3;
	char name4[20];
	long phone4;
	char name5[20];
	long phone5;
	char name6[20];
	long phone6;
	char name7[20];
	long phone7;
	char name8[20];
	long phone8;
	char name9[20];
	long phone9;
	char name10[20];
	long phone10;
} data_PHB_info;


/*
  struct PRO_info pro_info;
  struct payload_info my_payload_info;
  struct data_point_info my_data_point_info;
  struct data_SOS_info my_data_SOS_info;
  struct data_IP_info my_data_IP_info;
  struct data_SILENCETIME_info my_data_SILENCETIME_info;
  struct data_REMIND_info my_data_REMIND_info;
  struct data_PHB_info my_data_PHB_info;


extern  struct PRO_info pro_info;
extern  struct payload_info my_payload_info;
extern  struct data_point_info my_data_point_info;
extern  struct data_SOS_info my_data_SOS_info;
extern  struct data_IP_info my_data_IP_info;
extern  struct data_SILENCETIME_info my_data_SILENCETIME_info;
extern  struct data_REMIND_info my_data_REMIND_info;
extern  struct data_PHB_info my_data_PHB_info;
*/


int MQTTClient_setup_with_pid_and_did(char* pid, char *did, char *productSecret);
int get_Payload_info( char *json_data);
int getHostByName(kal_uint8 *host,kal_uint8 *addr,kal_uint8 *addr_len,kal_uint32 dtacct_id,PsIntFuncPtr cbFunc);

#endif

