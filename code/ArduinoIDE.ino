#include <Adafruit_NeoPixel.h>
#include <NewPing.h>
#include <MIDI.h>

#define PIN 6
#define NUM_PIXELS 512  
#define BRIGHTNESS 80

#define TRIG_PIN 10
#define ECHO_PIN 9
#define MAX_DISTANCE 30  // cm

Adafruit_NeoPixel strip(NUM_PIXELS, PIN, NEO_GRB + NEO_KHZ800);
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);
MIDI_CREATE_DEFAULT_INSTANCE();

String input = "";
int levels[16];

int lastNote = -1;
unsigned long lastPingTime = 0;
const unsigned long pingInterval = 100; // ms

void setup() {
  Serial.begin(9600);
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();
  MIDI.begin(MIDI_CHANNEL_1);
}

void loop() {
  // --- FFTデータ受信＆LED表示 ---
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      parseData(input);
      input = "";
      displayBars();
    } else {
      input += c;
    }
  }

  // --- 超音波センサー処理（一定間隔で） ---
  if (millis() - lastPingTime > pingInterval) {
    lastPingTime = millis();
    int distance = sonar.ping_cm();

    if (distance > 0) {
      int note = getNoteFromDistance(distance);

      if (note != lastNote) {
        if (lastNote != -1) {
          MIDI.sendNoteOff(lastNote, 0, 1);
        }
        MIDI.sendNoteOn(note, 100, 1);
        lastNote = note;
      }
    } else {
      if (lastNote != -1) {
        MIDI.sendNoteOff(lastNote, 0, 1);
        lastNote = -1;
      }
    }
  }
}

void parseData(String data) {
  int index = 0;
  int lastIndex = 0;
  for (int i = 0; i < 16; i++) {
    index = data.indexOf(',', lastIndex);
    if (index == -1 && i < 15) return;
    String val = (i < 15) ? data.substring(lastIndex, index) : data.substring(lastIndex);
    levels[i] = constrain(val.toInt() / 4, 0, 32); 
    lastIndex = index + 1;
  }
}

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

int getPixelIndex(int x, int y) {
  if (x % 2 == 0) {
    return x * 32 + y;
  } else {
    return x * 32 + (31 - y);
  }
}

uint32_t getColorForLevel(int level) {
  int r = map(level, 0, 31, 0, 255);
  int g = 255 - abs(16 - level) * 8;
  int b = 255 - r;
  return strip.Color(r, g, b);
}

int getNoteFromDistance(int d) {
  if (d < 6) return 60;     // C4
  else if (d < 10) return 62; // D4
  else if (d < 14) return 64; // E4
  else if (d < 18) return 65; // F4
  else if (d < 22) return 67; // G4
  else if (d < 26) return 69; // A4
  else return 71;            // B4
}
