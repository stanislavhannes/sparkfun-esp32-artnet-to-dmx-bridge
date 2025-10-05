#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArtnetWifi.h>
#include <SparkFunDMX.h>
#include <WiFiManager.h>

// Artnet settings
ArtnetWifi artnet;
const int startUniverse = 0;
const int endUniverse = 0;

// Create DMX object
SparkFunDMX dmx;

// Create serial port for DMX
HardwareSerial dmxSerial(2);

// Hardware pins
const int DMX_TX_PIN = 17;
const int DMX_RX_PIN = 16;
const int DMX_EN_PIN = 4;

// WiFi UDP
WiFiUDP UdpSend;

// DMX settings
#define MAX_CHANNEL 512

// Shared buffer with atomic access
volatile byte dmxData[MAX_CHANNEL + 1];
volatile bool dataReady = false;
SemaphoreHandle_t dmxSemaphore;

// Task handle
TaskHandle_t dmxTaskHandle;

// DMX output task running on Core 1
void dmxOutputTask(void * parameter) {
  while(true) {
    if (dataReady) {
      if (xSemaphoreTake(dmxSemaphore, portMAX_DELAY)) {
        // Copy data and clear flag
        byte localBuffer[MAX_CHANNEL + 1];
        memcpy(localBuffer, (void*)dmxData, MAX_CHANNEL + 1);
        dataReady = false;
        xSemaphoreGive(dmxSemaphore);
        
        // Write to DMX
        dmx.writeBytes(localBuffer, MAX_CHANNEL + 1, 0);
        dmx.update();
      }
    }
    vTaskDelay(1); // Small delay to prevent watchdog
  }
}

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data)
{
  if (universe != startUniverse) return;
  
  if (xSemaphoreTake(dmxSemaphore, 0)) { // Don't wait if busy
    // Copy new data
    memcpy((void*)(dmxData + 1), data, length);
    dataReady = true;
    xSemaphoreGive(dmxSemaphore);
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nSparkFun Artnet to DMX - Dual Core");
  
  // Create semaphore
  dmxSemaphore = xSemaphoreCreateMutex();
  
  // Initialize DMX
  Serial.print("Initializing DMX...");
  dmxSerial.begin(DMX_BAUD, DMX_FORMAT, DMX_RX_PIN, DMX_TX_PIN);
  dmx.begin(dmxSerial, DMX_EN_PIN, MAX_CHANNEL);
  dmx.setComDir(DMX_WRITE_DIR);
  
  // Clear buffer
  memset((void*)dmxData, 0, sizeof(dmxData));
  dmx.writeBytes((byte*)dmxData, MAX_CHANNEL + 1, 0);
  dmx.update();
  Serial.println(" Done!");
  
  // Start DMX task on Core 1
  xTaskCreatePinnedToCore(
    dmxOutputTask,    // Function
    "DMX_Output",     // Name
    2048,            // Stack size
    NULL,            // Parameters
    1,               // Priority
    &dmxTaskHandle,  // Handle
    1                // Core 1
  );
  
  // WiFiManager
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);
  
  Serial.println("Connecting to WiFi...");
  
  if (!wifiManager.autoConnect("Sparkfun-Artnet-DMX")) {
    Serial.println("Failed to connect, restarting...");
    delay(3000);
    ESP.restart();
  }
  
  Serial.println("Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  // Start Artnet
  artnet.begin();
  artnet.setArtDmxCallback(onDmxFrame);
  
  Serial.println("Ready for Artnet data!");
  Serial.println("Core 0: Artnet, Core 1: DMX");
}

void loop()
{
  artnet.read();
}