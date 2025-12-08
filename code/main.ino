#include <Adafruit_NeoPixel.h>        // WS2812BフルカラーLEDを制御するライブラリ
#include <DFRobotDFPlayerMini.h>      // DFPlayer mini（MP3プレイヤー）制御用ライブラリ
#include <SoftwareSerial.h>           // ソフトウェアシリアル通信ライブラリ（DFPlayerとの通信に使用）
#include <FFT.h>                      // FFT（高速フーリエ変換）で音の周波数解析を行うライブラリ
#include <ErriezTM1637.h>             // TM1637ベースのキーパッド／表示モジュール制御用ライブラリ

// ---- ピン設定 ----
#define LED_PIN     6   // LEDマトリックス（WS2812B）のデータ線をつなぐArduino側のピン
#define AUDIO_PIN   A0  // DFPlayerのオーディオを読み取るアナログ入力ピン（FFT用）
#define DF_TX       10  // Arduino→DFPlayer方向のTXピン（ArduinoのデータをDFPlayerへ送る）
#define DF_RX       11  // DFPlayer→Arduino方向のRXピン（DFPlayerの状態をArduinoで受け取る）
#define KP_CLK      3   // TM1637キーパッドのクロック線（CLK）
#define KP_DIO      4   // TM1637キーパッドのデータ線（DIO）

// ---- LEDマトリックスのサイズ設定 ----
#define LED_WIDTH   32                          // LEDマトリックスの横方向のLED数
#define LED_HEIGHT  8                           // LEDマトリックスの縦方向のLED数
#define NUM_LEDS    (LED_WIDTH * LED_HEIGHT)    // 全LEDの個数（32×8=256）

// Adafruit_NeoPixelオブジェクトの生成（LEDの数、ピン、通信方式を指定）
Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// DFPlayerと通信するためのソフトウェアシリアルポートを定義
// 第1引数：RXピン（DFPlayer TXと接続）
// 第2引数：TXピン（DFPlayer RXと接続）
SoftwareSerial dfSerial(DF_RX, DF_TX);

// DFPlayerライブラリ用のオブジェクトを作成
DFRobotDFPlayerMini dfplayer;

// TM1637キーパッドのオブジェクトを作成（CLKピンとDIOピンを指定）
ErriezTM1637 keypad(KP_CLK, KP_DIO);

// ---- FFT関連の設定 ----
#define SAMPLES         128      // 1回のFFTで使うサンプル数（2のべき乗である必要がある）
#define SAMPLING_FREQ   4000     // サンプリング周波数（1秒間に何回A0を読むか：4000Hz）

unsigned int sampling_period_us; // サンプリング間隔（マイクロ秒単位）を格納する変数
unsigned long microseconds;      // サンプリング時の現在時刻（micros()）を一時的に保持

// FFTライブラリが使う入力配列（実部）
// ライブラリによって型や名前が違う場合があるので、自分のFFTに合わせて要調整
int fft_input[SAMPLES];          // A0から読み取った波形データを入れる配列

// FFTの結果（対数スケールの振幅）を格納する配列
// 周波数成分はSAMPLES/2個分なのでそのサイズを確保
byte fft_log_out[SAMPLES / 2];

// 各列のピークを保持する配列（今は未使用だが、ピークホールド表示などに使える）
byte peak[LED_WIDTH];

// ---- セットアップ関数（起動時に1度だけ実行される）----
void setup() {
  Serial.begin(9600);             // PCとのデバッグ用シリアル通信を9600bpsで開始

  leds.begin();                   // LEDマトリックスの制御を開始
  leds.setBrightness(40);         // LEDの明るさを0〜255の範囲で設定（40はやや控えめ）
  leds.show();                    // 初期状態を反映（ここでは全消灯）

  dfSerial.begin(9600);           // DFPlayer用のソフトウェアシリアルを9600bpsで開始
  if (!dfplayer.begin(dfSerial)) {// DFPlayerに接続できるか確認
    Serial.println("DFPlayer Mini not found!"); // 見つからなければエラーメッセージ表示
    while (1);                    // 無限ループで停止（以降の処理を止める）
  }
  dfplayer.volume(20);            // DFPlayerの音量を設定（0〜30の範囲）
  Serial.println("DFPlayer ready"); // DFPlayerの初期化完了メッセージ

  keypad.begin();                 // TM1637キーパッドの初期化
  keypad.setBrightness(7);        // キーパッドの表示明るさ（0〜7）を最大に設定

  // サンプリング周期（マイクロ秒単位）を計算
  // 例：SAMPLING_FREQ=4000Hz → 1/4000秒 ≒ 250マイクロ秒
  sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQ));
}

// ---- メインループ（繰り返し実行される）----
void loop() {
  // 1. キーパッドの入力確認
  int key = keypad.getKey();      // キーが押されていれば、そのキー番号が返る（押されていなければ-1）
  if (key != -1) {                // -1でなければ、何かのキーが押されたということ
    Serial.print("Key pressed: "); // 押されたキー番号をシリアルモニタに表示
    Serial.println(key);
    dfplayer.play(key + 1);       // DFPlayerに再生コマンドを送る（キー0→トラック1、キー1→トラック2…）
  }

  // 2. FFT用の音声サンプリング
  for (int i = 0; i < SAMPLES; i++) {  // SAMPLES個分のデータを集める
    microseconds = micros();           // 現在のマイクロ秒タイムスタンプを取得
    int val = analogRead(AUDIO_PIN);   // A0からアナログ値を読み取る（0〜1023）
    fft_input[i] = val;                // 読み取った値をFFT入力配列に保存

    // 次のサンプルを取るまで、サンプリング周期分だけ待機
    // （目標サンプリング周波数を維持するための簡易ウェイト）
    while (micros() - microseconds < sampling_period_us);
  }

  // 3. FFTの実行（ライブラリ依存：実際の関数名は使っているFFTライブラリに合わせて要確認）
  fft_window();                // 窓関数（ハミング窓など）をかけて周波数解析の精度を上げる
  fft_reorder();               // FFT演算のためにデータの順番をビットリバース並びに変換
  fft_run();                   // 実際にFFT演算を行う
  fft_mag_log();               // 振幅を対数スケールに変換し、fft_log_out[]に結果を格納する

  // 4. FFT結果をLEDマトリックスに表示
  leds.clear();                // まず全てのLEDを消灯する（前のフレームを消すため）

  for (int x = 0; x < LED_WIDTH; x++) {  // 画面の左から右までループ
    // x列目に対応させるFFTの周波数ビン（インデックス）を計算
    // 低周波〜高周波をLEDの左〜右に割り当てるイメージ
    int bin = map(x, 0, LED_WIDTH - 1, 2, SAMPLES / 2 - 1);

    // 対応する周波数ビンの値を高さ（0〜LED_HEIGHT）にマッピング
    int level = map(fft_log_out[bin], 0, 100, 0, LED_HEIGHT);
    level = constrain(level, 0, LED_HEIGHT); // 万が一範囲外になったときに制限

    // 下から「level」の高さまでLEDを点灯
    for (int y = 0; y < level; y++) {
      int idx = getLedIndex(x, y);     // (x, y)座標を1次元のLEDインデックスに変換
      leds.setPixelColor(idx, leds.Color(0, 150, 255)); // LEDの色を設定（ここでは青系）
    }
  }
  leds.show();                 // 設定した色を実際にLEDに反映させる

  delay(30);                   // 表示更新間隔を少し開ける（30ミリ秒）→ 約33fps
}

// ---- LEDマトリックスのインデックス変換 ----
// 実際の配線が「ジグザグ」（蛇行）になっている前提で、(x, y)→配列番号を計算する関数
int getLedIndex(int x, int y) {
  if (x % 2 == 0) {                      // 偶数列（左から数えて0,2,4…）のとき
    return x * LED_HEIGHT + y;           // 下から上へ順番に並んでいると仮定
  } else {                               // 奇数列（1,3,5…）のとき
    return x * LED_HEIGHT + (LED_HEIGHT - 1 - y); // 上から下へ逆順に並んでいるとして計算
  }
}
