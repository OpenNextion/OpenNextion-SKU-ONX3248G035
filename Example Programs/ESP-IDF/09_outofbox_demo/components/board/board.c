#include <stdio.h>
#include "board.h"


static i2c_master_bus_handle_t esp32_i2c_bus_num0_handle = NULL;
static i2c_master_bus_handle_t esp32_i2c_bus_num1_handle = NULL;


void boardIIcAddDevToNum0(void)
{
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,   /* 时钟源 */
        .i2c_port = I2C_NUM_0,               /* I2C端口 */
        .scl_io_num = NEXTION_IIC_SCL_PIN,       /* SCL管脚 */    
        .sda_io_num = NEXTION_IIC_SDA_PIN,        /* SDA管脚 */
        .glitch_ignore_cnt = 7,              /* 故障周期 */
        .flags.enable_internal_pullup  = true  /* 内部上拉 */
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &esp32_i2c_bus_num0_handle));
}
  



void boardIIcAddDevToNum1(void)
{
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,   /* 时钟源 */
        .i2c_port = I2C_NUM_1,               /* I2C端口 */
        .scl_io_num = NEXTION_IIC_SCL_PIN,       /* SCL管脚 */    
        .sda_io_num = NEXTION_IIC_SDA_PIN,        /* SDA管脚 */
        .glitch_ignore_cnt = 7,              /* 故障周期 */
        .flags.enable_internal_pullup  = true  /* 内部上拉 */
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &esp32_i2c_bus_num0_handle));
}
  




esp_err_t boardSpiInit(spi_host_device_t SPI_ID,
                        gpio_num_t miso_pin,
                        gpio_num_t mosi_pin,
                        gpio_num_t sclk_pin,
                        int clk_hz )
{
    spi_bus_config_t buscfg = {
        .sclk_io_num     = sclk_pin,    /* 时钟引脚 */
        .mosi_io_num     = mosi_pin,    /* 主机输出从机输入引脚 */
        .miso_io_num     = -1,    /* 主机输入从机输出引脚 */
        .quadwp_io_num   = -1,              /* 用于Quad模式的WP引脚,未使用时设置为-1 */
        .quadhd_io_num   = -1,              /* 用于Quad模式的HD引脚,未使用时设置为-1 */
        .max_transfer_sz = clk_hz   /* 最大传输大小 */
    };
    /* 初始化SPI总线 */
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_ID, &buscfg, SPI_DMA_CH_AUTO));

    return ESP_OK;
}

