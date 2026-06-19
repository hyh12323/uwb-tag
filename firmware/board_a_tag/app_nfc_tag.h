#ifndef APP_NFC_TAG_H__
#define APP_NFC_TAG_H__

#include <stdint.h>
#include "sdk_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_NFC_TAG_DEFAULT_URL "https://hyhcloud.top:2356"

ret_code_t app_nfc_tag_program_url(char const *url);
ret_code_t app_nfc_tag_program_text(char const *text, char const *lang);
void app_nfc_tag_program_default(void);

#ifdef __cplusplus
}
#endif

#endif
