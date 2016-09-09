
#ifndef __MQTT_MTK_
#define __MQTT_MTK_

//#include "YxBasicApp.h"

#include "Soc_api.h"
#include "MMIDataType.h"
#include "Custom_mmi_default_value.h"
#include "App_datetime.h"
#include "App2soc_struct.h"
#include "OslMemory_Int.h"
#include "Med_utility.h"
#include "dlinkmq_api.h"
#include "cbm_api.h"
#include "cbm_consts.h"
#include "GPSSetting.h"

#pragma comment(lib,"ws2_32.lib")

#define MAPN_WAP             	0
#define MAPN_NET             	1
#define MAPN_WIFI            	2
#define PF_INET            		SOC_PF_INET 
#define SOCK_STREAM   			SOC_SOCK_STREAM





#define GetDateTime(t) applib_dt_get_date_time((applib_time_struct *)t)
#define	timeradd(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;	\
		if ((vvp)->tv_usec >= 1000000) {			\
			(vvp)->tv_sec++;				\
			(vvp)->tv_usec -= 1000000;			\
		}							\
	} while (0)
#define	timersub(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (0)
	
typedef enum
{
    networkNewAvailable,
    networkWaiting,
    networkFailed,
    networkFinished,
}networkStatus;

typedef struct{
  long tv_sec;
  long tv_usec;
}timeval;


typedef struct Timer Timer;
struct Timer {
	 timeval end_time;
};

typedef struct Network Network;

struct Network
{
	kal_int8 my_socket;
	int (*mqttread) (Network*, unsigned char*, int, int);
	int (*mqttwrite) (Network*, unsigned char*, int, int);
	void (*disconnect) (Network*);
	void (*readStatusHandler)(networkStatus status);
    void (*writeStatusHandler)(networkStatus status);
};


we_int DlinkmqNetwork_Init(we_handle *phDlinkmqNetworkHandle);

we_void DlinkmqNetwork_Destroy(we_handle hDlinkmqNetworkHandle);

char expired(Timer* timer);
void countdown_ms(Timer* timer, unsigned int);
void countdown(Timer* timer, unsigned int);
int left_ms(Timer* timer);

void InitTimer(Timer* timer);
int mtk_read(Network* n, unsigned char* buffer, int len, int timeout_ms);
int mtk_write(Network* n, unsigned char* buffer, int len, int timeout_ms);
void mtk_disconnect(Network* n);
void NewNetwork(Network* n  , PsIntFuncPtr soc_notify);
int ConnectNetwork(Network* n, char*addr, int port);
void *DlinkMQTTMalloc(size_t len);
void DlinkMQTTFree(void *buf);

#define DLINKMQ_MALLOC(Param)       DlinkMQTTMalloc(Param)

#define DLINKMQ_FREE(Param)	DlinkMQTTFree(Param)

#endif