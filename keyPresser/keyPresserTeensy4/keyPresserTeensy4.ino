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
// Pins
// -------------------------
constexpr int SOL0_PIN = 2;
constexpr int SOL1_PIN = 3;
constexpr int DRV8833_ENABLE_PIN = 5;

// -------------------------
// Timing (microseconds)
// Tune these carefully
// -------------------------
constexpr uint32_t PULSE_US = 25000; // solenoid on       // experiment down to 20000
constexpr uint32_t GAP_US   = 10000; // off between bits

// ISR tick period
constexpr uint32_t TICK_US = 25;

// Derived timing (ticks)
constexpr uint32_t PULSE_TICKS = PULSE_US / TICK_US;
constexpr uint32_t GAP_TICKS   = GAP_US   / TICK_US;

// -------------------------
// Ring buffer
// -------------------------
constexpr int BUF_SIZE = 1024;
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
  if (bufFull()) {
    // Drop oldest instead of new
    tail = (tail + 1) % BUF_SIZE;
  }
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
volatile int activePin = SOL0_PIN;

// -------------------------
// Timer
// -------------------------
IntervalTimer solTimer;

// -------------------------
// Modifiers
// -------------------------
constexpr uint8_t KEY_MOD_LCTRL  = 0x01;
constexpr uint8_t KEY_MOD_LSHIFT = 0x02;
constexpr uint8_t KEY_MOD_LALT   = 0x04;
constexpr uint8_t KEY_MOD_LGUI   = 0x08;
constexpr uint8_t KEY_MOD_RCTRL  = 0x10;
constexpr uint8_t KEY_MOD_RSHIFT = 0x20;
constexpr uint8_t KEY_MOD_RALT   = 0x40;
constexpr uint8_t KEY_MOD_RGUI   = 0x80;

uint8_t keyboard_modifiers = 0;

// -------------------------
// HID -> ASCII (full keyboard)
// -------------------------
char hidToAscii(uint8_t key, uint8_t modifiers) {
  bool shift = modifiers & (KEY_MOD_LSHIFT | KEY_MOD_RSHIFT);

  // Letters A-Z (HID 4–29)
  if (key >= 4 && key <= 29) {
    char c = 'a' + (key - 4);
    return shift ? (c - 32) : c;  // uppercase if shift
  }

  // Numbers 1-9 (HID 30–38)
  if (key >= 30 && key <= 38) {
    if (!shift) return '1' + (key - 30);
    const char shifted[] = "!@#$%^&*(";
    return shifted[key - 30];
  }

  // Number 0 (HID 39)
  if (key == 39) return shift ? ')' : '0';

  // Symbols (HID 45–53)
  if (key >= 45 && key <= 53) {
    const char unshifted[] = "-=[]\\;\'`,./";
    const char shifted[]   = "_+{}|:\"~<>?";
    return shift ? shifted[key - 45] : unshifted[key - 45];
  }

  // Space, Enter, Tab
  if (key == 44) return ' ';
  if (key == 40) return '\n';
  if (key == 43) return '\t';

  // Function keys F1-F12 (HID 58–69)
  if (key >= 58 && key <= 69) {
    return 0xF1 + (key - 58); // placeholder, can map to solenoid codes
  }

  // Arrows and special keys
  switch (key) {
    case 79: return 0xF0; // Right Arrow
    case 80: return 0xF1; // Left Arrow
    case 81: return 0xF2; // Down Arrow
    case 82: return 0xF3; // Up Arrow
    case 76: return 0xF4; // Delete
    case 71: return 0xF5; // Home
    case 74: return 0xF6; // End
    case 75: return 0xF7; // Page Down
    case 73: return 0xF8; // Page Up
    default: return 0;      // unsupported key
  }
}

// -------------------------
// Keyboard callbacks
// -------------------------
void onKeyPress(int key) {
  char c = keyboard.getKey();  // returns ASCII or 0 if non-printable
  if (c) {
    bufPush((uint8_t)c);
    // Serial.print("Queued ASCII: ");    // DONT PRINT IN ISR, CAN BE BLOCKING
    // Serial.println(c);
  } else {
    // Serial.println("Unsupported keycode");
    // Serial.println(key, HEX);
  }
}


void onKeyRelease(int) {
  // nothing needed for printable keys
}

// -------------------------
// Solenoid ISR (HARD REAL-TIME)
// -------------------------
void solenoidISR() {
  switch (pulseState) {
    case IDLE:
      if (!bufEmpty()) {
        digitalWriteFast(DRV8833_ENABLE_PIN, HIGH);
        bufPop(currentByte);
        bitIndex = 7;
        tickCount = 0;
        pulseState = PULSE_ON;
      } else {
        digitalWriteFast(DRV8833_ENABLE_PIN, LOW); // sleep when idle
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
  pinMode(SOL0_PIN, OUTPUT); digitalWrite(SOL0_PIN, LOW);
  pinMode(SOL1_PIN, OUTPUT); digitalWrite(SOL1_PIN, LOW);


  // Enable the DRV8833 board
  pinMode(DRV8833_ENABLE_PIN, OUTPUT); digitalWrite(DRV8833_ENABLE_PIN, HIGH);

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
