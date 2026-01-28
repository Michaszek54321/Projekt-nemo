#include "esp_camera.h"
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
//#include <SharpIR.h>
#include "time.h"
#include <FastLED.h>
#include <EEPROM.h>

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//
//            You must select partition scheme from the board menu that has at least 3MB APP space.
//            Face Recognition is DISABLED for ESP32 and ESP32-S2, because it takes up from 15 
//            seconds to process single frame. Face Detection is ENABLED if PSRAM is enabled as well

// ===================
// Select camera model
// ===================
#define CAMERA_MODEL_AI_THINKER // Has PSRAM

#include "camera_pins.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid = "T-Mobile_Swiatlowod_3938"; // T-Mobile_Swiatlowod_3938; iPhone (Michał)
const char* password = "00689644583091587728"; // 00689644583091587728; lol12345

void startCameraServer();
void updateTemp(float temp);
void updateLightState(int light_state);
void updateHeaterState(int heater_state);
void setupLedFlash(int pin);
void updateWaterLevel(int level);

// eeprom
#define EEPROM_SIZE 52


// oswietlenie
#define LED_PIN     15
#define NUM_LEDS    1
CRGB leds[NUM_LEDS];


// temperatura
const int oneWireBus = 2;
OneWire oneWire(oneWireBus);
DallasTemperature sensors (&oneWire);

//proby czujnika odleglosci

#define czujnik_IR 13
int sec_since_measure = 0;
// SharpIR splawik(1080, 13);
// SharpIR splawik = SharpIR(13, 1080);

#define przekaznik_grzalka 14

// czas
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

// pomiar poziomu wody
bool isWaterLvlEnabled = false;

void updateWaterBool(bool state){
  isWaterLvlEnabled = state;
}

// zmienne grzałki i oświetlenia
int getHour(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return -1;
  }
  return timeinfo.tm_hour;
}

int g_light_on = 8;
int g_light_off = 22;
float g_heat_min = 24.0;
float g_heat_max = 25.5;

String g_light_mode = "auto";

void set_light_mode(char *light_mode){
  g_light_mode = light_mode;
  Serial.print("Light mode set to: ");
  Serial.println(g_light_mode);
}

void set_params(int l_on, int l_off, float h_min, float h_max) {
  g_light_on = l_on;
  g_light_off = l_off;
  g_heat_min = h_min;
  g_heat_max = h_max;
}

void check_lights(int current_hour){
  if (g_light_mode == "morning"){
    leds[0] = CRGB(255, 255, 255);
    FastLED.show();
    updateLightState(1);
  }
  else if (g_light_mode == "evening"){
    leds[0] = CRGB(255, 140, 0);
    FastLED.show();
    updateLightState(1);
  }
  else if (g_light_mode == "night"){
    leds[0] = CRGB(0, 0, 255);
    FastLED.show();
    updateLightState(1);
  }
  else if (g_light_mode == "auto"){
    if(current_hour >= g_light_on){
      //wlacz swiatlo
      leds[0] = CRGB(255, 255, 255);
      FastLED.show();
      updateLightState(1);
    }
    else if(current_hour >= g_light_off || current_hour < g_light_on){
      //wylacz swiatlo
      leds[0] = CRGB(0, 0, 0);
      FastLED.show();
      updateLightState(0);
    }
  }
}

void check_temps(float current_temp){
  if(current_temp < g_heat_min){
    //wlacz grzalke
    digitalWrite(przekaznik_grzalka, LOW);
    updateHeaterState(1);
    delay(100);
  }
  if(current_temp > g_heat_max){
    //wylacz grzalke
    digitalWrite(przekaznik_grzalka, HIGH);
    updateHeaterState(0);
    delay(100);
  }
}

void check_water_level(){
  
  WiFi.disconnect(true);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected hura");

    float volts = analogRead(czujnik_IR) * (3.3/4096);
    Serial.print("Volts: ");
    Serial.println(volts);
    int level = convert_to_percentage(volts);
    updateWaterLevel(level);
  }

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

int convert_to_percentage(float volts) {
  int percentage = 0;
  
  // TODO: dostosować te wartości trzeba
  float MIN_VOLTS = 1.2;
  float MAX_VOLTS = 3.3;

  if (volts >= MAX_VOLTS) {
    percentage = 100;
  } 
  else if (volts <= MIN_VOLTS) {
    percentage = 0;
  } 
  else {
    percentage = (int)((volts - MIN_VOLTS) / (MAX_VOLTS - MIN_VOLTS) * 100);
  }

  return percentage;
}

void check_eeprom(){
  int rozmiar_ee = EEPROM.length();
  int godziny[rozmiar_ee/2];
  int temperatury[rozmiar_ee/2];
  for (int i = 0; i < rozmiar_ee; i++) {
    godziny[i] = EEPROM.read(i);
    Serial.print("EEPROM[");
    Serial.print(i);
    Serial.print("]: ");
    Serial.println(godziny[i]);
  }
}

void setup() {
  // eeprom begin
  EEPROM.begin(EEPROM_SIZE);

  // serial begin
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  // start czujnika temperatury
  sensors.begin();

  // ustawienia przekaźnika grzałki
  pinMode(przekaznik_grzalka, OUTPUT);
  digitalWrite(przekaznik_grzalka, HIGH); //wylacz grzałkę na start

  // proby czujnika odleglosci
  // SharpIR splawik = SharpIR(13, 1080);
  pinMode(czujnik_IR, INPUT);

  // ustawienia oświetlenia
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);



  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }


  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if(config.pixel_format == PIXFORMAT_JPEG){
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif
  
  float volts = analogRead(czujnik_IR) * (3.3/4096);
  Serial.print("Volts: ");
  Serial.println(volts);
  int level = convert_to_percentage(volts);
  updateWaterLevel(level);

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  check_eeprom();
  // dzień w ostatniej komurce eepromu
}

void loop() {

  
  // czujnik odleglosci
  if (sec_since_measure >= 60){
    check_water_level();
    sec_since_measure = 0;
  }

  
  // czujnik temperatury
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);
  // liczymy średnią na podstawie poprzednich
  // ostatnią komurke eepromu nadpisujemy średnią temperaturą

  updateTemp(temperatureC);

  // zarządzanie oświetleniem
  int hour = getHour();

  check_lights(hour);


  // zarządzanie grzałką
  check_temps(temperatureC);

  sec_since_measure += 1;

  delay(1000);
}