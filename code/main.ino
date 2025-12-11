import processing.serial.*;
import processing.sound.*;

AudioIn in;
FFT fft;
int bands = 8;
float[] spectrum = new float[bands];
Serial myPort;

void setup() {
  size(512, 200);
  in = new AudioIn(this, 0);
  fft = new FFT(this, bands);
  in.start();
  fft.input(in);

  printArray(Serial.list());
  myPort = new Serial(this, Serial.list()[2], 9600); // ← ポート番号を確認！
}

void draw() {
  background(0);
  fft.analyze(spectrum);

  // データを文字列に変換して送信（超スケーリング）
  String data = "";
  for (int i = 0; i < bands; i++) {
    int level = int(spectrum[i] * 1200); // 超敏感に反応！
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

  delay(30); // 少し速めに
}
