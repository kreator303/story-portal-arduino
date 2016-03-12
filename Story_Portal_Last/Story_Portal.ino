#include <FatReader.h>
#include <SdReader.h>
#include <avr/pgmspace.h>
#include "WaveUtil.h"
#include "WaveHC.h"

#define DEBUG 1 // comment out to omit debug code (saves over 1600 bytes)

#define ERROR_LED_PIN 13 // Pin for onboard LED (usually 13) so we can flash the LED if things are wonky
#define HALL_PIN 14   // Hall sensor pin

#define WHEEL_STATE_PIN 8
#define WHEEL_TICK_PIN 9

#define WHEEL_DEBOUNCE_MS 50  // debounce interval in milliseconds
#define WHEEL_TIMEOUT_MS 3000 // amount of time in milliseconds after which we consider the wheel to have stopped
#define WHEEL_TICKS_BEFORE_SPIN 5 // the number of ticks we need before we consider the wheel to be spinning
#define WHEEL_SLEEP_MS 10000 // the amount of time in milliseconds that we ignore input after the wheel stops
#define WHEEL_TICK_LENGTH_MS 100 // the amount of time in milliseconds that we keep the WHEEL_TICK_PIN high after we get a tick
#define TICK_FILE_NAME "tick.wav" // name of the tick WAV file in the root directory on the SD card 
#define STOP_FILE_NAME "stop.wav" // name of the stop WAV file in the root directory on the SD card 

#define WHEEL_STOPPED 0 // STOPPED wheel state
#define WHEEL_MOVING 1 // MOVING wheel state

volatile byte wheel_state; // holds the current wheel state
volatile long current_tick; // the time of the current tick cycle
volatile long last_tick;  // the last time we had a tick
volatile byte previous_sensor_state; // the previous state of the hall sensor (used for debouncing) 
volatile byte current_sensor_state; // the current state of the hall sensor (used for debouncing) 
volatile unsigned int tick_count; // the number of ticks we've had

SdReader card;    // This object holds the information for the card
FatVolume vol;    // This holds the information for the partition on the card
FatReader root;   // This holds the information for the filesystem on the card
FatReader f;      // This holds the information for the file we're playing

WaveHC wave;      // This is the only wave (audio) object, since we will only play one at a time

#ifdef DEBUG
// this handy function will return the number of bytes currently free in RAM, great for debugging!   
int freeRam(void)
{
  extern int  __bss_end; 
  extern int  *__brkval; 
  int free_memory; 
  if((int)__brkval == 0) {
    free_memory = ((int)&free_memory) - ((int)&__bss_end); 
  }
  else {
    free_memory = ((int)&free_memory) - ((int)__brkval); 
  }
  return free_memory; 
} 

void sdErrorCheck(void)
{
  if (!card.errorCode()) return;
  Serial.print("SD I/O ERROR (");
  Serial.print(card.errorCode(), HEX);
  Serial.print(") ");
  Serial.println(card.errorData(), HEX);
}
#endif

void setup() {

  // set up error led pin mode to output
  pinMode(ERROR_LED_PIN, OUTPUT);
  
#ifdef DEBUG  
  // set up serial port
  Serial.begin(115200);
    
  // Setup hall sensor
  // enable pull-up resistors on switch pins (analog inputs)
  Serial.print("HALL PIN "); Serial.println(HALL_PIN, DEC);
#endif
  digitalWrite(HALL_PIN, HIGH); // enable pull up resistor

  // Set up wave shield
#ifdef DEBUG
  Serial.print("WAVE INIT ");
#endif
  // Set the output pins for the DAC control. This pins are defined in the library
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
 
  //  if (!card.init(true)) { //play with 4 MHz spi if 8MHz isn't working for you
  if (!card.init()) {         //play with 8 MHz spi (default faster!)  
#ifdef DEBUG
    Serial.print("FAILED ");  // Something went wrong, lets print out why
    sdErrorCheck();
#endif
    while(1); // then 'halt' - do nothing!
  }
  
  // enable optimize read - some cards may timeout. Disable if you're having problems
  card.partialBlockRead(true);
 
  // Now we will look for a FAT partition!
  uint8_t part;
  for (part = 0; part < 5; part++) {     // we have up to 5 slots to look in
    if (vol.init(card, part)) 
      break;                             // we found one, lets bail
  }
  if (part == 5) {                       // if we ended up not finding one  :(
#ifdef DEBUG
    Serial.println("NO FAT PARTITION");
    sdErrorCheck();      // Something went wrong, lets print out why
#endif
    while(1); // then 'halt' - do nothing!
  }
  
#ifdef DEBUG
  // Lets tell the user about what we found
  Serial.print("FAT PARTITION ");
  Serial.print(part, DEC);
  Serial.println(vol.fatType(),DEC);     // FAT16 or FAT32?
#endif  
  // Try to open the root directory
  if (!root.openRoot(vol)) {
#ifdef DEBUG
    Serial.println("CAN'T OPEN ROOT"); // Something went wrong,
#endif
    while(1); // then 'halt' - do nothing!
  }
  
#ifdef DEBUG
  // If this is under 150 bytes it may spell trouble!
  Serial.print("RAM "); Serial.println(freeRam());
  
  // Whew! We got past the tough parts.
  Serial.println("READY");
#endif

  // Set initial wheel state
  pinMode(WHEEL_STATE_PIN, OUTPUT);
  digitalWrite(WHEEL_STATE_PIN, LOW);
  pinMode(WHEEL_TICK_PIN, OUTPUT);
  digitalWrite(WHEEL_TICK_PIN, LOW);
  wheel_state = WHEEL_STOPPED;
  tick_count = 0;

  // error pin off
  digitalWrite(ERROR_LED_PIN, LOW);

}

void loop() {
  // sets the value of the current tick
  current_tick = millis();
  
  // reads the hall sensor and sets the wheel state
  set_wheel_state();
  
  // do other things based on the wheel state
  switch (wheel_state) {
    case WHEEL_STOPPED:
       break;
    case WHEEL_MOVING:
       break; 
  }
  
}

void set_wheel_state() {

  // set the tick pin LOW if it has been on long enough
  if (current_tick - last_tick > WHEEL_TICK_LENGTH_MS) {
       digitalWrite(WHEEL_TICK_PIN, LOW); 
  }
  
  // Has it been more than WHEEL_TIMEOUT_MS since our last tick?
  if (current_tick - last_tick > WHEEL_TIMEOUT_MS) {
    tick_count = 0;
    wheel_state = WHEEL_STOPPED;
    digitalWrite(WHEEL_STATE_PIN, LOW);
  }

  current_sensor_state = digitalRead(HALL_PIN);

  if (current_sensor_state == LOW && previous_sensor_state == HIGH && current_tick - last_tick > WHEEL_DEBOUNCE_MS) {
    // magnet detected - TICK

    digitalWrite(WHEEL_TICK_PIN, HIGH);
    tick_count++;
    playfile(TICK_FILE_NAME);

#ifdef DEBUG
    Serial.print("TICK "); Serial.print(tick_count, DEC); Serial.print(" ");
#endif
    
    // check to see if we have reached the minimum amount of ticks we need to consider the wheel spinning 
    if (wheel_state == WHEEL_STOPPED && tick_count >= WHEEL_TICKS_BEFORE_SPIN) {
#ifdef DEBUG
//      Serial.print("STATE "); Serial.print(wheel_state, DEC); Serial.print(" ");
#endif
      wheel_state = WHEEL_MOVING;
      digitalWrite(WHEEL_STATE_PIN, HIGH);
    }
    
    last_tick = current_tick;

#ifdef DEBUG
    // If this is under 150 bytes it may spell trouble!
//    Serial.print("RAM "); Serial.print(freeRam()); Serial.print(" ");
    Serial.print("STATE "); Serial.print(wheel_state, DEC); Serial.print(" ");
    Serial.println("");
#endif

  } else {
    // no magnet detected
    
    // if the wheel is moving, check to see if we haven't received a tick in WHEEL_TIMEOUT_MS
    if (wheel_state == WHEEL_MOVING && current_tick - last_tick >= WHEEL_TIMEOUT_MS) {
      // The wheel has stopped
      wheel_state = WHEEL_STOPPED;
      tick_count = 0;
      playfile(STOP_FILE_NAME);
      digitalWrite(WHEEL_STATE_PIN, LOW);
#ifdef DEBUG
      Serial.print("SLEEP "); Serial.print(WHEEL_SLEEP_MS, DEC); 
      Serial.print(" ");
      Serial.print("STATE "); Serial.print(wheel_state, DEC); 
      Serial.println("");
#endif
      delay(WHEEL_SLEEP_MS);
#ifdef DEBUG
      Serial.print("RESUME");
      Serial.println("");
#endif
    }

  }
  
  previous_sensor_state = current_sensor_state;
  
}

void playfile(char *name) {
  // see if the wave object is currently doing something
  if (wave.isplaying) {
      wave.stop(); // stop it
  }

  // look in the root directory and open the file
  if (!f.open(root, name)) {
#ifdef DEBUG
    Serial.print("CAN'T OPEN "); Serial.print(name); 
#endif    
    return;
  }

  // OK read the file and turn it into a wave object
  if (!wave.create(f)) {
#ifdef DEBUG
    Serial.println("INVALID WAV"); 
#endif    
    return;
  }
  
  // ok time to play! start playback
  wave.play();
}
