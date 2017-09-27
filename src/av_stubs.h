#ifndef _AV_STUBS_H_               
#define _AV_STUBS_H_

#include <caml/mlvalues.h>

#include <libavformat/avformat.h>


AVFormatContext * ocaml_av_get_format_context(value *p_av);

/***** AVInputFormat *****/

#define InputFormat_val(v) (*(struct AVInputFormat**)Data_custom_val(v))

void value_of_inputFormat(AVInputFormat *inputFormat, value * p_value);


/***** AVOutputFormat *****/

#define OutputFormat_val(v) (*(struct AVOutputFormat**)Data_custom_val(v))

void value_of_outputFormat(AVOutputFormat *outputFormat, value * p_value);

/***** Control message *****/
value * ocaml_av_get_control_message_callback(struct AVFormatContext *ctx);

void ocaml_av_set_control_message_callback(value *p_av, av_format_control_message c_callback, value *p_ocaml_callback);

#endif // _AV_STUBS_H_ 
