const int tone_2200 = 1;
const int tone_1200 = 0;
int lasttoneport1 = tone_1200;
int bitstuff = 0;     
byte bitcnt=8;
byte outbyte=0;
int flagsize;
const byte FLAG = 0x7E;
const byte FLAGSIZE = 50;
const byte ENDFLAGSIZE = 5;
byte flagbuf[FLAGSIZE];
byte totaladdress[7];
byte pcktheader[23];
String messageAPRS;
byte ssidsource =3;
//  private Hdlc_RX hdlcrx;//For testing
void testFrame(){
  /*  byte SendData[27] = {
   0x86, 0xA2, 0x40, 0x40, 0x40, 0x40, 0x60, 0xAE, 0x64, 0x8C, 0xA6,
   0x40, 0x40, 0x68, 0xA4, 0x8A, 0x98, 0x82, 0xB2, 0x40, 0x61, 0x03,
   0xF0, 0x54, 0x65, 0x73, 0x74  };*/

  byte SendData[103] =  
  { 
    0x82,0xa0,0xa4,0xa6,0x40,0x40,0x00,0xac,0x96,0x66,0xa8,0x84,0x86,
    0x00,0xae,0x92,0x88,0x8a,0x40,0x40,0x05,0x03,0xf0,0x2f,0x32,0x31,
    0x34,0x39,0x35,0x37,0x68,0x33,0x37,0x34,0x34,0x2e,0x35,0x35,0x53,
    0x2f,0x31,0x34,0x34,0x35,0x34,0x2e,0x34,0x31,0x45,0x4f,0x20,0x2f,
    0x41,0x3d,0x30,0x30,0x30,0x32,0x36,0x39,0x20,0x20,0x53,0x71,0x30,0x30,
    0x30,0x31,0x20,0x53,0x30,0x30,0x30,0x20,0x54,0x69,0x30,0x30,0x30,0x43,
    0x20,0x54,0x6f,0x30,0x30,0x30,0x43,0x20,0x30,0x30,0x30,0x6d,0x56,0x20,
    0x53,0x61,0x30,0x30,0x20,0x47,0x46,0x30,0x20                      };

  transmiton=0;
  AX25senddata(SendData,0,103);
  transmiton=1;
}
void sendAPRSframe(){
  starttimer();
  while(!getGPSdata()&&timerstarted);
  stoptimer();
  // Serial.println(formatAPRSMessage());
  int msglength=formatAPRSMessage();
  Serial.println(messageAPRS);
  byte SendData[23+msglength];
  byte payload[msglength];
  messageAPRS.getBytes(payload,msglength);
  buildAPRSpacketheader();
  memcpy(SendData,pcktheader,23);
  memcpy(&SendData[23],payload,msglength);
  Serial.print("Message Length ");
  Serial.println(msglength);
  for(int i=0;i<23;i++){
    Serial.print((char)SendData[i],HEX);
    Serial.print(" ");
  }
  Serial.println();
  transmiton=0;
  AX25senddata(SendData,0,23+msglength); 
  transmiton=1;
}

void AX25senddata(byte txdatabuf[],int fsize,int length)
{
  //   hdlcrx = new Hdlc_RX();

  ushort crc;
  if (fsize != 0) flagsize = fsize; 
  else flagsize = FLAGSIZE;
  addflag(50);
  // byte[] buffer = new byte[15] { 0x90, 0x84, 0x72, 0xb4, 0xa4, 0x90, 0x60, 0x90, 0x84, 0x72, 0xa0, 0xaa, 0x9e, 0xe1, 0xf1 };
  /*  buffer byte $90, $84, $72, $b4, $a4, $90,$60, $90, $84, $72, $a0, $aa, $9e, $e1, $f1, 0, 0 'checksum $30 $26 test frames*/

  crc = CRC16(txdatabuf,length);
  byte crclohi[2];
  crclohi[0] = (byte)(crc & 0xFF);
  crclohi[1] = (byte)((crc >> 8) & 0xFF);

  byte ax25FrametoSend[length+2];
  sendFrameport1(flagbuf, true,FLAGSIZE);
  // Serial.println(*flagbuf);
  memcpy(ax25FrametoSend,txdatabuf,length);
  memcpy(&ax25FrametoSend[length],crclohi,2);
  // ax25FrametoSend[length]=crclohi[1];
  //  ax25FrametoSend[length+1]=crclohi[0];
  sendFrameport1(ax25FrametoSend, false,length+2);


  sendFrameport1(flagbuf,true,15);


}

void addflag(int flagsize)
{

  for (int i = 0; i < flagsize; i++)
  {
    flagbuf[i] = FLAG;
  }
  return ;
}


ushort CRC16(byte bytes[],int length)
{
  /* This was a pain in the arse. Not documented at all! */
  /* This is what is used for AX25                      */
  ushort crc = 0xFFFF; //(ushort.maxvalue, 65535)

  for (int j = 0; j < length; j++)
  {
    crc = (ushort)(crc ^ bytes[j]);
    for (int i = 0; i < 8; i++)
    {
      if ((crc & 0x0001) == 1)
        crc = (ushort)((crc >> 1) ^ 0x8408);
      else
        crc >>= 1;
    }
  }
  return (ushort)~(uint)crc;    //A neat way to invert a byte.
}

void sendFrameport1(byte ax25frame[], boolean flag,int length)
{
  byte inbyte;


  //bitcnt=8;
  for (int i = 0; i < length; i++)
  {
    inbyte = ax25frame[i];
    // Serial.print(inbyte,HEX);
    // Serial.print(" ");
    // Inner loop.

    int k, bt;
    for (k = 0; k < 8; k++)
    {                                               //do the following for each of the 8 bits in the byte
      bt = inbyte & 0x01;                           //strip off the rightmost bit of the byte to be sent (inbyte)


      if (bt == 0)
      {
        nrziport1();                                 // if this bit is a zero, flip the output state
      }
      else
      {                                            //otherwise if it is a 1, do the following:
        bitstuff++;                             //increment the count of consecutive 1's 
        if ((flag == false) && (bitstuff == 5))
          // if ((ax25frame[i] != 0x7e) && (bitstuff == 5))
        {   	                                //stuff an extra 0, if 5 1's in a row


          //gt.sendAX25tone1200BAUD(lasttoneport1);  //send 1/1200t of tone 833.3us
          outbyte=outbyte>>1;
          if(lasttoneport1==1){
            outbyte=0x80|outbyte;  
            // Serial.println(outbyte,HEX)  ;  
          }

          bitcnt--;
          if(bitcnt==0){
            //  storebyte in buffer;
            send_buffer.put(outbyte);
            //  Serial.println(outbyte,HEX);
            //  Serial.print(" ");           
            outbyte=0;
            bitcnt=8;
          }

          //  gt.send9600Baud(lasttoneport1);

          nrziport1();            		           //flip the output state to stuff a 0
        }
      }
      inbyte = (byte)(inbyte >> 1); 
      //go to the next bit in the byte

      //gt.sendAX25tone1200BAUD(lasttoneport1);  //send 1/1200t of tone 833.3us
      outbyte=outbyte>>1;
      if(lasttoneport1==1){
        outbyte=0x80|outbyte;  
        //  Serial.println(outbyte,HEX);
        //Serial.print(" ");     
      }

      bitcnt--;
      if(bitcnt==0){
        //  storebyte in buffer;
        send_buffer.put(outbyte);
        //  Serial.println(outbyte,HEX);
        // Serial.print(" ");   
        outbyte=0;
        bitcnt=8;
      }

    }
  }
  //Serial.println();
}

void nrziport1()
{
  bitstuff = 0;
  if (lasttoneport1 == tone_1200)
  {
    lasttoneport1 = tone_2200;
  }
  else
  {
    lasttoneport1 = tone_1200;
  }
  return;
}

String getlatAPRS(){
  char buf[32]; 
  String sign = "+";


  double value  = getlatd();
  if (value<0) { 
    sign = "-"; 
  }
  value = abs(value);
  int degrees = (int)value;
  double minutesandfracminutes = (value - degrees) * 60;
  degrees = degrees * 100;
  double convertedlat = degrees + minutesandfracminutes;
  sprintf(buf,"%02.2f",convertedlat);

  String north = "N";
  String south = "S";
  String lats;
  if (sign == "+") lats = buf + north; 
  else lats = buf + south;  
  Serial.println(lats);
  Serial.println(getlat());
  return lats;

}

String getlongitudeAPRS(){
  char buf[32]; 
  String sign = "+";

  double value = getlongd();
  if (value<0) { 
    sign = "-"; 
  }
  value = abs(value);
  int degrees = (int)value;
  double minutesandfracminutes = (value - degrees) * 60;
  degrees = degrees * 100;
  double convertedlong = degrees + minutesandfracminutes;
  sprintf(buf,"%03.2f",convertedlong);
  String east= "E";
  String west = "W";
  String longS;
  if (sign == "+") longS= buf + east; 
  else longS = buf + west;
  Serial.println(longS);
  Serial.println(getlong());
  return longS;
}
String getAPRSalt(){
  char buf[64];
  String altaprs;
  double altitduefeet = 3.2808399 * getaltd();//APRS protocol is in feet.
  // Serial.println("Altitude");
  // Serial.println(getaltd());
  // altitduefeet = round(altitduefeet);
  // int altft = (int)(altitduefeet);
  sprintf(buf,"%06.0f",altitduefeet)  ;
  // alt = String.Format("{0:000000}", altft);     
  return buf;
}
int  formatAPRSMessage(){
  String icon = "O";
  String symtableID = "/";
  String AltitudeExtend = " /A=";

  messageAPRS = "/" + getgpsTimeAPRS() + "h" + getlatAPRS() + symtableID + getlongitudeAPRS() + icon + AltitudeExtend + getAPRSalt()+" "+ " Sq" + seq  + " "  + "Ti" + tempin + "C " + "To" + tempout + "C " + voltage + "cV " + "Sa" + getnumsatellites() + " GF" + getage();
  return messageAPRS.length()+1;
}
void buildAPRSpacketheader(){

  byte destaddress[7];
  byte sourceaddress[7];
  byte pathaddress[7];
  byte control= 0x03;
  byte pid = 0xF0;
  char arraycallsignsaddress[6];//Max Length of callsign
  char arraycallsigndaddress[6];
  byte arraypathaddress[6];
  char *aprs="APRS  ";
  char *path="WIDE  ";
  byte PSSID=1;
  //   Serial.println(callsign);
  //  callsign = strcpy( callsign , padAddress(callsign).c_str());
  // Serial.println("Padded CallSign");
  // Serial.println(callsign);
  // memcpy(arraycallsignsaddress,callsign,6);

  buildaddress(aprs,0,0);
  memcpy(pcktheader,totaladdress,7); 

  buildaddress(callsign,ssidsource,7);
  memcpy( &pcktheader[7],totaladdress,7);

  // path.getBytes(arraypathaddress,6);
  buildaddress(path,PSSID,14);
  memcpy( &pcktheader[14],totaladdress,7);
  pcktheader[20]=  pcktheader[20]|0x01;//Path last bit set to one to terminate the callsign path correctly.
  pcktheader[21]=control;
  pcktheader[22]=pid;

  return ;
}
void buildaddress(char *address,byte ssid,byte addresspos){

  rotateAddress(address);

  if (ssid == 0)
  {
    totaladdress[6+addresspos] = 0;
  }
  else
  {
   
    totaladdress[6] = 0x60|(ssid<<1);

  }
 
  return;

}
void rotateAddress(char *address)
{

  for (int i = 0; i < 6; i++)
  {

    totaladdress[i] = address[i]<<1;
    //  Serial.print (totaladdress[i]);
  }
  //  Serial.println();
  return;
}
String padAddress(String address)
{

  /*Need to pad the address with spaces if the callsign is less than 6 characters*/
  String paddedaddress;
  if (address.length() < 6)
  {

    for (int i = address.length(); i < 6; i++)
    {
      paddedaddress= paddedaddress + " ";
    }

  }
  return paddedaddress;
}
















