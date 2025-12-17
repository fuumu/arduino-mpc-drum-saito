import serial
import rtmidi

# -------------------------------
# シリアルポートとボーレート設定
# -------------------------------

# Arduinoのシリアルポート名（環境に合わせて変更）
# 例: '/dev/tty.usbmodemXXXX'（Mac）や 'COM3'（Windows）
port = '/dev/tty.YOUR_ARDUINO_PORT_HERE'
baudrate = 115200

# -------------------------------
# MIDI出力ポートの初期化
# -------------------------------

midiout = rtmidi.MidiOut()
available_ports = midiout.get_ports()

if available_ports:
    midiout.open_port(0)  # 最初のMIDIポートを使用（IAC Driverなど）
    print(f"Connected to MIDI port: {available_ports[0]}")
else:
    print("No MIDI output ports found!")
    exit()

# -------------------------------
# Arduinoとのシリアル接続
# -------------------------------

try:
    ser = serial.Serial(port, baudrate)
    print(f"Connected to Arduino on {port}")
except serial.SerialException:
    print(f"Could not open serial port {port}")
    exit()

print("Listening for MIDI messages from Arduino...")

# -------------------------------
# メインループ：MIDIメッセージを転送
# -------------------------------

while True:
    if ser.in_waiting >= 3:
        msg = ser.read(3)
        midi_bytes = [b for b in msg]
        print(f"Sending MIDI: {midi_bytes}")
        midiout.send_message(midi_bytes)
