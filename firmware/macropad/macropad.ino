const long BAUD_RATE = 115200;
const int NUM_BUTTONS = 5;
const int buttonPins[NUM_BUTTONS] = {4, 5, 6, 7, 8};
const uint8_t buttonIDs[NUM_BUTTONS] = {0x01, 0x02, 0x03, 0x04, 0x05};

const int NUM_KNOBS = 5;
const int knobPins[NUM_KNOBS] = {A1, A2, A3, A4, A5};
const uint8_t knobIDs[NUM_KNOBS] = {0x11, 0x12, 0x13, 0x14, 0x15};

int lastButtonStates[NUM_BUTTONS];
int stableButtonStates[NUM_BUTTONS];
unsigned long lastDebounceTimes[NUM_BUTTONS];
const unsigned long DEBOUNCE_DELAY = 20;
int lastKnobValues[NUM_KNOBS];
const int KNOB_THRESHOLD = 2;

void setup() {
  Serial.begin(BAUD_RATE);
  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    lastButtonStates[i] = HIGH;
    stableButtonStates[i] = HIGH;
    lastDebounceTimes[i] = 0;
  }
  for (int i = 0; i < NUM_KNOBS; i++) {
    analogRead(knobPins[i]); delay(2);
    lastKnobValues[i] = map(analogRead(knobPins[i]), 0, 1023, 0, 100);
  }
}

void sendPacket(uint8_t componentId, uint8_t state) {
  uint8_t packet[4] = {0xAA, componentId, state, 0x55};
  Serial.write(packet, 4);
}

void loop() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    int reading = digitalRead(buttonPins[i]);
    if (reading != lastButtonStates[i]) lastDebounceTimes[i] = millis();
    if ((millis() - lastDebounceTimes[i]) > DEBOUNCE_DELAY) {
      if (reading != stableButtonStates[i]) {
        stableButtonStates[i] = reading;
        sendPacket(buttonIDs[i], (stableButtonStates[i] == LOW) ? 0x01 : 0x00);
      }
    }
    lastButtonStates[i] = reading;
  }

  for (int i = 0; i < NUM_KNOBS; i++) {
    analogRead(knobPins[i]); delay(2);
    int percentValue = map(analogRead(knobPins[i]), 0, 1023, 0, 100);
    if (abs(percentValue - lastKnobValues[i]) >= KNOB_THRESHOLD) {
      sendPacket(knobIDs[i], (uint8_t)percentValue);
      lastKnobValues[i] = percentValue;
    }
  }
}