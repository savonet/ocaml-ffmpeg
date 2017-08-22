#ifndef _AV_STUBS_H_               
#define _AV_STUBS_H_

#include <caml/mlvalues.h>

#include <libavformat/avformat.h>


/***** AVInputFormat *****/

#define InputFormat_val(v) (*(struct AVInputFormat**)Data_custom_val(v))

void value_of_inputFormat(AVInputFormat *inputFormat, value * pvalue);


/***** AVOutputFormat *****/

#define OutputFormat_val(v) (*(struct AVOutputFormat**)Data_custom_val(v))

void value_of_outputFormat(AVOutputFormat *outputFormat, value * pvalue);

#endif // _AV_STUBS_H_ 
