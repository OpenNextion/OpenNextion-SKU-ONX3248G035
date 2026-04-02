#ifndef  _BOARD_INCLUDE_BOARD_WIFI_H_
#define  _BOARD_INCLUDE_BOARD_WIFI_H_

#include "lvgl.h"


void vWifiSTAMode(void);

//返回 0X55为异常  否则为正常
signed char wifiGetFactoryRssi(void);


#endif /* _BOARD_INCLUDE_BOARD_WIFI_H_ */
