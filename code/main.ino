#include <Adafruit_NeoPixel.h>           // WS2812B LED制御用
#include <DFRobotDFPlayerMini.h>         // DFPlayer mini制御用
#include <SoftwareSerial.h>              // DFPlayerとの通信に使用
#include <FFT.h>                         // FFTライブラリ（音の周波数解析）
#include <ErriezTM1637.h>                // TM1637キーパッド制御用

// ピン定義
#define LED_PIN     6     // LEDマトリックスのデータピン
#define AUDIO_PIN   A0    // オーディオ入力（DFPlayerの出力をここで受ける）
#define DF_TX       10    // DFPlayerのTX（ArduinoのRX）
#define DF_RX       11    // DFPlayerのRX（ArduinoのTX）
#define KP_CLK      3     // キーパッドのCLKピン
#define KP_DIO      4     // キーパッドのDIOピン

// LEDマトリックスのサイズ
#define LED_WIDTH   32
#define LED_HEIGHT  8
#define NUM_LEDS    (LED_WIDTH * LED_HEIGHT)

// LEDマトリックスの初期化
Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// DFPlayerとの通信設定
SoftwareSerial dfSerial(DF_RX, DF_TX);
DFRobotDFPlayerMini dfplayer;

// キーパッド初期化
ErriezTM1637 keypad(KP_CLK, KP_DIO);

// FFT設定
#define SAMPLES         128               // サンプル数（2のべき乗）
#define SAMPLING_FREQ   4000              // サンプリング周波数（Hz）
unsigned int sampling_period_us;
unsigned long microseconds;
int fft_input[SAMPLES];                   // FFT入力用バッファ
byte fft_log_out[SAMPLES / 2];            // FFT出力（対数スケール）
byte peak[LED_WIDTH];                     // ピーク表示用（未使用）

void setup() {
  Serial.begin(9600);

  // LED初期化
  leds.begin();
  leds.setBrightness(40);
  leds.show();

  // DFPlayer初期化
  dfSerial.begin(9600);
  if (!dfplayer.begin(dfSerial)) {
    Serial.println("DFPlayer Mini not found!");
    while (1); // エラー時は停止
  }
  dfplayer.volume(20); // 音量設定（0〜30）
  Serial.println("DFPlayer ready");

  // キーパッド初期化
  keypad.begin();
  keypad.setBrightness(7); // 表示の明るさ（0〜7）

  // FFT用のサンプリング周期計算
  sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQ));
}

void loop() {
  //  キーパッドの入力チェック
  int key = keypad.getKey();
  if (key != -1) {
    Serial.print("Key pressed: ");
    Serial.println(key);
    dfplayer.play(key + 1);  // キー0→0001.wav、キー1→0002.wav…
  }

  // 🎧 音声のサンプリング
  for (int i = 0; i < SAMPLES; i++) {
    microseconds = micros();
    int val = analogRead(AUDIO_PIN);
    fft_input[i] = val;
    while (micros() - microseconds < sampling_period_us);
  }

  // 🔍 FFT処理
  fft_window();         // 窓関数（ハミングなど）を適用
  fft_reorder();        // データの並び替え（ビットリバース）
  fft_run();            // FFT実行
  fft_mag_log();        // 対数スケールで振幅を計算 → fft_log_out[]に出力

  //  LEDマトリックスに表示
  leds.clear();
  for (int x = 0; x < LED_WIDTH; x++) {
    int bin = map(x, 0, LED_WIDTH - 1, 2, SAMPLES / 2 - 1); // 周波数帯をマッピング
    int level = map(fft_log_out[bin], 0, 100, 0, LED_HEIGHT); // 振幅を高さに変換
    level = constrain(level, 0, LED_HEIGHT);

    for (int y = 0; y < level; y++) {
      int idx = getLedIndex(x, y);
      leds.setPixelColor(idx, leds.Color(0, 150, 255)); // 青系で表示
    }
  }
  leds.show();

  delay(30); // 表示更新間隔
}

//  LEDマトリックスのジグザグ配線に対応するインデックス計算
int getLedIndex(int x, int y) {
  if (x % 2 == 0) {
    return x * LED_HEIGHT + y;
  } else {
    return x * LED_HEIGHT + (LED_HEIGHT - 1 - y);
  }
}
