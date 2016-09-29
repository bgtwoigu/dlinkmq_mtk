
#ifndef __MQTT_MTK_
#define __MQTT_MTK_


#include "sci_api.h"
#include "socket_api.h"
#include "socket_types.h"
#include "Os_api.h"
#include "dlinkmq_api.h"

	
typedef enum
{
    networkNewAvailable,
    networkWaiting,
    networkFailed,
    networkFinished,
}networkStatus;


typedef struct Network Network;

struct Network
{
	TCPIP_SOCKET_T my_socket;
	int (*mqttread) (Network*, unsigned char*, int, int);
	int (*mqttwrite) (Network*, unsigned char*, int, int);
	void (*disconnect) (Network*);
	void (*readStatusHandler)(networkStatus status);
    void (*writeStatusHandler)(networkStatus status);
};


we_int DlinkmqNetwork_Init(we_handle *phDlinkmqNetworkHandle);

we_void DlinkmqNetwork_Destroy(we_handle hDlinkmqNetworkHandle);

int mtk_read(Network* n, unsigned char* buffer, int len, int timeout_ms);
int mtk_write(Network* n, unsigned char* buffer, int len, int timeout_ms);
void mtk_disconnect(Network* n);
void NewNetwork(Network* n );
int ConnectNetwork(Network* n, char*addr, int port);
void *DlinkMQTTMalloc(size_t len);
void DlinkMQTTFree(void *buf);

#define DLINKMQ_MALLOC(Param)       DlinkMQTTMalloc(Param)

#define DLINKMQ_FREE(param)  \
{\
	if(param != NULL)\
{\
	DlinkMQTTFree(param);\
	param = NULL;\
}\
}

#endif