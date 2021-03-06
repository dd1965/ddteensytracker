#include "config.h"
#include <TinyGPS++.h>
#include <OneWire.h>
#include <DallasTemperature.h>
extern int sendMorse(char *msg,int Length);
//#include "ssdv.h"
#include "configGPS.h"
#include "rs8.h"
#include "camera.h"

#include <stdio.h>
#include <ByteBuffer.h>
#include <SPIFIFO.h>
#include <SPI.h>
#include <AD9850.h>
#include <EEPROM.h>
// This is a telemetry sender for 1200 baud created by VK3TBC.
// Features 1200 baud digital modulator & experimental 4800 baud modulator
// Sample rate is 84Khz to the audio DAC. 1023 point sine wave (10bit)
// Constructs a simple frame for Telemetry and sends it.
// Two digitial temperature sennsor
// A to D to monitor bat voltage
// GPS to receive location data
// DDS to transmit QRP on 30M 10.100Mhz using grid locator and CW as transmission mode.
// VK3TBC synchronous protocol with correlation tags for syncing, 256byte buffer with RS error correction and also with bit shuffling for noise error burst correction
// Added a buffer for audio samples that need to be transmitted.
// 
// Original licence condition kept as it is based on some work for DDS and timer use (Note:now, highly modified)
// RCArduino DDS Sinewave by RCArduino is licensed under a Creative Commons Attribution 3.0 Unported License.
// For helpful background information on Arduino Due Timer Configuration, refer to the following link
// thanks to Sebastian Vik
// http://arduino.cc/forum/index.php?action=post;topic=130423.15;num_replies=20
// For background information on the DDS Technique see
// http://interface.khm.de/index.php/lab/experiments/arduino-dds-sinewave-generator/
// For audio sketches making extensive use of DDS Techniques, search the RCArduino Blog
// for the tags Audio or synth




// use a simple filter on the DAC.

// Experimental 4800 baud raised cosine filter constants
uint16_t baud_4800_table[] ={
  119,348,624,937,1274,1623,1969,2296,2591,
  2845,3086,3286,3443,3552,3614,3626,3589,3504,3370,3191,2970,2724,2448,2135,1798,1449,1103,776,481,227,23,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3088,3072,3072,3072,3072,3072,3072,3072,3072,3072,
  3108,3157,3214,3272,3322,3356,3363,3333,3257,3132};


uint16_t baud_9600_table[] ={
  50,455,1011,1646,2278,2828,3237,3473,3510,3354,3024,2555,1990,1383,790,267,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3074,3011,3001,3030,3069,3095,3100,3090,3087,3096
};

// 4800 Baud interrupt handler functions
volatile byte previoussentbit = 0;
volatile int waveIndex=0;
volatile  uint32_t txshreg=0;
volatile byte transmiton=1;
volatile boolean timerstarted=false;
volatile uint32_t timer=0;
volatile uint16_t timer10ms=0;
volatile boolean send_morse=false;
volatile uint8_t cwstringlength=0;
ByteBuffer send_buffer;
//End 4800 baud

// the phase accumulator points to the current sample in our wavetable
uint32_t ulPhaseAccumulator = 0;
// the phase increment controls the rate at which we move through the wave table
// higher values = higher frequencies
volatile uint32_t ulPhaseIncrement = 0;   // 32 bit phase increment, see below
volatile int idle=0;
volatile int bufcnt=0;
volatile int bti = 0;
volatile int K=8;
volatile  byte inbytei;
int sendtone = 0;
String startHdr="$CQ DE ";
String QRP =" QRP Balloon ";
String ALTI =" Altitude ";
String GRID =" Locator ";
byte hdrbyte[4];
String test="VK3TBC-1,0000,00:00:00,0.0,0.0,0,0,0,0,0,0,0,Test";
String part2="VK3TBC-1,0000,";
String part3=",000,000,000,Test*";
String tempin ="000";
String tempout ="000";
String voltage = "000";
//String speedveld = "0,0,";
String separator = ",";
char *callsign ="VK3TBC";
//String callsign ="VK3TBC";
String seqnostr ="0000";
byte testStr[256];
byte eol[2];

short seq=0;
char seqimg=0;
char bufseq[32]; 
byte flags[80];
byte encodeArray[12];
byte crcbyte[4];
ushort crc;
String crchex;
String seqNumStr;
int led = 13;
//Interleaver
#define numbits 2048
#define numbytes = 256
byte symbols_interleaved[numbits];
byte symbols[numbits];
byte bitin;
byte cbyte;
volatile byte baud_mode=0;
// full waveform = 0 to SAMPLES_PER_CYCLE
// Phase Increment for 1 Hz =(SAMPLES_PER_CYCLE_FIXEDPOINT/SAMPLE_RATE) = 1Hz
// Phase Increment for frequency F = (SAMPLES_PER_CYCLE/SAMPLE_RATE)*F
#define SAMPLE_RATE 96000.0//84000
#define SAMPLES_PER_CYCLE 1020
#define SAMPLES_PER_CYCLE_FIXEDPOINT (SAMPLES_PER_CYCLE<<20)
#define TICKS_PER_CYCLE (float)((float)SAMPLES_PER_CYCLE_FIXEDPOINT/(float)SAMPLE_RATE)
#define TEN_SEC_TIMER 10*SAMPLE_RATE
#define tenmscount SAMPLE_RATE/50
uint32_t timer10 = TEN_SEC_TIMER;
// to represent 1020 we need 10 bits
// Our fixed point format will be 10P22 = 32 bits

// Create a table to hold the phase increments we need to generate tone frequencies at our 84Khz sample rate
#define No_of_tones 2
uint32_t toneRegister[No_of_tones];
//volatile boolean baud4800on=false;

//Define One Wire routines
#define ONE_WIRE_BUS 2
#define ONE_WIRE_BUS2 3
OneWire oneWire(ONE_WIRE_BUS);
OneWire oneWireo(ONE_WIRE_BUS2);
DeviceAddress insideThermometer;
DeviceAddress outsideThermometer;
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
DallasTemperature sensorso(&oneWireo);
// arrays to hold device address
char tbuf[32];
char cwstring[100];

//Setup for DDS
//AD9850 ad(7, 6, 5); // w_clk, fq_ud, d7
//int RESET = 4;


IntervalTimer smpltimer;
// fill the table with the phase increment values we require to generate the tone 1200Hz and 2200Hz
void createToneIndex()
{
  toneRegister[0] = 1200*TICKS_PER_CYCLE;
  toneRegister[1] = 2200*TICKS_PER_CYCLE;
}

// Create a table to hold pre computed sinewave, the table has a resolution of 600 samples
#define WAVE_SAMPLES 1020
// default int is 32 bit, in most cases its best to use uint32_t but for large arrays its better to use smaller
// data types if possible, here we are storing 12 bit samples in 16 bit ints
uint16_t nSineTable[WAVE_SAMPLES];

// create the individual samples for our sinewave table
void createSineTable()
{
  for(uint32_t nIndex = 0;nIndex < WAVE_SAMPLES;nIndex++)
  {
    // normalised to 12 bit range 0-4095
    nSineTable[nIndex] = (uint16_t)  (((1+sin(((2.0*PI)/WAVE_SAMPLES)*nIndex))*4095.0)/2);
    // Serial.println(nSineTable[nIndex]);
  }
}

void setup()
{
  //fdev_setup_stream (&uartout, usb_serial_putchar, NULL, 1);
  pinMode(led, OUTPUT);  
  digitalWrite(led, HIGH);
  delay(1000);
  digitalWrite(led, LOW);
  Serial.begin(9600);
  Serial.println(tenmscount);
  send_buffer.init(10240);
  encodeGold(0);
  for(int f=0;f<2;f++) eol[f]=0x0A;
  for(int f=0;f<4;f++) hdrbyte[f]=0x24;
  createToneIndex();
  createSineTable();
  for(int f=0;f<20;f++) flags[f]=0x7E;
  initCamera(); 

  // this is a cheat - enable the DAC
  // analogWrite(DAC1,0);
  analogWriteResolution(12);
  double intrate = 1.0/96000.0;
  Serial.println(intrate,8);
  smpltimer.begin(TC4_Handler,intrate); 
  /* memset(testStr, 0x7E, sizeof(testStr));
   for(int i=0;i<100;i++){
   sendbitdata(testStr,256);
   }*/
  initGPS();
  digitalWrite(led, HIGH);
  //Temperature Sensor
  sensors.begin();
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Inside Device 0"); 
  sensors.setResolution(12);
  // Serial.println(sensors.getAddress(insideThermometer,0),HEX);

  sensorso.begin();  
  if (!sensorso.getAddress(outsideThermometer, 0)) Serial.println("Unable to find address for Outside Device 1"); 
  sensorso.setResolution(12);
  // Serial.println(sensorso.getAddress(outsideThermometer,0),HEX);
  initSPI();
  // initDDS();
  Serial.println("Enter Call Sign (will take 6 characters only & does not validate call)");
  starttimer();
  char incomingbyte[]{
    0x20,0x20,0x20,0x20,0x20,0x20    };
  byte inputcnt=0;

  while(timerstarted){
    if(Serial.available()>0){
      incomingbyte[inputcnt]=Serial.read();

      if((incomingbyte[inputcnt]==0x0D)||(inputcnt==5)){
        for(int i=0;i<6;i++){
          EEPROM.write(i, incomingbyte[i]);
        }       
        Serial.println("Callsign stored ");       
        stoptimer();
        break;            
      }
      inputcnt++;

    }
  }
  String callsignin="VK3TBC";
  Serial.println("Welcome to the VK3TBC balloon payload program ");
  Serial.print("The working callsign is ");
  for(int i=0;i<6;i++){
    //Serial.print(EEPROM.read(i));
    callsignin[i]=(char)EEPROM.read(i);
  }
  Serial.println(callsignin);
  callsign  = new char[callsignin.length() + 1];
  strcpy( callsign , callsignin.c_str()); 
}

void loop()
{
  //baud4800on=true;
  if(!send_morse) {  
    getTemperature();
    voltage=getVoltage();
    send_morse=true;          
  }
  // send_morse=true;
  sendTelemetry();  
  // fprintf(stderr, "Test");
  takePicture();  
  // transmiton=0; 
  encodeImage(seqimg,callsign);
  while(send_buffer.getSize()!=0){
    ;
  }
  sendAPRSframe();

  while(send_buffer.getSize()!=0){

    ;
  }
  baud_mode++;
  if(baud_mode==3){
    baud_mode=0;
  }
  /* if(baud4800on){
   baud4800on=false;
   }else{
   baud4800on=true;
   }
   */

  seqimg++;


}
void sendTelemetry(){
  // while(send_buffer.getSize()!=0){;}
  memset(testStr, 0, sizeof(testStr));
  //sensors.requestTemperatures();
  // getTemperature();
  // Serial.println(tempout);
  //  voltage=getVoltage();
  // getGPSdata();
  starttimer();
  while(!getGPSdata()&&timerstarted);//||getage()!="1");//Wait for GPS data & add timer elapesed here.  
  stoptimer();
  test=callsign+seqnostr+getgpsTime()+getlat()+getlong()+getalt()+getspeed()+getnumsatellites()+getage()+tempin+tempout+voltage;
  // seqNumStr = String(seq++, DEC);

  seq++;
  sprintf(bufseq,"%04d:",seq);
  for(int i =0;i<4;i++){
    test[i+6]=bufseq[i];
  }
  if (seq==10000) seq=0;

  crc=rtty_CRC16_checksum(testStr,test.length());

  // crcbyte[0]=crc;
  // crcbyte[1]=(crc>>8);
  //Serial.println("CRC");
  // Serial.println(crc);
  crchex = String(crc,HEX);
  crchex.toUpperCase();
  if (crchex.length()<4){
    int ln = 4-crchex.length();
    for(int i=ln;i<4;i++){
      crcbyte[i]=crchex[i-ln];

    }
    for(int i=0;i<ln;i++){
      crcbyte[i]= 0x30;
    }
  }
  else{ 

    for(int i=0;i<4;i++){
      crcbyte[i]=crchex[i];
    }
  }



  memset(testStr, 0xAA, sizeof(testStr));

  testStr[0]=0x24;
  testStr[1]=0x24;
  for(int i=0;i<test.length();i++){
    testStr[i+2]=test[i];                 
  }
  //  Serial.println(test);
  encode_rs_8(&testStr[0], &testStr[223], 0);
  // sendbitdata(flags,sizeof(flags));

  if(baud_mode>0){
    sendbitdata(flags,50);
  }
  else{//Preamble /20
    sendbitdata(flags,20);
  }
  sendbitdata(encodeArray,12);//Goldcode

  // sendbitdata(hdrbyte,4);
  shuffle(&testStr[0]);
  sendbitdata(testStr,256); //Data

  sendbitdata(flags,5); //Tail

  test=startHdr+callsign+separator+seqnostr+separator+getgpsTime()+separator+getlat()+separator+getlong()+separator+getalt();
  sprintf(bufseq,"%04d:",seq);
  test=startHdr+callsign+separator+seqnostr+QRP+GRID+getGrid()+ALTI+getalt()+separator;

  for(int i =0;i<4;i++){
    //  test[i+14]=bufseq[i];
    test[i+14]=bufseq[i];
  }

  for(int i=0;i<test.length();i++){
    cwstring[i]=test[i];
  }
  cwstringlength=test.length();
  // Serial.print(test);
  // Serial.print(" ");
  //  Serial.println(cwstringlength);
  //  send_morse=true;
  // sendMorse(cwstring,56);
  //  while(send_buffer.getSize()!=0){;}
  //delay(2000);

}
void starttimer(){
  timerstarted=true;
}
void stoptimer(){
  timerstarted=false;
  timer=0;
}
int gettimerstatus(){
  return timerstarted?1:0; 
}
void sendSSDVpic(uint8_t *packet)
{
  sendbitdata(flags,50);//Was 50
  sendbitdata(encodeArray,12);
  shuffle(&packet[0]);
  sendbitdata(packet,256);
  sendbitdata(flags,5);
  //  if(send_buffer.getSize()>1280){
  //    transmiton=1;
  // }
}
void sendbitdata(byte* toSend,int length)
{
  int bt = 0;
  byte inbyte;
  //Serial.println(toSend.length());
  //Serial.println(send_buffer.getSize());

  /* while(send_buffer.getSize()>9000){
   delay(2);
   // transmiton=1;
   }*/
  /*  if(send_buffer.getSize()<512){
   // transmiton=0;
   }*/
  for (int i = 0; i < length; i++)
  {
    //  noInterrupts();
    send_buffer.put(toSend[i]);


    // interrupts();

    /*   inbyte = toSend[i];
     //String tstr = String(inbyte,HEX);
     // Serial.print(tstr);
     for (int k = 0; k < 8; k++)
     {                                               //do the following for each of the 8 bits in the byte
     bt = inbyte & 0x01;                           //strip off the rightmost bit of the byte to be sent (inbyte) 
     sendAX25tone1200BAUD(bt);
     inbyte = (byte)(inbyte >> 1);
     }*/
  }

}
void sendAX25tone1200BAUD(int bt){


  //Serial.println(bufcnt); 
  ulPhaseIncrement = toneRegister[bt];
  bufcnt=70;
  while(bufcnt!=0){
    ;
    // Serial.println(bufcnt);
  }
}

void TC4_Handler()//TC4_Handler()
{
  //TC_GetStatus(TC1, 1);
  // Serial.println("I");
  // noInterrupts();
  if(timerstarted){
    timer++;
    if (timer == timer10)
    {
      timerstarted=false;
      timer=0;
    }      
  }
  if(send_morse){
    timer10ms++;
    if(timer10ms==tenmscount){//960  
      timer10ms=0;
      if(sendMorse(cwstring, cwstringlength)==1) send_morse=false;
    }  
    //return;
  }
  if(transmiton==0) return;
  if (baud_mode==1) {
    TC4_Handler4800();
    return;
  }
  if (baud_mode==2) {
    TC4_Handler9600();
    return;
  }
  if (bufcnt!=0){

    // We need to get the status to clear it and allow the interrupt to fire again


    ulPhaseAccumulator += ulPhaseIncrement;   // 32 bit phase increment, see below

    // if the phase accumulator over flows - we have been through one cycle at the current pitch,
    // now we need to reset the grains ready for our next cycle
    if(ulPhaseAccumulator >= SAMPLES_PER_CYCLE_FIXEDPOINT)
    {
      // DB 02/Jan/2012 - carry the remainder of the phase accumulator
      ulPhaseAccumulator -= SAMPLES_PER_CYCLE_FIXEDPOINT;
    }

    // get the current sample  
    uint32_t ulOutput = nSineTable[ulPhaseAccumulator>>20];

    // we cheated and user analogWrite to enable the dac, but here we want to be fast so
    // write directly 
    analogWrite(A14, ulOutput>>1);
    // dacc_write_conversion_data(DACC_INTERFACE, ulOutput>>1);
    bufcnt--;

  }
  if(bufcnt==0){
    if(K<8){
      bti = inbytei & 0x01;                           //strip off the rightmost bit of the byte to be sent (inbyte) 
      ulPhaseIncrement = toneRegister[bti];

      inbytei = (byte)(inbytei >> 1);
      K++;
      bufcnt=80;//70
    }
    else{
      K=8;
      if(send_buffer.getSize()> 0){             
        K=0;
        // noInterrupts();
        inbytei = send_buffer.get();
        //  Serial.print(send_buffer.getSize());
        //  Serial.print(" ");
        //  Serial.println(inbytei,HEX);
        // interrupts();
        bti = inbytei & 0x01;                           //strip off the rightmost bit of the byte to be sent (inbyte) 
        ulPhaseIncrement = toneRegister[bti];
        //
        inbytei = (byte)(inbytei >> 1);
        K++;    
        bufcnt=80;      //70     
      }
      else{ 
        analogWrite(A14, 1000);
      }   
    }
  }
  // interrupts();

}
void TC4_Handler4800()
{
  //  TC_GetStatus(TC1, 1);
  // if(transmiton==0) return;
  if (bufcnt!=0){

    //Serial.print(waveIndex);
    uint32_t ulOutput = baud_4800_table[waveIndex];
    waveIndex++;
    // we cheated and user analogWrite to enable the dac, but here we want to be fast so
    // write directly 
    analogWrite(A14, ulOutput>>1);
    //dacc_write_conversion_data(DACC_INTERFACE, ulOutput>>1);
    bufcnt--;

  }
  if(bufcnt==0){
    if(K<8){
      bti = inbytei & 0x01;                           //strip off the rightmost bit of the byte to be sent (inbyte) 
      selecttable();

      inbytei = (byte)(inbytei >> 1);
      K++;
      // waveIndex=0;
      bufcnt=20;

    }
    else{
      K=8;
      if(send_buffer.getSize()> 0){             
        K=0;
        // noInterrupts();
        inbytei = send_buffer.get();
        //  Serial.print(send_buffer.getSize());
        //  Serial.print(" ");
        //  Serial.println(inbytei,HEX);
        // interrupts();
        bti = inbytei & 0x01;                           //strip off the rightmost bit of the byte to be sent (inbyte) 
        selecttable();
        //
        inbytei = (byte)(inbytei >> 1);
        K++; 
        //   waveIndex=0;   
        bufcnt=20;           
      } 
      else{ 
        analogWrite(A14, 1000);
      }     
    }
  }
}
void selecttable(){
  //  Serial.println(" ");
  txshreg <<= 1;
  if (bti == 1) txshreg |= 1;
  if ((txshreg & 0x20000) == 0x20000) txshreg ^= 0x0021;
  if ((txshreg & 0x40000) == 0x40000) bti = 1; 
  else bti= 0; 
  if (previoussentbit == 1)
  {


    if (bti == 1)
    {
      //One to One Tra
      waveIndex=60;
    }
    else
    {
      waveIndex=20;
    }
  }
  else
  {
    if (bti == 1)
    {
      //Zero to 1
      waveIndex=00;
    }
    else
    {
      //Zero to Zero
      waveIndex=40;
    }
  }

  previoussentbit = bti;
}
void TC4_Handler9600()
{
  //  TC_GetStatus(TC1, 1);
  // if(transmiton==0) return;
  if (bufcnt!=0){

    //Serial.print(waveIndex);
    uint32_t ulOutput = baud_9600_table[waveIndex];
    waveIndex++;
    // we cheated and user analogWrite to enable the dac, but here we want to be fast so
    // write directly 
    analogWrite(A14, ulOutput>>1);
    //dacc_write_conversion_data(DACC_INTERFACE, ulOutput>>1);
    bufcnt--;

  }
  if(bufcnt==0){
    if(K<8){
      bti = inbytei & 0x01;                           //strip off the rightmost bit of the byte to be sent (inbyte) 
      selecttable9600();

      inbytei = (byte)(inbytei >> 1);
      K++;
      // waveIndex=0;
      bufcnt=10;

    }
    else{
      K=8;
      if(send_buffer.getSize()> 0){             
        K=0;
        // noInterrupts();
        inbytei = send_buffer.get();
        //  Serial.print(send_buffer.getSize());
        //  Serial.print(" ");
        //  Serial.println(inbytei,HEX);
        // interrupts();
        bti = inbytei & 0x01;                           //strip off the rightmost bit of the byte to be sent (inbyte) 
        selecttable9600();
        //
        inbytei = (byte)(inbytei >> 1);
        K++; 
        //   waveIndex=0;   
        bufcnt=10;           
      } 
      else{ 
        analogWrite(A14, 1000);
      }     
    }
  }
}
void selecttable9600(){
  //  Serial.println(" ");
  txshreg <<= 1;
  if (bti == 1) txshreg |= 1;
  if ((txshreg & 0x20000) == 0x20000) txshreg ^= 0x0021;
  if ((txshreg & 0x40000) == 0x40000) bti = 1; 
  else bti= 0; 
  if (previoussentbit == 1)
  {


    if (bti == 1)
    {
      //One to One Tra
      waveIndex=30;
    }
    else
    {
      waveIndex=10;
    }
  }
  else
  {
    if (bti == 1)
    {
      //Zero to 1
      waveIndex=00;
    }
    else
    {
      //Zero to Zero
      waveIndex=20;
    }
  }

  previoussentbit = bti;
}
void getTemperature()
{

  /*  OneWire oneWireo(ONE_WIRE_BUS2);
   DallasTemperature sensorso(&oneWireo);
   sensorso.begin(); 
   if (!sensorso.getAddress(outsideThermometer, 0)) Serial.println("Unable to find address for Outside Device 0"); */
  sensorso.setResolution(12);
  sensorso.requestTemperatures(); 
  float tempC = sensorso.getTempC(outsideThermometer);
  Serial.println(tempC);
  if(tempC>99.0f){ 
    tempC=99.0f;
  }
  if(tempC<-99.0f){
    tempC=-99.0f;
  }
  sprintf(tbuf,"%+03.0f",tempC);
  tempout=tbuf;
  // sensors.begin();
  //  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Inside Device 0"); 
  // sensors.setResolution(12);
  sensors.requestTemperatures(); 
  tempC = sensors.getTempC(insideThermometer);
  if(tempC>99.0f){ 
    tempC=99.0f;
  }
  if(tempC<-99.0f){
    tempC=-99.0f;
  }

  analogReadResolution(16);
  int sensorValue = analogRead(38);//Was A0
  double t1;
  t1 = (sensorValue-15296.32)/-35.7155;
  Serial.println(t1);
  sprintf(tbuf,"%+03.0f",t1);
  tempin=tbuf;
  Serial.println(tempin);
  Serial.println(tempout);

}

String getVoltage(){
  analogReference(EXTERNAL);
  analogReadResolution(12);
  analogReadAveraging(32);

  double mv;
  mv = 120 * 4096 /analogRead(39);
  Serial.println(mv);
  // delay(2000);
  // double voltage= 2*sensorValue * (3.3 / 1024.0)*100;
  //  Serial.print("ADC 10-bit (default) : ");
  // Serial.println(analogRead(A0));
  //  Serial.println(voltage);
  sprintf(tbuf,"%03.0f",mv);
  Serial.println(tbuf);
  return tbuf;
}
/*void ddsUP(){
 ad.up();
 }*/
/*void ddsDOWN(){
 ad.down();
 }*/
/*void initDDS(){
 
 
 ad.setfreq(10100000);
 }*/
void initSPI(){
  pinMode(20, OUTPUT);
  SPIFIFO.begin(13,0,0);
  SPI.setBitOrder(LSBFIRST) ;
  // SPI.setClockDivider(0,64 ) ;
}  

void initSPI1(){

}








