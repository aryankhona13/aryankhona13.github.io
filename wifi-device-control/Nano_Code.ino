#include <Adafruit_NeoPixel.h>

#define PIN 5
#define NUMPIXELS 8
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// --- Relay & Indicator Pins (R1 & R3 Swapped) ---
#define R1 4   
#define R2 3   
#define R3 2   
#define IND1 8 
#define IND2 7 
#define IND3 6 

unsigned long currentMillis;
enum MasterMode { NORMAL, CHASER, TIMER, CYCLIC, SEQUENCE, PARTY };
MasterMode currentMode = NORMAL;

int chaserType = 1; 
unsigned long chaserDelay = 400;
unsigned long lastChaserTime = 0;
int chaserStep = 0;

bool timerActive[ 3 ] = {false, false, false};
unsigned long timerEnd[ 3 ] = {0, 0, 0};

bool cycActive[ 3 ] = {false, false, false};
unsigned long cycOnTime[ 3 ] = {0, 0, 0};
unsigned long cycOffTime[ 3 ] = {0, 0, 0};
unsigned long lastCycTime[ 3 ] = {0, 0, 0};
bool cycState[ 3 ] = {false, false, false};

unsigned long seqTime[ 3 ] = {0, 0, 0};
unsigned long lastSeqTime = 0;
int seqStep = 0;
bool seqActive = false;

unsigned long lastPartyTime = 0;

// --- ADVANCED LED VARIABLES ---
int currentEffect = 0; 
int activeAutoEffect = 2; // MIT App starts effects from 2
unsigned long lastAutoCycleTime = 0;
unsigned long lastLedTime = 0;
uint32_t ledStep = 0; 
uint32_t extraData = 0; 

// Smart Number Extractor
long extractNumber(String str) {
  String numStr = "";
  for (int i = 0; i < str.length(); i++) {
    if (isDigit(str[ i ])) {
      numStr += str[ i ];
    }
  }
  return numStr.toInt();
}

void setup() {
  Serial.begin(9600);
  pinMode(R1, OUTPUT); pinMode(R2, OUTPUT); pinMode(R3, OUTPUT);
  pinMode(IND1, OUTPUT); pinMode(IND2, OUTPUT); pinMode(IND3, OUTPUT);
  setAllRelays(false); 

  pixels.begin();
  pixels.setBrightness(150); 
  pixels.clear();
  pixels.show();
  
  Serial.println("Perfect-Sync Nano Ready! Mapped 2 to 10.");
}

void loop() {
  currentMillis = millis(); 
  readSerialCommands();
  runRelayLogic();
  runLEDLogic();
}

// ==========================================
// 1. SERIAL COMMAND PARSER
// ==========================================
void readSerialCommands() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    Serial.print("Nano Received: "); Serial.println(cmd);

    if (cmd == "CMD:R1ON")  { stopAllModes(); currentMode = NORMAL; setRelay(1, true); }
    if (cmd == "CMD:R1OFF") { stopAllModes(); currentMode = NORMAL; setRelay(1, false); }
    if (cmd == "CMD:R2ON")  { stopAllModes(); currentMode = NORMAL; setRelay(2, true); }
    if (cmd == "CMD:R2OFF") { stopAllModes(); currentMode = NORMAL; setRelay(2, false); }
    if (cmd == "CMD:R3ON")  { stopAllModes(); currentMode = NORMAL; setRelay(3, true); }
    if (cmd == "CMD:R3OFF") { stopAllModes(); currentMode = NORMAL; setRelay(3, false); }
    if (cmd == "CMD:ALLON") { stopAllModes(); currentMode = NORMAL; setAllRelays(true); }
    if (cmd == "CMD:ALLOFF"){ stopAllModes(); currentMode = NORMAL; setAllRelays(false); }

    if (cmd.startsWith("CMD:CH")) {
      stopAllModes(); currentMode = CHASER;
      chaserType = extractNumber(cmd.substring(6, 7)); 
      int commaIdx = cmd.indexOf(",");
      chaserDelay = extractNumber(cmd.substring(commaIdx)); 
      if (chaserDelay < 100) chaserDelay = 500; 
      chaserStep = 0; lastChaserTime = currentMillis; setAllRelays(false);
    }

    if (cmd.startsWith("CMD:T")) {
      int relayNum = cmd.substring(5, 6).toInt();
      int vIndex = cmd.indexOf("VAL:");
      String timeVal = cmd.substring(vIndex + 4);
      if (timeVal == "OFF") {
        timerActive[ relayNum - 1 ] = false; setRelay(relayNum, false);
      } else {
        stopAllModesExcept(TIMER); currentMode = TIMER; 
        unsigned long duration = getTimerMillis(timeVal);
        if (duration > 0) {
          timerActive[ relayNum - 1 ] = true; timerEnd[ relayNum - 1 ] = currentMillis + duration; setRelay(relayNum, true); 
        }
      }
    }
    if (cmd == "CMD:TOFF") { for(int i=0; i<3; i++) timerActive[ i ] = false; setAllRelays(false); }

    if (cmd.startsWith("CMD:IN")) {
      stopAllModes(); currentMode = NORMAL; setAllRelays(false);
      if (cmd == "CMD:IN1") setRelay(1, true);
      if (cmd == "CMD:IN2") setRelay(2, true);
      if (cmd == "CMD:IN3") setRelay(3, true);
    }
    if (cmd == "CMD:INOFF") { stopAllModes(); setAllRelays(false); }

    if (cmd.startsWith("CMD:CYC")) {
      stopAllModesExcept(CYCLIC); currentMode = CYCLIC;
      int relayNum = extractNumber(cmd.substring(7, 8));
      int onIdx = cmd.indexOf("ON:"); int offIdx = cmd.indexOf(",OFF:");
      long onTime = extractNumber(cmd.substring(onIdx + 3, offIdx));
      long offTime = extractNumber(cmd.substring(offIdx + 5));
      if (onTime <= 0) onTime = 1; if (offTime <= 0) offTime = 1;
      cycOnTime[ relayNum - 1 ]  = onTime * 1000UL; cycOffTime[ relayNum - 1 ] = offTime * 1000UL;
      cycActive[ relayNum - 1 ] = true; cycState[ relayNum - 1 ] = true;
      lastCycTime[ relayNum - 1 ] = currentMillis; setRelay(relayNum, true);
    }
    if (cmd == "CMD:STOP1") { cycActive[ 0 ] = false; setRelay(1, false); }
    if (cmd == "CMD:STOP2") { cycActive[ 1 ] = false; setRelay(2, false); }
    if (cmd == "CMD:STOP3") { cycActive[ 2 ] = false; setRelay(3, false); }

    if (cmd.startsWith("CMD:SEQ")) {
      stopAllModes(); currentMode = SEQUENCE;
      int t1Idx = cmd.indexOf("T1:"); int t2Idx = cmd.indexOf(",T2:"); int t3Idx = cmd.indexOf(",T3:");
      long t1 = extractNumber(cmd.substring(t1Idx + 3, t2Idx));
      long t2 = extractNumber(cmd.substring(t2Idx + 4, t3Idx));
      long t3 = extractNumber(cmd.substring(t3Idx + 4));
      seqTime[ 0 ] = (t1 > 0 ? t1 : 1) * 1000UL; seqTime[ 1 ] = (t2 > 0 ? t2 : 1) * 1000UL; seqTime[ 2 ] = (t3 > 0 ? t3 : 1) * 1000UL;
      seqActive = true; seqStep = 0; lastSeqTime = currentMillis; setAllRelays(false); setRelay(1, true);
    }

    if (cmd == "CMD:PARTY") { stopAllModes(); currentMode = PARTY; currentEffect = 8; } 

    if (cmd == "CMD:LEDOFF"){ currentEffect = 0; setAllLEDs(0,0,0); }
    if (cmd == "CMD:CRED")  { currentEffect = 0; setAllLEDs(255,0,0); }
    if (cmd == "CMD:CGRN")  { currentEffect = 0; setAllLEDs(0,255,0); }
    if (cmd == "CMD:CBLU")  { currentEffect = 0; setAllLEDs(0,0,255); }
    if (cmd == "CMD:CWHT")  { currentEffect = 0; setAllLEDs(255,255,255); }
    if (cmd == "CMD:CYLW")  { currentEffect = 0; setAllLEDs(255,255,0); }
    if (cmd == "CMD:CORG")  { currentEffect = 0; setAllLEDs(255,128,0); }
    if (cmd == "CMD:CMAG")  { currentEffect = 0; setAllLEDs(255,0,255); }
    if (cmd == "CMD:CCYN")  { currentEffect = 0; setAllLEDs(0,255,255); }

    // DYNAMIC EFFECTS (Extracts exactly 2 through 10)
    if (cmd.startsWith("CMD:EFFECT,VAL:")) { 
      currentEffect = extractNumber(cmd.substring(15)); 
      ledStep = 0; pixels.clear(); 
    }
    
    // AUTO LOOP (Mapped to 99 so it never conflicts with App's 1-10)
    if (cmd == "CMD:ELOOP") { 
      currentEffect = 99; 
      lastAutoCycleTime = currentMillis; 
      ledStep = 0; pixels.clear(); 
    }
  }
}

// ==========================================
// 2. HARDWARE CONTROL LOGIC (RELAYS)
// ==========================================
void setRelay(int r, bool state) {
  if (r == 1) { digitalWrite(R1, state ? HIGH : LOW); digitalWrite(IND1, state ? HIGH : LOW); } 
  else if (r == 2) { digitalWrite(R2, state ? HIGH : LOW); digitalWrite(IND2, state ? HIGH : LOW); } 
  else if (r == 3) { digitalWrite(R3, state ? HIGH : LOW); digitalWrite(IND3, state ? HIGH : LOW); }
}

void setAllRelays(bool state) { setRelay(1, state); setRelay(2, state); setRelay(3, state); }

void stopAllModes() {
  for(int i=0; i<3; i++) { timerActive[ i ] = false; cycActive[ i ] = false; } seqActive = false;
}

void stopAllModesExcept(MasterMode exception) {
  if(exception != TIMER) for(int i=0; i<3; i++) timerActive[ i ] = false;
  if(exception != CYCLIC) for(int i=0; i<3; i++) cycActive[ i ] = false;
  if(exception != SEQUENCE) seqActive = false;
}

void runRelayLogic() {
  if (currentMode == TIMER) {
    for (int i = 0; i < 3; i++) {
      if (timerActive[ i ] && currentMillis >= timerEnd[ i ]) { setRelay(i + 1, false); timerActive[ i ] = false; }
    }
  }
  if (currentMode == CHASER) {
    if (currentMillis - lastChaserTime >= chaserDelay) {
      lastChaserTime = currentMillis; setAllRelays(false);
      if (chaserType == 1) { setRelay(chaserStep + 1, true); chaserStep = (chaserStep + 1) % 3; } 
      else if (chaserType == 2) { setRelay(3 - chaserStep, true); chaserStep = (chaserStep + 1) % 3; }
      else if (chaserType == 3) { int seq[ 4 ] = {1, 2, 3, 2}; setRelay(seq[ chaserStep ], true); chaserStep = (chaserStep + 1) % 4; }
      else if (chaserType == 4) { if (chaserStep == 0) { setRelay(1, true); setRelay(3, true); } else { setRelay(2, true); } chaserStep = (chaserStep + 1) % 2; }
    }
  }
  if (currentMode == CYCLIC) {
    for (int i = 0; i < 3; i++) {
      if (cycActive[ i ]) {
        unsigned long targetTime = cycState[ i ] ? cycOnTime[ i ] : cycOffTime[ i ];
        if (currentMillis - lastCycTime[ i ] >= targetTime) {
          cycState[ i ] = !cycState[ i ]; setRelay(i + 1, cycState[ i ]); lastCycTime[ i ] = currentMillis;
        }
      }
    }
  }
  if (currentMode == SEQUENCE && seqActive) {
    if (currentMillis - lastSeqTime >= seqTime[ seqStep ]) {
      lastSeqTime = currentMillis; seqStep = (seqStep + 1) % 3; setAllRelays(false); setRelay(seqStep + 1, true);
    }
  }
  if (currentMode == PARTY) {
    if (currentMillis - lastPartyTime >= 150) { 
      lastPartyTime = currentMillis; setRelay(1, random(2)); setRelay(2, random(2)); setRelay(3, random(2));
    }
  }
}

unsigned long getTimerMillis(String val) {
  val.toLowerCase(); val.replace(" ", ""); 
  if (val.indexOf("1m") != -1) return 60000UL;
  if (val.indexOf("5m") != -1) return 300000UL;
  if (val.indexOf("10m") != -1) return 600000UL;
  if (val.indexOf("30m") != -1) return 1800000UL;
  if (val.indexOf("1h") != -1) return 3600000UL;
  return 5000UL; 
}

// ==========================================
// 3. FULL-RAINBOW NEOPIXEL LOGIC
// ==========================================
void setAllLEDs(int r, int g, int b) {
  for(int i=0; i<NUMPIXELS; i++) pixels.setPixelColor(i, r, g, b); pixels.show();
}

void fadeAll(uint8_t fadeValue) {
  for(int i = 0; i < NUMPIXELS; i++) {
    uint32_t c = pixels.getPixelColor(i);
    uint8_t r = (c >> 16) & 0xff; uint8_t g = (c >>  8) & 0xff; uint8_t b =  c & 0xff;
    r = (r > fadeValue) ? r - fadeValue : 0;
    g = (g > fadeValue) ? g - fadeValue : 0;
    b = (b > fadeValue) ? b - fadeValue : 0;
    pixels.setPixelColor(i, r, g, b);
  }
}

int bounce(int step, int size) {
  int cycle = step % (size * 2 - 2);
  if (cycle >= size) cycle = (size * 2 - 2) - cycle;
  return cycle;
}

void runLEDLogic() {
  int effectToRun = currentEffect;

  // Auto Loop Logic (Now mapped to 99, completely isolated)
  if (currentEffect == 99) {
    if (currentMillis - lastAutoCycleTime >= 8000) { 
      lastAutoCycleTime = currentMillis; 
      activeAutoEffect++;
      if(activeAutoEffect > 10) activeAutoEffect = 2; // Loop cycles from 2 to 10
      pixels.clear(); ledStep = 0; 
    }
    effectToRun = activeAutoEffect;
  }

  // EXACT MAPPING FOR MIT APP COMMANDS (2 TO 10)
  switch(effectToRun) {
    case 2:  effect_RainbowVortex(); break;     // Button 1 in App
    case 3:  effect_RainbowBreathe(); break;    // Button 2 in App
    case 4:  effect_TrinityChase(); break;      // Button 3 in App
    case 5:  effect_ColorWave(); break;         // Button 4 in App
    case 6:  effect_RainbowComet(); break;      // Button 5 in App
    case 7:  effect_RainbowRadar(); break;      // Button 6 in App
    case 8:  effect_ColorHeartbeat(); break;    // Button 7 in App
    case 9:  effect_DualRainbowHelix(); break;  // Button 8 in App
    case 10: effect_NeonJuggle(); break;        // Button 9 in App (Now safe from loop bug!)
  }
}

// ==========================================
// THE 9 UNIQUE EFFECTS
// ==========================================

// Effect 1 (Mapped to 2)
void effect_RainbowVortex() {
  if (currentMillis - lastLedTime >= 30) {
    lastLedTime = currentMillis;
    for(int i=0; i<NUMPIXELS; i++) {
      pixels.setPixelColor(i, pixels.gamma32(pixels.ColorHSV((ledStep * 1500) + (i * 65536L / NUMPIXELS))));
    }
    pixels.show(); ledStep++;
  }
}

// Effect 2 (Mapped to 3)
void effect_RainbowBreathe() {
  if (currentMillis - lastLedTime >= 15) {
    lastLedTime = currentMillis;
    float mathStep = (ledStep % 314) / 100.0; 
    float brightness = (exp(sin(mathStep)) - 0.36787944) * 108.0; 
    for(int i=0; i<NUMPIXELS; i++) {
      uint32_t c = pixels.ColorHSV((ledStep * 100) + (i * 65536L / NUMPIXELS));
      uint8_t r = ((c >> 16) & 0xFF) * brightness / 255;
      uint8_t g = ((c >> 8) & 0xFF) * brightness / 255;
      uint8_t b = (c & 0xFF) * brightness / 255;
      pixels.setPixelColor(i, r, g, b);
    }
    pixels.show(); ledStep++;
  }
}

// Effect 3 (Mapped to 4)
void effect_TrinityChase() {
  if (currentMillis - lastLedTime >= 50) {
    lastLedTime = currentMillis;
    fadeAll(40); 
    int pos1 = (ledStep) % NUMPIXELS;
    int pos2 = (ledStep + NUMPIXELS/3) % NUMPIXELS; 
    int pos3 = (ledStep + (NUMPIXELS*2)/3) % NUMPIXELS; 
    pixels.setPixelColor(pos1, pixels.ColorHSV(ledStep * 2000)); 
    pixels.setPixelColor(pos2, pixels.ColorHSV(ledStep * 2000 + 21845)); 
    pixels.setPixelColor(pos3, pixels.ColorHSV(ledStep * 2000 + 43690)); 
    pixels.show(); ledStep++;
  }
}

// Effect 4 (Mapped to 5)
void effect_ColorWave() {
  if (currentMillis - lastLedTime >= 30) {
    lastLedTime = currentMillis;
    for(int i=0; i<NUMPIXELS; i++) {
      int wave = (sin((ledStep + i * 20) * 0.1) * 127) + 128; 
      uint32_t color = pixels.gamma32(pixels.ColorHSV((ledStep * 150) + (i * 65536L / NUMPIXELS)));
      uint8_t r = ((color >> 16) & 0xFF) * wave / 255;
      uint8_t g = ((color >> 8) & 0xFF) * wave / 255;
      uint8_t b = (color & 0xFF) * wave / 255;
      pixels.setPixelColor(i, r, g, b);
    }
    pixels.show(); ledStep++;
  }
}

// Effect 5 (Mapped to 6)
void effect_RainbowComet() {
  if (currentMillis - lastLedTime >= 60) {
    lastLedTime = currentMillis;
    fadeAll(70); 
    int head = ledStep % NUMPIXELS;
    pixels.setPixelColor(head, pixels.ColorHSV(ledStep * 3000)); 
    pixels.show(); ledStep++;
  }
}

// Effect 6 (Mapped to 7)
void effect_RainbowRadar() {
  if (currentMillis - lastLedTime >= 80) {
    lastLedTime = currentMillis;
    fadeAll(80); 
    int pos = bounce(ledStep, NUMPIXELS);
    pixels.setPixelColor(pos, pixels.ColorHSV(ledStep * 4000)); 
    pixels.show(); ledStep++;
  }
}

// Effect 7 (Mapped to 8)
void effect_ColorHeartbeat() {
  if (currentMillis - lastLedTime >= 50) {
    lastLedTime = currentMillis;
    int phase = ledStep % 20; 
    if (phase == 0) extraData = random(65536); 
    if (phase == 0 || phase == 1 || phase == 4 || phase == 5) {
      for(int i=0; i<NUMPIXELS; i++) {
        pixels.setPixelColor(i, pixels.ColorHSV(extraData + (i * 65536L / NUMPIXELS)));
      }
    } else {
      fadeAll(80); 
    }
    pixels.show(); ledStep++;
  }
}

// Effect 8 (Mapped to 9)
void effect_DualRainbowHelix() {
  if (currentMillis - lastLedTime >= 60) {
    lastLedTime = currentMillis;
    fadeAll(50); 
    int pos1 = ledStep % NUMPIXELS;
    int pos2 = (ledStep + (NUMPIXELS / 2)) % NUMPIXELS; 
    pixels.setPixelColor(pos1, pixels.ColorHSV(ledStep * 4000));
    pixels.setPixelColor(pos2, pixels.ColorHSV((ledStep * 4000) + 32768)); 
    pixels.show(); ledStep++;
  }
}

// Effect 9 (Mapped to 10)
void effect_NeonJuggle() {
  if (currentMillis - lastLedTime >= 40) {
    lastLedTime = currentMillis;
    fadeAll(50); 
    int pos1 = bounce(ledStep, NUMPIXELS);
    int pos2 = bounce(ledStep + 3, NUMPIXELS); 
    int pos3 = bounce(ledStep + 6, NUMPIXELS); 
    pixels.setPixelColor(pos1, pixels.ColorHSV(ledStep * 1500));
    pixels.setPixelColor(pos2, pixels.ColorHSV((ledStep * 1500) + 21845));
    pixels.setPixelColor(pos3, pixels.ColorHSV((ledStep * 1500) + 43690));
    pixels.show(); ledStep++;
  }
}