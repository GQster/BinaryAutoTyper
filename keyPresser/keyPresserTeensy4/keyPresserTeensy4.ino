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
// Protocol values (custom)
// -------------------------
constexpr uint8_t PROTO_RIGHT_ARROW = 0x10;
constexpr uint8_t PROTO_LEFT_ARROW  = 0x11;
constexpr uint8_t PROTO_DOWN_ARROW  = 0x12;
constexpr uint8_t PROTO_UP_ARROW    = 0x13;
constexpr uint8_t PROTO_BACKSPACE   = 0x14;
constexpr uint8_t PROTO_ENTER       = 0x15;
constexpr uint8_t PROTO_TAB         = 0x16;
constexpr uint8_t PROTO_ESCAPE      = 0x17;
constexpr uint8_t PROTO_DELETE      = 0x18;
constexpr uint8_t PROTO_INSERT      = 0x19;
constexpr uint8_t PROTO_HOME        = 0x1A;
constexpr uint8_t PROTO_END         = 0x1B;
constexpr uint8_t PROTO_PAGE_UP     = 0x1C;
constexpr uint8_t PROTO_PAGE_DOWN   = 0x1D;
// Use non-printable protocol values for F1..F12 so they aren't misread as digits
constexpr uint8_t PROTO_F1 = 0x02;
constexpr uint8_t PROTO_F2 = 0x03;
constexpr uint8_t PROTO_F3 = 0x04;
constexpr uint8_t PROTO_F4 = 0x05;
constexpr uint8_t PROTO_F5 = 0x06;
constexpr uint8_t PROTO_F6 = 0x07;
constexpr uint8_t PROTO_F7 = 0x08;
constexpr uint8_t PROTO_F8 = 0x09;
constexpr uint8_t PROTO_F9 = 0x0A;
constexpr uint8_t PROTO_F10 = 0x0B;
constexpr uint8_t PROTO_F11 = 0x0C;
constexpr uint8_t PROTO_F12 = 0x0D;

// Helper: map raw HID usage to protocol byte. returns 0 and found=false if no mapping.
uint8_t mapRawToProto(uint8_t raw, bool &found) {
  found = true;
  switch (raw) {
    // Some host controllers report extended/raw values for arrows — accept both sets
    case 0x4F: return PROTO_RIGHT_ARROW; // RIGHT
    case 0x50: return PROTO_LEFT_ARROW;  // LEFT
    case 0x51: return PROTO_DOWN_ARROW;  // DOWN
    case 0x52: return PROTO_UP_ARROW;    // UP
    // Alternate/raw codes observed on some keyboards/host stacks
    case 0xD7: return PROTO_RIGHT_ARROW;
    case 0xD8: return PROTO_LEFT_ARROW;
    case 0xD9: return PROTO_DOWN_ARROW;
    case 0xDA: return PROTO_UP_ARROW;
    // More alternate/raw codes observed for navigation keys
    case 0xD2: return PROTO_HOME;
    case 0xD3: return PROTO_PAGE_UP;
    case 0xD4: return PROTO_DELETE;
    case 0xD5: return PROTO_END;
    case 0xD6: return PROTO_PAGE_DOWN;
    case 0x2A: return PROTO_BACKSPACE;   // Backspace (HID 0x2A)
    case 0x7F: return PROTO_BACKSPACE;   // ASCII DEL -> Backspace
    case 0x28: return PROTO_ENTER;       // Enter (Return)
      // Alternate/ASCII codes sometimes seen for Enter: LF (0x0A) and CR (0x0D)
      case 0x0A: return PROTO_ENTER;       // Line Feed
      case 0x0D: return PROTO_ENTER;       // Carriage Return
    case 0x2B: return PROTO_TAB;         // Tab
      case 0x09: return PROTO_TAB;         // ASCII Tab (observed)
    case 0x29: return PROTO_ESCAPE;      // Escape (HID)
    case 0x1B: return PROTO_ESCAPE;      // ASCII ESC -> Escape
    case 0x4C: return PROTO_DELETE;      // Delete
    case 0x49: return PROTO_INSERT;      // Insert
    case 0xD1: return PROTO_INSERT;      // Alternate/raw Insert observed
    case 0x4A: return PROTO_PAGE_UP;     // Page Up
    case 0x4B: return PROTO_PAGE_DOWN;   // Page Down
    case 0x4D: return PROTO_HOME;        // Home
    case 0x4E: return PROTO_END;         // End
    // Standard HID F1..F12 usages (0x3A..0x45)
    case 0x3A: return PROTO_F1;
    case 0x3B: return PROTO_F2;
    case 0x3C: return PROTO_F3;
    case 0x3D: return PROTO_F4;
    case 0x3E: return PROTO_F5;
    case 0x3F: return PROTO_F6;
    case 0x40: return PROTO_F7;
    case 0x41: return PROTO_F8;
    case 0x42: return PROTO_F9;
    case 0x43: return PROTO_F10;
    case 0x44: return PROTO_F11;
    case 0x45: return PROTO_F12;
    // Alternate/raw codes observed for F1..F12
    case 0xC2: return PROTO_F1;
    case 0xC3: return PROTO_F2;
    case 0xC4: return PROTO_F3;
    case 0xC5: return PROTO_F4;
    case 0xC6: return PROTO_F5;
    case 0xC7: return PROTO_F6;
    case 0xC8: return PROTO_F7;
    case 0xC9: return PROTO_F8;
    case 0xCA: return PROTO_F9;
    case 0xCB: return PROTO_F10;
    case 0xCC: return PROTO_F11;
    case 0xCD: return PROTO_F12;
    default:
      found = false;
      return 0;
  }
}

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
volatile uint32_t lastKeyTime = 0;
constexpr uint32_t DEBOUNCE_MS = 10; // ignore presses <10ms apart

void onKeyPress(int key) {
    if (millis() - lastKeyTime < DEBOUNCE_MS) return;
    lastKeyTime = millis();
    uint8_t raw = (uint8_t)key;

    // If the host callback gives a HID modifier usage (0xE0..0xE7), map to 128..135
    if (raw >= 0xE0 && raw <= 0xE7) {
      uint8_t mod_val = 128 + (raw - 0xE0); // 0xE0->128 ... 0xE7->135
      Serial.print("onKeyPress MOD raw=0x"); Serial.print(raw, HEX);
      Serial.print(" -> mod=0x"); Serial.println(mod_val, HEX);
      bufPush(mod_val);
      return;
    }

    // If printable ASCII, send the ASCII code ON PRESS only (handles Shift+number -> symbols)
    if (raw >= 0x20 && raw <= 0x7E) {
      uint8_t ascii_val = raw & 0x7F;
      Serial.print("onKeyPress ASCII raw=0x"); Serial.print(raw, HEX);
      Serial.print(" -> ascii=0x"); Serial.println(ascii_val, HEX);
      bufPush(ascii_val);
      return;
    }
    // Handle control ASCII and alternate/raw HID usages via mapping first
    bool found;
    uint8_t proto = mapRawToProto(raw, found);
    if (found) {
      Serial.print("onKeyPress MAP raw=0x"); Serial.print(raw, HEX);
      Serial.print(" -> proto=0x"); Serial.println(proto, HEX);
      bufPush(proto);
      return;
    }

    // Some hosts deliver Ctrl+letter as ASCII control codes (0x01..0x1A).
    // Translate these into: send modifier press (LEFT_CONTROL proto=128),
    // then send the corresponding lowercase ASCII letter.
    if (raw >= 0x01 && raw <= 0x1A) {
      uint8_t letter_index = raw - 1; // 0 -> 'a'
      uint8_t ascii_val = (uint8_t)('a' + letter_index);
      uint8_t ctrl_proto = 128; // left control
      Serial.print("onKeyPress CTL raw=0x"); Serial.print(raw, HEX);
      Serial.print(" -> ctrl+ascii=0x"); Serial.println(ascii_val, HEX);
      // send ctrl down, then the ascii byte
      bufPush(ctrl_proto);
      bufPush(ascii_val);
      return;
    }

    // Unknown non-printable: ignore to avoid sending raw HID values that confuse receiver
    Serial.print("onKeyPress IGNORE raw=0x"); Serial.println(raw, HEX);
}

void onKeyRelease(int key) {
    if (millis() - lastKeyTime < DEBOUNCE_MS) return;
    lastKeyTime = millis();
    uint8_t raw = (uint8_t)key;

    // If modifier release, send same modifier value so receiver toggles state
    if (raw >= 0xE0 && raw <= 0xE7) {
      uint8_t mod_val = 128 + (raw - 0xE0);
      Serial.print("onKeyRelease MOD raw=0x"); Serial.print(raw, HEX);
      Serial.print(" -> mod=0x"); Serial.println(mod_val, HEX);
      bufPush(mod_val);
      return;
    }

    // Map selected non-ASCII HID usages to protocol bytes on release as well
    // For non-modifier mapped keys (arrows, backspace, etc.) we handled on press,
    // so ignore their releases to avoid double-sending.
    bool foundRel;
    uint8_t protoRel = mapRawToProto(raw, foundRel);
    if (foundRel) {
      Serial.print("onKeyRelease MAP ignored raw=0x"); Serial.print(raw, HEX);
      Serial.print(" -> proto=0x"); Serial.println(protoRel, HEX);
      return;
    }

    // Handle ASCII control codes (Ctrl+letter) on release by forwarding the
    // corresponding ctrl modifier release so receiver will toggle it off.
    if (raw >= 0x01 && raw <= 0x1A) {
      uint8_t ctrl_proto = 128; // left control
      Serial.print("onKeyRelease CTL raw=0x"); Serial.println(raw, HEX);
      bufPush(ctrl_proto);
      return;
    }

    // Non-modifier releases are ignored for the ASCII protocol
    if (raw >= 0x20 && raw <= 0x7F) {
      Serial.print("onKeyRelease ASCII ignored raw=0x"); Serial.println(raw, HEX);
      return;
    }

    // Map selected non-ASCII HID usages to protocol bytes on release as well
    // For non-modifier mapped keys (arrows, backspace, etc.) we handled on press,
    // so ignore their releases to avoid double-sending.
    bool found;
    uint8_t proto = mapRawToProto(raw, found);
    if (found) {
      Serial.print("onKeyRelease MAP ignored raw=0x"); Serial.print(raw, HEX);
      Serial.print(" -> proto=0x"); Serial.println(proto, HEX);
      return;
    }

    Serial.print("onKeyRelease IGNORE raw=0x"); Serial.println(raw, HEX);
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
void loop() { usb.Task(); pollModifiers(); }

// Poll for modifier state changes (some keyboards send modifiers in the report
// modifier byte instead of as separate key press events). This captures those
// changes and forwards them as the protocol modifier values (128..135).
volatile uint8_t _last_mods = 0;

void pollModifiers() {
  // KeyboardController provides getModifiers() returning a modifier bitmask
  uint8_t mods = 0;
  // guard in case the method isn't available on some library versions
  #if defined(__arm__) || defined(ARDUINO)
  mods = keyboard.getModifiers();
  #endif

  if (mods == _last_mods) return;
  uint8_t changed = mods ^ _last_mods;
  for (uint8_t i = 0; i < 8; ++i) {
    uint8_t mask = (1 << i);
    if (changed & mask) {
      uint8_t mod_val = 128 + i; // map bit to 128..135
      if (mods & mask) {
        Serial.print("MOD POLL PRESS bit="); Serial.print(i);
        Serial.print(" -> mod=0x"); Serial.println(mod_val, HEX);
        bufPush(mod_val);
      } else {
        Serial.print("MOD POLL RELEASE bit="); Serial.print(i);
        Serial.print(" -> mod=0x"); Serial.println(mod_val, HEX);
        bufPush(mod_val);
      }
    }
  }
  _last_mods = mods;
}

