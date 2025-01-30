#define PASSCODE_LENGTH 4
#define CORRECT_PASSCODE "1234"

char passcode[PASSCODE_LENGTH + 1] = ""; // Buffer to store user input passcode
bool passcodeEntered = false; // Flag to indicate if passcode has been entered correctly
bool pickingBulletNum = false; // Flag to indicate if we are in picking number to be fired

bool numberGuessingMode = false; // Flag to indicate if we are in number guessing mode
int guessNumber = 0; // What number guess the user is on
int targetNumber = 0; // The target the user is trying to guess
int MAX_GUESSES = 3; // The max number of guesses

//////////////////////////////////////////////////
                //  LIBRARIES  //
//////////////////////////////////////////////////
#include <Arduino.h>
#include <Servo.h>
#include <IRremote.hpp>

#define DECODE_NEC  //defines the type of IR transmission to decode based on the remote. See IRremote library for examples on how to decode other types of remote

//defines the specific command code for each button on the remote
#define left 0x8
#define right 0x5A
#define up 0x18
#define down 0x52
#define ok 0x1C
#define cmd1 0x45
#define cmd2 0x46
#define cmd3 0x47
#define cmd4 0x44
#define cmd5 0x40
#define cmd6 0x43
#define cmd7 0x7
#define cmd8 0x15
#define cmd9 0x9
#define cmd0 0x19
#define star 0x16
#define hashtag 0xD

//////////////////////////////////////////////////
          //  PINS AND PARAMETERS  //
//////////////////////////////////////////////////
Servo yawServo; //names the servo responsible for YAW rotation, 360 spin around the base
Servo pitchServo; //names the servo responsible for PITCH rotation, up and down tilt
Servo rollServo; //names the servo responsible for ROLL rotation, spins the barrel to fire darts

int yawServoVal = 90; //initialize variables to store the current value of each servo
int pitchServoVal = 100;
int rollServoVal = 90;

int lastYawServoVal = 90; //initialize variables to store the last value of each servo
int lastPitchServoVal = 90; 
int lastRollServoVal = 90;

int pitchMoveSpeed = 8; //this variable is the angle added to the pitch servo to control how quickly the PITCH servo moves - try values between 3 and 10
int yawMoveSpeed = 90; //this variable is the speed controller for the continuous movement of the YAW servo motor. It is added or subtracted from the yawStopSpeed, so 0 would mean full speed rotation in one direction, and 180 means full rotation in the other. Try values between 10 and 90;
int yawStopSpeed = 90; //value to stop the yaw motor - keep this at 90
int rollMoveSpeed = 90; //this variable is the speed controller for the continuous movement of the ROLL servo motor. It is added or subtracted from the rollStopSpeed, so 0 would mean full speed rotation in one direction, and 180 means full rotation in the other. Keep this at 90 for best performance / highest torque from the roll motor when firing.
int rollStopSpeed = 90; //value to stop the roll motor - keep this at 90

int yawPrecision = 100; // this variable represents the time in milliseconds that the YAW motor will remain at it's set movement speed. Try values between 50 and 500 to start (500 milliseconds = 1/2 second)
int shakePrecision = 25; // this variable represents the time in milliseconds for the smaller shake movement
int rollPrecision = 270; // this variable represents the time in milliseconds that the ROLL motor with remain at it's set movement speed. If this ROLL motor is spinning more or less than 1/6th of a rotation when firing a single dart (one call of the fire(); command) you can try adjusting this value down or up slightly, but it should remain around the stock value (270) for best results.

int pitchMax = 150; // this sets the maximum angle of the pitch servo to prevent it from crashing, it should remain below 180, and be greater than the pitchMin
int pitchMin = 33; // this sets the minimum angle of the pitch servo to prevent it from crashing, it should remain above 0, and be less than the pitchMax

//function prototypes for proper compiling
void shakeHeadYes(int nods = 3);
void shakeHeadNo(int nods = 3);
void fire(int darts = 1);
void rapidFire(int darts = 6);
void bootyShake(int shakes = 8, int shakeWidth = 4, int shakeDelay = 0);
void danceMove(int half = 4 , int full = 8);
void handleNumberGuess(int guess);

//////////////////////////////////////////////////
                //  S E T U P  //
//////////////////////////////////////////////////
void setup() {
    Serial.begin(9600);
    randomSeed(analogRead(0));

    yawServo.attach(10); //attach YAW servo to pin 3
    pitchServo.attach(11); //attach PITCH servo to pin 4
    rollServo.attach(12); //attach ROLL servo to pin 5

    // Just to know which program is running on my Arduino
    Serial.println(F("START " __FILE__ " from " __DATE__ "\r\nUsing library version " VERSION_IRREMOTE));

    // Start the receiver and if not 3. parameter specified, take LED_BUILTIN pin from the internal boards definition as default feedback LED
    IrReceiver.begin(9, ENABLE_LED_FEEDBACK);

    Serial.print(F("Ready to receive IR signals of protocols: "));
    printActiveIRProtocols(&Serial);
    Serial.println(F("at pin 9"));

  homeServos();
}

////////////////////////////////////////////////
                //  L O O P  //
////////////////////////////////////////////////

void loop() {
    if (IrReceiver.decode()) { //if we have recieved a comman this loop...
        int command = IrReceiver.decodedIRData.command; //store it in a variable
        IrReceiver.resume(); // Enable receiving of the next value
        handleCommand(command); // Handle the received command through switch statements
    }
    delay(5); //delay for smoothness
}

void checkPasscode() {
    if (strcmp(passcode, CORRECT_PASSCODE) == 0) {
        // Correct passcode entered, shake head yes
        Serial.println("CORRECT PASSCODE");
        passcodeEntered = true;
        shakeHeadYes();
    } else {
        // Incorrect passcode entered, shake head no
        passcodeEntered = false;
        shakeHeadNo();
        Serial.println("INCORRECT PASSCODE");
    }
    passcode[0] = '\0'; // Reset passcode buffer
}

void addPasscodeDigit(char digit) {
    if (!passcodeEntered && digit >= '0' && digit <= '9' && strlen(passcode) < PASSCODE_LENGTH) {
        strncat(passcode, &digit, 1); //adds a digit to the passcode
        Serial.println(passcode); //print the passcode to Serial
    } else if (strlen(passcode) > PASSCODE_LENGTH+1){
      passcode[0] = '\0'; // Reset passcode buffer
      Serial.println(passcode);
    }
}

void handleCommand(int command) {
    // this checks to see if the command is a repeat
    if((IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {
      // If the command is up down left or right continue
      if (command == up || command == down || command == left || command == right) {
          Serial.println("CONTINUING COMMAND");
      } else {
          Serial.println("DEBOUNCING REPEATED NUMBER - IGNORING INPUT");
          return; //discarding the repeated numbers prevent you from accidentally inputting a number twice
      }
    }

    switch (command) {
        case up:
            if (passcodeEntered) {
                // Handle up command
                upMove(1);
            } else {
                //shakeHeadNo();
            }
            break;

        case down:
            if (passcodeEntered) {
                // Handle down command
                downMove(1);
            } else {
                //shakeHeadNo();
            }
            break;

        case left:
            if (passcodeEntered) {
                // Handle left command
                leftMove(1);
            } else {
                //shakeHeadNo();
            }
            break;

        case right:
            if (passcodeEntered) {
              // Handle right command
              rightMove(1);
            } else {
                //shakeHeadNo();
            }
            break;

        case ok:
            if (passcodeEntered) {
                // Handle fire command
                fire();
                Serial.println("FIRE");
            } else {
                //shakeHeadNo();
            }
            break;

        case star:
            if (passcodeEntered) {
              Serial.println("LOCKING");
                // Return to locked mode
                passcodeEntered = false;
            } else {
                //shakeHeadNo();
            }
            break;

        case hashtag:
            Serial.println("Number guessing mode");
            if (passcodeEntered) {
                numberGuessingMode = true;
                guessNumber = 0;
                targetNumber = random(10);
                Serial.print("The Target Number is: ");
                Serial.print(targetNumber);
                Serial.println("");
            }
            break;

        case cmd1: // Add digit 1 to passcode
            Serial.println("1");
            if (!passcodeEntered) {
                addPasscodeDigit('1');
            } else if (pickingBulletNum) {
                pickingBulletNum = false;
                rapidFire(1);
            } else if (numberGuessingMode) {
                handleNumberGuess(1);
            } else {
              shakeHeadYes();
            }
            break;

        case cmd2: // Add digit 2 to passcode
            Serial.println("2");
            if (!passcodeEntered) {
                addPasscodeDigit('2');
            } else if (pickingBulletNum) {
                pickingBulletNum = false;
                rapidFire(2);
            } else if (numberGuessingMode) {
                handleNumberGuess(2);
            } else {
              shakeHeadNo();
            }
            break;

        case cmd3: // Add digit 3 to passcode
            Serial.println("3");
            if (!passcodeEntered) {
                addPasscodeDigit('3');
            } else if (pickingBulletNum) {
                pickingBulletNum = false;
                rapidFire(3);
            } else if (numberGuessingMode) {
                handleNumberGuess(3);
            } else {
              bootyShake();
            }
            break;

        case cmd4: // Add digit 4 to passcode
            Serial.println("4");
            if (!passcodeEntered) {
                addPasscodeDigit('4');
            } else if (pickingBulletNum) {
                pickingBulletNum = false;
                rapidFire(4);
            } else if (numberGuessingMode) {
                handleNumberGuess(4);
            } else {
              danceMove();
            }
            break;

        case cmd5: // Add digit 5 to passcode
            Serial.println("5");
            if (!passcodeEntered) {
                addPasscodeDigit('5');
            } else if (pickingBulletNum) {
                pickingBulletNum = false;
                rapidFire(5);
            } else if (numberGuessingMode) {
                handleNumberGuess(5);
            }
            break;

        case cmd6: // Add digit 6 to passcode
            Serial.println("6");
            if (!passcodeEntered) {
                addPasscodeDigit('6');
            } else if (pickingBulletNum) {
                pickingBulletNum = false;
                rapidFire(6);
            } else if (numberGuessingMode) {
                handleNumberGuess(6);
            }
            break;

        case cmd7: // Add digit 7 to passcode
            Serial.println("7");
            if (!passcodeEntered) {
                addPasscodeDigit('7');
            } else if (numberGuessingMode) {
                handleNumberGuess(7);
            }
            break;

        case cmd8: // Add digit 8 to passcode
            Serial.println("8");
            if (!passcodeEntered) {
                addPasscodeDigit('8');
            } else if (numberGuessingMode) {
                handleNumberGuess(8);
            }
            break;

        case cmd9: // Add digit 9 to passcode
            Serial.println("9");
            if (!passcodeEntered) {
                addPasscodeDigit('9');
            } else if (numberGuessingMode) {
                handleNumberGuess(9);
            }
            break;

        case cmd0: // Add digit 0 to passcode
            Serial.println("0");
            if (!passcodeEntered) {
                addPasscodeDigit('0');
            } else if (numberGuessingMode) {
                handleNumberGuess(0);
            } else if (!pickingBulletNum) {
                Serial.println("PICK NUMBER OF DARTS TO FIRE");
                pickingBulletNum = true;
            } else if (pickingBulletNum) {
                pickingBulletNum = false;
                rapidFire(0);
            }
            break;

        default:
            // Unknown command, do nothing
            Serial.println("Command Read Failed or Unknown, Try Again");
            break;
    }
    if (strlen(passcode) == PASSCODE_LENGTH){
        checkPasscode();
    }
}

void leftMove(int moves){
    for (int i = 0; i < moves; i++){
        yawServo.write(yawStopSpeed + yawMoveSpeed); // adding the servo speed = 180 (full counterclockwise rotation speed)
        delay(yawPrecision); // stay rotating for a certain number of milliseconds
        yawServo.write(yawStopSpeed); // stop rotating
        delay(5); //delay for smoothness
        Serial.println("LEFT");
  }

}

void rightMove(int moves){ // function to move right
  for (int i = 0; i < moves; i++){
      yawServo.write(yawStopSpeed - yawMoveSpeed); //subtracting the servo speed = 0 (full clockwise rotation speed)
      delay(yawPrecision);
      yawServo.write(yawStopSpeed);
      delay(5);
      Serial.println("RIGHT");
  }
}

void upMove(int moves){
  for (int i = 0; i < moves; i++){
        if((pitchServoVal+pitchMoveSpeed) < pitchMax){ //make sure the servo is within rotation limits (less than 150 degrees by default)
        pitchServoVal = pitchServoVal + pitchMoveSpeed;//increment the current angle and update
        pitchServo.write(pitchServoVal);
        delay(50);
        Serial.println("UP");
      }
  }
}

void downMove (int moves){
  for (int i = 0; i < moves; i++){
      if((pitchServoVal-pitchMoveSpeed) > pitchMin){//make sure the servo is within rotation limits (greater than 35 degrees by default)
        pitchServoVal = pitchServoVal - pitchMoveSpeed; //decrement the current angle and update
        pitchServo.write(pitchServoVal);
        delay(50);
        Serial.println("DOWN");
      }
  }
}


void homeServos(){
    yawServo.write(yawStopSpeed); //setup YAW servo to be STOPPED (90)
    delay(20);
    rollServo.write(rollStopSpeed); //setup ROLL servo to be STOPPED (90)
    delay(100);
    pitchServo.write(100); //set PITCH servo to 100 degree position
    delay(100);
    pitchServoVal = 100; // store the pitch servo value
}

void fire(int darts = 1) { //function for firing the passed in number of darts
    Serial.print("Firing ");
    Serial.print(darts);
    Serial.println(" darts");

    for (int i = 0; i < darts; i++) { 
      rollServo.write(rollStopSpeed + rollMoveSpeed);//start rotating the servo
      delay(rollPrecision);//time for approximately 60 degrees of rotation
      rollServo.write(rollStopSpeed);//stop rotating the servo
      delay(5); //delay for smoothness
    } 
}

// Function to rapid fire x darts (defaults to all 6)
void rapidFire(int darts = 6) {
    if (darts < 1) {
        return;
    }
    if (darts > 6) {
        darts = 6;
    }

    Serial.print("Rapid firing ");
    Serial.print(darts);
    Serial.println(" darts");

    rollServo.write(rollStopSpeed + rollMoveSpeed);//start rotating the servo
    delay(rollPrecision * darts); //time for 360 degrees of rotation
    rollServo.write(rollStopSpeed);//stop rotating the servo
    delay(5); // delay for smoothness
}    

// Function for shaking head yes
// sets the default number of nods to 3, but you can pass in whatever number of nods you want
void shakeHeadYes(int nods = 3) { 
      Serial.println("YES");

    if ((pitchMax - pitchServoVal) < 15){
      pitchServoVal = pitchServoVal - 15;
    }else if ((pitchServoVal - pitchMin) < 15){
      pitchServoVal = pitchServoVal + 15;
    }
    pitchServo.write(pitchServoVal);

    int startAngle = pitchServoVal; // Current position of the pitch servo
    int lastAngle = pitchServoVal;
    int nodAngle = startAngle + 15; // Angle for nodding motion

    for (int i = 0; i < nods; i++) { // Repeat nodding motion three times
        // Nod up
        for (int angle = startAngle; angle <= nodAngle; angle++) {
            pitchServo.write(angle);
            delay(7); // Adjust delay for smoother motion
        }
        delay(50); // Pause at nodding position
        // Nod down
        for (int angle = nodAngle; angle >= startAngle; angle--) {
            pitchServo.write(angle);
            delay(7); // Adjust delay for smoother motion
        }
        delay(50); // Pause at starting position
    }
}

// Function for shaking head no
// sets the default number of nods to 3, but you can pass in whatever number of nods you want
void shakeHeadNo(int nods = 3) {
    Serial.println("NO");

    for (int i = 0; i < nods; i++) { // Repeat nodding motion three times
        // rotate right, stop, then rotate left, stop
        yawServo.write(140);
        delay(190); // Adjust delay for smoother motion
        yawServo.write(yawStopSpeed);
        delay(50);
        yawServo.write(40);
        delay(190); // Adjust delay for smoother motion
        yawServo.write(yawStopSpeed);
        delay(50); // Pause at starting position
    }
}

void danceMove(int half, int full) {
    // Careful, this dance is sure to attract those that are binary!
    // Calculate the duration for a half turn and full turn (best effort since it's a modified 360 degree servo)
    int halfTurnMoves = half; // 4 moves'ish for a half turn
    int fullTurnMoves = full; // 8 moves'ish for a full turn
    homeServos();
    // Turn around 180 degrees (half turn)
    for (int i = 0; i < halfTurnMoves; i++) {
        yawServo.write(yawStopSpeed + yawMoveSpeed); // Counterclockwise rotation
        delay(yawPrecision);
        yawServo.write(yawStopSpeed); // Stop rotation
        delay(5);
    }
    delay(100);
    upMove(7); // "head" down
    delay(300);
    // Perform booty shake
    bootyShake();
    delay(100);
    downMove(7);
    pitchServo.write(100); // home servo
    delay(300);

    // Return to original position
    for (int i = 0; i < halfTurnMoves; i++) {
        yawServo.write(yawStopSpeed - yawMoveSpeed); // Clockwise rotation back to start
        delay(yawPrecision);
        yawServo.write(yawStopSpeed); // Stop rotation
        delay(5);
    }
}

// Function for having turrt do a booty shake!
void bootyShake(int shakes, int shakeWidth, int shakeDelay) {
    int width = shakeWidth; // Number of moves for a small shake
    // Pulsate back and forth (booty shake)
    for (int i = 0; i < shakes; i++) {
        for (int j = 0; j < width; j++) { // left shakeWidth
            yawServo.write(yawStopSpeed + yawMoveSpeed);
            delay(shakePrecision);
            yawServo.write(yawStopSpeed);
            delay(shakeDelay);
        }
        for (int j = 0; j < width; j++) { // right shakeWidth
            yawServo.write(yawStopSpeed - yawMoveSpeed);
            delay(shakePrecision);
            yawServo.write(yawStopSpeed);
            delay(shakeDelay);
        }
    }
}

void handleNumberGuess(int guess) {
    Serial.print("GUESSING: ");
    Serial.print(guess);
    Serial.println("");

    if (guess == targetNumber) {
        // If guess is correct
        Serial.println("CORRECT GUESS! Exiting guess mode");
        guessNumber = 0;
        numberGuessingMode = false;
        shakeHeadYes();
    } else {
        // If guess is not correct, increment guess number, shake head no, and fire and end if out of guesses
        Serial.println("WRONG GUESS");
        guessNumber++;
        shakeHeadNo();

        if (guessNumber == MAX_GUESSES) {
            Serial.println("OUT OF GUESSES, FIRING!");
            numberGuessingMode = false;
            fire();
            Serial.println("Exiting guess mode");
        }
    }
}
  