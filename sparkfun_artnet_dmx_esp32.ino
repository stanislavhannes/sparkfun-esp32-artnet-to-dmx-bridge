#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArtnetWifi.h>
#include <SparkFunDMX.h>
#include <WiFiManager.h>

// --- GLOBAL DEBUG TOGGLE ---
bool debugEnabled = false; // Set to false to disable serial debug output

// --- CONFIGURATION ---
const int targetUniverse = 0; // Set your desired universe here (e.g., 0, 1, 2...)
const int STATUS_LED = 13;    // SparkFun onboard LED
const int LED_ON = HIGH;   
const int LED_OFF = LOW;   

ArtnetWifi artnet;
SparkFunDMX dmx;
HardwareSerial dmxSerial(2);

// Hardware pins for RS485/DMX
const int DMX_TX_PIN = 17;
const int DMX_RX_PIN = 16;
const int DMX_EN_PIN = 4;
#define MAX_CHANNEL 512

// Shared Buffer & Synchronization
volatile byte dmxData[MAX_CHANNEL + 1];
volatile bool dataReady = false;
volatile uint32_t packetCount = 0;
volatile unsigned long lastPacketTime = 0;
SemaphoreHandle_t dmxSemaphore;

// --- TASK CORE 0: ART-NET, WIFI & DEBUG MONITOR ---
void artnetTask(void * parameter) {
  unsigned long lastDebugPrint = 0;
  
  for(;;) {
    artnet.read(); // Process incoming Art-Net packets
    
    // LED "Heartbeat" logic
    if (millis() - lastPacketTime > 50) {
      digitalWrite(STATUS_LED, LED_OFF);
    }

    // Process Debug Info only if enabled and 5 seconds have passed
    if (debugEnabled && (millis() - lastDebugPrint > 5000)) {
      float fps = packetCount / 5.0;
      
      Serial.println("\n--- [SYSTEM DEBUG MONITOR] ---");
      Serial.printf("Target Universe: %d\n", targetUniverse);
      Serial.printf("Core: %d | WiFi: %d dBm | Art-Net: %.1f FPS | Heap: %u bytes\n", 
                    xPortGetCoreID(), WiFi.RSSI(), fps, ESP.getFreeHeap());
      
      // DMX Snapshot: Display channels 1 to 16
      Serial.print("DMX Snapshot (Ch 1-16): ");
      if (xSemaphoreTake(dmxSemaphore, 100)) {
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

// --- CALLBACK: CALLED WHEN ART-NET DATA IS RECEIVED ---
void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data) {
  // Compare incoming universe with our targetUniverse
  if (universe != targetUniverse) return; 

  if (xSemaphoreTake(dmxSemaphore, 0)) {
    // Copy data to shared buffer
    memcpy((void*)(dmxData + 1), data, min((int)length, MAX_CHANNEL));
    dataReady = true;
    packetCount++;
    lastPacketTime = millis();
    xSemaphoreGive(dmxSemaphore);
    
    // Trigger LED activity
    digitalWrite(STATUS_LED, LED_ON);
  }
}

// --- TASK CORE 1: DEDICATED DMX OUTPUT ---
void dmxOutputTask(void * parameter) {
  byte localBuffer[MAX_CHANNEL + 1];
  
  if (debugEnabled) Serial.printf("[CORE %d] DMX Task Started for Universe %d.\n", xPortGetCoreID(), targetUniverse);

  for(;;) {
    if (dataReady) {
      if (xSemaphoreTake(dmxSemaphore, portMAX_DELAY)) {
        memcpy(localBuffer, (void*)dmxData, MAX_CHANNEL + 1);
        dataReady = false;
        xSemaphoreGive(dmxSemaphore);
        
        // Update DMX hardware
        dmx.writeBytes(localBuffer, MAX_CHANNEL + 1, 0);
        dmx.update();
      }
    }
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
  dmxSerial.begin(250000, SERIAL_8N2, DMX_RX_PIN, DMX_TX_PIN);
  dmx.begin(dmxSerial, DMX_EN_PIN, MAX_CHANNEL);
  dmx.setComDir(DMX_WRITE_DIR);

  // WiFi Setup
  WiFiManager wifiManager;
  if (debugEnabled) Serial.println("Starting WiFiManager Portal...");
  
  if (!wifiManager.autoConnect("Artnet-DMX-Node")) {
    if (debugEnabled) Serial.println("WiFi Failed. Restarting.");
    ESP.restart();
  }
  
  if (debugEnabled) {
    Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  }
  digitalWrite(STATUS_LED, LED_OFF);

  // Start Art-Net
  artnet.begin();
  artnet.setArtDmxCallback(onDmxFrame);

  // Task Pinning
  xTaskCreatePinnedToCore(artnetTask, "ArtnetTask", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(dmxOutputTask, "DMXTask", 4096, NULL, 2, NULL, 1);
}

void loop() {
  vTaskDelete(NULL); 
}