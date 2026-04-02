
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "board_uart.h"


static const char *TAG = "UART TEST";
static uint8_t usart_start_test = 1;



static void echo_task(void *arg)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));


    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM_1, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM_1, ECHO_TEST_TXD_1, ECHO_TEST_RXD_1, ECHO_TEST_RTS, ECHO_TEST_CTS));

//     // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    uint8_t *data1 = (uint8_t *) malloc(BUF_SIZE);
    usart_start_test = 1;
    while (1) {
        // Read data from the UART
        int len = uart_read_bytes(ECHO_UART_PORT_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
        int len1 = uart_read_bytes(ECHO_UART_PORT_NUM_1, data1, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);

        // Write data back to the UART
        if (len) {
            uart_write_bytes(ECHO_UART_PORT_NUM_1, (const char *) data, len);
        }
        if (len1) {
            uart_write_bytes(ECHO_UART_PORT_NUM, (const char *) data1, len1);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
        if (usart_start_test == 0)
        {
            vTaskDelete(NULL); 
        } 
    }
}


esp_err_t uartInit(void)
{
    esp_err_t err = ESP_OK;

    gpio_config_t uart0_conf = {
        .pin_bit_mask = (1ULL << UART0_TX_GPIO) | (1ULL << UART0_RX_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    if ((err = gpio_config(&uart0_conf)) != ESP_OK) {
        return err;
    }

    gpio_config_t uart1_first_conf = {
        .pin_bit_mask = (1ULL << UART1_TX_GPIO) | (1ULL << UART1_RX_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,    
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    if ((err = gpio_config(&uart1_first_conf)) != ESP_OK) {
        return err;
    }
    return err;
}



esp_err_t uartGpioTest(void) 
{
    esp_err_t err;
    bool tx_level,rx_level;
    uint8_t i = 0;

    gpio_set_level(UART0_TX_GPIO, 1);  
    gpio_set_level(UART0_RX_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(50));

    for(i = 0 ; i < 5 ;i++)
    {
        //这个应该一直为0 RX 接 RX 
        if(gpio_get_level(UART1_RX_GPIO) != 0)
        {
            return ESP_FAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    for(i = 0 ; i < 5 ;i++)
    {
        //这个应该一直为0 RX 接 RX 
        if(gpio_get_level(UART1_TX_GPIO) != 1)
        {
            return ESP_FAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    gpio_set_level(UART0_TX_GPIO, 0);  
    gpio_set_level(UART0_RX_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    
    for(i = 0 ; i < 5 ;i++)
    {
        //这个应该一直为0 RX 接 RX 
        if(gpio_get_level(UART1_RX_GPIO) != 1)
        {
            return ESP_FAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    for(i = 0 ; i < 5 ;i++)
    {
        //这个应该一直为0 RX 接 RX 
        if(gpio_get_level(UART1_TX_GPIO) != 0)
        {
            return ESP_FAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return ESP_OK;
}
