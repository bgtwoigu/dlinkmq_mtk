

#include "mmi_platform.h"
//#ifdef __NON_BLOCKING_FILE_MOVE_SUPPORT__
#include "kal_public_api.h"

#include "kal_public_defs.h" 
#include "stack_ltlcom.h"


#include "syscomp_config.h"
#include "task_config.h"        /* Task creation */

#include "stacklib.h"           /* Basic type for dll, evshed, stacktimer */
#include "stack_timer.h"        /* Stack timer */

#include "fat_fs.h"             /* file system */
///#endif /* __NON_BLOCKING_FILE_MOVE_SUPPORT__ */ 
#include "stack_ltlcom.h"
#include "FileManagerGProt.h"
#include "fmt_struct.h"


#ifdef __DRM_SUPPORT__
#include "drm_gprot.h"
#endif


#include "MQTTTask.h"


#include "MQTTTimer.h"

#include "MQTTTrace.h"





typedef struct mqtt_frm_timer_t{
	kal_uint16          timer_index;
    kal_uint16          timer_id;
	kal_uint32          left_ticks;
    mqtt_timer_cb_t     cb;
    void  *             arg;
	struct mqtt_frm_timer_t * next;
} mqtt_frm_timer_t;

typedef struct
{
	mqtt_frm_timer_t  mqtt_timer[MQTT_MAX_TIMERS];
	struct mqtt_frm_timer_t * pFirst;
	struct mqtt_frm_timer_t * pFree;
	struct mqtt_frm_timer_t * pFreeLast;
	kal_uint32        start_tick;
	kal_uint32        stop_tick;
	kal_uint32        ntimers;
}mqtt_timer_t;


static mqtt_timer_t g_mqtt_timer;

static stack_timer_struct g_mqtt_task_timer;

#define MQTT_STACK_BACK_TIMER_ID		(149)


void mqtt_timer_frm_data_reset(void)
{
	kal_uint16 i = 0;

	mqtt_frm_timer_t * ptimer = g_mqtt_timer.mqtt_timer;
	mqtt_frm_timer_t * pfreetimer = NULL;
	mqtt_frm_timer_t * plast = NULL;
	
	memset((void*)&g_mqtt_timer, 0, sizeof(g_mqtt_timer));
	
	for (i=0; i<MQTT_MAX_TIMERS; i++)
	{
		ptimer[i].timer_id    = MQTT_INVALID_TIMER_ID;
		ptimer[i].next        = NULL;
		ptimer[i].timer_index = i;
		if (NULL != plast)
			plast->next = &ptimer[i];
		else
			pfreetimer = &ptimer[i];
		plast = &ptimer[i];
	}

	g_mqtt_timer.pFirst     = NULL;
	g_mqtt_timer.pFree      = pfreetimer;
	g_mqtt_timer.pFreeLast  = plast;
	g_mqtt_timer.start_tick = 0;
	g_mqtt_timer.stop_tick  = 0;
	g_mqtt_timer.ntimers    = 0;
}

void mqtt_frame_timer_init(void)
{
	mqtt_timer_frm_data_reset();
	
	stack_init_timer(&g_mqtt_task_timer, "Mfrm timer", MOD_MQTT);
}


static void mqtt_stop_base_timer(void)
{
	g_mqtt_timer.stop_tick = kal_get_systicks();
	stack_stop_timer(&g_mqtt_task_timer);
	printf("=====stop_base_timer()ticks=%x=======", g_mqtt_timer.stop_tick);
}

static void mqtt_sart_base_timer(kal_uint32 elpase_tick)
{
	if (1 > elpase_tick)
		elpase_tick = 1;
	
	stack_start_timer(&g_mqtt_task_timer, MQTT_STACK_BACK_TIMER_ID, elpase_tick);
	g_mqtt_timer.start_tick = kal_get_systicks();
	printf("=====sart_base_timer()ticks=%x=======", g_mqtt_timer.start_tick);
}

int mqtt_timer_is_existed(kal_uint16 timer_id)
{
	int is_existed = 0;
	kal_uint16         cnt;
	mqtt_frm_timer_t * curtimer = g_mqtt_timer.pFirst; 

	cnt = 0;
	while((NULL != curtimer) && (MQTT_MAX_TIMERS > cnt))
	{
		if (timer_id == curtimer->timer_id)
		{
			is_existed = 1;
			break;
		}
		curtimer = curtimer->next;
		cnt ++;
	}
	
	return is_existed;
}




void mqtt_stop_timer(kal_uint16 timer_id)
{
	kal_uint16         cnt;
	mqtt_frm_timer_t * curtimer; 
	mqtt_frm_timer_t * prevtimer; 
	mqtt_frm_timer_t * deltimer  = NULL; 
	kal_uint32         diff_tick = 0;
	int        		   existed  = 0;
	int        		   need_hndlr  = 0;
	

	curtimer = g_mqtt_timer.pFirst;
	prevtimer = curtimer;
	cnt      = 0;
	while((NULL != curtimer) && (MQTT_MAX_TIMERS > cnt))
	{
		if (timer_id == curtimer->timer_id)
		{
			curtimer->timer_id = MQTT_INVALID_TIMER_ID;
			if (NULL != curtimer->next)
			{
				curtimer->next->left_ticks += curtimer->left_ticks;
			}
			deltimer = curtimer;
			existed = 1;
			break;
		}
		prevtimer = curtimer;
		curtimer  = curtimer->next;
		cnt ++;
	}

	if (1 == existed)
	{
		if (curtimer == g_mqtt_timer.pFirst)
		{
			g_mqtt_timer.pFirst = curtimer->next;
				
			mqtt_stop_base_timer();
			if (NULL != g_mqtt_timer.pFirst)
			{
				diff_tick = 0;
				if (g_mqtt_timer.stop_tick >= g_mqtt_timer.start_tick)
				{
					diff_tick = g_mqtt_timer.stop_tick - g_mqtt_timer.start_tick;
				}
				else
				{
					printf("=========== mqtt_stop_timer() ticks reverse===================");
					// .......
					//diff_tick = 1;
				}
				if (0 < diff_tick)
				{
					if (diff_tick <= g_mqtt_timer.pFirst->left_ticks)
						g_mqtt_timer.pFirst->left_ticks -= diff_tick;
					else 
					{
						kal_uint32   tot_ticks = 0;
						cnt = 0;
						tot_ticks = 0;
						curtimer = g_mqtt_timer.pFirst;
						while((NULL != curtimer) && (MQTT_MAX_TIMERS > cnt))
						{
							tot_ticks += curtimer->left_ticks;
							if (tot_ticks <= diff_tick)
							{
								curtimer->left_ticks = 0;
							}
							else
							{
								curtimer->left_ticks = tot_ticks - diff_tick;
								break;
							}
							curtimer  = curtimer->next;
							cnt ++;
						}
					}
				}
				mqtt_sart_base_timer(g_mqtt_timer.pFirst->left_ticks);
			}
		}
		else
		{
			prevtimer->next = curtimer->next;
		}
		
		if (NULL == g_mqtt_timer.pFree)
		{
			g_mqtt_timer.pFree     = deltimer;
		}
		else 
		{
			g_mqtt_timer.pFreeLast->next = deltimer;
		}
		
		g_mqtt_timer.pFreeLast = deltimer;
		deltimer->next = NULL;
		
		if (0 < g_mqtt_timer.ntimers)
			g_mqtt_timer.ntimers --;

		// delete timer message 
		// to do .......
		
	}
	
	if (0 == g_mqtt_timer.ntimers)
	{
		mqtt_timer_frm_data_reset();
	}
}


kal_uint16 mqtt_start_timer(kal_uint16 timer_id, kal_uint32 milli_seconds, mqtt_timer_cb_t cb, void * arg)
{
	kal_uint16 id       = 0;
	int        existed  = 0;
	kal_uint32 tot_tick = 0;
	kal_uint32 cur_tick = 0;
	kal_uint32 dif_tick = 0;
	kal_uint32 set_sec  = 0;
	kal_uint32 set_msec = 0;

	mqtt_frm_timer_t * curtimer = NULL; //g_mqtt_timer.mqtt_timer;
	mqtt_frm_timer_t * prevtimer = NULL; 
	mqtt_frm_timer_t * pFree = g_mqtt_timer.pFree; 
	
	
	do
	{
		if (NULL == cb)
		{
			break;
		}
		
		set_sec = milli_seconds / 1000;
		set_msec = milli_seconds % 1000;

		cur_tick = 0;
		if (0 < set_sec)
			cur_tick += kal_secs_to_ticks(set_sec);
		if (0 < set_msec)
			cur_tick += kal_milli_secs_to_ticks(set_msec);

		printf("=====mqtt_start_timer()cur_tick=%x=======", cur_tick);
			
		mqtt_stop_timer(timer_id);
		g_mqtt_timer.stop_tick = kal_get_systicks();
		if (NULL == pFree)
		{
			break;
		}

		pFree->timer_id   = timer_id;
		pFree->left_ticks = cur_tick;
		pFree->cb         = cb;
		pFree->arg        = arg;
		pFree->next       = NULL;


		
		if (NULL != g_mqtt_timer.pFirst)
		{
			prevtimer = NULL;
			curtimer  = g_mqtt_timer.pFirst;
			if (NULL != curtimer)
			{
				if (g_mqtt_timer.stop_tick > g_mqtt_timer.start_tick)
					dif_tick = g_mqtt_timer.stop_tick - g_mqtt_timer.start_tick;
				if (dif_tick <= curtimer->left_ticks)
					curtimer->left_ticks -= dif_tick;
				else
					curtimer->left_ticks = 0;
				printf("=====mqtt_start_timer()dif_tick=%x, first_ticks=%x=======", dif_tick, curtimer->left_ticks);
			}

			tot_tick = curtimer->left_ticks;
			
			while((NULL != curtimer->next) && (tot_tick < cur_tick))
			{
				prevtimer = curtimer;
				curtimer  = curtimer->next;
				tot_tick += curtimer->left_ticks;
			}

			
			if ((NULL == prevtimer) && (tot_tick >= cur_tick))
			{
				g_mqtt_timer.pFirst->left_ticks -= cur_tick;				
				pFree->next = g_mqtt_timer.pFirst;
				g_mqtt_timer.pFirst = pFree;
			}
			else if ((NULL == curtimer->next) && (tot_tick <= cur_tick))
			{
				curtimer->next = pFree;
				pFree->left_ticks -= tot_tick;
			}
			else 
			{
				pFree->left_ticks -= (tot_tick - curtimer->left_ticks);
				prevtimer->next = pFree;
				pFree->next = curtimer;
				curtimer->left_ticks -= pFree->left_ticks;
			}
		}
		else
		{
			g_mqtt_timer.pFirst = pFree;
		}
		
		if (g_mqtt_timer.pFree == g_mqtt_timer.pFreeLast)
			g_mqtt_timer.pFreeLast = g_mqtt_timer.pFree->next;
		g_mqtt_timer.pFree  = g_mqtt_timer.pFree->next;

		mqtt_sart_base_timer(g_mqtt_timer.pFirst->left_ticks);
		
		id = timer_id;
	}while(0);

	return id;
}


static void mqtt_timer_ind_print(kal_uint16 timer_id)
{
	switch(timer_id)
	{
	case MQTT_TIMER_ID_MQTT_REQ_TIMEOUT:
		printf("=====mqtt_on_timer() MQTT_TIMER_ID_MQTT_REQ_TIMEOUT=======");
		break;
	case MQTT_TIMER_ID_CONN_TIMEOUT:
		printf("=====mqtt_on_timer() MQTT_TIMER_ID_CONN_TIMEOUT=======");
		break;
	case MQTT_TIMER_ID_SEND_TIMEOUT:
		printf("=====mqtt_on_timer() MQTT_TIMER_ID_SEND_TIMEOUT=======");
		break;
	case MQTT_TIMER_ID_RECV_TIMEOUT:
		printf("=====mqtt_on_timer() MQTT_TIMER_ID_RECV_TIMEOUT=======");
		break;
	case MQTT_TIMER_ID_RECONNECT:
		printf("=====mqtt_on_timer() MQTT_TIMER_ID_RECONNECT=======");
		break;
	}
}


int mqtt_on_timer(kal_uint16 timer_index)
{
	kal_uint32 dif_ticks, tot_ticks;
	int handlered = 0;
	mqtt_frm_timer_t * prevtimer = NULL;
	mqtt_frm_timer_t * curtimer = NULL;
	mqtt_frm_timer_t * hndlrtimer = NULL;
	
	
	do
	{
		handlered = 0;
		if (MQTT_STACK_BACK_TIMER_ID != timer_index)
		{
			break;
		}
		printf("=====mqtt_on_timer() MQTT_STACK_BACK_TIMER_ID=======");
		handlered = 1;
		if (NULL == g_mqtt_timer.pFirst)
		{
			break;
		}
		
		curtimer = g_mqtt_timer.pFirst;
		
		dif_ticks = 0;
		mqtt_stop_base_timer();
		
		if (g_mqtt_timer.stop_tick >= g_mqtt_timer.start_tick)
		{
			dif_ticks = g_mqtt_timer.stop_tick - g_mqtt_timer.start_tick;
			printf("=====mqtt_on_timer()01 dif_ticks=%x=======", dif_ticks);
		}
		else
		{
			if (0 != g_mqtt_timer.start_tick)
			{
				dif_ticks = curtimer->left_ticks;
			}
			printf("=====mqtt_on_timer()02  ticks reverse, dif_ticks=%x=======", dif_ticks);
		}

		hndlrtimer = NULL;
		if (dif_ticks >= curtimer->left_ticks)
		{
			hndlrtimer = curtimer;
			prevtimer = NULL;
			tot_ticks = 0;
			while(NULL != curtimer)
			{
				prevtimer = curtimer;
				tot_ticks += curtimer->left_ticks;
				curtimer = curtimer->next;
				if ((NULL != curtimer) && ((tot_ticks + curtimer->left_ticks) > dif_ticks))
				{
					break;
				}
			}
			
			g_mqtt_timer.pFirst = prevtimer->next;
			prevtimer->next = NULL;
			printf("=====mqtt_on_timer() branch 01, timer event handler=======");
		}
		else
		{
			curtimer->left_ticks -= dif_ticks;
			printf("=====mqtt_on_timer() branch 02, ticks elapse=0x%x=======", dif_ticks);
		}

		if (NULL != g_mqtt_timer.pFirst)
		{
			mqtt_sart_base_timer(g_mqtt_timer.pFirst->left_ticks);
		}
			
		
		if (NULL == hndlrtimer)
		{
			break;
		}

		curtimer = hndlrtimer;
		while(NULL != curtimer)
		{
			prevtimer = curtimer;
			mqtt_timer_ind_print(curtimer->timer_id);
			if (NULL != curtimer->cb)
			{
				curtimer->cb(curtimer->arg);
			}
			curtimer->timer_id = MQTT_INVALID_TIMER_ID;
			curtimer = curtimer->next;
			
			if (0 < g_mqtt_timer.ntimers)
				g_mqtt_timer.ntimers --;
		}

		if (NULL == g_mqtt_timer.pFree)
		{
			g_mqtt_timer.pFree = hndlrtimer;
		}
		else
		{
			g_mqtt_timer.pFreeLast->next = hndlrtimer;
		}
		g_mqtt_timer.pFreeLast = prevtimer;
		prevtimer->next = NULL;
		
		if (NULL == g_mqtt_timer.pFirst)
		{
			mqtt_timer_frm_data_reset();
		}
	}while(0);
		
	return handlered;
}









