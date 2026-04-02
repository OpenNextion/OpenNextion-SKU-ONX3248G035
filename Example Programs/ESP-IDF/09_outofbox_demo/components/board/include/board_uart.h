#ifndef _BOARD_INCLUDE_BOARD_UART_H_
#define _BOARD_INCLUDE_BOARD_UART_H_


#define ECHO_TEST_TXD (GPIO_NUM_43)
#define ECHO_TEST_RXD (GPIO_NUM_44)

#define ECHO_TEST_TXD_1 (GPIO_NUM_13)
#define ECHO_TEST_RXD_1 (GPIO_NUM_12)

#define UART0_TX_GPIO GPIO_NUM_43 // UART0_TX → 输出
#define UART0_RX_GPIO GPIO_NUM_44 // UART0_RX → 输出

#define UART1_TX_GPIO GPIO_NUM_13 // UART1_TX → 输入
#define UART1_RX_GPIO GPIO_NUM_12 // UART1_TX → 输入


#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      (UART_NUM_0)
#define ECHO_UART_BAUD_RATE     (115200)
#define ECHO_TASK_STACK_SIZE    (2048)


#define ECHO_UART_PORT_NUM_1    (UART_NUM_1)


#define BUF_SIZE (1024)

void uartTask(void);

esp_err_t uartInit(void);
esp_err_t uartGpioTest(void);


#endif /* _BOARD_INCLUDE_BOARD_UART_H_ */