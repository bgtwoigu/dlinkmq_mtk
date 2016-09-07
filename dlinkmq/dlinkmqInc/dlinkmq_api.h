//
//  dlinkmq_api.h
//  DlinkMQ_Linux
//
//  Created by �ƽ� on 16/6/2.
//  Copyright ? 2016�� AppCan. All rights reserved.
//

#ifndef dlinkmq_api_h
#define dlinkmq_api_h

#include "dlinkmq_types.h"
#include "dlinkmq_error.h"





/* -------------------------------------------------

�豸��Ϣ
	product_id 	��ƷID,�ƶ˷��������ͬ�������豸��Ψһ��ʶ��
	product_secret 	��Ʒ��Կ,���ƷID���Ӧ��
	device_id 	�豸ID,ÿ���豸��Ψһ��ʶ��

 -------------------------------------------------*/
#define DlinkMQ_product_id_len 40
#define DlinkMQ_product_secret_len 40
#define DlinkMQ_device_id_len 40
typedef struct
{
    we_int8	product_id[DlinkMQ_product_id_len];
    we_int8	product_secret[DlinkMQ_product_secret_len];
    we_int8	device_id[DlinkMQ_device_id_len];
} dlinkmq_device_info;

/* -------------------------------------------------

���ݵ���Ϣ
	cmd_id 	ָ��ID,�ƶ˶�����豸���ܵ��Ψһ��ʶ��
	msg_id	���ݵ�Ψһ��ʾ��
	cmd_value	 ���ݵ����ݡ�
    	cmd_value_type	cmd_value���ͣ�1Ϊbool, 2Ϊnumber, 3Ϊstring(�˲���Ϊ��������ο��ã��豸����ʵ������)��
    	cmd_value_len	����value�ĳ���,���ݳ��ȡ�
 
 -------------------------------------------------*/
typedef struct
{
    we_uint32   cmd_id;
    we_uint16   msg_id;
    we_int8    	*cmd_value;
    we_uint32   cmd_value_type;
    we_uint32   cmd_value_len;
} dlinkmq_datapoint;


/* -------------------------------------------------

�豸��ʼ���ص��ͽ������ݵ�ص��ṹ��
	on_receive_datapoint 	�������ݵ�ص�
	on_receive_init  �豸��ʼ���ص�
 
 -------------------------------------------------*/
typedef struct
{
    void (*on_receive_datapoint)(dlinkmq_datapoint *datapoint);
    void (*on_receive_init)(we_int32 err_code);
} dlinkmq_on_receive;


/* -------------------------------------------------

�������ݵ�ص�
 
 -------------------------------------------------*/

typedef void (*on_send_msg)(we_int32 err_code, dlinkmq_datapoint *datapoint);


/* -------------------------------------------------

��չ�ص�
 
 -------------------------------------------------*/
typedef void (*dlinkmq_ext_callback) (we_int32 err_code, void* data);






/* -------------------------------------------------

�豸��ʼ��
@params
	device_info �豸��Ϣ
	pFun_cb ��ʼ���豸���������ݵ�ص������ṹ��

@return
	�������󷵻�DlinkMQ_ERROR_CODE_SUCCESS
 
 -------------------------------------------------*/
we_int32 dlinkmq_init_device_info(dlinkmq_device_info *device_info, dlinkmq_on_receive *fun_cb);


/* -------------------------------------------------

��ѭ������
 
 -------------------------------------------------*/
void dlinkmq_run();



/* -------------------------------------------------

������Ϣ
@params
	datapoint ���ݵ�
	fun_cb ������Ϣ�ص�����

@return
	�������󷵻�DlinkMQ_ERROR_CODE_SUCCESS
 
 -------------------------------------------------*/
we_int32 dlinkmq_publish(dlinkmq_datapoint *datapoint, on_send_msg fun_cb);


/* -------------------------------------------------

�ϴ��ļ�
@params
	file_path ���ݵ�
	fun_cb �ϴ��ļ��ص�����

@return
	�������󷵻�DlinkMQ_ERROR_CODE_SUCCESS
 
 -------------------------------------------------*/
we_int32 dlinkmq_upload(char* file_path, dlinkmq_ext_callback fun_cb);


#endif /* dlinkmq_api_h */
