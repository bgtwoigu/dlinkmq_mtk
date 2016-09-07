//
//  dlinkmq_miaoxin.h
//  DlinkMQ_Linux
//
//  Created by 黄锦 on 16/6/29.
//  Copyright ? 2016年 AppCan. All rights reserved.
//


/* -------------------------------------------------
 
 -------------------------------------------------*/
#define MIAOXIN_CMD_ID_DOWN_CALL				1//音频实时监听
#define MIAOXIN_CMD_ID_DOWN_AUDIO			2//录音
#define MIAOXIN_CMD_ID_DOWN_LOCATION_RT		3//实时定位
#define MIAOXIN_CMD_ID_DOWN_TEMP			4 //温度
#define MIAOXIN_CMD_ID_DOWN_STEP				5//运动步数
#define MIAOXIN_CMD_ID_DOWN_SLEEP			6//睡眠质量
#define MIAOXIN_CMD_ID_DOWN_DIET				7 //饮食数据
#define MIAOXIN_CMD_ID_DOWN_BATTERY			8//电量值
#define MIAOXIN_CMD_ID_DOWN_LOCATION		10//定位,区别于实时定位，这个只定位一次并返回数据
#define MIAOXIN_CMD_ID_DOWN_SETTING			11//设置设备数据
#define MIAOXIN_CMD_ID_DOWN_SETTING_LED		12//LED灯效报警时长
#define MIAOXIN_CMD_ID_DOWN_SETTING_BATTERY	13//低电量报警值
#define MIAOXIN_CMD_ID_DOWN_SETTING_UPLOAD	14//自动上报数据时间间隔
#define MIAOXIN_CMD_ID_DOWN_SETTING_STEP	15//设备运动上报开关




#define MIAOXIN_CMD_ID_UP_AUDIO				52//上报音频文件信息
#define MIAOXIN_CMD_ID_UP_LOCATION_RT		53//上报地理位置信息
#define MIAOXIN_CMD_ID_UP_TEMP				54 //上报温度
#define MIAOXIN_CMD_ID_UP_STEP				55//上报运动步数
#define MIAOXIN_CMD_ID_UP_SLEEP				56//上报睡眠质量
#define MIAOXIN_CMD_ID_UP_DIET				57 //上报饮食数据
#define MIAOXIN_CMD_ID_UP_BATTERY			58//上报电量值
#define MIAOXIN_CMD_ID_UP_LED					59//上报LED提示


/* -------------------------------------------------
 
 -------------------------------------------------*/
typedef struct
{
    we_uint32   record_time;		//录音时长
    we_uint32   LED_time;			//LED灯效报警提示时长
    we_uint32   battery_val;			//报警电量值
    we_uint32   upload_time;		//自动上传数据时间间隔
    we_uint32	location_time;			//实时定位时长
} miaoxin_settings;


enum data_type{
	DATA_TYPE_BOOL=1,
	DATA_TYPE_NUMBER,
	DATA_TYPE_STRING
};

