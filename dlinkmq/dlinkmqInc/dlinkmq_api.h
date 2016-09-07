//
//  dlinkmq_api.h
//  DlinkMQ_Linux
//
//  Created by 黄锦 on 16/6/2.
//  Copyright ? 2016年 AppCan. All rights reserved.
//

#ifndef dlinkmq_api_h
#define dlinkmq_api_h

#include "dlinkmq_types.h"
#include "dlinkmq_error.h"





/* -------------------------------------------------

设备信息
	product_id 	产品ID,云端分配给厂商同种类型设备的唯一标识。
	product_secret 	产品密钥,与产品ID相对应。
	device_id 	设备ID,每个设备的唯一标识。

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

数据点消息
	cmd_id 	指令ID,云端定义的设备功能点的唯一标识。
	msg_id	数据点唯一标示。
	cmd_value	 数据点数据。
    	cmd_value_type	cmd_value类型，1为bool, 2为number, 3为string(此参数为服务端做参考用，设备端无实际意义)。
    	cmd_value_len	属性value的长度,数据长度。
 
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

设备初始化回调和接收数据点回调结构体
	on_receive_datapoint 	接收数据点回调
	on_receive_init  设备初始化回调
 
 -------------------------------------------------*/
typedef struct
{
    void (*on_receive_datapoint)(dlinkmq_datapoint *datapoint);
    void (*on_receive_init)(we_int32 err_code);
} dlinkmq_on_receive;


/* -------------------------------------------------

发送数据点回调
 
 -------------------------------------------------*/

typedef void (*on_send_msg)(we_int32 err_code, dlinkmq_datapoint *datapoint);


/* -------------------------------------------------

扩展回调
 
 -------------------------------------------------*/
typedef void (*dlinkmq_ext_callback) (we_int32 err_code, void* data);






/* -------------------------------------------------

设备初始化
@params
	device_info 设备信息
	pFun_cb 初始化设备、接收数据点回调函数结构体

@return
	参数无误返回DlinkMQ_ERROR_CODE_SUCCESS
 
 -------------------------------------------------*/
we_int32 dlinkmq_init_device_info(dlinkmq_device_info *device_info, dlinkmq_on_receive *fun_cb);


/* -------------------------------------------------

主循环函数
 
 -------------------------------------------------*/
void dlinkmq_run();



/* -------------------------------------------------

发送消息
@params
	datapoint 数据点
	fun_cb 发送消息回调函数

@return
	参数无误返回DlinkMQ_ERROR_CODE_SUCCESS
 
 -------------------------------------------------*/
we_int32 dlinkmq_publish(dlinkmq_datapoint *datapoint, on_send_msg fun_cb);


/* -------------------------------------------------

上传文件
@params
	file_path 数据点
	fun_cb 上传文件回调函数

@return
	参数无误返回DlinkMQ_ERROR_CODE_SUCCESS
 
 -------------------------------------------------*/
we_int32 dlinkmq_upload(char* file_path, dlinkmq_ext_callback fun_cb);


#endif /* dlinkmq_api_h */
