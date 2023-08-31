#include <DHT.h>	// подключение библиотеки DHT
#include <LiquidCrystal.h>	// подключение библиотеки LiquidCrystal
#include <Thread.h>	// подключение библиотеки Thread
#include <SoftwareSerial.h>	// подключение библиотеки SoftwareSerial
#include "VirtuinoCM.h"	// подключение библиотеки VirtuinoCM
 
 #define DHTTYPE DHT11	// тип датчика DHT
 // значениЯ длЯ условий
 #define  SECOND 1000ul
 #define  MINUTE (SECOND*60ul)
 #define  HOUR (MINUTE*60ul)

//Ќастройка Wi-Fi соединениЯ
const char* ssid = "****";	// SSID сети
const char* password = "****";	// Џароль сети
int port=8000;	// ‘тандартный порт сервера Virtuino
const char* serverIP = "192.168.0.150";	// IP-адрес сервера

//VirtuinoCM  настройка
VirtuinoCM virtuino;               
#define V_memory_count 32          // размер виртуальной памЯти. можно изменить его на число <=255)
float V[V_memory_count];           // ќтот массив синхронизируетсЯ с памЯтью Virtuoso V. Њожно изменить тип на int, long и т.д.

boolean debug = true;              // установить длЯ этой переменной значение false в финальном коде, чтобы уменьшить времЯ запроса.

 // контакт подключения входа данных модулЯ DHT11 
 int pinDHT11=A4;
  // контакт подключениЯ аналогового выхода датчика уровнЯ воды
 int pinWaterLevelHigh=A0;	// выход датчика верхнего уровнЯ
 int pinWaterLevelLow=A1;	 // выход датчика нижнего уровнЯ
  // контакт подключениЯ аналогового выхода фоторезистора
 int pinPhotoresistor=A2;
  // контакт подключениЯ аналогового выхода датчика влажности почвы
 int pinSoilMoisture=A3;
 // контакты реле
 int pinLamp=10;	//реле систамы освещениЯ
 int pinFun=11;		//реле систамы вентилЯции
 int pinPump=12;	//реле систамы полива
 int pinValve=13;	//реле систамы набора воды

 int V10_TEMP_DETECT=30;	//значение порогв срабатываниЯ системы вентилЯции
 int V11_MOISTURE_DETECT=500;	//значение порогв срабатываниЯ системы полива
 int V12_LIGHT_DETECT=500;	//значение порогв срабатываниЯ системы освещениЯ
 
 // ‚спомогательные переменные длЯ виртуальных пинов
 int V20;
 int V21;
 int V22;
 int V23;
 int V20_TIME_SOIL = (20*SECOND);//(4*HOUR);
 int V21_TIME_TEMP = (10*SECOND);//(1*HOUR);
 int V22_TIME_LIGHT = (10*SECOND);//(1*HOUR);
 int V23_TIME_WATER = (10*SECOND);//(1*MINUTE);

// ЋбозначениЯ контактов дисплеЯ 1602
const int rs = 9, en = 8, d4 = 7, d5 = 6, d6 = 5, d7 = 4;
LiquidCrystal LCD(rs, en, d4, d5, d6, d7);

SoftwareSerial espSerial =  SoftwareSerial(2,3);      // arduino RX pin=2  arduino TX pin=3

Thread SoilThread = Thread(); // создаем поток управления датчиком влажности почвы
Thread WLevelThread = Thread(); // создаем поток управления датчиком уровня воды
Thread FunThread = Thread(); // создаем поток управления вентиляцией
Thread LightThread = Thread(); // создаем поток управления светом

// создание экземплЯра объекта DHT
 DHT dht(pinDHT11, DHTTYPE);
 
void setup() {
   // запуск последовательного порта
  if (debug) {
    Serial.begin(9600);
    while (!Serial) continue;
  }
  espSerial.begin(9600);  
  espSerial.setTimeout(50);

  virtuino.begin(onReceived,onRequested,256);  //‡апуск Virtuino. “становите буфер равным 256. ‘ помощью этого буфера Virtuino может управлЯть примерно 28 выводами (1 команда = 9 байт). „лЯ команд T(text) с 20 символами требуетсЯ 20+6 байт
  //virtuino.key="1234";                       //Џароль Virtuino
  connectToWiFiNetwork();	//‚ызов процедуры подключениЯ к WiFi
  
   dht.begin();   //инициализируем DHT11
   LCD.begin(16, 2); //инициализируем дисплей

// Ћбозначим начальные состоЯниЯ пинов
  pinMode(pinLamp,OUTPUT);digitalWrite(pinLamp,LOW);
  pinMode(pinFun,OUTPUT);digitalWrite(pinFun,LOW);
  pinMode(pinPump,OUTPUT);digitalWrite(pinPump,LOW);
  pinMode(pinValve,OUTPUT);digitalWrite(pinValve,LOW);
  pinMode(pinWaterLevelHigh,INPUT);
  pinMode(pinWaterLevelLow,INPUT);
   
    SoilThread.onRun(soil);  // назначаем потоку задачу
    WLevelThread.onRun(wlevel);  // назначаем потоку задачу
    FunThread.onRun(fun);  // назначаем потоку задачу
    LightThread.onRun(light);  // назначаем потоку задачу

// €нициируем виртуальные порты
V[10]=V10_TEMP_DETECT;
V[11]=V11_MOISTURE_DETECT;
V[12]=V12_LIGHT_DETECT;
V[20] = V20;
V[21] = V21;
V[22] = V22;
V[23] = V23;
}

void loop() {
        virtuinoRun();	//запуск Virtuino
        vDelay(1000);

    if (V[10]!=V10_TEMP_DETECT) {				// …сли V10 был изменен
      Serial.println("V10="+String(V[10]));		// выводим значение V10
      V10_TEMP_DETECT=V[10];					// V10_TEMP_DETECT принимает новое значение V[10]
    }

    if (V[11]!=V11_MOISTURE_DETECT) {
      Serial.println("V11="+String(V[11]));
      V11_MOISTURE_DETECT=V[11];
    }

    if (V[12]!=V12_LIGHT_DETECT) {
      Serial.println("V12="+String(V[12]));
      V12_LIGHT_DETECT=V[12];
    }
    
    if (V[20]!=V20) {							// …сли V20 был изменен
      Serial.println("V20="+String(V[20]));		// выводим значение V20
      V20=V[20];								// V20 принимает новое значение V[20]
      V20_TIME_SOIL = (V20*HOUR);				// V20_TIME_SOIL принимает новое значение (V20*HOUR)
    }
    
    if (V[21]!=V21) {
      Serial.println("V21="+String(V[21]));
      V21=V[21];
      V21_TIME_TEMP  = (V21*HOUR);
    }
    
    if (V[22]!=V22) {
      Serial.println("V22="+String(V[22]));
      V22=V[22];
      V22_TIME_LIGHT = (V22*HOUR);
    }
    
    if (V[23]!=V23) { 
      Serial.println("V23="+String(V[23]));
      V23=V[23];
      V23_TIME_WATER = (V23*MINUTE);
    }

    SoilThread.setInterval(V20_TIME_SOIL); // задаем интервал срабатываниЯ, мсек
    WLevelThread.setInterval(V20_TIME_SOIL); // задаем интервал срабатываниЯ, мсек
    FunThread.setInterval(V21_TIME_TEMP); // задаем интервал срабатываниЯ, мсек
    LightThread.setInterval(V22_TIME_LIGHT); // задаем интервал срабатываниЯ, мсек

 //// проверка условий
        if (FunThread.shouldRun())
        FunThread.run(); // запускаем поток
                
        if (LightThread.shouldRun())
        LightThread.run(); // запускаем поток
        
        if (WLevelThread.shouldRun())
        WLevelThread.run(); // запускаем поток
        
        if (SoilThread.shouldRun())
        SoilThread.run(); // запускаем поток
        
         Serial.println();
         delay(1000);
 }

 void fun () {
   // ‚лажность воздуха
   // получение данных с DHT11
   int h = dht.readHumidity();  //‚лажность
   int t = dht.readTemperature();	//’емпература
   if (isnan(h) || isnan(t)) // Џроверка"…сли поступает h или t...
   {
   Serial.println("Failed to read from DHT");
   LCD.setCursor(0, 0); //устанавливаем курсор
   LCD.print("error H/T");
   }
   else
   {
   Serial.print("HumidityDHT11= "); Serial.print(h);Serial.println(" %");
   Serial.print("TemperatureDHT11= "); Serial.print(t);Serial.println(" C");
   LCD.setCursor(0, 0);	//устанавливаем курсор
   LCD.print("H/T=");LCD.print(h);LCD.print("/");LCD.print(t);
   V[1] = h;
   V[2] = t;
   }
     if(t > V10_TEMP_DETECT)
     digitalWrite(pinFun,HIGH);
     else
     digitalWrite(pinFun,LOW);
 }

 void light (){
  // Ћсвещенность
   int val2=analogRead(pinPhotoresistor);
   if (isnan(val2))
   {
   Serial.println("Failed to read from Photoresistor");
   LCD.setCursor(10, 0); //устанавливаем курсор
   LCD.print("error");
   }
   else
   {
   Serial.print("Light= "); Serial.println(val2);
   V[3] = val2;
   LCD.setCursor(10, 0); //устанавливаем курсор
   
      // освещенность
     if(val2 < V12_LIGHT_DETECT){
     digitalWrite(pinLamp,HIGH);
     LCD.print("Lamp:+");
     }
     else{
     digitalWrite(pinLamp,LOW);
     LCD.print("Lamp:-");
     }
   }
 }

void wlevel () {
      // “ровень воды
       int val0=digitalRead(pinWaterLevelHigh);
       int val1=digitalRead(pinWaterLevelLow);
          // уровень воды
       if (val1  == LOW){
        Serial.print("W_low");
        while (val0  == LOW) {
          digitalWrite(pinValve,HIGH);
          Serial.print("Valve is open");
          LCD.setCursor(9, 1); // устанавливаем курсор
          LCD.print("Pumping");
          delay(5000);
          val0=digitalRead(pinWaterLevelHigh);
        }}
          digitalWrite(pinValve,LOW);
          Serial.print("Water +");
          LCD.setCursor(9, 1); // устанавливаем курсор
          LCD.print("Water:+");
}

 void soil () {

       int val3=analogRead(pinSoilMoisture);
       V[4] = val3;
       if (isnan(val3))
       {
       Serial.println("Failed to read from SoilMoisture");
       LCD.setCursor(0, 1); //устанавливаем курсор
       LCD.print("SoilErr");
       }
       else
       {
          // влажность почвы
       if (val3 > V11_MOISTURE_DETECT) 
          watering();
          digitalWrite(pinPump,LOW);
          Serial.print("Moisture= "); Serial.println(val3);
          LCD.setCursor(0, 1); //устанавливаем курсор
          LCD.print("Soil:+  ");
       }
 }

 void watering(){
          digitalWrite(pinPump,HIGH);
          Serial.print("Watering");
          LCD.setCursor(0, 1); //устанавливаем курсор
          LCD.print("Watering");
          delay(V23_TIME_WATER);
          digitalWrite(pinPump,LOW);
  }

 // Џодключение к сети WiFi
void connectToWiFiNetwork(){
    Serial.println("Connecting to "+String(ssid));
    while (espSerial.available()) espSerial.read();
    espSerial.println("AT+GMR");       // print firmware info
    waitForResponse("OK",1000);
    espSerial.println("AT+CWMODE=1");  // configure as client
    waitForResponse("OK",1000);
    espSerial.print("AT+CWJAP=\"");    // connect to your WiFi network
    espSerial.print(ssid);
    espSerial.print("\",\"");
    espSerial.print(password);
    espSerial.println("\"");
    waitForResponse("OK",10000);
    espSerial.print("AT+CIPSTA=\"");   // set IP
    espSerial.print(serverIP);
    espSerial.println("\"");   
    waitForResponse("OK",5000);
    espSerial.println("AT+CIPSTA?");
    waitForResponse("OK",3000); 
    espSerial.println("AT+CIFSR");           // get ip address
    waitForResponse("OK",1000);
    espSerial.println("AT+CIPMUX=1");         // configure for multiple connections   
    waitForResponse("OK",1000);
    espSerial.print("AT+CIPSERVER=1,");
    espSerial.println(port);
    waitForResponse("OK",1000);
   
}

//”ункциЯ получениЯ команды
/* ќта функциЯ вызываетсЯ каждый раз, когда приложение Virtuino отправлЯет запрос на сервер длЯ изменениЯ значениЯ Pin-кода
* 'variableType' может быть символом типа V, T, O V=‚иртуальный вывод T=’екстовый вывод O=‚ывод PWM
* 'variableIndex' - это индекс pin-кода приложениЯ Virtuino
* 'valueAsText' - это значение, отправленное из приложениЯ */
 void onReceived(char variableType, uint8_t variableIndex, String valueAsText){     
    if (variableType=='V'){
        float value = valueAsText.toFloat();
        if (variableIndex<V_memory_count) V[variableIndex]=value;
    }
}

/* ќта функциЯ вызываетсЯ каждый раз, когда приложение Virtuino запрашивает считывание pin-кода*/
String onRequested(char variableType, uint8_t variableIndex){     
    if (variableType=='V') {
    if (variableIndex<V_memory_count) return  String(V[variableIndex]);
  }
  return "";
}

  void virtuinoRun(){
  if(espSerial.available()){
        virtuino.readBuffer = espSerial.readStringUntil('\n');
        if (debug) Serial.print('\n'+virtuino.readBuffer);
        int pos=virtuino.readBuffer.indexOf("+IPD,");
        if (pos!=-1){
              int connectionId = virtuino.readBuffer.charAt(pos+5)-48;  // get connection ID
              int startVirtuinoCommandPos = 1+virtuino.readBuffer.indexOf(":");
              virtuino.readBuffer.remove(0,startVirtuinoCommandPos);
              String* response= virtuino.getResponse();    // get the text that has to be sent to Virtuino as reply. The library will check the inptuBuffer and it will create the response text
              if (debug) Serial.println("\nResponse : "+*response);
              if (response->length()>0) {
                String cipSend = "AT+CIPSEND=";
                cipSend += connectionId;
                cipSend += ",";
                cipSend += response->length();
                cipSend += "\r\n";
                while(espSerial.available()) espSerial.read();    // clear espSerial buffer 
                for (int i=0;i<cipSend.length();i++) espSerial.write(cipSend.charAt(i));
                if (waitForResponse(">",1000)) espSerial.print(*response);
                waitForResponse("OK",1000);
              }
              espSerial.print("AT+CIPCLOSE=");espSerial.println(connectionId);
         }// (pos!=-1)
           
  } // if espSerial.available
        
}

// ”ункциЯ ожиданиЯ ответа
boolean waitForResponse(String target1,  int timeout){
    String data="";
    char a;
    unsigned long startTime = millis();
    boolean rValue=false;
    while (millis() - startTime < timeout) {
        while(espSerial.available() > 0) {
            a = espSerial.read();
            if (debug) Serial.print(a);
            if(a == '\0') continue;
            data += a;
        }
        if (data.indexOf(target1) != -1) {
            rValue=true;
            break;
        } 
    }
    return rValue;
}

 // vDelay
  void vDelay(int delayInMillis){long t=millis()+delayInMillis;while (millis()<t) virtuinoRun();}