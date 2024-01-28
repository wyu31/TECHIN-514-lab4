#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Updated UUIDs for service and characteristic
static BLEUUID serviceUUID("1f212b3e-1e7b-44bd-97f6-5464f9450440"); // Server's Service UUID
static BLEUUID charUUID("00766a8b-e54b-4dd9-aec9-768c6060bacc");     // Server's Characteristic UUID

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

float currentDistance = 0.0;
float maxDistance = 0.0;
float minDistance = 0.0;

void aggregateData(float distance) {
    maxDistance = max(maxDistance, distance);
    if (minDistance == 0.0 || distance < minDistance) {
        minDistance = distance;
    }
    currentDistance = distance;

    Serial.print("Current Distance: ");
    Serial.print(currentDistance, 2);
    Serial.print(" cm, Max Distance: ");
    Serial.print(maxDistance, 2);
    Serial.print(", Min Distance: ");
    Serial.println(minDistance, 2);
}

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    String dataString = reinterpret_cast<char*>(pData);
    int startPos = dataString.indexOf(": ") + 2;
    int endPos = dataString.indexOf(" cm");
    String distanceStr = dataString.substring(startPos, endPos);
    float distance = distanceStr.toFloat();
    aggregateData(distance);
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) override {
    connected = true;
    Serial.println("Connected to server");
  }

  void onDisconnect(BLEClient* pclient) override {
    connected = false;
    Serial.println("Disconnected from server");
  }
};

bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());

    BLEClient* pClient = BLEDevice::createClient();
    Serial.println(" - Created client");
    pClient->setClientCallbacks(new MyClientCallback());

    if (pClient->connect(myDevice)) {
        BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
        if (pRemoteService == nullptr) {
            Serial.print("Failed to find our service UUID: ");
            Serial.println(serviceUUID.toString().c_str());
            pClient->disconnect();
            return false;
        }
        Serial.println(" - Found our service");

        pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
        if (pRemoteCharacteristic == nullptr) {
            Serial.print("Failed to find our characteristic UUID: ");
            Serial.println(charUUID.toString().c_str());
            pClient->disconnect();
            return false;
        }
        Serial.println(" - Found our characteristic");

        if (pRemoteCharacteristic->canNotify()) {
            pRemoteCharacteristic->registerForNotify(notifyCallback);
        }
        connected = true;
        return true;
    } else {
        Serial.println("Failed to connect to the server");
    }
    pClient->disconnect();
    return false;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = false;
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("ChaneyheESP32");

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}

void loop() {
  if (doConnect) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothing more we will do.");
    }
    doConnect = false;
  }

  if (connected) {
    // Add code here if you need to interact with the server
  } else if (doScan) {
    BLEDevice::getScan()->start(0);  // Restart scanning
  }

  delay(1000); // Delay a second between loops.
}