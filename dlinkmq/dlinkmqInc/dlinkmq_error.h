//
//  dlinkmq_error.h
//  DlinkMQ_Linux
//
//  Created by 黄锦 on 16/6/2.
//  Copyright ? 2016年 AppCan. All rights reserved.
//

#ifndef dlinkmq_error_h
#define dlinkmq_error_h


/* -------------------------------------------------
 
 -------------------------------------------------*/
#define DlinkMQ_ERROR_CODE_WOULDBLOCK	-2  //阻塞状态，请稍后重试
#define DlinkMQ_ERROR_CODE_FAIL       		-1
#define DlinkMQ_ERROR_CODE_SUCCESS          	0   //成功
#define DlinkMQ_ERROR_CODE_PARAM            	1   //参数错误
#define DlinkMQ_ERROR_CODE_SOC_CREAT        	2   //创建网络失败
#define DlinkMQ_ERROR_CODE_SOC_CONN         	3   //网络连接失败
#define DlinkMQ_ERROR_CODE_DATA             	4   //数据异常
#define DlinkMQ_ERROR_CODE_DISK_FULL		5   
#define DlinkMQ_ERROR_CODE_GET_HOSTNAME         	6   //GETHOSTNAME失败


#endif /* dlinkmq_error_h */
