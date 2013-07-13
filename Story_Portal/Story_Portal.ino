#include <FatReader.h>
#include <SdReader.h>
#include <avr/pgmspace.h>
#include "WaveUtil.h"
#include "WaveHC.h"

#define ERROR_LED_PIN 13 // Pin for onboard LED (usually 13) so we can flash the LED if things are wonky
#define HALL_PIN 14   // Hall sensor pin

#define DEBOUNCE 100  // button debounce interval

#define TICK_FILE_NAME "tick.WAV" // name of the tick WAV file on the SD card 
#define STOP_FILE_NAME "stop.WAV" // name of the stop WAV file on the SD card 

#define WHEEL_STOPPED 0
#define WHEEL_MOVING 1
#define WHEEL_STOP_THRESHOLD_MS 3000
#define WHEEL_TICKS_BEFORE_SPIN 5

volatile byte wheel_state;
volatile long current_tick;
volatile long last_tick; 
volatile byte previous_sensor_state;
volatile unsigned int tick_count;

SdReader card;    // This object holds the information for the card
FatVolume vol;    // This holds the information for the partition on the card
FatReader root;   // This holds the information for the filesystem on the card
FatReader f;      // This holds the information for the file we're playing

WaveHC wave;      // This is the only wave (audio) object, since we will only play one at a time

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
  Serial.print("\n\rSD I/O error: ");
  Serial.print(card.errorCode(), HEX);
  Serial.print(", ");
  Serial.println(card.errorData(), HEX);
  while(1);
}

void setup() {
  
  // set up serial port
  Serial.begin(9600);
  
  // This can help with debugging, running out of RAM is bad
  // if this is under 150 bytes it may spell trouble!
  Serial.print("Free RAM: "); Serial.println(freeRam());
  
  // Setup hall sensor
  // enable pull-up resistors on switch pins (analog inputs)
  Serial.print("Hall sensor listening on pin "); Serial.println(HALL_PIN, DEC);
  digitalWrite(HALL_PIN, HIGH); // enable pull up resistor
 
  // Set up wave shield
  Serial.println("Setting up wave shield...");

  // Set the output pins for the DAC control. This pins are defined in the library
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
 
  //  if (!card.init(true)) { //play with 4 MHz spi if 8MHz isn't working for you
  if (!card.init()) {         //play with 8 MHz spi (default faster!)  
    Serial.println("Card init. failed!");  // Something went wrong, lets print out why
    sdErrorCheck();
    while(1);                            // then 'halt' - do nothing!
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
    Serial.println("No valid FAT partition!");
    sdErrorCheck();      // Something went wrong, lets print out why
    while(1);                            // then 'halt' - do nothing!
  }
  
  // Lets tell the user about what we found
  Serial.print("Using partition ");
  Serial.print(part, DEC);
  Serial.print(", type is FAT");
  Serial.println(vol.fatType(),DEC);     // FAT16 or FAT32?
  
  // Try to open the root directory
  if (!root.openRoot(vol)) {
    Serial.println("Can't open root dir!"); // Something went wrong,
    while(1);                             // then 'halt' - do nothing!
  }
  
  // setup initial wheel state
  wheel_stop();
  
  // This can help with debugging, running out of RAM is bad
  // if this is under 150 bytes it may spell trouble!
  Serial.print("Free RAM: ");
  Serial.println(freeRam());
  
  // Whew! We got past the tough parts.
  Serial.println("I'm ready for you to spin the wheel!");
}

void loop() {
  current_tick = millis();
  byte reading = digitalRead(HALL_PIN);

  switch (wheel_state) {
    case WHEEL_STOPPED:
      if (reading == LOW && previous_sensor_state == HIGH && current_tick - last_tick > DEBOUNCE)
      {
        // magnet detected
        Serial.println("Tick..."); 
        
        // check the tick count
        if (tick_count > WHEEL_TICKS_BEFORE_SPIN) {
          wheel_state = WHEEL_MOVING;
          Serial.println("The wheel is spinning.");
        }
        
        last_tick = current_tick;
        tick_count++;  
        
        playfile(TICK_FILE_NAME);
    
      } else {
        // no magnet detected - figure out if the wheel is stopped
        if (current_tick - last_tick > WHEEL_STOP_THRESHOLD_MS) {
          // wheel is stopped
          playfile(STOP_FILE_NAME);
          wheel_stop();
        } 
      }
    break;
    case WHEEL_MOVING: 
      if (reading == LOW && previous_sensor_state == HIGH && current_tick - last_tick > DEBOUNCE)
      {
        // magnet detected
        Serial.println("Tick..."); 
    
        wheel_state = WHEEL_MOVING;
        last_tick = current_tick;
        tick_count++;
        
        playfile(TICK_FILE_NAME);
    
      } else {
        // no magnet detected - figure out if the wheel is stopped
        if (current_tick - last_tick > WHEEL_STOP_THRESHOLD_MS) {
          // wheel is stopped
          playfile(STOP_FILE_NAME);
          wheel_stop();
        } 
      }
    break;
  }

  previous_sensor_state = reading;
}

// Sets the initial state of the wheel and clears the speed buffer
void wheel_stop() {
  wheel_state = WHEEL_STOPPED;
  current_tick = 0;
  last_tick = 0;
  tick_count = 0;
  Serial.println("The wheel has stopped.");
}

void playfile(char *name) {
  // see if the wave object is currently doing something
  if (wave.isplaying) {// already playing something, so stop it!
    wave.stop(); // stop it
  }

  // look in the root directory and open the file
  if (!f.open(root, name)) {
    Serial.print("Couldn't open file "); Serial.print(name); return;
  }

  // OK read the file and turn it into a wave object
  if (!wave.create(f)) {
    Serial.println("Not a valid WAV"); return;
  }
  
  // ok time to play! start playback
  wave.play();
}
