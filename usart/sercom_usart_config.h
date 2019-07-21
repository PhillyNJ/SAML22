/*
 * sercom_usart_config.h
 *
 * Created: 1/23/2018 7:45:03 AM
 *  Author: pvallone
 */ 


#ifndef SERCOM_USART_CONFIG_H_
#define SERCOM_USART_CONFIG_H_

#include <asf.h>
uint8_t usart_read_buffer;


#define SERCOMMODULE	SERCOM2
#define SERCOMMUX		USART_RX_3_TX_2_XCK_3
#define SERCOMPAD0		EDBG_CDC_SERCOM_PINMUX_PAD0
#define SERCOMPAD1		EDBG_CDC_SERCOM_PINMUX_PAD1
#define SERCOMPAD2		EDBG_CDC_SERCOM_PINMUX_PAD2
#define SERCOMPAD3		EDBG_CDC_SERCOM_PINMUX_PAD3


struct usart_module usart_instance;

void configure_console(uint32_t baud);
void serialRead(uint16_t *const buffer);

#endif /* SERCOM_USART_CONFIG_H_ */