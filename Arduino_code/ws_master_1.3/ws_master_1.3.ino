/*//////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////                                                                     ///////////////
  //////////////////////////          Weather station by Fedejack - MASTER 1.3                   ///////////////
  //////////////////////////                                                                     ///////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////
*/
//////////////////////////////////////LIBRERIE E DEFINIZIONI////////////////////////////////////////////////////
#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <PString.h>
#include <RF24Network.h>
#include <RF24.h>
#include "nRF24L01.h"

//TIME DS3231
#include <DS3232RTC.h>
#include <TimeLib.h>
#include "RTClib.h"//da includere per settare il ds3231 con l'ora attuale
RTC_DS3231 rtc;    //da includere per settare il ds3231 con l'ora attuale

//T/H DHT22 E DS18B20
#include "DHT.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 40 // pin digitale a cui è collegato il DS18B20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

//SHT35
#include "SHT31.h"
SHT31 sht35;

#define DHTPIN 38 // pin digitale a cui è collegato il DHT22
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

//PIN PLUVIOMETRO E ANEMOMETRO DeA////////
#define pinInterrupt 3 // pin a cui è collegato il pluviometro reed
#define reedPin 2 // pin a cui è collegato l'anemometro reed

//PIN DIREZIONE VENTO DeA///////////
#define pinEst A8//pin ANALOGICI collegamento banderuola
#define pinSud A9
#define pinWest A10
#define pinNord A11

//SD
#include <SdFat.h> 
#define SD_SS 36 // pin usato dalla microSD
SdFat SD;

//DISPLAY TFT 1.8" //display collegabile per verifica corretto funzionamento
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#define sclk 52 // 13 se si usa Arduino UNO
#define mosi 51 // 11 se si usa Arduino UNO
#define rst  A0
#define cs   A1
#define dc   A2
Adafruit_ST7735 tft = Adafruit_ST7735(cs, dc, rst);

//Radio
RF24 radio(A12,A13);
//const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

RF24Network network(radio);
const uint16_t this_node = 00;        // Address of our node in Octal format
const uint16_t other_node = 01;       // Address of the other node in Octal format

//VOLTAGGIO batteria
const int Pin_Batteria = A5;

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////VARIABILI///////////////////////////////////////////////////////
float tp; //temp sht35
float tp2; // variabile della temperatura dallas
float tpdht; // variabile temperatura dht
float ur; // variabile dell'umidità
byte ur2;
float dewPoint; // variabile del punto di rugiada
float windchill; // variabile del raffreddamento causato dal vento
float heatindexc; // variabile dell'indice di calore

/////////////////////////////////////VARIABILI PIOGGIA///////////////////////////////////////////////////
const float mmGoccia = 0.3; // constante del valore di ogni basculata del pluviometro 
volatile float mmPioggia = 0; // variabile conteggio mm pioggia giorno
float previousPioggia = 0;
float rainrate = 0; // variabile per l'intensità della pioggia
volatile unsigned long PluvioStep = 0; // variabili necessarie da utilizzare con millis() per calcolare l'intensità della pioggia
volatile unsigned long PluvioOldStep = 0; // variabili necessarie da utilizzare con millis() per calcolare l'intensità della pioggia
volatile unsigned long time1;
const unsigned long OF = 4294940295; // constante per evitare che durante l'overflow l'intensità della pioggia venga sengnata male

//media rainrate ultimi 10'
unsigned long tmediaRR = 0; //variabile tempo per calcolo media
const int intervalRR = 600000; //ongi 10' ricalcolo media
float sommaRR = 0; //somma rainrate
float mediaRR; //media
int iRR = 0; //contatore dei valori sommati
float RRmax = 0; //valore massimo nell'intervallo della media

////////////////////////////////VARIABILI ANEMOMETRO E DIREZIONE/////////////////////////////////////
const float Pi = 3.141593; // Pigreco
const float raggio = 0.055; // raggio dell'anemometro in metri
unsigned long Conteggio = 0;// variabile che contiene il conteggio delle pulsazioni          
volatile unsigned long Tempo = 0; // per conteggiare il tempo trascorso dalla prma pulsazione
volatile unsigned long tempoPrec = 0;
volatile float deltaT; 

//media velocitá vento in 10'
const long intervalwind = 600000;//10 min intervallo in cui ci calcola media velocità vento
unsigned long tmediawind = 0;//intervallo di calcolo media velocità vento
float sommaWind = 0;//somma delle velocità per la media
int iwind = 0;//conteggio valori velocità da mediare

int analogEst;//valori di input della banderuola
int analogSud;
int analogWest;
int analogNord;

//media direzione vento 
// ogni misurazione viene incrementato di 1 l'array mediaDir nella posizione corrispondente alla direzione (vedi array direzioni)
//in modo da segnare quale valori direzione viene misurato piú volte (moda), si elimina l'errore di piccole oscillazione della banderuola
const long intervaldir = 600000;//10 min intervallo in cui si calcola media direzione vento
int mediaDir[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};//array della moda direzione del vento
unsigned long tmediaDir = 0;//intervallo calcolo moda direzione vento
int maxDir = 0;//indice di direzione che si ripete più volte nell'intervallo
String direzioni[] = {"NNE","NE","ENE","E","ESE","SE","SSE","S","SSO","SO","OSO","O","ONO","NO","NNO","N"};
String erdir[] = {"1","eE","eS","eO","eN"};//array di errore se banderuola da valori analogici errati o se scollegati fili input
int gradi[] = {22,45,67,90,112,135,157,180,202,225,247,270,292,315,337,0};

//STRUCT PER VALORE DEL VENTO E DIREZIONE
struct WindValue { 
  float val; //velocità vento m/s
  float valmedio; //velocità media vento 10'
  int dir; //direzione del vento
  String dirmedia; //direzione vento media 10'
  int err; //errore direzione vento
  float max; //valore massimo nell'intervallo della media
  float raffica; //raffica massima giornaliera
};
WindValue wind;

//TEMPORIZZAZIONI
unsigned long previousMillis = 0; //necessaria per salvataggio sd
const long interval = 600000; //600000ogni 10 minuti salvataggio dati Day in file csv
byte giornoSalvato; // variabile per memorizzare lo stato del giorno attuale

String lastRadiotx; //ultima data invio dati radio
String lastDatasd; //ultima data salvataggio datalogDay
bool erradiotx; //errore inizializzazione radio
bool errSent; //errore invio dati, nessuna risposta da rx
bool resetRx = false;
bool resetStatus = true;
unsigned long startResetstatus = 0;

//VOLTAGGIO BATTERIA
float ValPinBatteria;
float Vbatteria;
const int R1 = 50000; 
const int R2 = 10000;

/////STRUCT RADIO////////////////////////////////////////////////////////////////////////////
struct radioVal { //traferimento dati via radio
  long data;//trasferimento data allo slave per salvataggio dati 
  int tp; //temperatura dallas
  int tpdht; //temperatura dht22
  int hum; //umiditá
  byte hum2;
  int pioggiaday; //pioggia del giorno
  int rainrate; //intensitá di pioggia
  byte winddir; //indice direzione vento, da usare per estrarre stringhe da Direzioni[]
  byte winddirMedio; //indice direzione media vento, da usare per estrarre stringhe da Direzioni[]
  int windval; //velocitá vento m/s
  int raffica; //raffica giornaliera massima
  int tp2;
  
  byte erWinddir; //errore direzione vento
  bool ersd; //errore inizializzazione sd
  long lastDatalog; //ultima ora salvataggio datalog Day
  byte volt; //tensione batteria alimetazione
  bool reset; //status reset a buon fine
  // 30/32 byte occuped in nRF24
};
radioVal radioTx;

/////////////////////DICHIARAZIONI INDIRIZZI EEPROM///////////////////////////////////////////
//N.B. Sono distanziati tra di loro in modo di aver spazio nella loro memorizzazione in modo pulito
//ACCUMULO PIOGGIA
const unsigned int eeAddress1 = 0;

//FUNZIONE PER PRENDERE I VALORI DELLA EEPROM PRECEDENTEMENTE MEMORIZZATI//
void Eeprom() {
  EEPROM.get (eeAddress1, mmPioggia);
}

/////////////////////////////////////////FUNZIONE FUSO ORARIO//////////////////////////////////
byte dstOffset (byte d, byte m, unsigned int y, byte h) {
  /* This function returns the DST offset for the current UTC time.
    This is valid for the EU, for other places see
    http://www.webexhibits.org/daylightsaving/i.html

    Results have been checked for 2012-2030 (but should work since
    1996 to 2099) against the following references:
    - http://www.uniquevisitor.it/magazine/ora-legale-italia.php
    - http://www.calendario-365.it/ora-legale-orario-invernale.html
  */

  // Day in March that DST starts on, at 1 am
  byte dstOn = (31 - (5 * y / 4 + 4) % 7);

  // Day in October that DST ends  on, at 2 am
  byte dstOff = (31 - (5 * y / 4 + 1) % 7);

  if ((m > 3 && m < 10) ||
      (m == 3 && (d > dstOn || (d == dstOn && h >= 1))) ||
      (m == 10 && (d < dstOff || (d == dstOff && h <= 1))))
    return 1;
  else
    return 0;
}

time_t getLocalTime (void) {
  time_t t = RTC.get ();

  TimeElements tm;
  breakTime (t, tm);
  t += (dstOffset (tm.Day, tm.Month, tm.Year + 1970, tm.Hour)) * SECS_PER_HOUR;

  return t;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////INTERRUPT WIND////////////////////////////////////////////////////////
void windCount() {
  //if (Conteggio == 0){ Tempo =  millis();}//tempo iniziale se conteggio è 0 
  
  Tempo = millis();
  deltaT = float( Tempo - tempoPrec)/1000; // si conteggia il tempo trascorso fra un interrupt e un altro
  //Conteggio++;
  tempoPrec = Tempo;
   //Serial.println(Conteggio);
   //Serial.println(deltaT);
}

//INTENSITA' DELLA PIOGGIA//////////////////////////////////////////////////////////////////////////
void PluvioDataEngine() {
  if (((PluvioStep - PluvioOldStep) != 0)) {
    if ((millis() - PluvioStep) > (PluvioStep - PluvioOldStep)) {
      rainrate = 3600 / (((millis() - PluvioStep) / 1000)) * mmGoccia;
      if (rainrate < 1) {
        //gocce = 0;
        rainrate = 0;
      }
    } else {
      rainrate = 3600 / (((PluvioStep - PluvioOldStep) / 1000)) * mmGoccia;
    }
  } else {
    rainrate = 0.0;
  }

}

//ACCUMULO PIOGGIA//////////////////////////////////////////////////////////////////////////////////
void ContaGocce()
{// con pluviometro deAgostini ogni basculata attiva due volte l'interrupt quindi incremento di 0,05mm ogni interrupt
  PluvioOldStep = PluvioStep;
  PluvioStep = time1;
  mmPioggia += mmGoccia*0.5;
  time1 = millis(); 
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////INIZIALIZZAZIONI/////////////////////////////////////////////////////////////////////////////////////////
//SD
void initSD() {
  if (!SD.begin(SD_SS, SPI_HALF_SPEED)) 
    Serial.println(F("SD error"));    
  else{
    Serial.println(F("SD ok"));  
    File logFile = SD.open ("dataDay.csv", FILE_WRITE);
    if (logFile){///CREARE IL FILE csv SCOMMENTANDO LE RIGHE SOTTO PER AVERE I TITOLI DI OGNI COLONNA DOVE SI SCRIVERANNO I VALORI DEI SENSORI. CARICARE IL CODICE PRIMA SCOMMENTATO E POI COMMENTARE LE RIGHE COME SONO ADESSO E RICARICARE
      //String header = "Date Time Temperature Temperaturedht Humidity DewPoint HeatIndex WindSpeed(10') MaxWindSpeed(10') WindDir(10') Windchill Rain Rainrate(10') MaxRR(10')";
      //logFile.println(header);
      //logFile.close();
    }
  }
 radioTx.ersd = SD.begin(SD_SS, SPI_HALF_SPEED);//errore inizializzazione sd da inviare allo slave
}

//TEMPERATURA//
void initTemp () {
  sensors.begin();
  
  sht35.begin(0x44); //sensor i2c address
  Wire.setClock(100000);
  Serial.println(sht35.readStatus(), HEX);
  if ( sht35.isConnected() )
    Serial.println(F("sht35 ok"));
  else
    Serial.println(F("sht35 error"));
}

//UMIDITA'/////
void initHum () {
  dht.begin();
}

//VENTO/////////
void initWind () {
  attachInterrupt(0,windCount,FALLING);//interrupt wind speed
  pinMode(pinEst, INPUT);//set pin input segnale banderuola
  pinMode(pinSud, INPUT);
  pinMode(pinWest, INPUT);
  pinMode(pinNord, INPUT);
}

//PIOGGIA///////
void initPioggia () {
  attachInterrupt(1, ContaGocce, RISING); //interrupt per il pluviometro
}

//Radio
void initRadio() {
  if (!radio.begin()) //verifica corretta inizializzazione
    Serial.println(F("radio error"));
  else Serial.println(F("radio ok"));
  
  erradiotx = radio.begin(); //invio errore inizializzazione allo slave   
//nrf24
  radio.setPALevel(RF24_PA_MAX);          
  radio.setAddressWidth(5);                  
  radio.setDataRate(RF24_2MBPS);        
  radio.setCRCLength(RF24_CRC_16 );              
  radio.setAutoAck(true);
  //radio.openWritingPipe(pipes[0]);
  //radio.openReadingPipe(1,pipes[1]);
/////rf24network
  network.begin(/*channel*/ 90, /*node address*/ this_node);
}

void initVolt() {
  pinMode (Pin_Batteria, INPUT);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////FUNZIONI PER LOOP: ACQUISIZIONE////////////////////////////////////
//TEMPERATURA//////////
void readTemp () {
  sht35.read(false); //lettura in slow mode
  tp = sht35.getTemperature();
  radioTx.tp = int(tp*10);
  
  sensors.requestTemperatures(); //richiesta temp dal sensore dallas
  tp2 = sensors.getTempCByIndex(0); //temp dallas
  radioTx.tp2 = int(tp2*10); //invio radio temp dallas

  //moltiplico per 10 per rendere la variabile int e ridurre lo spazione in bit da inviare via radio
  
  tpdht = dht.readTemperature(); //temperatura dht22
  radioTx.tpdht = int(tpdht*10); //invio temp dht22 in radio  
}
//UMIDITA'//////////////////////////
void readHum () {
  ur = sht35.getHumidity();
  radioTx.hum = int(ur*10);
  
  ur2 = floor(dht.readHumidity()); //umiditá dht22
  radioTx.hum2 = byte(ur2); //invio radio umiditá
}

//VELOCITA' DEL VENTO E I SUOI ESTREMI//
void readWind () {

  if (deltaT != 0)                   //se é trascorso tempo calcolo la velocitá    
    wind.val = (2*Pi*raggio)/deltaT; //si calcola la velocitá come velocitá angolare diviso delta tempo

  else if (deltaT ==0 || deltaT > 2500){ //se non é trascorso tempo oppure velocitá minore 0.5 km/h
    wind.val = 0;
    //Conteggio = 0;
  }

  radioTx.windval = int(wind.val*10); //invio velocitá via radio

  if (wind.val > wind.raffica && wind.val < 150) {
    wind.raffica = wind.val;
    radioTx.raffica = int(wind.val*10);
  }

//MEDIA VELOCITÁ VENTO
  if ((millis() - tmediawind) <= intervalwind){  //finché sono dentro l'intervallo della media:
    sommaWind += wind.val;                 //aggiungo il valore attuale alla somma valori
    iwind++;                               //aumento counter dei valori aggiunti alla media
    wind.valmedio = sommaWind/iwind;       //calcolo la media ad ogni loop

    //valore massimo nell'intervallo media
    if ((wind.val > wind.max))
      wind.max = wind.val;
  }
  else if ((millis() - tmediawind) > intervalwind) {                   //se sono fuori dall'intervallo resetto tutto e inizio di nuovo al ciclo successivo
    iwind = 0;
    sommaWind = 0;
    tmediawind = millis();
  }

}

//DIREZIONE DEL VENTO
void readWindDir () {
  analogEst=analogRead(pinEst);// leggo i valori input della banderuola
  analogSud=analogRead(pinSud);
  analogWest=analogRead(pinWest);
  analogNord=analogRead(pinNord);

//CICLI IF per capire la posizione della banderuola a partire dai valori analogRead
//Si DEVE eseguire una TARATURA con strumenti diversi dalla banderuola DeAgostini

  if(analogEst>92 && analogEst<400 && analogNord>900){//NNE
    wind.dir= 0;   //indice byte che indica la posizione all'interno dell'array di stringe Direzioni[] per ottenere la direzione del vento
    mediaDir[0]++; //aumento di uno l'elemento dell'array mediaDir[] che contiene il numero di volte che la banderuola si trova in una posizione
                   // poco sotto si conta quale direzione é stata registrata il maggior numero di volte
  }
  else if(analogEst>=400 && analogEst<600 && analogNord<=900 && analogNord>600){ //NE
    wind.dir = 1;
    mediaDir[1]++;
  }
  else if(analogEst>=600 && analogNord<=900 && analogNord>100){ //ENE
    wind.dir = 2;
    mediaDir[2]++;
  }
  else if(analogEst>900 && analogNord<=100 && analogSud<=100 && analogWest<=100){  //E
    wind.dir = 3;
    mediaDir[3]++;
  }
  
  else if(analogSud>100 && analogEst>900){  //ESE
    wind.dir = 4;
    mediaDir[4]++;
  }
  else if(analogSud>=400 && analogSud<=600 && analogEst<=900 && analogEst>600){  //SE
    wind.dir= 5;
    mediaDir[5]++;
  }
  else if(analogSud>=600 && analogEst<=900 && analogEst>100){  //SSE
    wind.dir= 6;
    mediaDir[6]++;
  }
  else if(analogSud>850 && analogEst<=100 && analogWest<=100 && analogNord<=100){  //S
    wind.dir= 7;
    mediaDir[7]++;
  }
  
  else if(analogWest>100 && analogSud>800){  //SSO
    wind.dir= 8;
    mediaDir[8]++;
  }
  else if(analogWest>=523 && analogWest<600 && analogSud<=800 && analogSud>500){ //SO
    wind.dir= 9; 
    mediaDir[9]++;
  }
  else if(analogWest>=600 && analogSud<=800 && analogSud>100){ //OSO
    wind.dir= 10;
    mediaDir[10]++;
  }
  else if(analogWest>870 && analogEst<=100 && analogSud<=100 && analogNord<=80){ //O
    wind.dir= 11;
    mediaDir[11]++;
  }
  
  else if(analogNord>80 && analogWest>750){ //ONO
    wind.dir= 12;
    mediaDir[12]++;
  }
  else if(analogNord>=600 && analogNord<750 && analogWest<=750 && analogWest>500){  //NO
    wind.dir= 13;
    mediaDir[13]++;
  }
  else if(analogNord>=750 && analogWest<=800 && analogWest>100){ //NNO
    wind.dir= 14;
    mediaDir[14]++;
  }
  else if(analogNord>870 && analogEst<=92 && analogSud<=100 && analogWest<=100){ //N
    wind.dir= 15;
    mediaDir[15]++;
  }
  
//eventuali errori nel caso i fotodiodi delle 4 direzioni non funzionino e non diano il valore aspettato
  if (analogEst<=10) wind.err= 1; //errore Est
  else if (analogSud<=10) wind.err= 2; //errore Sus
  else if (analogWest<=10) wind.err= 3; //errore ovest
  else if (analogNord<=10) wind.err= 4; //errore nord
  else wind.err = 0; 
  
  radioTx.winddir = wind.dir;//invio direzione via radio in byte per risparmiare spazio
  //rappresenta la posizione all'interno dell'array di stringhe direzioni[] che ritorna una stringa con la sigla della direzione
  radioTx.erWinddir = wind.err;//invio errore via radio in byte
  //rappresenta la posizione all'interno dell'array di stringhe erdir[] che ritorna una stringa piú esplicativa dell'errore

//CALCOLO MODA della direzione wind negli ultimi 10'
if ((millis() - tmediaDir) <= intervaldir){
  for (int f=0; f<16; f++){
    if (mediaDir[f] > maxDir){//controllo quale valore direzione si é ripetuto di piú confrontando i valori dell'array mediaDir
      maxDir = mediaDir[f];
      wind.dirmedia = direzioni[f];//variabile direzione piú frequente in 10'
      radioTx.winddirMedio = f;// invio MODA direzione
    }
  }
}
else if ((millis()-tmediaDir) > intervaldir) {                        //fuori intervallo resetto tutto
  for (int f=0; f<16; f++){
    mediaDir[f] = 0;//azzero array della frequenza direzione del vento
  }
  tmediaDir = millis();//intervallo calcolo moda direzione vento
  maxDir = 0;//azzero indice di direzione che si ripete più volte nell'intervallo
}

}

//ACCUMULO PIOGGIA E INTENSITA' DELLA PIOGGIA///////////////
void readPioggia () {

  radioTx.pioggiaday = int(mmPioggia*10);//invio radio pioggia giornaliera
  
  if (mmPioggia != previousPioggia){
    previousPioggia = mmPioggia;
    EEPROM.put (eeAddress1, mmPioggia);
  }

  PluvioDataEngine();// conteggio rainrate
  radioTx.rainrate = int(rainrate*10);//invio rainrate via radio

//VALORE medio rainrate in intervallo
  if ((millis() - tmediaRR) <= intervalRR){
    sommaRR += rainrate;
    iRR++;
    mediaRR = sommaRR/iRR;
    
    //calcolo valore max in intervallo
    if (rainrate > RRmax)
      RRmax = rainrate;
  }
  else if ((millis() - tmediaRR) > intervalRR) { //reset dopo intervallo tempo
    iRR = 0;
    sommaRR = 0;
    tmediaRR = millis();
  }
}

//DEW POINT/////////////////
void readDewPoint () {
  float hu = dht.readHumidity();
  dewPoint = (pow (hu / 100, 0.125) * (112 + (0.9 * tp)) + 0.1 * tp - 112);//equazione per il calcolo del dewpoint
  
}

//WIND CHILL//////////
void readWindchill () {
  windchill = (13.12 + 0.6215 * tp) - (11.37 * pow(wind.val*3.6, 0.16)) + (0.3965 * tp * pow(wind.val*3.6, 0.16));//equazione per il calcolo del windchill

   if ((windchill < tp) && (wind.val*3.6 > 4.6) && (windchill > -127))   //correzione eventuali errori
  {
    windchill = (13.12 + 0.6215 * tp) - (11.37 * pow(wind.val*3.6, 0.16)) + (0.3965 * tp * pow(wind.val*3.6, 0.16));
  }
  else
  {
    windchill = tp;
  }
}

//HEAT INDEX/////////////////
void readHeatIndex() {
  float hu = dht.readHumidity();
  float fa = dht.readTemperature(false);
  float hic = dht.computeHeatIndex(tp, hu, false);//questo calcolo dell'indice di calore viene fornito dalla libreria 'dht'
  heatindexc = hic;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////ALTRE FUNZIONI///////////////////////////////////////////////////////
//SCRITTURA VALORI SUL DISPLAY//

void readDisplay() {
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor (ST7735_WHITE);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
    tft.print(F("T ")); //TEMPERATURA
    tft.print(tp, 1);
    tft.print(F("  "));
    tft.print(tpdht, 1);
    tft.print ((char)248);
    tft.println(F("C"));/*
    tft.setCursor(82,0);
    tft.print(F(" "));
    tft.setTextColor (ST7735_BLUE);
    tft.print(DayTemp.min, 1);
    tft.setTextColor (ST7735_WHITE);
    tft.print(F(" "));
    tft.setTextColor (ST7735_RED);
    tft.print(DayTemp.max, 1);
    tft.setTextColor (ST7735_WHITE);
    tft.print(F(" "));
    tft.setTextColor (ST7735_YELLOW);
    tft.println(dertp);*/
tft.drawLine(0,8,159,8,0x5d5348); //UMIDITÁ
tft.setCursor(0,11);    
tft.setTextColor (ST7735_WHITE);    
tft.print(F("U"));
tft.setCursor(17,11);
tft.print(ur);
tft.println(F("%"));/*
tft.setCursor(82,11);
tft.print(F(" "));
tft.setTextColor (ST7735_BLUE);
tft.print(DayHum.min, 0);
tft.print(F("%"));
tft.setTextColor (ST7735_WHITE);
tft.print(F(" "));
tft.setTextColor (ST7735_RED);
tft.print(DayHum.max, 0);
tft.print(F("%"));
tft.setTextColor (ST7735_WHITE);
tft.print(F(" "));
tft.setTextColor (ST7735_YELLOW);
tft.println(F("="));*/
    tft.drawLine(0,19,159,19,0x5d5348); //DEW POINT
    tft.setCursor(0,22); 
    tft.setTextColor (ST7735_WHITE);
    tft.print(F("DP"));
    tft.setCursor(17,22);
    tft.print(dewPoint, 1); 
    tft.print((char)248);
    tft.println(F("C"));/*
    tft.setCursor(82,22);
    tft.print(F(" "));
    tft.setTextColor (ST7735_BLUE);
    tft.print(DayDewPoint.min, 1);
    tft.setTextColor (ST7735_WHITE);
    tft.print(F(" "));
    tft.setTextColor (ST7735_RED);
    tft.print(DayDewPoint.max, 1);
    tft.setTextColor (ST7735_WHITE);
    tft.print(F(" "));
    tft.setTextColor (ST7735_YELLOW);
    tft.println(F("="));*/
tft.drawLine(0,30,159,30,0x5d5348); //HEAT INDEX
tft.setCursor(0,33);     
tft.setTextColor (ST7735_WHITE);    
tft.print(F("HI"));
tft.setCursor(17,33);
tft.print(heatindexc, 1); 
tft.print((char)248);
tft.println(F("C"));/*
tft.setCursor(82,33);
tft.print(F(" "));
tft.setTextColor (ST7735_RED);
tft.print(DayHeatIndex.max, 1);
tft.print((char)248);
tft.print(F("C"));
tft.setTextColor (ST7735_WHITE);
tft.print(F(" "));
tft.setTextColor (ST7735_YELLOW);
tft.println(F("="));*/
    tft.drawLine(0,41,159,41,0x5d5348); //PRESSIONE LIVELLO MARE
    tft.setCursor(0,44);
    tft.setTextColor (ST7735_WHITE);
    tft.print(F("P "));
    tft.setCursor(17,44);
    tft.print("nn"); 
    tft.print(F("hPa"));
    tft.setCursor(74,44);
    tft.print(F(" "));
    tft.setTextColor (ST7735_YELLOW);
    tft.print(F("nn"));
    tft.setTextColor (ST7735_WHITE);
    tft.print(F(" "));
    tft.setTextColor (ST7735_GREEN);
    tft.println(F("nn"));
  tft.setCursor(17,52);                //PRESSIONE
  tft.setTextColor (ST7735_WHITE);
  tft.print("nn");
  tft.print(F("hPa"));
  tft.setCursor(74,52);
  tft.print(F(" "));
  tft.setTextColor (ST7735_YELLOW);
  tft.print(F("nn"));
  tft.setTextColor (ST7735_WHITE);
  tft.print(F(" "));
  tft.setTextColor (ST7735_GREEN);
  tft.println(F("nn"));
tft.drawLine(0,61,159,61,0x5d5348); //VELOCITÁ VENTO
tft.setCursor(0,64);    
tft.setTextColor (ST7735_WHITE);    
tft.print(F("W"));
tft.setCursor(17,64);
tft.print(wind.val*3.6, 1); 
tft.print(F("Km/h"));
tft.setCursor(78,64);
tft.print(F("G "));
tft.setTextColor (ST7735_RED);
tft.print(wind.max*3.6, 1);
tft.setTextColor (ST7735_WHITE);
tft.print(F(" A "));
tft.setTextColor (ST7735_GREEN);
tft.println(wind.valmedio*3.6, 1);
    tft.drawLine(0,72,159,72,0x5d5348); //DIREZIONE VENTO
    tft.setCursor(0,75);
    tft.setTextColor (ST7735_WHITE);
    tft.print(F("Dir "));
    tft.print(wind.dir);
    tft.setCursor(47,75);
    tft.print(F("A"));
    tft.setTextColor (ST7735_GREEN);
    tft.println(wind.dirmedia);/*
    tft.drawLine(76,75,76,83,ST7735_CYAN);
    tft.setCursor(78,75);
    tft.setTextColor (ST7735_WHITE);
    tft.print("WC ");
    tft.print(windchill, 1);
    tft.print(" ");
    tft.setTextColor (ST7735_BLUE);
    tft.print(DayChill.min, 1);*/
tft.drawLine(0,83,159,83,0x5d5348); //PIOGGIA
tft.setCursor(0,86);    
tft.setTextColor (ST7735_BLUE);    
tft.println(F("Day"));/*
tft.setTextColor (ST7735_WHITE);
tft.setCursor(45,86);
tft.print(F("Month"));
tft.setTextColor (ST7735_BLUE); 
tft.setCursor(100,86);
tft.println(F("Year"));*/
    tft.print(mmPioggia, 1);
    tft.println(F("mm"));/*
    tft.setTextColor (ST7735_WHITE);
    tft.setCursor(45,94);
    tft.print(mmPioggiamese, 1);
    tft.print(F("mm"));
    tft.setCursor(100,94);
    tft.setTextColor (ST7735_BLUE); 
    tft.print(mmPioggiaanno, 1);
    tft.println(F("mm"));*/
tft.drawLine(0,104,159,104,0x5d5348); //RAINRATE
tft.setCursor(0,107);    
tft.setTextColor (ST7735_WHITE);    
tft.print(F("RR "));
tft.print(rainrate, 1); 
tft.print(F("mm/h"));
tft.setCursor(77,107);
tft.print(F("M"));
tft.setTextColor (ST7735_RED);
tft.print(RRmax, 1);
tft.setTextColor (ST7735_WHITE);
tft.print(F(" A"));
tft.setTextColor (ST7735_GREEN);
tft.println(mediaRR, 1);
    tft.drawLine(0,115,159,115,0x5d5348); //ERRORI E INFO DI STATO
    tft.setCursor(0,118);
    tft.setTextColor (ST7735_YELLOW);
    tft.print(lastDatasd);
    tft.print(" ");
    tft.setTextColor (ST7735_WHITE);
    tft.print(erradiotx);
    tft.print(" ");
    tft.setTextColor (ST7735_YELLOW);
    tft.print(radioTx.ersd);
    tft.print(" ");
    tft.setTextColor (ST7735_WHITE);
    tft.print(errSent);
    tft.print(" ");
    tft.setTextColor (ST7735_YELLOW);
    tft.print(erdir[wind.err]);
    tft.setTextColor (ST7735_WHITE);
    tft.print("|");
    tft.setTextColor (ST7735_GREEN);
    tft.print(lastRadiotx);
}

//TEMPORIZZAZIONE SALVATAGGI NELL'EEPROM DEI VALORI SOGGETTI A SALVATAGGI FREQUENTI//
void Datalog() {
  
//TEMPORIZZAZIONE SALVATAGGIO VALORI NEL FILE 'dataDay.csv'//
radioTx.ersd  = SD.begin(SD_SS, SPI_HALF_SPEED); 
  if (millis() - previousMillis >= interval) {
    if (!SD.begin(SD_SS, SPI_HALF_SPEED)) {
      Serial.println("SD error");    
    }
    else {
      Serial.println("SD ok");
      
      previousMillis = millis();
  
  /*    String dataString = String(year()) + "/" + String(month()) + "/" + String(day()) + " " + String(hour()) + ":" + String(minute()) + " " + String(tp, 1) + " " + String(tpdht, 1) + " " +
                          String(ur) + " " + String(dewPoint, 1) + " " + String(heatindexc, 1) + " " + String(wind.valmedio*3.6, 1) + " " + String(wind.max*3.6, 1) + " " + wind.dirmedia + " " + String(windchill, 1)
                          + " " + String(mmPioggia, 1) + " " + String(rainrate, 1) + " " + String(RRmax, 1);
  
      File logFile = SD.open("dataDay.csv", FILE_WRITE);
      if (logFile){
        logFile.println(dataString);
        logFile.close();*/
      File logFile = SD.open("dataDay.csv", FILE_WRITE);
      if (logFile){
        float dataday[] = {tp, tpdht, ur, dewPoint, heatindexc, wind.valmedio*3.6, wind.max*3.6, gradi[radioTx.winddirMedio], windchill, mmPioggia, rainrate, RRmax};
        logFile.print(day());
        logFile.print("/");
        logFile.print(month());
        logFile.print("/");
        logFile.print(year());
        logFile.print(" ");
        logFile.print(hour());
        logFile.print(":");
        logFile.print(minute());
        for (int i=0; i<9; i++) {
          logFile.print(dataday[i],1);
          logFile.print(" ");
        }
        logFile.println(dataday[9], 1);
      }
      logFile.close();  
      Serial.println(F("written"));
      radioTx.lastDatalog = long(now()); 
      lastDatasd = String(hour()) + ":" + String(minute());
    }
    //reset medie temporanee
    wind.max = 0;
    RRmax = 0;
    maxDir = 0;
    for (int f=0; f<16; f++){
      mediaDir[f] = 0;//azzero array della frequenza direzione del vento
    }
  }
}


////////////////////Reset temporizzazioni alla mezzanotte per sincronizzare datalog ogni 10 min esatti
void resetTempo() {
  if (giornoSalvato != day()) {
    giornoSalvato = day();
    mmPioggia = 0;
    previousPioggia = 0;
    EEPROM.put(eeAddress1, 0);
    wind.raffica = 0; //reset raffica giornaliera
    previousMillis = 0; //sincronizzo memorizzazione datalog ogni 10 min
  //sincronizzo media velocitá vento
    tmediawind = millis(); 
    iwind = 0;
    sommaWind = 0;
    wind.max = 0; //reset raffica massima
  //sincronizzo media direzione vento
    tmediaDir = millis(); 
    for (int f=0; f<16; f++)
      mediaDir[f] = 0;//azzero array della frequenza direzione del vento
    maxDir = 0;//azzero indice di direzione che si ripete più volte nell'intervallo
  //sincronizzo media rainrate
    tmediaRR = millis(); 
    iRR = 0;
    sommaRR = 0;
    RRmax = 0; //reset intensità della pioggia massima
  }
}

//////////////////////////////////////////////CALCOLO TENSIONE BATTERIA
void readVolt() {
  ValPinBatteria = analogRead(Pin_Batteria);
  Vbatteria = ValPinBatteria/31; //  1/30 = (5.0 / 1023.0) * ((R1 + R2) / R2)
  radioTx.volt = byte(Vbatteria*10);   
}

void(* resetFunc) (void) = 0; 

void resetArduino(){
  if(resetRx)
    resetFunc();
}

void statusReset(){
  radioTx.reset = resetStatus;
  if(millis() - startResetstatus > 180000){
    resetStatus = false;
    startResetstatus = millis();
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin (9600);
  Wire.begin();

  pinMode(12, INPUT);
  pinMode(14, OUTPUT); //pin led lampeggio ad ogni loop
  digitalWrite(14, LOW);
  pinMode(15, OUTPUT); //pin led stato invio dati radio
  digitalWrite(15, LOW);

  setSyncProvider(getLocalTime); // si prende l'ora attuale dell'RTC
  giornoSalvato = day(); //si prende giorno mese anno attuali per vedere se resettare estremi

  wind.max = 0; //reset raffica massima nell'intervallo
  wind.raffica = 0; //reset raffica massima giornaliera

  Eeprom();
  //initSD();
  initRadio();
  initTemp ();
  initHum ();
  initWind();
  initPioggia();
  initVolt();

/////////////////////imposto ora attuale
/*
 if (!rtc.begin()) {
  Serial.println("verifica connesione rtc");
 }
 if (rtc.lostPower()) 
 //rtc.adjust(DateTime(F(__DATE__),F(__TIME__)));
  rtc.adjust(DateTime(2021,12,06,21,29,00));
  */
}

void loop() {
  readTemp ();
  readHum ();
  readWind ();
  readWindDir();
  readPioggia();
  readDewPoint();
  readWindchill();
  readHeatIndex();
  //Datalog();
  readVolt();
  resetTempo();
  statusReset();
  //readDisplay(); 

  //radioTx.ersd  = SD.begin(SD_SS, SPI_HALF_SPEED);//invio radio errore sd

  /*if (!SD.begin(SD_SS, SPI_HALF_SPEED)) //check sd
    Serial.println("SD error");    
    else 
    Serial.println("SD ok");
*/
  radioTx.data = long(now());//invio data attuale allo slave
  
/////////RECEIVING DATA
  network.update();        // Check the network regularly

  while (network.available()){
    RF24NetworkHeader header;        // If so, grab it and print it out
    network.read(header, &resetRx, sizeof(resetRx));
    resetArduino();
    Serial.println(F("Reset status received"));
  }
  
////////SENDING
  Serial.print("Sending...");
  RF24NetworkHeader header2( other_node);
  bool ok = network.write(header2, &radioTx, sizeof(radioTx));
    if (ok){
      Serial.println("ok sending");
      errSent = true;
      digitalWrite(15, HIGH);
      delay(50);
      digitalWrite(15, LOW);
    }
    else{
      Serial.println("fail sending");
      errSent = false;
      digitalWrite(15, HIGH);
      delay(100);
      digitalWrite(15, LOW);
      delay(200);
      digitalWrite(15, HIGH);
      delay(100);
      digitalWrite(15, LOW);
    }
    
/*
  radio.stopListening();
  bool ok = radio.write( &radioTx, sizeof(radioTx) );//invio struct con dati allo slave
  
  bool rx;//variabile per controllo corretta risposta radio
  radio.startListening();
  unsigned long started_waiting_at = millis();//se dopo 5 secondi no risposta segnalo errore comunicazione
  bool timeout = false;
  while ( !radio.available() && ! timeout )
    if (millis() - started_waiting_at > 5000 )
      timeout = true;

  if ( timeout ){ //controllo risposta da slave
    Serial.println("Failed, response timed out.");
    errSent = false;
  }
  else{
    radio.read( &rx, sizeof(rx) );
    lastRadiotx = String(hour()) + ":" + String(minute()) + ":" + String(second()) + "|" + String(day()) +"/"+ String(month());//stringa indica time ultimo invio dati radio sul display
    }*/
///////////////////////////////////////////////////////////////////
Serial.print(hour());
Serial.print(":");
Serial.print(minute());
Serial.print(":");
Serial.println(second());
Serial.print(day());
Serial.print("/");
Serial.print(month());
Serial.print("/");
Serial.println(year());
Serial.print(tp);
Serial.println(" C");
Serial.println(tpdht);
Serial.println(" C  dht");
Serial.print(ur);
Serial.println(" %");
Serial.print(direzioni[wind.dir]);
Serial.print(" media 10' ");
Serial.println(direzioni[radioTx.winddirMedio]);
Serial.print(wind.val);
Serial.println(" m/s");
Serial.println(wind.raffica*3.6);
Serial.print(mmPioggia);
Serial.println(" mm");
Serial.print("Tens. batteria:  ");
Serial.println(Vbatteria);
Serial.println("_______");//per eventuali prove in Serial port

//lampeggio led al termine del loop
digitalWrite(14, HIGH);
delay(100);
digitalWrite(14, LOW);
}
