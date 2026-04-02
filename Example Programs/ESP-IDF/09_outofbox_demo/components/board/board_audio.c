
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "board_exio.h"
#include "board_audio.h"


static i2s_chan_handle_t tx_handle;

static bool  Speaker_state = false;


void boardAudioSpeakerInit(void)
{
    ESP_ERROR_CHECK (expandIoControlIo(I2S_CTRL,EXPAND_IO_LEVEL_H));

    i2s_chan_config_t tx_handle_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM,I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_handle_cfg,&tx_handle,NULL));
    //初始化i2s
    i2s_std_config_t tx_std_cfg = {
            .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(I2S_FRE),
            .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_MONO),
            .gpio_cfg = {
                    .mclk = -1,    // some codecs may require mclk signal, this example doesn't need it
                    .bclk = NS4168_BCLK,
                    .ws   = NS4168_WS_LRCLK,
                    .dout = NS4168_SDATA,
                    .din  = -1,
                    .invert_flags = {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv   = false,
                    },
            },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &tx_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    Speaker_state = true;
}

void boardAudioSpeakerOn(void)
{
    if((tx_handle != NULL) && (Speaker_state == false))
    {
        ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
        Speaker_state =   true;  
        printf("\r\n\r\n **** boardAudioSpeakerOn **** \r\n\r\n");
    }
}
void boardAudioSpeakerOff(void)
{
    if((tx_handle != NULL) && (Speaker_state == true))
    {
        ESP_ERROR_CHECK(i2s_channel_disable(tx_handle));
        Speaker_state =   false;  
        printf("\r\n\r\n **** boardAudioSpeakerOff **** \r\n\r\n");
    }
}

esp_err_t boardAudioSpeakerPlay(const void * play_buf,size_t play_size,size_t *size_written,TickType_t time)
{
    if((play_buf != NULL) && (size_written != NULL) && (tx_handle != NULL) && (Speaker_state == true))
    {
        return i2s_channel_write(tx_handle, play_buf, play_size, size_written, time);
    }
    return ESP_FAIL;
}