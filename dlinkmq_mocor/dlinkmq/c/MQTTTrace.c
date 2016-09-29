#include "MQTTTrace.h"
#include "sci_log.h"
#include "sci_types.h"
#include "stdio.h"
#include "stdarg.h"

#define MOD_MQTTTRACE	                   MOD_MQTT

#define MQTT_TRACE_MAX_SIZE                (512)
#define MQTT_TRACE_VISUAL_MAX_LEN          (200)
#define MQTT_TRACE_TEXT_MAX_LEN            (2048)
#define MQTT_TRACE_SPLIT_MAX_LEN           (64)

char g_trace_buf[MQTT_TRACE_MAX_SIZE+4];

void mqtt_fmt_print(char *format, ...)
{
	do
	{
	    va_list arg_list;

		if ((NULL == format) || (0 == *format))
			break;

		memset((void*)g_trace_buf, 0, sizeof(g_trace_buf));

	    va_start(arg_list, format);

		snprintf(g_trace_buf, MQTT_TRACE_MAX_SIZE, format, arg_list);
	    g_trace_buf[MQTT_TRACE_MAX_SIZE - 1] = '\0';

	    va_end(arg_list);

		SCI_TRACE_ID(TRACE_INFO,g_trace_buf,(uint8*)"");
	}while(0);
}






















