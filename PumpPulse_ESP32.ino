#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// PumpPulse - pulse-output sensor baseline.
// Verify the actual sensor and LCD electrical interface before wiring.

const int FLOW_PIN = 27;
const int PUMP_SWITCH_PIN = 26;
const int LCD_SDA = 21;
const int LCD_SCL = 22;

const uint8_t LCD_ADDRESS = 0x27;  // Verify with an I2C scanner.
LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);
WebServer server(80);

const char* AP_NAME = "PumpPulse";
const char* AP_PASSWORD = "PumpPulse26";

// PLACEHOLDER: calibrate using a measured volume and the actual sensor.
float pulsesPerLiter = 450.0f;
float lowFlowLimitLpm = 0.5f; // PLACEHOLDER: tune from measured normal flow.

const unsigned long SAMPLE_MS = 1000;
const unsigned long STARTUP_GRACE_MS = 5000;
const unsigned long LOW_FLOW_DELAY_MS = 8000;

volatile uint32_t pulseCountWindow = 0;
uint32_t totalPulses = 0;
float flowLpm = 0.0f;
float totalLitres = 0.0f;

unsigned long lastSampleMs = 0;
unsigned long pumpStartedMs = 0;
unsigned long lowFlowStartedMs = 0;

bool pumpOn = false;
bool previousPumpOn = false;
bool lowFlowTiming = false;
bool faultActive = false;

String currentStatus = "PUMP OFF";
String lastFault = "None";
uint32_t faultCount = 0;

void IRAM_ATTR onFlowPulse() {
  pulseCountWindow++;
}

const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PumpPulse</title>
<style>
body{font-family:Arial,sans-serif;max-width:620px;margin:24px auto;padding:0 14px}
.card{border:1px solid #bbb;border-radius:12px;padding:14px;margin:12px 0}
.big{font-size:28px;font-weight:bold}
</style></head><body>
<h1>PumpPulse</h1><p>Offline pump and water-delivery monitor</p>
<div class="card">Pump state: <b id="pump">--</b><br><br>
Status: <b id="status">--</b></div>
<div class="card">Flow rate<div class="big"><span id="flow">0</span> L/min</div>
Estimated delivered volume<div class="big"><span id="volume">0</span> L</div></div>
<div class="card">Latest fault: <b id="fault">None</b><br><br>
Fault events this session: <b id="count">0</b></div>
<p>Pump state is entered using the physical switch. Readings update every second.</p>
<script>
async function update(){
 try{
  const r=await fetch('/data'); const d=await r.json();
  document.getElementById('pump').textContent=d.pump;
  document.getElementById('status').textContent=d.status;
  document.getElementById('flow').textContent=d.flow.toFixed(2);
  document.getElementById('volume').textContent=d.volume.toFixed(2);
  document.getElementById('fault').textContent=d.fault;
  document.getElementById('count').textContent=d.faultCount;
 }catch(e){}
}
update(); setInterval(update,1000);
</script></body></html>
)HTML";

void sendData() {
  String json = "{";
  json += "\"pump\":\"";
  json += pumpOn ? "ON" : "OFF";
  json += "\",\"status\":\"" + currentStatus + "\"";
  json += ",\"flow\":" + String(flowLpm, 2);
  json += ",\"volume\":" + String(totalLitres, 2);
  json += ",\"fault\":\"" + lastFault + "\"";
  json += ",\"faultCount\":" + String(faultCount);
  json += "}";
  server.send(200, "application/json", json);
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", PAGE);
  });
  server.on("/data", HTTP_GET, sendData);
  server.on("/reset", HTTP_POST, []() {
    noInterrupts();
    totalPulses = 0;
    interrupts();
    totalLitres = 0.0f;
    faultCount = 0;
    lastFault = "None";
    server.send(200, "text/plain", "Session totals reset");
  });
  server.begin();
}

void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.print("Flow:");
  lcd.print(flowLpm, 1);
  lcd.print(" L/m     ");

  lcd.setCursor(0, 1);
  String line = currentStatus;
  if (line.length() > 16) line = line.substring(0, 16);
  lcd.print(line);
  for (int i = line.length(); i < 16; i++) lcd.print(' ');
}

void setup() {
  Serial.begin(115200);

  pinMode(FLOW_PIN, INPUT_PULLUP);
  pinMode(PUMP_SWITCH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), onFlowPulse, FALLING);

  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("PumpPulse Boot");
  lcd.setCursor(0, 1);
  lcd.print("Check sensors");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_NAME, AP_PASSWORD);
  setupWebServer();

  Serial.println();
  Serial.println("PumpPulse started");
  Serial.print("Dashboard: http://");
  Serial.println(WiFi.softAPIP());

  lastSampleMs = millis();
}

void loop() {
  server.handleClient();
  const unsigned long now = millis();

  // Switch closed to GND means pump is manually marked ON.
  pumpOn = (digitalRead(PUMP_SWITCH_PIN) == LOW);

  if (pumpOn && !previousPumpOn) {
    pumpStartedMs = now;
    lowFlowTiming = false;
    faultActive = false;
  }

  if (!pumpOn) {
    lowFlowTiming = false;
    faultActive = false;
    flowLpm = 0.0f;
    currentStatus = "PUMP OFF";
  }

  previousPumpOn = pumpOn;

  if (now - lastSampleMs >= SAMPLE_MS) {
    const unsigned long elapsedMs = now - lastSampleMs;
    lastSampleMs = now;

    noInterrupts();
    const uint32_t pulses = pulseCountWindow;
    pulseCountWindow = 0;
    interrupts();

    totalPulses += pulses;

    if (pulsesPerLiter > 0.0f && elapsedMs > 0) {
      flowLpm = (pulses * 60000.0f) / (pulsesPerLiter * elapsedMs);
      totalLitres = totalPulses / pulsesPerLiter;
    }

    if (pumpOn) {
      if (now - pumpStartedMs < STARTUP_GRACE_MS) {
        currentStatus = "STARTING";
        lowFlowTiming = false;
      } else if (flowLpm < lowFlowLimitLpm) {
        if (!lowFlowTiming) {
          lowFlowStartedMs = now;
          lowFlowTiming = true;
        }
        currentStatus = "LOW FLOW";
        if (!faultActive && now - lowFlowStartedMs >= LOW_FLOW_DELAY_MS) {
          faultActive = true;
          currentStatus = "NO/LOW FLOW";
          lastFault = "Low/no flow";
          faultCount++;
          Serial.println("WARNING: sustained low/no flow");
        } else if (faultActive) {
          currentStatus = "NO/LOW FLOW";
        }
      } else {
        currentStatus = "NORMAL";
        lowFlowTiming = false;
        faultActive = false;
      }
    }

    updateLCD();

    Serial.print("Pump=");
    Serial.print(pumpOn ? "ON" : "OFF");
    Serial.print(", flow=");
    Serial.print(flowLpm, 2);
    Serial.print(" L/min, volume=");
    Serial.print(totalLitres, 2);
    Serial.print(" L, status=");
    Serial.println(currentStatus);
  }
}
