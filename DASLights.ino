/* 
  DASlamps
  Archery timing lamps on Arduino 
  
  
  NOTES from FITA event
  
  Had to frig the final part of the FITA sequences, to add a delay because it was only doing a single beep for some reason...
  To do:
  FIX above fault
  Change beep so it is a % of a total beep cycle length - done
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
#define VERSION "1.9.2"

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

// Shooting sequences
#define sequenceFITA_3_1 0    //Three arrow FITA, one detail
#define sequenceFITA_3_2 1    //Three arrow FITA, two details
#define sequenceFITA_6_1 2    //Six arrow FITA, one detail
#define sequenceFITA_6_2 3    //Six arrow FITA, two details
#define sequenceGNAS     4    //GNAS 4 minute sequence
#define sequenceCONTINUAL 5  

// Beep states
#define beepStateNEW -1
#define beepStateON 1
#define beepStateOFF 0

// Beep timer - split of on/off time in ms
#define beepOnTime 100
#define beepOffTime 900


// Global variables because I'm too lazy not to use them

int sequence = 0;               // selected shooting sequence, determined by rotary control at startup
int selectedlamp = 0;
int sequenceStage = 0;
long sequenceStart = 0;         // unused (to do with pause function in some way, so still compiled)
//long detaillength = 0;          // unused
int currentlamp = 0;            // current lamp state
int oldlamp = 0;                // Previous lamp state
long beeptime = 0;              // System time when the beepState last changed
int beepcount = 0;              // Counter for number of requested beeps
//int dimmer = 0;               // unused
int currentState = 0;           // Current operational state of the system (sequence, wait, Pause etc)
//int pressingnextdetailpin = 0;  // press-to-make pins activate when released
//int pressingPausePin = 0;       // press-to-make pins activate when released
//long pauseStart = 0;          // unused

int currentBeepState = beepStateOFF;
int beepDisabled = 0;


/*
    The setup routine runs once at startup
    Two second delay, then sets up the I/O pins and tests the lights
*/    
void setup() {        
  Serial.println("Setup");
  delay(2000);
  // Start serial I/O
  Serial.begin(9600);

  // Initialize all lamp pins as an output.
  pinMode(redpin, OUTPUT);           //11
  pinMode(yellowpin, OUTPUT);        //10   
  pinMode(greenpin, OUTPUT);         //9
  pinMode(beeper, OUTPUT);           //8
  
  // Initialize all control pins as an input.
  pinMode(emergencypin, INPUT);      //2
  pinMode(resetpin, INPUT);          //6
  pinMode(nextdetailpin, INPUT);     //5
  pinMode(startpin, INPUT);          //4
  pinMode(beepswitch, INPUT);        //3
  
  pinMode(pausepin, INPUT);          //12
    
  // Initialise rotary controls as input
  pinMode(lampselect, INPUT);        //A2
  pinMode(dimmerPin, INPUT);         //A1
  pinMode(sequenceSelectPin, INPUT); //A0 
  
  //Set wait state of the system
  Serial.print("System start - DAS Lights version ");
  Serial.println(VERSION);

  // Test all lights in turn
  dolamps(lampSteadyRed);
  delay(1000);
  dolamps(lampSteadyYellow);
  delay(1000);
  dolamps(lampSteadyGreen);
  delay(1000);

  // Get initial states of all controls, and set red waiting lamp
  checkSensors();
  currentlamp = lampSteadyRed;
  Serial.print( "Starting with lamp: ");
  Serial.println(currentlamp);
  
  dolamps(currentlamp);
  currentState = stateWait;
  Serial.println("Startup complete.");
  
}


/* 
    The loop routine runs over and over again forever
*/
void loop() { 
  checksequenceselect();                                          // Check where the main selector knob is
  checkSensors();                                                 // Check buttons and switches
  if (currentState == stateSequence) doSequence(doSequenceContinue);     // If we're running a sequence, run the sequence loop
  dobeeper();                                                     // Check the beep loop
  dolamps(currentlamp);                                           // Set lamps according to lampState bitwise
  
  delay(100);                                                     // Then wait and do it all again
  }

/*
 *  Set lamps according to current lampState bitwise flag 
 */
void dolamps(int lampState)
{
#if defined (EMULATIONMODE)
  // Emulation mode - output lamp states to serial if there has been a change
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

    beeptime is the system clock time when the currentBeepState was last changed
    currentBeepState is the current requested beep 
      -1 = a beep sequence has been requested
      1 = currently beeping - start beep
      0 = currently not beeping - stop beep
    
*/
void dobeeper() {
  int beepStateCurrentTime = millis() - beeptime;

  if (beepcount > 0) {    // There are still beeps left to go
    if (currentBeepState == beepStateNEW) {  // New beep sequence requested
      currentBeepState = beepStateON;       // New beep state
      beeptime = millis();  // fresh state
      digitalWrite(beeper, HIGH); // and switch on
      Serial.println("BeepSTART");
    }
    else 
    {
      if (currentBeepState == beepStateON) {  // currently beeping
        if (beepStateCurrentTime > beepOnTime) { // done enough
          currentBeepState = beepStateOFF;
          beeptime = millis();
          digitalWrite(beeper, LOW);
          beepcount--;
          Serial.println("BeepOFF");
        }
      }
      else
      {
        if (currentBeepState == beepStateOFF) {  // currently not beeping
          if (beepStateCurrentTime > beepOffTime) { // done enough
            currentBeepState = beepStateON;
            beeptime = millis();
            digitalWrite(beeper, HIGH);
            Serial.println("BeepON");
          }
        }
      }
    }
  }
  else
  {       // Failsafe - beep OFF
    digitalWrite(beeper, LOW);
  }
  
/*
  
  // Do nothing if beepcount is zero (no beeps required or beeper switch is set OFF
  if (beepcount > 0) {    // There are still beeps left to go
    if (((millis() - beeptime) > (beeplength)) || (currentBeepState == -1)) { // either time to change beep or fresh request
      if (currentBeepState < 1) {    // currently not beeping or fresh request
        digitalWrite(beeper, HIGH);  // on for beeplength
        currentBeepState = 1;
        beeptime = millis();      // then reset time
      }
      else
      {                       // currently beeping
        digitalWrite(beeper, LOW);  // off for beeplength
        currentBeepState = 0;
        beeptime = millis();      // then reset time
        beepcount--;
      }     
    }
  }
  else 
  {
    digitalWrite(beeper, LOW);     // failsafe OFF
  }
*/
}


/*
    Check the auto/manual lamp setting
*/
void checklampselect() {
  if ((currentState != stateSequence) && (currentState != statePause)) { // don't change lamp state here if running a sequence

    int lampSelectReading = analogRead(lampselect);  // Not in a sequence, so pick up lamp selector reading
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
    //      selectedlamp = autolamp;
          currentlamp = lampSteadyRed;
        }
      }
    }
  }
}


/*
    Check the beeper switch and enable or disable as required
*/
void checkbeepselect() {
  if (digitalRead(beepswitch) == HIGH) beepDisabled = 0; else beepDisabled = 1;
}  


/*
    Check the sequence setting
    can change sequence only if not running one, otherwise does nothing
*/
void checksequenceselect() {

  if ((currentState != stateSequence) && (currentState != statePause)) {  
    int sequenceSelectReading = analogRead(sequenceSelectPin); // Only check sequence selector if not running a sequence
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
  if ((digitalRead(emergencypin) == HIGH) && (currentState != stateEmergencyStop)) {
    if (currentState == stateSequence) {
      Serial.println("Button - EMERGENCYSTOP");
      beep(3);
      currentlamp = lampSteadyRed;
      currentState = stateWait;
      sequenceStart = 0;
      doSequence(doSequenceReset);
    }
    if (currentState == statePause) {
      Serial.println("Button - STOP from PAUSE state");
      // beep(3);           // No beeps for paused stop
      currentlamp = lampSteadyRed;
      currentState = stateWait;
      sequenceStart = 0;
      doSequence(doSequenceReset);
    }
  }    
}


/*
    Check all buttons and act accordingly
*/
void checkSensors() {
  static int pressingResetPin = 0;
  static int pressingnextdetailpin = 0;

  // Read lamp selector, and if not running a sequence, set currentlamp variable
  checklampselect();  
  // Read beeper switch and set or clear beepdisable
  checkbeepselect();
  // Emergency stop - red lamp and three beeps
  checkemergencystop();
  
  // "go" button pressed, not currently running a sequence, AND we are set to automatic lamps
  // all this could really be done from the doSequence service routine
  if ((digitalRead(startpin) == HIGH) && (currentState != stateSequence) && (currentState != statePause) && (selectedlamp == autolamp)) {
    currentState = stateSequence;         // Paused or not, go to sequence
    Serial.println("Button - START"); //and say so
      
    if (currentState != statePause) {     // Not paused, so go for it
      // First set the sequence according to the rotary switch
      checksequenceselect();
      doSequence(doSequenceNextStage); // Next Stage from not running a sequence, effectively "start"
    } 
  }
    
  // "reset" pressed, and not already waiting, reset the system
  // RESET is now also PAUSE
  // Behaviour is as follows:
  //
  //   While running a sequence, Initiate PAUSE.
  //      - Sequence stops
  //      - Light to yellow
  //      - Start 10 second pause timer  (don't think we need this)
  //
  //   Can be interrupted by NEXT, RESET or STOP
  //
  //      NEXT - cancels PAUSE, stops the timer and starts the next sequence
  //      RESET - (PAUSE button pressed again) RESET as normal
  //      STOP - ends sequence silently
  //
  //
  
  if ((digitalRead(resetpin) == HIGH) && (pressingResetPin == 0)) {
    Serial.println("Button - PAUSE/RESET pressed");
    pressingResetPin = 1;
  }

  if ((digitalRead(resetpin) == LOW) && (pressingResetPin == 1)) {
    Serial.println("Button - PAUSE/RESET released");
    pressingResetPin = 0;
    
    
    if (currentState == statePause) { //  already paused, so reset
      Serial.println("Resetting system");
      doSequence(doSequenceReset);        
    } else if (currentState == stateSequence) { // running a sequence, so set up pause state
      Serial.println("Entering PAUSE state");
      currentlamp = yellowlamp;             //  Set yellow lamp
      currentState = statePause;            //  Set PAUSE state
    }
  }

  //  These pins must be checked for a high-low cycle so they only fire once
  if ((digitalRead(nextdetailpin) == HIGH) && (pressingnextdetailpin == 0)) {
    if (currentState == stateSequence) {
      Serial.println("Button - NEXT pressed");
      pressingnextdetailpin = 1;
    }
  }
  if ((digitalRead(nextdetailpin) == LOW) && (pressingnextdetailpin == 1)) {
    Serial.println("Button - NEXT released");
    if (currentState == stateSequence) {
      Serial.println("doSequenceNextDetail");
      pressingnextdetailpin = 0;
      doSequence(doSequenceNextDetail);
    }
  }
}

/*
    Set box back to wait state
void resetAll() {
    currentlamp = redlamp;
    currentState = stateWait;
}
*/


/*
    Set up the beep loop
    beep(0) will stop the beep at any point
*/
void beep(int count) {
  beepcount = count;
  
  if (beepDisabled == 1) {
    if (count == 0) {
      beepcount = 0;
      currentBeepState = beepStateOFF;
    }
    else
    {
      beepcount = count;
      beeptime = millis();
      currentBeepState = beepStateNEW;
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
    - doSequenceContinue - probably no action (call from main loop uses this)
    - doSequenceNextStage - Skip to next stage, whatever that is
    - doSequenceNextDetail - Skip to the "wait" before the start of the next detail
    - doSequenceStartSequence
*/
void doSequence(int sequenceAction){
  static int sequenceStage = 0; // Maintained as the stage in the current sequence
  static long stageStart = 0;   // this will be set to the current system time at each state change
  stage thisStage;              // holds data for current stage
  boolean newStage = false;     // Flag to signal a stage change
  long stageNow;                
  int timer;

  thisStage = Sequences[sequence].stagelist[sequenceStage];

// first, deal with sequenceAction command
  switch (sequenceAction) {
    // Reset sequence
    case doSequenceReset:
    // Actually, reset is handled in the checksensors routine 
    // so this is probably not neede
      Serial.println("RESET activated");
      currentlamp = redlamp;
      currentState = stateWait;
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
        currentState = stateWait;
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
        currentState = stateWait;
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
