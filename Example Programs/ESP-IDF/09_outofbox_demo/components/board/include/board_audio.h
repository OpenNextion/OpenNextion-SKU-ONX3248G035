#ifndef _BOARD_INCLUDE_BOARD_AUDIO_H_
#define _BOARD_INCLUDE_BOARD_AUDIO_H_

#include "driver/i2s_std.h"
#include "driver/gpio.h"




#define NS4168_BCLK     GPIO_NUM_14

#define NS4168_SDATA     GPIO_NUM_15    

#define NS4168_WS_LRCLK   GPIO_NUM_16 


#define I2S_FRE     16000
#define I2S_NUM     I2S_NUM_1


void boardAudioSpeakerInit(void);
void boardAudioSpeakerOff(void);
void boardAudioSpeakerOn(void);
esp_err_t boardAudioSpeakerPlay(const void * play_buf,size_t play_size,size_t *size_written,TickType_t time);

#endif /* _BOARD_INCLUDE_BOARD_AUDIO_H_ */



