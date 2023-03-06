/*//////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////                                                                     ///////////////
  //////////////////////////          Weather station by Fedejack - MASTER 1.5                   ///////////////
  //////////////////////////                                                                     ///////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////
*/
//////////////////////////////////////LIBRERIE E DEFINIZIONI////////////////////////////////////////////////////
#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <avr/wdt.h>
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

#define DHTPIN 38 // pin digitale a cui è collegato il DHT22
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

//SHT35
#include "SHT31.h"
SHT31 sht35;

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

//Radio
RF24 radio(A12,A13); //CE E CSN
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

//medie in intervallo tp
unsigned int mTp, mTpdht, mTp2, count;//somma medie delle temperature e contatore
float TP, TPDHT, TP2;//medie delle temperature
float tpmax=-100, tpmin=100;

//medie in intervallo ur
unsigned int mUr;//somma medie delle temperature e contatore
float UR;//medie delle temperature
float urmax=0, urmin=100;

/////////////////////////////////////VARIABILI PIOGGIA///////////////////////////////////////////////////
const float mmGoccia = 0.3; // constante del valore di ogni basculata del pluviometro 
volatile float mmPioggia = 0; // variabile conteggio mm pioggia giorno
volatile unsigned long prevRainInterrupt = 0;
float previousPioggia = 0;
float rainrate = 0; // variabile per l'intensità della pioggia
volatile unsigned long PluvioStep = 0; // variabili necessarie da utilizzare con millis() per calcolare l'intensità della pioggia
volatile unsigned long PluvioOldStep = 0; // variabili necessarie da utilizzare con millis() per calcolare l'intensità della pioggia
volatile unsigned long time1;

//media rainrate
float mediaRR;
float sommaRR = 0; //somma rainrate misurati nell'intervallo
float rainrateMax = 0; //rainrate massimo nell'intervallo media

////////////////////////////////VARIABILI ANEMOMETRO E DIREZIONE/////////////////////////////////////
const float Pi = 3.141593; // Pigreco
const float raggio = 0.055; // raggio dell'anemometro in metri
volatile unsigned long Conteggio = 0;// variabile che contiene il conteggio delle pulsazioni          
//volatile unsigned long Tempo = 0; // per conteggiare il tempo trascorso dalla prma pulsazione
//volatile unsigned long tempoPrec = 0;
//volatile float deltaT;
volatile unsigned long prevWindInterrupt = 0; //antidebouncing e max valore 311 km/h
volatile float wind4sec = 0; //vento in m/s calcolato su un periodo di 4 secondi

//media velocitá vento
float sommaWind = 0;//somma delle velocità per la media

int analogEst;//valori di input della banderuola
int analogSud;
int analogWest;
int analogNord;

//media direzione vento 
// ogni misurazione viene incrementato di 1 l'array mediaDir nella posizione corrispondente alla direzione (vedi array direzioni)
//in modo da segnare quale valori direzione viene misurato piú volte (moda), si elimina l'errore di piccole oscillazione della banderuola
int mediaDir[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};//array della moda direzione del vento
int maxDir = 0;//indice di direzione che si ripete più volte nell'intervallo
//char direzioni[16][4] = {"NNE","NE","ENE","E","ESE","SE","SSE","S","SSO","SO","OSO","O","ONO","NO","NNO","N"};
//char erdir[5][3] = {"1","eE","eS","eO","eN"};//array di errore se banderuola da valori analogici errati o se scollegati fili input
int gradi[] = {22,45,67,90,112,135,157,180,202,225,247,270,292,315,337,0};

//STRUCT PER VALORE DEL VENTO E DIREZIONE
struct WindValue { 
  float val; //velocità vento m/s
  float valmedio; //velocità media vento 
  int dir; //indice array direzione del vento
  int dirmedia; //indice array direzione vento media 
  int err; //indice errore direzione vento
  float max; //valore massimo nell'intervallo della media
  float raffica; //raffica massima giornaliera
};
WindValue wind;

//TEMPORIZZAZIONI
unsigned long previousMillis = 0; //necessaria per salvataggio sd
const long interval = 300000; //ogni 5 minuti salvataggio dati Day in file csv
byte giornoSalvato; // variabile per memorizzare lo stato del giorno attuale

String lastRadiotx; //ultima data invio dati radio
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
  int tp; //temperatura sht35
  int tpdht; //temperatura dht22
  int hum; //umiditá
  byte hum2; //umidita dht22
  int pioggiaday; //pioggia del giorno
  int rainrate; //intensitá di pioggia
  byte winddir; //indice direzione vento, da usare per estrarre stringhe da Direzioni[]
  int windval; //velocitá vento m/s
  int raffica; //raffica giornaliera massima
  int tp2; //temp ds18b20
  
  byte erWinddir; //errore direzione vento
  bool ersd; //errore inizializzazione sd
  //long lastDatalog; //ultima ora salvataggio datalog Day
  byte volt; //tensione batteria alimetazione
  bool reset; //status reset a buon fine
  // 26/32 byte occuped in nRF24
};
radioVal radioTx;

/////////////////////DICHIARAZIONI INDIRIZZI EEPROM///////////////////////////////////////////
//N.B. Sono distanziati tra di loro in modo di aver spazio nella loro memorizzazione in modo pulito
//ACCUMULO PIOGGIA
const unsigned int eeAddress1 = 2;

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
/* 
  METODO CALCOLO MISURANDO IL PERIODO DI UNA ROTAZIONE E PRENDENDO IL VALORE CHE "PASSA" DURANTE IL LOOP
  void windCount() {
    Tempo = millis();
    deltaT = float( Tempo - tempoPrec)/1000; // si conteggia il tempo trascorso fra un interrupt e un altro, poi si prendera l'ultimo valore quando passa il loop la funzione di calcolo del vento
    tempoPrec = Tempo;
}
*/

void windCount() {
  if( millis() - prevWindInterrupt > 4){ //anti debounce: max velocità instantanea = 86,39 m/s = 311,01 km/h
    Conteggio++;
    prevWindInterrupt = millis();
  }
}

ISR(TIMER1_COMPA_vect){ //funzione chiamata ogni 4 secondi
  wind4sec = (2*Pi*raggio*Conteggio)/4;
  Conteggio = 0;
}

void setWindClock(){
  //set timer 1
  TCCR1A = 0; //timer CTC mode
  TCCR1B = 0b00001101; //prescaler a 1024, (Da 16Mhz - 62,5 ns a 64 micro sec)
/*
 * Presclaer 001 010 011  100  101
 *            1   8   64  256  1024 
 * Timer Count = delay richiesto / periodo prescaled - 1
 * 
 */
  TCNT1 = 0; //contatore settato a zero
  TIMSK1 = 0b00000010; //set contatore uguale a OCR1A fa interrrupt

  OCR1A = 62500-1; //max valore del conteggio prima di fare overflow e attivare ISR
}

//INTENSITA' DELLA PIOGGIA//////////////////////////////////////////////////////////////////////////
void PluvioDataEngine() {
  if (((PluvioStep - PluvioOldStep) != 0)) {
    if ((millis() - PluvioStep) > (PluvioStep - PluvioOldStep)) {
      rainrate = 3600 / (((millis() - PluvioStep) / 1000)) * mmGoccia;
      if (rainrate < 1) {
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
void ContaGocce(){// con pluviometro deAgostini ogni basculata attiva due volte l'interrupt quindi incremento di 0,05mm ogni interrupt
  if(millis() - prevRainInterrupt > 100){ //antidebaunce 1 secondo --> 1080 mm/h
    PluvioOldStep = PluvioStep;
    PluvioStep = time1;
    mmPioggia += mmGoccia;
    time1 = millis();
    prevRainInterrupt = millis();
  }
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
  //Serial.println(sht35.readStatus(), HEX);
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

  mTp += int(tp*10);
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
}
//UMIDITA'//////////////////////////
void readHum () {
  ur = sht35.getHumidity();
  radioTx.hum = int(ur*10);
  
  ur2 = floor(dht.readHumidity()); //umiditá dht22
  radioTx.hum2 = byte(ur2); //invio radio umiditá

  //MEDIA 1 min 
  mUr += int(ur*10);
  UR = float(mUr/count)/10;

  //estremi 1 min
  if (ur > urmax)
    urmax = ur;
  if (ur < urmin)
    urmin = ur;
}

//VELOCITA' DEL VENTO E I SUOI ESTREMI//
void readWind () {
/*DA USARE CON METODO VELOCITA ISTANTANEA, CALCOLO DELLA VELOCITA MISURANDO IL PERIODO DI UNA ROTAZIONE, SI PRENDE IL VALORE DI V CHE PASSA AL MOMENTO IN CUI SI ESEGUE readWind
  if (deltaT != 0)                   //se é trascorso tempo calcolo la velocitá    
    wind.val = (2*Pi*raggio)/deltaT; //si calcola la velocitá come velocitá angolare diviso delta tempo

  else if (deltaT ==0 || deltaT > 2500){ //se non é trascorso tempo oppure velocitá minore 0.5 km/h
    wind.val = 0;
  }
*/
  wind.val = wind4sec; //misura velocità contando rotazioni in 4 secondi

  radioTx.windval = int(wind.val*10); //invio velocitá via radio

  if (wind.val > wind.raffica) {
    wind.raffica = wind.val;
    radioTx.raffica = int(wind.val*10);
  }

  //MEDIA
  sommaWind += wind.val;
  wind.valmedio = sommaWind/count;

  //valore massimo nell'intervallo media
  if (wind.val > wind.max)
    wind.max = wind.val;
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

//CALCOLO MODA della direzione
  for (int f=0; f<16; f++){
    if (mediaDir[f] > maxDir){//controllo quale valore direzione si é ripetuto di piú confrontando i valori dell'array mediaDir
      maxDir = mediaDir[f];
      wind.dirmedia = f;//variabile direzione piú frequente in 10'
    }
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

  //media 1 min
  sommaRR += rainrate;
  mediaRR = sommaRR/count;

  //rainrate massimo nell'intervallo della media
  if (rainrate > rainrateMax)
    rainrateMax = rainrate;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////ALTRE FUNZIONI///////////////////////////////////////////////////////

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

      ofstream dataday ("dataDay.csv", ios_base::app);

      dataday << int(year()) << "-" << int(month()) << "-" << int(day()) << " " << int(hour()) << ":" << int(minute()) << " " << TP << " " << TPDHT << " " << TP2 << " " << UR << " "
      << wind.valmedio << " " << gradi[wind.dirmedia] << " " << mmPioggia << " " << mediaRR  << " " << tpmax << " " << tpmin << " " << urmax << " " << urmin << " " << wind.max << " " << rainrateMax << endl;

      dataday.close();

      //reset medie 1 min
      count = 0;
      mTp  = 0;
      mTpdht  = 0;
      mTp2  = 0;
      mUr  = 0;
      sommaWind = 0;
      sommaRR = 0;
      for (int f=0; f<16; f++){
        mediaDir[f] = 0;//azzero array della frequenza direzione del vento
      }
      maxDir = 0;//azzero indice di direzione che si ripete più volte nell'intervallo
  
      wind.max = 0;
      rainrateMax = 0;

      Serial.println(F("written"));
      //radioTx.lastDatalog = long(now()); 
      //lastDatasd = String(hour()) + ":" + String(minute());
    }
  }
}


////////////////////Reset temporizzazioni alla mezzanotte per sincronizzare datalog ogni 10 min esatti
bool eseguiResetDay = false;

void resetTempo() {
  if (int(hour()) == 0 && eseguiResetDay == true) {
    eseguiResetDay = false;

    mmPioggia = 0;
    previousPioggia = 0;
    EEPROM.put(eeAddress1, 0.0);
    wind.raffica = 0; //reset raffica giornaliera
    previousMillis = 0; //sincronizzo memorizzazione datalog ogni 10 min
  }
  if (int(hour()) != 0 && eseguiResetDay == false){ //if status 1
    eseguiResetDay = true; //status 0
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
  digitalWrite(SDA, 0);//disable internal i2c pull up
  digitalWrite(SCL, 0);
  
  //abilito il watchdog e imposto come tempo di reser 2 secondi
  wdt_enable(WDTO_8S);

  pinMode(12, INPUT);
  pinMode(14, OUTPUT); //pin led lampeggio ad ogni loop
  digitalWrite(14, LOW);
  pinMode(15, OUTPUT); //pin led stato invio dati radio
  digitalWrite(15, LOW);

  setSyncProvider(getLocalTime); // si prende l'ora attuale dell'RTC
  giornoSalvato = day(); //si prende giorno mese anno attuali per vedere se resettare estremi

  wind.max = 0; //reset raffica massima nell'intervallo
  wind.raffica = 0; //reset raffica massima giornaliera
  
  setWindClock();
  Eeprom();
  initSD();
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
  rtc.adjust(DateTime(2022,11,14,23,8,0)); //(impostare -1 ora )
*/
}

void loop() {
  wdt_reset();
  count++;
  readTemp();
  readHum();
  readWind();
  readWindDir();
  readPioggia();
  Datalog();
  readVolt();
  resetTempo();
  statusReset();
  wdt_reset();

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
  wdt_reset();
  
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
  Serial.print(tpdht);
  Serial.println(" C  dht");
  Serial.print(ur);
  Serial.println(" %");
  Serial.print(gradi[wind.dir]);
  Serial.print(" media 10' ");
  Serial.println(gradi[wind.dirmedia]);
  Serial.print(wind.val);
  Serial.print(" m/s, max: ");
  Serial.println(wind.raffica*3.6);
  Serial.print(mmPioggia);
  Serial.println(" mm");
  Serial.print("Tens. batteria:  ");
  Serial.println(Vbatteria);
  Serial.println("_______");//per eventuali prove in Serial port
////////////////////////////////////////////////////////////////////////

  //lampeggio led al termine del loop
  digitalWrite(14, HIGH);
  delay(100);
  digitalWrite(14, LOW);
  wdt_reset();
}
