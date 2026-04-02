#ifndef _BOARD_INCLUDE_BOARD_RTC_H_
#define _BOARD_INCLUDE_BOARD_RTC_H_

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_log.h"
#include <time.h>
#include <sys/time.h>
#include "board_exio.h"

#define BM85638563_READ_ADDR               0xA3
#define BM85638563_WRITE_ADDR              0xA2

#define BM8563_ALARM_FLAG                  (1<<3)
#define BM8563_TIMER_FLAG                  (1<<2)
#define BM8563_ALARM_INTERRUPT_ENABLE      (1<<1)
#define BM8563_TIMER_INTERRUPT_ENABLE      (1<<0)

#define BM8563_CLKOUT_32768HZ              0b10000000
#define BM8563_CLKOUT_1024HZ               0b10000001
#define BM8563_CLKOUT_32HZ                 0b10000010
#define BM8563_CLKOUT_1HZ                  0b10000011
#define BM8563_CLKOUT_DISABLED             0b00000000

#define BM8563_TIMER_4096HZ                0b10000000
#define BM8563_TIMER_64HZ                  0b10000001
#define BM8563_TIMER_1HZ                   0b10000010
#define BM8563_TIMER_1_60HZ                0b10000011
#define BM8563_TIMER_DISABLED              0b00000011

#define BM8563_DISABLE_ALARM               80


typedef union 
{
    uint32_t save_data;
    uint8_t rtc_data[4];

}SAVE_RTC_BM8563_DATA;



typedef struct {
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t weekday;
} BM8563_Alarm;

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t weekday;
    uint8_t month;
    uint16_t year;
} BM8563_DateTime;


int BM8563_Init(uint8_t mode);

esp_err_t BM8563_Write(uint8_t addr, uint8_t *data, uint8_t count);
esp_err_t BM8563_Read(uint8_t addr, uint8_t *data, uint8_t count);
esp_err_t BM8563_GetLastError();
int BM8563_GetAndClearFlags(void);
int BM8563_SetClockOut(uint8_t mode);
int BM8563_SetTimer(uint8_t mode, uint8_t count);
int BM8563_GetTimer(void);
int BM8563_SetAlarm(BM8563_Alarm *alarm);
int BM8563_GetAlarm(BM8563_Alarm *alarm);
int BM8563_SetDateTime(BM8563_DateTime *dateTime);
int BM8563_GetDateTime(BM8563_DateTime *dateTime);
int BM8563_hctosys();
int BM8563_systohc();
esp_err_t xRtcBm8563I2CInit(void);


esp_err_t set_device_init_flag(uint32_t input_data);
esp_err_t get_rtc_data_from_nvs(uint32_t *input_data);
esp_err_t check_rtc_falsh_sign(void);
esp_err_t will_test_rtc_sign(void);

#endif /* _BOARD_INCLUDE_BOARD_RTC_H_ */

