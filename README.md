# RaspberryPi初期設定からLチカまで

## 頑張ってRasbianを入れる
がんばれ


## 頑張ってインターネットに繋がるようにする

メモ: raspberry pi 3のwifi設定

以下のコマンドでwifiに接続できるようになる
(`SSID`と`PASSPHRASE`は適切なものに置き換えること)
```
sudo sh -c 'wpa_passphrase SSID PASSPHRASE >> /etc/wpa_supplicant/wpa_supplicant.conf'
sudo vi /etc/wpa_supplicant/wpa_supplicant.conf # コメントアウトされた生パスワードを削除する
sudo reboot
```


## RaspberryPiの初期設定
これやったらなんとかなる
```
$ sudo apt-get update -y
$ sudo apt-get upgrade -y
$ sudo apt-get dist-upgrade -y
$ sudo rpi-update
$ sudo reboot   # １回再起動する
```


## eJSの導入
#### ツールのインストール

  これ入れたらなんとかなる
  ```
  $ sudo apt-get install vim -y
  $ sudo apt-get install git -y
  $ sudo apt-get install ant -y
  $ sudo apt-get install openjdk-8-jdk -y
  $ sudo apt-get install ruby -y
  $ sudo apt-get install ruby-dev -y
  $ sudo apt-get install libonig-dev -y
  $ sudo reboot # とりあえず再起動
  ```
#### eJSのインストール
  1. git clone
  ```
  git clone https://github.com/plasklab/open-campus-2018.git ejsvm
  git clone https://github.com/plasklab/ejstk-ejsc.git ejsc
  cd ejsvm
  git clone https://github.com/plasklab/ejstk-vmgen.git vmgen
  ```

  2. コンパイラのビルド
  ```
  cd ejsc
  wget http://www.antlr.org/download/antlr-4.5.3-complete.jar -P libs
  wget http://central.maven.org/maven2/org/glassfish/javax.json/1.0.4/javax.json-1.0.4.jar -P libs
  make
  ```

  3. vmgenのビルド
  ```
  cd ejsvm/vmgen
  ant
  ```

  4. VMのビルド
  ```
  cd ejsvm
  cp common.mk.template common.mk
  vim common.mk
  - 以下を変更する
    - CC = clang -> CC gcc
    - SED = gsed -> SED sed
  mkdir build
  cd build
  vim Makefile
  ```

  5. Makefile
  Makefileを以下のように書き換える
  ```
  .PRECIOUS: %.c %.h
  DATATYPES=../datatypes/default.def
  OPERANDSPEC=../operand-spec/any.spec
  CFLAGS=-DNDEBUG -UDEBUG -O3
  include ../common.mk
  ```
  makeする


## Lチカ

* 適当な場所でLチカのプログラムをかく
  ```
  // blink.js
  Raspi.init();
  var LED_GPIO = 17; // 各自適当なGPIOピンを定義する

  for (var k = 0; k < 10; k++) {
      Raspi.gpioWrite(LED_GPIO, 1); // LED ON
      Time.delay(1000);

      Raspi.gpioWrite(LED_GPIO, 0); // LED OFF
      Time.delay(1000);
  }
  ```

* コンパイルして実行する
  ```
  $ cd /path/to/ejsc
  $ java -jar newejsc.jar /path/to/blink.js
  $ /path/to/ejsvm/build/ejsvm /path/to/blink.sbc
  ```

