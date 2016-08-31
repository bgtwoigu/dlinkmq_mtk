
#if !defined(__MQTT_TRACE_H__)
#define __MQTT_TRACE_H__


#define U8_UINT(n)   ((kal_uint32)(kal_uint8)(n))
#define S8_INT(n)    ((kal_int32)(kal_int8)(n))
#define U16_UINT(n)  ((kal_uint32)(kal_uint16)(n))
#define S16_INT(n)   ((kal_int32)(kal_int16)(n))



void mqtt_trace_hex_data(char* pdat, int size);
void mqtt_text_split_printf(char* pdat, int size);
void mqtt_fmt_print(char *format, ...);

#undef printf
#define printf  mqtt_fmt_print

#undef sect_printf
#define sect_printf  mqtt_text_split_printf

#endif /*__MQTT_TRACE_H__*/



