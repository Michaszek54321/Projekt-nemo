//Dołączamy biblioteki OneWire i DallasTemperature
#include <OneWire.h>
#include <DallasTemperature.h>

//Wybieramy pin, do którego podłączony został czujnik temperatury
const int oneWireBus = 4;

//Komunikujemy, że będziemy korzysać z interfejsu OneWire
OneWire oneWire(oneWireBus);
//Komunikujemy, że czujnik będzie wykorzystywał interfejs OneWire
DallasTemperature sensors (&oneWire);
void setup() {
  //Ustawiamy prędkość dla monitora szeregowego
  Serial.begin(115200);
  sensors.begin();
}

void loop() {
  //Odczyt temperatury
  sensors.requestTemperatures();
  //Odczyt w stopniach celsjusza
  float temperatureC = sensors.getTempCByIndex(0);
  //Odczyt w stopniach Farenheita
  float temperatureF = sensors.getTempFByIndex(0);
  //Wypisanie danych do monitora szeregowego
  Serial.print("Zmierzona temperatura: ");
  Serial.print(temperatureC);
  Serial.println("°C");
  Serial.print("Zmierzona temperatura: ");
  Serial.print(temperatureF);
  Serial.println("°F");
  //Odczyt temperatury co 5 sekund
  delay(5000);
}