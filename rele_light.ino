

#include <iarduino_RTC.h>
//  iarduino_RTC time(RTC_DS1302, 1, 2, 3); // подключаем RTC модуль на базе чипа DS1302, указывая выводы Arduino подключённые к выводам модуля RST, CLK, DAT
//  iarduino_RTC time(RTC_DS1307);          // подключаем RTC модуль на базе чипа DS1307, используется аппаратная шина I2C
    iarduino_RTC time(RTC_DS3231);          // подключаем RTC модуль на базе чипа DS3231, используется аппаратная шина I2C
#include <TimeLib.h>

#define RELE_LIGHT_PIN 13

// расписание досвета
int timeLight[][2] = {
    {6,30}, {8,0},
    {17,0}, {23,50}
  };
  
// время включения света в часах и минутах
//int timeLightOn[3][2] = {
//    {6,30},
//    {17,0}
//  };
//// время выключения света
//int timeLightOff[3][2] = {
//    {8,0},
//    {23,50}
//  };
  

// состояния системы 
enum State
{
    OFF,
    ON,
};

 // объявляем переменную state
State stateLight;
State stateLightFan;

void setup() {
    delay(300);
    Serial.begin(9600);
   time.begin();
   // time.settime(0,35,22,8,11,18,4);  // 0  сек, 51 мин, 21 час, 27, октября, 2015 года, вторник
   time.gettime();
  setTime(
    time.Hours,
    time.minutes,
    time.seconds,
    time.day,
    time.month,
    time.year);
  pinMode(RELE_LIGHT_PIN, OUTPUT);
  digitalWrite( RELE_LIGHT_PIN, HIGH );
  stateLight = OFF;
  stateLightFan = OFF;

}

void loop(){
    if(millis()%1000==0){ // если прошла 1 секунда

  int tm_hour = hour();
  int tm_minute = minute();
  
  if ( ON == lightSheduler(tm_hour, tm_minute) ){  // @todo избыточная проверка расписания
    
      lightOn();
    
  }else{
    lightOff();
  }

//      Serial.println(time.gettime("d-m-Y, H:i:s, D")+String(" h")+String(tm_hour)+String(" m")+String(tm_minute)); // выводим время
      delay(1); // приостанавливаем на 1 мс, чтоб не выводить время несколько раз за 1мс
    }
}

/* ///////////////////////// */

// слежение за расписанием вкл/откл света
State lightSheduler(int tm_hour, int tm_minute){
  State result;
  result = OFF;
  for (int i=0; i< (sizeof(timeLight)/sizeof(int)/2); i=i+2){
    if (tm_hour >= timeLight[i][0] && tm_minute >= timeLight[i][1]
    && tm_hour <= timeLight[i+1][0] && tm_minute < timeLight[i+1][1]){
      result = ON;
    }
  }
//  for (int i=0; i< (sizeof(timeLightOn)/sizeof(int)/2); i++){
//    if (tm_hour >= timeLightOn[i][0] && tm_minute >= timeLightOn[i][1]
//    && tm_hour <= timeLightOff[i][0] && tm_minute < timeLightOff[i][1]){
//      result = ON;
//    }
//  }
  return result;
}

// свет включение
State lightOn(){
  // не продолжаем если всё включено
  if (stateLight == ON) return ON;

  stateLight = OFF;
  // сначала включаем вентиляторы на лампах
  if ( ON == lightFanOn() ){
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
State lightOff(){
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
  return ON;
}

State lightFanOff() {
  return OFF;
}

