// Sensing environmental response[Temperature, Lux, BiopotentialVal] in Mimosa Pudica and relaying over BLE through ESP32::Ultra96V2.
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 15
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#include <Wire.h>
#include <BH1750.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
BH1750 lightMeter;

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  //Nordic UART Service
#define CHARACTERISTIC_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

int BPV; 
boolean BPVval = false;
int sensorPin = A0;        //pin number to use the ADC
int sensorValue = 0;      //initialization of sensor variable, equivalent to EMA Y
 
float EMA_a_low = 0.3;    //initialization of EMA alpha
float EMA_a_high = 0.5;
 
int EMA_S_low = 0;        //initialization of EMA S
int EMA_S_high = 0;
 
int highpass = 0;
int bandpass = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(12, INPUT);
  pinMode(13, INPUT);
  sensors.begin();
  lightMeter.begin();
  
  EMA_S_low = analogRead(sensorPin);      //set EMA S for t=1
  EMA_S_high = analogRead(sensorPin);
  
  // Create the BLE Device
  BLEDevice::init("MiMo_Sense");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  sensors.requestTemperatures();
  
  int temperature = sensors.getTempCByIndex(0);  
  int lux = lightMeter.readLightLevel();
  if((digitalRead(12) == 1)||(digitalRead(13) == 1)){
    Serial.println("Leads Off!");
    //BPV = 0;
    BPVval = false;
  }
  else{    
      //Serial.print("BPV: ");
      //Serial.println(analogRead(A0));
      //BPV = analogRead(A0);
      BPVval = true;  }
  sensorValue = analogRead(sensorPin);    //read the sensor value using ADC
  
  EMA_S_low = (EMA_a_low*sensorValue) + ((1-EMA_a_low)*EMA_S_low);  //run the EMA
  EMA_S_high = (EMA_a_high*sensorValue) + ((1-EMA_a_high)*EMA_S_high);
   
  //highpass = sensorValue - EMA_S_low;     //find the high-pass as before (for comparison)
  //bandpass = EMA_S_high - EMA_S_low;      //find the band-pass
  
  BPV = EMA_S_high - EMA_S_low;
  // notify changed value
  if (deviceConnected) {
    
     char TempStr [2];
     char BPVStr [2];
     char LuxStr [2];
     dtostrf (temperature, 1, 2, TempStr);
     dtostrf (BPV, 1, 2, BPVStr);
     dtostrf (lux, 1, 2, LuxStr);
     char MiMoDataStr [16];
     sprintf (MiMoDataStr, "% d,% d,% d" , temperature, lux, BPV);
     pCharacteristic->setValue(MiMoDataStr);
     pCharacteristic->notify();
     Serial.println(MiMoDataStr);
     delay(3); // bluetooth stack will go into congestion, if too many packets are sent!
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}
