

#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "driver/i2s_pdm.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "sdkconfig.h"
#include "ns4168.h"
#include <math.h>



#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#include "microphone.h"

static const char *TAG = "pdm_rec_example";

#define CONFIG_EXAMPLE_BIT_SAMPLE 16
#define CONFIG_EXAMPLE_SAMPLE_RATE  44100
#define SPI_DMA_CHAN        SPI_DMA_CH_AUTO
#define NUM_CHANNELS        (1) // For mono recording only!
#define SD_MOUNT_POINT      "/sdcard"
#define SAMPLE_SIZE         (CONFIG_EXAMPLE_BIT_SAMPLE * 1024)
#define BYTE_RATE           (CONFIG_EXAMPLE_SAMPLE_RATE * (CONFIG_EXAMPLE_BIT_SAMPLE / 8)) * NUM_CHANNELS


#define EXAMPLE_PDM_RX_CLK_IO           GPIO_NUM_19                // I2S PDM RX clock io number
#define EXAMPLE_PDM_RX_DIN_IO           GPIO_NUM_20                // I2S PDM RX data in io number
#define EXAMPLE_PDM_RX_FREQ_HZ          16000                      // I2S PDM RX frequency

static int16_t i2s_readraw_buff[SAMPLE_SIZE];
const int WAVE_HEADER_SIZE = 44;
size_t bytes_read;
i2s_chan_handle_t rx_chan  = NULL;        // I2S rx channel handler



static i2s_chan_handle_t i2s_example_init_pdm_rx(void)
{
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &rx_chan));

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(CONFIG_EXAMPLE_SAMPLE_RATE),
        /* The data bit-width of PDM mode is fixed to 16 */
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = EXAMPLE_PDM_RX_CLK_IO,
            .din = EXAMPLE_PDM_RX_DIN_IO,
            .invert_flags = {
                .clk_inv = false,
            },
        }
    };

    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_chan, &pdm_rx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
    return rx_chan;
}





//rec_time 录制的时间
void record_wav(uint32_t rec_time)
{
    // Use POSIX and C standard library functions to work with files.
    int flash_wr_size = 0;
    ESP_LOGI(TAG, "Opening file");

    i2s_chan_handle_t rx_chan = i2s_example_init_pdm_rx();

    uint32_t flash_rec_time = BYTE_RATE * rec_time;
    const wav_header_t wav_header =
        WAV_HEADER_PCM_DEFAULT(flash_rec_time, 16, CONFIG_EXAMPLE_SAMPLE_RATE, 1);

    // First check if file exists before creating a new file.
    struct stat st;
    if (stat(SD_MOUNT_POINT"/record.wav", &st) == 0) {
        // Delete it if it exists
        unlink(SD_MOUNT_POINT"/record.wav");
    }

    // Create new WAV file
    FILE *f = fopen(SD_MOUNT_POINT"/record.wav", "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    // Write the header to the WAV file
    fwrite(&wav_header, sizeof(wav_header), 1, f);

    // Start recording
    while (flash_wr_size < flash_rec_time) {
        // Read the RAW samples from the microphone
        if (i2s_channel_read(rx_chan, (char *)i2s_readraw_buff, SAMPLE_SIZE, &bytes_read, 1000) == ESP_OK) {
            printf("[0] %d [1] %d [2] %d [3]%d ...\n", i2s_readraw_buff[0], i2s_readraw_buff[1], i2s_readraw_buff[2], i2s_readraw_buff[3]);
            // Write the samples to the WAV file
            fwrite(i2s_readraw_buff, bytes_read, 1, f);
            flash_wr_size += bytes_read;
        } else {
            printf("Read Failed!\n");
        }
    }

    ESP_LOGI(TAG, "Recording done!");
    fclose(f);
    ESP_LOGI(TAG, "File written on SDCard");
    ESP_LOGI(TAG, "Card unmounted");
}