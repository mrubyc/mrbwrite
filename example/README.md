# モニターモード参照実装

## What is this?

mrbwriteとの通信、ターゲット側の Cypress CY8CKIT-059 用の参照実装。

## How to use.

メインのプログラムに、以下の通り記述する。

```
#include "mrbc_monitor.h"

const uint8_t CYCODE mruby_bytecode[MRBC_SIZE_IREP_STRAGE];  // Flash ROM

if( mrbc_monitor_or_exec() ) mrbc_monitor_run();
mrubyc_start();
```
