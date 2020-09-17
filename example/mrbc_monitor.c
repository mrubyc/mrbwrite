/*! @file
  @brief
  mruby/c monitor (IREP writer) for Cypress CY8CKIT-059, PSoC5LP
  using USBUART.

  <pre>
  Copyright (C) 2018-2020 Kyushu Institute of Technology.
  Copyright (C) 2018-2020 Shimane IT Open-Innovation Center.

  This file is distributed under BSD 3-Clause License.

  (usage)
  in main program.

  const uint8_t CYCODE mruby_bytecode[MRBC_SIZE_IREP_STRAGE];  // Flash ROM

  if( mrbc_monitor_or_exec() ) mrbc_monitor_run();
  mrubyc_start();

  </pre>
*/


#include <project.h>
#include <stdlib.h>

#include "mrbc_monitor.h"

#define MRUBYC_VERSION_STRING   "mruby/c v2.1 PSoC5LP"
#define USBUART_BUFFER_SIZE 64	// max 64bytes. see USBFS manual.



//================================================================
/*! USBUART read buffer.
*/
static struct {
  uint8_t buf[USBUART_BUFFER_SIZE * 2 + 1];
  uint8_t flag_can_read_line;
  uint8_t *p_buf_w;
} usbuart_;


//================================================================
/*! tiny int to string conversion.
*/
static char *tiny_itoa( char *buf, int n )
{
  char *p1 = buf;

  do {
    int d = n % 10;
    *p1++ = '0' + d;
    n /= 10;
  } while( n != 0 );

  *p1-- = 0;

  char *p2 = buf;
  while( p2 < p1 ) {
    int ch = *p2;
    *p2++ = *p1;
    *p1-- = ch;
  }

  return buf;
}



//================================================================
/*! USBUART buffer clear.
*/
void usbuart_clear(void)
{
  usbuart_.p_buf_w = usbuart_.buf;
  usbuart_.flag_can_read_line = 0;
}


//================================================================
/*! USBUART Can read line ?

  @retval	true if a line of data can be read.
*/
int usbuart_can_read_line(void)
{
  return usbuart_.flag_can_read_line;
}


//================================================================
/*! USBUART return the byte size of buffer data.

  @retval	size
*/
int usbuart_size(void)
{
  return usbuart_.p_buf_w - usbuart_.buf;
}


//================================================================
/*! USBUART return the pointer to read buffer.

  @retval	pointer to read buffer.
*/
char * usbuart_cstr(void)
{
  return (char*)usbuart_.buf;
}


//================================================================
/*! USBUART put string

  @param
*/
void usbuart_put_string( const char *s )
{
  //  To be sure that the previous data has finished sending.
  while( !USBUART_CDCIsReady() ) { /* busy loop. */ }

  USBUART_PutString(s);
}


//================================================================
/*! USBUART binary read.

  @param  buf	pointer to read buffer.
  @param  nbyte	maximum reading byte length.
  @retval	read length.
*/
int usbuart_read( void *buf, int nbyte )
{
  int len = nbyte;

  while( len > 0 ) {
    if( usbuart_size() == 0 ) {
      if( usbuart_event() < 0 ) {
	return 0;
      }
      continue;
    }

    int copy_size = usbuart_size() < len ? usbuart_size() : len;
    int remain = usbuart_size() - copy_size;

    memcpy( buf, usbuart_.buf, copy_size );
    if( remain > 0 ) {
      memmove( usbuart_.buf, usbuart_.buf + copy_size, remain );
      usbuart_.p_buf_w = usbuart_.buf + remain;
    } else {
      usbuart_clear();
    }

    buf += copy_size;
    len -= copy_size;
  }

  return nbyte;
}


//================================================================
/*! USBUART binary write.

  @param  buf	pointer to write data.
  @param  nbyte	write byte size.
*/
void usbuart_write( const char *buf, int nbyte )
{
  while( nbyte > 0 ) {
    //  To be sure that the previous data has finished sending.
    while( !USBUART_CDCIsReady() ) { /* busy loop. */ }

    int size = (nbyte < USBUART_BUFFER_SIZE) ? nbyte : USBUART_BUFFER_SIZE;
    USBUART_PutData( (uint8_t*)buf, size );
    buf += USBUART_BUFFER_SIZE;
    nbyte -= USBUART_BUFFER_SIZE;
  }

  if( nbyte == 0 ) {
    /* Send zero-length packet to PC. */
    while( !USBUART_CDCIsReady() ) { /* busy loop. */ }
    USBUART_PutData(NULL, 0);
  }
}


//================================================================
/*! USBUART event handler.

  @retval
*/
int usbuart_event(void)
{
  if( USBUART_IsConfigurationChanged() ) {
    /* Initialize IN endpoints when device is configured. */
    if( USBUART_GetConfiguration() ) {
      /* Enumeration is done, enable OUT endpoint to receive data from host. */
      USBUART_CDC_Init();
    }

    return 1;
  }

  if( !USBUART_GetConfiguration() ) {
    return -1;
  }

  if( USBUART_DataIsReady() ) {
    int count = USBUART_GetCount();
    int remain = sizeof(usbuart_.buf) - usbuart_size() - 1;

    if( count > remain ) count = remain;
    USBUART_GetData( usbuart_.p_buf_w, count );

    for( ; count != 0; count-- ) {
      if( *usbuart_.p_buf_w++ == '\n' ) {
	usbuart_.flag_can_read_line = 1;
      }
    }
    *usbuart_.p_buf_w = '\0';

    // TODO check buffer full
    return 2;
  }

  return 0;
}



//================================================================
/*! write bytecode

  @param  size	size of bytecode
  @retval 0	OK.
  @retval 1	number of programs overflow.
  @retval 2	total bytecode size overflow.
*/
static int write_bytecode( uint16_t size )
{
  static const uint8_t *FLASH_ADDR_END = mruby_bytecode + MRBC_SIZE_IREP_STRAGE;
  uint16_t tbl_bytecode_size[MRBC_MAX_BYTECODES];
  const uint8_t *flash_addr = mruby_bytecode + sizeof(tbl_bytecode_size);
  int used_idx;

  memcpy( tbl_bytecode_size, mruby_bytecode, sizeof(tbl_bytecode_size));

  for( used_idx = 0; used_idx < MRBC_MAX_BYTECODES; used_idx++ ) {
    if( tbl_bytecode_size[used_idx] == 0 ) break;
    flash_addr += tbl_bytecode_size[used_idx];
  }

  if( used_idx >= MRBC_MAX_BYTECODES ) return 1;
  if( flash_addr + size > FLASH_ADDR_END ) return 2;

  tbl_bytecode_size[used_idx] = size;

  cystatus status = Em_EEPROM_Write((uint8_t *)tbl_bytecode_size,
				    mruby_bytecode, sizeof(tbl_bytecode_size));
  if (CYRET_SUCCESS != status) {
    CONS_PutString("Em EEPROM size table write error.\r\n");
    while (1) { /* Will stay here */ }
  }

  while( size > 0 ) {
    uint8_t buf[CY_FLASH_SIZEOF_ROW];
    int read_size = size < sizeof(buf) ? size : sizeof(buf);
    read_size = usbuart_read( buf, read_size );
    status = Em_EEPROM_Write(buf, flash_addr, read_size);

    if (CYRET_SUCCESS != status) {
      CONS_PutString("Em EEPROM error3\r\n");
      while (1) { /* Will stay here */ }
    }

    size -= read_size;
    flash_addr += read_size;
  }

  usbuart_clear();

  return 0;
}


//================================================================
/*! clear bytecode
*/
static int clear_bytecode( void )
{
  uint16_t tbl_bytecode_size[MRBC_MAX_BYTECODES] = {0};

  cystatus status = Em_EEPROM_Write((uint8_t *)tbl_bytecode_size,
				    mruby_bytecode, sizeof(tbl_bytecode_size));
  if (CYRET_SUCCESS != status) {
    CONS_PutString("Em EEPROM size table write error.\r\n");
    while (1) { /* Will stay here */ }
  }

  return 0;
}


//================================================================
/*! show program list
*/
static int show_prog( void )
{
  uint16_t *tbl_bytecode_size = (uint16_t *)mruby_bytecode;
  int used_size = 0;
  char buf[10];

  usbuart_put_string("idx   size\r\n");
  for( int i = 0; i < MRBC_MAX_BYTECODES; i++ ) {
    usbuart_put_string(" ");
    usbuart_put_string( tiny_itoa( buf, i ));
    usbuart_put_string("    ");
    usbuart_put_string( tiny_itoa( buf, *tbl_bytecode_size ));
    usbuart_put_string("+2\r\n");

    used_size += *tbl_bytecode_size++ + sizeof(uint16_t);
  }

  int percent = 100 * used_size / MRBC_SIZE_IREP_STRAGE;
  usbuart_put_string("total ");
  usbuart_put_string( tiny_itoa( buf, used_size ));
  usbuart_put_string(" / ");
  usbuart_put_string( tiny_itoa( buf, MRBC_SIZE_IREP_STRAGE ));
  usbuart_put_string(" (");
  usbuart_put_string( tiny_itoa( buf, percent ));
  usbuart_put_string("%)\r\n");

  return 0;
}


//================================================================
/*! initializer
*/
void mrbc_monitor_init(void)
{
  USBUART_Start(0, USBUART_5V_OPERATION);
  usbuart_clear();
}


//================================================================
/*! Choose to enter monitor mode or run mruby/c program.

  @retval  0   not receive CRLF. (to execute mruby/c VM)
  @retval  1   receive CRLF. (to monitor mode)
*/
int mrbc_monitor_or_exec(void)
{
  /*
    Wait for a while.
    Returns whether a line break was received in the meantime.
  */
  const int MAX_WAIT_CYCLE = 256;
  int i;
  for( i = 0; i < MAX_WAIT_CYCLE; i++ ) {
    LED1_Write(((i>>4) | (i>>1)) & 0x01 );        // Blink LED1

    usbuart_event();
    if( usbuart_can_read_line() ) break;
    if( !SW1_Read() ) break;

    CyDelay( 10 );
  }

  return i != MAX_WAIT_CYCLE;
}


//================================================================
/*! start monitor mode.
*/
int mrbc_monitor_run(void)
{
  static const char ws[] = " \t\r\n\f\v";
  static const char *help_msg =
    "Commands:\r\n"
    "  version\r\n"
    "  reset\r\n"
    "  execute\r\n"
    "  clear\r\n"
    "  write [size]\r\n"
    "  showprog\r\n";

  usbuart_clear();

  while( 1 ) {
    usbuart_event();
    if( !usbuart_can_read_line() ) continue;

    // TODO check buffer full

    char *token = strtok( usbuart_cstr(), ws );
    if( token == NULL ) {
      usbuart_put_string( "+OK mruby/c\r\n" );
      goto DONE;
    }

    if( strcmp(token, "help") == 0 ) {
      usbuart_put_string( "+OK\r\n" );
      usbuart_put_string(help_msg);
      usbuart_put_string( "+DONE\r\n" );
      goto DONE;
    }

    if( strcmp(token, "version") == 0 ) {
      usbuart_put_string("+OK " MRUBYC_VERSION_STRING "\r\n");
      goto DONE;
    }

    if( strcmp(token, "reset") == 0 ) {
      usbuart_put_string("+OK\r\n");
      CyDelay(100);
      CySoftwareReset();
    }

    if( strcmp(token, "execute") == 0 ) {
      usbuart_put_string("+OK Execute mruby/c.\r\n");
      return 0;	// to execute VM.
    }

    if( strcmp(token, "clear") == 0 ) {
      clear_bytecode();
      usbuart_put_string( "+OK\r\n" );
      goto DONE;
    }

    if( strcmp(token, "write") == 0 ) {
      token = strtok( NULL, ws );
      if( token == NULL ) {
	usbuart_put_string( "-ERR\r\n" );
	goto DONE;
      }

      int size = atoi(token);
      if( size < 0 || size > MRBC_SIZE_IREP_STRAGE ) {
	usbuart_put_string( "-ERR size overflow.\r\n" );
	goto DONE;
      }

      usbuart_clear();
      usbuart_put_string( "+OK Write bytecode.\r\n" );
      switch( write_bytecode( size ) ) {
      case 0:
	usbuart_put_string( "+DONE\r\n" );
	break;
      case 1:
	usbuart_put_string( "-ERR number of programs overflow.\r\n" );
	break;
      case 2:
	usbuart_put_string( "-ERR total bytecode size overflow.\r\n" );
	break;
      }
      goto DONE;
    }

    if( strcmp(token, "showprog") == 0 ) {
      show_prog();
      usbuart_put_string( "+DONE\r\n" );
      goto DONE;
    }

    usbuart_put_string( "-ERR Illegal command.\r\n" );

  DONE:
    usbuart_clear();
  }
}
