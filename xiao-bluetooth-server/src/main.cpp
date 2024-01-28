#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <stdlib.h>

const int trigPin = D7; // HC-SR04 Trig pin connected to D7
const int echoPin = D8; // Echo pin connected to D8

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long previousMillis = 0;
const long interval = 1000;

// Server and Client UUIDs
#define SERVER_UUID        "1f212b3e-1e7b-44bd-97f6-5464f9450440" 
#define CHARACTERISTIC_UUID "00766a8b-e54b-4dd9-aec9-768c6060bacc"

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

// Moving average filter setup
const int numReadings = 10;
int readings[numReadings]; // the readings from the sensor
int readIndex = 0; // the index of the current reading
int total = 0; // the running total
int average = 0; // the average

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    BLEDevice::init("WanlingESP32"); // Set the BLE device's name for the server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVER_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->addDescriptor(new BLE2902());
    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVER_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Characteristic defined! Now you can read it in your phone!");

    pinMode(trigPin, OUTPUT); // Sets the trigPin as an OUTPUT
    pinMode(echoPin, INPUT);  // Sets the echoPin as an INPUT

    // Initialize all the readings to 0:
    for (int thisReading = 0; thisReading < numReadings; thisReading++) {
        readings[thisReading] = 0;
    }
}

long measureDistance() {
    // Clears the trigPin condition
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    // Sets the trigPin HIGH (ACTIVE) for 10 microseconds
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);  // Corrected line
    // Reads the echoPin, returns the sound wave travel time in microseconds
    long duration = pulseIn(echoPin, HIGH);
    // Calculating the distance
    long distance = duration * 0.034 / 2; // Speed of sound wave divided by 2 (go and back)
    return distance;
}

void loop() {
    long distance = measureDistance(); // Raw distance measured by the sensor

    // Moving average filter
    total = total - readings[readIndex];
    readings[readIndex] = distance;
    total = total + readings[readIndex];
    readIndex = readIndex + 1;

    if (readIndex >= numReadings) {
        readIndex = 0;
    }

    average = total / numReadings;

    Serial.print("Raw Distance: ");
    Serial.print(distance);
    Serial.print(" cm, Average Distance: ");
    Serial.print(average);
    Serial.println(" cm");

    if (deviceConnected) {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
            if (average < 30) {  // Check if the denoised distance is less than 30 cm
                String distanceStr = "Distance: " + String(average) + " cm";
                pCharacteristic->setValue(distanceStr.c_str());
                pCharacteristic->notify();
                Serial.println("Notify value: " + distanceStr);
            }
            previousMillis = currentMillis;
        }
    }

    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        Serial.println("Start advertising");
        oldDeviceConnected = deviceConnected;
    }

    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

    delay(1000);
}
