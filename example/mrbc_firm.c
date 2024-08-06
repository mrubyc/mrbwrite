/*! @file
  @brief
  Receive bytecode and write to FLASH.

  <pre>
  Copyright (C) 2024- Shimane IT Open-Innovation Center.

  This file is distributed under BSD 3-Clause License.

  </pre>
*/

//@cond
#include <stdint.h>
#include <string.h>
//@endcond

#include "main.h"
#include "../mrubyc_src/mrubyc.h"
#include "stm32f4_uart.h"


#define VERSION_STRING   "mruby/c v3.3 RITE0300 MRBW1.2"

const uint32_t IREP_START_ADDR = 0x08060000;
const uint32_t IREP_END_ADDR   = 0x0807FFFF;

static const char RITE[4] = "RITE";
static const char WHITE_SPACE[] = " \t\r\n\f\v";


#define STRM_READ(buf, len)	uart_read(UART_HANDLE_CONSOLE, buf, len)
#define STRM_GETS(buf, size)	uart_gets(UART_HANDLE_CONSOLE, buf, size)
#define STRM_PUTS(buf)		uart_write(UART_HANDLE_CONSOLE, buf, strlen(buf))
#define STRM_RESET()		uart_clear_rx_buffer(UART_HANDLE_CONSOLE)
#define SYSTEM_RESET()		HAL_NVIC_SystemReset()

static int cmd_help();
static int cmd_version();
static int cmd_reset();
static int cmd_execute();
static int cmd_clear();
static int cmd_write();
static int cmd_showprog();


static uint32_t irep_write_addr_;	//!< IREP file write point.

//! command table.
static struct COMMAND_T {
  const char *command;
  int (*function)();

} const TBL_COMMANDS[] = {
  {"help",	cmd_help },
  {"version",	cmd_version },
  {"reset",	cmd_reset },
  {"execute",	cmd_execute },
  {"clear",	cmd_clear },
  {"write",	cmd_write },
  {"showprog",	cmd_showprog },
};

static const int NUM_TBL_COMMANDS = sizeof(TBL_COMMANDS)/sizeof(struct COMMAND_T);


//================================================================
/*! command 'help'
*/
static int cmd_help(void)
{
  STRM_PUTS("+OK\r\nCommands:\r\n");

  for( int i = 0; i < NUM_TBL_COMMANDS; i++ ) {
    STRM_PUTS("  ");
    STRM_PUTS(TBL_COMMANDS[i].command);
    STRM_PUTS("\r\n");
  }
  STRM_PUTS("+DONE\r\n");
  return 0;
}


//================================================================
/*! command 'version'
*/
static int cmd_version(void)
{
  STRM_PUTS("+OK " VERSION_STRING "\r\n");
  return 0;
}


//================================================================
/*! command 'reset'
*/
static int cmd_reset(void)
{
  SYSTEM_RESET();
  return 0;
}


//================================================================
/*! command 'execute'
*/
static int cmd_execute(void)
{
  STRM_PUTS("+OK Execute mruby/c.\r\n");
  return 1;	// to execute VM.
}


//================================================================
/*! command 'clear'
*/
static int cmd_clear(void)
{
  HAL_FLASH_Unlock();

  FLASH_EraseInitTypeDef erase = {
    .TypeErase = FLASH_TYPEERASE_SECTORS,
    .Sector = FLASH_SECTOR_7,
    .NbSectors = 1,
    .VoltageRange = FLASH_VOLTAGE_RANGE_3,  // Device operating range: 2.7V to 3.6V
  };
  uint32_t error = 0;
  HAL_StatusTypeDef sts = HAL_FLASHEx_Erase(&erase, &error);
  HAL_FLASH_Lock();

  if( sts == HAL_OK && error == 0xFFFFFFFF ) {
    STRM_PUTS("+OK\r\n");
  } else {
    STRM_PUTS("-ERR\r\n");
  }

  irep_write_addr_ = IREP_START_ADDR;
  return 0;
}


//================================================================
/*! command 'write'
*/
static int cmd_write( void *buffer, int buffer_size )
{
  char *token = strtok( NULL, WHITE_SPACE );
  if( token == NULL ) {
    STRM_PUTS("-ERR\r\n");
    return -1;
  }

  // check size
  int size = mrbc_atoi(token, 10);
  uint32_t irep_write_end = irep_write_addr_ + size;
  if( (irep_write_end > IREP_END_ADDR) || (size > buffer_size) ) {
    STRM_PUTS("-ERR IREP file size overflow.\r\n");
    return -1;
  }

  STRM_PUTS("+OK Write bytecode.\r\n");

  // get a bytecode.
  uint8_t *p = buffer;
  int n = size;
  while (n > 0) {
    int readed_size = STRM_READ( p, size );
    p += readed_size;
    n -= readed_size;
  }

  // check 'RITE' magick code.
  p = buffer;
  if( strncmp( (const char *)p, RITE, sizeof(RITE)) != 0 ) {
    STRM_PUTS("-ERR No RITE code received.\r\n");
    return -1;
  }

  // Write bytecode to FLASH.
  HAL_FLASH_Unlock();

  size += (-size & 3);		// align 4 byte.
  irep_write_end = irep_write_addr_ + size;

  while( irep_write_addr_ < irep_write_end ) {
    uint32_t data = p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0];

    HAL_StatusTypeDef sts;
    sts = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, irep_write_addr_, data);

    if( sts != HAL_OK ) {
      STRM_PUTS("-ERR Flash write error.\r\n");
      HAL_FLASH_Lock();
      return -1;
    }

    p += 4;
    irep_write_addr_ += 4;
  }
  HAL_FLASH_Lock();

  STRM_PUTS("+DONE\r\n");

  return 0;
}


//================================================================
/*! command 'showprog'
*/
static int cmd_showprog(void)
{
  uint8_t *addr = (uint8_t *)IREP_START_ADDR;
  int n = 0;
  char buf[80];

  STRM_PUTS("idx size offset\r\n");
  while( strncmp( (const char *)addr, RITE, sizeof(RITE)) == 0 ) {
    unsigned int size = 0;
    for( int i = 0; i < 4; i++ ) {
      size = (size << 8) | addr[8 + i];
    }
    mrbc_snprintf(buf, sizeof(buf), " %d  %-4d %p\r\n", n++, size, addr);
    STRM_PUTS(buf);

    addr += size + (-size & 3);	// align 4 byte.
  }

  int total = (IREP_END_ADDR - IREP_START_ADDR + 1);
  int used = (uintptr_t)addr - IREP_START_ADDR;
  int percent = 100 * used / total;
  mrbc_snprintf(buf, sizeof(buf), "total %d / %d (%d%%)\r\n", used, total, percent);
  STRM_PUTS(buf);
  STRM_PUTS("+DONE\r\n");

  return 0;
}


//================================================================
/*! receive bytecode mode
*/
int receive_bytecode( void *buffer, int buffer_size )
{
  char buf[50];

  STRM_PUTS("+OK mruby/c\r\n");

  while( 1 ) {
    // get the command string.
    if( STRM_GETS(buf, sizeof(buf)) < 0 ) {
      STRM_RESET();
      continue;
    }

    // split tokens.
    char *token = strtok( buf, WHITE_SPACE );
    if( token == NULL ) {
      STRM_PUTS("+OK mruby/c\r\n");
      continue;
    }

    // find command.
    int i;
    for( i = 0; i < NUM_TBL_COMMANDS; i++ ) {
      if( strcmp( token, TBL_COMMANDS[i].command ) == 0 ) break;
    }
    if( i == NUM_TBL_COMMANDS ) {
      STRM_PUTS("-ERR Illegal command. '");
      STRM_PUTS(token);
      STRM_PUTS("'\r\n");
      continue;
    }

    // execute command.
    if( (TBL_COMMANDS[i].function)(buffer, buffer_size) == 1 ) break;
  }

  return 0;
}


//================================================================
/*! pick up a task
*/
void * pickup_task( void *task )
{
  uint8_t *addr = (uint8_t *)IREP_START_ADDR;

  if( task ) {
    if( strncmp( task, RITE, sizeof(RITE)) != 0 ) return 0;

    addr = task;
    unsigned int size = 0;
    for( int i = 0; i < 4; i++ ) {
      size = (size << 8) | addr[8 + i];
    }

    addr += size + (-size & 3);	// align 4 byte.
  }

  if( strncmp( (const char *)addr, RITE, sizeof(RITE)) == 0 ) {
    return addr;
  }

  return 0;
}
