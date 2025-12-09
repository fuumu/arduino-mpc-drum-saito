#include <Adafruit_NeoPixel.h>          // Adafruit社製のフルカラーLED（NeoPixel）を制御するためのライブラリを読み込む
#include <DFRobotDFPlayerMini.h>        // DFPlayer Mini（MP3プレーヤーモジュール）を制御するためのライブラリを読み込む
#include <SoftwareSerial.h>             // Arduinoの任意のデジタルピンでシリアル通信を行うためのソフトウェアシリアルライブラリを読み込む
#include <arduinoFFT.h>                 // FFT（高速フーリエ変換）を行うためのarduinoFFTライブラリを読み込む
#include "ErriezTM1637.h"               // TM1637（4桁7セグ＋キー入力などのモジュール）を扱うライブラリを読み込む

// --- ピン設定 ---
#define LED_PIN     6                   // NeoPixel LEDマトリックスのデータ信号を接続しているArduinoピン番号を指定
#define AUDIO_PIN   A0                  // マイクやライン入力など、アナログ音声信号が接続されているアナログピンA0を指定
#define DF_TX       10                  // DFPlayer Miniへ送信するTX用に使うArduinoのデジタルピン番号（Arduino側のTX）
#define DF_RX       11                  // DFPlayer Miniから受信するRX用に使うArduinoのデジタルピン番号（Arduino側のRX）
#define KP_CLK      3                   // TM1637モジュールのクロック線（CLK）を接続しているArduinoピン番号
#define KP_DIO      4                   // TM1637モジュールのデータ線（DIO）を接続しているArduinoピン番号

// --- LEDマトリックス設定 ---
#define LED_WIDTH   32                  // LEDマトリックスの横方向のLED数（列数）を32に設定
#define LED_HEIGHT  8                   // LEDマトリックスの縦方向のLED数（行数）を8に設定
#define NUM_LEDS    (LED_WIDTH * LED_HEIGHT) // マトリックス全体のLED数を計算（32×8=256個）して定数として定義

// --- FFT設定 ---
#define SAMPLES         128             // FFTで使用するサンプル数（配列の長さ）を128に設定（2のべき乗である必要がある）
#define SAMPLING_FREQ   4000            // サンプリング周波数を4000Hz（1秒間に4000回サンプリング）に設定

// --- グローバル変数 ---
Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800); 
// **leds**: NeoPixelオブジェクトを生成。総LED数、信号ピン、LEDの色順（GRB）とプロトコル（800kHz）を指定

SoftwareSerial dfSerial(DF_RX, DF_TX);  // DFPlayer用のソフトウェアシリアルを作成（受信ピン、送信ピンの順番で指定）
DFRobotDFPlayerMini dfplayer;           // DFPlayer Miniを制御するためのオブジェクトを生成
TM1637 keypad(KP_CLK, KP_DIO);          // TM1637キー＆表示モジュール用のオブジェクトをクロックとデータピンで初期化

double vReal[SAMPLES];                  // FFT用の実数成分を格納する配列を定義（サンプル数と同じ長さ）
double vImag[SAMPLES];                  // FFT用の虚数成分を格納する配列を定義（最初は0を入れて使う）
ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, SAMPLING_FREQ);
// arduinoFFTクラスのインスタンスを生成し、実部配列・虚部配列・サンプル数・サンプリング周波数を登録

unsigned int sampling_period_us;        // サンプリング間隔をマイクロ秒単位で保持する変数（1サンプルにかける時間）
unsigned long microseconds;            // micros()で取得した現在のマイクロ秒時間を一時的に保存するための変数

void setup() {                          // Arduinoが起動した時に一度だけ実行される初期化用関数
  Serial.begin(9600);                   // ハードウェアシリアル（USB経由）を9600bpsで開始し、デバッグ表示などに使う

  leds.begin();                         // NeoPixelライブラリを初期化し、LED制御を開始できる状態にする
  leds.setBrightness(40);               // 全体の明るさを0〜255の範囲で設定（ここでは40と控えめな明るさにする）
  leds.show();                          // 現在のLEDバッファの内容を実際のLEDに反映（初期状態では全消灯）

  dfSerial.begin(9600);                 // DFPlayer用ソフトウェアシリアル通信を9600bpsで開始
  if (!dfplayer.begin(dfSerial)) {      // DFPlayer Miniの初期化を行い、接続に成功したかをチェック
    Serial.println("DFPlayer Mini not found!"); // DFPlayerが見つからなかった場合にエラーメッセージをシリアルモニタに表示
    while (1);                          // 無限ループに入り、処理を停止（致命的エラーとしてスケッチを進めない）
  }
  dfplayer.volume(20);                  // DFPlayerの音量を0〜30の範囲で設定（ここでは少し小さめの20）
  Serial.println("DFPlayer ready");     // DFPlayerの初期化が完了したことをシリアルモニタに表示

  keypad.begin();                       // TM1637キー＆表示モジュールの初期化処理を実行
  keypad.setBrightness(7);              // TM1637の表示の明るさを0〜7で設定（最大の7に設定）

  sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQ));
  // サンプリング周波数から1サンプルあたりの周期を秒で計算し、それをマイクロ秒に変換して四捨五入
  // SAMPLING_FREQ=4000Hzの場合、1/4000秒=0.00025秒=250マイクロ秒がサンプリング間隔になる
}

void loop() {                           // setup()の後に繰り返し実行されるメインループ関数
  // --- キーパッド入力処理 ---
  uint8_t keys = keypad.getKeys();      // TM1637からキー入力の状態を読み取り、各ビットにキーのON/OFF情報を格納
  if (keys != 0xFF) {                   // 0xFFは「何も押されていない」状態とみなし、それ以外なら何かが押されたと判断
    for (int i = 0; i < 8; i++) {       // 8個ぶん（0〜7）のキーを順番にチェックするループを開始
      if (!(keys & (1 << i))) {         // i番目のビットが0なら、そのキーが押されたと判定（ライブラリ仕様に合わせた判断）
        Serial.print("Key pressed: ");  // シリアルモニタに「どのキーが押されたか」を表示するためのメッセージ出力
        Serial.println(i);              // 実際に押されたキー番号をシリアルモニタに表示

        dfplayer.play(i + 1);           // DFPlayerに対し、(i+1)番目のMP3ファイルの再生を指示（ファイル番号は1始まり想定）
      }
    }
  }

  // --- 音声サンプリング ---
  for (int i = 0; i < SAMPLES; i++) {   // FFT用に必要なサンプル数（SAMPLES=128）だけ繰り返して音を取り込むループ
    microseconds = micros();            // 現在のマイクロ秒カウンタを取得し、サンプリング開始時間として記録
    vReal[i] = analogRead(AUDIO_PIN);   // アナログピンから音声信号の電圧を読み取り、実部配列vRealのi番目に格納
    vImag[i] = 0;                       // 虚部配列vImagのi番目は0で初期化（実数信号のみなので虚部は0スタート）

    while (micros() - microseconds < sampling_period_us);
    // ループの残り時間を待機し、サンプリング間隔（sampling_period_us）になるまで空回ししてサンプリング周期を一定に保つ
  }

  // --- FFT実行（小文字関数名に修正） ---
  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  // ハミング窓関数を使用して時間領域データに窓掛けを行い、スペクトルのリークを抑える処理を実行
  // FFT_FORWARDは順方向FFTを行うことを示す定数

  FFT.compute(FFT_FORWARD);
  // 実部配列vRealと虚部配列vImagのデータに対して実際にFFT計算を行い、周波数領域の複素スペクトルに変換

  FFT.complexToMagnitude();
  // FFT結果の複素数（実部と虚部のペア）を、それぞれの大きさ（振幅＝√(Re^2 + Im^2)）に変換してvRealに格納する
  // この後はvReal配列に周波数ごとの振幅が入っている状態になる

  // --- LED表示 ---
  leds.clear();                         // すべてのLEDのバッファを消灯状態にリセット（実際のLEDはまだ変化しない）

  for (int x = 0; x < LED_WIDTH; x++) { // LEDマトリックスの横方向の各列（0〜31）に対して順に処理するループ
    int bin = map(x, 0, LED_WIDTH - 1, 2, SAMPLES / 2 - 1);
    // 現在の列xを、FFT結果の周波数ビン番号に対応付ける
    // 0列→ビン2、最後の列（31）→ビン(SAMPLES/2-1=63)になるように線形マッピング
    // 低周波側のビン0,1はノイズが出やすいので、2から始める設計

    int level = map(vReal[bin], 0, 100, 0, LED_HEIGHT);
    // 該当周波数ビンの振幅値vReal[bin]（0〜100と仮定）をLEDの高さ（0〜LED_HEIGHT=8）の段数に変換
    // 振幅が大きいほど上までバーが伸びるグラフ表示になる

    level = constrain(level, 0, LED_HEIGHT);
    // 何らかの理由で計算結果が範囲外になった場合でも、0〜LED_HEIGHTに収まるように制限する

    for (int y = 0; y < level; y++) {   // その列の下から「level」段ぶんのLEDを点灯させるためのループ
      int idx = getLedIndex(x, y);      // マトリックスの座標(x,y)を、物理的な配線順に対応した1次元インデックスに変換
      leds.setPixelColor(idx, leds.Color(0, 150, 255));
      // 計算したインデックスのLEDをシアン寄りの色（R=0, G=150, B=255）で点灯させる
    }
  }

  leds.show();                          // ここまでに設定したLEDの色を一括して実際のLEDに反映させる
  delay(30);                            // 表示更新の間隔を30ミリ秒空けて、描画を滑らかにしつつ処理の負荷を軽減
}

// --- LEDマトリックスのインデックス変換（ジグザグ配線対応） ---
int getLedIndex(int x, int y) {         // マトリックスの論理座標(x,y)を、実際のNeoPixel配列のインデックスに変換する関数
  if (x % 2 == 0) {                     // xが偶数列（0,2,4, ...）かどうかを判定
    return x * LED_HEIGHT + y;
    // 偶数列は下から上に順番に配線されている想定なので、そのままyを足してインデックスを計算
    // 例: x=0なら0〜7、x=2なら16〜23のように昇順配列
  } else {                              // xが奇数列（1,3,5, ...）の場合の処理
    return x * LED_HEIGHT + (LED_HEIGHT - 1 - y);
    // 奇数列は上から下に逆向きに配線されているジグザグ構造を想定し、yを反転してインデックスを計算
    // これにより、論理的には縦方向にそろったマトリックスとして扱える
  }
}
