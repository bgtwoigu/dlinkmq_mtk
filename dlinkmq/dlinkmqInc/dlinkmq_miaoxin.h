//
//  dlinkmq_miaoxin.h
//  DlinkMQ_Linux
//
//  Created by �ƽ� on 16/6/29.
//  Copyright ? 2016�� AppCan. All rights reserved.
//


/* -------------------------------------------------
 
 -------------------------------------------------*/
#define MIAOXIN_CMD_ID_DOWN_CALL				1//��Ƶʵʱ����
#define MIAOXIN_CMD_ID_DOWN_AUDIO			2//¼��
#define MIAOXIN_CMD_ID_DOWN_LOCATION_RT		3//ʵʱ��λ
#define MIAOXIN_CMD_ID_DOWN_TEMP			4 //�¶�
#define MIAOXIN_CMD_ID_DOWN_STEP				5//�˶�����
#define MIAOXIN_CMD_ID_DOWN_SLEEP			6//˯������
#define MIAOXIN_CMD_ID_DOWN_DIET				7 //��ʳ����
#define MIAOXIN_CMD_ID_DOWN_BATTERY			8//����ֵ
#define MIAOXIN_CMD_ID_DOWN_LOCATION		10//��λ,������ʵʱ��λ�����ֻ��λһ�β���������
#define MIAOXIN_CMD_ID_DOWN_SETTING			11//�����豸����
#define MIAOXIN_CMD_ID_DOWN_SETTING_LED		12//LED��Ч����ʱ��
#define MIAOXIN_CMD_ID_DOWN_SETTING_BATTERY	13//�͵�������ֵ
#define MIAOXIN_CMD_ID_DOWN_SETTING_UPLOAD	14//�Զ��ϱ�����ʱ����
#define MIAOXIN_CMD_ID_DOWN_SETTING_STEP	15//�豸�˶��ϱ�����




#define MIAOXIN_CMD_ID_UP_AUDIO				52//�ϱ���Ƶ�ļ���Ϣ
#define MIAOXIN_CMD_ID_UP_LOCATION_RT		53//�ϱ�����λ����Ϣ
#define MIAOXIN_CMD_ID_UP_TEMP				54 //�ϱ��¶�
#define MIAOXIN_CMD_ID_UP_STEP				55//�ϱ��˶�����
#define MIAOXIN_CMD_ID_UP_SLEEP				56//�ϱ�˯������
#define MIAOXIN_CMD_ID_UP_DIET				57 //�ϱ���ʳ����
#define MIAOXIN_CMD_ID_UP_BATTERY			58//�ϱ�����ֵ
#define MIAOXIN_CMD_ID_UP_LED					59//�ϱ�LED��ʾ


/* -------------------------------------------------
 
 -------------------------------------------------*/
typedef struct
{
    we_uint32   record_time;		//¼��ʱ��
    we_uint32   LED_time;			//LED��Ч������ʾʱ��
    we_uint32   battery_val;			//��������ֵ
    we_uint32   upload_time;		//�Զ��ϴ�����ʱ����
    we_uint32	location_time;			//ʵʱ��λʱ��
} miaoxin_settings;


enum data_type{
	DATA_TYPE_BOOL=1,
	DATA_TYPE_NUMBER,
	DATA_TYPE_STRING
};

