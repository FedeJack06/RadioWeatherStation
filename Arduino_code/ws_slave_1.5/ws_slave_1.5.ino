/*//////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////                                                                     ///////////////
  //////////////////////////          Weather station by Fedejack - SLAVE 1.3                    //////////////
  //////////////////////////                                                                     ///////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////
*/
//////////////////////////////////////LIBRERIE E DEFINIZIONI////////////////////////////////////////////////////
#include <SPI.h>
#include <Wire.h>
#include <avr/wdt.h>
#include <TimeLib.h>

////////////////////////////////////////PRIMA DI INIZIARE
bool INIZIALIZZAZIONE = 0;   // mettere 1 al posto di 0 se é la prima volta che si esegue il programma oppure se si é resettato tutto, avvia i processi che devono essere eseguiti una volta
float CALIBRATION = 3070; //3/6/21 da meteomin.it 3265 taratura per calcolo pressione sul livello del mare, differenza (in Pa) fra pressione slm attuale e pressione letta dal bmp180

//DEWPOINT HEATINDEX
#include "DHT.h"
#define DHTTYPE DHT22
DHT dht(1, DHTTYPE);

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

//Radio
#include <RF24Network.h>
#include <RF24.h>
RF24 radio(41,39);
//const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL }; se non si usa rf24network
RF24Network network(radio);      // Network uses that radio
const uint16_t this_node = 01;    // Address of our node in Octal format ( 04,031, etc)
const uint16_t other_node = 00;   // Address of the other node in Octal format

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////VARIABILI///////////////////////////////////////////////////////
/////////////////////////////////TEMPERATURA//////////////////////
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
char andamentoTp;// = < >
float tpChange;//ANDAMENTO

//medie in intervallo
unsigned int mTp, mTpdht, mTp2, count;//somma medie delle temperature e contatore
float TP, TPDHT, TP2;//medie delle temperature
float tpmax=-100, tpmin=100;

///////////////////////////////////UMIDITÁ//////////////////////////
float ur; // variabile dell'umidità
byte ur2;

//andamento
byte ur10min=0, ur5min=0, urnow=0;
unsigned long intAndamentoUr=0;
char andamentoUr;// = < >
int urChange;

//medie in intervallo
unsigned int mUr;//somma medie delle temperature e contatore
float UR;//medie delle temperature
float urmax=0, urmin=100;

//////////////////////////////////DEW POINT////////////////////////
float dewPoint; // variabile del punto di rugiada

//andamento
int dp10min=0, dp5min=0, dpnow=0;
unsigned long intAndamentoDp=0;
char andamentoDp;
int dpChange;

////////////////////////////////PRESSIONE///////////////////////////
float pressionelivellodelmare;// variabile per la pressione slm in Pa
float pressione;// pressione bmp180

//andamento
unsigned long intPressure = 0; //intervallo per salvataggio orario della pressione
float previousThree, previousSix; //differenza pressione precedente 3 6 ore
float pNow, pOne, pTwo, pThree, pFour, pFive, pSix; //pressioni precenti di 0 fino a 6 ore fa

//medie in intervallo
int countPr=0;
unsigned long mPr=0;
float PR, SLPR;//medie
float pressionemax=0, pressionemin=200000, pressioneSlmMax, pressioneSlmMin;

////////////////////////////////PIOGGIA/////////////////////////////
float mmPioggia = 0.0; //conteggio mm pioggia in corso
float mmPioggiagiorno; // mm pioggia cumulata nel giorno
float mmPioggiamese; // conteggio mm pioggia mese
float mmPioggiaanno; // conteggio mm pioggia anno
float prevPioggia; // variabile per incremetare i conteggi precedenti
float rainrate = 0; // variabile per l'intensità della pioggia

//media rainrate
float mediaRR;
float sommaRR = 0; //somma rainrate misurati nell'intervallo
float rainrateMax = 0; //rainrate massimo nell'intervallo media

////////////////////////////////VARIABILI ANEMOMETRO E DIREZIONE///////
struct WindValue { 
  float val; //velocità vento m/s
  float valmedio; //velocità media vento
  float raffica; //raffica massima giornaliera rilevata dal master
  float max; //velocitá massima nell'intervallo della media
  char dir[4]; //direzione del vento
  int ang; //direzione del vento in gradi
  int angmedio;
  char dirmedia[4]; //direzione vento media 10'
  char err[3]; //errore direzione vento
};
WindValue wind;

//media velocitá vento
float sommaWind = 0;//somma delle velocità per la media

//direzione
char direzioni[16][4] = {"NNE","NE","ENE","E","ESE","SE","SSE","S","SSO","SO","OSO","O","ONO","NO","NNO","N"};
int gradi[] = {22,45,67,90,112,135,157,180,202,225,247,270,292,315,337,0};
char erdir[5][3] = {"1","eE","eS","eO","eN"};
//media
int mediaDir[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};//array della moda direzione del vento
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
//ultima data ricezione dati radio
int Year,Month,Day,Hour,Minute,Second;
tmElements_t d;

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
TimedValue DayWind;
TimedValue DayChill; 
TimedValue DayHeatIndex; 
TimedValue DayRainRate; 

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
  int windval;
  int raffica; //raffica giornaliera massima
  int tp2;

  byte erWinddir; //errore direzione vento
  bool ersd; //errore inizializzazione sd master
  //long lastDatalog; //ultima ora salvataggio datalog Day
  byte volt;
  bool reset;
};
radioVal radioRx;   // 26/32 byte occuped

//////////////INIZIALIZZAZIONI/////////////////////////////////////////////////////
//SD
void initSD() {
  //evita conflitto SPI tra nrf24 e ethernet shield  
  pinMode(4, OUTPUT);
  pinMode(10, OUTPUT);
  digitalWrite(4, HIGH);
  digitalWrite(10, HIGH);
  
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
                  >> DayDewPoint.max >> DayDewPoint.timemax >> DayDewPoint.min >> DayDewPoint.timemin
                  >> DayWind.max >> DayWind.timemax
                  >> DayChill.min >> DayChill.timemin
                  >> DayHeatIndex.max >> DayHeatIndex.timemax
                  >> DayRainRate.max >> DayRainRate.timemax
                  
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

void initEstremi(){
  if (INIZIALIZZAZIONE) {
    pNow = pressione;
    pSix = pressione;
    pFive = pressione;
    pFour = pressione;
    pThree = pressione;
    pTwo = pressione;
    pOne = pressione;

    DayTemp.max = -100; //temperatura massima giornaliera
    DayTemp.min = 100; //temperatura minima giornaliera
    DayHum.max = -2; // umidità massima giornaliera
    DayHum.min = 150; // umidità minima giornaliera
    DayDewPoint.max = -100; // punto di rugiada massimo giornaliero
    DayDewPoint.min = 100; // punto di rugiada minimo giornaliero
    DayWind.max = 0; // raffica massima giornaliera
    DayChill.min = 100; // raffreddamento da vento minimo giornaliero
    DayHeatIndex.max = -100; // indice di calore massimo giornaliero
    DayRainRate.max = 0; // intensità della pioggia massima giornaliero
  }
}

void initTFT(){
  tft.initR(INITR_BLACKTAB);// inizializzazione display
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLUE);
  tft.setCursor (10, 20);
  tft.setTextSize(2);
  tft.setTextColor (ST7735_GREEN);
  tft.print (F(" STAZIONE"));
  tft.setCursor (15, 40);
  tft.print (F("  METEO:"));
  delay (100);
  tft.setTextColor (ST7735_YELLOW);
  tft.setCursor (5, 60);
  tft.print (F(" SITUAZIONE    ATTUALE"));
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
  
  //MEDIA 1 min, reset media dopo scrittura su dataday 
  mTpdht += int(tpdht*10);
  mTp2 += int(tp2*10);
  TP = float(mTp/count)/10;
  TPDHT = float(mTpdht/count)/10;
  TP2 = float(mTp2/count)/10;

  //ESTREMI in 1 min
  if (tp > tpmax)
    tpmax = tp;
  if (tp < tpmin)
    tpmin = tp;
  
  //ANDAMENTO 5' e 10'
  if (millis() - intAndamentoTp > 300000) {
    tp10min = tp5min;
    tp5min = tpnow;
    tpnow = int(tp*10);
    intAndamentoTp = millis();
  }

  if (abs(tp*10 - tp10min) > 50)
    andamentoTp = ' ';
  else if (tp*10 - tp10min > 3)
    andamentoTp = '+';
  else if (tp*10 - tp10min < -3)
    andamentoTp = '-';
  else
    andamentoTp = '=';

  if (millis() > 900000)
    tpChange = tp - (float(tp10min)/10);//andamento orario
    
  //MEDIA GIORNALIERA di ogni dato ricevuto
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
}

//UMIDITA' //////////////////////////
void readHum () {
  ur = float(radioRx.hum)/10;
  ur2 = radioRx.hum2;

  //MEDIA 1 min 
  mUr += int(ur*10);
  UR = float(mUr/count)/10;

  //estremi 1 min
  if (ur > urmax)
    urmax = ur;
  if (ur < urmin)
    urmin = ur;

  //ANDAMENTO
  if (millis() - intAndamentoUr > 300000) {
    ur10min = ur5min;
    ur5min = urnow;
    urnow = ur;
    intAndamentoUr = millis();
  }

  if (abs(ur - ur10min) > 20)
    andamentoUr = ' ';
  else if (ur - ur10min > 2)
    andamentoUr = '+';
  else if (ur - ur10min < -2)
    andamentoUr = '-';
  else
    andamentoUr = '=';

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
}

////////////////PRESSIONE E I SUOI ESTREMI/////////////////////////
void readPressure () {
  pressione = barometer.readPressure();
  pressionelivellodelmare = pressione + CALIBRATION;

  previousThree = pressione - pThree;
  previousSix = pressione - pSix;

  //MEDIA
  mPr += pressione;
  countPr++;
  PR = mPr/countPr;
  SLPR = PR+CALIBRATION;

  //ESTREMI in 1 min
  if (pressione > pressionemax){
    pressionemax = pressione;
    pressioneSlmMax = pressionelivellodelmare;
  }
  if (pressione < pressionemin){
    pressionemin = pressione;
    pressioneSlmMin = pressionelivellodelmare;
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
     pNow = pressione; //pressione attuale
  }
}

//VELOCITA' DEL VENTO
void readWind () {    
  wind.val = float(radioRx.windval)/10;
  wind.raffica = float(radioRx.raffica)/10;

  //MEDIA
  sommaWind += wind.val;
  wind.valmedio = sommaWind/count;

  //valore massimo nell'intervallo media
  if (wind.val > wind.max)
    wind.max = wind.val;

  //ESTREMI GG
  if (wind.raffica > DayWind.max) {
    DayWind.max = wind.raffica;
    DayWind.timemax = radioRx.data;
  }
}

//DIREZIONE DEL VENTO
void readWindDir () {
  strcpy(wind.dir, direzioni[radioRx.winddir]);//wind.dir = direzioni[radioRx.winddir];
  strcpy(wind.err, erdir[radioRx.erWinddir]);//wind.err = erdir[radioRx.erWinddir];
  wind.ang = gradi[radioRx.winddir];
  
  //media 5'
  mediaDir[radioRx.winddir]++; //add 1 at moda array
  //CALCOLO MODA della direzione
  for (int f=0; f<16; f++){
    if (mediaDir[f] > maxDir){//controllo quale valore direzione si é ripetuto di piú confrontando i valori dell'array mediaDir
      maxDir = mediaDir[f];
      strcpy(wind.dirmedia, direzioni[f]);//variabile direzione piú frequente in 10'
      wind.angmedio = gradi[f];
    }
  }
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

  //media 1 min
  sommaRR += rainrate;
  mediaRR = sommaRR/count;

  //rainrate massimo nell'intervallo della media
  if (rainrate > rainrateMax)
    rainrateMax = rainrate;
  
  //ESTREMI
  if (rainrate > DayRainRate.max){
    DayRainRate.max = rainrate;
    DayRainRate.timemax = radioRx.data;
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
    andamentoDp = ' ';
  else if (dewPoint*10 - dp10min > 3)
    andamentoDp = '+';
  else if (dewPoint*10 - dp10min < -3)
    andamentoDp = '-';
  else
    andamentoDp = '=';
//andamento orario
  if (millis() > 900000)
    dpChange = dewPoint - (float(dp10min)/10);

//ESTREMI
  if (dewPoint > DayDewPoint.max)
  {
    DayDewPoint.max = dewPoint;
    DayDewPoint.timemax = radioRx.data;
  }
  if (dewPoint < DayDewPoint.min)
  {
    DayDewPoint.min = dewPoint;
    DayDewPoint.timemin = radioRx.data;
  }
}

//RAFFREDDAMENTO DEL VENTO E I SUOI ESTREMI//////////
void readWindchill () {
  windchill = (13.12 + 0.6215 * tp) - (11.37 * pow(wind.val*3.6, 0.16)) + (0.3965 * tp * pow(wind.val*3.6, 0.16));//equazione per il calcolo del windchill

//correzione errori
  if ((windchill < tp) && (wind.val*3.6 > 4.6))
  {
    windchill = (13.12 + 0.6215 * tp) - (11.37 * pow(wind.val*3.6, 0.16)) + (0.3965 * tp * pow(wind.val*3.6, 0.16));
  }
  else
  {
    windchill = tp;
  }

//ESTREMI
  if (windchill < DayChill.min)
  {
    DayChill.min = windchill;
    DayChill.timemin = radioRx.data;
  }
}

//INDICE DI CALORE E I SUOI ESTREMI/////////////////
void readHeatIndex() {
  int hu = ur;
  float hic = dht.computeHeatIndex(tp, hu, false);//questo calcolo dell'indice di calore viene fornito dalla libreria 'dht'
  heatindexc = hic;

//ESTREMI
  if (heatindexc > DayHeatIndex.max)
  {
    DayHeatIndex.max = heatindexc;
    DayHeatIndex.timemax = radioRx.data;
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
    tft.print(Hour);
    tft.print(":");
    tft.print(Minute);
    tft.print(":");
    tft.print(Second);
    tft.print("-");
    tft.print(Day);
    tft.print("/");
    tft.print(Month);
    tft.drawLine(118,116,118,126,ST7735_RED);
    tft.setCursor(120,118);
    tft.setTextColor (ST7735_WHITE);
    tft.print(barometer.readTemperature(),1);
    tft.print ((char)248);
    tft.print(F("C"));
  
}
//RESET ESTREMI AL TERMINE DI GIORNO, MESE O ANNO//
bool eseguiResetDay = false;

void RestartEstremi() {
  if (d.Hour == 0 && eseguiResetDay == true){ //if status 0
    eseguiResetDay = false; //status 1

    mmPioggiagiorno = 0;
    DayWind.max = wind.val;
    DayTemp.max = tp;
    DayTemp.min = tp;
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
  }
  if (d.Hour != 0 && eseguiResetDay == false){ //if status 1
    eseguiResetDay = true; //status 0
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

      ofstream dataday ("dataDay.csv", ios_base::app);

      dataday << int(d.Year+1970) << "-" << int(d.Month) << "-" << int(d.Day) << " " << int(d.Hour) << ":" << int(d.Minute) << " " << TP << " " << TPDHT << " " << TP2 << " " << UR << " "
      << wind.valmedio << " " << wind.angmedio << " " << mmPioggiagiorno << " " << mediaRR  << " " 
      << tpmax << " " << tpmin << " " << urmax << " " << urmin << " " << wind.max << " " << rainrateMax << " " 
      << mediaTp << " " << SLPR << " " << pressionemax << " " << pressionemin << " " << pressioneSlmMax << " " << pressioneSlmMin << endl;

      dataday.close();

      /*String dataString = String(d.Year+1970) + "/" + String(d.Month) + "/" + String(d.Day) + " " + String(d.Hour) + ":" + String(d.Minute)+ " " + String(TP, 1) + " " + String(TPDHT, 1) + " " +
                          String(UR) + " " + String(dewPoint, 1) + " " + String(heatindexc, 1) + " " + String(SLPR/100, 1) + " " + String(wind.valmedio*3.6, 1) + " " + String(wind.max*3.6, 1) 
                          + " " + wind.dirmedia + " " + String(windchill, 1) + " " + String(mmPioggiagiorno, 1) + " " + String(mediaRR, 1) + " " + String(rainrateMax, 1) + " " + String(TP2, 1);

      File logFile = SD.open("dataDay.csv", FILE_WRITE);
      if (logFile){
        logFile.println(dataString);
        logFile.close();
        //Serial.println(F("written dataDay.csv"));
      }*/

      //reset medie 1 min
      count = 0;
      mTp  = 0;
      mTpdht  = 0;
      mTp2  = 0;
      mUr  = 0;
      mPr  = 0;
      sommaWind = 0;
      sommaRR = 0;
      for (int f=0; f<16; f++){
        mediaDir[f] = 0;//azzero array della frequenza direzione del vento
      }
      maxDir = 0;//azzero indice di direzione che si ripete più volte nell'intervallo

      //reset estremi temporanei dopo averli scritti
      wind.max = 0;
      rainrateMax = 0;
    }
  }
}

void writeEstremi () {
  if (SD.begin(SD_SS, SPI_HALF_SPEED)) {
    ofstream estremiOut ("estremi.dat");
  
    estremiOut << DayTemp.max << " " << DayTemp.timemax << " " << DayTemp.min << " " << DayTemp.timemin << endl
    << DayHum.max << " " << DayHum.timemax << " " << DayHum.min << " " << DayHum.timemin << endl
    << DayDewPoint.max << " " << DayDewPoint.timemax << " " << DayDewPoint.min << " " << DayDewPoint.timemin << endl
    << DayWind.max << " " << DayWind.timemax << endl
    << DayChill.min << " " << DayChill.timemin << endl
    << DayHeatIndex.max << " " << DayHeatIndex.timemax << endl
    << DayRainRate.max << " " << DayRainRate.timemax << endl
    
    << pOne << " " << pTwo << " " << pThree << " " << pFour << " " << pFive << " " << pSix << endl
    << mmPioggia << " " << mmPioggiagiorno << " " << mmPioggiamese << " " << mmPioggiaanno << endl
    << sommaTp << " " << numberTp << endl;
   
    //if (!estremiOut) Serial.println (F("error write estremi"));
  
    estremiOut.close();
  }
} 

void getTime() {
  breakTime (radioRx.data, d);//usare d.Hour d.Minute d.Second d.Day d.Month d.Year per registrare valori orari
  //lastRadioRx = String(d.Hour) + ":" + String(d.Minute) + ":" + String(d.Second) + "-" + String(d.Day) + "/" + String(d.Month);
  Year = d.Year+1970;
  Month = d.Month;
  Day = d.Day;
  Hour = d.Hour;
  Minute = d.Minute;
  Second = d.Second;
}

//reset da remoto
void(* resetFunc) (void) = 0; 
int valButtonOld = LOW;
void resetArduino() {
  if(radioRx.reset == true){ //se reset remoto a buon fine, non invio piu comando di reset remoto
    resetTx = false;
  }

  int valButton = digitalRead(A4);
  if (digitalRead(A4) == HIGH && valButtonOld == LOW)
    resetTx = !resetTx;

  int valButtonOld = valButton;
}

void setPin(){
  pinMode(41, OUTPUT);
  pinMode(39, OUTPUT);
  digitalWrite(41, HIGH);
  digitalWrite(39, HIGH);
}

void radioRX(){
  //RECEIVING DATA
  network.update();                  // Check the network regularly

  bool timeout = false;
  while ( !network.available() && !timeout)
      timeout = true;
      
  radioLink = !timeout;

  if ( timeout )
    Serial.println(F("%0"));//Failed, response timed out.
  else{
    RF24NetworkHeader header;        // If so, grab it and print it out
    network.read(header, &radioRx, sizeof(radioRx));
    Serial.println(F("%1")); //RECEIVED DATA
  }

  //SENDING
  //Serial.print("Sending...");
  RF24NetworkHeader header2( other_node);
  bool o = network.write(header2, &resetTx, sizeof(resetTx));
  Serial.println(o);
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {  
  Serial.begin(9600);
  while (!Serial){
    ;
  }
  Wire.begin();
  digitalWrite(SDA, 0);//disable internal i2c pull up
  digitalWrite(SCL, 0);
  SPI.begin();
  wdt_enable(WDTO_8S);//watchdog a 8 secondi
  
  setPin();
  initEstremi();
  initTFT();
  initSD();
  initRadio();
  initPressure();
  readEstremi();
}

void loop() {
  delay(300);
  resetArduino();
  radioRX();
  wdt_reset();
  readDisplay();
  tft.drawPixel(50, 53, ST7735_WHITE);
  readPressure();
  tft.drawPixel(53, 55, ST7735_WHITE);
  if (radioLink){
    count++; //conteggio loop per medie
    getTime();//prendo data attuale dal valore time int ricevuto da radio
    if (onetime) {//funzioni da eseguire solo al primo loop, dopo aver ricevuto  i dati
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
    Datalog();
    tft.drawPixel(86, 53, ST7735_BLUE);
    if(readEstremiOk)
      writeEstremi();
  }
  tft.drawPixel(89, 55, ST7735_RED);
  wdt_reset();
////////////////////////////////////////////////////////
  //dati da mediare su python
  if (tp == -45) //sensor not connected
    Serial.println(999);
  else
    Serial.println(tp);
  if (tp2 == -127){
    Serial.println(999);
    Serial.println(999);
  }
  else{
    Serial.println(tpdht);
    Serial.println(tp2);
  }
  if (ur == 0)
    Serial.println(999);
  else
    Serial.println(ur);
  if (ur <= 0)
    Serial.println(999);
  else
    Serial.println(ur2);
  Serial.println(pressione);
  Serial.println(pressionelivellodelmare);
  Serial.println(wind.val);
  Serial.println(rainrate);
  //dati singoli
  Serial.print(Year);
  Serial.print(F("-"));
  Serial.print(Month);
  Serial.print(F("-"));
  Serial.println(Day);
  Serial.print(Hour);
  Serial.print(F(":"));
  Serial.print(Minute);
  Serial.print(F(":"));
  Serial.println(Second);

  Serial.println(radioRx.winddir);//invio index of dir array
  Serial.println(mmPioggiagiorno);
  Serial.println(mediaTp);
  Serial.println(DayWind.max);
  Serial.println(DayRainRate.max);
  
  Serial.println(erbmp);
  Serial.println(erradioRX);
  Serial.println(erroreSD);
  Serial.println(float(radioRx.volt)/10, 1);
  delay(300);
}
