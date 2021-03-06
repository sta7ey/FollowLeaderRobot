#include "EventTimer.h"

EventTimer turnTimer;

// pin numbers
int PWMright = 5;
int rightIn2 = 4;
int rightIn1 = 2;
int leftIn1 = 7;
int leftIn2 = 9;
int PWMleft = 6;

int leftPhoto = 13;
int rightPhoto = 12;

// line thresholds
unsigned long leftThres;
unsigned long rightThres;

// ultrasonic sensor variables
int TRIGGER = 10;
int ECHO = 3;
double dist;
double duration;
double targetDistance = 0.5; // in meters
double Ki = 0.75; //change these values
double Kd=0; //change these values
double Kp= 70; //change these values
double sumError = 0;
double prevDist = 0;

unsigned long startTime;
unsigned long endTime;
int delta;
bool newReading;

// current speed
int currSpeed; //set the value in setup (when we can see his robot)

// turn speed
int turnSpeed = 25;

// turn booleans
bool prepLeft = false;
bool prepRight = false;

void setup() {
  Serial.begin(9600);

  // put your setup code here, to run once:
  pinMode(PWMright, INPUT);
  pinMode(rightIn2, INPUT);
  pinMode(rightIn1, INPUT);
  pinMode(PWMleft, INPUT);
  pinMode(leftIn2, INPUT);
  pinMode(leftIn1, INPUT);

  // turn wheels to start moving forward
  digitalWrite(rightIn2, LOW);
  digitalWrite(rightIn1, HIGH);
  digitalWrite(leftIn2, LOW);
  digitalWrite(leftIn1, HIGH);

  // ultrasonic sensor setup
  pinMode(TRIGGER, OUTPUT); //trigger 
  pinMode(ECHO, INPUT); //echo 

  //disable interrupts
  cli();

  //set timer1 to Normal mode, pre-scaler of 8
  //use ASSIGNMENT since the bootloader sets up timer1 for 8-bit PWM, which isn't very useful here
  TCCR1A = 0x00;
  TCCR1B = 0x02;

  //enable input capture interrupt
  TIMSK1 |= (1 << ICIE1);

  //enable noise cancelling
  TCCR1B |= (1 << ICNC1);

  //set for falling edge on ICP1 (pin 8 on an Arduino Uno)
  TCCR1B &= ~(1 << ICES1);

  //re-enable interrupts
  sei();

  // set up interupt to catch retrievial of Ultasonic
  attachInterrupt(1, detectEcho, FALLING);

  //calibrate sensors
  calibrate();

  //start taking reading
  takeReading();

  Serial.println("Beginning");
}

// send of signal of ultrasonic
void takeReading(){
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER, LOW);

  // start timing
  startTime = micros();

  newReading = false;
}

// retreive echo
void detectEcho(){
  endTime = micros();
  delta = endTime - startTime;
//  Serial.print("Start Time: ");
//  Serial.println(startTime);
//  Serial.print("End Time: ");
//  Serial.println(endTime);
//  Serial.print("Delta: ");
//  Serial.println(delta);
  newReading = true;
}

// check line sensor
unsigned long checkSensor(int pin) {
  pinMode(pin, OUTPUT);           //set to output
  digitalWrite(pin, HIGH);        //set high
  delay(1);          //charge cap

  pinMode(pin, INPUT);             //set low
  unsigned long s = micros();     //start timing

  while (digitalRead(pin) == HIGH) {

  }

  return micros() - s;            // find final time
}

// distance calculation
double distance() { //in meters
  double s = 343; // speed of sound in meters/s 
  double durationSeconds = ((delta/2.0)/1000000);
  double distance = (durationSeconds*s);
  return distance - 0.15;
}

// PID algorithm
void PID() {
  
  double actualDistance = distance();
  
  if ((prevDist - actualDistance > 0.15 || prevDist - actualDistance < -0.15) && prevDist != 0){
    actualDistance = prevDist;
    Serial.println("ACTUAL = PREV");
  }
  
  double error = actualDistance - targetDistance; //gives us voltage (val between 0 to 255)

  prevDist = actualDistance;
  
    
//  sumError += error;
//  if(sumError > 5) sumError = 2;
//  if(sumError < 0) sumError = 0; 
//
  Serial.print("actualDistance: ");
  Serial.println(actualDistance);
  Serial.print("Error: ");
  Serial.println(error);

  currSpeed = (int)(30 + Kp * error);
  //currSpeed = (int)(30 + Kp * error + Ki * sumError); 
  if(currSpeed < 0) {
    currSpeed = 0;
//    Serial.println("Speed is Negative");
  }
  if(currSpeed > 200) currSpeed = 200; 

Serial.print("Current Speed: ");
Serial.println(currSpeed);

  //start taking new reading
  takeReading();
}

//calibrate line sensors
void calibrate() {
  EventTimer t;
  t.start(5000);

  int leftMax = 0;
  int leftMin = 0xffff;
  int rightMax = 0;
  int rightMin = 0xffff;

  while (!t.checkExpired()) {
    int tempLeft = checkSensor(leftPhoto);
    int tempRight = checkSensor(rightPhoto);

    if (tempLeft < leftMin)
      leftMin = tempLeft;
    else if (tempLeft > leftMax)
      leftMax = tempLeft;
    if (tempRight < rightMin)
      rightMin = tempRight;
    else if (tempRight > rightMax)
      rightMax = tempRight;
  }

  leftThres = (leftMax + leftMin) / 2;
  rightThres = (rightMax + rightMin) / 2;

//  Serial.print("leftThres: ");
//  Serial.println(leftThres);
//  Serial.print("rightThres: ");
//  Serial.println(rightThres);
}


volatile uint8_t dataReady = 0;
volatile uint16_t code = 0;
volatile uint8_t index = 0;

// check for turn signal
int getTurnSignal() {
  if (dataReady)
  {
    dataReady = 0;
    return code;
  }
  return 0;
}

volatile uint16_t fallingEdge = 0;
volatile uint16_t risingEdge = 0;

//ICR1 is the input capture ("timestamp") register
//capture happens on PB0/Arduino pin 8
ISR(TIMER1_CAPT_vect)
{
  if (!(TCCR1B & (1 << ICES1))) //if we're looking for FALLING edges
  {
    fallingEdge = ICR1;
    TCCR1B |= (1 << ICES1); //now set to look for a rising edge
  }

  else //we must be looking for a RISING edge
  {
    risingEdge = ICR1;
    TCCR1B &= ~(1 << ICES1); //set to look for a falling edge

    //and process
    uint16_t delta = risingEdge - fallingEdge; //length of pulse, in timer counts
    delta /= 2; //scaled to us

    if (delta > 2250 && delta < 2750) //start pulse
    {
      index = 0;
      code = 0; //reset code
      dataReady = 0; //clobber previous read if it wasn't processed
    }

    else if (delta > 500 && delta < 800) //short pulse
    {
      index++;
    }

    else if (delta > 1100 && delta < 1500) //long pulse
    {
      code += (1 << index);
      index++;
    }

    else //error
    {
      index = 0; //start over
      code = 0;
    }

    if (index == 12) dataReady = 1;
  }
}


// check if left or right turn signal
void checkTurnSignal(int ts) {
  // left turn signal
  if (ts == 147) {
    prepLeft = true;
    prepRight = false;
  }
  // right turn signal
  else if (ts == 146) {
    prepRight = true;
    prepLeft = false;
  }
}

//turn right
void turnRight() {
  Serial.println("Turn right");
  //stop
  digitalWrite(rightIn1, LOW);
  digitalWrite(leftIn1, LOW);


  // continue forward to center of intersection
  digitalWrite(rightIn1, HIGH);
  digitalWrite(leftIn1, HIGH);
  analogWrite(PWMright, turnSpeed);
  analogWrite(PWMleft, turnSpeed);
  turnTimer.start(400);
  while(!turnTimer.checkExpired()){
  }

   //stop
  digitalWrite(rightIn1, LOW);
  digitalWrite(leftIn1, LOW);

  // turn right
  digitalWrite(rightIn2, HIGH);
  digitalWrite(leftIn1, HIGH);

  turnTimer.start(830);
  while(!turnTimer.checkExpired()){
  }

  // stop
  digitalWrite(rightIn2, LOW);
  digitalWrite(leftIn1, LOW);

  //continue straight
  digitalWrite(rightIn1, HIGH);
  digitalWrite(leftIn1, HIGH);
  
  // reset turnSignal
  prepRight = false;
}

// turn left
void turnLeft() {
  Serial.println("Turn left");
  //stop
  digitalWrite(rightIn1, LOW);
  digitalWrite(leftIn1, LOW);

  // continue to center of intersection
  digitalWrite(rightIn1, HIGH);
  digitalWrite(leftIn1, HIGH);
  analogWrite(PWMright, turnSpeed);
  analogWrite(PWMleft, turnSpeed);
  turnTimer.start(400);
  while(!turnTimer.checkExpired()){
  }

   //stop
  digitalWrite(rightIn1, LOW);
  digitalWrite(leftIn1, LOW);

  // turn left 
  digitalWrite(rightIn1, HIGH);
  digitalWrite(leftIn2, HIGH);

  turnTimer.start(830);
  while(!turnTimer.checkExpired()){
  }

  // stop
  digitalWrite(rightIn1, LOW);
  digitalWrite(leftIn2, LOW);

  //continue straight
  digitalWrite(rightIn1, HIGH);
  digitalWrite(leftIn1, HIGH);
  
  // reset turnSignal
  prepLeft = false;
}

void loop() {
  //check for line sensor
  int turnSignal = getTurnSignal();
  checkTurnSignal(turnSignal);

  // if new reading from sensor do PID
  if (newReading){
    PID();
  }
  // check if on line
  unsigned long left = checkSensor(leftPhoto);
  unsigned long right = checkSensor(rightPhoto);


//  Serial.print(left);
//  Serial.print(" ");
//  Serial.println(right);
  
  // continue if on line
  if ((left < leftThres) && (right < rightThres)) {
    analogWrite(PWMright, currSpeed);
    analogWrite(PWMleft, currSpeed);
  }

  // enter intersection
  else if ((left >= leftThres) && (right >= rightThres)) {
    Serial.println("INTERSECTION!!");

    // check if turn right
    if (prepRight) {
      turnRight();
    }
    // check if turn left
    else if (prepLeft) {
      turnLeft();
    }
  }

  //slow left wheel if left sensor is high
  else if (left >= leftThres) {
    analogWrite(PWMleft, currSpeed / 2);  // 28 = 1 V for 9 V source
    analogWrite(PWMright, currSpeed * 3/2);
    Serial.println("Left is dark");
  }

  //slow right wheel if right sensor is high
  else if (right >= rightThres) {
    analogWrite(PWMright, currSpeed / 2);
    analogWrite(PWMleft, currSpeed * 3/2);
    Serial.println("Right is dark");
  }
}
