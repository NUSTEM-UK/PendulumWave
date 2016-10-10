#include <MPR121.h> //Libraries Required.
#include <Wire.h>
#include <SoftwareSerial.h>
#include <Adafruit_NeoPixel.h>
#include <avr/power.h>

#define baudRate 57600 //Set Serial connection rate.
#define numElectrodes 10 // Set number of electrodes used.
#define averageSample 5 // Set sample size odd only.
#define delayTime 10
#define PIN 11
#define NUMPIXELS 121

int calibrationMAXs[numElectrodes];
int calibrationMINs[numElectrodes];
int calibrationMIDs[numElectrodes];
int lastDATA[numElectrodes];
int pointedAverage[numElectrodes][averageSample]={0};
//Array Containing LED numbers for LEDs above the given pendulum
const int electrodeLEDs[numElectrodes][8]={{115,116,117,118,119,120,120,120},{102,103,104,105,106,107,108,109},{88,89,90,91,92,93,94,94},{74,75,76,77,78,79,80,81},{60,61,62,63,64,65,66,67},{51,52,53,54,55,56,57,58},{38,39,40,41,42,43,44,45},{24,25,26,27,28,29,30,31},{10,11,12,13,14,15,16,17},{120,120,120,0,1,2,3,4}};
int colour[120];
const int chimeColour[8]={70,50,25,0,0,25,50,70};

//Scales used for audio genration (MIDI number arrays)

//Minor Pentatonic
//const int pitch[12] = {74, 71, 69, 67, 64, 62, 59, 57, 55, 52};
//Major Pentatonic
  const int pitch[12] = {48, 50, 52, 55, 57, 60, 62, 64, 67, 69};
//C Chord
//int pitch[12] = {84, 79, 76, 72, 67, 64, 60, 55, 52, 48};
//C dim Chord
//const int pitch[12] = {48, 51, 54, 60, 63, 66, 72, 75, 78, 84};

int tempPointedAverage[averageSample]={0};
int DATA[numElectrodes]={0};
bool gradient[numElectrodes]; //0 is negative 1 is positive
bool played[numElectrodes]={0}; //Makes sure note is only played once.
int delayCount=0;

bool isolatePendulum = 0;
int pendulumOfInterest = 8;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup()//Initial NeoPixel strip set up. Colour all green.
{ 
  strip.begin();
  strip.setBrightness(50);
  for(int i=0; i<120; i++){
    colour[i]=85;
    strip.setPixelColor(i,Wheel(colour[i]));
    strip.show();
  }
  
  const int prepTime=5; //Time between Middle Calibration and beginning o void loop().
  Serial.begin(baudRate); //Begin serial connection.
  while(!Serial); //Required for TouchBoard no clue why.
  MPR121.begin(0x5C);
  MPR121.goFast(); //4x Super Speed Mode.
  delay(1000);
  noteOn(1,60); //Chime for beginning of calibration

  // BEGIN MIDDLE CALIBRATION PASSES
  Serial.println("Start Middle Calibration");
  
  for(int i=0; i < numElectrodes ; i++ ) { // Calibrate each electrode.
    double calibrationValueSum = 0;
    int count = 0;
    int time=millis(); //Make each calibration 0.5 seconds long.
    while(millis() < time + 500){
      MPR121.updateFilteredData();
      int calibrationValue=MPR121.getFilteredData(i);
      calibrationValueSum += calibrationValue;
      count++;
    }
    calibrationMIDs[i]=calibrationValueSum/count;
    Serial.print(i); // Provide information on calibration.
    Serial.println(" Middle Calibration Complete");
    Serial.print("Middle:  ");
    Serial.println(calibrationMIDs[i]);
  }
  
  noteOff(1,60); //Chimes for end of middle calibration
  for(int i=0; i<prepTime; i++){
    noteOn(1,60);
    delay(1000);
    noteOff(1,60);
  }
}

void loop() 
{
    //LEDs Flash red when pendulum hits maximum this loop fades the LEDs back to green
    delayCount++;//Custom delay. Uses loop duration as delay so that audio generation is not disturbed
    if(delayCount==delayTime){ //Update colour for each electrode unless it is already green.
      delayCount=0;

      for(int i=0; i<120; i++){
        if(colour[i]==85){ //If green do nothing.
        }
        else{//If not update colour by 1.
          colour[i]++;
        }
        strip.setPixelColor(i,Wheel(colour[i]));
      } 
      strip.show();  
    }
    
    MPR121.updateFilteredData();
    updateDATA();
    
    for (int i=0 ; i < numElectrodes; i++ ){ 
      
      if (DATA[i] > calibrationMIDs[i]){
        played[i]=0;//Pendulum passed middle position, allow playing of note again when it reaches back-swing maximum.
      }
      else{
        
        if (lastDATA[i] > DATA[i]){//Gradient detection
          gradient[i]=0;
        } 
        else if (gradient[i]==0 && lastDATA[i]<DATA[i] && played[i]==0){//Gradient change couses note to be played.
          noteOff(i,pitch[i]); //Turn off last note. Stops GarageBand playing too many notes.
          noteOn(i,pitch[i]);
          gradient[i] = 1;
          played[i] = 1;
          for(int j=0; j<8; j++){//On note playing flash NeoPixels above pendulum by setting flash colour into colour array.
            colour[electrodeLEDs[i][j]]=chimeColour[j];
            strip.setPixelColor(electrodeLEDs[i][j],Wheel(colour[electrodeLEDs[i][j]])); 
          }
          strip.show();
        }
        lastDATA[i]=DATA[i];//Update lastDATA
      }
    }
    MIDIUSB.flush();
}


void updateDATA() //Shift all values one along using temporary storage array.
{ 
  for(int i; i<numElectrodes; i++){
    for(int j=0; j < averageSample; j++ ) {
      tempPointedAverage[j] = pointedAverage[i][j]; //Populate temporary array.
    }
    for(int j=0; j < (averageSample-1); j++){
      pointedAverage[i][j+1]=tempPointedAverage[j]; //Shift all values apart from last one.
    }
    pointedAverage[i][0]=MPR121.getFilteredData(i); //Update first value.
    DATA[i]=((pointedAverage[i][0]/2)+(pointedAverage[i][1]/2)+(pointedAverage[i][2])+(pointedAverage[i][3]/2)+(pointedAverage[i][4]/2))/3;
  }
}

void noteOn(byte channel, int pitch) //Define noteOn function.
{
  MIDIEvent noteOn = {0x09, 0x90 | channel, pitch, 127};
  MIDIUSB.write(noteOn);
}

void noteOff(byte channel, int pitch) //Define noteOff function.
{ 
  MIDIEvent noteOff = {0x08, 0x80 | channel, pitch, 127};
  MIDIUSB.write(noteOff);
}

uint32_t Wheel(byte WheelPos) 
{
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if(WheelPos < 170) {
    WheelPos -= 85;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}

