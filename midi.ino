#include <SPI.h>

#define VS_XCS    6 // 6Control Chip Select Pin (for accessing SPI Control/Status registers)
#define VS_XDCS   7 // 7Data Chip Select / BSYNC Pin
#define VS_DREQ   2 // 2Data Request Pin: Player asks for more data
#define VS_RESET  8 //8Reset is active low

//keyboard settings
#define pin1 3//4
#define pin2 4//5
#define pin3 5//6

#define in1  A0
#define in2  A1
#define in3  A2
#define res  A5

#define startNote 30

#define keyUp    34
#define keyDown  35
#define keyBank  39
#define keyInstr 38
#define key5     36
#define key6     37

bool exVal[64]= 
{
  false, false, false, false, false, false, false, false, 
  false, false, false, false, false, false, false, false, 
  false, false, false, false, false, false, false, false, 
  false, false, false, false, false, false, false, false, 
  false, false, false, false, false, false, false, false, 
  false, false, false, false, false, false, false, false, 
  false, false, false, false, false, false, false, false, 
  false, false, false, false, false, false, false, false
};

int ledPins[8][3] = 
{
   {0,0,0}
  ,{0,0,1}
  ,{0,1,0}
  ,{0,1,1}
  ,{1,0,0}
  ,{1,0,1}
  ,{1,1,0}
  ,{1,1,1}
};
int val = 0;
int note = 0;
int bank = 0;
int instrument = 0;
int mode = keyInstr;

//Write to VS10xx register
//SCI: Data transfers are always 16bit. When a new SCI operation comes in 
//DREQ goes low. We then have to wait for DREQ to go high again.
//XCS should be low for the full duration of operation.
void VSWriteRegister(unsigned char addressbyte, unsigned char highbyte, unsigned char lowbyte)
{
  while(!digitalRead(VS_DREQ)) ; //Wait for DREQ to go high indicating IC is available
  digitalWrite(VS_XCS, LOW); //Select control

  //SCI consists of instruction byte, address byte, and 16-bit data word.
  SPI.transfer(0x02); //Write instruction
  SPI.transfer(addressbyte);
  SPI.transfer(highbyte);
  SPI.transfer(lowbyte);
  while(!digitalRead(VS_DREQ)) ; //Wait for DREQ to go high indicating command is complete
  digitalWrite(VS_XCS, HIGH); //Deselect Control
}

//
// Plugin to put VS10XX into realtime MIDI mode
// Originally from http://www.vlsi.fi/fileadmin/software/VS10XX/vs1053b-rtmidistart.zip
// Permission to reproduce here granted by VLSI solution.
//
const unsigned short sVS1053b_Realtime_MIDI_Plugin[28] = { /* Compressed plugin */
  0x0007, 0x0001, 0x8050, 0x0006, 0x0014, 0x0030, 0x0715, 0xb080, /*    0 */
  0x3400, 0x0007, 0x9255, 0x3d00, 0x0024, 0x0030, 0x0295, 0x6890, /*    8 */
  0x3400, 0x0030, 0x0495, 0x3d00, 0x0024, 0x2908, 0x4d40, 0x0030, /*   10 */
  0x0200, 0x000a, 0x0001, 0x0050,
};

void VSLoadUserCode(void) {
  int i = 0;

  while (i<sizeof(sVS1053b_Realtime_MIDI_Plugin)/sizeof(sVS1053b_Realtime_MIDI_Plugin[0])) {
    unsigned short addr, n, val;
    addr = sVS1053b_Realtime_MIDI_Plugin[i++];
    n = sVS1053b_Realtime_MIDI_Plugin[i++];
    while (n--) {
      val = sVS1053b_Realtime_MIDI_Plugin[i++];
      VSWriteRegister(addr, val >> 8, val & 0xFF);
    }
  }
}

void setup() 
{
  pinMode(VS_DREQ, INPUT);
  pinMode(VS_XCS, OUTPUT);
  pinMode(VS_XDCS, OUTPUT);

  digitalWrite(VS_XCS, HIGH); //Deselect Control
  digitalWrite(VS_XDCS, HIGH); //Deselect Data
  pinMode(VS_RESET, OUTPUT);

/// Keyboard
  pinMode(pin1, OUTPUT);
  pinMode(pin2, OUTPUT);
  pinMode(pin3, OUTPUT);

  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);

  pinMode(res, INPUT);

//SPI
  digitalWrite(VS_XDCS, LOW);

  Serial.begin(57600); //Use serial for debugging 
  Serial.println("\n******\n");
  Serial.println("MP3 Shield Example");

  //Initialize VS1053 chip 
  digitalWrite(VS_RESET, LOW); //Put VS1053 into hardware reset

  //Setup SPI for VS1053
  pinMode(10, OUTPUT); //Pin 10 must be set as an output for the SPI communication to work

  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);

  //From page 12 of datasheet, max SCI reads are CLKI/7. Input clock is 12.288MHz. 
  //Internal clock multiplier is 1.0x after power up. 
  //Therefore, max SPI speed is 1.75MHz. We will use 1MHz to be safe.
  SPI.setClockDivider(SPI_CLOCK_DIV16); //Set SPI bus speed to 1MHz (16MHz / 16 = 1MHz)
  SPI.transfer(0xFF); //Throw a dummy byte at the bus

  delayMicroseconds(1);
  digitalWrite(VS_RESET, HIGH); //Bring up VS1053
  
  VSLoadUserCode();

  talkMIDI(0xB0, 0, 0x00); //Default bank GM1
  talkMIDI(0xC0, 0, 0x00); //Set instrument number. 0xC0 is a 1 data byte command
  
}

void sendMIDI(byte data)
{
  SPI.transfer(0);
  SPI.transfer(data);
}

//Plays a MIDI note. Doesn't check to see that cmd is greater than 127, or that data values are less than 127
void talkMIDI(byte cmd, byte data1, byte data2) {
  //
  // Wait for chip to be ready (Unlikely to be an issue with real time MIDI)
  //
  while (!digitalRead(VS_DREQ))
    ;
  digitalWrite(VS_XDCS, LOW);
  sendMIDI(cmd);
  //Some commands only have one data byte. All cmds less than 0xBn have 2 data bytes 
  //(sort of: http://253.ccarh.org/handout/midiprotocol/)
  if( (cmd & 0xF0) <= 0xB0 || (cmd & 0xF0) >= 0xE0) {
    sendMIDI(data1);
    sendMIDI(data2);
  } else {
    sendMIDI(data1);
  }

  digitalWrite(VS_XDCS, HIGH);
}

//Send a MIDI note-on message.  Like pressing a piano key
//channel ranges from 0-15
void noteOn(byte channel, byte note, byte attack_velocity) {
  talkMIDI( (0x90 | channel), note, attack_velocity);
}

//Send a MIDI note-off message.  Like releasing a piano key
void noteOff(byte channel, byte note, byte release_velocity) {
  talkMIDI( (0x80 | channel), note, release_velocity);
}

void loop() 
{

  for (int i=0; i<8; i++)
  {
    digitalWrite(pin1, ledPins[i][0]);
    digitalWrite(pin2, ledPins[i][1]);
    digitalWrite(pin3, ledPins[i][2]);
    
    for (int j=0; j<8; j++)
    {
      digitalWrite(in1, ledPins[j][0]);
      digitalWrite(in2, ledPins[j][1]);
      digitalWrite(in3, ledPins[j][2]);
      val= digitalRead(res);
  
      note = (8*i) + (7-j);
      
      if (val == LOW)
      {
        if (!exVal[note])
        {
            Serial.print ("On ");
            Serial.print (i);
            Serial.print (":");
            Serial.print (j);
            Serial.print ("=");
            Serial.print (note);
            Serial.print ("  instument ");
            Serial.print (instrument);
            Serial.print ("  bank ");
            Serial.println (bank);

          if (note == keyUp)
          {
            if (mode == keyInstr)
            {
              instrument ++;
              if (instrument > 128)
                instrument = 0;
              talkMIDI(0xC0, instrument, 0x00); //Set instrument number. 0xC0 is a 1 data byte command
            }
          }
          else if (note == keyDown)
          {
            if (mode == keyInstr)
            {
              instrument --;
              if (instrument < 0)
                instrument = 128;
              talkMIDI(0xC0, instrument, 0x00); //Set instrument number. 0xC0 is a 1 data byte command
            }
          }
          else if (note == keyBank)
          {
            bank++;
            if (bank > 3)
              bank = 0;
            if (bank == 0)
              talkMIDI(0xB0, 0, 0x00); //Default bank GM1
            else if (bank == 1)
              talkMIDI(0xB0, 0, 0x78); //Default bank GM1
            else if (bank == 2)
              talkMIDI(0xB0, 0, 0x7f); //Default bank GM1
            else if (bank == 3)
              talkMIDI(0xB0, 0, 0x79); //Default bank GM1
    //            0 is default, 0x78 and 0x7f is drums, 0x79 melodic
          }
          else if (note == keyInstr)
          {
            instrument = 0;
            talkMIDI(0xC0, instrument, 0x00); //Set instrument number. 0xC0 is a 1 data byte command
          }
          else if (note == key5)
          {
          }
          else if (note == key6)
          {
          }
          else
          {
            noteOn(0, note + startNote, 120);
//            delayMicroseconds(100);
          }
          exVal[note] = true;
        }
      }
      else
      {
        if (exVal[note])
        {
          exVal[note] = false;
          if (note == keyUp)
          {
          }
          else if (note == keyDown)
          {
          }
          else if (note == keyBank)
          {
          }
          else if (note == keyInstr)
          {
          }
          else if (note == key5)
          {
          }
          else if (note == key6)
          {
          }
          else
            noteOff(0, note + startNote, 120);
//          delayMicroseconds(100);
          Serial.print ("Off ");
          Serial.println (note);
        }
      }
    }
  }
  
/*
  delay(100);
  
  talkMIDI(0xB0, 0x07, 120); //0xB0 is channel message, set channel volume to near max (127)

  //Demo Basic MIDI instruments, GM1
  //=================================================================
  Serial.println("Basic Instruments");
  talkMIDI(0xB0, 0, 0x00); //Default bank GM1

  //Change to different instrument
  for(int instrument = 0 ; instrument < 127 ; instrument++) 
  {

    Serial.print(" Instrument: ");
    Serial.println(instrument, DEC);

    talkMIDI(0xC0, instrument, 0); //Set instrument number. 0xC0 is a 1 data byte command

    //Play notes from F#-0 (30) to F#-5 (90):
    for (int = 30 ; note < 40 ; note++) {

//      Serial.print("N:");
//      Serial.println(note, DEC);
      
      //Note on channel 1 (0x90), some note value (note), middle velocity (0x45):
      noteOn(0, note, 10);
      delay(200);

      //Turn off the note with a given off/release velocity
      noteOff(0, note, 10);
      delay(50);
      
      nr++;
      if (nr>7)
        nr = 0;  
      digitalWrite(pin1, ledPins[nr][0]);
      digitalWrite(pin2, ledPins[nr][1]);
      digitalWrite(pin3, ledPins[nr][2]);

      val1 = analogRead(read1);
      val2 = analogRead(read2);
      val3 = analogRead(read3);

      Serial.print("  A");
      Serial.print(val1);
      Serial.print("  B");
      Serial.print(val2);
      Serial.print("  C");
      Serial.print(val3);
      Serial.println("");
    }

    delay(100); //Delay between instruments
  }
  //=================================================================
  */
}