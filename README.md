# mruby/c mrb file writer.

## What is this?

コンパイル後のmrbファイル（バイトコード）を、シリアル通信を使ってターゲットに転送する。

## How to compile

クロスプラットフォームのために、Qt5 (https://www.qt.io/jp) を使用している。まずQt5をインストール (https://download.qt.io/official_releases/qt/) してから、下記コマンドにてコンパイルする。

```
qmake
make  # or gmake, nmake
```

### 動作確認済の、Qtバージョン
 * Windows  Qt5.12.12
 * Mac  Qt5.12.12


## Deploy

### for Windows

```
mkdir ¥PATH¥TO¥DEPLOY¥mrbwrite
copy mrbwrite.exe ¥PATH¥TO¥DEPLOY¥mrbwrite
cd ¥PATH¥TO¥DEPLOY¥mrbwrite
windeployqt mrbwrite.exe
```

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
mrbwrite:                  :TARGET
   |                          |
   |  "version"               |
   +------------------------->|
   |                          |
   |  "+OK mruby/c v1.00"     |
   |<-------------------------+
   |                          |
```

## コマンドサマリー

<dl>
  <dt>version
  <dd>バージョン表示

  <dt>clear
  <dd>書き込み済みバイトコードの消去

  <dt>write (size)
  <dd>mrubyバイトコード書き込み

  <dt>execute
  <dd>書き込んだプログラムを実行

  <dt>reset
  <dd>ソフトウェアリセット

  <dt>help
  <dd>コマンド一覧表示（人間用）

  <dt>showprog
  <dd>書き込み済みプログラムサイズ表示（人間用）
</dl>


## 接続の開始

mrbwrite（ホストPC）側からターゲットへ空行（CRLFのみ）を送信することで開始される。  
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
   |<-----------------------+
   |                        |
   |  "clear"               |
   +----------------------->|
   |                        |
   |  "+OK"                 |
   |<-----------------------+
   |                        |
   |  "write 250"           |
   +----------------------->|
   |                        |
   |  "+OK Write bytecode"  |
   |<-----------------------+
   |                        |
   | (mrb file 250 bytes)   |
   +----------------------->|
   |                        |
   |  "+DONE"               |
   |<-----------------------+
   |                        |
   |  "execute"             |
   +----------------------->|
   |                        |
   |  "+OK"                 |
   |<-----------------------+
   |                        |
```


## 各コマンドの仕様と応答例

応答例は1行目は送信したコマンド行で、2行目以降は受信した行。
一部のコマンドを除き、行頭の `+`, `-` で成否を表し、以降のコメントは参考（デバッグ）用と考えて良い。

### version
バージョン表示

次項のバージョンチェックを参照。

応答例
```
version
+OK mruby/c v3.3 RITE0300 MRBW1.2
```

### clear
書き込み済みバイトコードの消去

複数のプログラムが書かれている場合も、すべて消去する。

応答例
```
clear
+OK
```

### write
mrubyバイトコード書き込み

複数のプログラムを書き込む場合、当コマンドを書き込むプログラムの個数の回数分、連続して発行する。

応答例
```
write 250
+OK Write bytecode.
(バイトコード送信 250 bytes)
+DONE
```

送信したバイトコードが、正しいIREPファイルではなかった場合。
```
-ERR No RITE code received.
```

### execute
書き込んだプログラムを実行

応答例
```
execute
+OK Execute mruby/c.
```

### reset
ソフトウェアリセット

応答例
```
reset
+OK
```

### help
コマンド一覧表示（人間用）

人間用なので、実装しなくても良い。

応答例
```
help
+OK
Commands:
  help
  version
  reset
  execute
  clear
  write
  showprog
+DONE
```

### showprog
書き込み済みプログラムサイズ表示（人間用）

人間用なので、実装しなくても良い。

応答例
```
showprog
idx size offset
 0  188  $bd037000
total 256 / 32768 (0%)
+DONE
```

### コマンドエラー

応答例
```
UNKNOWN_COMMAND
-ERR Illegal command. 'UNKNOWN_COMMAND'
```

## バージョンチェック

`version` コマンドを送信することによって、ターゲットのバージョンを得ることができる。
そのフォーマットは、1つのスペースキャラクタ区切りによる以下のフォーマットが期待される。

(例）
```
+OK mruby/c v3.3 RITE0300 MRBW1.2
```
カラム

1. +OK　　　POP3互換ステータス文字列  
2. mruby/c　歴史的経緯  
3. v3.3　 　mruby/c VM のバージョン  
4. RITE0300　実行できるバイトコードRITEバージョン  
5. MRBW1.2　この通信プロトコルバージョン  
