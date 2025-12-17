#include <Adafruit_NeoPixel.h>  // LED制御ライブラリ
#include <NewPing.h>            // 超音波センサー用ライブラリ
#include "MIDI.h"              // MIDI送信用ライブラリ

#define MIDI_CHANNEL_1 1

// --- LEDマトリクス設定 ---
#define LED_PIN 6                   // LEDデータピン
#define NUM_PIXELS 512          // LEDの総数（16列 × 32段）
#define BRIGHTNESS 80           // 明るさ（0〜255）

Adafruit_NeoPixel strip(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- 超音波センサー設定 ---
#define TRIG_PIN 10
#define ECHO_PIN 9
#define MAX_DISTANCE 30         // 測定最大距離（cm）

NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

// --- MIDI設定 ---
MIDI_CREATE_DEFAULT_INSTANCE(); // デフォルトMIDIインスタンス作成

// --- FFTデータ受信用 ---
String input = "";              // シリアルからの受信バッファ
int levels[16];                 // 各バンドのLED高さ（0〜32）

// --- 超音波MIDI制御用 ---
int lastNote = -1;              // 前回送信したノート番号
unsigned long lastPingTime = 0;
const unsigned long pingInterval = 100; // 測定間隔（ms）

void setup() {
  Serial.begin(115200);         // シリアル通信（Processingと同じボーレートに）
  strip.begin();                // LED初期化
  strip.setBrightness(BRIGHTNESS);
  strip.show();                 // 初期状態で全消灯
  MIDI.begin(MIDI_CHANNEL_1);   // MIDIチャンネル1で開始
}

void loop() {
  handleSerial();               // FFTデータ受信＆LED表示
  handleUltrasonic();           // 超音波センサー処理＆MIDI送信
}

// --- シリアルからFFTデータを受信し、LEDに表示 ---
void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      parseData(input);        // データを数値に変換
      input = "";
      displayBars();           // LED表示更新
    } else {
      input += c;
    }
  }
}

// --- 超音波センサーで距離を測定し、MIDIノートを送信 ---
void handleUltrasonic() {
  if (millis() - lastPingTime > pingInterval) {
    lastPingTime = millis();
    int distance = sonar.ping_cm();

    if (distance > 0) {
      int note = getNoteFromDistance(distance);

      if (note != lastNote) {
        if (lastNote != -1) {
          MIDI.sendNoteOff(lastNote, 0, 1); // 前のノートをオフ
        }
        MIDI.sendNoteOn(note, 100, 1);      // 新しいノートをオン
        lastNote = note;
      }
    } else {
      if (lastNote != -1) {
        MIDI.sendNoteOff(lastNote, 0, 1);   // 手が離れたらノートオフ
        lastNote = -1;
      }
    }
  }
}

// --- 受信した文字列を数値配列に変換 ---
void parseData(String data) {
  int index = 0;
  int lastIndex = 0;
  for (int i = 0; i < 16; i++) {
    index = data.indexOf(',', lastIndex);
    if (index == -1 && i < 15) return; // データ不足なら無視
    String val = (i < 15) ? data.substring(lastIndex, index) : data.substring(lastIndex);
    levels[i] = constrain(val.toInt() / 4, 0, 32); // 0〜255 → 0〜32にスケーリング
    lastIndex = index + 1;
  }
}

// --- LEDマトリクスにバーを表示 ---
void displayBars() {
  strip.clear();

  for (int x = 0; x < 16; x++) {
    int height = levels[x];
    for (int y = 0; y < height; y++) {
      int pixelIndex = getPixelIndex(x, y);
      uint32_t color = getColorForLevel(y);
      strip.setPixelColor(pixelIndex, color);
    }
  }

  strip.show();
}

// --- ジグザグ配線に対応したインデックス計算 ---
int getPixelIndex(int x, int y) {
  if (x % 2 == 0) {
    return x * 32 + y;
  } else {
    return x * 32 + (31 - y);
  }
}

// --- 高さに応じた色を生成（グラデーション） ---
uint32_t getColorForLevel(int level) {
  int r = map(level, 0, 31, 0, 255);
  int g = 255 - abs(16 - level) * 8;
  int b = 255 - r;
  return strip.Color(r, g, b);
}

// --- 距離に応じたMIDIノートを返す ---
int getNoteFromDistance(int d) {
  if (d < 6) return 60;     // C4
  else if (d < 10) return 62; // D4
  else if (d < 14) return 64; // E4
  else if (d < 18) return 65; // F4
  else if (d < 22) return 67; // G4
  else if (d < 26) return 69; // A4
  else return 71;            // B4
}
