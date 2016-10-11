#ifndef dlinkmq_upload_h
#define dlinkmq_upload_h


#include "Fs_gprot.h"
#include "dlinkmq_types.h"
#include "MQTTMTK.h"
#include "MQTTTrace.h"
#include "dlinkmq_msg.h"
#include "dlinkmq_api.h"

typedef enum tagSt_DlinkmqUpload_Status{
	DlinkmqUpload_Status_INIT = 0,
	DlinkmqUpload_Status_CONNECTING,
	DlinkmqUpload_Status_CONNECT
};
typedef struct tagSt_DlinkmqUploadPaths
{
	we_char *path;
	dlinkmq_ext_callback cb;
	struct tagSt_DlinkmqUploadPaths *tail;
	struct tagSt_DlinkmqUploadPaths *next;

}St_DlinkmqUploadPaths;

typedef struct tagSt_DlinkmqUpload
{
	Network *pstNetWork;
	we_char *pSendBuff;
	we_int iSentSize;
	we_int iBuffSize;
	we_int status;
	FS_HANDLE file;
	St_DlinkmqUploadPaths *paths;

}St_DlinkmqUpload, *P_St_DlinkmqUpload;

we_int DlinkmqUpload_Init(we_handle *phDlinkmqUploadHandle);

we_void DlinkmqUpload_Destroy(we_handle hDlinkmqHUploadHandle);

we_int DlinkmqUpload_AddUploadParams(we_handle hDlinkmqUploadHandle, we_char* file_path, dlinkmq_ext_callback cb);

#endif /* dlinkmq_http_h */