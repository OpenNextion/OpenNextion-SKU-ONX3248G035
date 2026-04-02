
#include "esp_system.h"
#include "driver/i2c_master.h"

#include "board_rtc.h"
#include <stdlib.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"


const char *LOG_TAG = "RTC_BMP8563";

static i2c_master_dev_handle_t  rtc_bm8563_i2c_handle = NULL;

static esp_err_t last_i2c_err = ESP_OK; 

SAVE_RTC_BM8563_DATA  static_bm8563_data = {0};



#define EPOCH_YEAR		1970
#define TM_YEAR_BASE	1900


static time_t sub_mkgmt(struct tm *tm)
{
	int y, nleapdays;
	time_t t;
	/* days before the month */
	static const unsigned short moff[12] = {
		0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
	};

	/*
	 * XXX: This code assumes the given time to be normalized.
	 * Normalizing here is impossible in case the given time is a leap
	 * second but the local time library is ignorant of leap seconds.
	 */

	/* minimal sanity checking not to access outside of the array */
	if ((unsigned) tm->tm_mon >= 12)
		return (time_t) -1;
	if (tm->tm_year < EPOCH_YEAR - TM_YEAR_BASE)
		return (time_t) -1;

	y = tm->tm_year + TM_YEAR_BASE - (tm->tm_mon < 2);
	nleapdays = y / 4 - y / 100 + y / 400 -
	    ((EPOCH_YEAR-1) / 4 - (EPOCH_YEAR-1) / 100 + (EPOCH_YEAR-1) / 400);
	t = ((((time_t) (tm->tm_year - (EPOCH_YEAR - TM_YEAR_BASE)) * 365 +
			moff[tm->tm_mon] + tm->tm_mday - 1 + nleapdays) * 24 +
		tm->tm_hour) * 60 + tm->tm_min) * 60 + tm->tm_sec;

	return (t < 0 ? (time_t) -1 : t);
}


time_t timegm(struct tm *tm)
{
	time_t t, t2;
	struct tm *tm2;
	int sec;

	/* Do the first guess. */
	if ((t = sub_mkgmt(tm)) == (time_t) -1)
		return (time_t) -1;

	/* save value in case *tm is overwritten by gmtime() */
	sec = tm->tm_sec;

	tm2 = gmtime(&t);
	if ((t2 = sub_mkgmt(tm2)) == (time_t) -1)
		return (time_t) -1;

	if (t2 < t || tm2->tm_sec != sec) {
		/*
		 * Adjust for leap seconds.
		 *
		 *     real time_t time
		 *           |
		 *          tm
		 *         /	... (a) first sub_mkgmt() conversion
		 *       t
		 *       |
		 *      tm2
		 *     /	... (b) second sub_mkgmt() conversion
		 *   t2
		 *			--->time
		 */
		/*
		 * Do the second guess, assuming (a) and (b) are almost equal.
		 */
		t += t - t2;
		tm2 = gmtime(&t);

		/*
		 * Either (a) or (b), may include one or two extra
		 * leap seconds.  Try t, t + 2, t - 2, t + 1, and t - 1.
		 */
		if (tm2->tm_sec == sec
		    || (t += 2, tm2 = gmtime(&t), tm2->tm_sec == sec)
		    || (t -= 4, tm2 = gmtime(&t), tm2->tm_sec == sec)
		    || (t += 3, tm2 = gmtime(&t), tm2->tm_sec == sec)
		    || (t -= 2, tm2 = gmtime(&t), tm2->tm_sec == sec))
			;	/* found */
		else {
			/*
			 * Not found.
			 */
			if (sec >= 60)
				/*
				 * The given time is a leap second
				 * (sec 60 or 61), but the time library
				 * is ignorant of the leap second.
				 */
				;	/* treat sec 60 as 59,
					   sec 61 as 0 of the next minute */
			else
				/* The given time may not be normalized. */
				t++;	/* restore t */
		}
	}

	return (t < 0 ? (time_t) -1 : t);
}


esp_err_t BM8563_Write(uint8_t addr, uint8_t *data, uint8_t count) 
{
	esp_err_t ret; 
    uint8_t send_buf[257]= {0};
    uint8_t i = 0;
    send_buf[0] = addr;
    for(i = 0; i < count ;i++ )
    {
        send_buf[ 1 + i ] = data[i];
    }
    ret =  i2c_master_transmit(rtc_bm8563_i2c_handle,send_buf,1+count, -1);
    return ret;
}


esp_err_t BM8563_Read(uint8_t addr, uint8_t *data, uint8_t count) {

	esp_err_t ret;
    uint8_t send_buf[1]= {addr};

    ret = i2c_master_transmit_receive(rtc_bm8563_i2c_handle,send_buf,1,data,count,-1);
    return ret;
}

esp_err_t BM8563_GetLastError(){
	return last_i2c_err;
}

#define BinToBCD(bin) ((((bin) / 10) << 4) + ((bin) % 10))

int BM8563_Init(uint8_t mode){
	static bool init = false;
	if(!init){
		uint8_t tmp = 0b00000000;
		esp_err_t ret = BM8563_Write(0x00, &tmp, 1);
		if (ret != ESP_OK){
			return -1;
		}
		mode &= 0b00000000;
		ret = BM8563_Write(0x01, &mode, 1);
		if (ret != ESP_OK){
			return -2;
		}
		init = true;
	}
	return 0;
}

int BM8563_GetAndClearFlags(){
	uint8_t flags;

	esp_err_t ret = BM8563_Read(0x01, &flags, 1);
	if (ret != ESP_OK){
		return -1;
	}
	uint8_t cleared = flags & 0b00010011;
	ret = BM8563_Write(0x01, &cleared, 1);
	if (ret != ESP_OK){
		return -1;
	}

	return flags & 0x0C;
}

int BM8563_SetClockOut(uint8_t mode){

	mode &= 0b10000011;
	esp_err_t ret = BM8563_Write(0x0D, &mode, 1);
	if (ret != ESP_OK) {
		return -1;
	}
	return 0;	
}

//倒计时的玩意 用处不大
int BM8563_SetTimer(uint8_t mode, uint8_t count){

	mode &= 0b10000011;
	esp_err_t ret = BM8563_Write(0x0E, &mode, 1);
	if (ret != ESP_OK) {
		return -1;
	}
	ret = BM8563_Write(0x0F, &count, 1);
	if (ret != ESP_OK) {
		return -1;
	}
	return 0;
}

int BM8563_GetTimer(){
	uint8_t count;

	esp_err_t ret = BM8563_Read(0x0F, &count, 1);
	if (ret != ESP_OK) {
		return -1;
	}
	return (int) count;
}

int BM8563_SetAlarm(BM8563_Alarm *alarm){
	if ((alarm->minute >= 60 && alarm->minute != 80) || (alarm->hour >= 24 && alarm->hour != 80) || (alarm->day > 32 && alarm->day != 80) || (alarm->weekday > 6 && alarm->weekday != 80))
	{
		return -2;
	}

	uint8_t buffer[4];

	buffer[0] = BinToBCD(alarm->minute) & 0xFF;
	buffer[1] = BinToBCD(alarm->hour) & 0xBF;
	buffer[2] = BinToBCD(alarm->day) & 0xBF;
	buffer[3] = BinToBCD(alarm->weekday) & 0x87;

	esp_err_t ret = BM8563_Write(0x09, buffer, sizeof(buffer));
	if (ret != ESP_OK) {
		return -1;
	}

	return 0;
}

int BM8563_GetAlarm(BM8563_Alarm *alarm) {
	uint8_t buffer[4];

	esp_err_t ret = BM8563_Read(0x09, buffer, sizeof(buffer));
	if (ret != ESP_OK) {
		return -1;
	}

	alarm->minute = (((buffer[0] >> 4) & 0x0F) * 10) + (buffer[0] & 0x0F);
	alarm->hour = (((buffer[1] >> 4) & 0x0B) * 10) + (buffer[1] & 0x0F);
	alarm->day = (((buffer[2] >> 4) & 0x0B) * 10) + (buffer[2] & 0x0F);
	alarm->weekday = (((buffer[3] >> 4) & 0x08) * 10) + (buffer[3] & 0x07);

	return 0;
}


//这两个是关键
int BM8563_SetDateTime(BM8563_DateTime *dateTime) {
	if (dateTime->second >= 60 || dateTime->minute >= 60 || dateTime->hour >= 24 || dateTime->day > 32 || dateTime->weekday > 6 || dateTime->month > 12 || dateTime->year < 1900 || dateTime->year >= 2100)
	{
		return -2;
	}

	uint8_t buffer[7];

	buffer[0] = BinToBCD(dateTime->second) & 0x7F;
	buffer[1] = BinToBCD(dateTime->minute) & 0x7F;
	buffer[2] = BinToBCD(dateTime->hour) & 0x3F;
	buffer[3] = BinToBCD(dateTime->day) & 0x3F;
	buffer[4] = BinToBCD(dateTime->weekday) & 0x07;
	buffer[5] = BinToBCD(dateTime->month) & 0x1F;

	if (dateTime->year >= 2000)
	{
		buffer[5] |= 0x80;
		buffer[6] = BinToBCD(dateTime->year - 2000);
	}
	else
	{
		buffer[6] = BinToBCD(dateTime->year - 1900);
	}

	esp_err_t ret = BM8563_Write(0x02, buffer, sizeof(buffer));
	if (ret != ESP_OK) {
		return -1;
	}

	return 0;
}

int BM8563_GetDateTime(BM8563_DateTime *dateTime) {
	uint8_t buffer[7];
    uint8_t i = 0;
	esp_err_t ret;

	ret = BM8563_Read(0x02, buffer, sizeof(buffer));
	// if (ret != ESP_OK) {
	// 	return -1;
	// }

	// for(i = 0 ;i < 7 ;i ++)
	// {
	// 	printf("buffer[%d] = %d\r\n",i,buffer[i]);
	// }

	if (buffer[0] & 0x80) //Clock integrity not guaranted
	{
		dateTime->second = 0;
		dateTime->minute = 0;
		dateTime->hour = 0;
		dateTime->day =  0;
		dateTime->weekday = 0;
		dateTime->month =  0;
		dateTime->year = 0;
		buffer[0] = 0;
		BM8563_Write(0x02, buffer, sizeof(buffer));
		return 0;
	}
	dateTime->second = (((buffer[0] >> 4) & 0x07) * 10) + (buffer[0] & 0x0F);
	dateTime->minute = (((buffer[1] >> 4) & 0x07) * 10) + (buffer[1] & 0x0F);
	dateTime->hour = (((buffer[2] >> 4) & 0x03) * 10) + (buffer[2] & 0x0F);
	dateTime->day = (((buffer[3] >> 4) & 0x03) * 10) + (buffer[3] & 0x0F);
	dateTime->weekday = (buffer[4] & 0x07);
	dateTime->month = ((buffer[5] >> 4) & 0x01) * 10 + (buffer[5] & 0x0F);
	dateTime->year = 1900 + ((buffer[6] >> 4) & 0x0F) * 10 + (buffer[6] & 0x0F);

	if (buffer[5] &  0x80)
	{
		dateTime->year += 100;
	}

	// if (buffer[0] & 0x80) //Clock integrity not guaranted
	// {
	// 	return 1;
	// }

	return 0;
}

int BM8563_hctosys(){
	int ret;
	BM8563_DateTime date = {0};
	struct tm tm = {0};
	struct timeval tv = {0};
	
	ret = BM8563_Init(0);
	printf("BM8563_Init %d\n", ret);
	if (ret != 0) {
		goto fail;
	}
    ret = BM8563_GetDateTime(&date);
	printf("BM8563_GetDateTime %d\n", ret);
    if (ret != 0) {
		goto fail;
    }
	tm.tm_sec = date.second;
	tm.tm_min = date.minute;
	tm.tm_hour = date.hour;
	tm.tm_mday = date.day;
	tm.tm_mon = date.month - 1;
	tm.tm_year = date.year - 1900;

	tv.tv_sec = timegm(&tm);
	tv.tv_usec = 0;
	ret = settimeofday(&tv, NULL);
fail:
	return ret;
}

int BM8563_systohc(){
	int ret;
	BM8563_DateTime date = {0};
	struct tm tm = {0};

	ret = BM8563_Init(0);
	if (ret != 0) {
		goto fail;
	}

	time_t now = time(NULL);
	gmtime_r(&now, &tm);
	date.second = tm.tm_sec;
	date.minute = tm.tm_min;
	date.hour = tm.tm_hour;
	date.day = tm.tm_mday;
	date.month = tm.tm_mon + 1;
	date.year = tm.tm_year + 1900;
	date.weekday = tm.tm_wday;
	ret = BM8563_SetDateTime(&date);
fail:
	return ret;
}


esp_err_t xRtcBm8563I2CInit(void)
{
	
    esp_err_t ret = ESP_OK;
    i2c_master_bus_handle_t NUM_0_handle;
    
    i2c_device_config_t bm8563_i2c_dev_conf = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,  /* 从机地址长度 */
        .scl_speed_hz    = 100000,     /* 传输速率 */
        .device_address  = 0x51,   
    };

    ESP_ERROR_CHECK(i2c_master_get_bus_handle(0, &NUM_0_handle));

    if(NUM_0_handle == NULL)
    {
		printf("NUM_0_handle = null\n");
        return ESP_FAIL;
    }
	ret = i2c_master_bus_add_device(NUM_0_handle,&bm8563_i2c_dev_conf, &rtc_bm8563_i2c_handle);
    return ret;
}




esp_err_t set_device_init_flag(uint32_t input_data)
{
    esp_err_t ret;
    nvs_handle_t nvs_handle;

    //2、打开NVS
    ret = nvs_open("rtc_data", NVS_READWRITE, &nvs_handle);
    if(ret != ESP_OK)
    {
        ESP_LOGI("nvs", "nvs open failed");
        nvs_close(nvs_handle);
        return ret;
    }
    //3、写入数据
    ret = nvs_set_u32(nvs_handle, "up_data", input_data); //写一个uint8_t类型的数据
    if(ret != ESP_OK)
    {
        ESP_LOGI("nvs", "nvs write init flag failed");
        nvs_close(nvs_handle);
        return ret;
    }
    //4、写完提交数据
    ret = nvs_commit(nvs_handle);
    if(ret != ESP_OK)
    {
        ESP_LOGI("nvs", "nvs commit failed");
        nvs_close(nvs_handle);
        return ret;
    }
    //4、关闭NVS
    nvs_close(nvs_handle);
    return ESP_OK;
}



esp_err_t get_rtc_data_from_nvs(uint32_t *input_data)
{
    esp_err_t ret;
    nvs_handle_t nvs_handle;

    //2、打开NVS
    ret = nvs_open("rtc_data", NVS_READONLY, &nvs_handle);
    if(ret != ESP_OK)
    {
        printf("ret = %d\n", ret);
        ESP_LOGI("nvs", "nvs open failed");
        nvs_close(nvs_handle);
        return ret;
    }
 
    //3、读取数据
    ret = nvs_get_u32(nvs_handle, "up_data", input_data);
    if(ret != ESP_OK)
    {
        ESP_LOGI("nvs", "nvs read init flag failed");
        nvs_close(nvs_handle);
        return ret;
    }
    //ESP_LOGI("nvs", "FLAG = %#X", flag);
    printf("FLAG = %ld\n", *input_data);
    //4、关闭NVS
    nvs_close(nvs_handle);
    return ESP_OK;
}




esp_err_t check_rtc_falsh_sign(void)
{
    esp_err_t ret;
    nvs_handle_t nvs_handle;

    //2、打开NVS
    ret = nvs_open("rtc_data", NVS_READONLY, &nvs_handle);
    if(ret != ESP_OK)
    {
        printf("ret = %d\n", ret);
        ESP_LOGI("nvs", "nvs open failed");
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }
 
    //3、读取数据
    ret = nvs_get_u32(nvs_handle, "up_data", &static_bm8563_data.save_data);
    if(ret != ESP_OK)
    {
        ESP_LOGI("nvs", "nvs read init flag failed");
        nvs_close(nvs_handle);
        return ret;
    }

    nvs_close(nvs_handle);

	if(static_bm8563_data.rtc_data[0] == 0x55)
	{
		return ESP_OK;
	}
	else
	{
		return ESP_FAIL;
	}
}


esp_err_t will_test_rtc_sign(void)
{
	if(static_bm8563_data.rtc_data[0] == 0x55)
	{
		return ESP_OK;
	}
	else
	{
		return ESP_FAIL;
	}
}


