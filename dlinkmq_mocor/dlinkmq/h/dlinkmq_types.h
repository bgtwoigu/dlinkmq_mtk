//
//  dlinkmq_types.h
//  DlinkMQ_Linux
//
//  Created by »Æ½õ on 16/6/3.
//  Copyright ? 2016Äê AppCan. All rights reserved.
//

#ifndef dlinkmq_types_h
#define dlinkmq_types_h

#ifndef TRUE
#define TRUE          1       /* TRUE  :  Integer value 1 */
#endif

#ifndef FALSE
#define FALSE         0       /* FALSE :  Integer value 0 */
#endif


#ifndef we_uint8
typedef  unsigned char we_uint8;
#endif

#ifndef we_uint16
typedef  unsigned short  we_uint16;
#endif

#ifndef we_uint32
 typedef  unsigned int  we_uint32;
#endif

#ifndef we_char
typedef char we_char;
#endif

#ifndef we_int8
typedef char we_int8;
#endif

#ifndef we_int16
typedef short we_int16;
#endif

#ifndef we_int32
typedef int we_int32;
#endif

#ifndef we_int
typedef int we_int;
#endif

#ifndef we_void
typedef  void  we_void;
#endif

#ifndef we_handle
typedef  void *             we_handle;
#endif

#ifndef we_bool
typedef  unsigned char             we_bool;
#endif


enum dlinkmq_data_type{
	DLINKMQ_DATA_TYPE_BOOL=1,
	DLINKMQ_DATA_TYPE_NUMBER,
	DLINKMQ_DATA_TYPE_STRING
};


#endif /* dlinkmq_types_h */
