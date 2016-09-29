#include "MQTTMTK.h"

#define MQTTAPP_SOCK_BUFFER_LEN 2048
#define MQTTAPP_SOCK_READ_BUFFER_LEN 2048

void *dlinkmq_med_malloc(size_t len);
void dlinkmq_med_free(void *buf);

int mtk_read(Network* n, char* buffer, int len, int flag)
{
	int rc = -1;
	do
	{
		if ((NULL == n) || (0 > n->my_socket))
			break;
		rc = sci_sock_recv(n->my_socket, buffer, len, flag);
	}while(0);

	return rc;
}


int mtk_write(Network* n, char* buffer, int len, int flags )
{
	int rc = -1;
	do
	{
		if ((NULL == n) || (0 > n->my_socket))
			break;
		rc = sci_sock_send(n->my_socket, buffer, len, flags);
	}while(0);

	return rc;
}

void mtk_disconnect(Network* n)
{
	sci_sock_sutdown(n->my_socket);
}


void NewNetwork(Network* n)
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
		
		sci_sock_sutdown(net_work->my_socket);
		net_work->my_socket = -1;	
	}

}


void MqttAppCloseSocket(Network *net_work)
{
	MqttAppCloseAccount(net_work);
}

extern BLOCK_ID g_dlinkmq_task_id; 
static int MqttAppSocketConnect(Network *n, struct sci_sockaddr *address)
{
	int status = 0;
	int ret = 0;
	ret = sci_sock_connect(n->my_socket, address, sizeof(struct sci_sockaddr));
	SCI_TRACE_ID(TRACE_INFO,"--MqttAppSocketConnect--sci_sock_connect--ret:%d",(uint8*)"",ret);

	if(ret < 0)
	{
		status = sci_sock_errno(n->my_socket);
		SCI_TRACE_ID(TRACE_INFO,"--MqttAppSocketConnect--sci_sock_connect--status:%d",(uint8*)"",status);

		switch(status)
		{
			case EWOULDBLOCK :
			case EINPROGRESS :

				ret = sci_sock_asyncselect(n->my_socket, g_dlinkmq_task_id, AS_CONNECT);
				break;
				
			default:

				ret = status;
				break;
		}
	}

	return ret;
}


static int MqttAppTcpStartConnect(Network *n, char* host, int port)
{
	int32 ret = 0;
	struct sci_sockaddr sockAddr;
	TCPIP_IPADDR_T ipaddr = 0; 

	sockAddr.family = AF_INET;
	sockAddr.port = htons(port);
	//SCI_MEMSET((void*)sockAddr.sa_data, 0, 8*sizeof(char));
	sockAddr.ip_addr = inet_addr(host); 
	

	ret = MqttAppSocketConnect(n, &sockAddr);

	return ret;
}


int ConnectNetwork(Network* n, char* host, int port)
{
	int32 ret = 0;
	int net_id = 0;

	n->my_socket = sci_sock_socket(AF_INET, SOCK_STREAM, 0, net_id);
	SCI_TRACE_ID(TRACE_INFO,"--ConnectNetwork--sci_sock_socket--socket:%d",(uint8*)"",n->my_socket);
		
	if(n->my_socket < 0)
	{
		ret = sci_sock_errno(n->my_socket);
		SCI_TRACE_ID(TRACE_ERROR,"--ConnectNetwork--sci_sock_socket--ret:%d",(uint8*)"",ret);
		return ret;
	}

	
	ret = sci_sock_setsockopt(n->my_socket, SO_NBIO, NULL);
	if(ret < 0)
	{
		ret = sci_sock_errno(n->my_socket);
		SCI_TRACE_ID(TRACE_ERROR,"--ConnectNetwork--sci_sock_setsockopt--ret:%d",(uint8*)"",ret);
		return ret;
	}

	ret = MqttAppTcpStartConnect(n, host, port);


	return ret;
		
}

int DlinkMQTTMalloc_num = 0;
void *DlinkMQTTMalloc(size_t len){

	void * result = SCI_ALLOC(len);
	if(result){
		memset(result,0,len);
		DlinkMQTTMalloc_num++;
	}
	return result;
}
void DlinkMQTTFree(void *buf){

	if (buf != NULL)
	{

		SCI_FREE(buf);
		DlinkMQTTMalloc_num--;
		SCI_TRACE_ID(TRACE_ERROR,"--DlinkMQTTFree--DlinkMQTTMalloc_num:%d",(uint8*)"",DlinkMQTTMalloc_num);
	}
}

void *dlinkmq_med_malloc(size_t len){

	void * result = SCI_ALLOC(len);
	if(result){
		memset(result,0,len);
	}
	return result;
}
void dlinkmq_med_free(void *buf){

	if (buf != NULL)
	{

		SCI_FREE(buf);
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
