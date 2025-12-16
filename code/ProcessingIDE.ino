/**
 * 使用ライブラリ:
 * - processing.sound
 * - processing.serial
 *
 * 接続:
 * - ArduinoとUSB接続
 * - Arduino側でシリアル通信を受信し、LEDマトリクスに表示
 */

import processing.serial.*;
import processing.sound.*;

AudioIn in;
FFT fft;
int bands = 16; // バンド数（LED列と合わせる）
float[] spectrum = new float[bands];
Serial myPort;

void setup() {
  size(512, 200);
  frameRate(30); // 安定した描画速度

  // オーディオ入力とFFT初期化
  in = new AudioIn(this, 0);
  fft = new FFT(this, bands);
  in.start();
  fft.input(in);

  // シリアルポートの一覧を表示（必要に応じて番号を変更）
  printArray(Serial.list());
  myPort = new Serial(this, Serial.list()[2], 9600); // ← [2]は環境に応じて変更
}

void draw() {
  background(0);

  // FFT解析
  fft.analyze(spectrum);

  // スペクトラムデータを文字列に変換して送信
  String data = "";
  for (int i = 0; i < bands; i++) {
    int level = int(spectrum[i] * 1200); // スケーリング係数は調整可能
    level = constrain(level, 0, 255);
    data += level;
    if (i < bands - 1) data += ",";
  }
  myPort.write(data + "\n");

  // 可視化（オプション）
  for (int i = 0; i < bands; i++) {
    float h = spectrum[i] * height * 15;
    rect(i * (width / bands), height - h, width / bands, h);
  }
}
