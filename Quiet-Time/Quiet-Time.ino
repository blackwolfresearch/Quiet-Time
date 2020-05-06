#include <EEPROM.h>
#include "LedControl.h"
#include "binary.h"
#include "pitches.h"

//#define DEBUG_SOUND   // debug sound detection level
#define DEBUG         // general debug

// only use for testing so as not to disturb those who actually get to sleep :)
//#define SILENT        

// right now make these mutually exclusive so that the sound
// debugging can dump raw numeric data for graphing purposes
// and not have all the other general messages in there

#ifdef DEBUG_SOUND
#undef DEBUG
#endif

// display states

#define DISP_HAPPY_FACE   1
#define DISP_SAD_FACE     2
#define DISP_NEUTRAL_FACE 3
#define DISP_PROGRESS     4

// general setup parameters
#define NOISE_PENALTY       1       // noise penalty (in minutes)
#define TIMEOUT_MINUTES     10      // number of minutes in timeout
#define CALIBRATION_MINUTES 1       // number of minutes for calibration
#define PENALTY_INTERVAL    30      // minimum number of seconds between penalties

#define PENALTY_THRESHOLD   20      // number of readings to trigger penalty 
#define PENALTY_RATE_LIM    500L    // minimum number of ms between loud noise counts

#define DEBOUNCE_DELAY      50      // ms for debounce timer
#define LONG_PRESS_TIME     1500L   // minimum time in ms to consider button press a "long press"

#define DEF_SOUND_THRESHOLD 550     // default sound threshold - anything below is considered silence

unsigned long timeout_minutes=TIMEOUT_MINUTES;          // number of minutes in timeout
unsigned long penalty_interval=PENALTY_INTERVAL*1000L;  // minimum number of seconds between penalties
unsigned long penalty = 60L*NOISE_PENALTY*1000L;        // actual penalty in ms

int sound_threshold=DEF_SOUND_THRESHOLD;                // sound threshold - anything below is considered silence
int noise_threshold=40;                                 // noise threshold - anything above this is too loud 

int calibration_level;                                  // max noise level seen during calibration

// pin assignments
#define LED_DIN_PIN       12
#define LED_CLK_PIN       10
#define LED_CS_PIN        11

#define PIEZO_PIN         8
#define BUTTON_PIN        2

#define SOUND_SENSOR_PIN  A0

// led control object
LedControl lc=LedControl(LED_DIN_PIN, LED_CLK_PIN, LED_CS_PIN, 1);

// led matrix for different faces - normal orientation for if/when I fix the mounting orientation
/*
byte happy_face[8]= {B00111100,B01000010,B10100101,B10000001,B10100101,B10011001,B01000010,B00111100};
byte sad_face[8]= {B00111100,B01000010,B10100101,B10000001,B10011001,B10100101,B01000010,B00111100};
byte neutral_face[8]={B00111100, B01000010,B10100101,B10000001,B10111101,B10000001,B01000010,B00111100};
*/

// led matrix for different faces - flipped because of the way the display mounted in the case
byte happy_face[8]= {B00111100,B01000010,B10011001,B10100101,B10000001,B10100101,B01000010,B00111100};
byte sad_face[8]= {B00111100,B01000010,B10100101,B10011001,B10000001,B10100101,B01000010,B00111100};
byte neutral_face[8]={B00111100,B01000010,B10000001,B10111101,B10000001,B10100101,B01000010,B00111100};

// notes in the melody:
int melody[] = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
int note_durations[] = {
  4, 8, 8, 4, 4, 4, 4, 4
};

int display_mode=0;                   // what is currently displayed on screen
int display_counter=0;                // counter used to track flashes

// interval tracking variables
unsigned long last_loop_time=0;       // last time loop ran
unsigned long last_penalty_time=0;    // last time penalty was issued
unsigned long last_display_time=0;    // last time display updated
unsigned long last_debounce_time=0;   // debounce timer

unsigned long last_loud_noise_time=0; // last time there was a loud noise reading
unsigned long last_bargraph_time=0;   // last time bargraph was updated

int last_bargraph_level=0;            // last sound level bargraph was at

boolean is_running=false;
boolean calibration_mode=false;

unsigned long max_time=0;             // maximum time to run (in ms)
unsigned long time_per_led=0;         // number of ms per led within matrix
float bargraph_led_increment=0;       // sound differential between representing each bargraph led
long time_remaining=0;                // how much time remaining (in ms)
int loud_noise_count=0;               // number of loud noises in period

int last_button_state = LOW;

// not sure if this is necessary, but it seems sometimes there's a bunch of
// bad data at the start of the analogRead() cycle, so drain any bad data here

void drain_audio_buffer() {
  int i;

  for (i=0;i<1000;i++) {
    analogRead(SOUND_SENSOR_PIN);
  }
}

void init_vars() {
  if (calibration_mode) {
#ifdef DEBUG
    Serial.println("Configuring for calibration mode");
#endif
    sound_threshold=DEF_SOUND_THRESHOLD;
    calibration_level=0;
    timeout_minutes=CALIBRATION_MINUTES;
  } else {
    timeout_minutes=TIMEOUT_MINUTES;
    get_settings();
  }
  
  max_time=60*timeout_minutes*1000;
  time_per_led=max_time/56; 
  time_remaining=max_time;
  last_display_time=0;
  last_bargraph_time=0;
  last_bargraph_level=0;
  bargraph_led_increment=(float)noise_threshold/6.0;
  display_mode=DISP_PROGRESS;
  is_running=true;

  drain_audio_buffer();
}

#ifndef SILENT 
void play_melody() {
  int this_note;
  
 
  for (this_note = 0; this_note < 8; this_note++) {
    // to calculate the note duration, take one second divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int note_duration = 1000 / note_durations[this_note];
    tone(PIEZO_PIN, melody[this_note], note_duration);

    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pause_between_notes = note_duration * 1.30;
    delay(pause_between_notes);
    // stop the tone playing:
    noTone(PIEZO_PIN);
  }
}
#endif

void do_display_update() {
  unsigned long now=millis();
  unsigned long elapsed=now-last_display_time;

  if (last_display_time==0) {
    lc.clearDisplay(0);
  }

  switch (display_mode) {
    case DISP_PROGRESS:
      if (last_display_time == 0 || elapsed > time_per_led) {
        show_progress();
        last_display_time=now;
      }

      break;

    case DISP_HAPPY_FACE:
      if (last_display_time==0) {
        show_face(happy_face);
        last_display_time=now;
#ifndef SILENT
        play_melody();
#endif
      }

      break;
      
    case DISP_NEUTRAL_FACE:
      if (last_display_time==0) {
        show_face(neutral_face);
        last_display_time=now;
      }

      break;
        
    case DISP_SAD_FACE:
      // sad face will flash 3 times, on for 1000 ms, off for 500 ms
      if (display_counter % 2 == 0) {
        if (elapsed > 1000) { 
          lc.clearDisplay(0);
          last_display_time=now;
          ++display_counter;
        }
      } else {
        if (elapsed > 500) {
          show_face(sad_face);
          last_display_time=now;
          ++display_counter;
        }
      }

      // go back to regular progress display after sad face
      if (display_counter > 5) {
        display_mode=DISP_PROGRESS;
        last_display_time=0;
      }

      break;
  }
}

void do_penalty() {
  unsigned long now=millis();
  unsigned long elapsed=now-last_penalty_time;
  unsigned long noise_elapsed=now-last_loud_noise_time;

  if (last_loud_noise_time == 0) {
    last_loud_noise_time=now;
    noise_elapsed=0;
  }

  // do not do penalties for a while after last penalty was issued
  // this gives toddler time to process the "penalty" situation
  if (last_penalty_time > 0 && elapsed < penalty_interval) {
    return;
  }

  // ignore loud noise events that occur too frequently, since the
  // sampling rate is pretty high

  if (noise_elapsed > 0 && noise_elapsed < PENALTY_RATE_LIM) {
    return;
  }

  if (noise_elapsed > penalty_interval) {
    loud_noise_count=1;
  } else {
    ++loud_noise_count;
  }

  //show_bargraph(); 
  last_loud_noise_time=now;

#ifdef DEBUG
  Serial.print("Loud noise count=");
  Serial.println(loud_noise_count);
#endif

  // haven't received enough noise events yet,
  // so don't do penalty just yet. this protects
  // against random loud noises that could happen
  // in the background
  
  if (loud_noise_count < PENALTY_THRESHOLD) {
    return;
  }

#ifdef DEBUG
  Serial.print("Doing penalty of ");
  Serial.println(penalty);
#endif

  // reset counter values
  last_loud_noise_time=0;
  loud_noise_count=0;

  // apply penalty
  time_remaining+=penalty;
  if (time_remaining > max_time) { 
    time_remaining=max_time;
  }

  display_mode=DISP_SAD_FACE;
  display_counter=0;
  last_display_time=0;
  last_penalty_time=now;

#ifndef SILENT
  tone(PIEZO_PIN, 1000, 500);
#endif
}

void show_face(byte *face) {
  int i=0;
  
  for (i=0;i<8;i++) {
    lc.setColumn(0,i,face[i]);
  }
}

// shows bargraph on lower part of led matrix display
// this depicts the loud noise count relative to the penalty threshold

void show_bargraph() {
  int led_count, row, col;

  if (display_mode != DISP_PROGRESS) { return; }
  
  // how many leds (of 1 row) represent the loud noise count relative 
  // to the penalty threshold 
  led_count=(int)(8.0/(float)((float)PENALTY_THRESHOLD/(float)loud_noise_count));
  
  if (loud_noise_count > 0 && led_count == 0) {
    led_count=1;
  }
  
  if (led_count > 8) { 
    led_count=8; 
  }

  // show bargraph representing current noise count vs threshold
  
  for (row=0;row<8;++row) {
    if (row < led_count) {
      lc.setLed(0, row, 0, 1);
    } else {
      lc.setLed(0, row, 0, 0);
    }
  }
}

// this bargraph function makes more sense. it shows the current
// sound level relative to the penalty threshold, but allows for
// representing values twice as large as the threshold. this means
// that, with 4+ leds lit up on the bar, the threshold has been
// crossed and there is room to see by how much

void show_bargraph_sound_level(int sound_level) {
  unsigned long now=millis();
  unsigned long elapsed = now-last_bargraph_time;
  int led_count, row, col;

  if (display_mode != DISP_PROGRESS) { return; }

  // update display no more often than penalty_rate_lim unless sound 
  // level has gone above the previous reading 
  
  if (sound_level < last_bargraph_level && elapsed < PENALTY_RATE_LIM) {
    return;
  }

  last_bargraph_level=sound_level;
  last_bargraph_time=now;
 
  // how many leds (of 1 row) represent the loud noise count relative 
  // to the penalty threshold. float conversion done for better precision
  
  led_count=(int)((float)sound_level/bargraph_led_increment);
  
  if (led_count > 8) { 
    led_count=8; 
  }

  // show bargraph representing current noise count vs threshold
  
  for (row=0;row<8;++row) {
    if (row < led_count) {
      lc.setLed(0, row, 0, 1);
    } else {
      lc.setLed(0, row, 0, 0);
    }
  }
}

// show overall progress toward completion of timeout. 7 of the 8 rows
// are used, for a total of 56 LEDs. the more LEDs illuminated, the more
// time remains

void show_progress() {
  int led_count, row, col, count;

#ifdef DEBUG
  Serial.print("Time remaining=");
  Serial.println(time_remaining);
#endif

  // how many leds (of 7 rows) represent the time remaining
  led_count=time_remaining/time_per_led;
  
  if (led_count <= 0) { 
    lc.clearDisplay(0); 
    return;
  }
  
  // yes this looks very strange, but the
  // display wouldn't mount in the expected
  // orientation
  
  count=0;
  for (col=7;col>=1;--col) {
    for (row=0;row<8;++row,++count) {
      if (count < led_count) {
        lc.setLed(0, row, col, 1);    
      } else {
        lc.setLed(0, row, col, 0);
      }
    }
  }
}

// check current audio levels and update bargraph accordingly
// also trigger any penalties if noise is above threshold

void do_audio_check() {
  unsigned long now=millis();
  int sound_level=0;
  int sensor_level=0;

  sensor_level=analogRead(SOUND_SENSOR_PIN);
  sound_level=sensor_level-sound_threshold;

  if (sound_level < 0) {
    sound_level=0;
  }

  if (!is_running) { return; }

#ifdef DEBUG_SOUND
  Serial.println(sound_level);
#endif

  if (calibration_mode) {
    if (sound_level > calibration_level) {
      calibration_level=sound_level;
    }
  } else {
    show_bargraph_sound_level(sound_level);
    
    if (sound_level > noise_threshold) {
#ifdef DEBUG
      Serial.print("Sound level=");
      Serial.println(sound_level);
#endif
      do_penalty();
    } else {
      // reset loud noise count value if no loud noise heard within penalty interval time
      // this ensures the bargraph gets updated properly if noise has stopped
      if (loud_noise_count > 0 && (now-last_loud_noise_time) > penalty_interval) {
        loud_noise_count=0;
        last_bargraph_time=0;
        show_bargraph_sound_level(0);
      }
    }
  }
}

// get stored sound threshold from eeprom

void get_settings() {
  EEPROM.get(0, sound_threshold);
#ifdef DEBUG
  Serial.print("Got sound_threshold value of ");
  Serial.print(sound_threshold);
  Serial.println(" from eeprom");
#endif
}

// store sound threshold in eeprom

void save_settings() {
#ifdef DEBUG
  Serial.print("Saving sound_threshold of ");
  Serial.print(sound_threshold);
  Serial.println(" to eeprom"); 
#endif

  EEPROM.put(0, sound_threshold); 
}

void setup() {
  Serial.begin(250000);

#ifdef DEBUG
  Serial.println("Quiet Time v0.1 Starting!");
#endif
  
  // initialize led display
  lc.shutdown(0,false);
  lc.setIntensity(0,6);
  show_face(neutral_face);

  pinMode(BUTTON_PIN, INPUT);
  pinMode(SOUND_SENSOR_PIN, INPUT);
}

void loop() {
  unsigned long now;
  unsigned long elapsed;
  unsigned long debounce_ms;
  int button_state, button_reading;

  now=millis();

  button_reading=digitalRead(BUTTON_PIN);

  if (last_debounce_time > 0) {
    debounce_ms=now - last_debounce_time;
  } else {
    debounce_ms=0;
  }

  if (button_reading != last_button_state) {
    last_debounce_time=now;
#ifdef DEBUG  
    Serial.print("Button=");
    if (button_reading==HIGH) { 
      Serial.println("HIGH"); 
    } else {
      Serial.println("LOW");
    }

    Serial.print("debounce_ms=");
    Serial.println(debounce_ms);
#endif      
  }

  if (debounce_ms > DEBOUNCE_DELAY) {
    button_state=button_reading;  
  }

  if (is_running) {
    if (last_loop_time == 0) {
      last_loop_time=now;
    }
    
    elapsed=now-last_loop_time;
    time_remaining-=elapsed;
  
    if (time_remaining <= 0) {
      last_display_time=0;
      display_mode=DISP_HAPPY_FACE;
      is_running=false;

      if (calibration_mode) {
        sound_threshold=calibration_level+DEF_SOUND_THRESHOLD;
        calibration_mode=false;
#ifdef DEBUG
        Serial.println("Calibration done!");
        Serial.print("calibration_level=");
        Serial.println(calibration_level);
#endif
        save_settings();
      } else {
#ifdef DEBUG
        Serial.println("YAY!! Timeout is over!!");
#endif
      }
    } 
    
    do_audio_check();
    do_display_update();
  } else {
    if (button_state == LOW && last_button_state == HIGH) {
      last_debounce_time=0;
      if (debounce_ms > LONG_PRESS_TIME) {
        calibration_mode=true;  
      }
      
      init_vars();
    }
  } 

  last_button_state = button_state;
  last_loop_time=now;
}
