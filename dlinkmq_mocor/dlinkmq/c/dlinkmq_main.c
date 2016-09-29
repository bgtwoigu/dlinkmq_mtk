/****************************************************************************
** File Name:      dlinkmq_main.c                                             *
** Author:                                                                 *
** Date:           22/09/2016                                             *
** Copyright:      APPCAN. All Rights Reserved.       				*
** Description:    DLINKMQ            						*
****************************************************************************
**                         Important Edit History                          *
** ------------------------------------------------------------------------*
** DATE           NAME             DESCRIPTION                             *
** 09/2016       jin.huang         Create
** 
****************************************************************************/

#ifdef DLINKMQ_SUPPORT

/**--------------------------------------------------------------------------*
 **                         Include Files                                    *
 **--------------------------------------------------------------------------*/
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
#include "dlinkmq_export.h"
#include "dlinkmq_api.h"



#define  P_DLINKMQ_STACK_SIZE            4000  
#define   P_DLINKMQ_QUEUE_NUM            8  
#define   P_DLINKMQ_TASK_PRIORITY 32 

//#define dlinkmq_ip "139.224.11.153"
//#define dlinkmq_port 10000
#define dlinkmq_ip "139.224.11.153"
#define dlinkmq_port 4004

extern BLOCK_ID g_dlinkmq_task_id;  
TCPIP_SOCKET_T dlinkmq_soc = 0;
SCI_TIMER_PTR *dlinkmq_init_timer = NULL;
uint8 dlinkmq_init_timer2 = 0;
void DLINKMQ_INIT_Task(uint32 argc, void *argv);  




int dlinkmq_SocInit(void)
{
	int32 ret = -1;
	int net_id = 0;

	dlinkmq_soc = sci_sock_socket(AF_INET, SOCK_STREAM, 0, net_id);
	SCI_TRACE_ID(TRACE_INFO,"--dlinkmq_SocInit--sci_sock_socket--socket:%d",(uint8*)"",dlinkmq_soc);
		
	if(dlinkmq_soc >= 0)
	{
		ret = sci_sock_setsockopt(dlinkmq_soc, SO_NBIO, NULL);
		//ret = sci_sock_setsockopt(dlinkmq_soc, SO_NONBLOCK, NULL);
		if(ret < 0)
		{
			ret = sci_sock_errno(dlinkmq_soc);
		}
		
	}
	else
	{ 
		ret = sci_sock_errno(dlinkmq_soc);
	}

	return ret;

}
int32 dlinkmq_SocConnect(int32 s)
{
	int32 ret = 0;
	struct sci_sockaddr sockAddr;
	TCPIP_IPADDR_T ipaddr = 0; 
	int temp_count = 0;

	sockAddr.family = AF_INET;
	sockAddr.port = htons(dlinkmq_port);
	//SCI_MEMSET((void*)sockAddr.sa_data, 0, 8*sizeof(char));

	if( 1 != inet_aton(&dlinkmq_ip, &ipaddr) )
	{
		return ret;
	}
	sockAddr.ip_addr = inet_addr(dlinkmq_ip); 

// 	ipaddr = lswap((int)dlinkmq_ip);
// 	sockAddr.ip_addr = htonl(ipaddr); 

// 	do
// 	{
// 		ret = sci_parse_host(&dlinkmq_ip, &sockAddr.ip_addr, 0);
// 		temp_count ++;
// 	}while((ret == 1) && (temp_count < 100));
    SCI_TRACE_ID(TRACE_INFO,"--dlinkmq_SocConnect--ip_addr:%d",(uint8*)"",sockAddr.ip_addr);
	
	ret = sci_sock_connect((long)s, &sockAddr, sizeof(struct sci_sockaddr));

	if(ret < 0)
	{
		ret = sci_sock_errno(dlinkmq_soc);
	}	
	if(ret == EINPROGRESS)
	{
		sci_sock_asyncselect(dlinkmq_soc, g_dlinkmq_task_id, AS_CONNECT);
	}
	
	return ret;

}

void dlinkmq_socket_test(void)
{
	int ret = 0;
	int error = 0;
	char send_buf[]="POST /mqtt/info HTTP/1.1\r\n"
		"Host: 139.224.11.153:4004\r\n"
		"Accept: application/json\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: 104\n\n"
		"{\"did\": \"333333\", \"pid\": \"b1e57467c92140e299022deb808cdd24\", \"sign\": \"44f2805b020aa6fef1e529d4d7d8319d\"}";
	
	char recv_buf[1000] = {0};
	int len = 1000;


	ret = dlinkmq_SocInit();
       SCI_TRACE_ID(TRACE_INFO,"--dlinkmq_socket_test--dlinkmq_SocInit--ret:%d",(uint8*)"",ret);

	if(ret == 0)
	{
		ret = dlinkmq_SocConnect(dlinkmq_soc);
       	SCI_TRACE_ID(TRACE_INFO,"--dlinkmq_socket_test--dlinkmq_SocConnect--ret:%d",(uint8*)"",ret);
		if (ret == 0 || ret == EINPROGRESS)
		{
			while(1)
			{
				SCI_Sleep(1000);
				ret = sci_sock_send((long)dlinkmq_soc, (char*)&send_buf, strlen(send_buf), 0);
				if(ret < 0)
				{
					ret = sci_sock_errno(dlinkmq_soc);
				}
				else
				{
					break;
				}
			}
				
       		SCI_TRACE_ID(TRACE_INFO,"--dlinkmq_socket_test--sci_sock_send--ret:%d",(uint8*)"",ret);

			while(1)
			{	
				SCI_Sleep(1000);
				ret = sci_sock_recv((long)dlinkmq_soc, &recv_buf, len, 0);
				if(ret < 0)
				{
					error = sci_sock_errno(dlinkmq_soc);
					if( EWOULDBLOCK == error ) 
					{
						SCI_Sleep(1000);
					}
				}
				else
				{
					break;
				}
			}
       		SCI_TRACE_ID(TRACE_INFO,"--dlinkmq_socket_test--sci_sock_recv--ret:%d",(uint8*)"",ret);
			if(ret > 0)
			{

			}
		}
	}
}


void TimerCallBack2( uint8  timer_id, uint32 param)
{
	dlinkmq_socket_test();
}

extern void dlinkmq_run(void *msg)
void DLINKMQ_INIT_Task(           //task处理方法的定义  
    uint32 argc,   
    void *argv)  
{  
    xSignalHeaderRec *psig = NULL;  

	
	dlinkmq_init_timer2 = MMK_CreateTimer(10000, 0);
	MMK_StartTimerCallback(dlinkmq_init_timer2, 10000, TimerCallBack2, NULL, 0);

	
    while (1)      
    {  
		g_dlinkmq_task_id = SCI_IdentifyThread();
		SCI_RECEIVE_SIGNAL(psig, g_dlinkmq_task_id); 
		
		dlinkmq_run((void*)psig);
		SCI_FREE_SIGNAL(psig);
        
    }
	
	
}



PUBLIC void DLINKMQ_Init(void)
{
	
	SCI_TRACE_ID(TRACE_INFO,"--DLINKMQ_Init--",(uint8*)"");
	
	g_dlinkmq_task_id = SCI_CreateThread(  
                            "T_P_DLINKMQ_TASK",  
                            "Q_P_DLINKMQ_TASK",  
                            DLINKMQ_INIT_Task,  
                            0,  
                            0,  
                            P_DLINKMQ_STACK_SIZE,  
                            P_DLINKMQ_QUEUE_NUM,  
                            P_DLINKMQ_TASK_PRIORITY,  
                            SCI_PREEMPT,   
                            SCI_AUTO_START);  




}

#endif

