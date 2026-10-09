// --- Pin Definitions ---
const int LED_PIN = 1;    // Confirm this is your LED's GPIO pin. (GPIO0 can be tricky on ESP32-C3)
const int BUTTON_PIN = 2; // Confirm this is your button's GPIO pin. (GPIO1 can be tricky on ESP32-C3)

// --- State Variables ---
bool isPairingMode = false;
bool isPaired = false; // Placeholder: Assume device is NOT paired initially.
                       // You will integrate your connection logic here later to change this.
bool currentLedState = LOW; // Tracks the current state of the LED (for blinking)

// --- Button Debouncing and Hold Detection Variables ---
bool lastButtonState = HIGH; // Stores the previous stable button state (HIGH for INPUT_PULLUP = not pressed)
unsigned long lastDebounceTime = 0; // The last time the button input pin was toggled
const unsigned long DEBOUNCE_DELAY = 1000; // Debounce time; increase if you get false presses

unsigned long buttonPressStartTime = 0; // Records when the button was first pressed down
const unsigned long BUTTON_HOLD_DURATION = 2000; // Time (ms) button must be held to trigger action (e.g., 2 seconds)
bool buttonWasHeld = false; // Flag to ensure hold action only triggers once per hold

// --- Pairing Mode Timeout Variables ---
unsigned long pairingModeStartTime = 0; // Records when pairing mode was entered
const unsigned long PAIRING_MODE_TIMEOUT = 15000; // 15 seconds in milliseconds

// --- LED Blinking Variables ---
unsigned long lastLedToggleTime = 0; // The last time the LED was toggled for blinking
const unsigned long BLINK_INTERVAL = 250; // How fast the LED blinks (e.g., 250ms on, 250ms off)

// --- Serial Debugging Variables (to prevent flooding) ---
bool lastIsPairingModePrint = false;
bool lastIsPairedPrint = false; // For the 'isPaired' state print

void setup() {
  Serial.begin(115200);

  // Configure pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Use internal pull-up

  // Initial state of LED
  digitalWrite(LED_PIN, LOW); // Start with LED off
  currentLedState = LOW;

  Serial.println("Setup complete.");
  Serial.println("Initial State: Not Paired, Not in Pairing Mode. LED is OFF.");

}

void loop() {
  unsigned long currentMillis = millis(); // Get the current time for non-blocking operations

  // --- Read Button State with Debouncing ---
  bool reading = digitalRead(BUTTON_PIN);

  // If the button state has changed (from what was last stable)
  if (reading != lastButtonState) {
    lastDebounceTime = currentMillis; // Reset the debounce timer
  }

  // Only proceed if the button state has been stable for DEBOUNCE_DELAY
  if ((currentMillis - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != lastButtonState) { // Check if the stable state is actually different
      lastButtonState = reading; // Update the stable button state

      if (reading == LOW) { // Button is now pressed (active low for INPUT_PULLUP)
        buttonPressStartTime = currentMillis; // Record the start time of the press
        buttonWasHeld = false; // Reset the hold flag for this new press
        Serial.println("Button Pressed.");
      } else { // Button is now released
        // Check if the button was held long enough
        if (buttonWasHeld) { // Action taken on release after a hold
          Serial.println("Button Released after HOLD.");
        } else {
          Serial.println("Button Released (Quick Press).");
        }
      }
    }
  }

  // --- Detect Button Hold ---
  // If button is currently pressed and has been held longer than BUTTON_HOLD_DURATION
  if (lastButtonState == LOW && (currentMillis - buttonPressStartTime) >= BUTTON_HOLD_DURATION && !buttonWasHeld) {
    buttonWasHeld = true; // Mark as held to prevent repeated triggering
    Serial.println("Button held down! Toggling Pairing Mode.");

    // Toggle Pairing Mode
    isPairingMode = !isPairingMode;

    if (isPairingMode) {
      Serial.println("--> Entering Pairing Mode.");
      pairingModeStartTime = currentMillis; // Start the 15-second timeout
      currentLedState = HIGH; // Ensure LED is ON to start blinking cycle
      digitalWrite(LED_PIN, currentLedState);
      lastLedToggleTime = currentMillis; // Reset blink timer
    } else {
      Serial.println("--> Exiting Pairing Mode.");
      // When exiting, the LED state will be handled by the main LED logic below
    }
  }

  // --- Pairing Mode Timeout Logic ---
  if (isPairingMode && (currentMillis - pairingModeStartTime) >= PAIRING_MODE_TIMEOUT) {
    isPairingMode = false; // Pairing mode timed out
    Serial.println("--> Pairing Mode Timed Out. Exiting Pairing Mode.");
  }

  // --- LED Control Logic ---
  if (isPairingMode) {
    // Blinking LED while in pairing mode
    if ((currentMillis - lastLedToggleTime) >= BLINK_INTERVAL) {
      currentLedState = !currentLedState; // Toggle the LED state
      digitalWrite(LED_PIN, currentLedState);
      lastLedToggleTime = currentMillis; // Update the last toggle time
    }
  } else {
    // Not in pairing mode, LED state depends on 'isPaired'
    if (isPaired) {
      // Device paired, LED ON
      if (currentLedState == LOW) { // Only change if it's currently off
        digitalWrite(LED_PIN, HIGH);
        currentLedState = HIGH;
      }
    } else {
      // Device not paired, LED OFF
      if (currentLedState == HIGH) { // Only change if it's currently on
        digitalWrite(LED_PIN, LOW);
        currentLedState = LOW;
      }
    }
  }

  // --- Serial Output for State Changes (to avoid flooding) ---
  if (isPairingMode != lastIsPairingModePrint) {
      Serial.print("Current State: ");
      if (isPairingMode) {
          Serial.println("IN PAIRING MODE (Blinking LED)");
      } else {
          Serial.print("NOT IN PAIRING MODE. ");
          if (isPaired) {
              Serial.println("Device Paired (Solid LED ON)");
          } else {
              Serial.println("Device Unpaired (LED OFF)");
          }
      }
      lastIsPairingModePrint = isPairingMode;
  }
  // This part only prints if isPaired changes *while not in pairing mode*
  // If we wanted to print paired state continuously, a different logic is needed
  // For now, the print above should cover it.
}
