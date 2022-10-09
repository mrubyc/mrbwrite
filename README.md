# mruby/c mrb file writer.

## What is this?

コンパイル後のmrbファイルを、シリアル通信を使ってターゲットに転送する。

## How to compile

クロスプラットフォームのために、Qt (https://www.qt.io/jp) を使用している。まずQtをインストール (https://download.qt.io/official_releases/qt/) してから、下記コマンドにてコンパイルする。

```
qmake
make  # or gmake, nmake
```

## Deploy

### for Mac
```
cp ~/Qt/*/clang_64/lib/QtCore.framework/Versions/5/QtCore .
cp ~/Qt/*/clang_64/lib/QtSerialPort.framework/Versions/5/QtSerialPort .
install_name_tool -change "@rpath/QtCore.framework/Versions/5/QtCore" "@executable_path/QtCore" mrbwrite
install_name_tool -change "@rpath/QtSerialPort.framework/Versions/5/QtSerialPort" "@executable_path/QtSerialPort" mrbwrite
```

## How to use
```
./mrbwrite --showline   # show all lines
./mrbwrite -l cu.USBSERIAL -s 57600 PROG1.mrb PROG2.mrb ...
```


# 通信プロトコル

## 概要

* POP3プロトコルを模倣している。
* テキストで１行コマンドを送信すると、それに対するレスポンスが返る。
* 改行コードは、CRLF。

レスポンスは、
* `+`で始まる場合は正常終了
* `-`で始まる場合はエラー

となる。



### 例
```
[SEND] version
[RECV] +OK mruby/c v1.00
[SEND] BADCOMMAND
[RECV] -ERR Illegal command.
```

## コマンド

<dl>
  <dt>version
  <dd>バージョン表示

  <dt>clear
  <dd>書き込み済みバイトコードの消去

  <dt>write (size)
  <dd>mrubyバイトコード書き込み

  <dt>execute
  <dd>mrubyプログラム実行

  <dt>reset
  <dd>ソフトウェアリセット

  <dt>help
  <dd>コマンド一覧表示（人間用）

  <dt>showprog
  <dd>書き込み済みプログラムサイズ表示（人間用）
</dl>


## 接続の開始

mrbwrite側からターゲットへ空行（CRLFのみ）を送信することで開始される。  
ターゲットでは、通常は起動後に書き込み済みのプログラムを実行するため、この参照実装では、起動直後の数秒間にCRLFを受信するか、ボード上のSW1が押されるとコマンド受け付けモードに入る。

## 典型的なmrbファイル書き込み例
以下の例は、mrbファイルのサイズが250バイトの場合であり、トランザクション中に250という数値がでてくる。

```
mrbwrite:                :TARGET
   |                        |
   |  CRLF,CRLF,...         |    返信があるまで、Syncコードとして何度かCRLFを送信する
   +----------------------->|
   |                        |
   |  "+OK mruby/c"         |
   |<-----------------------|
   |                        |
   |  "clear"               |
   +----------------------->|
   |                        |
   |  "+OK"                 |
   |<-----------------------|
   |                        |
   |  "write 250"           |
   +----------------------->|
   |                        |
   |  "+OK Write bytecode"  |
   |<-----------------------|
   |                        |
   | (mrb file 250 bytes)   |
   +----------------------->|
   |                        |
   |  "+DONE"               |
   |<-----------------------|
   |                        |
   |  "execute"             |
   +----------------------->|
   |                        |
   |  "+OK"                 |
   |<-----------------------|
   |                        |
```
