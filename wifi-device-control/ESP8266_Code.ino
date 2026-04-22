#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// --- 1. AP Mode Settings (ESP's Default Hotspot) ---
const char *ap_ssid = "WiControl"; 
const char *ap_password = "ak123456";      

ESP8266WebServer server(80);

// Status LED Pin (NodeMCU D4 / GPIO2)
const int statusLed = 2; 
unsigned long previousMillis = 0;

void setup() {
  Serial.begin(9600); // Ye command Nano aur Serial Monitor dono ko jayegi
  pinMode(statusLed, OUTPUT);
  delay(1000);

  Serial.println("\n--- ESP8266 System Starting ---");

  // Dual Mode for stable connection
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress IP = WiFi.softAPIP();
  
  Serial.print("WiFi Hotspot Ready! Connect phone to: ");
  Serial.println(ap_ssid);
  Serial.print("App IP Address is: ");
  Serial.println(IP); 

  server.onNotFound(handleAppCommands);
  server.begin();
  Serial.println("Server Started. Waiting for App commands...\n");
}

void loop() {
  server.handleClient();

  // --- LED BLINKING LOGIC ---
  // Agar AP me 0 log connect hain AUR Home WiFi se bhi connected nahi hai
  if (WiFi.softAPgetStationNum() == 0 && WiFi.status() != WL_CONNECTED) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 500) { // Blink every 500ms
      previousMillis = currentMillis;
      digitalWrite(statusLed, !digitalRead(statusLed)); // Toggle state
    }
  } else {
    // Agar connection ho gaya toh LED permanent OFF (NodeMCU led is Active Low, so HIGH = OFF)
    digitalWrite(statusLed, HIGH); 
  }
}

// ==========================================
// THE COMMAND PARSER & NANO SENDER
// ==========================================
void handleAppCommands() {
  String uri = server.uri(); 

  // --- 0. DYNAMIC WIFI SWITCHING ---
  if (uri == "/SWITCH_STA") {
    String new_ssid = server.arg("ssid");
    String new_pass = server.arg("pass");
    
    Serial.println("\nSYSTEM: Switching to Station Mode...");
    WiFi.mode(WIFI_AP_STA); 
    WiFi.begin(new_ssid.c_str(), new_pass.c_str());
    
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(500); retries++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      server.send(200, "text/plain", "NEW_IP:" + WiFi.localIP().toString());
    } else {
      server.send(200, "text/plain", "FAIL");
    }
    return; 
  }
  else if (uri == "/SWITCH_AP") {
    WiFi.disconnect(); delay(500);
    WiFi.mode(WIFI_AP_STA); 
    WiFi.softAP(ap_ssid, ap_password);
    server.send(200, "text/plain", "NEW_IP:192.168.4.1");
    return; 
  }

  // Send OK back to App for standard commands
  server.send(200, "text/plain", "Command Received by ESP");

  if (uri == "/") {
    Serial.println("SYSTEM: App Connected Successfully!");
  }
  
  // 1. DASHBOARD COMMANDS (Relays)
  else if (uri == "/R1ON" || uri == "/CH1ON") { Serial.println("CMD:R1ON"); } 
  else if (uri == "/R1OFF" || uri == "/CH1OFF") { Serial.println("CMD:R1OFF"); }
  else if (uri == "/R2ON" || uri == "/CH2ON") { Serial.println("CMD:R2ON"); } 
  else if (uri == "/R2OFF" || uri == "/CH2OFF") { Serial.println("CMD:R2OFF"); }
  else if (uri == "/R3ON" || uri == "/CH3ON") { Serial.println("CMD:R3ON"); } 
  else if (uri == "/R3OFF" || uri == "/CH3OFF") { Serial.println("CMD:R3OFF"); }
  
  // 2. MASTER CONTROLS
  else if (uri == "/ALLON") { Serial.println("CMD:ALLON"); }
  else if (uri == "/ALLOFF") { Serial.println("CMD:ALLOFF"); } 
  else if (uri == "/LEDOFF") { Serial.println("CMD:LEDOFF"); } 

  // 3. INTERLOCK MODE
  else if (uri == "/IN1") { Serial.println("CMD:IN1"); }
  else if (uri == "/IN2") { Serial.println("CMD:IN2"); }
  else if (uri == "/IN3") { Serial.println("CMD:IN3"); }
  else if (uri == "/INOFF") { Serial.println("CMD:INOFF"); }

  // 4. TIMER MODE 
  else if (uri.startsWith("/T1=")) { Serial.print("CMD:T1,VAL:"); Serial.println(uri.substring(4)); }
  else if (uri.startsWith("/T2=")) { Serial.print("CMD:T2,VAL:"); Serial.println(uri.substring(4)); }
  else if (uri.startsWith("/T3=")) { Serial.print("CMD:T3,VAL:"); Serial.println(uri.substring(4)); }
  else if (uri == "/TOFF") { Serial.println("CMD:TOFF"); }

  // 5. CHASER MODE 
  else if (uri == "/CH1") { Serial.print("CMD:CH1,D:"); Serial.println(server.arg("d")); }
  else if (uri == "/CH2") { Serial.print("CMD:CH2,D:"); Serial.println(server.arg("d")); }
  else if (uri == "/CH3") { Serial.print("CMD:CH3,D:"); Serial.println(server.arg("d")); }
  else if (uri == "/CH4") { Serial.print("CMD:CH4,D:"); Serial.println(server.arg("d")); }

  // 6. CYCLIC MODE 
  else if (uri == "/CYC1") { Serial.print("CMD:CYC1,ON:"); Serial.print(server.arg("on")); Serial.print(",OFF:"); Serial.println(server.arg("off")); }
  else if (uri == "/STOP1") { Serial.println("CMD:STOP1"); }
  else if (uri == "/CYC2") { Serial.print("CMD:CYC2,ON:"); Serial.print(server.arg("on")); Serial.print(",OFF:"); Serial.println(server.arg("off")); }
  else if (uri == "/STOP2") { Serial.println("CMD:STOP2"); }
  else if (uri == "/CYC3") { Serial.print("CMD:CYC3,ON:"); Serial.print(server.arg("on")); Serial.print(",OFF:"); Serial.println(server.arg("off")); }
  else if (uri == "/STOP3") { Serial.println("CMD:STOP3"); }

  // 7. SEQUENTIAL MASTER MODE 
  else if (uri == "/SEQ") { 
    Serial.print("CMD:SEQ,T1:"); Serial.print(server.arg("t1")); 
    Serial.print(",T2:"); Serial.print(server.arg("t2")); 
    Serial.print(",T3:"); Serial.println(server.arg("t3")); 
  }

  // 8. VOICE / SPECIAL
  else if (uri == "/PARTY") { Serial.println("CMD:PARTY"); }
  
  // 9. RGB COLORS & EFFECTS
  else if (uri == "/CRED") { Serial.println("CMD:CRED"); }
  else if (uri == "/CGRN") { Serial.println("CMD:CGRN"); }
  else if (uri == "/CBLU") { Serial.println("CMD:CBLU"); }
  else if (uri == "/CWHT") { Serial.println("CMD:CWHT"); }
  else if (uri == "/CYLW") { Serial.println("CMD:CYLW"); }
  else if (uri == "/CORG") { Serial.println("CMD:CORG"); }
  else if (uri == "/CMAG") { Serial.println("CMD:CMAG"); }
  else if (uri == "/CCYN") { Serial.println("CMD:CCYN"); } // NEW CYAN COLOR ADDED!
  else if (uri == "/ELOOP") { Serial.println("CMD:ELOOP"); }
  
  // 10. DYNAMIC EFFECTS
  else if (uri.startsWith("/E")) {
    Serial.print("CMD:EFFECT,VAL:"); Serial.println(uri.substring(2));
  }
}