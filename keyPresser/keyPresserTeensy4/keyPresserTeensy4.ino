#include <USBHost_t36.h>

// -------------------------
// USB Host objects
// -------------------------
USBHost usb;
USBHub hub1(usb);
KeyboardController keyboard(usb);

// REQUIRED for keyboards
USBHIDParser hid1(usb);
USBHIDParser hid2(usb);
USBHIDParser hid3(usb);

// -------------------------
// Solenoid pins
// -------------------------
constexpr int SOL0_PIN = 2;
constexpr int SOL1_PIN = 3;

// -------------------------
// Timing (microseconds)
// Tune these carefully
// -------------------------
constexpr uint32_t PULSE_US = 35000; // solenoid on       // experiment down to 20000
constexpr uint32_t GAP_US   = 35000; // off between bits  // experiment down to 10000

// ISR tick period
constexpr uint32_t TICK_US = 50;

// Derived timing (ticks)
constexpr uint32_t PULSE_TICKS = PULSE_US / TICK_US;
constexpr uint32_t GAP_TICKS   = GAP_US   / TICK_US;

// -------------------------
// Ring buffer
// -------------------------
constexpr int BUF_SIZE = 256;
volatile uint8_t buf[BUF_SIZE];
volatile uint8_t head = 0;
volatile uint8_t tail = 0;

inline bool bufEmpty() {
  return head == tail;
}

inline bool bufFull() {
  return ((head + 1) % BUF_SIZE) == tail;
}

inline void bufPush(uint8_t v) {
  if (bufFull()) return; // drop if overflow
  buf[head] = v;
  head = (head + 1) % BUF_SIZE;
}

inline bool bufPop(uint8_t &v) {
  if (bufEmpty()) return false;
  v = buf[tail];
  tail = (tail + 1) % BUF_SIZE;
  return true;
}

// -------------------------
// Solenoid state machine
// -------------------------
enum PulseState {
  IDLE,
  PULSE_ON,
  PULSE_OFF
};

volatile PulseState pulseState = IDLE;
volatile uint32_t tickCount = 0;

uint8_t currentByte = 0;
volatile int bitIndex = -1;
volatile int activePin = -1;

// -------------------------
// Timer
// -------------------------
IntervalTimer solTimer;

// -------------------------
// ASCII mapping
// -------------------------
char keycodeToASCII(int key) {
  if (key >= 0x20 && key <= 0x7E) return (char)key;
  if (key == '\n') return '\n';
  return 0;
}

// -------------------------
// Keyboard callbacks
// -------------------------
void onKeyPress(int key) {
  // Printable ASCII
  char c = keycodeToASCII(key);
  if (c) {
    Serial.print("Queued ASCII: ");
    Serial.println(c);
  } else {
    Serial.print("Queued keycode: 0x");
    Serial.println(key, HEX);
  }

  bufPush((uint8_t)key);  // send raw keycode to solenoids
}


void onKeyRelease(int) {
  // intentionally unused
}

// -------------------------
// Solenoid ISR (HARD REAL-TIME)
// -------------------------
void solenoidISR() {
  switch (pulseState) {

    case IDLE:
      if (!bufEmpty()) {
        bufPop(currentByte);
        bitIndex = 7;
        tickCount = 0;
        pulseState = PULSE_ON;
      }
      break;

    case PULSE_ON:
      if (tickCount == 0) {
        bool bit = (currentByte >> bitIndex) & 1;
        activePin = bit ? SOL1_PIN : SOL0_PIN;
        digitalWriteFast(activePin, HIGH);
        // Serial.print("PULSE ON pin "); Serial.print(activePin); Serial.print(" bit "); Serial.println(bit); //Debugging
      }

      if (++tickCount >= PULSE_TICKS) {
        digitalWriteFast(activePin, LOW);
        tickCount = 0;
        pulseState = PULSE_OFF;
      }
      break;

    case PULSE_OFF:
      if (++tickCount >= GAP_TICKS) {
        tickCount = 0;
        bitIndex--;
        pulseState = (bitIndex < 0) ? IDLE : PULSE_ON;
      }
      break;
  }
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

  keyboard.attachPress(onKeyPress);
  keyboard.attachRelease(onKeyRelease);

  usb.begin();

  // Start solenoid timer
  solTimer.begin(solenoidISR, TICK_US);

  Serial.println("USB host ready — ISR-driven solenoid engine running");
}

// -------------------------
// Loop
// -------------------------
void loop() {
  usb.Task();   // USB MUST be serviced frequently
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
