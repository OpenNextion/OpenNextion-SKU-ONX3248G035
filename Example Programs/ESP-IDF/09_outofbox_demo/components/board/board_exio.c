#include "board_exio.h"
#include "esp_log.h"
static volatile uint8_t expand_now_io_state = 0XFF;


#define IO_INIT_SUCCESS  (0X55)
#define IO_INIT_ERROR    (0XAA)

static uint8_t init_pin_sign = IO_INIT_ERROR;




const char *pcf8574_tag = "pcf8574";
static volatile uint8_t pcf8574_now_io_state = 0;

static i2c_master_dev_handle_t pcf8574_i2c_handle = NULL;



esp_err_t pcf8574ControlIO(uint8_t ControlIO, uint8_t level)
{
    esp_err_t ret;
    uint8_t cmd[1] = {ControlIO};

    printf("pcf8574_ControlIO = 0x%x  level = %d\r\n ",ControlIO,level);
    ret = i2c_master_transmit(pcf8574_i2c_handle, cmd, 1, -1);
    return ret;
}



esp_err_t pcf8574ControlIOAllH(void)
{
    esp_err_t ret;
    uint8_t cmd[1] = {0x55};
    pcf8574_now_io_state  = 0x55;
    printf("pcf8574_now_io_state = 0x%x\r\n",pcf8574_now_io_state);

    ret = i2c_master_transmit(pcf8574_i2c_handle, cmd, 1, -1);

    return ret;
}


esp_err_t pcf8574ControlIOAllL(void)
{
    esp_err_t ret;
    uint8_t cmd[1] = {0x00};
    pcf8574_now_io_state  = 0x00;
    printf("pcf8574_now_io_state = 0x%x\r\n",pcf8574_now_io_state);

    ret = i2c_master_transmit(pcf8574_i2c_handle, cmd, 1, -1);

    return ret;
}


esp_err_t pcf8574ControlReadAllIo(uint8_t *io_state)
{
    esp_err_t ret;

    ret = i2c_master_transmit(pcf8574_i2c_handle, io_state, 1, -1);
    pcf8574_now_io_state = *io_state;
    printf("pcf8574_now_io_state = %d\r\n",pcf8574_now_io_state);

    return ret;
}





esp_err_t pcf8574ReadIO(uint8_t *io_state)
{
    esp_err_t ret;
    ret = i2c_master_receive(pcf8574_i2c_handle, io_state, 1, -1);
    return ret;
}





esp_err_t xPCF8574Init(void)
{
    uint8_t r_data[2];
    esp_err_t ret = 0;
    uint16_t cmd;
    i2c_master_bus_handle_t NUM_0_handle;
    
    //PCF8574 居然没地址
    i2c_device_config_t pcf8574_i2c_dev_conf = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,  /* 从机地址长度 */
        .scl_speed_hz    = PCF8574_IIC_SPEED,     /* 传输速率 */
        .device_address  = PCF8574_ADDR,   
    };

    ESP_ERROR_CHECK(i2c_master_get_bus_handle(0, &NUM_0_handle));

    if(NUM_0_handle != NULL)
    {
        ESP_ERROR_CHECK(i2c_master_bus_add_device(NUM_0_handle,&pcf8574_i2c_dev_conf, &pcf8574_i2c_handle));
    }

    /* I2C总线上添加XL9555设备 */
    

    if(pcf8574_i2c_handle == NULL  )
    {
        printf("pcf8574_i2c_handle ERROR\r\n");
        ESP_LOGE(pcf8574_tag, "%s 111  ERROR", __func__);    
    }

    // //SD这个引脚必须拉高
    // ret  = pcf8574ControlIO(PCF8574_IO6_SDCS,PCF_CONTROLEVEL_H);
    // //CAM这个引脚必须拉低
    // ret  = pcf8574ControlIO(PCF8574_IO3_CAM_PWDN,PCF_CONTROLEVEL_L); 
    // ret  = pcf8574ControlIO(PCF8574_IO0_I2S_CTRL,PCF_CONTROLEVEL_H);

    return ret;
}





esp_err_t expandIoInit(void)
{
    esp_err_t err = ESP_FAIL;

    #if EXPAND_IO_CH422
        if(xCh422Init() == ESP_OK) 
        {
            init_pin_sign = IO_INIT_SUCCESS;
            return ESP_OK;
        }
    #endif  

    #if EXPAND_IO_PCF8574
        if(xPCF8574Init() == ESP_OK) 
        {
            init_pin_sign = IO_INIT_SUCCESS;
            return ESP_OK;
        }
    #endif

    return err;
}


esp_err_t expandIoControlIo(ExpandIoPin io,ExpandIoLevel level)
{
    esp_err_t err = ESP_FAIL;
    uint8_t i = 0x01;

    if(level == EXPAND_IO_LEVEL_H)
    { 
        expand_now_io_state |= (i << (io - 1));
    }
    else
    {
        expand_now_io_state &= (~(i <<  (io - 1)));
    }
   
    #if EXPAND_IO_CH422
        err = xCh422ControlIO(expand_now_io_state,level);
    #endif  

    #if EXPAND_IO_PCF8574
        err = pcf8574ControlIO(expand_now_io_state,level);
    #endif
    return err;
}


ExpandIoLevel expandIoReadIo(ExpandIoPin io)
{
    uint8_t i = (0x01 <<  (io - 1)); 
    uint8_t read_io_state = 0;

    //不支持读
    #if EXPAND_IO_CH422
        return   PCF_CONTROLEVEL_ERR;
    #endif  

    #if EXPAND_IO_PCF8574

        if( pcf8574ReadIO(&read_io_state) == ESP_OK)
        {
            if(read_io_state & i )
            {
                return EXPAND_IO_LEVEL_H;
            }
            else
            {
                return EXPAND_IO_LEVEL_L;
            }
        }
        else
        {
            return   EXPAND_IO_LEVEL_ERR;
        }

    #endif

    return   EXPAND_IO_LEVEL_ERR;
}



