
#include "DtcntSrvDb.h"
#include "DtcntSrvIntStruct.h"

#include "SimCtrlSrvGprot.h"
#include "NwInfoSrvGprot.h"
#include "ProfilesSrvGprot.h"

#include "med_api.h"
#include "TimerEvents.h"

#include "MQTTMTK.h"

#define MQTTAPP_SOCK_BUFFER_LEN 2048
#define MQTTAPP_SOCK_READ_BUFFER_LEN 2048
//static Network* g_netWork = NULL;
U8 g_user_apn[8] = {0};
kal_uint32 account_id = 0;

void *dlinkmq_med_malloc(size_t len);
void dlinkmq_med_free(void *buf);

static U8 MqttAppSockNotifyCb(void* inMsg);
static S8 DlinkmqNetwork_GetHostByNameInt(const S8 *host,U8 *addr,U8 *addr_len,U32 dtacct_id, kal_int32 request_id);
/*****************************************************************************
* DESCRIPTION:根据当前的网络环境获取对应APN
* PARAMETERS :void
* RETURNS    :void
*****************************************************************************/
BOOL User_Socket_GetApn(void)
{	
	char pPLMN_SIM1[SRV_MAX_PLMN_LEN + 1] = {0};
	char pPLMN_SIM2[SRV_MAX_PLMN_LEN + 1] = {0};
	char *pPLMN = NULL;

	srv_nw_info_get_nw_plmn(MMI_SIM1, &pPLMN_SIM1[0], SRV_MAX_PLMN_LEN + 1);
	if ((srv_nw_info_get_service_availability(MMI_SIM1) == SRV_NW_INFO_SA_FULL_SERVICE) )
	{
		if (memcmp((char *)&pPLMN_SIM1[0],"46002", 5) == 0
			||memcmp((char *)&pPLMN_SIM1[0],"46000", 5) == 0)
		{
			strcpy(g_user_apn, "cmnet");
			return TRUE;
		} 
		else if (memcmp((char *)&pPLMN_SIM1[0], "46001",5)==0)
		{
			strcpy(g_user_apn, "uninet");
			return TRUE;
		} 
		else
		{
			memset(g_user_apn, 0, sizeof(g_user_apn));
		} 
	}

	return FALSE;
}


/*****************************************************************************
* DESCRIPTION:新增数据账号
* PARAMETERS :void
* RETURNS    :void
*****************************************************************************/
U32 User_Socket_AddAccProfId(const WCHAR* account_name)
{
	srv_dtcnt_store_prof_data_struct store_prof_data;
	srv_dtcnt_prof_gprs_struct prof_gprs;
	U32 acc_prof_id;
	srv_dtcnt_result_enum ret;

	memset(&prof_gprs, 0, sizeof(prof_gprs));
	prof_gprs.APN = g_user_apn;
	prof_gprs.prof_common_header.sim_info = SRV_DTCNT_SIM_TYPE_1;
	prof_gprs.prof_common_header.AccountName = (const U8*)account_name;
	prof_gprs.prof_common_header.acct_type = SRV_DTCNT_PROF_TYPE_USER_CONF;
	prof_gprs.prof_common_header.px_service = SRV_DTCNT_PROF_PX_SRV_HTTP;

	store_prof_data.prof_data = &prof_gprs;
	store_prof_data.prof_fields = SRV_DTCNT_PROF_FIELD_ALL;
	store_prof_data.prof_type = SRV_DTCNT_BEARER_GPRS;

	ret = srv_dtcnt_store_add_prof(&store_prof_data, &acc_prof_id);
	if(ret == SRV_DTCNT_RESULT_SUCCESS)
	{

	}
	else
	{
		acc_prof_id = -1;
	}

	return acc_prof_id;
}

U32 User_Socket_FindAccProfIdByApn(const char* apn, cbm_sim_id_enum sim)
{
	extern srv_dtcnt_store_info_context g_srv_dtcnt_store_ctx;
	int i;

	//    mmi_wlan_dtcnt_query_profile(g_wlan_display_context.trust_list_hilt_profile_p->profile_id, &g_wlan_info_prof);


	for(i = 0; i < SRV_DTCNT_PROF_MAX_ACCOUNT_NUM; ++i)
	{		
		if(g_srv_dtcnt_store_ctx.acc_list[i].in_use &&
			g_srv_dtcnt_store_ctx.acc_list[i].bearer_type == SRV_DTCNT_BEARER_GPRS &&
			g_srv_dtcnt_store_ctx.acc_list[i].sim_info == (sim + 1) &&
			app_stricmp((char*)g_srv_dtcnt_store_ctx.acc_list[i].dest_name, (char*)apn) == 0)
		{
			return g_srv_dtcnt_store_ctx.acc_list[i].acc_id;
		}
	}

	return -1;
}

/*****************************************************************************
* DESCRIPTION:获取数据账号
* PARAMETERS :void
* RETURNS    :void
*****************************************************************************/
U32 User_Socket_GetAccProfId(char* apn, cbm_sim_id_enum sim_id)
{

	const WCHAR name[] = L"USER GPRS";
	U32 acc_prof_id;

	acc_prof_id = User_Socket_FindAccProfIdByApn((const char*)apn, sim_id);
	if(acc_prof_id != -1)
	{
		return acc_prof_id;
	}	
	else	
	{
		acc_prof_id = User_Socket_AddAccProfId(name);
	}	

	return acc_prof_id;
}


void getTimeval( timeval *timev ,MYTIME t)
{
	long sec=(t.nYear -1970) *365*24*60*60+( t.nMonth-1)*30*24*60*60 +(t.nDay-1)*24*60*60 +t.nHour*60*60 +t.nMin*60 +t.nSec;
	timev->tv_sec=sec;
	timev->tv_usec=0;
}

char expired(St_Timer* timer)
{
	 timeval now, res;
	MYTIME t;
	GetDateTime(&t);
	getTimeval(&now,  t);
	timersub(&timer->end_time, &now, &res);	
	
	return res.tv_sec < 0 || (res.tv_sec == 0 && res.tv_usec <= 0);
}


void countdown_ms(St_Timer* timer, unsigned int timeout)
{
	 timeval now,interval;
	MYTIME t;
	GetDateTime(&t);
	getTimeval(&now,  t);
	interval.tv_sec=timeout / 1000;
	interval.tv_usec=(timeout % 1000) * 1000;
	 
	timeradd(&now, &interval, &timer->end_time);
}


void countdown(St_Timer* timer, unsigned int timeout)
{
	 timeval now ,interval;
	MYTIME t;
	GetDateTime(&t);
	getTimeval(&now,  t);
	interval.tv_sec=timeout;
	interval.tv_usec=0;
	
	timeradd(&now, &interval, &timer->end_time);
}

int left_ms(St_Timer* timer)
{
	 timeval now, res;
	MYTIME t;
	GetDateTime(&t);
	getTimeval(&now,  t);
	timersub(&timer->end_time, &now, &res);
	//printf("left %d ms\n", (res.tv_sec < 0) ? 0 : res.tv_sec * 1000 + res.tv_usec / 1000);
	
	return (res.tv_sec < 0) ? 0 : res.tv_sec * 1000 + res.tv_usec / 1000;
}


void InitTimer(St_Timer* timer)
{
	 timeval interval;
	interval.tv_sec=0;
	interval.tv_usec=0;
	timer->end_time = interval;
}


int mtk_read(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
#if 1
	int rc = -1;
	do
	{
		if ((NULL == n) || (0 > n->my_socket))
			break;
		rc = soc_recv(n->my_socket, buffer, len, 0);
	}while(0);

	return rc;
#else
	int rc = soc_recv(n->my_socket, buffer, len, 0);
	return rc;
#endif
}


int mtk_write(Network* n, unsigned char* buffer, int len, int timeout_ms )
{
#if 1
	int rc = -1;
	do
	{
		if ((NULL == n) || (0 > n->my_socket))
			break;
		rc = soc_send(n->my_socket, buffer, len, 0);
	}while(0);

	return rc;
#else
    	//printf("ssss-len = %d, socketid = 0x%x\r\n", len, n->my_socket);
	int	rc = soc_send(n->my_socket, buffer, len,0);
	
	return rc;
#endif

}

void mtk_disconnect(Network* n)
{
	soc_close(n->my_socket);
}


void NewNetwork(Network* n  , PsIntFuncPtr soc_notify)
{
	n->my_socket = -1;
	n->mqttread = mtk_read;
	n->mqttwrite = mtk_write;
	n->disconnect =  mtk_disconnect;
	//g_netWork = n;
	//mmi_frm_set_protocol_event_handler(MSG_ID_APP_SOC_NOTIFY_IND, soc_notify, MMI_TRUE);
}

static void MqttAppCloseAccount(Network* net_work)
{
	if(net_work && net_work->my_socket>=0)
	{
		
		soc_close(net_work->my_socket);
		//cbm_deregister_app_id(net_work->app_id);
		//account_id = 0;
		net_work->my_socket = -1;
		/*mmi_frm_clear_protocol_event_handler(MSG_ID_APP_SOC_GET_HOST_BY_NAME_IND, 
			(PsIntFuncPtr)YxAppGetHostByNameIntCb);*/
		//mmi_frm_clear_protocol_event_handler(MSG_ID_APP_SOC_NOTIFY_IND, 
			//(PsIntFuncPtr)MqttAppSockNotifyCb);	
	}

}


void MqttAppCloseSocket(Network *net_work)
{
	MqttAppCloseAccount(net_work);
}



/*static U8 MqttAppSockNotifyCb(void* inMsg)
{
    app_soc_notify_ind_struct *ind = NULL;
    if(inMsg==NULL)                   
        return 0;
	ind = (app_soc_notify_ind_struct*)inMsg;
	if((ind->socket_id != g_netWork->my_socket)||(g_netWork->my_socket <0))
		return 0;
	switch(ind->event_type)
	{
	case SOC_CONNECT:
		if(ind->result != FALSE)
        {
			
		}
		else
		{
			MqttAppCloseSocket(1);

		}
		break;
	case SOC_READ:
__CONTINUE_READ:
		{
			
		}
		break;
	case SOC_CLOSE:
		MqttAppCloseSocket(1);
		break;
	case SOC_WRITE:
		break;
	default:
		break;
	}
    return 1;
}*/

static int MqttAppSocketConnect(Network *net_work, sockaddr_struct *address)
{
	int status;
	status=soc_connect(net_work->my_socket, address);
	printf("\n--soc:%d-soc_connect:%d",net_work->my_socket,status);
	kal_prompt_trace(MOD_MQTT,"---soc_connect:%d",status);

	switch(status)
	{
	case SOC_SUCCESS:
		return DlinkMQ_ERROR_CODE_SUCCESS;
	case SOC_WOULDBLOCK:
		//mmi_frm_set_protocol_event_handler(MSG_ID_APP_SOC_NOTIFY_IND, soc_notify, MMI_TRUE);
		return DlinkMQ_ERROR_CODE_WOULDBLOCK;
	default:
		MqttAppCloseSocket(net_work);
		return DlinkMQ_ERROR_CODE_SOC_CONN;
	}
}


static int MqttAppTcpStartConnect(Network *net_work, const S8* host, U16 port, S8 apn)
{

	sockaddr_struct addr;               // connect 地址
	kal_uint8 addr_len = 0;
	kal_int8  ret = -1;
	kal_bool  is_ip_valid = KAL_FALSE;

	//getHostByName();
	if(net_work->my_socket <0
		|| host == NULL)
	{
		return ret;
	}
	memset(&addr,0,sizeof(sockaddr_struct));
	addr.sock_type = SOC_SOCK_STREAM;
	if(apn == MAPN_WAP)    // cmwap 连接
	{
		
	}
	else// cmnet 连接
	{
		kal_char* host_name=NULL;
		
		host_name = DLINKMQ_MALLOC(strlen(host) + 1);
		if(host_name==NULL)
		{
			MqttAppCloseSocket(net_work);
			return ret;
		}
		memset(host_name, 0x00, strlen(host) + 1);
		strcpy(host_name, host);
		if(soc_ip_check(host_name, addr.addr, &is_ip_valid) == KAL_FALSE)
		{
			
			kal_uint8  buf[4]={0};
			DLINKMQ_FREE(host_name);

			ret = DlinkmqNetwork_GetHostByNameInt(host, buf, &addr_len, account_id, net_work->my_socket); 
			if(ret == SOC_SUCCESS)             // success
			{
				memcpy(addr.addr, buf, 4);
				addr.addr_len = addr_len;
				addr.port = port;
			}
			else if(ret == SOC_WOULDBLOCK)// block
			{
				return DlinkMQ_ERROR_CODE_WOULDBLOCK;
			}
			else                                    // error
			{
				return DlinkMQ_ERROR_CODE_GET_HOSTNAME; //需要重连
			}

		}
		else if(is_ip_valid == KAL_FALSE)  
		{
			DLINKMQ_FREE(host_name);
			MqttAppCloseSocket(net_work);
			return ret;
		}
		else
		{
			DLINKMQ_FREE(host_name);
			addr.addr_len = 4;
			addr.port = port;
		}
	}
	return MqttAppSocketConnect(net_work, &addr);
}


int ConnectNetwork(Network* n, char* addr, int port)
{
	int rc = -1;
	//U32    account_id = 33570304; //模拟器
	//U32    account_id = 10;
	
	kal_uint8 app_id = 0;
	kal_uint8 sim_id = 1;
	kal_uint32 data_account = 0;
	//kal_uint32 account_id = 0;
#if 0
	cbm_register_app_id(&n->app_id);
	//sim_id = mmi_gps_setting_get_sim();
	data_account= cbm_encode_data_account_id(CBM_DEFAULT_ACCT_ID, (cbm_sim_id_enum)sim_id, 0, MMI_FALSE);
	kal_prompt_trace(MOD_MQTT,"---soc_creat-data_account:%d",data_account);
	account_id = cbm_encode_data_account_id(data_account, (cbm_sim_id_enum)sim_id, n->app_id, MMI_FALSE);
	kal_prompt_trace(MOD_MQTT,"---soc_creat-account_id:%d",account_id);

#else
	if(account_id==0)
	{
		
		cbm_app_info_struct app_info;
		S8 ret = 0;
		S32 i;
		User_Socket_GetApn();
		memset(&app_info, 0, sizeof(app_info));
		app_info.app_icon_id = 301;
		app_info.app_str_id = 111;
		app_info.app_type = 0;	 //DTCNT_APPTYPE_NONE 
		ret = cbm_register_app_id_with_app_info(&app_info, &app_id);
		i = User_Socket_GetAccProfId(g_user_apn, CBM_SIM_ID_SIM1);
		cbm_hold_bearer(app_id);
		account_id = cbm_encode_data_account_id(i, CBM_SIM_ID_SIM1, app_id, KAL_FALSE);
	
	}

#endif
	if (n == NULL || addr == NULL)
	{
		return rc; 
	}

	n->my_socket = soc_create(SOC_PF_INET, SOC_SOCK_STREAM, 0, MOD_MQTT, account_id);
	printf("\n---soc_creat-soc_id:%d---accound_id:%d",n->my_socket,account_id);
	kal_prompt_trace(MOD_MQTT,"---soc_creat-soc_id:%d---accound_id:%d",n->my_socket,account_id);

	if (n->my_socket >= 0) 
	{
		kal_uint8 val = 1;
		/*kal_int32 bufLen = MQTTAPP_SOCK_BUFFER_LEN;
		if(soc_setsockopt(n->my_socket, SOC_SENDBUF, &bufLen, sizeof(bufLen)) < 0)
		{
			MqttAppCloseSocket(0);
			return rc;
		}
		bufLen = MQTTAPP_SOCK_READ_BUFFER_LEN;
		if(soc_setsockopt(n->my_socket, SOC_RCVBUF, &bufLen, sizeof(bufLen)) < 0)
		{
			MqttAppCloseSocket(0);
			return rc;
		}*/
        	if(soc_setsockopt(n->my_socket, SOC_NBIO, &val, sizeof(val)) < 0)
        	{
				MqttAppCloseSocket(n);
				return rc;
        	}
        	else
        	{   
            		val = SOC_READ | SOC_WRITE | SOC_CLOSE | SOC_CONNECT;
            		if(soc_setsockopt(n->my_socket, SOC_ASYNC, &val, sizeof(val)) < 0)
            		{
                		MqttAppCloseSocket(n);
						return rc;
            		}

				return MqttAppTcpStartConnect(n, addr, port, MAPN_NET);
        	}
	}  else {

		rc = DlinkMQ_ERROR_CODE_SOC_CREAT;
	}

	return rc;
		
}

int DlinkMQTTMalloc_num = 0;
void *DlinkMQTTMalloc(size_t len){

	void * result = med_alloc_ext_mem(len);
	if(result){
		memset(result,0,len);
		DlinkMQTTMalloc_num++;
	}
	return result;
}
void DlinkMQTTFree(void *buf){

	if (buf != NULL)
	{

		med_free_ext_mem(&buf);
		DlinkMQTTMalloc_num--;
		kal_prompt_trace(MOD_MQTT,"---DlinkMQTTMalloc_num= %d",DlinkMQTTMalloc_num);
	}
}

void *dlinkmq_med_malloc(size_t len){

	void * result = med_alloc_ext_mem(len);
	if(result){
		memset(result,0,len);
	}
	return result;
}
void dlinkmq_med_free(void *buf){

	if (buf != NULL)
	{

		med_free_ext_mem(&buf);
	}
	
}



we_int DlinkmqNetwork_Init(we_handle *phDlinkmqNetworkHandle)
{
	we_int ret = DlinkMQ_ERROR_CODE_FAIL;

	Network *pstNet = (Network *)DLINKMQ_MALLOC(sizeof(Network));

	if (pstNet == NULL) {

		return ret;
	}

	
	
	*phDlinkmqNetworkHandle = pstNet;

	ret = DlinkMQ_ERROR_CODE_SUCCESS;
	return ret;
}

we_void DlinkmqNetwork_Destroy(we_handle hDlinkmqNetworkHandle)
{
	Network *pstNet = (Network *)hDlinkmqNetworkHandle;

	if(pstNet != NULL) {

		MqttAppCloseSocket(pstNet);
		DLINKMQ_FREE(pstNet);
	}
}



static S8 DlinkmqNetwork_GetHostByNameInt(const S8 *host,U8 *addr,U8 *addr_len,U32 dtacct_id, kal_int32 request_id)
{
	kal_int8 ret = 0;    

	ret = soc_gethostbyname(KAL_FALSE, 
		MOD_MQTT, 
		request_id, 
		host, 
		(kal_uint8*)addr, 
		(kal_uint8*)addr_len, 
		0, 
		dtacct_id);
	return ret;
}