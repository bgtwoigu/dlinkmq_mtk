

#include "mmi_platform.h"
#include "kal_public_api.h"

#include "kal_public_defs.h" 
#include "stack_ltlcom.h"


#include "syscomp_config.h"
#include "task_config.h"        /* Task creation */

#include "stacklib.h"           /* Basic type for dll, evshed, stacktimer */
#include "stack_timer.h"        /* Stack timer */

#include "kal_trace.h"
#include "kal_debug.h"


#include "MQTTTrace.h"


#define MOD_MQTTTRACE	                   MOD_MQTT

#define MQTT_TRACE_MAX_SIZE                (512)
#define MQTT_TRACE_VISUAL_MAX_LEN          (200)
#define MQTT_TRACE_TEXT_MAX_LEN            (2048)
#define MQTT_TRACE_SPLIT_MAX_LEN           (64)

char g_trace_buf[MQTT_TRACE_MAX_SIZE+4];

void mqtt_trace_hex_data(char* pdat, int size)
{
	int len = 0;
	int i   = 0;
	int ilen= 0;
	int rsize   = size;
	char * buf = (char*)g_trace_buf;
	do
	{
		if ((NULL == pdat) || (0 >= size))
		{
			break;
		}
		
		if (MQTT_TRACE_VISUAL_MAX_LEN < size)
			size = MQTT_TRACE_VISUAL_MAX_LEN;
		
		len = 0;
		memset((void*)g_trace_buf, 0, sizeof(g_trace_buf));

		kal_prompt_trace(MOD_MQTTTRACE, "\r\n=================size=%d===========================\r\n", size);
		//kal_prompt_trace(MOD_MQTTTRACE, pdat);
		for (i=0; i<size; i++)
		{
		#if 1
			if ((0x20 <= U8_UINT(pdat[i])) && (127 > U8_UINT(pdat[i])))
			{
				if (3 == (i&3))
				{
					sprintf(&buf[len], "_[%c-%02x]_", U8_UINT(pdat[i]), pdat[i]);
				}
				else
				{
					sprintf(&buf[len], "_[%c-%02x]", U8_UINT(pdat[i]), pdat[i]);
				}
			}
			else
			{
				if (3 == (i&3))
				{
					sprintf(&buf[len], "_%02x_", U8_UINT(pdat[i]), pdat[i]);
				}
				else
				{
					sprintf(&buf[len], "_%02x", U8_UINT(pdat[i]), pdat[i]);
				}
			}
			len += strlen(&buf[len]);
			ilen = 1;
			i += (ilen - 1);
			
			if ((11 == (i%12)) || ((i+1) >= size))
			{
				kal_prompt_trace(MOD_MQTTTRACE, "\r\n--------------------------------------------\r\n");
				kal_prompt_trace(MOD_MQTTTRACE, buf);
				memset((void*)g_trace_buf, 0, sizeof(g_trace_buf));
				buf = (char*)g_trace_buf;
				len = 0;
				//kal_prompt_trace(MOD_MQTTTRACE, "\r\n");
			}
		#else
			if ((i+4) <= size)
			{
				sprintf(&buf[len], "_%02x_%02x_%02x_%02x_", U8_UINT(pdat[i]), U8_UINT(pdat[i+1]), U8_UINT(pdat[i+2]), U8_UINT(pdat[i+3]));
				ilen = 4;
			}
			else if ((i+3) <= size)
			{
				sprintf(&buf[len], "_%02x_%02x_%02x_", U8_UINT(pdat[i]), U8_UINT(pdat[i+1]), U8_UINT(pdat[i+2]));
				ilen = 3;
			}
			else if ((i+2) <= size)
			{
				sprintf(&buf[len], "_%02x_%02x_", U8_UINT(pdat[i]), U8_UINT(pdat[i+1]));
				ilen = 2;
			}
			else 
			{
				sprintf(&buf[len], "_%02x_", U8_UINT(pdat[i]));
				ilen = 1;
			}
			
			len += strlen(&buf[len]);
			i += (ilen - 1);
			
			if ((MQTT_TRACE_SPLIT_MAX_LEN <= len) || ((i+1) >= size))
			{
				kal_prompt_trace(MOD_MQTTTRACE, "\r\n--------------------------------------------\r\n");
				kal_prompt_trace(MOD_MQTTTRACE, buf);
				memset((void*)g_trace_buf, 0, sizeof(g_trace_buf));
				buf = (char*)g_trace_buf;
				len = 0;
				//kal_prompt_trace(MOD_MQTTTRACE, "\r\n");
			}
		#endif

		}
		
		if (rsize > size)
		{
			kal_prompt_trace(MOD_MQTTTRACE, "\r\n===========....................==========\r\n");
		}
		kal_prompt_trace(MOD_MQTTTRACE, "\r\n============================================\r\n");
	}while(0);
}


void mqtt_text_split_printf(char* pdat, int size)
{
	int len = 0;
	int i   = 0;
	int ilen= 0;
	int rsize;
	int tsize;
	char * buf = (char*)g_trace_buf;
	
	do
	{
		if (NULL == pdat)
			break;

		if (0 >= size)
		{
			len = strlen((char*)pdat);
			if (0 >= len)
			{
				break;
			}
		}
		else
		{
			len = size;
		}
		ilen = len;
		if (MQTT_TRACE_TEXT_MAX_LEN <= len)
			len = MQTT_TRACE_TEXT_MAX_LEN;
		
		kal_prompt_trace(MOD_MQTTTRACE, "\r\n=======split text=len=%d.limit_len=%d, ===begin=======\r\n", ilen, len);
		
		tsize = 0;
		while(tsize < len)
		{
			buf[0] = 0;
			if ((tsize+MQTT_TRACE_SPLIT_MAX_LEN) <= len)
				rsize = MQTT_TRACE_SPLIT_MAX_LEN;
			else
				rsize = len - tsize;
			
			strncpy(buf, (char*)&pdat[tsize], rsize);
			buf[rsize] = 0;
			
			kal_prompt_trace(MOD_MQTTTRACE, buf);
			
			tsize += rsize;
		}
		kal_prompt_trace(MOD_MQTTTRACE, "\r\n===========split text============end==========\r\n");
		
	}while(0);
}


void mqtt_fmt_print(char *format, ...)
{
	do
	{
	    va_list arg_list;

		if ((NULL == format) || (0 == *format))
			break;

		memset((void*)g_trace_buf, 0, sizeof(g_trace_buf));

	    va_start(arg_list, format);

		#ifdef __MTK_TARGET__
			vsnprintf(g_trace_buf, MQTT_TRACE_MAX_SIZE, format, arg_list);
		#else 
		    _vsnprintf(g_trace_buf, MQTT_TRACE_MAX_SIZE, format, arg_list);
		#endif 

	    g_trace_buf[MQTT_TRACE_MAX_SIZE - 1] = '\0';

	    va_end(arg_list);

		#ifdef __MTK_TARGET__
			kal_prompt_trace(MOD_MQTTTRACE, g_trace_buf);
		#else 
			kal_printf("\n%s", g_trace_buf);
		#endif 
	}while(0);
}






















