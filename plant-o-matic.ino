#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <iarduino_DHT.h> 
#include <OneWire.h>
#include <DallasTemperature.h>
#include <iarduino_RTC.h>
#include <TimeLib.h>


byte degree[8] = // кодируем символ градуса
{
B00111,
B00101,
B00111,
B00000,
B00000,
B00000,
B00000,
};


byte l [8] = // кодируем символ (л)
{
0b01111,
0b01001,
0b01001,
0b01001,
0b01001,
0b01001,
0b11001,
0b00000
};
byte g [8] = // кодируем символ (г)
{
0b11111,
0b10001,
0b10000,
0b10000,
0b10000,
0b10000,
0b10000,
0b00000
};


byte ge [8] = // кодируем символ (ж)
{
0b10101,
0b10101,
0b10101,
0b01110,
0b01110,
0b10101,
0b10101,
0b00000
};

byte mz [8] = // кодируем символ (ь)
{
0b10000,
0b10000,
0b10000,
0b11110,
0b10001,
0b10001,
0b01111,
0b00000
};
byte y [8] = // кодируем символ (У)
{
B10001,
B10001,
B01001,
B00110,
B00100,
B01000,
B10000,
B00000

};

byte p [8] = // кодируем символ (п)
{
0b11111,
0b10001,
0b10001,
0b10001,
0b10001,
0b10001,
0b10001,
0b00000
};


#define RELE_VENT_PIN 27
#define ONE_WIRE_BUS 32 // номер пина к которому подключен DS18B20

#define DHT_PIN 2
#define RELE_MOISTURE_PIN 23
#define MOISTUSE_PIN A0
#define RELE_POMPA_PIN 24

// время включения полива в часах и минутах
#define TIME_HOUR         9
#define TIME_MINUTES      10

#define MOISTUSE_MIN 300
#define POLIV_INTERVAL 5
#define VENT_INTERVAL 120

//#define VENT_ON 1
//#define VENT_OFF 0

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
iarduino_DHT DHT22(DHT_PIN);
LiquidCrystal_I2C lcd(0x27,20,4);  // Устанавливаем дисплей
iarduino_RTC time(RTC_DS1302, 40, 38, 39); // подключаем RTC модуль на базе чипа DS1302, указывая выводы Arduino подключённые к выводам модуля RST, CLK, DAT

// состояния системы 
enum State
{
    OFF,
    ON,
};

 // объявляем переменную state
State statePompa;
State stateVent;

float sensHum = 0;
int sensorHumGroundValue =0;




// временя последнего полива
long tmWatering;
// время последней проверки влажности почвы
long tmMoistureCheck;
// время последнего включения вентилятора
long tmVentOn;



void setup()
{ 
  lcd.init();                     
  lcd.backlight();// Включаем подсветку дисплея
  lcd.createChar(1, degree); // Создаем символ под номером 1
  lcd.createChar(2,l);
  lcd.createChar(3,ge);
  lcd.createChar(5,mz);
  lcd.createChar(6,p);
  lcd.createChar(7,y);
  pinMode(RELE_VENT_PIN, OUTPUT);
  pinMode(RELE_POMPA_PIN, OUTPUT);
  digitalWrite( RELE_POMPA_PIN, HIGH );
  pinMode(RELE_MOISTURE_PIN, OUTPUT);
  digitalWrite( RELE_MOISTURE_PIN, HIGH );
  
  ventOff();
  
  
  statePompa = OFF;
 // __TIMESTAMP__
  DS18B20.begin(); // DS18B20
  time.begin();
   time.settime(0,0,9,31,01,18,2);
  // синкаем вручную время
  time.gettime();
  setTime(
    time.Hours,
    time.minutes,
    time.seconds,
    time.day,
    time.month,
    time.year);

}


void loop()
{
  int chk = DHT22.read();
  DS18B20.requestTemperatures(); 

  // управление вентилятором
  // приоритет контроль перегрева, когда t > 28C
  // влажность держим ниже 65
  // включаем пока не упадёт ниже а потом с запасом на 120 сек 
  if (DHT22.tem > 27) {
    ventOn();    // tmVentOn обновляется каждый раз
  }
  if (DHT22.hum > 65) {
    ventOn();
  }
  // просто обновляем немного воздух каждые 10 минут
  if ( stateVent == OFF && (now() - (tmVentOn + VENT_INTERVAL) > 600)) {
    ventOn();
  }
  // if (ventState == VENT_ON && (DHT22.hum < 45 || DHT22.tem < 23)) {
  if (stateVent == ON && ( now() - tmVentOn > VENT_INTERVAL) ) {
    ventOff();
  }

  // полив если сухо по расписанию
  // запрашиваем данные с часов
  // clock.read();
  int tm_hour = hour();
  int tm_minute = minute();

  // если система выключена
  if (statePompa == OFF) {
    // включаем полив если надо
    if (tm_hour == TIME_HOUR && tm_minute == TIME_MINUTES && ((tmWatering + 60) < now())) {      
      // проверяем влажность земли
      if (isGroundDry()){
        wateringOn();
        // запоминаем время полива
        tmWatering = now();
      }
    }
  }
  // если полив включен
  if (statePompa == ON) {
    if (now() - tmWatering > POLIV_INTERVAL) {
      // если прошёл заданный интервал времени для полива
      // выключаем полив
      wateringOff();
    }
  }

  displayInfo();
  
}


// время последнего вывода
long tmDisplay;

// отображаем текущую инфу
void displayInfo() {

  String tmp;

  // не частим с выводом
  if (tmDisplay + 1 > now()) {
    return false;
  }
  lcd.setCursor(0, 0);
  lcd.print("B\2A\3HOCT\5        %");
  lcd.setCursor(12, 0);
  lcd.print(DHT22.hum);
  lcd.setCursor(0, 1);
  lcd.print("TEM\6      \1");
  lcd.setCursor(5, 1);
  lcd.print(DHT22.tem);
  lcd.setCursor(11, 1);
  lcd.print(" " +String(DS18B20.getTempCByIndex(0),2));
  lcd.setCursor(0, 2);
  if (stateVent == ON){
    tmp = "BEHT ON ";
  } else tmp = "BEHT OFF";
  lcd.print( tmp );
  lcd.setCursor(10,2);
  lcd.print(sensorHumGroundValue + String("   "));
  lcd.setCursor(0,3);
  lcd.print(time.gettime("d-m-Y H:i:s"));
  // lcd.noBacklight();
  // lcd.blink();
}

// вентилятор
void ventOn(){
    digitalWrite(RELE_VENT_PIN, LOW);
    stateVent = ON;
    tmVentOn = now();
}

void ventOff(){
    digitalWrite(RELE_VENT_PIN, HIGH);
    stateVent = OFF;
}

enum stateMeasure {
//  START,
//  RELE_ON,
  STOP,
  MEASURING,
  TIMEDELAY
};

stateMeasure groundCheckState = STOP;
unsigned long tmMeasureEnd;

// проверка сухой земли
bool isGroundDry() {

//  switch (groundCheckState) {
//
//    case STOP:
//      //замыкаем реле, питающее датчики влажности
//      digitalWrite( RELE_MOISTURE_PIN , LOW );
//      groundCheckState = MEASURING;
//      tmMeasureEnd = 1000 + now();
//      break;
//
//    case MEASURING:
//      // ждём когда прогреется :)
//      if ( tmMeasureEnd < now() ) {
//        sensorHumGroundValue = analogRead(MOISTUSE_PIN);
//        tmMoistureCheck = now();
//        //размыкаем реле, питающее датчик влажности
//        digitalWrite( RELE_MOISTURE_PIN, HIGH );
//        groundCheckState = TIMEDELAY;
//      }
//      break;
//    case TIMEDELAY:
//      // чтобы не клацать 20 раз
//      if ( tmMoistureCheck + 60 < now()) {
//        groundCheckState = STOP;
//      }
//      break;
//  }   
  
//  if ( sensorHumGroundValue < MOISTUSE_MIN ){ 
//    return true;
//  }      
//  return false;
  
  
  // чтобы не клацать 20 раз
  if ( tmMoistureCheck + 60 < now()) {
      //замыкаем реле, питающее датчики влажности
      digitalWrite( RELE_MOISTURE_PIN , LOW ); 
      //ждем 3с для установления значения
      delay( 3000 ); 
      sensorHumGroundValue = analogRead(MOISTUSE_PIN);
      tmMoistureCheck = now();
      //размыкаем реле, питающее датчик влажности
      digitalWrite( RELE_MOISTURE_PIN, HIGH ); 
  }
  if ( sensorHumGroundValue < MOISTUSE_MIN ){ 
    return true;
  }      
  return false;  
}

 
// функция включения полива
void wateringOn() {
  digitalWrite(RELE_POMPA_PIN, LOW);
  statePompa = ON;
}
 
// функция выключения полива
void wateringOff() {
  digitalWrite(RELE_POMPA_PIN, HIGH);
  statePompa = OFF;
}
