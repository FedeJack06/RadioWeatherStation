/*//////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////                                                                     ///////////////
  //////////////////////////          Weather station by Fedejack - SLAVE 1.3                    //////////////
  //////////////////////////                                                                     ///////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////
*/
//////////////////////////////////////LIBRERIE E DEFINIZIONI////////////////////////////////////////////////////
#include <SPI.h>
#include <Wire.h>
#include <TimeLib.h>
#include <Webbino.h>
#include <PString.h>
char Buffer[30];
PString suBuffer (Buffer,30);

////////////////////////////////////////PRIMA DI INIZIARE
bool INIZIALIZZAZIONE = 0;   // mettere 1 al posto di 0 se é la prima volta che si esegue il programma oppure se si é resettato tutto, avvia i processi che devono essere eseguiti una volta
float CALIBRATION = 3070; //3/6/21 da meteomin.it 3265 taratura per calcolo pressione sul livello del mare, differenza (in Pa) fra pressione slm attuale e pressione letta dal bmp180

//per calcolo dewpoint e heatindex
#include "DHT.h"
#define DHTTYPE DHT22
DHT dht(1, DHTTYPE);
/*
//sensore ds18b20 interno
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 18 // pin digitale a cui è collegato il DS18B20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
*/
//Radio
#include <RF24Network.h>
#include <RF24.h>
RF24 radio(41,39);
//const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL }; se non si usa rf24network
RF24Network network(radio);      // Network uses that radio
const uint16_t this_node = 01;    // Address of our node in Octal format ( 04,031, etc)
const uint16_t other_node = 00;   // Address of the other node in Octal format

//PRESSURE BMP180
#include <Adafruit_BMP085.h>
Adafruit_BMP085 barometer;

//SD
#include <SdFat.h>
#define SD_SS 4 // pin usato dalla microSD
SdFat SD;

//DISPLAY TFT 1.8" 
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#define sclk ICSP-3 // 13 se si usa Arduino UNO
#define mosi ICSP-4 // 11 se si usa Arduino UNO
#define rst  5
#define cs   6
#define dc   7
Adafruit_ST7735 tft = Adafruit_ST7735(cs, dc, rst);

//WEBBINO
WebServer webserver;
#include <WebbinoInterfaces/WIZ5x00.h>
NetworkInterfaceWIZ5x00 netint;

//Configurazione Network
#define IP_ADDRESS 192,168,1,124 // l'ip usato dalla scheda ethernet 
//#define DNS_ADDRESS 8,8,8,8
#define GW_ADDRESS 192,168,1,1 // l'ip del vostro modem
#define NETMASK  255,255,255,0 // la netmask della vostra rete

/////////////////////////////////////////////////////////////////////////////////////////

#define REP_BUFFER_LEN 32
char replaceBuffer[REP_BUFFER_LEN];
PString subBuffer (replaceBuffer, REP_BUFFER_LEN);

//////////////DEFINIZIONI TAG D'ESEMPIO CHE POSSONO ESSERE UTILI/////////////////////////
//void *data __attribute__ ((unused))
PString& evaluate_webbino_version (void *data __attribute__ ((unused))) {
  subBuffer.print (WEBBINO_VERSION);
  Serial.println(replaceBuffer);
  return subBuffer;
}

PString& evaluate_uptime (void *data __attribute__ ((unused))) {
  unsigned long uptime = millis () / 1000;
  byte d, h, m, s;

  d = uptime / 86400;
  uptime %= 86400;
  h = uptime / 3600;
  uptime %= 3600;
  m = uptime / 60;
  uptime %= 60;
  s = uptime;

  if (d > 0) {
    subBuffer.print (d);
    subBuffer.print (d == 1 ? F(" day, ") : F(" days, "));
  }

  if (h < 10)
    subBuffer.print ('0');
  subBuffer.print (h);
  subBuffer.print (':');
  if (m < 10)
    subBuffer.print ('0');
  subBuffer.print (m);
  subBuffer.print (':');
  if (s < 10)
    subBuffer.print ('0');
  subBuffer.print (s);

  return subBuffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////VARIABILI///////////////////////////////////////////////////////

///////////////////////////////////TEMPERATURA//////////////////////
float tp; //sht35
float tpdht; //dht22
float tp2; //ds18b20
float tpbmp; //bmp180
float tpint; //temperatura interna
float windchill;
float heatindexc;

//media giornaliera
float sommaTp=0;
unsigned int numberTp=0;
float mediaTp;//MEDIA GIORNALIERA

//andamento
int tp10min=0, tp5min=0, tpnow=0;
unsigned long intAndamentoTp=0;
String andamentoTp;// = < >
float tpChange;//ANDAMENTO

//medie in intervallo
unsigned long intmediatp = 0; //temporizzazione media 5' delle temperature
unsigned int mTp, mTpdht, mTp2, count;//somma medie delle temperature e contatore
float TP, TPDHT, TP2;//medie delle temperature

///////////////////////////////////UMIDITÁ//////////////////////////
float ur; // variabile dell'umidità
byte ur2;
//andamento
byte ur10min=0, ur5min=0, urnow=0;
unsigned long intAndamentoUr=0;
String andamentoUr;// = < >
int urChange;

//medie in intervallo
unsigned long intmediaur = 0; //temporizzazione media 5' delle temperature
unsigned int mUr, countUr;//somma medie delle temperature e contatore
float UR;//medie delle temperature

//////////////////////////////////DEW POINT/////////////////////////
float dewPoint; // variabile del punto di rugiada
//andamento
int dp10min=0, dp5min=0, dpnow=0;
unsigned long intAndamentoDp=0;
String andamentoDp;
int dpChange;

////////////////////////////////PRESSIONE///////////////////////////
float pressionelivellodelmare;// variabile per la pressione slm in Pa
float pressione;// pressione bmp180
//andamento
unsigned long intPressure = 0; //intervallo per salvataggio orario della pressione
float previousThree, previousSix; //differenza pressione precedente 3 6 ore
float pNow, pOne, pTwo, pThree, pFour, pFive, pSix; //pressioni precenti di 0 fino a 6 ore fa 
//medie in intervallo
unsigned long intmediapr = 0, mPr, mSlpr; //temporizzazione media 5' delle temperature e somma medie
unsigned int countPr;//contatore
float PR, SLPR;//medie

////////////////////////////////PIOGGIA/////////////////////////////
float mmPioggia = 0.0; //conteggio mm pioggia in corso
float mmPioggiagiorno; // mm pioggia cumulata nel giorno
float mmPioggiamese; // conteggio mm pioggia mese
float mmPioggiaanno; // conteggio mm pioggia anno
float prevPioggia; // variabile per incremetare i conteggi precedenti
float rainrate = 0; // variabile per l'intensità della pioggia

//media rainrate
float mediaRR;
unsigned long tmediaRR = 0; //temporizzazione per calcolo media
float sommaRR = 0; //somma rainrate misurati nell'intervallo
int iRR = 0; //contatore dei valori sommati
float rainrateMax = 0; //rainrate massimo nell'intervallo media

////////////////////////////////VARIABILI ANEMOMETRO E DIREZIONE///////
struct WindValue { 
  float val; //velocità vento m/s
  float valmedio; //velocità media vento
  float raffica; //raffica massima giornaliera rilevata dal master
  float max; //velocitá massima nell'intervallo della media
  String dir; //direzione del vento
  int ang; //direzione del vento in gradi
  int angmedio;
  String dirmedia; //direzione vento media 10'
  String err; //errore direzione vento
};
WindValue wind;

//media velocitá vento
unsigned long tmediawind = 0;//temporizzazione per calcolo media velocità vento
float sommaWind = 0;//somma delle velocità per la media
int iwind = 0;//conteggio valori velocità da mediare

//direzione
String direzioni[] = {"NNE","NE","ENE","E","ESE","SE","SSE","S","SSO","SO","OSO","O","ONO","NO","NNO","N"};
int gradi[] = {22,45,67,90,112,135,157,180,202,225,247,270,292,315,337,0};
String erdir[] = {"1","eE","eS","eO","eN"};
//media
const long intervaldir = 300000;//5' intervallo in cui ci calcola media direzione vento
int mediaDir[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};//array della moda direzione del vento
unsigned long tmediaDir = 0;//temporizzazione per calcolo moda direzione vento
int maxDir = 0;//indice di direzione che si ripete più volte nell'intervallo


////////////////////////TEMPORIZZAZIONI/////////////////////////////////////////
const long interval = 60000;//TEMPORIZZAZIONE MEDIE e DATALOG
unsigned long previousMillis = 0; //temporizzazione salvataggio dataDay.csv
byte giornoSalvato; // variabile per memorizzare lo stato del giorno attuale
byte meseSalvato; // variabile per memorizzare lo stato del mese attuale
unsigned int annoSalvato; // variabile per memorizzare lo stato dell'anno attuale
bool onetime = true; //variabile per inizializzare giorni/mesi/anni salvati

const long intervalWeb = 300000; //ogni 5 min report per meteonetwork
unsigned long previousWeb = 0;

/////////////CONTROLLI ED ERRORI/////////////////////////////////////////////////
String lastRadioRx; //ultima data ricezione dati radio
tmElements_t d;
String lastDatalog; //ultimo salvataggio sd in master
tmElements_t s;
String wsDate, wsTime;//element ora e data
tmElements_t dd;

String lastRadioRX; //ultima data invio dati radio
bool erbmp=false; //errore init bmp180 SLAVE
bool erradioRX=false; //errore init radio SLAVE
bool erroreSD=false; //errore sd SLAVE
bool radioLink=false; //errore mancato collegamento radio con master
bool resetTx = false;
bool writeExtreme = false;
bool readEstremiOk = false;

///////STRUCT PER L'ORARIO DI RILEVAZIONE DEGLI ESTREMI GIORNALIERI E PER I VALORI DI ESSI(ANCHE MENSILI E ANNUI)/////////////
struct TimedValue {
  float max;
  float min;
  time_t timemax;
  time_t timemin;
};
//VALORI GIORNALIERI/////////
TimedValue DayTemp;
TimedValue DayHum;
TimedValue DayDewPoint;
TimedValue DayPressure;
TimedValue DayPressureSlm;
TimedValue DayWind;
TimedValue DayChill; 
TimedValue DayHeatIndex; 
TimedValue DayRainRate; 
//VALORI MENSILI////////////
TimedValue MonthTemp;
TimedValue MonthHum;
TimedValue MonthDewPoint;
TimedValue MonthPressure;
TimedValue MonthPressureSlm;
TimedValue MonthWind;
TimedValue MonthChill;
TimedValue MonthHeatIndex;
TimedValue MonthRainRate;
//VALORI ANNUI//////////////
TimedValue YearTemp;
TimedValue YearHum;
TimedValue YearDewPoint;
TimedValue YearPressure;
TimedValue YearPressureSlm;
TimedValue YearWind;
TimedValue YearChill;
TimedValue YearHeatIndex;
TimedValue YearRainRate;

/////STRUCT RADIO////////////////////////////////////////////////////////////////////////////
struct radioVal {
  long data;//trasferimento data allo slave per salvataggio dati
  int tp;
  int tpdht;
  int hum;
  byte hum2;
  int pioggiaday;
  int rainrate;
  byte winddir;
  byte winddirMedio;
  int windval;
  int raffica; //raffica giornaliera massima
  int tp2;

  byte erWinddir; //errore direzione vento
  bool ersd; //errore inizializzazione sd master
  long lastDatalog; //ultima ora salvataggio datalog Day
  byte volt;
  bool reset;
};
radioVal radioRx;   // 30/32 byte occuped

//////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////PRESSIONE E MEDIA PER EVITARE INTERFERENZE//////////////////////////////////////////////
#define LETTURE 30
#define SCARTO 60

#define MAX(A,B) ((A>B)?A:B)
#define MIN(A,B) ((A<B)?A:B)

static uint32_t current = 0;

uint8_t errata(uint32_t pressione, uint32_t attesa)
{
  uint32_t absolute = MAX(pressione, attesa) - MIN(pressione, attesa);
  return absolute > SCARTO ? 1 : 0;
}

uint32_t GETPressure()
{
  uint32_t pressioni[LETTURE];
  uint32_t totale;
  uint32_t media;
  uint8_t k;
  uint8_t i;

  for (i = 0, totale = 0; i < LETTURE; ++i)
  {
    pressioni[i] = barometer.readPressure();
    totale += pressioni[i];
  }

  media = totale / LETTURE;

  for (i = 0, k = 0; i < LETTURE; ++i)
  {
    if ( errata(pressioni[i], media) )
    {
      totale -= pressioni[i];
      ++k;
    }
  }

  return totale / (LETTURE - k);
}

//////////////INIZIALIZZAZIONI/////////////////////////////////////////////////////
//SD
void initSD() {
  erroreSD  = SD.begin(SD_SS, SPI_HALF_SPEED); 

  if (!erroreSD){ 
    //Serial.println(F("SD error"));
    tft.setCursor(60,107);
    tft.setTextSize(1);
    tft.setTextColor (ST7735_RED);
    tft.println(F("NO SD CARD"));
    tft.println(F("insert SD to initialize"));
    while(2);
  }
  else{
    //Serial.println(F("SD ok"));  
    File logFile = SD.open ("dataDay.csv", FILE_WRITE);
    if (logFile){///CREARE IL FILE csv SCOMMENTANDO LE RIGHE SOTTO PER AVERE I TITOLI DI OGNI COLONNA DOVE SI SCRIVERANNO I VALORI DEI SENSORI. CARICARE IL CODICE PRIMA SCOMMENTATO E POI COMMENTARE LE RIGHE COME SONO ADESSO E RICARICARE
      //String header = "Date Time Temperature Temperaturedht Humidity DewPoint HeatIndex Pressure(slm) AverageSpeed(5') MaxWindSpeed(5') WindDir(5') Windchill Rain AverageRRate(5') MaxRR(5')";
      //logFile.println(header);
      //logFile.close();
    }

    File logFile2 = SD.open ("extDay.csv", FILE_WRITE);
    if (logFile2){///CREARE IL FILE csv SCOMMENTANDO LE RIGHE SOTTO PER AVERE I TITOLI DI OGNI COLONNA DOVE SI SCRIVERANNO I VALORI DEI SENSORI. CARICARE IL CODICE PRIMA SCOMMENTATO E POI COMMENTARE LE RIGHE COME SONO ADESSO E RICARICARE
      //String header = "Day MaxTemp timeMaxT MinTemp timeMinT MaxHum timeMaxH MinHum timeMinH MaxDP timeMaxDP MinDP timeMinDP MaxPress timeMaxP MinPress timeMinP MaxWSpeed timeMaxW MinWC timeMinWC MaxRR timeMaxRR mediaTp mmPioggia";
      //logFile2.println(header);
      //logFile2.close();
    }
  }
}

//LETTURA FILE ESTREMI
void readEstremi () {
  if (!SD.begin(SD_SS, SPI_HALF_SPEED))
    erroreSD = false;
    //Serial.println(F("SD error"));    
  else{
    ifstream estremiIn("estremi.dat");
    //if (!estremiIn.is_open()) Serial.println(F("errore apertura estremi.csv"));

     //if (estremiIn.fail()) 
       //Serial.println (F("bad input estremi"));

      estremiIn   >> DayTemp.max >> DayTemp.timemax >> DayTemp.min >> DayTemp.timemin
                  >> DayHum.max >> DayHum.timemax >> DayHum.min >> DayHum.timemin
                  >> DayPressure.max >> DayPressure.timemax >> DayPressure.min >> DayPressure.timemin
                  >> DayPressureSlm.max >> DayPressureSlm.timemax >> DayPressureSlm.min >> DayPressureSlm.timemin
                  >> DayDewPoint.max >> DayDewPoint.timemax >> DayDewPoint.min >> DayDewPoint.timemin
                  >> DayWind.max >> DayWind.timemax
                  >> DayChill.min >> DayChill.timemin
                  >> DayHeatIndex.max >> DayHeatIndex.timemax
                  >> DayRainRate.max >> DayRainRate.timemax
                  
                  >> MonthTemp.max >> MonthTemp.timemax >> MonthTemp.min >> MonthTemp.timemin
                  >> MonthHum.max >> MonthHum.timemax >> MonthHum.min >> MonthHum.timemin
                  >> MonthPressure.max >> MonthPressure.timemax >> MonthPressure.min >> MonthPressure.timemin
                  >> MonthPressureSlm.max >> MonthPressureSlm.timemax >> MonthPressureSlm.min >> MonthPressureSlm.timemin
                  >> MonthDewPoint.max >> MonthDewPoint.timemax >> MonthDewPoint.min >> MonthDewPoint.timemin
                  >> MonthWind.max >> MonthWind.timemax
                  >> MonthChill.min >> MonthChill.timemin
                  >> MonthHeatIndex.max >> MonthHeatIndex.timemax
                  >> MonthRainRate.max >> MonthRainRate.timemax

                  >> YearTemp.max >> YearTemp.timemax >> YearTemp.min >> YearTemp.timemin
                  >> YearHum.max >> YearHum.timemax >> YearHum.min >> YearHum.timemin
                  >> YearPressure.max >> YearPressure.timemax >> YearPressure.min >> YearPressure.timemin
                  >> YearPressureSlm.max >> YearPressureSlm.timemax >> YearPressureSlm.min >> YearPressureSlm.timemin
                  >> YearDewPoint.max >> YearDewPoint.timemax >> YearDewPoint.min >> YearDewPoint.timemin
                  >> YearWind.max >> YearWind.timemax
                  >> YearChill.min >> YearChill.timemin
                  >> YearHeatIndex.max >> YearHeatIndex.timemax
                  >> YearRainRate.max >> YearRainRate.timemax
                  >> pOne >> pTwo >> pThree >> pFour >> pFive >> pSix
                  >> mmPioggia >> mmPioggiagiorno >> mmPioggiamese >> mmPioggiaanno
                  >> sommaTp >> numberTp;

      estremiIn.skipWhite();
      readEstremiOk = true;
      
     /// if (estremiIn.fail())
      //  Serial.println(F("bad END input estremi"));
        
  }
}

//PRESSIONE////
void initPressure () {
/*if (!barometer.begin()) 
  Serial.println("BMP180 error");
else Serial.println("BMP180 ok");*/
erbmp = barometer.begin();
  
  current = GETPressure();
  current += GETPressure();
  current += GETPressure();
  current += GETPressure();
  current >>= 2;
}

//RADIO
void initRadio() {
  /*if (!radio.begin())
    Serial.println(F("radio error"));
  else Serial.println("radio ok");*/
  erradioRX = radio.begin();
////nrf24
  radio.setPALevel(RF24_PA_MAX);
  radio.setAddressWidth(5);                  
  radio.setDataRate(RF24_2MBPS);        
  radio.setCRCLength(RF24_CRC_16 );              
  radio.setAutoAck(true);
  //radio.openWritingPipe(pipes[1]);
  //radio.openReadingPipe(1,pipes[0]);
  //radio.startListening();
  ////////rf24network
  network.begin(/*channel*/ 90, /*node address*/ this_node);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////FUNZIONI PER LOOP: ACQUISIZIONE////////////////////////////////////
//TEMPERATURA E I SUOI ESTREMI//////////
void readTemp () {

//DATI ISTANTANEI DAL MASTER
  tp = float(radioRx.tp)/10;
  tpdht = float(radioRx.tpdht)/10;
  tpbmp = barometer.readTemperature();
  tp2 = float(radioRx.tp2)/10;
  
  //sensors.requestTemperatures(); //richiesta temp dal sensore dallas interno
  //tpint = sensors.getTempCByIndex(0); //temp dallas
  
//MEDIA
  if(millis() - intmediatp < interval){
    mTp += int(tp*10);
    mTpdht += int(tpdht*10);
    mTp2 += int(tp2*10);
    count++;
    TP = float(mTp/count)/10;
    TPDHT = float(mTpdht/count)/10;
    TP2 = float(mTp2/count)/10;
  } else {
    mTp  = 0;
    mTpdht  = 0;
    mTp2  = 0;
    count = 0;
    intmediatp = millis();
  }
  
//ANDAMENTO 5' e 10'
  if (millis() - intAndamentoTp > 300000) {
    tp10min = tp5min;
    tp5min = tpnow;
    tpnow = int(tp*10);
    intAndamentoTp = millis();
  }

  if (abs(tp*10 - tp10min) > 50)
    andamentoTp = " ";
  else if (tp*10 - tp10min > 3)
    andamentoTp = "+";
  else if (tp*10 - tp10min < -3)
    andamentoTp = "-";
  else
    andamentoTp = "=";

  if (millis() > 900000)
    tpChange = tp - (float(tp10min)/10);//andamento orario
    
//MEDIA GIORNALIERA
  sommaTp += tp;
  numberTp ++;
  mediaTp = sommaTp / numberTp;
  
//ESTREMI
  if ((tp < DayTemp.min) && (tp > -100)) {  //////GIORNALIERI
    DayTemp.min = tp;
    DayTemp.timemin = radioRx.data;
    
  }
  if ((tp > DayTemp.max) && (tp < 80)) {
    DayTemp.max = tp;
    DayTemp.timemax = radioRx.data;

  }
  if ((tp > MonthTemp.max) && (tp < 80))  ///////MENSILI
  {
    MonthTemp.max = tp;
    MonthTemp.timemax = radioRx.data;
  }
  if ((tp < MonthTemp.min) && (tp > -100))
  {
    MonthTemp.min = tp;
    MonthTemp.timemin = radioRx.data;
  }

  if ((tp > YearTemp.max) && (tp < 80))    /////////ANNUALI
  {
    YearTemp.max = tp;
    YearTemp.timemax = radioRx.data;
  }
  if ((tp < YearTemp.min) && (tp > -100))
  {
    YearTemp.min = tp;
    YearTemp.timemin = radioRx.data;
  }
}

//UMIDITA' //////////////////////////
void readHum () {
  ur = float(radioRx.hum)/10;
  ur2 = radioRx.hum2;

//MEDIA
  if(millis() - intmediaur < interval){
    mUr += int(ur*10);
    countUr++;
    UR = float(mUr/countUr)/10;
  } else {
    mUr  = 0;
    countUr = 0;
    intmediaur = millis();
  }
  
//ANDAMENTO
  if (millis() - intAndamentoUr > 300000) {
    ur10min = ur5min;
    ur5min = urnow;
    urnow = ur;
    intAndamentoUr = millis();
  }

  if (abs(ur - ur10min) > 20)
    andamentoUr = " ";
  else if (ur - ur10min > 2)
    andamentoUr = "+";
  else if (ur - ur10min < -2)
    andamentoUr = "-";
  else
    andamentoUr = "=";

  if (millis() > 900000)
    urChange = ur - ur10min;
    
//ESTREMI
  if (ur > DayHum.max)   /////GIORNALIERI
  {
    DayHum.max = ur;
    DayHum.timemax = radioRx.data;
  }
  if (ur < DayHum.min)
  {
    DayHum.min = ur;
    DayHum.timemin = radioRx.data;
  }

  if (ur > MonthHum.max)    //////MENSILI
  {
    MonthHum.max = ur;
    MonthHum.timemax = radioRx.data;
  }
  if (ur < MonthHum.min)
  {
    MonthHum.min = ur;
    MonthHum.timemin = radioRx.data;
  }

  if (ur > YearHum.max)    /////ANNUALI
  {
    YearHum.max = ur;
    YearHum.timemax = radioRx.data;
  }
  if (ur < YearHum.min)
  {
    YearHum.min = ur;
    YearHum.timemin = radioRx.data;
  }
}

////////////////PRESSIONE E I SUOI ESTREMI/////////////////////////
void readPressure () {
  
  uint8_t retry = 255;
  uint32_t tmpPressure;

  do
  {
    tmpPressure = GETPressure();
  }
  while ( errata(tmpPressure, current) && --retry > 0);

  current = tmpPressure;
  pressione = current;
  pressionelivellodelmare = pressione + CALIBRATION;

  previousThree = current - pThree;
  previousSix = current - pSix;

//MEDIA
  if(millis() - intmediapr < interval){
    mPr += pressione;
    mSlpr += pressionelivellodelmare;
    countPr++;
    PR = mPr/countPr;
    SLPR = mSlpr/countPr;
  } else {
    mPr  = 0;
    mSlpr = 0;
    countPr = 0;
    intmediapr = millis();
  }

//SALVATAGGIO pressione precente da 1 a 6 ore
  if ((millis() - intPressure) > 3600000){ 
     intPressure = millis();
     pSix = pFive; //pressione -6 ore
     pFive = pFour; //pressione -5 ore
     pFour = pThree; //pressione -4 ore
     pThree = pTwo; //pressione -3 ore
     pTwo = pOne; //pressione -2 ore
     pOne = pNow; //pressione -1 ore
     pNow = current; //pressione attuale
  }

//ESTREMI
  if (pressione > DayPressure.max) /////////GIORNALIERI
  {
    DayPressure.max = pressione;
    DayPressureSlm.max = pressionelivellodelmare;
    DayPressure.timemax = radioRx.data;
  }
  if (pressione < DayPressure.min)
  {
    DayPressure.min = pressione;
    DayPressureSlm.min = pressionelivellodelmare;
    DayPressure.timemin = radioRx.data;
  }

  if (pressione > MonthPressure.max)  ////////MENSILI
  {
    MonthPressure.max = pressione;
    MonthPressureSlm.max = pressionelivellodelmare;
    MonthPressure.timemax = radioRx.data;
  }
  if (pressione < MonthPressure.min)
  {
    MonthPressure.min = pressione;
    MonthPressureSlm.min = pressionelivellodelmare;
    MonthPressure.timemin = radioRx.data;
  }

  if (pressione > YearPressure.max) //////////ANNUALI
  {
    YearPressure.max = pressione;
    YearPressureSlm.max = pressionelivellodelmare;
    YearPressure.timemax = radioRx.data;

  }
  if (pressione < YearPressure.min)
  {
    YearPressure.min = pressione;
    YearPressureSlm.min = pressionelivellodelmare;
    YearPressure.timemin = radioRx.data;
  }

}

//VELOCITA' DEL VENTO E I SUOI ESTREMI//
void readWind () {    
  wind.val = float(radioRx.windval)/10;
  wind.raffica = float(radioRx.raffica)/10;
  
//MEDIA
  if ((millis() - tmediawind) <= interval){
    if(wind.val < 150){
      sommaWind += wind.val;
      iwind++;
      wind.valmedio = sommaWind/iwind;
    }
    
    //valore massimo nell'intervallo media
    if ((wind.val > wind.max && wind.val < 150))
      wind.max = wind.val;
  }
  else {
    iwind = 0;
    sommaWind = 0;
    tmediawind = millis();
  }

//ESTREMI
  if (wind.raffica > DayWind.max && wind.val < 150) {  /////GIORNALIERI
    DayWind.max = wind.raffica;
    DayWind.timemax = radioRx.data;
   
  }

  if (wind.raffica > MonthWind.max && wind.val < 150)  ////MENSILI
  {
    MonthWind.max = wind.val;
    MonthWind.timemax = radioRx.data;
    
  }

  if (wind.raffica > YearWind.max && wind.val < 150)  ///ANNUALI
  {
    YearWind.max = wind.val;
    YearWind.timemax = radioRx.data;
  }
}

//DIREZIONE DEL VENTO
void readWindDir () {
  wind.dir = direzioni[radioRx.winddir];
  wind.ang = gradi[radioRx.winddir];
  wind.err = erdir[radioRx.erWinddir];
  
//media wind dir ultimi 10'
  wind.dirmedia = direzioni[radioRx.winddirMedio];
  wind.angmedio = gradi[radioRx.winddirMedio];

}

//ACCUMULO PIOGGIA E INTENSITA' DELLA PIOGGIA E I SUOI ESTREMI///////////////
void readPioggia () {
  prevPioggia = mmPioggia;
  mmPioggia = float(radioRx.pioggiaday)/10;
  rainrate = float(radioRx.rainrate)/10;
  
  if (mmPioggia == 0)
    prevPioggia = 0;
  mmPioggiagiorno += (mmPioggia-prevPioggia);
  mmPioggiamese += (mmPioggia-prevPioggia);
  mmPioggiaanno += (mmPioggia-prevPioggia);

//media rainrate
  if ((millis() - tmediaRR) <= interval){
    sommaRR += rainrate;
    iRR++;
    mediaRR = sommaRR/iRR;

    //rainrate massimo nell'intervallo della media
    if (rainrate > rainrateMax)
      rainrateMax = rainrate;
  }
  else {
    iRR = 0;
    sommaRR = 0; 
    tmediaRR = millis();
  }
  
//ESTREMI
  if (rainrate > DayRainRate.max) /////GIORNALIERI
  {
    DayRainRate.max = rainrate;
    DayRainRate.timemax = radioRx.data;
  }

  if (rainrate > MonthRainRate.max) ////MENSILI
  {
    MonthRainRate.max = rainrate;
    MonthRainRate.timemax = radioRx.data;

  }

  if (rainrate > YearRainRate.max) /////ANNUALI
  {
    YearRainRate.max = rainrate;
    YearRainRate.timemax = radioRx.data;
  }
}

//PUNTO DI RUGIADA E I SUOI ESTREMI/////////////////
void readDewPoint () {
  float hu = float(ur);
  dewPoint = (pow (hu / 100, 0.125) * (112 + (0.9 * tp)) + 0.1 * tp - 112);//equazione per il calcolo del dewpoint
  
//ANDAMENTO
  if (millis() - intAndamentoDp > 300000) {
    dp10min = dp5min;
    dp5min = dpnow;
    dpnow = int(dewPoint*10);
    intAndamentoDp = millis();
  }

  if (abs(dewPoint*10 - dp10min) > 50)
    andamentoDp = " ";
  else if (dewPoint*10 - dp10min > 3)
    andamentoDp = "+";
  else if (dewPoint*10 - dp10min < -3)
    andamentoDp = "-";
  else
    andamentoDp = "=";
//andamento orario
  if (millis() > 900000)
    dpChange = dewPoint - (float(dp10min)/10);

//ESTREMI
  if ((dewPoint > DayDewPoint.max) && (dewPoint < 80))  ////GIORNALIERI
  {
    DayDewPoint.max = dewPoint;
    DayDewPoint.timemax = radioRx.data;
    
  }
  if ((dewPoint < DayDewPoint.min) && (dewPoint > -90))
  {
    DayDewPoint.min = dewPoint;
    DayDewPoint.timemin = radioRx.data;
  
  }

  if ((dewPoint > MonthDewPoint.max) && (dewPoint < 80))  ////MENSILI
  {
    MonthDewPoint.max = dewPoint;
    MonthDewPoint.timemax = radioRx.data;
    
  }
  if ((dewPoint < MonthDewPoint.min) && (dewPoint > -90))
  {
    MonthDewPoint.min = dewPoint;
    MonthDewPoint.timemin = radioRx.data;
    
  }

  if ((dewPoint > YearDewPoint.max) && (dewPoint < 80))  ///ANNUALI
  {
    YearDewPoint.max = dewPoint;
    YearDewPoint.timemax = radioRx.data;
    
  }
  if ((dewPoint < YearDewPoint.min) && (dewPoint > -90))
  {
    YearDewPoint.min = dewPoint;
    YearDewPoint.timemin = radioRx.data;
    
  }

}

//RAFFREDDAMENTO DEL VENTO E I SUOI ESTREMI//////////
void readWindchill () {
  windchill = (13.12 + 0.6215 * tp) - (11.37 * pow(wind.val*3.6, 0.16)) + (0.3965 * tp * pow(wind.val*3.6, 0.16));//equazione per il calcolo del windchill

//correzione errori
  if ((windchill < tp) && (wind.val*3.6 > 4.6) && (windchill > -127))
  {
    windchill = (13.12 + 0.6215 * tp) - (11.37 * pow(wind.val*3.6, 0.16)) + (0.3965 * tp * pow(wind.val*3.6, 0.16));
  }
  else
  {
    windchill = tp;
  }

//ESTREMI
  if ((windchill < DayChill.min) && (windchill > -100))  ////GIORNALIERI
  {
    DayChill.min = windchill;
    DayChill.timemin = radioRx.data;

  }

  if ((windchill < MonthChill.min) && (windchill > -100))  ///MENSILI
  {
    MonthChill.min = windchill;
    MonthChill.timemin = radioRx.data;
   
  }

  if ((windchill < YearChill.min) && (windchill > -100))  ////ANNUALI
  {
    YearChill.min = windchill;
    YearChill.timemin = radioRx.data;
   
  }

}

//INDICE DI CALORE E I SUOI ESTREMI/////////////////
void readHeatIndex() {
  int hu = ur;
  float hic = dht.computeHeatIndex(tp, hu, false);//questo calcolo dell'indice di calore viene fornito dalla libreria 'dht'
  heatindexc = hic;

//ESTREMI
  if ((heatindexc > DayHeatIndex.max) && (heatindexc < 80)) //GIORNALIERI
  {
    DayHeatIndex.max = heatindexc;
    DayHeatIndex.timemax = radioRx.data;
    
  }

  if ((heatindexc > MonthHeatIndex.max) && (heatindexc < 80))  //MENSILI
  {
    MonthHeatIndex.max = heatindexc;
    MonthHeatIndex.timemax = radioRx.data;
  }

  if ((heatindexc > YearHeatIndex.max) && (heatindexc < 80))  //ANNUALI
  {
    YearHeatIndex.max = heatindexc;
    YearHeatIndex.timemax = radioRx.data;
  }

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////ALTRE FUNZIONI///////////////////////////////////////////////////////
//SCRITTURA DI ALCUNI VALORI SUL DISPLAY//
void readDisplay() {
  tft.fillScreen(ST7735_BLACK);
  tft.setTextColor (ST7735_WHITE);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
    tft.print(F("T "));
    tft.print(tp, 1);
    tft.print(F("  "));
    tft.print(tpdht, 1);
    tft.print ((char)248);
    tft.print(F("C"));
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
    tft.println(andamentoTp);
    tft.drawLine(148,9,148,41,ST7735_CYAN);
tft.drawLine(0,8,159,8,0x5d5348);
tft.setCursor(0,11);    
tft.setTextColor (ST7735_WHITE);    
tft.print(F("U"));
tft.setCursor(17,11);
tft.print(ur);
tft.print(F("%"));
tft.setCursor(72,11);
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
tft.print(andamentoUr);
tft.setCursor(154,11);
tft.setTextColor (ST7735_GREEN);
tft.println(erbmp);       /////////errore bmp180
    tft.drawLine(0,19,148,19,0x5d5348);
    tft.setCursor(0,22); 
    tft.setTextColor (ST7735_WHITE);
    tft.print(F("DP"));
    tft.setCursor(17,22);
    tft.print(dewPoint, 1); 
    tft.print((char)248);
    tft.print(F("C"));
    tft.setCursor(72,22);
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
    tft.print(andamentoDp);
    tft.setCursor(149,22);
    tft.setTextColor (ST7735_WHITE);
    tft.print(radioLink);   ///////errore mancato collegamento radio con master
    tft.setCursor(154,22);
    tft.setTextColor (ST7735_GREEN);
    tft.println(erradioRX);  ////////errore radio init slave
tft.drawLine(0,30,148,30,0x5d5348);
tft.setCursor(0,33);     
tft.setTextColor (ST7735_WHITE);    
tft.print(F("HI"));
tft.setCursor(17,33);
tft.print(heatindexc, 1); 
tft.print((char)248);
tft.print(F("C"));
tft.setCursor(72,33);
tft.print(F(" "));
tft.setTextColor (ST7735_RED);
tft.print(DayHeatIndex.max, 1);
tft.print((char)248);
tft.print(F("C"));
tft.setTextColor (ST7735_WHITE);
tft.print(F(" "));
tft.setTextColor (ST7735_YELLOW);
tft.println(F("="));
tft.setCursor(149,33);
tft.setTextColor (ST7735_RED);
tft.print(resetTx);//radioRx.ersd);   ///////errore sd master
tft.setCursor(154,33);
tft.setTextColor (ST7735_GREEN);
tft.println(erroreSD);         ////////errore sd slave
    tft.drawLine(0,41,159,41,ST7735_RED);
    tft.setCursor(0,44);
    tft.setTextColor (ST7735_WHITE);
    tft.print(F("P "));
    tft.setCursor(13,44);
    tft.print(pressionelivellodelmare/100, 1); 
    tft.print(F(" hPa"));
    tft.setCursor(74,44);
    tft.print(F(" "));
    tft.setTextColor (ST7735_YELLOW);
    tft.print(previousThree/100, 1);
    tft.setTextColor (ST7735_WHITE);
    tft.print(F(" "));
    tft.setTextColor (ST7735_GREEN);
    tft.println(previousSix/100, 1);
  tft.setCursor(13,52);
  tft.setTextColor (ST7735_WHITE);
  tft.print(pressione/100, 1);
  tft.setCursor(127,52);
  tft.print(float(radioRx.volt)/10, 1);
  tft.println(F("V"));
tft.drawLine(0,61,159,61,ST7735_RED);   
tft.setCursor(0,64);    
tft.setTextColor (ST7735_WHITE);    
tft.print(F("W"));
tft.setCursor(17,64);
tft.print(wind.val*3.6, 1); 
tft.print(F("Km/h"));
tft.setCursor(78,64);
tft.print(F("G "));
tft.setTextColor (ST7735_RED);
tft.print(DayWind.max*3.6, 1);
tft.setTextColor (ST7735_WHITE);
tft.print(F(" A "));
tft.setTextColor (ST7735_GREEN);
tft.println(wind.valmedio*3.6, 1);
    tft.drawLine(0,72,159,72,0x5d5348);
    tft.setCursor(0,75);
    tft.setTextColor (ST7735_WHITE);
    tft.print(F("Dir "));
    tft.print(wind.dir);
    tft.setCursor(47,75);
    tft.print(F("A"));
    tft.setTextColor (ST7735_GREEN);
    tft.print(wind.dirmedia);
    tft.drawLine(76,75,76,83,ST7735_CYAN);
    tft.setCursor(78,75);
    tft.setTextColor (ST7735_WHITE);
    tft.print("WC ");
    tft.print(windchill, 1);
    tft.print(" ");
    tft.setTextColor (ST7735_BLUE);
    tft.print(DayChill.min, 1);
    tft.drawLine(154,75,154,83,ST7735_CYAN);
    tft.setCursor(155,75);
    tft.setTextColor(ST7735_WHITE);
    tft.println(wind.err);
tft.drawLine(0,83,159,83,ST7735_RED);
tft.setCursor(0,86);    
tft.setTextColor (ST7735_BLUE);    
tft.print(F("Day"));
tft.setTextColor (ST7735_BLUE);
tft.setCursor(45,86);
tft.print(F("Month"));
tft.setTextColor (ST7735_BLUE); 
tft.setCursor(100,86);
tft.println(F("Year"));
tft.setCursor(130,86);
tft.setTextColor (ST7735_WHITE);
tft.println(float(radioRx.tp2)/10, 1);
    tft.setTextColor (ST7735_WHITE);
    tft.print(mmPioggiagiorno, 1);
    tft.print(F("mm"));
    tft.setCursor(45,94);
    tft.print(mmPioggiamese, 1);
    tft.print(F("mm"));
    tft.setCursor(100,94);
    tft.print(mmPioggiaanno, 1);
    tft.println(F("mm"));
tft.drawLine(0,104,159,104,0x5d5348); 
tft.setCursor(0,107);    
tft.setTextColor (ST7735_WHITE);    
tft.print(F("RR "));
tft.print(rainrate, 1); 
tft.print(F("mm/h"));
tft.setCursor(77,107);
tft.print(F("M"));
tft.setTextColor (ST7735_RED);
tft.print(DayRainRate.max, 1);
tft.setTextColor (ST7735_WHITE);
tft.print(F(" A"));
tft.setTextColor (ST7735_GREEN);
tft.println(mediaRR, 1);
    tft.drawLine(0,115,159,115,ST7735_RED);
    tft.setCursor(0,118);
    tft.setTextColor (ST7735_YELLOW);
    //tft.print(writeExtreme);
    tft.print(radioRx.reset);
    tft.print(" ");
    tft.setTextColor (ST7735_GREEN);
    tft.print(lastRadioRx);
    tft.drawLine(118,116,118,126,ST7735_RED);
    tft.setCursor(120,118);
    tft.setTextColor (ST7735_WHITE);
    tft.print(barometer.readTemperature(),1);
    tft.print ((char)248);
    tft.print(F("C"));
  
}
//RESET ESTREMI AL TERMINE DI GIORNO, MESE O ANNO//
bool eseguiResetDay = false;
bool eseguiResetMonth = false;
bool eseguiResetYear = false;

void RestartEstremi() {
  if (d.Hour == 0 && eseguiResetDay == true){ //if status 0
    eseguiResetDay = false; //status 1
    
    if (!SD.begin(SD_SS, SPI_HALF_SPEED))
      erroreSD = false; 
      //Serial.println(F("SD error"));    
    else {
      erroreSD = true;
      //Serial.println(F("SD ok"));
      
      String dataString = String(annoSalvato) + "/" + String(meseSalvato) + "/" + String(giornoSalvato) + " " + String(DayTemp.max, 1) + " " + String(maxtemptime()) + " " + String(DayTemp.min, 1) + " " + String(mintemptime()) 
                          + " " + String(DayHum.max, 1) + " " + String(maxhumtime()) + " " + String(DayHum.min) + " " + String(minhumtime()) + " " + String(DayDewPoint.max, 1) + " " + String(maxdewtime()) + " " + String(DayDewPoint.min, 1) + " " + String(mindewtime()) 
                          + " " + String(DayPressureSlm.max/100, 1) + " " + String(maxpressuretime()) + " " + String(DayPressureSlm.min/100, 1) + " " + String(minpressuretime()) + " " + String(DayWind.max*3.6, 1) + " " + String(maxwindtime())
                          + " " + String(DayChill.min, 1) + " " + String(minchilltime()) + " " + String(DayRainRate.max, 1) + " " + String(maxratetime()) + " " + String(DayHeatIndex.max, 1) + " " + String(maxindextime()) + " " + String(mediaTp) + " " + String(mmPioggiagiorno);
      
      String stringTimestamp = String(annoSalvato) + " " + String(meseSalvato) + " " + String(giornoSalvato) + " " + String(DayTemp.max, 1) + " " + String(DayTemp.timemax) + " " + String(DayTemp.min, 1) + " " + String(DayTemp.timemin) 
                          + " " + String(DayHum.max, 1) + " " + String(DayHum.timemax) + " " + String(DayHum.min) + " " + String(DayHum.timemin) + " " + String(DayDewPoint.max, 1) + " " + String(DayDewPoint.timemax) + " " + String(DayDewPoint.min, 1) + " " + String(DayDewPoint.timemin) 
                          + " " + String(DayPressureSlm.max/100, 1) + " " + String(DayPressureSlm.timemax) + " " + String(DayPressureSlm.min/100, 1) + " " + String(DayPressureSlm.timemin) + " " + String(DayWind.max*3.6, 1) + " " + String(DayWind.timemax)
                          + " " + String(DayChill.min, 1) + " " + String(DayChill.timemin) + " " + String(DayRainRate.max, 1) + " " + String(DayRainRate.timemax) + " " + String(DayHeatIndex.max, 1) + " " + String(DayHeatIndex.timemax) + " " + String(mediaTp) + " " + String(mmPioggiagiorno);

      File logFile = SD.open("extDay.csv", FILE_WRITE);
      if (logFile){
        logFile.println(dataString);
        logFile.close();
        writeExtreme = !writeExtreme;
        //Serial.println(F("written extDay.csv"));
      }

      File logFile2 = SD.open("extDayTs.csv", FILE_WRITE);
      if (logFile2){
        logFile2.println(stringTimestamp);
        logFile2.close();
        //Serial.println(F("written extDayTs.csv"));
      }
    }
    //reset estremi
    //-prevPioggia = 0;
    //mmPioggia = 0;
    mmPioggiagiorno = 0;
    DayWind.max = wind.val;
    DayTemp.max = tp;
    DayTemp.min = tp;
    DayPressureSlm.max = pressionelivellodelmare;
    DayPressureSlm.min = pressionelivellodelmare;
    DayPressure.max = pressione;
    DayPressure.min = pressione;
    DayHum.max = ur;
    DayHum.min = ur;
    DayDewPoint.max = dewPoint;
    DayDewPoint.min = dewPoint;
    DayHeatIndex.max = heatindexc;
    DayChill.min = windchill;
    DayRainRate.max = rainrate;
    DayWind.timemax = radioRx.data;
    DayTemp.timemax = radioRx.data;
    DayTemp.timemin = radioRx.data;
    DayPressureSlm.timemax = radioRx.data;
    DayPressureSlm.timemin = radioRx.data;
    DayPressure.timemin = radioRx.data;
    DayPressure.timemax = radioRx.data;
    DayHum.timemax = radioRx.data;
    DayHum.timemin = radioRx.data;
    DayDewPoint.timemax = radioRx.data;
    DayDewPoint.timemin = radioRx.data;
    DayHeatIndex.timemax = radioRx.data;
    DayChill.timemin = radioRx.data;
    DayRainRate.timemax = radioRx.data;
    intPressure = 0;
    previousMillis = 0;
    sommaTp = 0;
    numberTp = 0;
    mediaTp = tp;
    File logFile = SD.open("pmn175.txt", FILE_WRITE);
    logFile.remove();
  }
  if (d.Hour != 0 && eseguiResetDay == false){ //if status 1
    eseguiResetDay = true; //status 0
  }

//RESET MESE
  if (d.Day == 1 && eseguiResetMonth == true){ //if status 0
    eseguiResetMonth = false; //status 1
    meseSalvato = d.Month;
    MonthTemp.max = tp;
    MonthTemp.min = tp;
    MonthWind.max = wind.val;
    mmPioggiamese = 0;
    MonthPressure.max = pressione;
    MonthPressure.min = pressione;
    MonthPressureSlm.max = pressionelivellodelmare;
    MonthPressureSlm.min = pressionelivellodelmare;
    MonthHum.max = ur;
    MonthHum.min = ur;
    MonthDewPoint.max = dewPoint;
    MonthDewPoint.min = dewPoint;
    MonthHeatIndex.max = heatindexc;
    MonthChill.min = windchill;
    MonthRainRate.max = rainrate;
  }
  if (d.Day != 1 && eseguiResetMonth == false){ //if status 1
    eseguiResetMonth = true; //status 0
  }

//RESET ANNO
  if (d.Month == 1 && eseguiResetYear == true){ //if status 0
    eseguiResetYear = false; //status 1
    annoSalvato = d.Year+1970;
    YearTemp.max = tp;
    YearTemp.min = tp;
    YearWind.max = wind.val;
    mmPioggiaanno = 0;
    YearPressure.max = pressione;
    YearPressure.min = pressione;
    YearPressureSlm.max = pressionelivellodelmare;
    YearPressureSlm.min = pressionelivellodelmare;
    YearHum.max = ur;
    YearHum.min = ur;
    YearDewPoint.max = dewPoint;
    YearDewPoint.min = dewPoint;
    YearHeatIndex.max = heatindexc;
    YearChill.min = windchill;
    YearRainRate.max = rainrate;
  }
  if (d.Month != 1 && eseguiResetYear == false){ //if status 1
    eseguiResetYear = true; //status 0
  }
}


//SALVATAGGIO VALORI NEL FILE 'datalog.csv'
void Datalog() {
  erroreSD  = SD.begin(SD_SS, SPI_HALF_SPEED);
  if (millis() - previousMillis >= interval) {
    if (!erroreSD)
      //Serial.println(F("SD ok"));
      erroreSD = false;    
    else {
      //Serial.println(F("SD ok"));
      erroreSD=true;
      previousMillis = millis();
  
      String dataString = String(d.Year+1970) + "/" + String(d.Month) + "/" + String(d.Day) + " " + String(d.Hour) + ":" + String(d.Minute)+ " " + String(TP, 1) + " " + String(TPDHT, 1) + " " +
                          String(UR) + " " + String(dewPoint, 1) + " " + String(heatindexc, 1) + " " + String(SLPR/100, 1) + " " + String(wind.valmedio*3.6, 1) + " " + String(wind.max*3.6, 1) 
                          + " " + wind.dirmedia + " " + String(windchill, 1) + " " + String(mmPioggiagiorno, 1) + " " + String(mediaRR, 1) + " " + String(rainrateMax, 1) + " " + String(TP2, 1);

      File logFile = SD.open("dataDay.csv", FILE_WRITE);
      if (logFile){
        logFile.println(dataString);
        logFile.close();
        //Serial.println(F("written dataDay.csv"));
      }
      //reset estremi temporanei dopo averli scritti
      wind.max = 0;
      rainrateMax = 0;
    }
  }
}


void DatalogWeb() {
  if (millis() - previousWeb >= intervalWeb) {
    if (SD.begin(SD_SS, SPI_HALF_SPEED)) {
      previousWeb = millis();

      String dataString = "pmn175;" + daymonth() + "/" + String(d.Year-30) + ";" + hourminute() + ";" + String(TP, 1) + ";" + String(pressionelivellodelmare/100, 1) + ";" +
                            String(ur) + ";" + String(wind.valmedio*3.6, 1)+ ";"+ String(gradi[radioRx.winddirMedio]) + ";" + String(DayWind.max*3.6, 1) + ";" + String(rainrate, 1) + ";" + String(mmPioggiagiorno, 1)
                            + ";" + String(dewPoint, 1) + ";" + "IDEarduino;1.8.17;" + "-99999;-99999;-99999;-99999;";
                            
      File logFile = SD.open("pmn175.txt", FILE_WRITE);
      if (logFile){
        logFile.println(dataString);
        logFile.close();
        //Serial.println(F("written data.txt"));
      }
    }
  }
}


String daymonth (){
  String data;
  if (d.Day < 10) {
    data += "0" + String(d.Day);
  } else data += String(d.Day);
  data += "/";
  if (d.Month < 10) {
    data += "0" + String(d.Month);
  } else data += String(d.Month);
  return data;
}

String hourminute() {
  String time;
  if (d.Hour < 10) {
    time += "0" + String(d.Hour);
  } else time += String(d.Hour);
  time += ":";
  if (d.Minute < 10) {
    time += "0" + String(d.Minute);
  } else time += String(d.Minute);
  return time;
}

void writeEstremi () {
  if (SD.begin(SD_SS, SPI_HALF_SPEED)) {
    ofstream estremiOut ("estremi.dat");
  
    estremiOut << DayTemp.max << " " << DayTemp.timemax << " " << DayTemp.min << " " << DayTemp.timemin << endl
    << DayHum.max << " " << DayHum.timemax << " " << DayHum.min << " " << DayHum.timemin << endl
    << DayPressure.max << " " << DayPressure.timemax << " " << DayPressure.min << " " << DayPressure.timemin << endl
    << DayPressureSlm.max << " " << DayPressureSlm.timemax << " " << DayPressureSlm.min << " " << DayPressureSlm.timemin << endl
    << DayDewPoint.max << " " << DayDewPoint.timemax << " " << DayDewPoint.min << " " << DayDewPoint.timemin << endl
    << DayWind.max << " " << DayWind.timemax << endl
    << DayChill.min << " " << DayChill.timemin << endl
    << DayHeatIndex.max << " " << DayHeatIndex.timemax << endl
    << DayRainRate.max << " " << DayRainRate.timemax << endl
    
    << MonthTemp.max << " " << MonthTemp.timemax << " " << MonthTemp.min << " " << MonthTemp.timemin << endl
    << MonthHum.max << " " << MonthHum.timemax << " " << MonthHum.min << " " << MonthHum.timemin << endl
    << MonthPressure.max << " " << MonthPressure.timemax << " " << MonthPressure.min << " " << MonthPressure.timemin << endl
    << MonthPressureSlm.max << " " << MonthPressureSlm.timemax << " " << MonthPressureSlm.min << " " << MonthPressureSlm.timemin << endl
    << MonthDewPoint.max << " " << MonthDewPoint.timemax << " " << MonthDewPoint.min << " " << MonthDewPoint.timemin << endl
    << MonthWind.max << " " << MonthWind.timemax << endl
    << MonthChill.min << " " << MonthChill.timemin << endl
    << MonthHeatIndex.max << " " << MonthHeatIndex.timemax << endl
    << MonthRainRate.max << " " << MonthRainRate.timemax << endl
    
    << YearTemp.max << " " << YearTemp.timemax << " " << YearTemp.min << " " << YearTemp.timemin << endl
    << YearHum.max << " " << YearHum.timemax << " " << YearHum.min << " " << YearHum.timemin << endl
    << YearPressure.max << " " << YearPressure.timemax << " " << YearPressure.min << " " << YearPressure.timemin << endl
    << YearPressureSlm.max << " " << YearPressureSlm.timemax << " " << YearPressureSlm.min << " " << YearPressureSlm.timemin << endl
    << YearDewPoint.max << " " << YearDewPoint.timemax << " " << YearDewPoint.min << " " << YearDewPoint.timemin << endl
    << YearWind.max << " " << YearWind.timemax << endl
    << YearChill.min << " " << YearChill.timemin << endl
    << YearHeatIndex.max << " " << YearHeatIndex.timemax << endl
    << YearRainRate.max << " " << YearRainRate.timemax << endl
    << pOne << " " << pTwo << " " << pThree << " " << pFour << " " << pFive << " " << pSix << endl
    << mmPioggia << " " << mmPioggiagiorno << " " << mmPioggiamese << " " << mmPioggiaanno << endl
    << sommaTp << " " << numberTp << endl;
   
    //if (!estremiOut) Serial.println (F("error write estremi"));
  
    estremiOut.close();
  }
} 


void getTime() {
  breakTime (radioRx.data, d);//usare d.Hour d.Minute d.Second d.Day d.Month d.Year per registrare valori orari
  lastRadioRx = String(d.Hour) + ":" + String(d.Minute) + ":" + String(d.Second) + "-" + String(d.Day) + "/" + String(d.Month);
}

void getLastSdTx() {
  breakTime (radioRx.lastDatalog, s);
  lastDatalog = String(s.Hour) + ":" + String(s.Minute);
}
//data attuale
void getDateTime () {
  breakTime (radioRx.data, dd);
  wsDate = String(dd.Year+1970) + "-" + String(dd.Month) + "-" + String(dd.Day);
  wsTime = String(dd.Hour) + ":" + String(dd.Minute) + ":" + String(dd.Second);
}

//reset da remoto
void(* resetFunc) (void) = 0; 

int valButtonOld = LOW;
void resetArduino() {
  int valButton = digitalRead(A4);
  
  if (digitalRead(A4) == HIGH && valButtonOld == LOW)
    resetTx = !resetTx;

  int valButtonOld = valButton;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////VARIABILI PER SITO//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////VALORI ATTUALI//////////////////////////////////////////////////////////////////////////////////
//SOSTITUZIONI TAG PAGINE HTML DI ORA E DATA//
//ORA//

PString& evaluate_time (void *data __attribute__ ((unused))) {
  byte x;

  x = d.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = d.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = d.Second;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORA HH:MM//
PString& evaluateweb_time (void *data __attribute__ ((unused))) {
  byte x;

  x = d.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = d.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//DATA//
PString& data_time (void *data __attribute__ ((unused))) {
  int x;

  x = d.Day;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print ('/');

  x = d.Month;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print ('/');

  x = d.Year+1970;
  subBuffer.print (x);

  return subBuffer;
}

//TENSIONE BATTERIA
PString& voltbatteria (void *data __attribute__ ((unused))) {
  float tensione = float(radioRx.volt)/10;
  subBuffer.print(tensione, 1);
  return subBuffer;
}
  
//SOSTITUZIONE TAG PAGINE HTML DEI VALORI ATTUALI//
//TEMPERATURA//
PString& temperatura_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (tp, 1);
  return subBuffer;
}
//TEMPERATURA 2//
PString& temperatura2_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (tp2, 1);
  return subBuffer;
}
//TEMPERATURA DHT//
PString& temperaturadht_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (tpdht, 1);
  return subBuffer;
}
//TEMPERATURA MEDIA//
PString& temperaturamedia_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (mediaTp, 1);
  return subBuffer;
}
//UMIDITA'//
PString& umidita_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (ur);
  return subBuffer;
}
//PRESSIONE//
PString& pressione_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (pressione/100, 1);
  return subBuffer;
}
//PRESSIONE  slm//
PString& pressionemare_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (pressionelivellodelmare/100, 1);
  return subBuffer;
}
//VELOCITA' DEL VENTO//
PString& vento_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (wind.val, 1);
  return subBuffer;
}
PString& ventomedio_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (wind.valmedio, 1);
  return subBuffer;
}
PString& ventokm_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (wind.val*3.6, 1);
  return subBuffer;
}
PString& ventomediokm_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (wind.valmedio*3.6, 1);
  return subBuffer;
}
//DIREZIONE DEL VENTO//
PString& direzione_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (wind.dir);
  return subBuffer;
}
//DIREZIONE DEL VENTO MEDIO//
PString& direzionemedia_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (direzioni[radioRx.winddirMedio]);
  return subBuffer;
}
//PIOGGIA ACCUMULO GIORNALIERO//
PString& pioggia_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (mmPioggiagiorno, 1);
  return subBuffer;
}

//PIOGGIA ACCUMULO MENSILE//
PString& pioggiamese_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (mmPioggiamese, 1);
  return subBuffer;
}
//PIOGGIA ACCUMULO ANNUALE//
PString& pioggiaanno_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (mmPioggiaanno, 1);
  return subBuffer;
}

//INTENSITA' DELLA PIOGGIA//
PString& RR_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (rainrate, 1);
  return subBuffer;
}
//PUNTO DI RUGIADA//
PString& dewpoint_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (dewPoint, 1);
  return subBuffer;
}
//RAFFREDDAMENTO DA VENTO//
PString& windchill_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (windchill, 1);
  return subBuffer;
}
//INDICE DI CALORE//
PString& heatindex_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (heatindexc, 1);
  return subBuffer;
}
//ANDAMENTO TEMPERTURA//
PString& andamento_tp (void *data __attribute__ ((unused))) {

  subBuffer.print (tpChange, 1);
  return subBuffer;
}

//ANDAMENTO UMIDITA//
PString& andamento_ur (void *data __attribute__ ((unused))) {

  subBuffer.print (urChange);
  return subBuffer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////VARIABILI PER SITO MASSIME E MINIME////////////////////////////////////////////
//SOSTITUZIONI TAG PAGINE HTML ESTREMI GIORNALIERI//
//TEMPERATURA MINIMA//
PString& mintemp_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (DayTemp.min, 1);
  return subBuffer;
}
//TEMPERATURA MASSIMA//
PString& maxtemp_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (DayTemp.max, 1);
  return subBuffer;
}
//UMIDITA' MASSIMA//
PString& maxhum_notime (void *data __attribute__ ((unused))) {
  byte x = DayHum.max;

  subBuffer.print (x);
  return subBuffer;
}
//UMIDITA' MINIMA//
PString& minhum_notime (void *data __attribute__ ((unused))) {
  byte x = DayHum.min;

  subBuffer.print (x);
  return subBuffer;
}
//PUNTO DI RUGIADA MASSIMO//
PString& maxdew_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (DayDewPoint.max, 1);
  return subBuffer;
}
//PUNTO DI RUGIADA MINIMO//
PString& mindew_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (DayDewPoint.min, 1);
  return subBuffer;
}
//RAFFICA MASSIMA//
PString& maxraffica_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (DayWind.max*3.6, 1);
  return subBuffer;
}
//INTENSITA' DELLA PIOGGIA MASSIMO//
PString& maxrate_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (DayRainRate.max, 1);
  return subBuffer;
}
//RAFFREDDAMENTO DA VENTO MINIMO//
PString& minchill_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (DayChill.min, 1);
  return subBuffer;
}
//INDICE DI CALORE MASSIMO//
PString& maxindex_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (DayHeatIndex.max, 1);
  return subBuffer;
}
//PRESSIONE MASSIMA//
PString& maxpressure_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (DayPressure.max/100, 1);
  return subBuffer;
}
//PRESSIONE MINIMA//
PString& minpressure_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (DayPressure.min/100, 1);
  return subBuffer;
}

//PRESSIONE SLM MASSIMA//
PString& maxpressureslm_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (DayPressureSlm.max/100, 1);
  return subBuffer;
}
//PRESSIONE SLM MINIMA//
PString& minpressureslm_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (DayPressureSlm.min/100, 1);
  return subBuffer;
}

//PRESSIONE -3 ore//
PString& pressureThree (void *data __attribute__ ((unused))) {

  subBuffer.print (previousThree/100, 1);
  return subBuffer;
}

//PRESSIONE -6 ore//
PString& pressureSix (void *data __attribute__ ((unused))) {

  subBuffer.print (previousSix/100, 1);
  return subBuffer;
}
///////////////////////////////////////////////////////////////////////////////////////////////

////////////////////VARIABILI PER SITO MASSIME E MINIME MENSILI////////////////////////////////
//SOSTITUZIONI TAG PAGINE HTML ESTREMI MENSILI//
//TEMPERATURA MASSIMA MESE//
PString& maxmesetemp_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (MonthTemp.max, 1);
  return subBuffer;
}
//TEMPERATURA MINIMA MESE//
PString& minmesetemp_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (MonthTemp.min, 1);
  return subBuffer;
}
//UMIDITA' MASSIMA MESE//
PString& maxmesehum_notime (void *data __attribute__ ((unused))) {
  byte x = MonthHum.max;

  subBuffer.print (x);
  return subBuffer;
}
//UMIDITA' MINIMA MESE//
PString& minmesehum_notime (void *data __attribute__ ((unused))) {
  byte x = MonthHum.min;

  subBuffer.print (x);
  return subBuffer;
}
//PRESSIONE MASSIMA MESE//
PString& maxmesepressure_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (MonthPressure.max/100, 1);
  return subBuffer;
}
//PRESSIONE MINIMA MESE//
PString& minmesepressure_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (MonthPressure.min/100, 1);
  return subBuffer;
}
//PRESSIONE SLM MASSIMA MESE//
PString& maxmesepressureslm_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (MonthPressureSlm.max/100, 1);
  return subBuffer;
}
//PRESSIONE SLM MINIMA MESE//
PString& minmesepressureslm_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (MonthPressureSlm.min/100, 1);
  return subBuffer;
}
//PUNTO DI RUGIADA MASSIMO MESE//
PString& maxmesedew_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (MonthDewPoint.max, 1);
  return subBuffer;
}
//PUNTO DI RUGIADA MINIMO MESE//
PString& minmesedew_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (MonthDewPoint.min, 1);
  return subBuffer;
}

//INDICE DI CALORE MASSIMO MESE//
PString& maxmeseindex_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (MonthHeatIndex.max, 1);
  return subBuffer;
}
//RAFFREDDAMENTO DA VENTO MINIMO MESE//
PString& minmesechill_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (MonthChill.min, 1);
  return subBuffer;
}
//RAFFICA MASSIMA MESE//
PString& maxmeseraffica_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (MonthWind.max*3.6, 1);
  return subBuffer;
}
//INTENSITA' PRECIPITAZIONI MASSIMA MESE//
PString& maxmeserate_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (MonthRainRate.max, 1);
  return subBuffer;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////VARIABILI PER SITO MASSIME E MINIME ANNUE//////////////////////////////////////
//SOSTITUZIONI TAG PAGINE HTML ESTREMI ANNUI//
//TEMPERATURA MASSIMA ANNO//
PString& maxannotemp_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (YearTemp.max, 1);
  return subBuffer;
}
//TEMPERATURA MINIMA ANNO//
PString& minannotemp_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (YearTemp.min, 1);
  return subBuffer;
}
//UMIDITA' MASSIMA ANNO//
PString& maxannohum_notime (void *data __attribute__ ((unused))) {
  byte x = YearHum.max;

  subBuffer.print (x);
  return subBuffer;
}
//UMIDITA' MINIMA ANNO//
PString& minannohum_notime (void *data __attribute__ ((unused))) {
  byte x = YearHum.min;

  subBuffer.print (x);
  return subBuffer;
}
//PRESSIONE MASSIMA ANNO//
PString& maxannopressure_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (YearPressure.max/100, 1);
  return subBuffer;
}
//PRESSIONE MINIMA ANNO//
PString& minannopressure_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (YearPressure.min/100, 1);
  return subBuffer;
}
//PRESSIONE SLM MASSIMA ANNO//
PString& maxannopressureslm_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (YearPressureSlm.max/100, 1);
  return subBuffer;
}
//PRESSIONE SLM MINIMA ANNO//
PString& minannopressureslm_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (YearPressureSlm.min/100, 1);
  return subBuffer;
}
//PUNTO DI RUGIADA MASSIMO ANNO/
PString& maxannodew_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (YearDewPoint.max, 1);
  return subBuffer;
}
//PUNTO DI RUGIADA MINIMO ANNO//
PString& minannodew_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (YearDewPoint.min, 1);
  return subBuffer;
}

//INDICE DI CALORE MASSIMO ANNO//
PString& maxannoindex_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (YearHeatIndex.max, 1);
  return subBuffer;
}
//RAFFREDDAMENTO DA VENTO MINIMO ANNO//
PString& minannochill_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (YearChill.min, 1);
  return subBuffer;
}
//RAFFICA MASSIMA ANNO//
PString& maxannoraffica_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (YearWind.max*3.6, 1);
  return subBuffer;
}
//INTENSITA' PRECIPITAZIONI MASSIMA ANNO//
PString& maxannorate_notime (void *data __attribute__ ((unused))) {

  subBuffer.print (YearRainRate.max, 1);
  return subBuffer;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////ORARI PER SITO MASSIME E MINIME GIORNALIERE////////////////////////////////////
//SOSTITUZIONE TAG PAGINE HTML ORARI DI RILEVAZIONE MASSIME E MINIME GIORNALIERE//
//ORARIO TEMPERATURA MINIMA//
PString& mintemp_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t t;
  breakTime (DayTemp.timemin, t);
  x = t.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = t.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
PString mintemptime (){
  char buffer[6];
  PString mintemp(buffer, 6);
  
  byte x;
  tmElements_t t;
  breakTime (DayTemp.timemin, t);
  x = t.Hour;

  if (x < 10)
    mintemp.print ('0');
  mintemp.print (x);
  mintemp.print (':');

  x = t.Minute;
  if (x < 10)
    mintemp.print ('0');
  mintemp.print (x);

  return mintemp;
}
//ORARIO TEMPERATURA MASSIMA//
PString& maxtemp_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t n;
  breakTime (DayTemp.timemax, n);
  x = n.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = n.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
 
  return subBuffer;
}
PString maxtemptime (){
  char buffer[6];
  PString maxtemp(buffer, 6);
  
  byte x;
  tmElements_t n;
  breakTime (DayTemp.timemax, n);
  x = n.Hour;

  if (x < 10)
    maxtemp.print ('0');
  maxtemp.print (x);
  maxtemp.print (':');

  x = n.Minute;
  if (x < 10)
    maxtemp.print ('0');
  maxtemp.print (x);

  return maxtemp;
}
//ORARIO UMIDITA' MINIMA//
PString& minhum_time (void *data __attribute__ ((unused))) {
  
  byte x;
  tmElements_t h;
  breakTime (DayHum.timemin, h);
  x = h.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = h.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
PString minhumtime (){
  char buffer[6];
  PString minhum(buffer, 6);
  
  byte x;
  tmElements_t h;
  breakTime (DayHum.timemin, h);
  x = h.Hour;

  if (x < 10)
    minhum.print ('0');
  minhum.print (x);
  minhum.print (':');

  x = h.Minute;
  if (x < 10)
    minhum.print ('0');
  minhum.print (x);

  return minhum;
}
//ORARIO UMIDITA' MASSIMA//
PString& maxhum_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t d;
  breakTime (DayHum.timemax, d);
  x = d.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = d.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
PString maxhumtime (){
  char buffer[6];
  PString maxhum(buffer, 6);
  
  byte x;
  tmElements_t d;
  breakTime (DayHum.timemax, d);
  x = d.Hour;

  if (x < 10)
    maxhum.print ('0');
  maxhum.print (x);
  maxhum.print (':');

  x = d.Minute;
  if (x < 10)
    maxhum.print ('0');
  maxhum.print (x);

  return maxhum;
}
//ORARIO PUNTO DI RUGIADA MINIMO//
PString& mindew_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t j;
  breakTime (DayDewPoint.timemin, j);
  x = j.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = j.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
PString mindewtime (){
  char buffer[6];
  PString mindew(buffer, 6);
  
  byte x;
  tmElements_t d;
  breakTime (DayDewPoint.timemin, d);
  x = d.Hour;

  if (x < 10)
    mindew.print ('0');
  mindew.print (x);
  mindew.print (':');

  x = d.Minute;
  if (x < 10)
    mindew.print ('0');
  mindew.print (x);

  return mindew;
}
//ORARIO PUNTO DI RUGIADA MASSIMO//
PString& maxdew_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t p;
  breakTime (DayDewPoint.timemax, p);
  x = p.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = p.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
PString maxdewtime (){
  char buffer[6];
  PString maxdew(buffer, 6);
  
  byte x;
  tmElements_t d;
  breakTime (DayDewPoint.timemax, d);
  x = d.Hour;

  if (x < 10)
    maxdew.print ('0');
  maxdew.print (x);
  maxdew.print (':');

  x = d.Minute;
  if (x < 10)
    maxdew.print ('0');
  maxdew.print (x);

  return maxdew;
}
//ORARIO RAFFICA MASSIMA//
PString& maxwind_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t u;
  breakTime (DayWind.timemax, u);
  x = u.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = u.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
PString maxwindtime (){
  char buffer[6];
  PString maxwind(buffer, 6);
  
  byte x;
  tmElements_t u;
  breakTime (DayWind.timemax, u);
  x = u.Hour;

  if (x < 10)
    maxwind.print ('0');
  maxwind.print (x);
  maxwind.print (':');

  x = u.Minute;
  if (x < 10)
    maxwind.print ('0');
  maxwind.print (x);

  return maxwind;
}
//ORARIO INTENSITA' DELLA PIOGGIA MASSIMA//
PString& maxrate_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t o;
  breakTime (DayRainRate.timemax, o);
  x = o.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = o.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
PString maxratetime (){
  char buffer[6];
  PString maxrate(buffer, 6);
  
  byte x;
  tmElements_t o;
  breakTime (DayRainRate.timemax, o);
  x = o.Hour;

  if (x < 10)
    maxrate.print ('0');
  maxrate.print (x);
  maxrate.print (':');

  x = o.Minute;
  if (x < 10)
    maxrate.print ('0');
  maxrate.print (x);

  return maxrate;
}
//ORARIO RAFFREDDAMENTO DA VENTO MINIMO//
PString& minchill_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t q;
  breakTime (DayChill.timemin, q);
  x = q.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = q.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
PString minchilltime (){
  char buffer[6];
  PString minchil(buffer, 6);
  
  byte x;
  tmElements_t o;
  breakTime (DayChill.timemin, o);
  x = o.Hour;

  if (x < 10)
   minchil.print ('0');
  minchil.print (x);
  minchil.print (':');

  x = o.Minute;
  if (x < 10)
    minchil.print ('0');
  minchil.print (x);

  return minchil;
}
//ORARIO INDICE DI CALORE MASSIMO//
PString& maxindex_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t z;
  breakTime (DayHeatIndex.timemax, z);
  x = z.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = z.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
PString maxindextime (){
  char buffer[6];
  PString maxindex(buffer, 6);
  
  byte x;
  tmElements_t z;
  breakTime (DayHeatIndex.timemax, z);
  x = z.Hour;

  if (x < 10)
    maxindex.print ('0');
  maxindex.print (x);
  maxindex.print (':');

  x = z.Minute;
  if (x < 10)
    maxindex.print ('0');
  maxindex.print (x);

  return maxindex;
}
//ORARIO PRESSIONE MASSIMA//
PString& maxpressure_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t l;
  breakTime (DayPressure.timemax, l);
  x = l.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = l.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
PString maxpressuretime (){
  char buffer[6];
  PString maxpressure(buffer, 6);
  
  byte x;
  tmElements_t o;
  breakTime (DayPressure.timemax, o);
  x = o.Hour;

  if (x < 10)
    maxpressure.print ('0');
  maxpressure.print (x);
  maxpressure.print (':');

  x = o.Minute;
  if (x < 10)
    maxpressure.print ('0');
  maxpressure.print (x);

  return maxpressure;
}
//ORARIO PRESSIONE MINIMA//
PString& minpressure_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t r;
  breakTime (DayPressure.timemin, r);
  x = r.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = r.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
PString minpressuretime (){
  char buffer[6];
  PString minpressure(buffer, 6);
  
  byte x;
  tmElements_t o;
  breakTime (DayPressure.timemin, o);
  x = o.Hour;

  if (x < 10)
    minpressure.print ('0');
  minpressure.print (x);
  minpressure.print (':');

  x = o.Minute;
  if (x < 10)
    minpressure.print ('0');
  minpressure.print (x);

  return minpressure;
}
//SOSTITUZIONE TAG PAGINE HTML ORARI DI RILEVAZIONE MASSIME E MINIME MENSILI//
//ORARIO TEMPERATURA MINIMA//
PString& monthmintemp_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t t;
  breakTime (MonthTemp.timemin, t);
  x = t.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = t.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO TEMPERATURA MASSIMA//
PString& monthmaxtemp_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t n;
  breakTime (MonthTemp.timemax, n);
  x = n.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = n.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO UMIDITA' MINIMA//
PString& monthminhum_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t h;
  breakTime (MonthHum.timemin, h);
  x = h.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = h.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO UMIDITA' MASSIMA//
PString& monthmaxhum_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t d;
  breakTime (MonthHum.timemax, d);
  x = d.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = d.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO PUNTO DI RUGIADA MINIMO//
PString& monthmindew_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t j;
  breakTime (MonthDewPoint.timemin, j);
  x = j.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = j.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO PUNTO DI RUGIADA MASSIMO//
PString& monthmaxdew_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t p;
  breakTime (MonthDewPoint.timemax, p);
  x = p.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = p.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO RAFFICA MASSIMA//
PString& monthmaxwind_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t u;
  breakTime (MonthWind.timemax, u);
  x = u.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = u.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO INTENSITA' DELLA PIOGGIA MASSIMA//
PString& monthmaxrate_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t o;
  breakTime (MonthRainRate.timemax, o);
  x = o.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = o.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO RAFFREDDAMENTO DA VENTO MINIMO//
PString& monthminchill_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t q;
  breakTime (MonthChill.timemin, q);
  x = q.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = q.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO INDICE DI CALORE MASSIMO//
PString& monthmaxindex_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t z;
  breakTime (MonthHeatIndex.timemax, z);
  x = z.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = z.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO PRESSIONE MASSIMA//
PString& monthmaxpressure_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t l;
  breakTime (MonthPressure.timemax, l);
  x = l.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = l.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO PRESSIONE MINIMA//
PString& monthminpressure_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t r;
  breakTime (MonthPressure.timemin, r);
  x = r.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = r.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}

//SOSTITUZIONE TAG PAGINE HTML ORARI DI RILEVAZIONE MASSIME E MINIME ANNUALI//
//ORARIO TEMPERATURA MINIMA//
PString& yearmintemp_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t t;
  breakTime (YearTemp.timemin, t);
  x = t.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = t.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO TEMPERATURA MASSIMA//
PString& yearmaxtemp_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t n;
  breakTime (YearTemp.timemax, n);
  x = n.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = n.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO UMIDITA' MINIMA//
PString& yearminhum_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t h;
  breakTime (YearHum.timemin, h);
  x = h.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = h.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO UMIDITA' MASSIMA//
PString& yearmaxhum_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t d;
  breakTime (YearHum.timemax, d);
  x = d.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = d.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO PUNTO DI RUGIADA MINIMO//
PString& yearmindew_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t j;
  breakTime (YearDewPoint.timemin, j);
  x = j.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = j.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO PUNTO DI RUGIADA MASSIMO//
PString& yearmaxdew_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t p;
  breakTime (YearDewPoint.timemax, p);
  x = p.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = p.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO RAFFICA MASSIMA//
PString& yearmaxwind_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t u;
  breakTime (YearWind.timemax, u);
  x = u.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = u.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO INTENSITA' DELLA PIOGGIA MASSIMA//
PString& yearmaxrate_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t o;
  breakTime (YearRainRate.timemax, o);
  x = o.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = o.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO RAFFREDDAMENTO DA VENTO MINIMO//
PString& yearminchill_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t q;
  breakTime (YearChill.timemin, q);
  x = q.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = q.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO INDICE DI CALORE MASSIMO//
PString& yearmaxindex_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t z;
  breakTime (YearHeatIndex.timemax, z);
  x = z.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = z.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO PRESSIONE MASSIMA//
PString& yearmaxpressure_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t l;
  breakTime (YearPressure.timemax, l);
  x = l.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = l.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}
//ORARIO PRESSIONE MINIMA//
PString& yearminpressure_time (void *data __attribute__ ((unused))) {
  byte x;
  tmElements_t r;
  breakTime (YearPressure.timemin, r);
  x = r.Hour;

  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);
  subBuffer.print (':');

  x = r.Minute;
  if (x < 10)
    subBuffer.print ('0');
  subBuffer.print (x);

  return subBuffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////DEFINIZIONI PER SITO USATO DALLA LIBRERIA WEBBINO/////////////////////////////////////////////

// Max length of these is MAX_TAG_LEN (24)

///////////////TAG UTILI//////////////////////////////////////////////////
EasyReplacementTag (tagWebbinoVer, WEBBINO_VER, evaluate_webbino_version);
EasyReplacementTag (tagUptime, UPTIME, evaluate_uptime);
EasyReplacementTag (subVolt, VOLTBATTERIA, voltbatteria);

///////////////DATA E ORA/////////////////////////////////////////////////
EasyReplacementTag (subTimeVarSub, ORA, evaluate_time);
EasyReplacementTag (subTimeWebVarSub, ORAWEB, evaluateweb_time);
EasyReplacementTag (subTimeDatSub, DATA, data_time);

//////////////VALORI ATTUALI//////////////////////////////////////////////
EasyReplacementTag (subTimeTempSub, TEMPERATURA, temperatura_notime);
EasyReplacementTag (subTimeTemp2Sub, TEMPERATURA2, temperatura2_notime);
EasyReplacementTag (subTimeTempDhtSub, TEMPERATURADHT, temperaturadht_notime);
EasyReplacementTag (subTimeTempMediaSub, TEMPERATURAMEDIA, temperaturamedia_notime);
EasyReplacementTag (subTimeHumSub, UMIDITA, umidita_notime);
EasyReplacementTag (subTimePressureSub, PRESSIONE, pressione_notime);
EasyReplacementTag (subTimePressureMareSub, PRESSIONEMARE, pressionemare_notime);
EasyReplacementTag (subTimeWindKmSub, VENTOKM, ventokm_notime);
EasyReplacementTag (subTimeWindMedioKmSub, VENTOMEDIOKM, ventomediokm_notime);
EasyReplacementTag (subTimeWindSub, VENTO, vento_notime);
EasyReplacementTag (subTimeWindMedioSub, VENTOMEDIO, ventomedio_notime);
EasyReplacementTag (subTimeDirezioneSub, DIREZIONE, direzione_notime);
EasyReplacementTag (subTimeDirezioneMediaSub, DIREZIONEMEDIA, direzionemedia_notime);
EasyReplacementTag (subTimePioggiaSub, PIOGGIA, pioggia_notime);
EasyReplacementTag (subTimePioggiaMeseSub, PIOGGIAMESE, pioggiamese_notime);
EasyReplacementTag (subTimePioggiaAnnoSub, PIOGGIAANNO, pioggiaanno_notime);
EasyReplacementTag (subTimeRRSub, RAINRATE, RR_notime);
EasyReplacementTag (subTimeDewPointSub, DEWPOINT, dewpoint_notime);
EasyReplacementTag (subTimeWindChillSub, WINDCHILL, windchill_notime);
EasyReplacementTag (subTimeHeatIndexSub, HEATINDEX, heatindex_notime);
EasyReplacementTag (subTimeVarTp, VARIAZIONETEMP, andamento_tp);
EasyReplacementTag (subTimeVarUr, VARIAZIONEUR, andamento_ur);

/////////////ESTREMI ODIERNI/////////////////////////////////////////////
EasyReplacementTag (subTimeMinTempSub, MINTEMP, mintemp_notime);
EasyReplacementTag (subTimeMaxTempSub, MAXTEMP, maxtemp_notime);
EasyReplacementTag (subTimeMaxHumSub, UMIDITAMAX, maxhum_notime);
EasyReplacementTag (subTimeMinHumSub, UMIDITAMIN, minhum_notime);
EasyReplacementTag (subTimeMaxDewSub, DEWPOINTMAX, maxdew_notime);
EasyReplacementTag (subTimeMinDewSub, DEWPOINTMIN, mindew_notime);
EasyReplacementTag (subTimeMinPressureSub, PRESSIONEMIN, minpressure_notime);
EasyReplacementTag (subTimeMaxPressureSub, PRESSIONEMAX, maxpressure_notime);
EasyReplacementTag (subTimeMinPressureSlmSub, PRESSIONESLMMIN, minpressureslm_notime);
EasyReplacementTag (subTimeMaxPressureSlmSub, PRESSIONESLMMAX, maxpressureslm_notime);
EasyReplacementTag (subTimeMaxWindSub, RAFFICA, maxraffica_notime);
EasyReplacementTag (subTimeMaxRRSub, RAINRATEMAX, maxrate_notime);
EasyReplacementTag (subTimeMinChillSub, WINDCHILLMIN, minchill_notime);
EasyReplacementTag (subTimeMaxHeatSub, HEATINDEXMAX, maxindex_notime);
EasyReplacementTag (subPressureThree, PREVIOUSTHREE, pressureThree);
EasyReplacementTag (subPressureSix, PREVIOUSSIX, pressureSix);

/////////////ORARI MASSIME E MINIME GIORNALIERE/////////////////////////
EasyReplacementTag (subTimeMinTempOraSub, MINTEMP_ORA, mintemp_time);
EasyReplacementTag (subTimeMaxTempOraSub, MAXTEMP_ORA, maxtemp_time);
EasyReplacementTag (subTimeMaxHumOraSub, UMIMAX_ORA, maxhum_time);
EasyReplacementTag (subTimeMinHumOraSub, UMIMIN_ORA, minhum_time);
EasyReplacementTag (subTimeMinDewOraSub, DEWPOINTMIN_ORA, mindew_time);
EasyReplacementTag (subTimeMaxDewOraSub, DEWPOINTMAX_ORA, maxdew_time);
EasyReplacementTag (subTimeMinPressureOraSub, PRESSIONEMIN_ORA, minpressure_time);
EasyReplacementTag (subTimeMaxPressureOraSub, PRESSIONEMAX_ORA, maxpressure_time);
EasyReplacementTag (subTimeMaxWindOraSub, RAFFICA_ORA, maxwind_time);
EasyReplacementTag (subTimeMaxRROraSub, RAINRATEMAX_ORA, maxrate_time);
EasyReplacementTag (subTimeMinChillOraSub, WINDCHILLMIN_ORA, minchill_time);
EasyReplacementTag (subTimeMaxHeatOraSub, HEATINDEXMAX_ORA, maxindex_time);

//////////////ESTREMI MENSILI////////////////////////////////////////////
EasyReplacementTag (subTimeMinTempMeseSub,  TEMPMINMESE, minmesetemp_notime);
EasyReplacementTag (subTimeMaxTempMeseSub, TEMPMAXMESE, maxmesetemp_notime);
EasyReplacementTag (subTimeMaxHumMeseSub, UMIDITAMAXMESE, maxmesehum_notime);
EasyReplacementTag (subTimeMinHumMeseSub, UMIDITAMINMESE, minmesehum_notime);
EasyReplacementTag (subTimeMinDewMeseSub, DEWPOINTMINMESE, minmesedew_notime);
EasyReplacementTag (subTimeMaxDewMeseSub, DEWPOINTMAXMESE, maxmesedew_notime);
EasyReplacementTag (subTimeMinPressureMeseSub, PRESSIONEMINMESE, minmesepressure_notime);
EasyReplacementTag (subTimeMaxPressureMeseSub, PRESSIONEMAXMESE, maxmesepressure_notime);
EasyReplacementTag (subTimeMinPressureSlmMeseSub, PRESSIONESLMMINMESE, minmesepressureslm_notime);
EasyReplacementTag (subTimeMaxPressureSlmMeseSub, PRESSIONESLMMAXMESE, maxmesepressureslm_notime);
EasyReplacementTag (subTimeMaxWindMeseSub, RAFFICAMAXMESE, maxmeseraffica_notime);
EasyReplacementTag (subTimeMaxRRMeseSub, RAINRATEMAXMESE, maxmeserate_notime);
EasyReplacementTag (subTimeMinChillMeseSub, WINDCHILLMINMESE, minmesechill_notime);
EasyReplacementTag (subTimeMaxHeatMeseSub, HEATINDEXMAXMESE, maxmeseindex_notime);

/////////////ORARI MASSIME E MINIME MENSILI/////////////////////////
EasyReplacementTag (subTimeMonthMinTempOraSub, MONTHMINTEMP_ORA, monthmintemp_time);
EasyReplacementTag (subTimeMonthMaxTempOraSub, MONTHMAXTEMP_ORA, monthmaxtemp_time);
EasyReplacementTag (subTimeMonthMaxHumOraSub, MONTHUMIMAX_ORA, monthmaxhum_time);
EasyReplacementTag (subTimeMonthMinHumOraSub, MONTHUMIMIN_ORA, monthminhum_time);
EasyReplacementTag (subTimeMonthMinDewOraSub, MONTHDEWPOINTMIN_ORA, monthmindew_time);
EasyReplacementTag (subTimeMonthMaxDewOraSub, MONTHDEWPOINTMAX_ORA, monthmaxdew_time);
EasyReplacementTag (subTimeMonthMinPressureOraSub, MONTHPRESSIONEMIN_ORA, monthminpressure_time);
EasyReplacementTag (subTimeMonthMaxPressureOraSub, MONTHPRESSIONEMAX_ORA, monthmaxpressure_time);
EasyReplacementTag (subTimeMonthMaxWindOraSub, MONTHRAFFICA_ORA, monthmaxwind_time);
EasyReplacementTag (subTimeMonthMaxRROraSub, MONTHRAINRATEMAX_ORA, monthmaxrate_time);
EasyReplacementTag (subTimeMonthMinChillOraSub, MONTHWINDCHILLMIN_ORA, monthminchill_time);
EasyReplacementTag (subTimeMonthMaxHeatOraSub, MONTHHEATINDEXMAX_ORA, monthmaxindex_time);

//////////////ESTREMI ANNUI////////////////////////////////////////////
EasyReplacementTag (subTimeMinTempAnnoSub, TEMPMINANNO, minannotemp_notime);
EasyReplacementTag (subTimeMaxTempAnnoSub, TEMPMAXANNO, maxannotemp_notime);
EasyReplacementTag (subTimeMaxHumAnnoSub, UMIDITAMAXANNO, maxannohum_notime);
EasyReplacementTag (subTimeMinHumAnnoSub, UMIDITAMINANNO, minannohum_notime);
EasyReplacementTag (subTimeMinDewAnnoSub, DEWPOINTMINANNO, minannodew_notime);
EasyReplacementTag (subTimeMaxDewAnnoSub, DEWPOINTMAXANNO, maxannodew_notime);
EasyReplacementTag (subTimeMinPressureAnnoSub, PRESSIONEMINANNO, minannopressure_notime);
EasyReplacementTag (subTimeMaxPressureAnnoSub, PRESSIONEMAXANNO, maxannopressure_notime);
EasyReplacementTag (subTimeMinPressureSlmAnnoSub, PRESSIONESLMMINANNO, minannopressureslm_notime);
EasyReplacementTag (subTimeMaxPressureSlmAnnoSub, PRESSIONESLMMAXANNO, maxannopressureslm_notime);
EasyReplacementTag (subTimeMaxWindAnnoSub, RAFFICAMAXANNO, maxannoraffica_notime);
EasyReplacementTag (subTimeMaxRRAnnoSub, RAINRATEMAXANNO, maxannorate_notime);
EasyReplacementTag (subTimeMinChillAnnoSub, WINDCHILLMINANNO, minannochill_notime);
EasyReplacementTag (subTimeMaxHeatAnnoSub, HEATINDEXMAXANNO, maxannoindex_notime);

/////////////ORARI MASSIME E MINIME ANNUALI/////////////////////////
EasyReplacementTag (subTimeYearMinTempOraSub, YEARMINTEMP_ORA, yearmintemp_time);
EasyReplacementTag (subTimeYearMaxTempOraSub, YEARMAXTEMP_ORA, yearmaxtemp_time);
EasyReplacementTag (subTimeYearMaxHumOraSub, YEARUMIMAX_ORA, yearmaxhum_time);
EasyReplacementTag (subTimeYearMinHumOraSub, YEARUMIMIN_ORA, yearminhum_time);
EasyReplacementTag (subTimeYearMinDewOraSub, YEARDEWPOINTMIN_ORA, yearmindew_time);
EasyReplacementTag (subTimeYearMaxDewOraSub, YEARDEWPOINTMAX_ORA, yearmaxdew_time);
EasyReplacementTag (subTimeYearMinPressureOraSub, YEARPRESSIONEMIN_ORA, yearminpressure_time);
EasyReplacementTag (subTimeYearMaxPressureOraSub, YEARPRESSIONEMAX_ORA, yearmaxpressure_time);
EasyReplacementTag (subTimeYearMaxWindOraSub, YEARRAFFICA_ORA, yearmaxwind_time);
EasyReplacementTag (subTimeYearMaxRROraSub, YEARRAINRATEMAX_ORA, yearmaxrate_time);
EasyReplacementTag (subTimeYearMinChillOraSub, YEARWINDCHILLMIN_ORA, yearminchill_time);
EasyReplacementTag (subTimeYearMaxHeatOraSub, YEARHEATINDEXMAX_ORA, yearmaxindex_time);

EasyReplacementTagArray tags[] PROGMEM = {
  /////////TAG UTILI///////////////////
  &tagWebbinoVer,
  &tagUptime,
  &subTimeWebVarSub,
  &subVolt,
  /////////VALORI ATTUALI//////////////
  &subTimeVarSub,
  &subTimeDatSub,
  &subTimeTempSub,
  &subTimeTemp2Sub,
  &subTimeTempDhtSub,
  &subTimeTempMediaSub,
  &subTimeHumSub,
  &subTimePressureSub,
  &subTimePressureMareSub,
  &subTimeWindSub,
  &subTimeWindMedioSub,
  &subTimeWindKmSub,
  &subTimeWindMedioKmSub,
  &subTimeDirezioneSub,
  &subTimeDirezioneMediaSub,
  &subTimePioggiaSub,
  &subTimePioggiaMeseSub,
  &subTimePioggiaAnnoSub,
  &subTimeRRSub,
  &subTimeDewPointSub,
  &subTimeWindChillSub,
  &subTimeHeatIndexSub,
  &subTimeVarTp,
  &subTimeVarUr,
//  &subTimeTempMediaSub,
  /////////ESTREMI ODIERNI////////////
  &subTimeMinTempSub,
  &subTimeMaxTempSub,
  &subTimeMaxHumSub,
  &subTimeMinHumSub,
  &subTimeMaxDewSub,
  &subTimeMinDewSub,
  &subTimeMinPressureSub,
  &subTimeMaxPressureSub,
  &subTimeMinPressureSlmSub,
  &subTimeMaxPressureSlmSub,
  &subTimeMaxWindSub,
  &subTimeMaxRRSub,
  &subTimeMinChillSub,
  &subTimeMaxHeatSub,
  &subPressureThree,
  &subPressureSix,
  /////////ORARI MASSIME E MINIME GG/////
  &subTimeMinTempOraSub,
  &subTimeMaxTempOraSub,
  &subTimeMaxHumOraSub,
  &subTimeMinHumOraSub,
  &subTimeMinDewOraSub,
  &subTimeMaxDewOraSub,
  &subTimeMinPressureOraSub,
  &subTimeMaxPressureOraSub,
  &subTimeMaxWindOraSub,
  &subTimeMaxRROraSub,
  &subTimeMinChillOraSub,
  &subTimeMaxHeatOraSub,
  /////////ESTREMI MENSILI///////////
  &subTimeMinTempMeseSub,
  &subTimeMaxTempMeseSub,
  &subTimeMaxHumMeseSub,
  &subTimeMinHumMeseSub,
  &subTimeMaxDewMeseSub,
  &subTimeMinDewMeseSub,
  &subTimeMaxPressureMeseSub,
  &subTimeMinPressureMeseSub,
  &subTimeMinPressureSlmMeseSub,
  &subTimeMaxPressureSlmMeseSub,
  &subTimeMaxWindMeseSub,
  &subTimeMaxRRMeseSub,
  &subTimeMinChillMeseSub,
  &subTimeMaxHeatMeseSub,
  /////////ORARI MASSIME E MINIME MENSILI/////
  &subTimeMonthMinTempOraSub,
  &subTimeMonthMaxTempOraSub,
  &subTimeMonthMaxHumOraSub,
  &subTimeMonthMinHumOraSub,
  &subTimeMonthMinDewOraSub,
  &subTimeMonthMaxDewOraSub,
  &subTimeMonthMinPressureOraSub,
  &subTimeMonthMaxPressureOraSub,
  &subTimeMonthMaxWindOraSub,
  &subTimeMonthMaxRROraSub,
  &subTimeMonthMinChillOraSub,
  &subTimeMonthMaxHeatOraSub,
  /////////ESTREMI ANNUI/////////////
  &subTimeMinTempAnnoSub,
  &subTimeMaxTempAnnoSub,
  &subTimeMaxHumAnnoSub,
  &subTimeMinHumAnnoSub,
  &subTimeMaxDewAnnoSub,
  &subTimeMinDewAnnoSub,
  &subTimeMaxPressureAnnoSub,
  &subTimeMinPressureAnnoSub,
  &subTimeMaxPressureSlmAnnoSub,
  &subTimeMinPressureSlmAnnoSub,
  &subTimeMaxWindAnnoSub,
  &subTimeMaxRRAnnoSub,
  &subTimeMinChillAnnoSub,
  &subTimeMaxHeatAnnoSub,
  /////////ORARI MASSIME E MINIME ANNUALI/////
  &subTimeYearMinTempOraSub,
  &subTimeYearMaxTempOraSub,
  &subTimeYearMaxHumOraSub,
  &subTimeYearMinHumOraSub,
  &subTimeYearMinDewOraSub,
  &subTimeYearMaxDewOraSub,
  &subTimeYearMinPressureOraSub,
  &subTimeYearMaxPressureOraSub,
  &subTimeYearMaxWindOraSub,
  &subTimeYearMaxRROraSub,
  &subTimeYearMinChillOraSub,
  &subTimeYearMaxHeatOraSub,
  NULL
};

#if !defined (WEBBINO_ENABLE_SD) && !defined (WEBBINO_ENABLE_SDFAT)
//#error Please enable WEBBINO_ENABLE_SD or WEBBINO_ENABLE_SDFAT in webbino_config.h
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
//evita conflitto SPI tra nrf24 e ethernet shield  
  pinMode(4, OUTPUT);
  pinMode(10, OUTPUT);
  digitalWrite(4, HIGH);
  digitalWrite(10, HIGH);
  pinMode(41, OUTPUT);
  pinMode(39, OUTPUT);
  digitalWrite(41, HIGH);
  digitalWrite(39, HIGH);

//SETUP  
  Serial.begin(9600);
  Wire.begin();
  SPI.begin();

  pinMode(A0, INPUT);
  
  tft.initR(INITR_BLACKTAB);// inizializzazione display
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLUE);
  tft.setCursor (10, 20);
  tft.setTextSize(2);
  tft.setTextColor (ST7735_GREEN);
  tft.print (F(" STAZIONE"));
  tft.setCursor (15, 40);
  tft.print (F("  METEO:"));
  delay (800);
  tft.setTextColor (ST7735_YELLOW);
  tft.setCursor (5, 60);
  tft.print (F(" SITUAZIONE    ATTUALE"));

  lastDatalog = " ";//reset delle stringhe di errore
  lastRadioRx = " ";

  //initTemp();
  initRadio();
  initSD();
  initPressure ();
  readEstremi ();

  if (INIZIALIZZAZIONE) {
    pNow = current;
    pSix = current;
    pFive = current;
    pFour = current;
    pThree = current;
    pTwo = current;
    pOne = current;

    DayTemp.max = -100; //temperatura massima giornaliera
    DayTemp.min = 100; //temperatura minima giornaliera
    DayHum.max = -2; // umidità massima giornaliera
    DayHum.min = 150; // umidità minima giornaliera
    DayPressure.max = 0; // pressione massima giornaliera
    DayPressure.min = 2000; // pressione minima giornaliera
    DayPressureSlm.max = 0; // pressione massima giornaliera
    DayPressureSlm.min = 2000; // pressione minima giornaliera
    DayDewPoint.max = -100; // punto di rugiada massimo giornaliero
    DayDewPoint.min = 100; // punto di rugiada minimo giornaliero
    DayWind.max = 0; // raffica massima giornaliera
    DayChill.min = 100; // raffreddamento da vento minimo giornaliero
    DayHeatIndex.max = -100; // indice di calore massimo giornaliero
    DayRainRate.max = 0; // intensità della pioggia massima giornaliero
    MonthTemp.max = -100;
    MonthTemp.min = 100;
    MonthHum.max = -2;
    MonthHum.min = 150;
    MonthPressure.max = 0;
    MonthPressure.min = 2000;
    MonthPressureSlm.max = 0;
    MonthPressureSlm.min = 2000;
    MonthDewPoint.max = -100;
    MonthDewPoint.min = 100;
    MonthWind.max = 0;
    MonthChill.min = 100;
    MonthHeatIndex.max = -100;
    MonthRainRate.max = 0;
    YearTemp.max = -100;
    YearTemp.min = 100;
    YearHum.max = -2;
    YearHum.min = 150;
    YearPressure.max = 0;
    YearPressure.min = 2000;
    YearPressureSlm.max = 0;
    YearPressureSlm.min = 2000;
    YearDewPoint.max = -100;
    YearDewPoint.min = 100;
    YearWind.max = 0;
    YearChill.min = 100;
    YearHeatIndex.max = -100;
    YearRainRate.max = 0;
  }
  
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//Serial.println (F("Using Webbino " WEBBINO_VERSION));
/*
  IPAddress ip (IP_ADDRESS);
  //IPAddress dns (DNS_ADDRESS);
  IPAddress gw (GW_ADDRESS);
  IPAddress mask (NETMASK);

  //Serial.println (F("Configuring static IP address"));
  byte mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
  bool ok = netint.begin (mac); //, ip, dns, gw, mask, SD_SS);


  if (!ok) {
    //Serial.println (F("Failed to configure static IP address"));
      ;
  } else {
      //Serial.println (F("Static IP configuration done:"));
      //Serial.print (F("- IP: "));
      //Serial.println (netint.getIP ());
      //Serial.print (F("- Netmask: "));
      //Serial.println (netint.getNetmask ());
      //Serial.print (F("- Default Gateway: "));
      //Serial.println (netint.getGateway ());

      //Serial.print (F("Initializing SD card..."));
    if (!webserver.begin (netint, NULL, tags, SD_SS)) {
      //Serial.println (F(" failed"));
    }
   // Serial.println (F(" done"));
  }
  */
}

void loop() {
  resetArduino();
  
/////////RECEIVING DATA
  network.update();                  // Check the network regularly

  bool timeout = false;
  while ( !network.available() && !timeout)
      timeout = true;
      
  radioLink = !timeout;

  if ( timeout )
    Serial.println(F("0"));//Failed, response timed out.
  else{
    RF24NetworkHeader header;        // If so, grab it and print it out
    network.read(header, &radioRx, sizeof(radioRx));
    Serial.println(F("1")); //RECEIVED DATA
  }

////////SENDING
  //Serial.print("Sending...");
  RF24NetworkHeader header2( other_node);
  bool o = network.write(header2, &resetTx, sizeof(resetTx));
  Serial.println(o);
  
/*
  bool rx=true;
  unsigned long started_waiting_at = millis();
  bool timeout = false;
  while ( ! radio.available() && ! timeout )
    if (millis() - started_waiting_at > 5000 )
      timeout = true;

radioLink = !timeout;

  if ( timeout )
    Serial.println(F("Failed, response timed out."));
  else
    radio.read( &radioRx, sizeof(radioRx) );

    radio.stopListening();
    radio.write( &rx, sizeof(rx) );
    radio.startListening();
*/
 
  readDisplay();
  tft.drawPixel(50, 53, ST7735_WHITE);
  readPressure();
  tft.drawPixel(53, 55, ST7735_WHITE);
  if (radioLink){
    getTime();//prendo data attuale dal valore time int ricevuto da radio
    getDateTime();
    //getLastSdTx();
    if (onetime) {
      giornoSalvato = d.Day;
      meseSalvato = d.Month;
      annoSalvato = d.Year+1970;
      readEstremi();
      onetime = 0;
    }
    RestartEstremi();
    tft.drawPixel(56, 53, ST7735_WHITE);
    readTemp ();
    tft.drawPixel(59, 55, ST7735_WHITE);
    readHum ();
    tft.drawPixel(62, 53, ST7735_WHITE);
    readWind ();
    tft.drawPixel(65, 55, ST7735_WHITE);
    readWindDir();
    tft.drawPixel(68, 53, ST7735_WHITE);
    readPioggia();
    tft.drawPixel(71, 55, ST7735_WHITE);
    readDewPoint();
    tft.drawPixel(74, 53, ST7735_WHITE);
    readWindchill();
    tft.drawPixel(77, 55, ST7735_WHITE);
    readHeatIndex();
    tft.drawPixel(80, 53, ST7735_WHITE);
    
    //DatalogWeb();
    //tft.drawPixel(83, 55, ST7735_BLUE);
    Datalog();
    tft.drawPixel(86, 53, ST7735_BLUE);
    if(readEstremiOk)
      writeEstremi();
  }
  tft.drawPixel(89, 55, ST7735_BLUE);
  //webserver.loop ();
  tft.drawPixel(92, 53, ST7735_RED);
////////////////////////////////////////////////////////

Serial.println(wsDate);
Serial.println(wsTime);
Serial.println(TP);
Serial.println(TPDHT);
Serial.println(TP2);
Serial.println(UR);
Serial.println(ur2);
Serial.println(dewPoint);
Serial.println(heatindexc);
Serial.println(PR);
Serial.println(SLPR);
Serial.println(wind.val);
Serial.println(wind.valmedio);
Serial.println(DayWind.max);
Serial.println(wind.dir);
Serial.println(wind.dirmedia);
Serial.println(wind.ang);
Serial.println(wind.angmedio);
Serial.println(windchill);
Serial.println(mmPioggiagiorno);
Serial.println(rainrate);
Serial.println(mediaRR);
Serial.println(DayRainRate.max);

Serial.println(DayTemp.max);
Serial.println(DayTemp.min);
Serial.println(DayHum.max);
Serial.println(DayHum.min);
Serial.println(DayPressureSlm.max);
Serial.println(DayPressureSlm.min);
Serial.println(wind.max);
Serial.println(rainrateMax);

Serial.println(previousThree);
Serial.println(previousSix);
Serial.println(tpChange);
Serial.println(urChange);
Serial.println(dpChange);
Serial.println(F("___"));
Serial.println(erbmp);
Serial.println(erradioRX);
Serial.println(erroreSD);
Serial.println(writeExtreme);
Serial.println(readEstremiOk);
Serial.println(float(radioRx.volt)/10, 1);
Serial.println(F("______"));
}
