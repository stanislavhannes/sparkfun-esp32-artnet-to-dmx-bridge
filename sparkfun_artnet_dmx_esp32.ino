#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArtnetWifi.h>
#include <SparkFunDMX.h>
#include <WiFiManager.h>

// --- GLOBAL DEBUG TOGGLE ---
constexpr bool debugEnabled = false; // Set to true to enable serial debug output (compile-time, no runtime cost when false)

// --- CONFIGURATION ---
const int targetUniverse = 0; // Set your desired universe here (e.g., 0, 1, 2...)
const int STATUS_LED = 13;    // SparkFun onboard LED
const int LED_ON = HIGH;   
const int LED_OFF = LOW;   

const char* HOSTNAME = "artnet-dmx-node";         // DHCP hostname, shown in your router (e.g. Fritz!Box)
const char* AP_NAME = "Artnet-DMX-Node";          // Fallback access point for WiFi setup
const unsigned long WIFI_PORTAL_TIMEOUT_S = 180;  // Restart if no WiFi is configured within this time

ArtnetWifi artnet;
SparkFunDMX dmx;
HardwareSerial dmxSerial(2);
WiFiManager wifiManager;

// Hardware pins for RS485/DMX (fixed by the SparkFun ESP32 Thing Plus DMX to LED Shield)
const int DMX_TX_PIN = 17;
const int DMX_RX_PIN = 16;
const int DMX_EN_PIN = 21;
const int MAX_CHANNEL = 512;

// Shared Buffer & Synchronization
byte dmxData[MAX_CHANNEL + 1];   // Index 0 = DMX start code (always 0), channels 1..512. Protected by dmxSemaphore.
volatile bool dataReady = false;
uint32_t packetCount = 0;          // Only touched from artnetTask
unsigned long lastPacketTime = 0;  // Only touched from artnetTask
SemaphoreHandle_t dmxSemaphore;

// --- TASK CORE 0: ART-NET, WIFI, WEB PORTAL & DEBUG MONITOR ---
void artnetTask(void * parameter) {
  unsigned long lastDebugPrint = 0;
  
  for(;;) {
    // Drain all queued Art-Net packets: lwIP only buffers ~6 datagrams per socket and
    // multi-universe senders burst more than that per frame (bounded so we still yield)
    for (int i = 0; i < 16 && artnet.read() != 0; i++) {}
    wifiManager.process(); // Serve the web portal (returns immediately when no client is connected)
    
    // LED "Heartbeat" logic
    if (millis() - lastPacketTime > 50) {
      digitalWrite(STATUS_LED, LED_OFF);
    }

    // Process Debug Info only if enabled and 5 seconds have passed
    if (debugEnabled && (millis() - lastDebugPrint > 5000)) {
      float fps = packetCount / 5.0;
      
      Serial.println("\n--- [SYSTEM DEBUG MONITOR] ---");
      Serial.printf("Target Universe: %d | Hostname: %s | IP: %s\n", 
                    targetUniverse, HOSTNAME, WiFi.localIP().toString().c_str());
      Serial.printf("Core: %d | WiFi: %d dBm | Art-Net: %.1f FPS | Heap: %u bytes | Stack free: %u bytes\n", 
                    xPortGetCoreID(), WiFi.RSSI(), fps, ESP.getFreeHeap(), uxTaskGetStackHighWaterMark(NULL));
      
      // DMX Snapshot: Display channels 1 to 16
      Serial.print("DMX Snapshot (Ch 1-16): ");
      if (xSemaphoreTake(dmxSemaphore, pdMS_TO_TICKS(100))) {
        for (int i = 1; i <= 16; i++) {
          Serial.printf("%03d ", dmxData[i]);
        }
        xSemaphoreGive(dmxSemaphore);
      }
      Serial.println("\n------------------------------");

      packetCount = 0;
      lastDebugPrint = millis();
    }
    
    vTaskDelay(1); // Yield for system tasks
  }
}

// --- CALLBACK: CALLED WHEN ART-NET DATA IS RECEIVED (runs inside artnet.read() on Core 0) ---
void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data) {
  // Compare incoming universe with our targetUniverse
  if (universe != targetUniverse) return; 

  // Short timeout instead of 0: the DMX task only holds the mutex for a 512-byte memcpy,
  // so waiting a moment avoids silently dropping frames
  if (xSemaphoreTake(dmxSemaphore, pdMS_TO_TICKS(2))) {
    memcpy(dmxData + 1, data, min((int)length, MAX_CHANNEL));
    dataReady = true;
    xSemaphoreGive(dmxSemaphore);

    // Stats & LED activity (only touched from this task, no lock needed)
    packetCount++;
    lastPacketTime = millis();
    digitalWrite(STATUS_LED, LED_ON);
  }
}

// --- TASK CORE 1: DEDICATED DMX OUTPUT ---
void dmxOutputTask(void * parameter) {
  if (debugEnabled) Serial.printf("[CORE %d] DMX Task Started for Universe %d.\n", xPortGetCoreID(), targetUniverse);

  for(;;) {
    // Pick up new Art-Net data if available (copies channels 1..512 into the DMX driver buffer)
    if (dataReady && xSemaphoreTake(dmxSemaphore, portMAX_DELAY)) {
      dmx.writeBytes(dmxData + 1, MAX_CHANNEL, 1);
      dataReady = false;
      xSemaphoreGive(dmxSemaphore);
    }

    // Always refresh the DMX line, even without new Art-Net data:
    // fixtures expect a continuous signal and may blackout/hold otherwise
    dmx.update();
    dmxSerial.flush(); // Wait until the frame is fully sent before the next break changes the baud rate
    vTaskDelay(pdMS_TO_TICKS(5)); 
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  if (debugEnabled) Serial.println("\n--- ESP32 ART-NET NODE BOOTING ---");

  // Status LED boot sequence
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LED_ON);
  delay(500);
  digitalWrite(STATUS_LED, LED_OFF);
  delay(500);
  digitalWrite(STATUS_LED, LED_ON); 

  dmxSemaphore = xSemaphoreCreateMutex();

  // DMX Hardware Init
  dmxSerial.begin(DMX_BAUD, DMX_FORMAT, DMX_RX_PIN, DMX_TX_PIN);
  dmx.begin(dmxSerial, DMX_EN_PIN, MAX_CHANNEL);
  dmx.setComDir(DMX_WRITE_DIR);

  // WiFi Setup
  wifiManager.setHostname(HOSTNAME);                         // Must be set before connecting (WiFiManager handles the ESP32 ordering)
  wifiManager.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S); // Without a timeout autoConnect() never returns false
  if (debugEnabled) Serial.println("Starting WiFiManager Portal...");
  
  if (!wifiManager.autoConnect(AP_NAME)) {
    if (debugEnabled) Serial.println("WiFi Failed. Restarting.");
    ESP.restart();
  }

  // Keep the portal reachable at http://<ip> while connected (served non-blocking from artnetTask)
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.startWebPortal();
  
  if (debugEnabled) {
    Serial.printf("Connected! IP: %s | Hostname: %s | Web portal: http://%s\n", 
                  WiFi.localIP().toString().c_str(), HOSTNAME, WiFi.localIP().toString().c_str());
  }
  digitalWrite(STATUS_LED, LED_OFF);

  // Start Art-Net
  artnet.begin();
  artnet.setArtDmxCallback(onDmxFrame);

  // Task Pinning
  xTaskCreatePinnedToCore(artnetTask, "ArtnetTask", 8192, NULL, 1, NULL, 0); // 8k: serves the web portal too
  xTaskCreatePinnedToCore(dmxOutputTask, "DMXTask", 4096, NULL, 2, NULL, 1);
}

void loop() {
  vTaskDelete(NULL); 
}
