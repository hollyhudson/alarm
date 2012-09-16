#include <avr/io.h>      // this contains all the IO port definitions
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <string.h>
#include <i2c.h>
#include <stdlib.h>
#include "util.h"
#include "ratt.h"
#include "ks0108.h"
#include "glcd.h"


// NUM_LEDS are num in strip, the 3 is for RGB.  Each LED is 1 pixel of RGB.
static uint8_t pixels[NUM_LEDS][3];

int LED_seq_running = 0;
int intensity = 0;
int times_through = 0;
int begin_seq_m = 0;
int begin_seq_h = 0;
volatile uint8_t time_s, time_m, time_h;
volatile uint8_t old_h, old_m, old_s;
volatile uint8_t timeunknown = 1;
volatile uint8_t date_m, date_d, date_y;
volatile uint8_t alarming, alarm_on, alarm_tripped, alarm_h, alarm_m;
volatile uint8_t displaymode;
volatile uint8_t volume;
volatile uint8_t sleepmode = 0;
volatile uint8_t region;
volatile uint8_t time_format;
extern volatile uint8_t screenmutex;
volatile uint8_t minute_changed = 0, hour_changed = 0, second_changed = 0;
volatile uint8_t score_mode_timeout = 0;
volatile uint8_t score_mode = SCORE_MODE_TIME;
volatile uint8_t last_score_mode;

// These store the current button states for all 3 buttons. We can 
// then query whether the buttons are pressed and released or pressed
// This allows for 'high speed incrementing' when setting the time
extern volatile uint8_t last_buttonstate, just_pressed, pressed;
extern volatile uint8_t buttonholdcounter;

extern volatile uint8_t timeoutcounter;

// How long we have been snoozing
uint16_t snoozetimer = 0;

SIGNAL(TIMER1_OVF_vect) {
  PIEZO_PORT ^= _BV(PIEZO);
}

volatile uint16_t millis = 0;
volatile uint16_t animticker, alarmticker;
SIGNAL(TIMER0_COMPA_vect) {
  if (millis)
    millis--;
  if (animticker)
    animticker--;

  if (alarming && !snoozetimer) {
    if (alarmticker == 0) {
	// length of beeping
      alarmticker = 300;
	// makes the beep
      if (TCCR1B == 0) {
	TCCR1A = 0; 
	TCCR1B =  _BV(WGM12) | _BV(CS10); // CTC with fastest timer
	TIMSK1 = _BV(TOIE1) | _BV(OCIE1A);
	OCR1A = (F_CPU / ALARM_FREQ) / 2;
      } else {
	TCCR1B = 0;
	// turn off piezo
	PIEZO_PORT &= ~_BV(PIEZO);
	// end of code that makes the beep
      }
    }
    alarmticker--;    
  }
}


SIGNAL(TIMER1_COMPA_vect) {
  PIEZO_PORT ^= _BV(PIEZO);
}

static inline void
beep(const uint16_t freq, const uint8_t duration)
{
  // use timer 1 for the piezo/buzzer 
  TCCR1A = 0; 
  TCCR1B =  _BV(WGM12) | _BV(CS10); // CTC with fastest timer
  TIMSK1 = _BV(TOIE1) | _BV(OCIE1A);
  OCR1A = (F_CPU / freq) / 2;
  _delay_ms(duration);
  TCCR1B = 0;
  // turn off piezo
  PIEZO_PORT &= ~_BV(PIEZO);
}


void init_eeprom(void)
{	//Set eeprom to a default state.
#ifndef OPTION_DOW_DATELONG
  if(eeprom_read_byte((uint8_t *)EE_INIT) != EE_INITIALIZED) {
#else
  if(eeprom_read_byte((uint8_t *)EE_INIT) != (EE_INITIALIZED-1)) {
#endif
    eeprom_write_byte((uint8_t *)EE_ALARM_HOUR, 8);
    eeprom_write_byte((uint8_t *)EE_ALARM_MIN, 0);
    eeprom_write_byte((uint8_t *)EE_BRIGHT, OCR2A_VALUE);
    eeprom_write_byte((uint8_t *)EE_VOLUME, 1);
    eeprom_write_byte((uint8_t *)EE_REGION, REGION_US);
    eeprom_write_byte((uint8_t *)EE_TIME_FORMAT, TIME_12H);
    eeprom_write_byte((uint8_t *)EE_SNOOZE, 10);
#ifndef OPTION_DOW_DATELONG
    eeprom_write_byte((uint8_t *)EE_INIT, EE_INITIALIZED);
#else
    eeprom_write_byte((uint8_t *)EE_INIT, EE_INITIALIZED-1);
#endif
  }
}

int main(void)
{
  	uint8_t inverted = 0;
  	uint8_t mcustate;
  	uint8_t display_date = 0;

  	// check if we were reset
  	mcustate = MCUSR;
  	MCUSR = 0;
  
  	//Just in case we were reset inside of the glcd init function
  	//which would happen if the lcd is not plugged in. The end result
  	//of that, is it will beep, pause, for as long as there is no lcd
  	//plugged in.
  	wdt_disable();

  	// setup uart
  	uart_init(BRRL_192);
  	DEBUGP("RATT Clock");
	
  	// set up piezo
  	PIEZO_DDR |= _BV(PIEZO);
	
  	DEBUGP("clock!");
  	clock_init();
  	//beep(4000, 100);

  	init_eeprom();
  	region = eeprom_read_byte((uint8_t *)EE_REGION);
  	time_format = eeprom_read_byte((uint8_t *)EE_TIME_FORMAT);
  	DEBUGP("buttons!");
  	initbuttons();
	
  	setalarmstate();

  	// setup 1ms timer on timer0
  	TCCR0A = _BV(WGM01);
  	TCCR0B = _BV(CS01) | _BV(CS00);
  	OCR0A = 125;
  	TIMSK0 |= _BV(OCIE0A);

  	// turn backlight on
  	DDRD |= _BV(3);
#ifndef BACKLIGHT_ADJUST
  	PORTD |= _BV(3);
#else
  	TCCR2A = _BV(COM2B1); // PWM output on pin D3
  	TCCR2A |= _BV(WGM21) | _BV(WGM20); // fast PWM
  	TCCR2B |= _BV(WGM22);
  	OCR2A = OCR2A_VALUE;
  	OCR2B = eeprom_read_byte((uint8_t *)EE_BRIGHT);
#endif

  	DDRB |= _BV(5);
  	beep(4000, 100);
  
	//glcdInit locks and disables interrupts in one of its
	//functions.  If the LCD is not plugged in, glcd will run
	//forever.  For good reason, it would be desirable to know
	//that the LCD is plugged in and working correctly as a result.
	//This is why we are using a watch dog timer.  The lcd should
	//initialized in way less than 500 ms.

  	wdt_enable(WDTO_2S);
  	glcdInit();
  	glcdClearScreen();
	
  	initanim();
  	initdisplay(0);

	_delay_ms(1000);
	LEDsetup();

	//added by Holly for debugging purposes
      	beep(4000, 100);

	/********** LOOP LOOP LOOP *******************/
  	while (1) {

	/**********  Holly's LED strip code (begin) *************/
	// LED_sep_running = 1 during ramp_up process, we must keep
	//returning to the function each loop to test whether it is
	//time to increase intensity.

		if(LED_seq_running) ramp_up();

	// if it's time for the alarm to go off, and the sequence
	// isn't running, go into the ramp_up function
		if((time_h == begin_seq_h) && (time_m == begin_seq_m) && (time_h != 0))
		{
			if(!LED_seq_running){
				LED_seq_running = 1;
				times_through = 0;
				ramp_up();	
			}
		}
		/**********  Holly's LED strip code (end) *************/

/// begin Trammell's debug code for dead pixels
#if 0
		static int led_counter;
		static int value = 0;
		pixels[led_counter][0] = value == 0 ? 0xFF : 0;
		pixels[led_counter][1] = value == 1 ? 0xFF : 0;
		pixels[led_counter][2] = value == 2 ? 0xFF : 0;
		if (led_counter < NUM_LEDS)
			led_counter++;
		else {
			led_counter = 0;
			if (value == 2)
				value = 0;
			else
				value++;
		}
		send_pixels();
#endif
/// end Trammell's code

    		animticker = ANIMTICK_MS;

  		glcdSetAddress(0, 7);
  		//glcdPutStr("Holly clock", NORMAL);
		printnumber(alarm_h, NORMAL);
      		glcdWriteChar(':', NORMAL);
		printnumber(alarm_m, NORMAL);
  		//glcdPutStr(alarm_h,alarm_m, NORMAL);

    		// check buttons to see if we have interaction stuff to deal with
		if(just_pressed && alarming) {
	  		just_pressed = 0;
	  		setsnooze();
		}

		if(display_date==3 && !score_mode_timeout) {
			display_date=0;
			score_mode = SCORE_MODE_YEAR;
	    		score_mode_timeout = 3;
	    		//drawdisplay();
		}
#ifdef OPTION_DOW_DATELONG
		else if(display_date==2 && !score_mode_timeout) {
			display_date=3;
			score_mode = SCORE_MODE_DATE;
	    	score_mode_timeout = 3;
	    	//drawdisplay();
		}
		else if(display_date==1 && !score_mode_timeout) {
			display_date=4;
			score_mode = SCORE_MODE_DATELONG_MON;
	    		score_mode_timeout = 3;
	    		//drawdisplay();
		}
		else if(display_date==4 && !score_mode_timeout) {
			display_date=3;
			score_mode = SCORE_MODE_DATELONG_DAY;
			score_mode_timeout = 3;
		}
#endif
	
		/*if(display_date && !score_mode_timeout) {
	  		if(last_score_mode == SCORE_MODE_DATELONG) {
	    			score_mode = SCORE_MODE_DOW;
	    			score_mode_timeout = 3;
	    			setscore();
	  		} else if(last_score_mode == SCORE_MODE_DOW) {
	
	    			score_mode = SCORE_MODE_DATE;
	    			score_mode_timeout = 3;
	    			setscore();
	  		}
	  		else if(last_score_mode == SCORE_MODE_DATE) {
	    			score_mode = SCORE_MODE_YEAR;
	    			score_mode_timeout = 3;
	    			setscore();
	    			display_date = 0;
	  		}
	  
		}*/
		/*if(display_date && !score_mode_timeout) {
	  		score_mode = SCORE_MODE_YEAR;
	  		score_mode_timeout = 3;
	  		setscore();
	  		display_date = 0;
		}*/

		//Was formally set for just the + button.  However,
		//because the Set button was never accounted for, If
		//the alarm was turned on, and ONLY the set button
		//was pushed since then, the alarm would not sound
		//at alarm time, but go into a snooze immediately
		//after going off.  This could potentially make you
		//late for work, and had to be fixed.

		if (just_pressed & 0x6) {
	  		just_pressed = 0;
#ifdef OPTION_DOW_DATELONG
	  		if((region == REGION_US) || (region == REGION_EU)) {
#endif
	  			display_date = 3;
	  			score_mode = SCORE_MODE_DATE;
#ifdef OPTION_DOW_DATELONG
	  		} else if ((region == DOW_REGION_US) || (region == DOW_REGION_EU)) {
	  			display_date = 2;
	  			score_mode = SCORE_MODE_DOW;
	  		} else if (region == DATELONG) {
	  			display_date = 4;
	  			score_mode = SCORE_MODE_DATELONG_MON;
	  		} else {
	  			display_date = 1;
	  			score_mode = SCORE_MODE_DOW;
	  		}
#endif
	  		score_mode_timeout = 3;
	  		//drawdisplay();
		}

    		if (just_pressed & 0x1) {
      			just_pressed = 0;
      			display_date = 0;
      			score_mode = SCORE_MODE_TIME;
      			score_mode_timeout = 0;
      			//drawdisplay();
      			switch(displaymode) {
      				case (SHOW_TIME):
					displaymode = TOGGLE_DEMO_MODE;
					toggle_demo_mode();
					break;
				case TOGGLE_DEMO_MODE:
					displaymode = SET_ALARM;
					set_alarm();
					break;
      				case SET_ALARM:
					displaymode = SET_TIME;
					set_time();
					timeunknown = 0;
					break;
      				case SET_TIME:
					displaymode = TOGGLE_BRIGHTS;
					//set_date();
					toggle_brights();
					break;
      				case TOGGLE_BRIGHTS:
					displaymode = TOGGLE_REDS;
					//set_region();
					toggle_reds();
					break;
//#ifdef BACKLIGHT_ADJUST
	  			case TOGGLE_REDS:
					displaymode = TOGGLE_LAMP;
					toggle_lamp();
					break;
//#endif
      				default:
					displaymode = SHOW_TIME;
					glcdClearScreen();
					initdisplay(0);
      			}

     			if (displaymode == SHOW_TIME) {
				glcdClearScreen();
				initdisplay(0);
      			}
    		}

    		step();
    		if (displaymode == SHOW_TIME) {
      			if (! inverted && alarming && (time_s & 0x1)) {
				inverted = 1;
				initdisplay(inverted);
      			} else if ((inverted && ! alarming) || (alarming && inverted && !(time_s & 0x1))) { 
				inverted = 0;
				initdisplay(0);
      			} else {
				PORTB |= _BV(5);
				drawdisplay(inverted);
				PORTB &= ~_BV(5);
      			}
    		}
  
    		while (animticker);
    		//uart_getchar();  // you would uncomment this so you can manually 'step'
  	}

  	halt();
}


// This turns on/off the alarm when the switch has been
// set. It also displays the alarm time
void setalarmstate(void)
{
  	DEBUGP("a");
  	if (ALARM_PIN & _BV(ALARM)) { 
    		if (alarm_on) {
      			// turn off the alarm
      			alarm_on = 0;
      			alarm_tripped = 0;
      			snoozetimer = 0;
      			if (alarming) {
				// if the alarm is going off, we
				// should turn it off  and quiet the
				// speaker
				DEBUGP("alarm off");
				alarming = 0;
				TCCR1B = 0;
				// turn off piezo
				PIEZO_PORT &= ~_BV(PIEZO);
      			} 
    		}
  	} else {
    	// Don't display the alarm/beep if we already have
    		if  (!alarm_on) {
      			// alarm on!
      			alarm_on = 1;
      			// reset snoozing
      			snoozetimer = 0;
	  		score_mode = SCORE_MODE_ALARM;
	  		score_mode_timeout = 3;
	  		//drawdisplay();
      			DEBUGP("alarm on");
    		}   
  	}
}


void drawArrow(uint8_t x, uint8_t y, uint8_t l)
{
  glcdFillRectangle(x, y, l, 1, ON);
  glcdSetDot(x+l-2,y-1);
  glcdSetDot(x+l-2,y+1);
  glcdSetDot(x+l-3,y-2);
  glcdSetDot(x+l-3,y+2);
}


void printnumber(uint8_t n, uint8_t inverted)
{
  glcdWriteChar(n/10+'0', inverted);
  glcdWriteChar(n%10+'0', inverted);
}

uint8_t readi2ctime(void)
{
  	uint8_t regaddr = 0, r;
  	uint8_t clockdata[8];
  
  	// check the time from the RTC
  	cli();
  	r = i2cMasterSendNI(0xD0, 1, &regaddr);

  	if (r != 0) {
    		DEBUG(putstring("Reading i2c data: ")); DEBUG(uart_putw_dec(r)); DEBUG(putstring_nl(""));
    		while(1) {
      			sei();
      			beep(4000, 100);
      			_delay_ms(100);
      			beep(4000, 100);
      			_delay_ms(1000);
    		}
  	}

  	r = i2cMasterReceiveNI(0xD0, 7, &clockdata[0]);
  	sei();

  	if (r != 0) {
    		DEBUG(putstring("Reading i2c data: ")); DEBUG(uart_putw_dec(r)); DEBUG(putstring_nl(""));
    		while(1) {
      			beep(4000, 100);
      			_delay_ms(100);
      			beep(4000, 100);
      			_delay_ms(1000);
    		}
  	}

  	time_s = ((clockdata[0] >> 4) & 0x7)*10 + (clockdata[0] & 0xF);
  	time_m = ((clockdata[1] >> 4) & 0x7)*10 + (clockdata[1] & 0xF);
  	if (clockdata[2] & _BV(6)) {
    		// "12 hr" mode
    		time_h = ((clockdata[2] >> 5) & 0x1)*12 + 
      			((clockdata[2] >> 4) & 0x1)*10 + (clockdata[2] & 0xF);
  	} else {
    		time_h = ((clockdata[2] >> 4) & 0x3)*10 + (clockdata[2] & 0xF);
  	}
  
  	date_d = ((clockdata[4] >> 4) & 0x3)*10 + (clockdata[4] & 0xF);
  	date_m = ((clockdata[5] >> 4) & 0x1)*10 + (clockdata[5] & 0xF);
  	date_y = ((clockdata[6] >> 4) & 0xF)*10 + (clockdata[6] & 0xF);

  	return clockdata[0] & 0x80;
}

void writei2ctime(uint8_t sec, uint8_t min, uint8_t hr, uint8_t day,
		  uint8_t date, uint8_t mon, uint8_t yr)
{
  	uint8_t clockdata[8] = {0,0,0,0,0,0,0,0};

  	clockdata[0] = 0; // address
  	clockdata[1] = i2bcd(sec);  // s
  	clockdata[2] = i2bcd(min);  // m
  	clockdata[3] = i2bcd(hr); // h
  	clockdata[4] = i2bcd(day);  // day
  	clockdata[5] = i2bcd(date);  // date
  	clockdata[6] = i2bcd(mon);  // month
  	clockdata[7] = i2bcd(yr); // year
  
  	cli();
  	uint8_t r = i2cMasterSendNI(0xD0, 8, &clockdata[0]);
  	sei();

  //DEBUG(putstring("Writing i2c data: ")); DEBUG(uart_putw_dec()); DEBUG(putstring_nl(""));

  	if (r != 0) {
    		while(1) {
      			beep(4000, 100);
      			_delay_ms(100);
      			beep(4000, 100);
      			_delay_ms(1000);
    		}
  	}
}

// runs at about 30 hz
uint8_t t2divider1 = 0, t2divider2 = 0;
SIGNAL (TIMER2_OVF_vect)
{
	wdt_reset();
#ifdef BACKLIGHT_ADJUST
  	if (t2divider1 == TIMER2_RETURN) {
#else
  		if (t2divider1 == 5) {
#endif
    			t2divider1 = 0;
  		} else {
    			t2divider1++;
    			return;
  		}

  		//This occurs at 6 Hz

  		uint8_t last_s = time_s;
  		uint8_t last_m = time_m;
  		uint8_t last_h = time_h;

  		readi2ctime();
  
  		if (time_h != last_h) {
    			hour_changed = 1; 
    			minute_changed = 1;
    			second_changed = 1;
    			old_h = last_h;
    			old_m = last_m;
  		} else if (time_m != last_m) {
    			hour_changed = 0; 
    			minute_changed = 1;
    			second_changed = 1;
    			old_m = last_m;
  		} else if (time_s != last_s) {
    			hour_changed = 0; 
    			minute_changed = 0;
    			second_changed = 1;
    			old_s = last_s;
  		}


  		if (time_s != last_s) {
    			if(alarming && snoozetimer)
	  			snoozetimer--;

    			if(score_mode_timeout) {
	  			score_mode_timeout--;
	  			if(!score_mode_timeout) {
	  				last_score_mode = score_mode;
	    				score_mode = SCORE_MODE_TIME;
	    				if(hour_changed) {
	      					time_h = old_h;
	      					time_m = old_m;
	    				} else if (minute_changed) {
	      					time_m = old_m;
	    				}
	    				if(hour_changed || minute_changed) {
	      					time_h = last_h;
	      					time_m = last_m;
	    				}
	  			}
			}


    			DEBUG(putstring("**** "));
    			DEBUG(uart_putw_dec(time_h));
    			DEBUG(uart_putchar(':'));
    			DEBUG(uart_putw_dec(time_m));
    			DEBUG(uart_putchar(':'));
    			DEBUG(uart_putw_dec(time_s));
    			DEBUG(putstring_nl("****"));
  		}

  		if (((displaymode == SET_ALARM) ||
       			(displaymode == TOGGLE_BRIGHTS) ||
       			(displaymode == TOGGLE_REDS) ||
       			(displaymode == TOGGLE_LAMP)) &&
      			(!screenmutex) ) {
      			glcdSetAddress(MENU_INDENT + 10*6, 2);
      			print_timehour(time_h, NORMAL);
      			glcdWriteChar(':', NORMAL);
      			printnumber(time_m, NORMAL);
      			glcdWriteChar(':', NORMAL);
      			printnumber(time_s, NORMAL);

      			if (time_format == TIME_12H) {
				glcdWriteChar(' ', NORMAL);
				if (time_h >= 12) {
	  				glcdWriteChar('P', NORMAL);
				} else {
	  				glcdWriteChar('A', NORMAL);
				}
      			}
  		}

  		// check if we have an alarm set
  		if (alarm_on && (time_s == 0) && (time_m == alarm_m) && (time_h == alarm_h)) {
    			DEBUG(putstring_nl("ALARM TRIPPED!!!"));
    			alarm_tripped = 1;
  		}

  		//if (minute_changed)
  		//	beep(1000, 100);
  
  		//And wait till the score changes to actually set the alarm off.
  		if(!minute_changed && !hour_changed && alarm_tripped) {
  	 		DEBUG(putstring_nl("ALARM GOING!!!!"));
  	 		alarming = 1;
			// DEBUG DEBUG DEBUG -- can't print to screen in this function
  			//glcdSetAddress(0, 7);
  			//glcdPutStr("Is alarm going off?", NORMAL);
			// DEBUG DEBUG DEBUG
  	 		alarm_tripped = 0;
  		}

  		if (t2divider2 == 6) {
    			t2divider2 = 0;
  		} else {
    			t2divider2++;
    			return;
  		}

  		if (buttonholdcounter) {
    			buttonholdcounter--;
  		}

  		if (timeoutcounter) {
    			timeoutcounter--;
  		}
}

uint8_t leapyear(uint16_t y)
{
  return ( (!(y % 4) && (y % 100)) || !(y % 400));
}

void tick(void)
{

}

inline uint8_t i2bcd(uint8_t x)
{
  return ((x/10)<<4) | (x%10);
}

void clock_init(void)
{
	// talk to clock
  	i2cInit();


  	if (readi2ctime()) {
    		DEBUGP("uh oh, RTC was off, lets reset it!");
    		writei2ctime(0, 0, 12, 0, 1, 1, 9); // noon 1/1/2009
   	}

  	readi2ctime();

  	DEBUG(putstring("\n\rread "));
  	DEBUG(uart_putw_dec(time_h));
  	DEBUG(uart_putchar(':'));
  	DEBUG(uart_putw_dec(time_m));
  	DEBUG(uart_putchar(':'));
  	DEBUG(uart_putw_dec(time_s));

  	DEBUG(uart_putchar('\t'));
  	DEBUG(uart_putw_dec(date_d));
  	DEBUG(uart_putchar('/'));
  	DEBUG(uart_putw_dec(date_m));
  	DEBUG(uart_putchar('/'));
  	DEBUG(uart_putw_dec(date_y));
  	DEBUG(putstring_nl(""));

  	alarm_m = eeprom_read_byte((uint8_t *)EE_ALARM_MIN) % 60;
  	alarm_h = eeprom_read_byte((uint8_t *)EE_ALARM_HOUR) % 24;
	// count 2 min before alarm should go off to begin sequence.
	// this will not work near midnight/1 oclock!!!
	
	set_begin_seq(alarm_m, alarm_h);

  	//ASSR |= _BV(AS2); // use crystal

  	TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20); // div by 1024
  	// overflow ~30Hz = 8MHz/(255 * 1024)

  	// enable interrupt
  	TIMSK2 = _BV(TOIE2);

  	sei();
}

void setsnooze(void) 
{
  //snoozetimer = eeprom_read_byte((uint8_t *)EE_SNOOZE);
  //snoozetimer *= 60; // convert minutes to seconds
  snoozetimer = MAXSNOOZE;
  TCCR1B = 0;
  // turn off piezo
  PIEZO_PORT &= ~_BV(PIEZO);
  DEBUGP("snooze");
  //displaymode = SHOW_SNOOZE;
  //_delay_ms(1000);
  displaymode = SHOW_TIME;
}

/**************************************************/
// Support for Holly's light alarm project.
/**************************************************/

// These values will ramp up the LEDs at the correct rate,
// assuming intensity levels range from 0 to 255.
static const int lookup[] = { 1, 1, 2, 3, 4, 5, 6, 8, 9, 10, 12,
        14, 16, 18 , 20, 22, 25, 28, 30, 33, 36, 39, 42, 46, 49,
        52, 56, 60, 64, 68, 72, 76, 81, 86,90, 95, 100, 105, 110,
        116, 121, 126, 132, 138, 144, 150, 156, 162,169, 176, 182,
        189, 196, 203, 210, 218, 225, 232, 240, 248, 255};
// check to see if the LEDs are busy running a sequence 

// Turn the lamp on
void turn_lamp_on(void)
{
	DDRC |= 2;
	PORTC |= 2;
	lamp_state = 1;
}

// Turn the lamp off
void turn_lamp_off(void)
{
	PORTC &= ~(1 << 1);
	lamp_state = 0;
}

// All lights at brightest possible setting.
void brightest_lights(void)
{
        for(int j = 0 ; j < NUM_LEDS ; j++)
        {
                pixels[j][0] = 255;
                pixels[j][1] = 255;
                pixels[j][2] = 255;
        }
        send_pixels();
        _delay_ms(1000);
}

// Slowly raise the lights from dark to bright, with bluish cast.
static void ramp_up(void)
{
	// mod 4 is 2 min, mod 64 should be 32 min (it's actually 20)
	// 80 is too short, 128 is too long
        // if(times_through++ % 32 != 0)
        if(times_through++ % 100 != 0)
	{
		/***** DEBUGGING *******/
		//glcdSetAddress(0, 1);
                //printnumber(times_through, NORMAL);
		/***** DEBUGGING *******/
		return;
	}

	//if(intensity == 60){
	if(intensity == 90){
		// end the sequences with all lights bright
		brightest_lights();
		turn_lamp_on();
		// stop entering fx from main loop when
		// we're done ramping up
		LED_seq_running = 0;
		// reset intensity to be ready for tomorrow's ramp_up
		intensity = 0;
		/************ DEBUG ***********/
		pixels[0][0] = 0;
		pixels[0][1] = 255;
		pixels[0][2] = 0;
		send_pixels();
		/*********** DEBUG ************/
		// we're already at brightest level, so get out of here 
		return;
	}

	// increase brightness to previously set intensity
	// set all the pixels on the strip to something
#if 1
	for(int j = 0 ; j < NUM_LEDS ; j++)
	{
		if(intensity < 30)
		{
			pixels[j][0] = 0;
			pixels[j][1] = 0;
			pixels[j][2] = lookup[intensity];
			/***** DEBUGGING *******/
			//glcdSetAddress(0, 2);
                        //printnumber(intensity, NORMAL);
			/***** DEBUGGING *******/
		}else if(intensity < 61){
			pixels[j][0] = lookup[intensity - 30];
			pixels[j][1] = lookup[intensity - 30];
			pixels[j][2] = lookup[intensity];
			//pixels[j][2] = 255;
			//glcdSetAddress(0, 1);
                        //printnumber(intensity, NORMAL);
		}else if(intensity < 90 ){
			pixels[j][0] = lookup[intensity - 30];
			pixels[j][1] = lookup[intensity - 30];
			pixels[j][2] = 255;
		}
	}
	//_delay_ms(200);
	send_pixels();
	intensity++;
#endif
#if 0
	for(int j = 0 ; j < NUM_LEDS ; j++)
	{
		if(intensity < 60)
		{
			if(lookup[intensity] == intensity){
				pixels[j][0] = lookup[intensity];
				pixels[j][1] = lookup[intensity];
			}
			pixels[j][2] = intensity;
			glcdSetAddress(0, 1);
                        printnumber(intensity, NORMAL);
		}
	}
#endif
}

// Slowly turn the lights off, with bluish cast.
static void ramp_down(void)
{
        for( int intensity = 30 ; intensity > -1 ; intensity = intensity - 1 )
        {
                for(int j = 0 ; j < NUM_LEDS ; j++)
                {
                        if(lookup[intensity] < 150)
                        {
                                pixels[j][0] = 0;
                                pixels[j][1] = 0;
                                pixels[j][2] = lookup[intensity];
                        }else{
                                pixels[j][0] = lookup[intensity] - 150;
                                pixels[j][1] = lookup[intensity] - 150;
                                pixels[j][2] = lookup[intensity];
                        }
                }
                send_pixels();
                _delay_ms(50);
        }
}

// Lights dimly red for night time.
void nightlight(void)
{
        for(int j = 0 ; j < NUM_LEDS ; j++)
        {
                pixels[j][0] = 70;
                pixels[j][1] = 5;
                pixels[j][2] = 0;
        }
        send_pixels();
        _delay_ms(1000);
}

// Shuts all the aEDs off.
void all_off(void)
{
        for(int j = 0 ; j < NUM_LEDS ; j++)
        {
                pixels[j][0] = 0;
                pixels[j][1] = 0;
                pixels[j][2] = 0;
        }
        send_pixels();
}

// Quickly raise the lights from dark to bright, with bluish cast, for demo.
void ramp_fast(void)
{
	//if(intensity == 60){
	// increase brightness to previously set intensity
	// set all the pixels on the strip to something
#if 1
	while(intensity <= 89){

		for(int j = 0 ; j < NUM_LEDS ; j++)
		{
			if(intensity < 30)
			{
				pixels[j][0] = 0;
				pixels[j][1] = 0;
				pixels[j][2] = lookup[intensity];
				/***** DEBUGGING *******/
				//glcdSetAddress(0, 2);
                        	//printnumber(intensity, NORMAL);
				/***** DEBUGGING *******/
			}else if(intensity < 61){
				pixels[j][0] = lookup[intensity - 30];
				pixels[j][1] = lookup[intensity - 30];
				pixels[j][2] = lookup[intensity];
				//pixels[j][2] = 255;
				//glcdSetAddress(0, 1);
                        	//printnumber(intensity, NORMAL);
			}else if(intensity < 90 ){
				pixels[j][0] = lookup[intensity - 30];
				pixels[j][1] = lookup[intensity - 30];
				pixels[j][2] = 255;
			}
		}
		_delay_ms(100);
		send_pixels();
		intensity++;
	}

	if(intensity == 90){
		// end the sequences with all lights bright
		brightest_lights();
		turn_lamp_on();
		// reset intensity to be ready for tomorrow's ramp_up
		intensity = 0;
		/************ DEBUG ***********/
		pixels[0][0] = 0;
		pixels[0][1] = 255;
		pixels[0][2] = 0;
		send_pixels();
		/*********** DEBUG ************/
		// we're already at brightest level, so get out of here 
		return;
	}

#endif
#if 0
	for(int j = 0 ; j < NUM_LEDS ; j++)
	{
		if(intensity < 60)
		{
			if(lookup[intensity] == intensity){
				pixels[j][0] = lookup[intensity];
				pixels[j][1] = lookup[intensity];
			}
			pixels[j][2] = intensity;
			glcdSetAddress(0, 1);
                        printnumber(intensity, NORMAL);
		}
	}
#endif
}

// copy array to physical strip to light up LEDs
static void send_pixels(void)
{
        uint16_t i;

        for (i = 0 ; i < NUM_LEDS ; i++)
        {
                uint8_t * const p = pixels[i];
                uint8_t r = p[0];
                uint8_t g = p[1];
                uint8_t b = p[2];

                send_byte(0x80 | (g >> 1));
                send_byte(0x80 | (r >> 1));
                send_byte(0x80 | (b >> 1));
        }

        latch();
}

static void send_byte(uint8_t v)
{
        unsigned bit;
        for (bit = 0x80 ; bit ; bit >>= 1)
        {
                clock(0);
                if (v & bit)
                        PORTD |=  (1 << 1);
                else
                        PORTD &= ~(1 << 1);
                clock(1);
        }
}

static inline void clock(const uint8_t value)
{
        if (value)
                PORTD |= (1 << 0);
        else
                PORTD &= ~(1 << 0);
}

static void send_zero(void)
{
        unsigned i;
        PORTD &= ~(1 << 1);

        for (i = 0 ; i < 8 ; i++)
        {
                clock(1);
                clock(0);
        }
}

static void latch(void)
{
        unsigned i;
        const unsigned zeros = 3 * ((NUM_LEDS + 63) / 64);
        for (i = 0 ; i < zeros ; i++)
                send_zero();
}

static void LEDsetup(void)
{
	// initialize the digital pin as an output.
	cbi(UCSR0B, TXEN0);
	cbi(UCSR0B, RXEN0);
        DDRD |= 1 << 1;
        DDRD |= 1 << 0;

        latch();
	// initialization sequence (they all blink really bright)
        //uint16_t i;
        //for (i = 0 ; i < NUM_LEDS ; i++)
	//{
             //pixels[i][0] = 0xFF;
             //pixels[i][1] = 0x00;
             //pixels[i][2] = 0x00;
	//}
        //send_pixels();
	all_off();
}

void set_begin_seq(int min, int hour)
{
	// here is where we set the sequence length
	// from where the lights start to come up, until full brightness
	// This code works for a 35 minute long sequence
	
	int diff = 0;  // holds offset for minutes if needed
	
	if(min < 35)
	{
		// we need to wrap hours back
		begin_seq_h = hour - 1;
		// we need to determine new minutes
		diff = 35 - min;
		begin_seq_m = 60 - diff;
	}else{
		// no need to go back an hour
		begin_seq_h = hour;
		begin_seq_m = min - 35;
	}
}
