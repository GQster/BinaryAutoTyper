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
constexpr int SOL0_PIN = 3;
constexpr int SOL1_PIN = 2;
constexpr int DRV8833_ENABLE_PIN = 5;

// -------------------------
// Timing (microseconds) (cant go faster)
// -------------------------
constexpr uint32_t PULSE_US = 25000; // solenoid on 
constexpr uint32_t GAP_US   = 15000; // off between bits

// ISR tick period
constexpr uint32_t TICK_US = 25;

// Derived timing (ticks)
constexpr uint32_t PULSE_TICKS = PULSE_US / TICK_US;
constexpr uint32_t GAP_TICKS   = GAP_US   / TICK_US;

// Frame structure
constexpr int START_SYMBOL = 2; // Fire both solenoids for start symbol

// -------------------------
// Protocol values
// 0x00-0x1F: Control chars (Ctrl+A=0x01 ... Ctrl+Z=0x1A)
// 0x20-0x7F: Printable ASCII
// 0x80-0x87: Modifier PRESS
// 0x88-0x8F: Modifier RELEASE  
// 0x90-0x9F: Navigation keys
// 0xA0-0xAB: Function keys F1-F12
// -------------------------

// Modifier PRESS
constexpr uint8_t PROTO_MOD_LCTRL_PRESS  = 0x80;
constexpr uint8_t PROTO_MOD_LSHIFT_PRESS = 0x81;
constexpr uint8_t PROTO_MOD_LALT_PRESS   = 0x82;
constexpr uint8_t PROTO_MOD_LGUI_PRESS   = 0x83;
constexpr uint8_t PROTO_MOD_RCTRL_PRESS  = 0x84;
constexpr uint8_t PROTO_MOD_RSHIFT_PRESS = 0x85;
constexpr uint8_t PROTO_MOD_RALT_PRESS   = 0x86;
constexpr uint8_t PROTO_MOD_RGUI_PRESS   = 0x87;

// Modifier RELEASE
constexpr uint8_t PROTO_MOD_LCTRL_REL  = 0x88;
constexpr uint8_t PROTO_MOD_LSHIFT_REL = 0x89;
constexpr uint8_t PROTO_MOD_LALT_REL   = 0x8A;
constexpr uint8_t PROTO_MOD_LGUI_REL   = 0x8B;
constexpr uint8_t PROTO_MOD_RCTRL_REL  = 0x8C;
constexpr uint8_t PROTO_MOD_RSHIFT_REL = 0x8D;
constexpr uint8_t PROTO_MOD_RALT_REL   = 0x8E;
constexpr uint8_t PROTO_MOD_RGUI_REL   = 0x8F;

// Navigation keys
constexpr uint8_t PROTO_RIGHT_ARROW = 0x90;
constexpr uint8_t PROTO_LEFT_ARROW  = 0x91;
constexpr uint8_t PROTO_DOWN_ARROW  = 0x92;
constexpr uint8_t PROTO_UP_ARROW    = 0x93;
constexpr uint8_t PROTO_BACKSPACE   = 0x94;
constexpr uint8_t PROTO_ENTER       = 0x95;
constexpr uint8_t PROTO_TAB         = 0x96;
constexpr uint8_t PROTO_ESCAPE      = 0x97;
constexpr uint8_t PROTO_DELETE      = 0x98;
constexpr uint8_t PROTO_INSERT      = 0x99;
constexpr uint8_t PROTO_HOME        = 0x9A;
constexpr uint8_t PROTO_END         = 0x9B;
constexpr uint8_t PROTO_PAGE_UP     = 0x9C;
constexpr uint8_t PROTO_PAGE_DOWN   = 0x9D;
constexpr uint8_t PROTO_CLEAR_BUFFER = 0x9E;  // Emergency clear

// Function keys
constexpr uint8_t PROTO_F1  = 0xA0;
constexpr uint8_t PROTO_F2  = 0xA1;
constexpr uint8_t PROTO_F3  = 0xA2;
constexpr uint8_t PROTO_F4  = 0xA3;
constexpr uint8_t PROTO_F5  = 0xA4;
constexpr uint8_t PROTO_F6  = 0xA5;
constexpr uint8_t PROTO_F7  = 0xA6;
constexpr uint8_t PROTO_F8  = 0xA7;
constexpr uint8_t PROTO_F9  = 0xA8;
constexpr uint8_t PROTO_F10 = 0xA9;
constexpr uint8_t PROTO_F11 = 0xAA;
constexpr uint8_t PROTO_F12 = 0xAB;

// Other special keys
constexpr uint8_t PROTO_CAPS_LOCK = 0xB0;
constexpr uint8_t PROTO_FN_PRESS = 0xB1;
constexpr uint8_t PROTO_FN_RELEASE = 0xB2;

// Convert HID scan code to ASCII (unshifted)
// Returns 0 if not a printable key
uint8_t hidToAsciiUnshifted(uint8_t hid) {
  // Letters a-z (HID 0x04-0x1D)
  if (hid >= 0x04 && hid <= 0x1D) {
    return 'a' + (hid - 0x04);
  }
  // Numbers 1-9 (HID 0x1E-0x26)
  if (hid >= 0x1E && hid <= 0x26) {
    return '1' + (hid - 0x1E);
  }
  // Number 0 (HID 0x27)
  if (hid == 0x27) return '0';
  
  // Punctuation
  switch (hid) {
    case 0x2C: return ' ';   // Space
    case 0x2D: return '-';   // Minus
    case 0x2E: return '=';   // Equals
    case 0x2F: return '[';   // Left bracket
    case 0x30: return ']';   // Right bracket
    case 0x31: return '\\';  // Backslash
    case 0x33: return ';';   // Semicolon
    case 0x34: return '\'';  // Quote
    case 0x35: return '`';   // Grave accent
    case 0x36: return ',';   // Comma
    case 0x37: return '.';   // Period
    case 0x38: return '/';   // Slash
    default: return 0;
  }
}

// Convert HID scan code to ASCII (shifted)
uint8_t hidToAsciiShifted(uint8_t hid) {
  // Letters A-Z (HID 0x04-0x1D)
  if (hid >= 0x04 && hid <= 0x1D) {
    return 'A' + (hid - 0x04);
  }
  // Shifted numbers -> symbols
  switch (hid) {
    case 0x1E: return '!';  // Shift+1
    case 0x1F: return '@';  // Shift+2
    case 0x20: return '#';  // Shift+3
    case 0x21: return '$';  // Shift+4
    case 0x22: return '%';  // Shift+5
    case 0x23: return '^';  // Shift+6
    case 0x24: return '&';  // Shift+7
    case 0x25: return '*';  // Shift+8
    case 0x26: return '(';  // Shift+9
    case 0x27: return ')';  // Shift+0
    // Shifted punctuation
    case 0x2C: return ' ';  // Space (same)
    case 0x2D: return '_';  // Shift+minus
    case 0x2E: return '+';  // Shift+equals
    case 0x2F: return '{';  // Shift+[
    case 0x30: return '}';  // Shift+]
    case 0x31: return '|';  // Shift+backslash
    case 0x33: return ':';  // Shift+semicolon
    case 0x34: return '"';  // Shift+quote
    case 0x35: return '~';  // Shift+grave
    case 0x36: return '<';  // Shift+comma
    case 0x37: return '>';  // Shift+period
    case 0x38: return '?';  // Shift+slash
    default: return 0;
  }
}

// Check if HID code is a navigation/special key (not affected by shift for ASCII)
bool isNavigationKey(uint8_t hid, uint8_t &proto) {
  switch (hid) {
    case 0x28: proto = PROTO_ENTER; return true;       // Enter
    case 0x29: proto = PROTO_ESCAPE; return true;      // Escape
    case 0x2A: proto = PROTO_BACKSPACE; return true;   // Backspace
    case 0x2B: proto = PROTO_TAB; return true;         // Tab
    case 0x39: proto = PROTO_CAPS_LOCK; return true;   // Caps Lock
    case 0x4F: proto = PROTO_RIGHT_ARROW; return true;
    case 0x50: proto = PROTO_LEFT_ARROW; return true;
    case 0x51: proto = PROTO_DOWN_ARROW; return true;
    case 0x52: proto = PROTO_UP_ARROW; return true;
    case 0x49: proto = PROTO_INSERT; return true;
    case 0x4A: proto = PROTO_HOME; return true;
    case 0x4B: proto = PROTO_PAGE_UP; return true;
    case 0x4C: proto = PROTO_DELETE; return true;
    case 0x4D: proto = PROTO_END; return true;
    case 0x4E: proto = PROTO_PAGE_DOWN; return true;
    // Function keys
    case 0x3A: proto = PROTO_F1; return true;
    case 0x3B: proto = PROTO_F2; return true;
    case 0x3C: proto = PROTO_F3; return true;
    case 0x3D: proto = PROTO_F4; return true;
    case 0x3E: proto = PROTO_F5; return true;
    case 0x3F: proto = PROTO_F6; return true;
    case 0x40: proto = PROTO_F7; return true;
    case 0x41: proto = PROTO_F8; return true;
    case 0x42: proto = PROTO_F9; return true;
    case 0x43: proto = PROTO_F10; return true;
    case 0x44: proto = PROTO_F11; return true;
    case 0x45: proto = PROTO_F12; return true;
    // Alternate codes (some keyboards)
    case 0xC1: proto = PROTO_CAPS_LOCK; return true;
    case 0xC2: proto = PROTO_F1; return true;
    case 0xC3: proto = PROTO_F2; return true;
    case 0xC4: proto = PROTO_F3; return true;
    case 0xC5: proto = PROTO_F4; return true;
    case 0xC6: proto = PROTO_F5; return true;
    case 0xC7: proto = PROTO_F6; return true;
    case 0xC8: proto = PROTO_F7; return true;
    case 0xC9: proto = PROTO_F8; return true;
    case 0xCA: proto = PROTO_F9; return true;
    case 0xCB: proto = PROTO_F10; return true;
    case 0xCC: proto = PROTO_F11; return true;
    case 0xCD: proto = PROTO_F12; return true;
    case 0xD1: proto = PROTO_INSERT; return true;
    case 0xD2: proto = PROTO_HOME; return true;
    case 0xD3: proto = PROTO_PAGE_UP; return true;
    case 0xD4: proto = PROTO_DELETE; return true;
    case 0xD5: proto = PROTO_END; return true;
    case 0xD6: proto = PROTO_PAGE_DOWN; return true;
    case 0xD7: proto = PROTO_RIGHT_ARROW; return true;
    case 0xD8: proto = PROTO_LEFT_ARROW; return true;
    case 0xD9: proto = PROTO_DOWN_ARROW; return true;
    case 0xDA: proto = PROTO_UP_ARROW; return true;
    default: return false;
  }
}

// -------------------------
// Ring buffer
// -------------------------
constexpr int BUF_SIZE = 1024;
volatile uint8_t buf[BUF_SIZE];
volatile uint16_t head = 0;
volatile uint16_t tail = 0;

inline bool bufEmpty() { return head == tail; }
inline bool bufFull() { return ((head + 1) % BUF_SIZE) == tail; }

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

void bufClear() {
  noInterrupts();
  head = 0;
  tail = 0;
  interrupts();
}

void enqueueByte(uint8_t v) {
  bufPush(START_SYMBOL);
  for (int i = 7; i >= 0; i--) {
    bufPush((v >> i) & 1);
  }
}

// Helper to send modifier press bytes for currently held modifiers
void sendModifierPresses(uint8_t mods) {
  if (mods & 0x01) enqueueByte(PROTO_MOD_LCTRL_PRESS);
  if (mods & 0x02) enqueueByte(PROTO_MOD_LSHIFT_PRESS);
  if (mods & 0x04) enqueueByte(PROTO_MOD_LALT_PRESS);
  if (mods & 0x08) enqueueByte(PROTO_MOD_LGUI_PRESS);
  if (mods & 0x10) enqueueByte(PROTO_MOD_RCTRL_PRESS);
  if (mods & 0x20) enqueueByte(PROTO_MOD_RSHIFT_PRESS);
  if (mods & 0x40) enqueueByte(PROTO_MOD_RALT_PRESS);
  if (mods & 0x80) enqueueByte(PROTO_MOD_RGUI_PRESS);
}

// Helper to send modifier release bytes
void sendModifierReleases(uint8_t mods) {
  if (mods & 0x01) enqueueByte(PROTO_MOD_LCTRL_REL);
  if (mods & 0x02) enqueueByte(PROTO_MOD_LSHIFT_REL);
  if (mods & 0x04) enqueueByte(PROTO_MOD_LALT_REL);
  if (mods & 0x08) enqueueByte(PROTO_MOD_LGUI_REL);
  if (mods & 0x10) enqueueByte(PROTO_MOD_RCTRL_REL);
  if (mods & 0x20) enqueueByte(PROTO_MOD_RSHIFT_REL);
  if (mods & 0x40) enqueueByte(PROTO_MOD_RALT_REL);
  if (mods & 0x80) enqueueByte(PROTO_MOD_RGUI_REL);
}

// -------------------------
// Solenoid state machine
// -------------------------
enum PulseState {
  IDLE,
  START_PULSE_ON_FIRST,   // First solenoid of start symbol
  START_PULSE_OFF_FIRST,  // Gap before second solenoid
  START_PULSE_ON_SECOND,  // Second solenoid of start symbol
  START_PULSE_OFF_SECOND, // Gap after start symbol
  BIT_PULSE_ON,
  BIT_PULSE_OFF
};

volatile PulseState pulseState = IDLE;
volatile uint32_t tickCount = 0;
volatile int activePin = SOL0_PIN;

IntervalTimer solTimer;

// -------------------------
// Modifier tracking
// -------------------------
volatile uint8_t lastModifiers = 0;
volatile uint8_t standaloneModsPressed = 0;  // Track which standalone modifiers we've sent press for
volatile bool keyPressedWithMods = false;    // Flag: was a key pressed while modifiers held?

// Pending GUI release (for Windows key tap detection)
volatile uint8_t pendingGuiRelease = 0;
volatile uint32_t guiPressTime = 0;
constexpr uint32_t GUI_MIN_HOLD_MS = 50;  // Minimum time to hold GUI before release

// -------------------------
// Emergency exit tracking (3x ESC)
// -------------------------
volatile uint32_t escPressCount = 0;
volatile uint32_t lastEscTime = 0;
constexpr uint32_t ESC_WINDOW_MS = 500;  // 3 ESCs within 500ms

// -------------------------
// Keyboard callbacks
// -------------------------
volatile uint32_t lastKeyTime = 0;
constexpr uint32_t DEBOUNCE_MS = 10; // ignore presses <10ms apart

void onKeyPress(int key) {
    if (millis() - lastKeyTime < DEBOUNCE_MS) return;
    lastKeyTime = millis();
    uint8_t raw = (uint8_t)key;

  if (raw == 0x00) return;

  // Get current modifier state RIGHT NOW
  uint8_t mods = keyboard.getModifiers();
    bool shiftHeld = (mods & 0x22) != 0;
    bool ctrlHeld = (mods & 0x11) != 0;

  Serial.print("onKeyPress raw=0x"); Serial.print(raw, HEX);
  Serial.print(" mods=0x"); Serial.println(mods, HEX);

  // Mark that a key was pressed with modifiers (so pollModifiers won't double-send)
  if (mods != 0) {
    keyPressedWithMods = true;
    // Cancel any pending GUI release since a key was pressed with it
    pendingGuiRelease = 0;
  }

  // Handle ASCII control codes that should be navigation keys
    // These come through when the library auto-converts certain keys
    
    // Backspace comes as 0x08 (ASCII BS) or 0x7F (ASCII DEL)
    if (raw == 0x08 || raw == 0x7F) {
        Serial.println("  -> Backspace (from ASCII)");
        if (mods != 0) sendModifierPresses(mods);
        enqueueByte(PROTO_BACKSPACE);
        if (mods != 0) sendModifierReleases(mods);
        return;
    }
    
    // Enter comes as 0x0A (LF) or 0x0D (CR)
    if (raw == 0x0A || raw == 0x0D) {
        Serial.println("  -> Enter (from ASCII)");
        if (mods != 0) sendModifierPresses(mods);
        enqueueByte(PROTO_ENTER);
        if (mods != 0) sendModifierReleases(mods);
        return;
    }
    
    // Tab comes as 0x09
    if (raw == 0x09) {
        Serial.println("  -> Tab (from ASCII)");
        if (mods != 0) sendModifierPresses(mods);
        enqueueByte(PROTO_TAB);
        if (mods != 0) sendModifierReleases(mods);
        return;
    }
    
    // Escape comes as 0x1B
    if (raw == 0x1B) {
        Serial.println("  -> Escape (from ASCII)");
        
        // ESC emergency exit handling
        uint32_t now = millis();
        if (now - lastEscTime < ESC_WINDOW_MS) {
            escPressCount++;
        } else {
            escPressCount = 1;
        }
        lastEscTime = now;
        
        if (escPressCount >= 3) {
            Serial.println("!!! EMERGENCY CLEAR - 3x ESC !!!");
            bufClear();
            escPressCount = 0;
            enqueueByte(PROTO_CLEAR_BUFFER);
            return;
        }
        
        if (mods != 0) sendModifierPresses(mods);
        enqueueByte(PROTO_ESCAPE);
        if (mods != 0) sendModifierReleases(mods);
        return;
    }
    
  // Check if it's already printable ASCII (0x20-0x7E)
  // Some keyboard modes give ASCII directly
  if (raw >= 0x20 && raw <= 0x7E) {
    Serial.print("  -> Direct ASCII: '"); Serial.print((char)raw); Serial.println("'");
    uint8_t modsToSend = mods & 0xCC;  // Alt and GUI only
    if (modsToSend != 0) {
      sendModifierPresses(modsToSend);
      enqueueByte(raw);
      sendModifierReleases(modsToSend);
    } else {
      enqueueByte(raw);
    }
    return;
  }

  // Check for Ctrl+letter (0x01-0x1A)
  if (raw >= 0x01 && raw <= 0x1A) {
    Serial.print("  -> Ctrl+"); Serial.println((char)('a' + raw - 1));
    enqueueByte(raw);
    return;
  }

  // Check for Fn key (typically HID 0x3D-0x3E for Fn key)
  if (raw == 0x3D || raw == 0x3E) {  // Common Fn key HID codes
    // For key press, we'll always send PRESS first, then RELEASE in onKeyRelease
    enqueueByte(PROTO_FN_PRESS);
    return;
  }
  
  // Check if it's a navigation/special key
  uint8_t proto;
  if (isNavigationKey(raw, proto)) {
    // Special handling: if it's Backspace/Enter/Tab/Escape and NO shift held,
    // treat as navigation. If shift IS held, we still send as navigation but
    // with modifiers for combos like Shift+Enter
    
    // ESC emergency exit
    if (proto == PROTO_ESCAPE) {
    uint32_t now = millis();
    if (now - lastEscTime < ESC_WINDOW_MS) {
      escPressCount++;
    } else {
      escPressCount = 1;
    }
    lastEscTime = now;
    
    if (escPressCount >= 3) {
      Serial.println("!!! EMERGENCY CLEAR - 3x ESC !!!");
      bufClear();
      escPressCount = 0;
      enqueueByte(PROTO_CLEAR_BUFFER);
      return;
    }
  } else {
    escPressCount = 0;  // Reset if non-ESC key pressed
  }

    Serial.print("  -> Nav key proto=0x"); Serial.println(proto, HEX);
    if (mods != 0) sendModifierPresses(mods);
      enqueueByte(proto);
    if (mods != 0) sendModifierReleases(mods);
      return;
    }

  // It's an HID scan code for a printable key - convert to ASCII
  uint8_t ascii;
  if (shiftHeld && !ctrlHeld) {
    ascii = hidToAsciiShifted(raw);
  } else {
    ascii = hidToAsciiUnshifted(raw);
  }

  if (ascii != 0) {
    Serial.print("  -> HID->ASCII: '"); Serial.print((char)ascii); Serial.println("'");
    
    // For Alt+key or GUI+key combos
    uint8_t modsToSend = mods & 0xCC;  // Alt and GUI only
    if (modsToSend != 0) {
      sendModifierPresses(modsToSend);
      enqueueByte(ascii);
      sendModifierReleases(modsToSend);
    } else {
      enqueueByte(ascii);
    }
    return;
  }

  // Ctrl+letter gives HID code, convert to control character
  if (ctrlHeld && raw >= 0x04 && raw <= 0x1D) {
    uint8_t ctrlChar = 1 + (raw - 0x04);  // Ctrl+A = 0x01, Ctrl+B = 0x02, etc.
    Serial.print("  -> Ctrl+"); Serial.print((char)('a' + raw - 0x04));
    Serial.print(" = 0x"); Serial.println(ctrlChar, HEX);
    enqueueByte(ctrlChar);
      return;
    }

  Serial.println("  -> IGNORED");
}

void onKeyRelease(int key) {
  uint8_t raw = (uint8_t)key;
  
  // Handle Fn key release
  if (raw == 0x3D || raw == 0x3E) {  // Common Fn key HID codes
    enqueueByte(PROTO_FN_RELEASE);
  }
}

// Poll modifier state changes
void pollModifiers() {
  uint8_t mods = keyboard.getModifiers();
  uint32_t now = millis();
  
  // Check for pending GUI release
  if (pendingGuiRelease != 0 && (now - guiPressTime) >= GUI_MIN_HOLD_MS) {
    sendModifierReleases(pendingGuiRelease);
    standaloneModsPressed &= ~pendingGuiRelease;
    pendingGuiRelease = 0;
  }
  
  if (mods == lastModifiers) return;
  
  uint8_t changed = mods ^ lastModifiers;
  uint8_t pressed = changed & mods;
  uint8_t released = changed & ~mods;
  
  if (keyPressedWithMods) {
    Serial.println("  -> Skipping (key was pressed with mods)");
    keyPressedWithMods = false;
    lastModifiers = mods;
    return;
  }
  
  // Standalone GUI and Alt
  uint8_t standaloneMask = 0xCC;
  uint8_t guiMask = 0x88;  // LGUI and RGUI
  
  // Send presses for standalone modifiers
  uint8_t standalonePressed = pressed & standaloneMask;
  if (standalonePressed) {
    sendModifierPresses(standalonePressed);
    standaloneModsPressed |= standalonePressed;
    
    // Track GUI press time
    if (standalonePressed & guiMask) {
      guiPressTime = now;
    }
  }
  
  // Send releases for standalone modifiers
  uint8_t standaloneReleased = released & standaloneMask;
  if (standaloneReleased) {
    // For GUI keys, delay the release to ensure minimum hold time
    uint8_t guiReleased = standaloneReleased & guiMask;
    uint8_t otherReleased = standaloneReleased & ~guiMask;
    
    if (otherReleased) {
      sendModifierReleases(otherReleased);
      standaloneModsPressed &= ~otherReleased;
    }
    
    if (guiReleased) {
      if ((now - guiPressTime) >= GUI_MIN_HOLD_MS) {
        sendModifierReleases(guiReleased);
        standaloneModsPressed &= ~guiReleased;
      } else {
        pendingGuiRelease = guiReleased;
      }
    }
  }

  lastModifiers = mods;
}

// -------------------------
// Solenoid ISR
// -------------------------
void solenoidISR() {
  uint8_t symbol = 0;
  
  switch (pulseState) {
    case IDLE:
      if (!bufEmpty()) {
        digitalWriteFast(DRV8833_ENABLE_PIN, HIGH);
        bufPop(symbol);
        
        if (symbol == START_SYMBOL) {
          // Start symbol - fire first solenoid
        tickCount = 0;
          pulseState = START_PULSE_ON_FIRST;
        } else {
          // It's a bit (0 or 1)
          activePin = symbol ? SOL1_PIN : SOL0_PIN;
          tickCount = 0;
          pulseState = BIT_PULSE_ON;
        }
      } else {
        digitalWriteFast(DRV8833_ENABLE_PIN, LOW);
      }
      break;

    case START_PULSE_ON_FIRST:
      if (tickCount == 0) digitalWriteFast(SOL0_PIN, HIGH);
      if (++tickCount >= PULSE_TICKS) {
        digitalWriteFast(SOL0_PIN, LOW);
        tickCount = 0;
        pulseState = START_PULSE_OFF_FIRST;
      }
      break;

    case START_PULSE_OFF_FIRST:
      // Short gap before second solenoid (just enough for power recovery)
      if (++tickCount >= GAP_TICKS) {
        tickCount = 0;
        pulseState = START_PULSE_ON_SECOND;
      }
      break;

    case START_PULSE_ON_SECOND:
      // Fire second solenoid (SOL1) for start symbol
      if (tickCount == 0) {digitalWriteFast(SOL1_PIN, HIGH);}
      if (++tickCount >= PULSE_TICKS) {
        digitalWriteFast(SOL1_PIN, LOW);
        tickCount = 0;
        pulseState = START_PULSE_OFF_SECOND;
      }
      break;

    case START_PULSE_OFF_SECOND:
      // Gap after start symbol before first data bit
      if (++tickCount >= GAP_TICKS) {
        tickCount = 0;
        pulseState = IDLE;
      }
      break;

    case BIT_PULSE_ON:
      // Fire single solenoid for bit
      if (tickCount == 0) {digitalWriteFast(activePin, HIGH);}
      if (++tickCount >= PULSE_TICKS) {
        digitalWriteFast(SOL0_PIN, LOW);
        digitalWriteFast(SOL1_PIN, LOW);
        tickCount = 0;
        pulseState = BIT_PULSE_OFF;
      }
      break;

    case BIT_PULSE_OFF:
      // Gap after bit before next item
      if (++tickCount >= GAP_TICKS) {
        tickCount = 0;
        pulseState = IDLE;
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

  Serial.println("USB host ready - 3x ESC for emergency clear");
}

// -------------------------
// Loop
// -------------------------
void loop() {
  usb.Task();
  pollModifiers();
  
  // // Continuously monitor modifier state
  // static uint8_t lastDebugMods = 0xFF;
  // uint8_t mods = keyboard.getModifiers();
  // if (mods != lastDebugMods) {
  //   Serial.print("DEBUG MODS: 0x"); Serial.println(mods, HEX);
  //   lastDebugMods = mods;
  // }
}