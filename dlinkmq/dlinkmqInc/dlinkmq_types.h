//
//  dlinkmq_types.h
//  DlinkMQ_Linux
//
//  Created by »Æ½õ on 16/6/3.
//  Copyright ? 2016Äê AppCan. All rights reserved.
//

#ifndef dlinkmq_types_h
#define dlinkmq_types_h

typedef unsigned char  we_uint8;
typedef unsigned short we_uint16;
typedef unsigned int   we_uint32;
typedef char  we_int8;
typedef short we_int16;
typedef int   we_int32;
typedef void   we_void;


enum dlinkmq_data_type{
	DLINKMQ_DATA_TYPE_BOOL=1,
	DLINKMQ_DATA_TYPE_NUMBER,
	DLINKMQ_DATA_TYPE_STRING
};


#endif /* dlinkmq_types_h */
