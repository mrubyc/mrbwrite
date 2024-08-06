# ターゲットマイコン側バイトコード受信参照実装

## What is this?

* mrbwriteとの通信によりバイトコードを受信し、内蔵フラッシュメモリに保存するルーチンの参照実装です
* ターゲットは STM32F401RE です
* ST製 HAL ライブラリと、CubeIDEを使います

## How to use.

UART2を使う前提で記述します。
1. CubeIDEで、UART2を選びます。
2. Modeを Asynchronous に設定します。
3. ParameterSettings の BaudRateを必要に応じて変更します。
4. DMASettings の [Add]ボタンをクリックします。
5. Select欄が表示されるので、USART2_RXに変更します。
6. Modeを、Circularに変更します。
7. メインのプログラムに、以下の通り記述します。

```c
#define MRBC_MEMORY_SIZE (1024*30)
static uint8_t memory_pool[MRBC_MEMORY_SIZE];

uart_init();
receive_bytecode( memory_pool, MRBC_MEMORY_SIZE );
```
