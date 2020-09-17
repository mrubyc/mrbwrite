# モニターモード参照実装

## What is this?

mrbwriteとの通信、ターゲット側の Cypress CY8CKIT-059 用の参照実装。

## How to use.

メインのプログラムに、以下の通り記述する。

```
#include "mrbc_monitor.h"

const uint8_t CYCODE mruby_bytecode[MRBC_SIZE_IREP_STRAGE];  // Flash ROM

// in main function.
{
  ...
  // enter monitor mode?
  mrbc_monitor_init();
  if( mrbc_monitor_or_exec() ) mrbc_monitor_run();

  // execute mruby/c program on Flash ROM.
  uint16_t *tbl_bytecode_size = (uint16_t *)mruby_bytecode;
  const uint8_t *bytecode = mruby_bytecode + sizeof(uint16_t) * MRBC_MAX_BYTECODES;

  for( int i = 0 ; i < MRBC_MAX_BYTECODES ; i++ ) {
    if( tbl_bytecode_size[i] == 0 ) break;
    mrbc_create_task( bytecode, 0 );
    bytecode += tbl_bytecode_size[i];
  }

  mrbc_run();
}
```
