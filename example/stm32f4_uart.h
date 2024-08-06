/*! @file
  @brief
  UART wrapper.

  <pre>
  Copyright (C) 2024- Shimane IT Open-Innovation Center.

  This file is distributed under BSD 3-Clause License.

  </pre>
*/

#ifndef STM32F4_UART_H
#define STM32F4_UART_H

//@cond
#include <stdint.h>
//@endcond

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UART_SIZE_RXFIFO
#define UART_SIZE_RXFIFO 1024
#endif

/*!@brief
  UART Handle
*/
typedef struct UART_HANDLE {
  uint8_t unit_num;		//!< UART unit number 1..
  uint8_t delimiter;		//!< line delimiter such as '\\n'
  uint16_t rx_rd;		//!< index of rxfifo for read.

  UART_HandleTypeDef *hal_uart;		//!< STM32 HAL library UART handle.
  int rxfifo_size;			//!< FIFO size
  uint8_t rxfifo[UART_SIZE_RXFIFO];	//!< FIFO for received data.

} UART_HANDLE;

extern UART_HANDLE * const TBL_UART_HANDLE[];


//! Defines the UART to be used for the console.
#define UART_HANDLE_CONSOLE TBL_UART_HANDLE[2]


/*
  function prototypes.
*/
void uart_init(void);
int uart_setmode(const UART_HANDLE *hndl, int baud, int parity, int stop_bits);
int uart_read(UART_HANDLE *hndl, void *buffer, int size);
int uart_write(UART_HANDLE *hndl, const void *buffer, int size);
int uart_gets(UART_HANDLE *hndl, void *buffer, int size);
int uart_is_readable(const UART_HANDLE *hndl);
int uart_bytes_available(const UART_HANDLE *hndl);
int uart_can_read_line(const UART_HANDLE *hndl);
void uart_clear_rx_buffer(UART_HANDLE *hndl);


#ifdef __cplusplus
}
#endif
#endif
