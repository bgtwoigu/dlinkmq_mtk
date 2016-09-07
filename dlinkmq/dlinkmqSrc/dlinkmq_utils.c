#include "dlinkmq_utils.h"
#include "MQTTMTK.h"


we_int32 dlinkmq_data_A_To_B(we_int8 *data, we_int32 indexA, we_int32 A, we_int32 B, we_int8 *out_data){
	we_int32 ret=-1;
	we_int32 dataIndex=0;
	we_int32 numA=0;
	we_int32 i;

	if (strlen(data)>30){
		return ret;
	}

	for(i=0; i<strlen(data); i++){
		if(numA==indexA && data[i]==B){
			return 0;
		}
		if(numA==indexA){
			out_data[dataIndex]=data[i];
			dataIndex++;

			if(i==strlen(data)-1){
				return 0;
			}
		}
		if(data[i]==A){
			numA++;
		}

	}
	return ret;
}
