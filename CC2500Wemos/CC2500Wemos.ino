

#include "cc2500.h"
#include "cc2500_REG.h"
#include <stdio.h>
#include <string.h>
#include <SPI.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <ESP8266WiFi.h>

long rawcount1 = 0;
long rawcount2 = 0;
int firstTime = true;
//
//

typedef struct _Dexcom_packet
{
  uint32_t dest_addr;
  uint32_t src_addr;
  uint8_t  port;
  uint8_t  device_info;
  uint8_t  txId;
  uint16_t raw;
  uint16_t filtered;
  uint8_t  battery;
  uint8_t  unknown;
  uint8_t  checksum;
  uint8_t  RSSI;
  uint8_t  LQI;
} Dexcom_packet;

uint8_t packet[40];
uint8_t oldPacket[40];

static const int NUM_CHANNELS = 4;
static uint8_t nChannels[NUM_CHANNELS] = { 0, 100, 199, 209 };
int8_t fOffset[NUM_CHANNELS] = {0xe4, 0xe3, 0xe2, 0xe2};

/*
SCLK: D5
MISO: D6
MOSI: D7
CSn : D8
*/
const int GDO0_PIN = D0;     // the number of the GDO0_PIN pin

CC2500 cc2500;

void setup()
{
  delay(100);
  Serial.begin(9600);
  Serial.println("*************Restart");
  
  Serial.printf("SS: %d\n", GDO0_PIN);
  
  
  pinMode(GDO0_PIN, INPUT);
  cc2500.init();

  Serial.print("Version: ");
  Serial.println((int) cc2500.ReadStatusReg(REG_VERSION));
  
  memset(&packet, 0, sizeof(packet));
}

void loop()
{
  int waitTime = RxData_RF();

  firstTime = false;
  cc2500.SendStrobe(CC2500_CMD_SPWD);
  //just not sure about the adjustment until further testing
  delay(50);
  cc2500.SendStrobe(CC2500_CMD_SIDLE);
  while ((cc2500.ReadStatusReg(REG_MARCSTATE) & 0x1F) != 0x01) {};
  delay(50);
  Serial.println("Wake");
}


void swap_channel(uint8_t channel, uint8_t newFSCTRL0)
{
  cc2500.WriteReg(REG_CHANNR, channel);
  cc2500.WriteReg(REG_FSCTRL0, newFSCTRL0);

  cc2500.SendStrobe(CC2500_CMD_SRX);
  while ((cc2500.ReadStatusReg(REG_MARCSTATE) & 0x0F) != 0x0D) {};
}


char state()
{
  return (cc2500.ReadStatusReg(REG_MARCSTATE) & 0x1F);
}

long RxData_RF(void)
{
  int Delay = 0;
  int packetFound = 0;
  int channel = 0;
  int crc = 0;
  int lqi = 0;
  uint8_t PacketLength;
  uint8_t freqest;
  Dexcom_packet Pkt;
  long timeStart = 0;
  int continueWait = false;
  Serial.print("Start Listening:");
  Serial.println(millis());

  while (!packetFound && channel < 4) {
    //
    continueWait = false;
    swap_channel(nChannels[channel], fOffset[channel]);
    timeStart = millis();
    //
    Serial.print("Channel:");
    Serial.println(channel);

    //wait on this channel until we receive something
    //if delay is set, wait for a short time on each channel
    Serial.print("Delay:");
    Serial.println(Delay);
    while (!(state() != 0x01) && (millis() - timeStart < Delay) || ((state() != 0x01)  && Delay == 0)) // Instead of reading GDO0 we pull the state. 
    {
      delay(1);
    }

    // Read bytes available in 
    char bytes_in_rx = cc2500.ReadStatusReg(CC2500_REG_RXBYTES) & 0x7F;
    char bytes_to_read_state = cc2500.SendStrobe(CC2500_CMD_SNOP | CC2500_OFF_READ_SINGLE) & 0x07;
    Serial.printf("Bytes in rx: %d, and status: %d\n", bytes_in_rx, bytes_to_read_state);
    
    if (bytes_in_rx > 0 && bytes_to_read_state > 0){
      Serial.println("Got packet");
      PacketLength = cc2500.ReadReg(CC2500_REG_RXFIFO);
      //
      Serial.print("Wait:");
      Serial.println(millis() - timeStart);
      Serial.println(PacketLength);
      //if packet length isn't 18 skip it
      if (PacketLength == 18) {
        //keep the values around in oldPacket so there's something to return over BLE
        //if packet capture fails crc check
        memcpy(oldPacket, packet, 18);
        //
        cc2500.ReadBurstReg(CC2500_REG_RXFIFO, packet, PacketLength);
        memcpy(&Pkt, packet, 18);
        //you should have
        //first byte, not in this array, packet length (18)
        //packet#  sample  comment
        //0        FF      dest addr
        //1        FF      dest addr
        //2        FF      dest addr
        //3        FF      dest addr
        //4        CA      xmtr id
        //5        4C      xmtr id
        //6        62      xmtr id
        //7        0       xmtr id
        //8        3F      port
        //9        3       hcount
        //10       93      transaction id
        //11       93      raw isig data
        //12       CD      raw isig data
        //13       1D      filtered isig data
        //14       C9      filtered isig data
        //15       D2      battery
        //16       0       fcs(crc)
        //17       AD      fcs(crc)
        //
        //
        Pkt.LQI = cc2500.ReadStatusReg(REG_LQI);
        crc = Pkt.LQI & 0x80;

        //packet is good
        if (true) {
          //packet has the correct transmitter id
          if (true) {
            Pkt.RSSI = cc2500.ReadStatusReg(REG_RSSI);
            lqi = (int)(Pkt.LQI & 0x7F);
            Serial.print("RSSI:");

            if ((int)Pkt.RSSI >= 128)
              Serial.println((((int)Pkt.RSSI - 256) / 2 - 73), DEC);
            else
              Serial.println(((int)Pkt.RSSI / 2 - 73), DEC);
            
            Serial.printf("Raw: %X\n", Pkt.raw);
            Serial.printf("Filtered: %X\n", Pkt.filtered);
            convertFloat();
            freqest = cc2500.ReadStatusReg(REG_FREQEST);            
            fOffset[channel] += freqest;            
            Serial.print("Offset:");
            Serial.println(fOffset[channel], DEC);
            
            packetFound = 1;
          } else {
            continueWait = false;
            Serial.println("Another dexcom found");
          }
        } else {
          memcpy(packet, oldPacket, 18);
          continueWait = false;
          Serial.println("CRC Failed");
        }
      } else {
        memcpy(packet, oldPacket, 18);
        continueWait = true;
        Serial.println("Packet length issue");
        //go to next channel and camp out there
        if (channel < 3) {
          channel++;
        } else {
          channel = 0;
        }
        Delay = 0;
      }  //packet length=18
    }

    if (!continueWait) {
      channel++;
      Delay = 600;
    }

    // Make sure that the radio is in IDLE state before flushing the FIFO
    // (Unless RXOFF_MODE has been changed, the radio should be in IDLE state at this point)
    cc2500.SendStrobe(CC2500_CMD_SIDLE);
    while ((cc2500.ReadStatusReg(REG_MARCSTATE) & 0x1F) != 0x01) {};

    // Flush RX FIFO
    cc2500.SendStrobe(CC2500_CMD_SFRX);
  }
  Serial.print("End Listening:");
  Serial.println(millis());

  //add one additional second(per channel) to the delay 
  return channel - 1;
}// Rf RxPacket


//String based method to turn the ISIG packets into a binary string
//also need to add back the left hand zeros if any
//much more elegant method in dexterity, but on arduino I get a bad value every 24 hours or so
//due to (assumed)loss of leading zeros
void convertFloat() {
  String result;
  result = lpad(String(packet[11], BIN), 8);
  result += lpad(String(packet[12], BIN), 8);
  result += lpad(String(packet[13], BIN), 8);
  result += lpad(String(packet[14], BIN), 8);

  char ch[33];
  char reversed[31];
  result.toCharArray(ch, 33);

  int j = 0;
  for (int i = 31; i >= 0; i--) {
    reversed[j] = ch[i];
    j++;
  }

  

  String reversed_str = String(reversed);
  String exp1 = reversed_str.substring(0, 3);
  int exp1_int = (int)binStringToLong(exp1);

  String mantissa1 = reversed_str.substring(3, 16);
  long mantissa1_flt = binStringToLong(mantissa1);

  String exp2 = reversed_str.substring(16, 19);
  int exp2_int = (int)binStringToLong(exp2);

  String mantissa2 = reversed_str.substring(19, 32);
  long mantissa2_flt = binStringToLong(mantissa2);

  rawcount1 = (mantissa1_flt * pow(2, exp1_int) * 2);
  rawcount2 = mantissa2_flt * pow(2, exp2_int);

  Serial.print("Values: ");
  Serial.println(reversed_str);
  
  Serial.print("Raw ISIG:");
  Serial.println(rawcount2);
}

//add zeros back to a string representation of a binary number
//ex.  111 should be 00000111 or the calculations in convertFloat
//would go haywire
String lpad(String str, int length) {
  String zeroes;
  int len = length - str.length();
  while (len > 0) {
    zeroes = zeroes + "0";
    len--;
  }
  return zeroes + str;
}

long binStringToLong(String binary) {
  long result = 0;
  long power = 1;
  char ch[14];

  binary.toCharArray(ch, binary.length() + 1);
  for (int j = binary.length() - 1; j >= 0; j--) {
    if (ch[j] == '1') {
      result = result + power;
    }
    power = power * 2;
  }
  return result;
}
