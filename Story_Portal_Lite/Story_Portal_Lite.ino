#include <FatReader.h>
#include <SdReader.h>
#include <avr/pgmspace.h>
#include "WaveUtil.h"
#include "WaveHC.h"

#define HALL_PIN 14   // Hall sensor pin
#define DEBOUNCE 100  // button debouncer
#define TICK_FILE_NAME "tick55.WAV" // name of the tick WAV file on the SD card 

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
  putstring("\n\rSD I/O error: ");
  Serial.print(card.errorCode(), HEX);
  putstring(", ");
  Serial.println(card.errorData(), HEX);
  while(1);
}

void setup() {
  
  // set up serial port @ 9600 baud - this can be any speed really
  Serial.begin(9600);
  
  // This can help with debugging, running out of RAM is bad
  // if this is under 150 bytes it may spell trouble!
  putstring("Free RAM: ");
  Serial.println(freeRam());
  
  // Setup hall sensor
  // enable pull-up resistors on switch pins (analog inputs)
  putstring("Hall sensor listening on pin ");
  Serial.println(HALL_PIN, DEC);
  digitalWrite(HALL_PIN, HIGH);
 
  putstring_nl("Setting up wave shield...");
  // This sets up the wave shield
  // Set the output pins for the DAC control. This pins are defined in the library
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
 
  //  if (!card.init(true)) { //play with 4 MHz spi if 8MHz isn't working for you
  if (!card.init()) {         //play with 8 MHz spi (default faster!)  
    putstring_nl("Card init. failed!");  // Something went wrong, lets print out why
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
    putstring_nl("No valid FAT partition!");
    sdErrorCheck();      // Something went wrong, lets print out why
    while(1);                            // then 'halt' - do nothing!
  }
  
  // Lets tell the user about what we found
  putstring("Using partition ");
  Serial.print(part, DEC);
  putstring(", type is FAT");
  Serial.println(vol.fatType(),DEC);     // FAT16 or FAT32?
  
  // Try to open the root directory
  if (!root.openRoot(vol)) {
    putstring_nl("Can't open root dir!"); // Something went wrong,
    while(1);                             // then 'halt' - do nothing!
  }
  
  // This can help with debugging, running out of RAM is bad
  // if this is under 150 bytes it may spell trouble!
  putstring("Free RAM: ");
  Serial.println(freeRam());
  
  // Whew! We got past the tough parts.
  putstring_nl("I'm ready for you to spin the wheel!");
}

void loop() {
  switch (check_hall_switch()) {
    case 1:
      playfile(TICK_FILE_NAME);
      break;
  }
}

boolean check_hall_switch()
{
  static byte previous;
  static long time;
  byte reading;
  byte pressed = false;
  pressed = 0;

  reading = digitalRead(HALL_PIN);
  if (reading == LOW && previous == HIGH && millis() - time > DEBOUNCE)
  {
      // switch pressed
      time = millis();
      pressed = true;
  }
  previous = reading;
  
  if (pressed) {
    Serial.print("Tick goes the wheel..."); 
    Serial.print("\n");
  }
  return (pressed);
}


void playfile(char *name) {
  // see if the wave object is currently doing something
  if (wave.isplaying) {// already playing something, so stop it!
    wave.stop(); // stop it
  }

  // look in the root directory and open the file
  if (!f.open(root, name)) {
    putstring("Couldn't open file "); Serial.print(name); return;
  }

  // OK read the file and turn it into a wave object
  if (!wave.create(f)) {
    putstring_nl("Not a valid WAV"); return;
  }
  
  // ok time to play! start playback
  wave.play();
}
