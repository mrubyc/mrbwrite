/*! @file
  @brief
  UART wrapper.

  <pre>
  Copyright (C) 2024- Shimane IT Open-Innovation Center.

  This file is distributed under BSD 3-Clause License.

  </pre>
*/

#include "main.h"
#include "../mrubyc_src/mrubyc.h"
#include "stm32f4_uart.h"

extern UART_HandleTypeDef huart2;

UART_HANDLE * const TBL_UART_HANDLE[/*unit*/] = {
  0, 0,

  // UART2
  &(UART_HANDLE){
    .unit_num = 2,
    .delimiter = '\n',
    .hal_uart = &huart2,
    .rxfifo_size = UART_SIZE_RXFIFO,
  },
};


//================================================================
/*! get the Rx FIFO write position.
*/
static inline int uart_get_wr_pos( const UART_HANDLE *hndl )
{
  return hndl->rxfifo_size - hndl->hal_uart->hdmarx->Instance->NDTR;
}


//================================================================
/*! initialize unit
*/
void uart_init(void)
{
  for( int i = 0; i < sizeof(TBL_UART_HANDLE)/sizeof(UART_HANDLE *); i++ ) {
    UART_HANDLE *hndl = TBL_UART_HANDLE[i];
    if( !hndl ) continue;

    HAL_UART_Receive_DMA(hndl->hal_uart, hndl->rxfifo, hndl->rxfifo_size);
  }
}


//================================================================
/*! set mode

  @memberof UART_HANDLE
  @param  hndl		target UART_HANDLE
  @param  baud		baud rate.
  @param  parity	0:none 1:odd 2:even
  @param  stop_bits	1 or 2
  @note  Pass -1 if you don't need change the parameter.
*/
int uart_setmode( const UART_HANDLE *hndl, int baud, int parity, int stop_bits )
{
  if( baud >= 0 ) {
    hndl->hal_uart->Init.BaudRate = baud;
  }

  switch( parity ) {
  case 0:
    hndl->hal_uart->Init.Parity = UART_PARITY_NONE;
    hndl->hal_uart->Init.WordLength = UART_WORDLENGTH_8B;
    break;
  case 1:
    hndl->hal_uart->Init.Parity = UART_PARITY_ODD;
    hndl->hal_uart->Init.WordLength = UART_WORDLENGTH_9B;
    break;
  case 2:
    hndl->hal_uart->Init.Parity = UART_PARITY_EVEN;
    hndl->hal_uart->Init.WordLength = UART_WORDLENGTH_9B;
    break;
  }

  if( 1 <= stop_bits && stop_bits <= 2 ) {
    static uint32_t const TBL_STOPBITS[] = { UART_STOPBITS_1, UART_STOPBITS_2 };
    hndl->hal_uart->Init.StopBits = TBL_STOPBITS[ stop_bits - 1 ];
  }

  if( HAL_UART_Init( hndl->hal_uart ) != HAL_OK ) return -1;

  return 0;
}


//================================================================
/*! Receive binary data.

  @memberof UART_HANDLE
  @param  hndl		target UART_HANDLE
  @param  buffer	pointer to buffer.
  @param  size		Size of buffer.
  @return int		Num of received bytes.

  @note			If no data received, it blocks execution.
*/
int uart_read( UART_HANDLE *hndl, void *buffer, int size )
{
  uint8_t *buf = buffer;
  int cnt = size;

  while( cnt > 0 ) {
    int ba = uart_bytes_available(hndl);
    if( ba == 0 ) {
      __NOP(); __NOP(); __NOP(); __NOP();
      continue;
    }

    if( ba < cnt ) {
      cnt -= ba;
    } else {
      ba = cnt;
      cnt = 0;
    }

    // copy fifo to buffer
    for( ; ba > 0; ba-- ) {
      *buf++ = hndl->rxfifo[hndl->rx_rd++];
      if( hndl->rx_rd >= hndl->rxfifo_size ) hndl->rx_rd = 0;
    }
  }

  return size;
}


//================================================================
/*! Send out binary data.

  @memberof UART_HANDLE
  @param  hndl		target UART_HANDLE
  @param  buffer	pointer to buffer.
  @param  size		Size of buffer.
  @return		Size of transmitted.
*/
int uart_write( UART_HANDLE *hndl, const void *buffer, int size )
{
  HAL_UART_Transmit( hndl->hal_uart, buffer, size, HAL_MAX_DELAY );

  return size;
}


//================================================================
/*! Receive string.

  @memberof UART_HANDLE
  @param  hndl		target UART_HANDLE
  @param  buffer	pointer to buffer.
  @param  size		Size of buffer.
  @return int		Num of received bytes.

  @note			If no data received, it blocks execution.
*/
int uart_gets( UART_HANDLE *hndl, void *buffer, int size )
{
  uint8_t *buf = buffer;
  int len;

  while( 1 ) {
    len = uart_can_read_line(hndl);
    if( len > 0 ) break;

    __NOP(); __NOP(); __NOP(); __NOP();
  }

  if( len >= size ) return -1;		// buffer size too small.

  // copy fifo to buffer
  for( int ba = len; ba > 0; ba-- ) {
    *buf++ = hndl->rxfifo[hndl->rx_rd++];
    if( hndl->rx_rd >= hndl->rxfifo_size ) hndl->rx_rd = 0;
  }
  *buf = '\0';

  return len;
}


//================================================================
/*! check data can be read.

  @memberof UART_HANDLE
  @param  hndl		target UART_HANDLE
  @return int           result (bool)
*/
int uart_is_readable( const UART_HANDLE *hndl )
{
  return hndl->rx_rd != uart_get_wr_pos( hndl );
}


//================================================================
/*! check data length can be read.

  @memberof UART_HANDLE
  @param  hndl		target UART_HANDLE
  @return int		result (bytes)
*/
int uart_bytes_available( const UART_HANDLE *hndl )
{
  uint16_t rx_wr = uart_get_wr_pos(hndl);

  if( hndl->rx_rd <= rx_wr ) {
    return rx_wr - hndl->rx_rd;
  }
  else {
    return hndl->rxfifo_size - hndl->rx_rd + rx_wr;
  }
}


//================================================================
/*! check data can be read a line.

  @memberof UART_HANDLE
  @param  hndl		target UART_HANDLE
  @return int		string length.
*/
int uart_can_read_line( const UART_HANDLE *hndl )
{
  uint16_t idx   = hndl->rx_rd;
  uint16_t rx_wr = uart_get_wr_pos(hndl);

  while( idx != rx_wr ) {
    if( hndl->rxfifo[idx++] == hndl->delimiter ) {
      if( hndl->rx_rd < idx ) {
	return idx - hndl->rx_rd;
      } else {
	return hndl->rxfifo_size - hndl->rx_rd + idx;
      }
    }
    if( idx >= hndl->rxfifo_size ) idx = 0;
  }

  return 0;
}


//================================================================
/*! Clear receive buffer.

  @memberof UART_HANDLE
  @param  hndl		target UART_HANDLE
*/
void uart_clear_rx_buffer( UART_HANDLE *hndl )
{
  hndl->rx_rd = uart_get_wr_pos( hndl );
}
