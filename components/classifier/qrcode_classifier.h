#pragma once


#ifdef __cplusplus
extern "C" {
#endif

void classifier_init(void);
const char* classifier_get_pic_from_qrcode_data(const char* text);

#ifdef __cplusplus
}
#endif