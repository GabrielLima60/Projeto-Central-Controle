#include <iostream>
#include <map>

#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <esp_now.h>

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// ----------------------------
// REPLACE WITH YOUR RECEIVER MAC Address
uint8_t broadcastAddress[] = {0xEC, 0x64, 0xC9, 0x85, 0xA3, 0x9C}; // EC:64:C9:85:A3:9C
esp_now_peer_info_t peerInfo;

// Structure to send
typedef struct pin_message
{
  int pin_id;
  bool pin_on;
} pin_message;
pin_message pinData;
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

// Structure to receive
typedef struct sensor_message
{
  float temperature;
  float humidity;
  int soil_humidity;
} sensor_message;
sensor_message sensorData;
void OnDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);

float temperature;
float humidity;
int soil_humidity;
int active_time = 0;

int auto_temperature = 1000;
int auto_humidity = 1000;
int auto_soil_humidity = 1000;
int auto_active_time = 30;

SPIClass mySpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

int x = 320 / 2; // center of display
int y = 100;
int buttonHoriz = 100;
int buttonVetic = 50;
int fontSize = 5;

std::map<String, boolean> dictTela = {
    {"Tela 1", false},
    {"Tela 2", false}};
String listTela[] = {"Tela 1", "Tela 2"};
String currentTela = listTela[0];
int currentIndex = 0;

void loop2(void* pvParameters);

void setup()
{
  Serial.begin(115200);

  WiFi.mode(WIFI_MODE_STA);          // configura o WIFi para o Modo de estação WiFi
  Serial.print("Endereço MAC: ");    // A0:A3:B3:AB:5F:7C
  Serial.println(WiFi.macAddress()); // retorna o endereço MAC do dispositivo

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataReceived);

  // Once ESPNow is successfully Init, we will register for Send CB to get the status of Transmitted packet
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }

  // Start the SPI for the touch screen and init the TS library
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);
  ts.setRotation(1);

  // Start the tft display and set it to black
  tft.init();
  tft.setRotation(1); // This is the display in landscape

  // Clear the screen before writing to it
  tft.fillScreen(TFT_BLACK);

  int x = 320 / 2; // center of display
  int y = 100;
  int fontSize = 2;
  tft.drawCentreString("Touch Screen to Start", x, y, fontSize);
}

void printTouchToSerial(TS_Point p)
{
  Serial.print("Pressure = ");
  Serial.print(p.z);
  Serial.print(", x = ");
  Serial.print(p.x);
  Serial.print(", y = ");
  Serial.print(p.y);
  Serial.println();
}

void printTouchToDisplay(TS_Point p)
{
  // Clear screen first
  tft.fillScreen(TFT_BLACK);

  int x = 320 / 2; // center of display
  int y = 100;
  int fontSize = 4;

  tft.fillTriangle(80, 180, 20, 200, 80, 220, TFT_WHITE);
  tft.fillTriangle(240, 180, 300, 200, 240, 220, TFT_WHITE);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //tft.drawCentreString(currentTela, x, 190, fontSize);

  if (currentTela == "Tela 1") {
    tft.drawCentreString("Central de Controle", x, 20, fontSize);

    String display_text = "Temperatura: " + String(temperature);
    tft.drawCentreString(display_text, x, 65, fontSize -2);
    display_text = "Umidade do Ar: " + String(humidity);
    tft.drawCentreString(display_text, x, 90, fontSize -2);
    display_text = "Umidade do solo: " + String(soil_humidity);
    tft.drawCentreString(display_text, x, 115, fontSize -2);
    display_text = "Tempo acionado: " + String(active_time);
    tft.drawCentreString(display_text, x, 140, fontSize -2);

  
    if (dictTela[currentTela] == false)
    {
      tft.fillRect(x - buttonHoriz / 2, y+75, buttonHoriz, buttonVetic, TFT_RED);
      tft.setTextColor(TFT_BLACK, TFT_RED);
      tft.drawCentreString("OFF", x, y + 15 +75, fontSize);

      // struct off
      pinData.pin_id = 1;
      pinData.pin_on = false;
    }

    else
    {
      tft.fillRect(x - buttonHoriz / 2, y+75, buttonHoriz, buttonVetic, TFT_GREEN);
      tft.setTextColor(TFT_BLACK, TFT_GREEN);
      tft.drawCentreString("ON", x, y + 15 +75, fontSize);

      // struct on
      pinData.pin_id = 1;
      pinData.pin_on = true;
    }
  }

  else { //Tela 2
    tft.drawCentreString("Acionamento", x, 180, fontSize);
    tft.drawCentreString("Automatico", x, 210, fontSize);

    tft.drawCentreString("Temperatura: ", 100, 30, fontSize -2);
    tft.drawCentreString("Umidade: ", 100, 65, fontSize -2);
    tft.drawCentreString("Umidade do Solo: ", 105, 100, fontSize -2);
    tft.drawCentreString("Tempo Acionado: ", 105, 135, fontSize -2);

    tft.drawCentreString(String(auto_temperature), 247, 30, fontSize -2);
    tft.drawCentreString(String(auto_humidity), 247, 65, fontSize -2);
    tft.drawCentreString(String(auto_soil_humidity), 247, 100, fontSize -2);
    tft.drawCentreString(String(auto_active_time), 247, 135, fontSize -2);

    // Temperatura
    tft.fillTriangle(190, 30, 175, 40, 190, 50, TFT_WHITE);
    tft.fillTriangle(305, 30, 320, 40, 305, 50, TFT_WHITE);

    //Umidade
    tft.fillTriangle(190, 30 +35, 175, 40 +35, 190, 50 +35, TFT_WHITE);
    tft.fillTriangle(305, 30 +35, 320, 40 +35, 305, 50 +35, TFT_WHITE);

    //Umidade do Solo
    tft.fillTriangle(190, 30 +70, 175, 40 +70, 190, 50 +70, TFT_WHITE);
    tft.fillTriangle(305, 30 +70, 320, 40 +70, 305, 50 +70, TFT_WHITE);

    //Tempo Acionado
    tft.fillTriangle(190, 30 +105, 175, 40 +105, 190, 50 +105, TFT_WHITE);
    tft.fillTriangle(305, 30 +105, 320, 40 +105, 305, 50 +105, TFT_WHITE);
    
  }

  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&pinData, sizeof(pinData));
  if (result == ESP_OK)
  {
    Serial.println("Sent with success");
  }
  else
  {
    Serial.println("Error sending the data");
  }
  delay(1000);

  /*
  String temp = "Pressure = " + String(p.z);
  tft.drawCentreString(temp, x, y, fontSize);

  y += 16;
  temp = "X = " + String(p.x);
  tft.drawCentreString(temp, x, y, fontSize);

  y += 16;
  temp = "Y = " + String(p.y);
  tft.drawCentreString(temp, x, y, fontSize);
  */
}

void loop()
{

  if (ts.tirqTouched() && ts.touched())
  {
    TS_Point p = ts.getPoint();
    float x_tela = p.x / 12.5;       // Para o máximo ser 320
    float y_tela = p.y / 16.6666666; // Para o máximo ser 240

    if (x_tela > x - buttonHoriz / 2 and x_tela < x + buttonHoriz / 2 and y_tela > y +75 and y_tela < y + buttonVetic +75)
    {
      dictTela[currentTela] = !dictTela[currentTela];

      if (dictTela[currentTela]) {
        active_time = 30;

        xTaskCreatePinnedToCore (
          loop2,     // Function to implement the task
          "loop2",   // Name of the task
          16000,      // Stack size in bytes
          NULL,      // Task input parameter
          0,         // Priority of the task
          NULL,      // Task handle.
          0          // Core where the task should run
        ); 
      }
      else 
        active_time = 0;

    }

    else if (x_tela > 20 and x_tela < 100 and y_tela > 180 and y_tela < 220)
    {
      currentIndex = currentIndex - 1;
      if (currentIndex < 0)
        currentIndex = 2;
      currentTela = listTela[currentIndex];
      delay(300);
    }

    else if (x_tela > 220 and x_tela < 300 and y_tela > 180 and y_tela < 220)
    {
      currentIndex = currentIndex + 1;
      if (currentIndex > 1)
        currentIndex = 0;
      currentTela = listTela[currentIndex];
      delay(300);
    }
    // printTouchToSerial(p);
    printTouchToDisplay(p);

    delay(100);
  }
}

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  if (status == ESP_NOW_SEND_SUCCESS)
  {
    // print green icon on top right
    tft.fillCircle(310, 10, 5, TFT_GREEN);
  }
  else
  {
    // print red icon on top right
    tft.fillCircle(310, 10, 5, TFT_RED);
  }
}

// callback function that will be executed when data is received
void OnDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len)
{
  memcpy(&sensorData, incomingData, sizeof(sensorData));
  TS_Point p = ts.getPoint();
  temperature = sensorData.temperature;
  humidity = sensorData.humidity;
  soil_humidity = sensorData.soil_humidity;

  printTouchToDisplay(p);
}

void loop2(void* pvParameters) {
  while (1) {
    if (active_time >= 1) {
      delay (1000); // wait for a second
      active_time--;
      tft.fillRect(20, 130, 240, 30, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_WHITE);
      tft.drawCentreString("Tempo acionado: " + String(active_time), x, 140, fontSize -2);
    }
  }
}
