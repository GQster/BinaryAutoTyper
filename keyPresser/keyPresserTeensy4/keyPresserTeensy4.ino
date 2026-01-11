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
constexpr uint32_t PULSE_US = 25000; // solenoid on 
constexpr uint32_t GAP_US   = 10000; // off between bits

// ISR tick period
constexpr uint32_t TICK_US = 25;

// Derived timing (ticks)
constexpr uint32_t PULSE_TICKS = PULSE_US / TICK_US;
constexpr uint32_t GAP_TICKS   = GAP_US   / TICK_US;

// -------------------------
// Modifier key bit masks
// -------------------------
constexpr uint8_t KEY_MOD_LCTRL  = 0x01;
constexpr uint8_t KEY_MOD_LSHIFT = 0x02;
constexpr uint8_t KEY_MOD_LALT   = 0x04;
constexpr uint8_t KEY_MOD_LGUI   = 0x08;
constexpr uint8_t KEY_MOD_RCTRL  = 0x10;
constexpr uint8_t KEY_MOD_RSHIFT = 0x20;
constexpr uint8_t KEY_MOD_RALT   = 0x40;
constexpr uint8_t KEY_MOD_RGUI   = 0x80;

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
// Keyboard callbacks
// -------------------------
void onKeyPress(int key) {
  uint8_t mods = keyboard.getModifiers();

  // Push modifiers first (stateful)
  for (int i = 0; i < 8; i++) {
    if (mods & (1 << i)) {
      bufPush(0xE0 + i);
    }
  }

  // Push key itself
  bufPush((uint8_t)key);
}

void onKeyRelease(int key) {
  uint8_t mods = keyboard.getModifiers();

  // Release all modifiers when they go away
  for (int i = 0; i < 8; i++) {
    if (!(mods & (1 << i))) {
      bufPush(0xE0 + i); // RP2040 will re-press or ignore safely
    }
  }
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
void loop() { usb.Task(); }
