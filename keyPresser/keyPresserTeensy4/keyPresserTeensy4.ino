#include <USBHost_t36.h>

// -------------------------
// USB Host objects
// -------------------------
USBHost usb;
USBHub hub1(usb);
KeyboardController keyboard(usb);

// REQUIRED for keyboards to function
USBHIDParser hid1(usb);
USBHIDParser hid2(usb);
USBHIDParser hid3(usb);

// -------------------------
// Solenoid pins
// -------------------------
constexpr int SOL0_PIN = 2;
constexpr int SOL1_PIN = 3;

// Timing (ms)
constexpr int PULSE_MS = 35;  // experiment down to 15-20?
constexpr int GAP_MS   = 35;  // experimetn down to 10

// Track last key to suppress repeats
uint8_t lastKey = 0;

// -------------------------
// Solenoid helpers
// -------------------------
void pulse(int pin) {
  digitalWrite(pin, HIGH);
  delay(PULSE_MS);
  digitalWrite(pin, LOW);
  delay(GAP_MS);
}

void sendByte(uint8_t byteVal) {
  for (int i = 7; i >= 0; i--) {
    pulse((byteVal >> i) & 1 ? SOL1_PIN : SOL0_PIN);
  }
}

// -------------------------
// HID → ASCII (with shift)
// -------------------------
char hidToAscii(uint8_t key, uint8_t mods) {
  bool shift = mods & (MODIFIERKEY_SHIFT | MODIFIERKEY_RIGHT_SHIFT);

  // A–Z (HID 4–29)
  if (key >= 4 && key <= 29) {
    char c = 'a' + (key - 4);
    return shift ? c - 32 : c;
  }

  // 1–9,0 (HID 30–39)
  const char nums[] = "1234567890";
  const char shifted[] = "!@#$%^&*()";
  if (key >= 30 && key <= 39)
    return shift ? shifted[key - 30] : nums[key - 30];

  switch (key) {
    case 44: return ' ';        // space
    case 40: return '\n';       // enter
    case 45: return shift ? '_' : '-';
    case 46: return shift ? '+' : '=';
    case 47: return shift ? '{' : '[';
    case 48: return shift ? '}' : ']';
    case 49: return shift ? '|' : '\\';
    case 51: return shift ? ':' : ';';
    case 52: return shift ? '"' : '\'';
    case 53: return shift ? '~' : '`';
    case 54: return shift ? '<' : ',';
    case 55: return shift ? '>' : '.';
    case 56: return shift ? '?' : '/';
  }

  return 0;
}

// -------------------------
// RAW keyboard callbacks
// -------------------------
void onRawPress(uint8_t key) {
  if (key == lastKey) return;
  lastKey = key;

  uint8_t mods = keyboard.getModifiers();
  char c = hidToAscii(key, mods);

  if (c) {
    Serial.print("Sending: ");
    Serial.println(c);
    sendByte((uint8_t)c);
  }
}

void onRawRelease(uint8_t key) {
  if (key == lastKey) lastKey = 0;
}

// -------------------------
// Setup
// -------------------------
void setup() {
  pinMode(SOL0_PIN, OUTPUT);
  pinMode(SOL1_PIN, OUTPUT);
  digitalWrite(SOL0_PIN, LOW);
  digitalWrite(SOL1_PIN, LOW);

  Serial.begin(115200);
  while (!Serial && millis() < 2000) {}

  keyboard.attachRawPress(onRawPress);
  keyboard.attachRawRelease(onRawRelease);

  usb.begin();

  Serial.println("USB host ready, waiting for keyboard...");
}

// -------------------------
// Loop
// -------------------------
void loop() {
  usb.Task();
}






//// minimal test sketch, prints keystrokes over serial:
// #include <USBHost_t36.h>

// USBHost usb;
// USBHub hub1(usb);
// KeyboardController keyboard(usb);

// // REQUIRED or many keyboards will be silent
// USBHIDParser hid1(usb);
// USBHIDParser hid2(usb);
// USBHIDParser hid3(usb);

// void onRawPress(uint8_t keycode);
// void onRawRelease(uint8_t keycode);

// void setup() {
//   Serial.begin(115200);
//   while (!Serial && millis() < 2000) {}

//   keyboard.attachRawPress(onRawPress);
//   keyboard.attachRawRelease(onRawRelease);

//   usb.begin();

//   Serial.println("USB host ready — plug in keyboard");
// }

// void loop() {
//   usb.Task();   // REQUIRED
// }

// // -------------------------
// // Raw HID callbacks
// // -------------------------
// void onRawPress(uint8_t keycode) {
//   Serial.print("RAW PRESS: ");
//   Serial.print(keycode);

//   char c = hidToAscii(keycode);
//   if (c) {
//     Serial.print("  -> '");
//     Serial.print(c);
//     Serial.print("'");
//   }
//   Serial.println();
// }

// void onRawRelease(uint8_t keycode) {
//   Serial.print("RAW RELEASE: ");
//   Serial.println(keycode);
// }

// // -------------------------
// // Minimal HID → ASCII map
// // -------------------------
// char hidToAscii(uint8_t key) {
//   // A–Z (HID 4–29)
//   if (key >= 4 && key <= 29) return 'a' + (key - 4);

//   // 1–9, 0 (HID 30–39)
//   if (key >= 30 && key <= 38) return '1' + (key - 30);
//   if (key == 39) return '0';

//   if (key == 44) return ' ';    // space
//   if (key == 40) return '\n';   // enter

//   return 0;
// }
