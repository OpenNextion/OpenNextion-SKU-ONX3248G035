#ifndef  _BOARD_INCLUDE_BOARD_SDCARD_H_
#define  _BOARD_INCLUDE_BOARD_SDCARD_H_

#include <stdio.h>

void esp_sdcard_port_init(void);
uint64_t esp_sdcard_port_get_size(void);
void lv_fs_fatfs_init(void);

#endif /* _BOARD_INCLUDE_BOARD_SDCARD_H_ */
