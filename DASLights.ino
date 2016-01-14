/* 
  DASlamps
  Archery timing lamps on Arduino 
  
  
  NOTES from FITA event
  
  Had to frig the final part of the FITA sequences, to add a delay because it was only doing a single beep for sone reason...
  To do:
  FIX above fault
  Change beep so it is a % of a total beep cycle length
  Add pause button - ends current detail and goes to yellow
  Add display
  Add wireless
  
  Notes from Vixster
  Also just as a note, not that there's any problem but sue doesn't think the next function runs right in that it goes 
  to green instead of red. 

  To Do...
  Time shift mode - compress all sequence timings by a given factor for testing purposes
  Check sequence data - the next to last stage we put in last time may be unnecessary 
  now the timing of the NEXT button is fixed.
  Is the Emergency Stop causing three beeps??
  
 */
#define VERSION 1.9

// Compilation flag, uncomment for hardware-free test mode
#define EMULATIONMODE 0

// Load hardware pin definitions
#include <DASLights-pins.h>
// Load light sequences
#include <DASLights-datastructures.h>

// Lamp flags - could set these to the same as the input pins
#define redlamp 4
#define yellowlamp 2
#define greenlamp 1
#define autolamp 0

// Current system state
#define stateWait 1              //Waiting for a start button
#define stateSequence 2          //Currently running a sequence
#define stateEmergencyStop 3     //Running the Emergency Stop process
#define stateAttract 4           //"Attract" mode (useful for testing lamps, I guess...)
#define statePause 5             //Sequence running but paused

// flags for doSequence routine
#define doSequenceReset 0
#define doSequenceContinue 1
#define doSequenceNextStage 2
#define doSequenceNextDetail 3

// Sequences
#define sequenceFITA_3_1 0    //Three arrow FITA, one detail
#define sequenceFITA_3_2 1    //Three arrow FITA, two details
#define sequenceFITA_6_1 2    //Six arrow FITA, one detail
#define sequenceFITA_6_2 3    //Six arrow FITA, two details
#define sequenceGNAS     4    //GNAS 4 minute sequence
#define sequenceCONTINUAL 5  


// Global variables because I'm too lazy not to use them
int beeplength = 500;
int lamplength = 500;

int sequence = 0;
int selectedlamp = 0;
int sequenceStage = 0;
long sequenceStart = 0;
long detaillength = 0;
int currentlamp = 0;
int oldlamp = 0;
long beeptime = 0;
int beepcount = 0;
int dimmer = 0;
int state = 0;
int pressingnextdetailpin = 0;
int pressingPausePin = 0;
long pauseStart = 0;
int beepstate = 0;
int beepdisabled = 0;


/*
    The setup routine runs once at startup
    Two second delay, then sets up the I/O pins and tests the lights
*/    
void setup() {        
  Serial.println("Setup");
  delay(2000);
  // Start serial I/O
  Serial.begin(9600);

  // Initialize all pins as an output.
  pinMode(redpin, OUTPUT);           //11
  pinMode(yellowpin, OUTPUT);        //10   
  pinMode(greenpin, OUTPUT);         //9
  pinMode(beeper, OUTPUT);           //8
  
  pinMode(emergencypin, INPUT);      //2
  pinMode(resetpin, INPUT);          //6
  pinMode(nextdetailpin, INPUT);     //5
  pinMode(startpin, INPUT);          //4
  pinMode(beepswitch, INPUT);        //3
  
  pinMode(pausepin, INPUT);          //12
    
  pinMode(lampselect, INPUT);        //A2
  pinMode(dimmerPin, INPUT);         //A1
  pinMode(sequenceSelectPin, INPUT); //A0 
  
  //Set wait state of the system
  Serial.print("System start - DAS Lights version ");
  Serial.println(VERSION);
  
  dolamps(lampSteadyRed);
  delay(1000);
  dolamps(lampSteadyYellow);
  delay(1000);
  dolamps(lampSteadyGreen);
  delay(1000);
  
  checkSensors();
  currentlamp = lampSteadyRed;
  Serial.print( "Starting with lamp: ");
  Serial.println(currentlamp);
  
  dolamps(currentlamp);
  state = stateWait;
  Serial.println("Startup complete.");
  
}




/* 
    The loop routine runs over and over again forever
*/
void loop() { 
  checksequenceselect();                                          //Check where the main selector knob is
  checkSensors();                                                 //Check buttons and switches
  if (state == stateSequence) doSequence(doSequenceContinue);     //Service the sequence loop
  dobeeper();                                                     //Check the beep loop
  dolamps(currentlamp);                                           //And the lamp loop
  //lampson(currentlamp);                                         //And check lamps
  delay(100);                                                     //Then wait and do it all again
}


void dolamps(int lampState)
{
#if defined (EMULATIONMODE)
  if (lampState != oldlamp) {
    if(lampState & redlamp) Serial.print("RRR"); else Serial.print("xxx");
    if(lampState & yellowlamp) Serial.print("YYY"); else Serial.print("xxx");
    if(lampState & greenlamp) Serial.println("GGG"); else Serial.println("xxx");
    oldlamp = lampState;
  }
#endif
  if(lampState & redlamp) digitalWrite(redpin, HIGH); else digitalWrite(redpin, LOW);
  if(lampState & greenlamp) digitalWrite(greenpin, HIGH); else digitalWrite(greenpin, LOW);
  if(lampState & yellowlamp) digitalWrite(yellowpin, HIGH); else digitalWrite(yellowpin, LOW);

}


/*
    Beep loop
    The service loop for the automatic beep
*/
void dobeeper() {
  if (beepcount > 0) {
    if (((millis() - beeptime) > (beeplength)) || (beepstate == -1)) {
      if (beepstate < 1) {
        digitalWrite(beeper, HIGH);  // on for beeplength
        beepstate = 1;
        beeptime = millis();      // then reset time
      }
      else
      {
        digitalWrite(beeper, LOW);  // on for beeplength
        beepstate = 0;
        beeptime = millis();      // then reset time
        beepcount--;
      }     
    }
  }
  else 
  {
    digitalWrite(beeper, LOW);     // failsafe OFF
  }
}



/*
    Check the auto/manual lamp setting
*/
void checklampselect() {
  int lampSelectReading = analogRead(lampselect);
  if (state != stateSequence) { // don't care about lamp state if running a sequence
  //  doSequence(doSequenceReset);
    if (lampSelectReading >700) {
    //  selectedlamp = greenlamp;
      currentlamp = lampSteadyGreen;
    }
    else
    {
      if (lampSelectReading >400) {
    //    selectedlamp = yellowlamp;
        currentlamp = lampSteadyYellow;
      }
      else
      {
        if (lampSelectReading >100) {
    //      selectedlamp = redlamp;
          currentlamp = lampSteadyRed;
        }
        else
        {
    //      selectedlamp = redlamp;
          currentlamp = lampSteadyRed;
        }
      }
    }
  }
  // 4th switch setting (0) is no specific lamp
  // so leave currentlamp to be set by sequence, emergency or whatever
}


/*
    Check the beeper switch and enable if required
*/
void checkbeepselect() {
  if (digitalRead(beepswitch) == HIGH) beepdisabled = 0; else beepdisabled = 1;
}  


/*
    Check the sequence setting
    can change sequence only if not running one, otherwise does nothing
*/
void checksequenceselect() {
  int sequenceSelectReading = analogRead(sequenceSelectPin);

  if (state != stateSequence)  
  {  
    if (sequenceSelectReading >1000) {
      sequence = sequenceCONTINUAL;
//      Serial.println("Sequence set to CONTINUAL");
    }
    else
    {
      if (sequenceSelectReading >800) {
        sequence = sequenceGNAS;
//        Serial.println("Sequence set to GNAS");
      }
      else
      {
        if (sequenceSelectReading >600) {
          sequence = sequenceFITA_6_2;
//          Serial.println("Sequence set to FITA_6_2");
        }
        else
        {
          if (sequenceSelectReading >400) {
            sequence = sequenceFITA_6_1;
//            Serial.println("Sequence set to FITA_6_1");
          }
          else
          {
            if (sequenceSelectReading >200) {
              sequence = sequenceFITA_3_2;
//              Serial.println("Sequence set to FITA_3_2");
            }
            else
            {
              sequence = sequenceFITA_3_1;
//              Serial.println("Sequence set to FITA_3_1");
            }
          }    
        }
      }    
    }
  }
}

void checkemergencystop() {
  // Emergency stop - red lamp and three beeps
  if ((digitalRead(emergencypin) == HIGH) && (state != stateEmergencyStop)) {
    if (state == stateSequence) {
      Serial.println("Button - EMERGENCYSTOP");
      beep(3);
      currentlamp = lampSteadyRed;
      state = stateWait;
      sequenceStart = 0;
      doSequence(doSequenceReset);
    }
  }    
}


/*
    Check all buttons and act accordingly
*/
void checkSensors() {
  
  // Read lamp selector, and if not running a sequence, set currentlamp variable
  checklampselect();  
  // Read beeper switch and set or clear beepdisable
  checkbeepselect();
  // Emergency stop - red lamp and three beeps
  checkemergencystop();
  
  // "go" button pressed, not currently running a sequence, AND we are set to automatic lamps
  // all this could really be done from the doSequence service routine
  if ((digitalRead(startpin) == HIGH) && (state != stateSequence) && (selectedlamp == autolamp)) {
    state = stateSequence;         // Paused or not, go to sequence
    Serial.println("Button - START"); //and say so
    Serial.println(state); //and say so
      
    if (state != statePause) {     // Not paused, so go for it
      Serial.println("Button - START (not paused)"); //and say so
      // First set the sequence according to the rotary switch
      checksequenceselect();
      doSequence(doSequenceNextStage); // Next Stage from not running a sequence, effectively "start"
    } 
  }

    
  // "reset" pressed, and not already waiting, reset the system
  if ((digitalRead(resetpin) == HIGH) && (state != stateWait)) {
    Serial.println("Button - RESET");
    doSequence(doSequenceReset);
  }

  //  These pins must be checked for a high-low cycle so they only fire once
  if ((digitalRead(nextdetailpin) == HIGH) && (pressingnextdetailpin == 0)) {
    if (state == stateSequence) {
      Serial.println("Button - NEXT pressed");
      pressingnextdetailpin = 1;
    }
  }
  if ((digitalRead(nextdetailpin) == LOW) && (pressingnextdetailpin == 1)) {
    Serial.println("Button - NEXT released");
    if (state == stateSequence) {
      Serial.println("doSequenceNextDetail");
      pressingnextdetailpin = 0;
      doSequence(doSequenceNextDetail);
    }
  }
/*
  //  Pause - Pause the current shooting sequence, and resume with no further signals
  if ((digitalRead(pausepin) == HIGH) && (pressingPausePin == 0)){
    pressingPausePin = 1;
  }
  if ((digitalRead(pausepin) == LOW) && (pressingPausePin == 1)) {
    pressingPausePin = 0;
    if (state == stateSequence) {
      actionPause("ON");
    }
    else 
    {
      actionPause("OFF");
    }
  }
  */

    
}

/*
    Set or release pause state
    Add the time paused to sequenceStart, which 
    extends the current sequence phase by that amount
*/
void actionPause(String Which) {
  long pauseTime;
  if (Which == "ON") {
    state = statePause;
    pauseTime = millis();
  }
  else
  {
    state = stateSequence;
    sequenceStart = sequenceStart + (pauseTime - millis());
  }
}


/*
    Set box back to wait state
void resetAll() {
    currentlamp = redlamp;
    state = stateWait;
}
*/


/*
    Set up the beep loop
    beep(0) will stop the beep at any point
*/
void beep(int count) {
  if (beepdisabled == 1) {
    if (count == 0) {
      beepcount = 0;
      beepstate = 0;
    }
    else
    {
      beepcount = count;
      beeptime = millis();
      beepstate = -1;
    }
  }
}


/*
    Warning - briefly flash lamps
*/
void lampswarn() {
  dolamps(lampSteadyRed & lampSteadyYellow & lampSteadyGreen);
  delay(100);
  dolamps(currentlamp);
}



/*==================================================================*/
/*
    Sequence servicing routine
    Called with a sequenceAction
    - doSequenceReset - cancel current sequence
    - doSequenceContinue - probably no action
    - doSequenceNextStage - Skip to next stage, whatever that is
    - doSequenceStartSequence
*/
void doSequence(int sequenceAction){
  static int sequenceStage = 0;
  static long stageStart = 0; //this will be set at each state change
  static long detailStart = 0;
  stage thisStage;            // holds data for current stage
  boolean newStage = false;
  long stageNow;
  long detailNow;
  static long detailLength = 0;
  int timer;

  thisStage = Sequences[sequence].stagelist[sequenceStage];

// first, deal with sequenceAction command
  switch (sequenceAction) {
    // Reset sequence
    case doSequenceReset:
    // Actually, reset is handled in the checksensors routine 
    // so this is probably not needed
      currentlamp = redlamp;
      state = stateWait;
      sequenceStage = 0;
      stageStart = millis();
    break;
    case doSequenceContinue:
      // Continue as normal, if past time, trigger new stage
      stageNow = millis() - stageStart;
      if (stageNow > thisStage.time*1000) {
        Serial.println("");
        Serial.print("New Stage (");
        Serial.print(sequenceStage);
        Serial.print(") reached, current time=");
        Serial.print(millis());
        Serial.print(" stageNow=");
    //    Serial.println(now);
        Serial.print(stageNow);
        Serial.print(" stageStart=");
        Serial.println(stageStart);    
        sequenceStage++;
        stageStart = millis();    
        newStage = true;    // less than 100ms implies new stage
      }
    
    break;
    case doSequenceNextStage:
    // time for next stage
      sequenceStage++;
      stageStart = millis();
      Serial.println("Next Stage forced");
      Serial.println(Sequences[sequence].name);
      newStage = true;
    break;
    
    case doSequenceNextDetail:
      Serial.println("Next Detail forced");
      Serial.println(Sequences[sequence].name);
      //Find the next "prepare" or "stop"stage
      while ((thisStage.type != stageTypePrepare) && (thisStage.type != stageTypeStop)) {
        sequenceStage++;
        thisStage = Sequences[sequence].stagelist[sequenceStage];
        // Serial.println(thisStage.type);
      }
      if (thisStage.type == stageTypeStop) {
        state = stateWait;
        Serial.println("No more details - STOP");
      }
      else
      {
        Serial.println("Starting next detail");
        stageStart = millis();    
        newStage = true;    // less than 100ms implies new stage
      }
    break;
  }
    
//get current stage data
  thisStage = Sequences[sequence].stagelist[sequenceStage];
  
  switch(thisStage.action) {
    case stageActionStatic:
     // probably do nothing, "GO" button will do a doSequenceNextStage
    break;
    
    case stageActionTimed:
     //Do nothing - waiting for next stage
       if (thisStage.type == stageTypePrepare) {
         // showing 10 second countdown
         //Serial.println("Current timer - " + stageNow);
         timer = (int) ((10000 - stageNow) / 1000);
         if ((stageNow % 1000) < 100) {    
           Serial.print(timer);
           Serial.print("...");
         }
       } 
       else
      {
        
      } 
//       displayTime();
    break;
    
    case stageActionGoto:
      sequenceStage = thisStage.time; // for Goto stages, the time is used as the destination stage
      thisStage = Sequences[sequence].stagelist[sequenceStage];
      if (thisStage.time == 0) {
        state = stateWait;
      }
      Serial.print("GOTO stage - moving to stage ");
      Serial.println(sequenceStage);
    break;
    
    case stageActionStop:
      sequenceStage = 0;
    break;
  }
  currentlamp = thisStage.lamps;
  if (newStage) {
    beep(thisStage.buzzer); 
    Serial.print("Beep - ");
    Serial.println(thisStage.buzzer);
  }
}
