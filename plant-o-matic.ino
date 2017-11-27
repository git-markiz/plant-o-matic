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


#define RELE_VENT_PIN 22
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

#define VENT_ON 1
#define VENT_OFF 0

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
iarduino_DHT sensor(DHT_PIN);
LiquidCrystal_I2C lcd(0x27,20,4);  // Устанавливаем дисплей
iarduino_RTC time(RTC_DS1302, 40, 38, 39); // подключаем RTC модуль на базе чипа DS1302, указывая выводы Arduino подключённые к выводам модуля RST, CLK, DAT

float sensHum = 0;
int ventState = VENT_OFF;
int sensorHumGroundValue =0;
String tmp;
// состояния системы
enum State
{
    OFF,
    ON,
};
 
// объявляем переменную state
State statePompa;
//State stateVent;

// переменная для хранения времени в формате unixtime
long unixTime;
// проверка влажности почвы
long timeMoistureCheck;

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
  // pinMode(RELE_VENT_PIN, OUTPUT);
  pinMode(RELE_POMPA_PIN, OUTPUT);
  digitalWrite( RELE_POMPA_PIN, HIGH );
  pinMode(RELE_MOISTURE_PIN, OUTPUT);
  digitalWrite( RELE_MOISTURE_PIN, HIGH );
  
  vent(VENT_OFF);
  
  statePompa = OFF;
 // __TIMESTAMP__
  sensors.begin(); // DS18B20
  time.begin();
  // time.settime(0,7,0,23,11,17,2);
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
  int chk = sensor.read();
  sensors.requestTemperatures(); 
  
  if (sensor.hum > 80) {
    vent(VENT_ON);
    // lcd.backlight();
  } else if (ventState == VENT_ON) {
    vent(VENT_OFF);
   // lcd.noBacklight();
  }

  // полив если сухо по расписанию
  // запрашиваем данные с часов
  // clock.read();
  int tm_hour = hour();
  int tm_minute = minute();

  // если система выключена
  if (statePompa == OFF) {
    if (tm_hour == TIME_HOUR && tm_minute == TIME_MINUTES && ((unixTime + 60) < now())) {      
      // включаем полив если надо
      // проверяем влажность земли
      if (isGroundDry()){
        // запоминаем текущее время

        unixTime = now(); //clock.getUnixTime();
        wateringOn();
      }
    }
  }
  // если полив включен
  if (statePompa == ON) {
    if (now() - unixTime > POLIV_INTERVAL) {
      // если прошёл заданный интервал времени для полива
      // выключаем полив
      wateringOff();
    }
  }

  
  
  lcd.setCursor(0, 0);
  lcd.print("B\2A\3HOCT\5        %");
  lcd.setCursor(12, 0);
  lcd.print(sensor.hum);
  lcd.setCursor(0, 1);
  lcd.print("TEM\6      \1");
  lcd.setCursor(5, 1);
  lcd.print(sensor.tem);
  lcd.setCursor(11, 1);
  lcd.print(" " +String(sensors.getTempCByIndex(0),2));
  lcd.setCursor(0, 2);
  if (ventState == VENT_ON){
    tmp = "BEHT ON ";
  } else tmp = "BEHT OFF";
  lcd.print( tmp );
  lcd.setCursor(10,2);
  lcd.print(sensorHumGroundValue + String("   "));
  lcd.setCursor(0,3);
  //if(millis()%500==0){ // если прошла 1 секунда
      lcd.print(time.gettime("d-m-Y H:i:s")); // выводим время
  //    delay(1); // приостанавливаем на 1 мс, чтоб не выводить время несколько раз за 1мс
  //  }
  delay(1000);
  // lcd.noBacklight();
  // lcd.blink();

}



void vent(int state){
  //if (ventState != state) {
    digitalWrite(RELE_VENT_PIN, !state);
    ventState = state;
  //}
}


// проверка сухой земли
bool isGroundDry() {
  // чтобы не клацать 20 раз
  if ( timeMoistureCheck + 60 < now()) {
      //замыкаем реле, питающее датчики влажности
      digitalWrite( RELE_MOISTURE_PIN , LOW ); 
      //ждем 3с для установления значения
      delay( 3000 ); 
      sensorHumGroundValue = analogRead(MOISTUSE_PIN);
      timeMoistureCheck = now();
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
