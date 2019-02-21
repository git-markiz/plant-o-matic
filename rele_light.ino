#include <LiquidCrystal_I2C.h>

#include <iarduino_DHT.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <iarduino_RTC.h>
//  iarduino_RTC time(RTC_DS1302, 1, 2, 3); // подключаем RTC модуль на базе чипа DS1302, указывая выводы Arduino подключённые к выводам модуля RST, CLK, DAT
//  iarduino_RTC time(RTC_DS1307);          // подключаем RTC модуль на базе чипа DS1307, используется аппаратная шина I2C
iarduino_RTC time(RTC_DS3231);          // подключаем RTC модуль на базе чипа DS3231, используется аппаратная шина I2C
#include <TimeLib.h>


#define RELE_LIGHT_PIN 13
#define LIGHT_FAN_PWM_PIN 9
#define AIR_FAN_PWM_PIN 10

// номер пина к которому подключен DS18B20
#define ONE_WIRE_BUS_PIN 5 

// пин темп и влажн воздуха
#define DHT_PIN 6

// RPM ввод тахоимпульсов
#define RPM_FAN_IN 2 

// регистры таймера для ШИМ вентиляторов  
#define LIGHT_FAN_SPEED     OCR1A
#define AIR_FAN_SPEED       OCR1B

// расписание досвета
int timeLight[][2] = {
  {6, 30}, {8, 30},
  {17, 0}, {23, 50}
};

/* --- создаём объекты ---*/
iarduino_DHT DHT22(DHT_PIN);
OneWire oneWire(ONE_WIRE_BUS_PIN);
DallasTemperature DS18B20(&oneWire);
LiquidCrystal_I2C lcd(0x27,20,4);




// 2 состояния любой системы
enum State
{
  OFF,
  ON,
};

// объявляем переменную state
State stateLight;
State stateLightFan;

void setup() {

  lcd.init();                     
  lcd.backlight();// Включаем подсветку дисплея

  Serial.begin(9600);
  DS18B20.begin(); // DS18B20
int deviceCount = (int)DS18B20.getDeviceCount(); // Получаем количество найденных датчиков 
Serial.println("Found " + String(deviceCount) + " devices."); // Показываем количество найденных датчиков 
// Получаем режим питания датчиков - паразитное или обычное 
Serial.print("Parasite power is: "); 
Serial.println(DS18B20.isParasitePowerMode() ? "ON" : "OFF"); // Выводим режим питания


  // запуск и синхронизация времени между библиотеками
  time.begin();
  time.gettime();
  setTime(time.Hours, time.minutes, time.seconds, time.day, time.month, time.year);

  // инициируем реле включения света
  pinMode(RELE_LIGHT_PIN, OUTPUT);
  digitalWrite( RELE_LIGHT_PIN, HIGH );
  
  stateLight = OFF;
  stateLightFan = OFF;

  // Configure Timer 1 for PWM @ 25 kHz.
  TCCR1A = 0;           // undo the configuration done by...
  TCCR1B = 0;           // ...the Arduino core library
  TCNT1  = 0;           // reset timer
  TCCR1A = _BV(COM1A1)  // non-inverted PWM on ch. A
         | _BV(COM1B1)  // same on ch; B
         | _BV(WGM11);  // mode 10: ph. correct PWM, TOP = ICR1
  TCCR1B = _BV(WGM13)   // ditto
         | _BV(CS10);   // prescaler = 1
  ICR1   = 320;         // TOP = 320

  // Set the PWM pins as output.
  pinMode( LIGHT_FAN_PWM_PIN, OUTPUT);
  pinMode( AIR_FAN_PWM_PIN, OUTPUT);

  // подсчет RPM кулера на лампе
  pinMode(RPM_FAN_IN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RPM_FAN_IN), pickRPM, RISING);


AIR_FAN_SPEED = 270;
}

unsigned long cur, prev1, prev2, prev3 = 0; // Time placeholders
int tempLamp;
unsigned int rpm;

void loop() {
    cur = millis();
//  if (millis() % 1000 == 0) { // если прошла 1 секунда

    int chk = DHT22.read();
//Serial.println(DHT22.tem +String(" ")+ String(DHT22.hum));

    if (cur - prev3 >= 3000){
      prev3 = cur;
      DS18B20.requestTemperatures();
      DS18B20.setWaitForConversion(true);
      tempLamp = DS18B20.getTempCByIndex(0);
    }
//Serial.println(  tempLamp); //String(DS18B20.getTempCByIndex(0),2));

    if (cur - prev1 >= 500){
      prev1 = cur;
      calcRpm(&rpm);
    }
    
    int tm_hour = hour();
    int tm_minute = minute();

    if (cur - prev2 >= 1000){
      prev2 = cur;
      if ( ON == lightSheduler(tm_hour, tm_minute) ) { // @todo избыточная проверка расписания
        lightOn();
      } else {
        lightOff();
      }
    }

//    Serial.println(time.gettime("d-m-Y, H:i:s, D")+String(" h")+String(tm_hour)+String(" m")+String(tm_minute)); // выводим время
//    Serial.println(rpm);
      
    displayInfo();
      
}

/* ///////////////////////// */

 

/* --- RPM вычисления --- */
volatile unsigned long duration = 0; // accumulates pulse width
volatile unsigned int pulsecount = 0;
volatile unsigned long previousMicros = 0;
unsigned int ticks = 0;


// по прерыванию суммирование тахоимпульсов
void pickRPM ()
{
  volatile unsigned long _currentMicros = micros();

  if (_currentMicros - previousMicros > 10000) // Prevent pulses less than 20k micros far.
  {
    duration += _currentMicros - previousMicros;
    previousMicros = _currentMicros;
    ticks++;
  }
}

// вычисление скорости вращения
void calcRpm(int *calc){
    unsigned long _duration = duration;
    unsigned long _ticks = ticks;
    duration = 0;
    ticks = 0;

    // Calculate fan speed
    float Freq = (1e6 / float(_duration) * _ticks) / 2;
    calc = int(Freq * 60);
}

/* --- свет --- */
// слежение за расписанием вкл/откл света
State lightSheduler(int tm_hour, int tm_minute) {
  State result;
  result = OFF;
  int tm = tm_hour *100 + tm_minute;
  
  for (int i = 0; i < (sizeof(timeLight) / sizeof(int) / 2); i = i + 2) {
    int t_on = timeLight[i][0] *100 + timeLight[i][1];
    int t_off = timeLight[i+1][0] *100 + timeLight[i+1][1];  
    if ( t_on <= tm && tm < t_off ){
      result = ON;
    }
  }
  return result;
}

// свет включение
State lightOn() {
  // не продолжаем если всё включено
  if (stateLight == ON) return ON;

  stateLight = OFF;
  // сначала включаем вентиляторы на лампах
  if ( ON == lightFanOn() ) {
    // а напряжение подаём только после старта вентиляторов
    digitalWrite(RELE_LIGHT_PIN, LOW);
    // @todo здесь контроль включения света по фотодатчику
    // if ( ON == checkLightOn() )
    stateLight = ON;
  }
  return stateLight;
}

// выключение света
// сначала гасим лампы
// а потом инициируем выключение кулеров
// но не следим за ними
State lightOff() {
  // не продолжаем если всё выключено
  if (stateLight == OFF) return OFF;

  digitalWrite(RELE_LIGHT_PIN, HIGH);
  stateLight = OFF;
  lightFanOff();

  return stateLight;
}

// включение кулеров на лампах
// @todo
// реализация управляемых кулеров от температуры
// сейчас блок питания кулеров на линии питания от реле света
State lightFanOn() {
  LIGHT_FAN_SPEED = 160;
  return ON;
}

State lightFanOff() {
  LIGHT_FAN_SPEED = 0;
  return OFF;
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
  tmDisplay = now();
  lcd.setCursor(0, 0);
  lcd.print("t ");
  lcd.setCursor(2, 0);
  lcd.print(String(DHT22.tem,1));
  lcd.setCursor(7, 0);
  lcd.print("hum %");
  lcd.setCursor(12, 0);
  lcd.print(DHT22.hum);
  
  lcd.setCursor(0, 1);
  lcd.print("RPM");  
  lcd.setCursor(4, 1);
  lcd.print(rpm+String("  "));
  lcd.setCursor(11, 1);
  lcd.print(" " +String(tempLamp)+String("  "));
lcd.setCursor(0, 2); lcd.print(String(LIGHT_FAN_SPEED)+String("  "));
  lcd.setCursor(0,3);
  //lcd.print(now());
  lcd.print(time.gettime("d-m-Y H:i:s"));
  // lcd.noBacklight();
  // lcd.blink();
}


