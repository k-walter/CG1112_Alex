#include <serialize.h>
#include <avr/sleep.h>
#include <stdarg.h>

#include "packet.h"
#include "constants.h"

#define PRR_TWI_MASK            0b10000000
#define PRR_SPI_MASK            0b00000100
#define ADCSRA_ADC_MASK         0b10000000
#define PRR_ADC_MASK            0b00000001
#define PRR_TIMER2_MASK         0b01000000
#define PRR_TIMER0_MASK         0b00100000
#define PRR_TIMER1_MASK         0b00001000
#define SMCR_SLEEP_ENABLE_MASK  0b00000001
#define SMCR_IDLE_MODE_MASK     0b11110001

typedef enum
{
  STOP = 0,
  FORWARD = 1,
  BACKWARD = 2,
  LEFT = 3,
  RIGHT = 4
} TDirection;

volatile TDirection dir = STOP;

/*
   Alex's configuration constants
*/

// Number of ticks per revolution from the
// wheel encoder.
#define COUNTS_PER_REV      195 // 185

#define PIN5                (1<<5)
#define PIN6                (1<<6)
#define PIN2                (1<<2)
#define PIN3                (1<<3)
// Wheel circumference in cm.
// We will use this to calculate forward/backward distance traveled
// by taking revs * WHEEL_CIRC

#define WHEEL_CIRC          22
const float WIDTH = 3.14159265 * 12.5 / 8;

// Motor control pins. You need to adjust these till
// Alex moves in the correct direction
#define LF                  10   // Left forward pin
#define LR                  11   // Left reverse pin
#define RF                  6  // Right forward pin
#define RR                  5  // Right reverse pin

/*
      Alex's State Variables
*/

// Store the ticks from Alex's left and
// right encoders for moving forward and backwards
volatile unsigned long leftForwardTicks;
volatile unsigned long rightForwardTicks;
volatile unsigned long leftReverseTicks;
volatile unsigned long rightReverseTicks;


// left and right encoder ticks for turning
volatile unsigned long leftForwardTicksTurns;
volatile unsigned long rightForwardTicksTurns;
volatile unsigned long leftReverseTicksTurns;
volatile unsigned long rightReverseTicksTurns;

// Store the revolutions on Alex's left
// and right wheels
volatile unsigned long leftRev;
volatile unsigned long rightRev;

// Forward and backward distance traveled
volatile unsigned long forwardDist;
volatile unsigned long reverseDist;

// variables to keep track of whether we have moved a commanded distance
unsigned long deltaDist;
unsigned long newDist;
const float leftVal = 0.8; // 0.7


/*

   Alex Communication Routines.

*/

TResult readPacket(TPacket *packet)
{
  // Reads in data from the serial port and
  // deserializes it.Returns deserialized
  // data in "packet".

  char buffer[PACKET_SIZE];
  int len;

  len = readSerial(buffer);

  if (len == 0)
    return PACKET_INCOMPLETE;
  else
    return deserialize(buffer, len, packet);

}

void sendStatus()
{
  // Implement code to send back a packet containing key
  // information like leftTicks, rightTicks, leftRevs, rightRevs
  // forwardDist and reverseDist
  // Use the params array to store this information, and set the
  // packetType and command files accordingly, then use sendResponse
  // to send out the packet. See sendMessage on how to use sendResponse.
  //
  TPacket statusPacket;
  statusPacket.packetType = PACKET_TYPE_RESPONSE;
  statusPacket.command = RESP_STATUS;
  statusPacket.params[0] = leftForwardTicks;
  statusPacket.params[1] = rightForwardTicks;
  statusPacket.params[2] = leftReverseTicks;
  statusPacket.params[3] = rightReverseTicks;
  statusPacket.params[4] = leftForwardTicksTurns;
  statusPacket.params[5] = rightForwardTicksTurns;
  statusPacket.params[6] = leftReverseTicksTurns;
  statusPacket.params[7] = rightReverseTicksTurns;
  statusPacket.params[8] = forwardDist;
  statusPacket.params[9] = reverseDist;
  sendResponse(&statusPacket);

}

void sendMessage(const char *message)
{
  // Sends text messages back to the Pi. Useful
  // for debugging.

  TPacket messagePacket;
  messagePacket.packetType = PACKET_TYPE_MESSAGE;
  strncpy(messagePacket.data, message, MAX_STR_LEN);
  sendResponse(&messagePacket);
}

void dbprint(char *format, ...) {
  va_list args;
  char buffer[128];

  va_start(args, format);
  vsprintf(buffer, format, args);
  sendMessage(buffer);
}

void sendBadPacket()
{
  // Tell the Pi that it sent us a packet with a bad
  // magic number.

  TPacket badPacket;
  badPacket.packetType = PACKET_TYPE_ERROR;
  badPacket.command = RESP_BAD_PACKET;
  sendResponse(&badPacket);

}

void sendBadChecksum()
{
  // Tell the Pi that it sent us a packet with a bad
  // checksum.

  TPacket badChecksum;
  badChecksum.packetType = PACKET_TYPE_ERROR;
  badChecksum.command = RESP_BAD_CHECKSUM;
  sendResponse(&badChecksum);
}

void sendBadCommand()
{
  // Tell the Pi that we don't understand its
  // command sent to us.

  TPacket badCommand;
  badCommand.packetType = PACKET_TYPE_ERROR;
  badCommand.command = RESP_BAD_COMMAND;
  sendResponse(&badCommand);

}

void sendBadResponse()
{
  TPacket badResponse;
  badResponse.packetType = PACKET_TYPE_ERROR;
  badResponse.command = RESP_BAD_RESPONSE;
  sendResponse(&badResponse);
}

void sendOK()
{
  TPacket okPacket;
  okPacket.packetType = PACKET_TYPE_RESPONSE;
  okPacket.command = RESP_OK;
  sendResponse(&okPacket);
}

void sendResponse(TPacket *packet)
{
  // Takes a packet, serializes it then sends it out
  // over the serial port.
  char buffer[PACKET_SIZE];
  int len;

  len = serialize(buffer, packet, sizeof(TPacket));
  writeSerial(buffer, len);
}


/*
   Setup and start codes for external interrupts and
   pullup resistors.

*/
// Enable pull up resistors on pins 2 and 3
void enablePullups()
{
  // Use bare-metal to enable the pull-up resistors on pins
  // 2 and 3. These are pins PD2 and PD3 respectively.
  // We set bits 2 and 3 in DDRD to 0 to make them inputs.
  DDRD &= 0b11110011;

  PORTD |= 0b00001100;
}

// Functions to be called by INT0 and INT1 ISRs.
void leftISR()
{
  if (dir == FORWARD) {
    leftForwardTicks++;
    forwardDist = (unsigned long) ((float) leftForwardTicks / COUNTS_PER_REV * WHEEL_CIRC);
  }
  else if (dir == LEFT) {
    leftReverseTicksTurns++;
    leftRev = ((float)leftReverseTicksTurns / COUNTS_PER_REV * WHEEL_CIRC);
  }

  else if (dir == RIGHT) {
    leftForwardTicksTurns++;
    rightRev = ((float)leftForwardTicksTurns / COUNTS_PER_REV * WHEEL_CIRC);
  }
  else if (dir == BACKWARD) {
    leftReverseTicks++;
    reverseDist = (unsigned long) ((float) leftReverseTicks / COUNTS_PER_REV * WHEEL_CIRC);
  }
}

void rightISR()
{
  if (dir == FORWARD) {
    rightForwardTicks++;
  }
  else if (dir == LEFT) {
    rightForwardTicksTurns++;
  }

  else if (dir == RIGHT) {
    rightReverseTicksTurns++;
  }
  else if (dir == BACKWARD) {
    rightReverseTicks++;
  }
}

// Set up the external interrupt pins INT0 and INT1
// for falling edge triggered. Use bare-metal.
void setupEINT()
{
  // Use bare-metal to configure pins 2 and 3 to be
  // falling edge triggered. Remember to enable
  // the INT0 and INT1 interrupts.
  EICRA |= 0b00001010;
  EIMSK |= 0b00000011;
}

// Implement the external interrupt ISRs below.
// INT0 ISR should call leftISR while INT1 ISR
// should call rightISR.

ISR (INT0_vect) {
  leftISR();
}

ISR (INT1_vect) {
  rightISR();
}


// Implement INT0 and INT1 ISRs above.

/*
   Setup and start codes for serial communications

*/
// Set up the serial connection. For now we are using
// Arduino Wiring, you will replace this later
// with bare-metal code.
void setupSerial()
{
  // To replace later with bare-metal.
  Serial.begin(9600);

//   // Try u2x mode first
//   UBRR0 = 207; // = (F_CPU / 4 / baud - 1) / 2

//   uint16_t baud_setting = (F_CPU / 4 / baud - 1) / 2;
//   *_ucsra = 1 << U2X0;

//   // hardcoded exception for 57600 for compatibility with the bootloader
//   // shipped with the Duemilanove and previous boards and the firmware
//   // on the 8U2 on the Uno and Mega 2560. Also, The baud_setting cannot
//   // be > 4095, so switch back to non-u2x mode if the baud rate is too
//   // low.
//   if (((F_CPU == 16000000UL) && (baud == 57600)) || (baud_setting >4095))
//   {
//     *_ucsra = 0;
//     baud_setting = (F_CPU / 8 / baud - 1) / 2;
//   }

//   // assign the baud_setting, a.k.a. ubrr (USART Baud Rate Register)
//   *_ubrrh = baud_setting >> 8;
//   *_ubrrl = baud_setting;

//   _written = false;

//   //set the data bits, parity, and stop bits
// #if defined(__AVR_ATmega8__)
//   config |= 0x80; // select UCSRC register (shared with UBRRH)
// #endif
//   *_ucsrc = config;
  
//   sbi(*_ucsrb, RXEN0);
//   sbi(*_ucsrb, TXEN0);
//   sbi(*_ucsrb, RXCIE0);
//   cbi(*_ucsrb, UDRIE0);

}

// Start the serial connection. For now we are using
// Arduino wiring and this function is empty. We will
// replace this later with bare-metal code.

void startSerial()
{
  // Empty for now. To be replaced with bare-metal code
  // later on.

}

// Read the serial port. Returns the read character in
// ch if available. Also returns TRUE if ch is valid.
// This will be replaced later with bare-metal code.

int readSerial(char *buffer)
{

  int count = 0;

  while (Serial.available())
    buffer[count++] = Serial.read();

  return count;
}

// Write to the serial port. Replaced later with
// bare-metal code

void writeSerial(const char *buffer, int len)
{
  Serial.write(buffer, len);
}

/*
   Alex's motor drivers.

*/ 
void setupMotors()
{
  /* Our motor set up is:
        RR  A1IN - Pin 5, PD5, OC0B
        RF  A2IN - Pin 6, PD6, OC0A
        LF  B1IN - Pin 10, PB2, OC1B
        LR  B2In - pIN 11, PB3, OC2A

        OLD : 
        LF  B1IN - Pin 3, PD3, OC2B
  */
  DDRD |= (PIN6 | PIN5);
  DDRB |= (PIN3 | PIN2);
  // PORTB = PORTD = 0;
}

// Start the PWM for Alex's motors.
// We will implement this later. For now it is
// blank.
void startMotors()
{
  // Right motor
  TCNT0 = 0; // counter
  // TIMSK0 = 0b110; // interrupt on OCIEA = 1 OCIEB = 1
  OCR0A = OCR0B = 0; // compare
  TCCR0B = 0b00000011; // prescalar 64
  // TCCR0A = 0b10100001; // sets Phase Correct, set/clear for OC0B, OC0A

  // Left motor
  TCNT2 = TCNT1H = TCNT1L = 0;
//  TIMSK2 |= 0b010;
//  TIMSK1 |= 0b100;
  OCR2A = OCR1B = 0;
  TCCR2B = TCCR1B = 0b11;
//  TCCR2A = 0b10000001;
//  TCCR1A = 0b00100001;
}

// Convert percentages to PWM values
int pwmVal(float speed)
{
  if (speed < 0.0)
    speed = 0;

  if (speed > 100.0)
    speed = 100.0;

  return (int) ((speed / 100.0) * 255.0);
}

// Move Alex forward "dist" cm at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// move forward at half speed.
// Specifying a distance of 0 means Alex will
// continue moving forward indefinitely.
void forward(float dist, float speed = 75)
{
  // code to tell us how far to move
  if (dist > 0)
    deltaDist = dist;
  else
    deltaDist = 9999999;
  newDist = forwardDist + deltaDist;

  dir = FORWARD;

  int val = pwmVal(speed);
  //analogWrite(LF, (float)val * leftVal);
  //analogWrite(RF, val);
  //analogWrite(LR, 0);
  //analogWrite(RR, 0);
  
  // Right motor
  TCCR0A = (TCCR0A & 0b00001111) | 0b10000000; // sets Phase Correct, set/clear for OC0A
  OCR0A = val; PORTD &= ~PIN5; // compare

  // Left motor
  TCCR2A &= 0b00001111;
  TCCR1A = (TCCR1A & 0b00001111) | 0b00100000; // sets Phase Correct, set/clear for OC1B
  OCR1B = (float)val * leftVal; // compare
  PORTB &= ~PIN3;
}

// Reverse Alex "dist" cm at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// reverse at half speed.
// Specifying a distance of 0 means Alex will
// continue reversing indefinitely.
void reverse(float dist, float speed = 75)
{
  if (dist > 0)
    deltaDist = dist;
  else
    deltaDist = 9999999;
  newDist = reverseDist + deltaDist;

  dir = BACKWARD;

  int val = pwmVal(speed);
//  analogWrite(LR, (float)val * leftVal);
//  analogWrite(RR, val);
//  analogWrite(LF, 0);
//  analogWrite(RF, 0);

  // Right motor
  TCCR0A = (TCCR0A & 0b00001111) | 0b00100000; // sets Phase Correct, set/clear for OC0A
  OCR0B = val; PORTD &= ~PIN6; // compare

  // Left motor
  TCCR1A &= 0b00001111;
  TCCR2A = (TCCR2A & 0b00001111) | 0b10000000; // sets Phase Correct, set/clear for OC1B
  OCR2A = (float)val * leftVal; // compare
  PORTB &= ~PIN2;
}

// Turn Alex left "ang" degrees at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// turn left at half speed.
// Specifying an angle of 0 degrees will cause Alex to
// turn left indefinitely.
void left(float ang, float speed = 90)
{
  deltaDist = (ang > 0) ? max(1, ang * WIDTH / 90) : 9999999;
  newDist = leftRev + deltaDist;
  
  dir = LEFT;

  int val = pwmVal(speed);
//  analogWrite(LR, (float)val * leftVal);
//  analogWrite(RF, val);
//  analogWrite(LF, 0);
//  analogWrite(RR, 0);

  // Right motor
  TCCR0A = (TCCR0A & 0b00001111) | 0b10000000; // sets Phase Correct, set/clear for OC0A
  OCR0A = val; PORTD &= ~PIN5; // compare

  // Left motor
  TCCR1A &= 0b00001111;
  TCCR2A = (TCCR2A & 0b00001111) | 0b10000000; // sets Phase Correct, set/clear for OC1B
  OCR2A = (float)val * leftVal; // compare
  PORTB &= ~PIN2;
}

// Turn Alex right "ang" degrees at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// turn left at half speed.
// Specifying an angle of 0 degrees will cause Alex to
// turn right indefinitely.
void right(float ang, float speed = 90)
{
  deltaDist = (ang > 0) ? max(1, ang * WIDTH / 90) : 9999999;
  newDist = rightRev + deltaDist;
  
  dir = RIGHT;

  int val = pwmVal(speed);
//  analogWrite(RR, val);
//  analogWrite(LF, (float)val * leftVal);
//  analogWrite(LR, 0);
//  analogWrite(RF, 0);

  // Right motor
  TCCR0A = (TCCR0A & 0b00001111) | 0b00100000; // sets Phase Correct, set/clear for OC0A
  OCR0B = val; PORTD &= ~PIN6; // compare

  // Left motor
  TCCR2A &= 0b00001111;
  TCCR1A = (TCCR1A & 0b00001111) | 0b00100000; // sets Phase Correct, set/clear for OC1B
  OCR1B = (float)val * leftVal; // compare
  PORTB &= ~PIN3;
}

// Stop Alex. To replace with bare-metal code later.
void stop()
{
  dir = STOP;

//  analogWrite(LF, 0);
//  analogWrite(LR, 0);
//  analogWrite(RF, 0);
//  analogWrite(RR, 0);

  // Right motor
  TCCR0A &= 0b00001111;
  PORTD &= ~(PIN5 | PIN6); // compare

  // Left motor
  TCCR2A &= 0b00001111;
  TCCR1A &= 0b00001111;
  PORTB &= ~(PIN2 | PIN3);
}

/*
   Alex's setup and run codes

*/

// Clears all our counters
void clearCounters()
{
  leftForwardTicks = 0;
  rightForwardTicks = 0;
  leftReverseTicks = 0;
  rightReverseTicks = 0;


  leftForwardTicksTurns = 0;
  rightForwardTicksTurns = 0;
  leftReverseTicksTurns = 0;
  rightReverseTicksTurns = 0;

  forwardDist = 0;
  reverseDist = 0;
}

// Clears one particular counter
void clearOneCounter(int which)
{
  clearCounters();
}
// Intialize Vincet's internal states

void initializeState()
{
  clearCounters();
}

void handleCommand(TPacket *command)
{
  switch (command->command)
  {
    // For movement commands, param[0] = distance, param[1] = speed.
    case COMMAND_FORWARD:
      sendOK();
      forward((float) command->params[0], (float) command->params[1]);
      break;

    case COMMAND_REVERSE:
      sendOK();
      reverse((float) command->params[0], (float) command->params[1]);
      break;

    case COMMAND_TURN_LEFT:
      sendOK();
      left((float) command->params[0], (float) command->params[1]);
      break;

    case COMMAND_TURN_RIGHT:
      sendOK();
      right((float) command->params[0], (float) command->params[1]);
      break;

    case COMMAND_STOP:
      sendOK();
      stop();
      break;
    case COMMAND_GET_STATS:
      sendOK();
      sendStatus();
      break;
    case COMMAND_CLEAR_STATS:
      sendOK();
      clearOneCounter(command->params[0]);
      break;

    default:
      sendBadCommand();
  }
}

void waitForHello()
{
  int exit = 0;

  while (!exit)
  {
    TPacket hello;
    TResult result;

    do
    {
      result = readPacket(&hello);
    } while (result == PACKET_INCOMPLETE);

    if (result == PACKET_OK)
    {
      if (hello.packetType == PACKET_TYPE_HELLO)
      {
        sendOK();
        exit = 1;
      }
      else
        sendBadResponse();
    }
    else if (result == PACKET_BAD)
    {
      sendBadPacket();
    }
    else if (result == PACKET_CHECKSUM_BAD)
      sendBadChecksum();
  } // !exit
}

// Power reduction code
void WDT_off(void)
{
  /* Global interrupt should be turned OFF here if not
  already done so */
  /* Clear WDRF in MCUSR */
  MCUSR &= ~(1<<WDRF);
  /* Write logical one to WDCE and WDE */
  /* Keep old prescaler setting to prevent unintentional
  time-out */
  WDTCSR |= (1<<WDCE) | (1<<WDE);
  /* Turn off WDT */
  WDTCSR = 0x00;
  /* Global interrupt should be turned ON here if
  subsequent operations after calling this function do
  not require turning off global interrupt */
}

void setupPowerSaving()
{
  // Turn off the Watchdog Timer
  WDT_off();
  // Modify PRR to shut down TWI
  PRR |= PRR_TWI_MASK;
  // Modify PRR to shut down SPICG1112 AY1920S2 Week 11 – Studio 1
  PRR |= PRR_SPI_MASK;
  // Modify ADCSRA to disable ADC,
  // then modify PRR to shut down ADC
  ADCSRA &= ~ADCSRA_ADC_MASK;
  PRR |= PRR_ADC_MASK;
  // Set the SMCR to choose the IDLE sleep mode
  // Do not set the Sleep Enable (SE) bit yet
  SMCR &= SMCR_IDLE_MODE_MASK;
  // Set Port B Pin 5 as output pin, then write a logic LOW
  // to it so that the LED tied to Arduino's Pin 13 is OFF.
  DDRB |= PIN5;
  PORTB &= ~PIN5;
}

void putArduinoToIdle()
{
  // Modify PRR to shut down TIMER 0, 1, and 2
  PRR |= (PRR_TIMER2_MASK | PRR_TIMER0_MASK | PRR_TIMER1_MASK);
  // Modify SE bit in SMCR to enable (i.e., allow) sleep
  SMCR |=  SMCR_SLEEP_ENABLE_MASK;
  // The following function puts ATmega328P’s MCU into sleep;
  // it wakes up from sleep when USART serial data arrives
  sleep_cpu();
  // Modify SE bit in SMCR to disable (i.e., disallow) sleep
  SMCR &= ~SMCR_SLEEP_ENABLE_MASK;
  // Modify PRR to power up TIMER 0, 1, and 2
  PRR &= ~PRR_TIMER2_MASK;
  PRR &= ~PRR_TIMER0_MASK;
  PRR &= ~PRR_TIMER1_MASK;
}


void setup() {
  // put your setup code here, to run once:

  cli();
  setupEINT();
  setupSerial();
  startSerial();
  setupMotors();
  startMotors();
  enablePullups();
  // Power Managment
  initializeState();
  setupPowerSaving();
  sei();
}

void handlePacket(TPacket *packet)
{
  switch (packet->packetType)
  {
    case PACKET_TYPE_COMMAND:
      handleCommand(packet);
      break;

    case PACKET_TYPE_RESPONSE:
      break;

    case PACKET_TYPE_ERROR:
      break;

    case PACKET_TYPE_MESSAGE:
      break;

    case PACKET_TYPE_HELLO:
      break;
  }
}

void loop() {

  // Test Code
  // left(0, 50);
  // while(1);
  // return;

  TPacket recvPacket; // This holds commands from the Pi
  TResult result = readPacket(&recvPacket);
  
  if (result == PACKET_OK)
    handlePacket(&recvPacket);
  else if (result == PACKET_BAD)
  {
    sendBadPacket();
  }
  else if (result == PACKET_CHECKSUM_BAD)
  {
    sendBadChecksum();
  }

  if (deltaDist <= 0) 
        putArduinoToIdle();
  else
  {
    switch (dir) {
      case FORWARD:
        if (forwardDist > newDist)
        {
          deltaDist = 0;
          newDist = 0;
          stop();
          putArduinoToIdle();
        }
        break;
      case BACKWARD:
        if (reverseDist >= newDist)
        {
          deltaDist = 0;
          newDist = 0;
          stop();
          putArduinoToIdle();
        }
        break;
      case STOP:
        deltaDist = 0;
        newDist = 0;
        stop();
        putArduinoToIdle();
        break;
      case LEFT:
        if (leftRev >= newDist) {
          deltaDist = 0;
          newDist = 0;
          stop();
          putArduinoToIdle();
        }
        break;
      case RIGHT:
        if (rightRev >= newDist) {
          deltaDist = 0;
          newDist = 0;
          stop();
          putArduinoToIdle();
        }
        break;
    }
  }
}
