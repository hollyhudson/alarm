#include <avr/io.h>      // this contains all the IO port definitions
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <string.h>
#include "util.h"
#include "ratt.h"
#include "ks0108.h"
#include "glcd.h"

extern volatile uint8_t time_s, time_m, time_h;
extern volatile uint8_t date_m, date_d, date_y;
extern volatile uint8_t alarm_h, alarm_m;
extern volatile uint8_t last_buttonstate, just_pressed, pressed;
extern volatile uint8_t buttonholdcounter;
extern volatile uint8_t time_format;

extern volatile uint8_t displaymode;
// This variable keeps track of whether we have not pressed any
// buttons in a few seconds, and turns off the menu display
volatile uint8_t timeoutcounter = 0;

volatile uint8_t screenmutex = 0;

int brights_state = 0;
int reds_state = 0;
int lamp_state = 0;
int demo_state = 0;

// True when the bright lights are on, false when they're turned off
void print_brights_state(int brights_state)
{
  	glcdSetAddress(MENU_INDENT, 3);
  	glcdPutStr("Brights:", NORMAL);
	if (brights_state == 1) {
  		glcdPutStr("       ",NORMAL);
  		glcdPutStr("ON",NORMAL);
	} else {
  		glcdPutStr("       ",NORMAL);
  		glcdPutStr("OFF",NORMAL);
	}
}

// True when the red night lights are on, false when they're off
void print_reds_state(int reds_state)
{
  	glcdSetAddress(MENU_INDENT, 4);
  	glcdPutStr("Reds:", NORMAL);
	if (reds_state == 1) {
  		glcdPutStr("          ",NORMAL);
  		glcdPutStr("ON",NORMAL);
	} else {
  		glcdPutStr("          ",NORMAL);
  		glcdPutStr("OFF",NORMAL);
	}
}

// True when the lamp is on, false when it's off
void print_lamp_state(int lamp_state)
{
  	glcdSetAddress(MENU_INDENT, 5);
  	glcdPutStr("Lamp:", NORMAL);
	if (lamp_state == 1) {
  		glcdPutStr("          ",NORMAL);
  		glcdPutStr("ON",NORMAL);
	} else {
  		glcdPutStr("          ",NORMAL);
  		glcdPutStr("OFF",NORMAL);
	}
}

// True when demo mode is on, false when it's off
void print_demo_state(int demo_state)
{
  	glcdSetAddress(MENU_INDENT, 0);
  	glcdPutStr("Demo Mode:", NORMAL);
	if (brights_state == 1) {
  		glcdPutStr("     ",NORMAL);
  		glcdPutStr("ON",NORMAL);
	} else {
  		glcdPutStr("     ",NORMAL);
  		glcdPutStr("OFF",NORMAL);
	}
}

void display_menu(void)
{
  	DEBUGP("display menu");
  
  	screenmutex++;

  	glcdClearScreen();
  
  	glcdSetAddress(MENU_INDENT, 0);
	print_demo_state(demo_state);
  
  	glcdSetAddress(MENU_INDENT, 1);
  	glcdPutStr("Set Alarm:  ", NORMAL);
  	print_alarmhour(alarm_h, NORMAL);
  	glcdWriteChar(':', NORMAL);
  	printnumber(alarm_m, NORMAL);
  
  	glcdSetAddress(MENU_INDENT, 2);
  	glcdPutStr("Set Time: ", NORMAL);
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
  
  	glcdSetAddress(MENU_INDENT, 3);
	print_brights_state(brights_state);

  	glcdSetAddress(MENU_INDENT, 4);
	print_reds_state(reds_state);

/***********
#ifdef BACKLIGHT_ADJUST
  	glcdSetAddress(MENU_INDENT, 5);
  	glcdPutStr("Set Backlight: ", NORMAL);
  	printnumber(OCR2B>>OCR2B_BITSHIFT,NORMAL);
#endif
***********/
  
  	glcdSetAddress(MENU_INDENT, 5);
  	print_lamp_state(lamp_state);

  	glcdSetAddress(0, 6);
  	glcdPutStr("Press MENU to advance", NORMAL);
  	glcdSetAddress(0, 7);
  	glcdPutStr("Press SET to set", NORMAL);

  	screenmutex--;
}

void toggle_demo_mode(void)
{
	uint8_t mode = TOGGLE_DEMO_MODE;

  	display_menu();

  	screenmutex++;
  	// put a small arrow next to 'set date'
  	drawArrow(0, 3, MENU_INDENT -1);
  	screenmutex--;
  	
  	timeoutcounter = INACTIVITYTIMEOUT;  

  	while (1) {
    		if (just_pressed & 0x1) { // mode change
      			return;
    		}
    		if (just_pressed || pressed) {
      			timeoutcounter = INACTIVITYTIMEOUT;  
      			// timeout w/no buttons pressed after 3 seconds?
    		} else if (!timeoutcounter) {
      			//timed out!
      			displaymode = SHOW_TIME;     
      			return;
    		}
    		if (just_pressed & 0x2) {
      			just_pressed = 0;
      			screenmutex++;

			if (mode == TOGGLE_DEMO_MODE) {
				// now it has been selected
				mode = SET_TOG_DEMO;
				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press + to toggle    ", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to save   ", NORMAL);
			}else{
				mode = TOGGLE_DEMO_MODE;

				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press MENU to advance.", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to toggle.", NORMAL);
			}
      		}
      		screenmutex--;
		if ((just_pressed & 0x4) || (pressed & 0x4)) {
			just_pressed = 0;

			if (mode == SET_TOG_DEMO) {
				// toggle demo_state
				if(demo_state){
					demo_state = 0;				
                                	glcdSetAddress(MENU_INDENT + 15*6, 0);
					glcdPutStr("OFF", INVERTED);
				}else{
					demo_state = 1;
                                	glcdSetAddress(MENU_INDENT + 15*6, 0);
					glcdPutStr("ON", INVERTED);
				}

				// use new value
				if(demo_state){
					ramp_fast();
				}else{
					all_off();
				}
				screenmutex++;
				display_menu();
				
				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press + to toggle    ", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to save   ", NORMAL);

                               	// put a small arrow next to 'set 12h/24h'
				// (I don't think this does anything)
                                drawArrow(0, 3, MENU_INDENT -1);
                                glcdSetAddress(MENU_INDENT + 15*6, 0);
				if(demo_state){
					glcdPutStr("ON", INVERTED);
				}else{
					glcdPutStr("OFF", INVERTED);
				}	
			}
		}
      		screenmutex--;
      		if (pressed & 0x4) _delay_ms(200);  
   	}
}

void toggle_brights(void)
{
	uint8_t mode = TOGGLE_BRIGHTS;

  	display_menu();

  	screenmutex++;
  	// put a small arrow next to 'set date'
  	drawArrow(0, 27, MENU_INDENT -1);
  	screenmutex--;
  	
  	timeoutcounter = INACTIVITYTIMEOUT;  

  	while (1) {
    		if (just_pressed & 0x1) { // mode change
      			return;
    		}
    		if (just_pressed || pressed) {
      			timeoutcounter = INACTIVITYTIMEOUT;  
      			// timeout w/no buttons pressed after 3 seconds?
    		} else if (!timeoutcounter) {
      			//timed out!
      			displaymode = SHOW_TIME;     
      			return;
    		}
    		if (just_pressed & 0x2) {
      			just_pressed = 0;
      			screenmutex++;

			if (mode == TOGGLE_BRIGHTS) {
				// now it has been selected
				mode = SET_TOG_BTS;
				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press + to toggle    ", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to save   ", NORMAL);
			}else{
				mode = TOGGLE_BRIGHTS;

				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press MENU to advance.", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to toggle.", NORMAL);
			}
      		}
      		screenmutex--;
		if ((just_pressed & 0x4) || (pressed & 0x4)) {
			just_pressed = 0;

			if (mode == SET_TOG_BTS) {
				// toggle brights_state
				if(brights_state){
					brights_state = 0;				
                                	glcdSetAddress(MENU_INDENT + 15*6, 3);
					glcdPutStr("OFF", INVERTED);
				}else{
					brights_state = 1;
                                	glcdSetAddress(MENU_INDENT + 15*6, 3);
					glcdPutStr("ON", INVERTED);
				}

				// use new value
				if(brights_state){
					brightest_lights();
				}else{
					all_off();
				}
				screenmutex++;
				display_menu();
				
				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press + to toggle    ", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to save   ", NORMAL);

                               	// put a small arrow next to 'set 12h/24h'
				// (I don't think this does anything)
                                drawArrow(0, 27, MENU_INDENT -1);
                                glcdSetAddress(MENU_INDENT + 15*6, 3);
				if(brights_state){
					glcdPutStr("ON", INVERTED);
				}else{
					glcdPutStr("OFF", INVERTED);
				}	
			}
		}
      		screenmutex--;
      		if (pressed & 0x4) _delay_ms(200);  
   	}
}

void toggle_reds(void)
{
#if 1 
	uint8_t mode = TOGGLE_REDS;

  	display_menu();

  	screenmutex++;
  	// put a small arrow next to 'set date'
  	drawArrow(0, 35, MENU_INDENT -1);
  	screenmutex--;
  	
  	timeoutcounter = INACTIVITYTIMEOUT;  

  	while (1) {
    		if (just_pressed & 0x1) { // mode change
      			return;
    		}
    		if (just_pressed || pressed) {
      			timeoutcounter = INACTIVITYTIMEOUT;  
      			// timeout w/no buttons pressed after 3 seconds?
    		} else if (!timeoutcounter) {
      			//timed out!
      			displaymode = SHOW_TIME;     
      			return;
    		}
    		if (just_pressed & 0x2) {
      			just_pressed = 0;
      			screenmutex++;

			if (mode == TOGGLE_REDS) {
				// now it has been selected
				mode = SET_TOG_RDS;
				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press + to toggle    ", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to save   ", NORMAL);
			}else{
				mode = TOGGLE_REDS;

				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press MENU to advance", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to toggle", NORMAL);

                               	// put a small arrow next to 'set 12h/24h'
				// (I don't think this does anything)
                                drawArrow(0, 35, MENU_INDENT -1);
				if(reds_state){
					reds_state = 0;				
                                	glcdSetAddress(MENU_INDENT + 15*6, 4);
					glcdPutStr("OFF", INVERTED);
				}else{
					reds_state = 1;
                                	glcdSetAddress(MENU_INDENT + 15*6, 4);
					glcdPutStr("ON", INVERTED);
				}

			}
      		}
		screenmutex--;
                if ((just_pressed & 0x4) || (pressed & 0x4)) {
                        just_pressed = 0;

                        if (mode == SET_TOG_RDS) {
                                // toggle brights_state
                                if(reds_state){
                                        reds_state = 0;                       
                                }else{
                                        reds_state = 1;
                                }

                                // use new value
                                if(reds_state){
                                        nightlight();
                                }else{
                                        all_off();
                                }
                                screenmutex++;
                                display_menu();

                                // display instructions below
                                glcdSetAddress(0, 6);
                                glcdPutStr("Press + to toggle    ", NORMAL);
                                glcdSetAddress(0, 7);
                                glcdPutStr("Press SET to save   ", NORMAL);

                                // put a small arrow next to 'set 12h/24h'
                                // (I don't think this does anything)
                                drawArrow(0, 35, MENU_INDENT -1);
                                glcdSetAddress(MENU_INDENT + 15*6, 4);
				if(reds_state){
					glcdPutStr("ON", INVERTED);
				}else{
					glcdPutStr("OFF", INVERTED);
				}	
                        }
		}
      		screenmutex--;
      		if (pressed & 0x4) _delay_ms(200);  
   	}
#endif
}

void toggle_lamp(void)
{
#if 1 
	uint8_t mode = TOGGLE_LAMP;

  	display_menu();

  	screenmutex++;
  	// put a small arrow next to 'set date'
  	drawArrow(0, 43, MENU_INDENT -1);
  	screenmutex--;
  	
  	timeoutcounter = INACTIVITYTIMEOUT;  

  	while (1) {
    		if (just_pressed & 0x1) { // mode change
      			return;
    		}
    		if (just_pressed || pressed) {
      			timeoutcounter = INACTIVITYTIMEOUT;  
      			// timeout w/no buttons pressed after 3 seconds?
    		} else if (!timeoutcounter) {
      			//timed out!
      			displaymode = SHOW_TIME;     
      			return;
    		}
    		if (just_pressed & 0x2) {
      			just_pressed = 0;
      			screenmutex++;

			if (mode == TOGGLE_LAMP) {
				// now it has been selected
				mode = SET_TOG_LAMP;
				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press + to toggle    ", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to save   ", NORMAL);
			}else{
				mode = TOGGLE_LAMP;

				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press MENU to advance", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to toggle", NORMAL);

                               	// put a small arrow next to 'set 12h/24h'
				// (I don't think this does anything)
                                drawArrow(0, 43, MENU_INDENT -1);
				if(reds_state){
					lamp_state = 0;				
                                	glcdSetAddress(MENU_INDENT + 15*6, 5);
					glcdPutStr("OFF", INVERTED);
				}else{
					lamp_state = 1;
                                	glcdSetAddress(MENU_INDENT + 15*6, 5);
					glcdPutStr("ON", INVERTED);
				}

			}
      		}
		screenmutex--;
                if ((just_pressed & 0x4) || (pressed & 0x4)) {
                        just_pressed = 0;

                        if (mode == SET_TOG_LAMP) {
                                // toggle brights_state
                                if(lamp_state){
                                        lamp_state = 0;                       
                                }else{
                                        lamp_state = 1;
                                }

                                // use new value
                                if(lamp_state){
					turn_lamp_on();
                                }else{
					turn_lamp_off();
                                }
                                screenmutex++;
                                display_menu();

                                // display instructions below
                                glcdSetAddress(0, 6);
                                glcdPutStr("Press + to toggle    ", NORMAL);
                                glcdSetAddress(0, 7);
                                glcdPutStr("Press SET to save   ", NORMAL);

                                // put a small arrow next to 'set 12h/24h'
                                // (I don't think this does anything)
                                drawArrow(0, 43, MENU_INDENT -1);
                                glcdSetAddress(MENU_INDENT + 15*6, 5);
				if(lamp_state){
					glcdPutStr("ON", INVERTED);
				}else{
					glcdPutStr("OFF", INVERTED);
				}	
                        }
		}
      		screenmutex--;
      		if (pressed & 0x4) _delay_ms(200);  
   	}
#endif
}

#ifdef BACKLIGHT_ADJUST
void set_backlight(void)
{
	uint8_t mode = SET_BRIGHTNESS;

	display_menu();

	screenmutex++;
	glcdSetAddress(0, 6);
	glcdPutStr("Press MENU to exit   ", NORMAL);

	// put a small arrow next to 'set 12h/24h'
	drawArrow(0, 43, MENU_INDENT -1);
	screenmutex--;

	timeoutcounter = INACTIVITYTIMEOUT;  

	while (1) {
		if (just_pressed & 0x1) { // mode change
			return;
		}
		if (just_pressed || pressed) {
			timeoutcounter = INACTIVITYTIMEOUT;  
			// timeout w/no buttons pressed after 3 seconds?
		} else if (!timeoutcounter) {
			//timed out!
			displaymode = SHOW_TIME;     
			return;
		}

		if (just_pressed & 0x2) {
			just_pressed = 0;
			screenmutex++;

			if (mode == SET_BRIGHTNESS) {
				DEBUG(putstring("Setting backlight"));
				// ok now its selected
				mode = SET_BRT;
				// print the region 
				glcdSetAddress(MENU_INDENT + 15*6, 5);
				printnumber(OCR2B>>OCR2B_BITSHIFT,INVERTED);

				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press + to change   ", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to save   ", NORMAL);
			} else {
				mode = SET_BRIGHTNESS;
				// print the region normal
				glcdSetAddress(MENU_INDENT + 15*6, 5);
				printnumber(OCR2B>>OCR2B_BITSHIFT,NORMAL);

				glcdSetAddress(0, 6);
				glcdPutStr("Press MENU to exit", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to set   ", NORMAL);
			}
			screenmutex--;
		}
		if ((just_pressed & 0x4) || (pressed & 0x4)) {
			just_pressed = 0;

			if (mode == SET_BRT) {
				OCR2B += OCR2B_PLUS;
				if(OCR2B > OCR2A_VALUE)
					OCR2B = 0;
				screenmutex++;
				display_menu();
				glcdSetAddress(0, 6);
				glcdPutStr("Press + to change    ", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to save    ", NORMAL);

				// put a small arrow next to 'set 12h/24h'
				drawArrow(0, 43, MENU_INDENT -1);
				glcdSetAddress(MENU_INDENT + 15*6, 5);
				printnumber(OCR2B>>OCR2B_BITSHIFT,INVERTED);

				screenmutex--;

				eeprom_write_byte((uint8_t *)EE_BRIGHT, OCR2B);
			}
		}
	}
}
#endif

void set_alarm(void)
{
	uint8_t mode = SET_ALARM;

	display_menu();
	screenmutex++;
	// put a small arrow next to 'set alarm'
	drawArrow(0, 11, MENU_INDENT -1);
	screenmutex--;
	timeoutcounter = INACTIVITYTIMEOUT;  

	while (1) {
		if (just_pressed & 0x1) { // mode change
			return;
		}
		if (just_pressed || pressed) {
			timeoutcounter = INACTIVITYTIMEOUT;  
			// timeout w/no buttons pressed after 3 seconds?
		} else if (!timeoutcounter) {
			//timed out!
			displaymode = SHOW_TIME;     
			return;
		}
		if (just_pressed & 0x2) {
			just_pressed = 0;
			screenmutex++;

			if (mode == SET_ALARM) {
				DEBUG(putstring("Set alarm hour"));
				// ok now its selected
				mode = SET_HOUR;

				// print the hour inverted
				print_alarmhour(alarm_h, INVERTED);
				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press + to change hr.", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to set hour", NORMAL);
			} else if (mode == SET_HOUR) {
				DEBUG(putstring("Set alarm min"));
				mode = SET_MIN;
				// print the hour normal
				glcdSetAddress(MENU_INDENT + 12*6, 1);
				print_alarmhour(alarm_h, NORMAL);
				// and the minutes inverted
				glcdSetAddress(MENU_INDENT + 15*6, 1);
				printnumber(alarm_m, INVERTED);
				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press + to change min", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to set mins", NORMAL);

			} else {
				mode = SET_ALARM;
				// print the hour normal
				glcdSetAddress(MENU_INDENT + 12*6, 1);
				print_alarmhour(alarm_h, NORMAL);
				// and the minutes inverted
				glcdSetAddress(MENU_INDENT + 15*6, 1);
				printnumber(alarm_m, NORMAL);
				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press MENU to advance", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to set", NORMAL);
			}
			screenmutex--;
		}
		if ((just_pressed & 0x4) || (pressed & 0x4)) {
			just_pressed = 0;
			screenmutex++;

			if (mode == SET_HOUR) {
				alarm_h = (alarm_h+1) % 24;
				// print the hour inverted
				print_alarmhour(alarm_h, INVERTED);
				eeprom_write_byte((uint8_t *)EE_ALARM_HOUR, alarm_h);    
				
			}
			if (mode == SET_MIN) {
				alarm_m = (alarm_m+1) % 60;
				glcdSetAddress(MENU_INDENT + 15*6, 1);
				printnumber(alarm_m, INVERTED);
				eeprom_write_byte((uint8_t *)EE_ALARM_MIN, alarm_m);    
			}
			screenmutex--;
			if (pressed & 0x4)
			_delay_ms(200);
			set_begin_seq(alarm_m, alarm_h);
		}
	}
}

void set_time(void)
{
	uint8_t mode = SET_TIME;

	uint8_t hour, min, sec;

	hour = time_h;
	min = time_m;
	sec = time_s;

	display_menu();

	screenmutex++;
	// put a small arrow next to 'set time'
	drawArrow(0, 19, MENU_INDENT -1);
	screenmutex--;

	timeoutcounter = INACTIVITYTIMEOUT;  

	while (1) {
		if (just_pressed & 0x1) { // mode change
			return;
		}
		if (just_pressed || pressed) {
			timeoutcounter = INACTIVITYTIMEOUT;  
			// timeout w/no buttons pressed after 3 seconds?
		} else if (!timeoutcounter) {
			//timed out!
			displaymode = SHOW_TIME;     
			return;
		}
		if (just_pressed & 0x2) {
			just_pressed = 0;
			screenmutex++;

			if (mode == SET_TIME) {
				DEBUG(putstring("Set time hour"));
				// ok now its selected
				mode = SET_HOUR;

				// print the hour inverted
				glcdSetAddress(MENU_INDENT + 10*6, 2);
				print_timehour(hour, INVERTED);
				glcdSetAddress(MENU_INDENT + 18*6, 2);
				if (time_format == TIME_12H) {
					glcdWriteChar(' ', NORMAL);
					if (hour >= 12) {
						glcdWriteChar('P', INVERTED);
					} else {
						glcdWriteChar('A', INVERTED);
					}
				}

				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press + to change hr.", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to set hour", NORMAL);
			} else if (mode == SET_HOUR) {
				DEBUG(putstring("Set time min"));
				mode = SET_MIN;
				// print the hour normal
				glcdSetAddress(MENU_INDENT + 10*6, 2);
				print_timehour(hour, NORMAL);
				// and the minutes inverted
				glcdWriteChar(':', NORMAL);
				printnumber(min, INVERTED);
				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press + to change min", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to set mins", NORMAL);

				glcdSetAddress(MENU_INDENT + 18*6, 2);
				if (time_format == TIME_12H) {
					glcdWriteChar(' ', NORMAL);
					if (hour >= 12) {
						glcdWriteChar('P', NORMAL);
					} else {
						glcdWriteChar('A', NORMAL);
					}
				}
			} else if (mode == SET_MIN) {
				DEBUG(putstring("Set time sec"));
				mode = SET_SEC;
				// and the minutes normal
				if(time_format == TIME_12H) {
					glcdSetAddress(MENU_INDENT + 13*6, 2);
				} else {
					glcdSetAddress(MENU_INDENT + 15*6, 2);
				}
				printnumber(min, NORMAL);
				glcdWriteChar(':', NORMAL);
				// and the seconds inverted
				printnumber(sec, INVERTED);
				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press + to change sec", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to set secs", NORMAL);
			} else {
				// done!
				DEBUG(putstring("done setting time"));
				mode = SET_TIME;
				// print the seconds normal
				if(time_format == TIME_12H) {
					glcdSetAddress(MENU_INDENT + 16*6, 2);
				} else {
					glcdSetAddress(MENU_INDENT + 18*6, 2);
				}
				printnumber(sec, NORMAL);
				// display instructions below
				glcdSetAddress(0, 6);
				glcdPutStr("Press MENU to advance", NORMAL);
				glcdSetAddress(0, 7);
				glcdPutStr("Press SET to set", NORMAL);

				time_h = hour;
				time_m = min;
				time_s = sec;
				writei2ctime(time_s, time_m, time_h, 0, date_d, date_m, date_y);
			}
			screenmutex--;
		}
		if ((just_pressed & 0x4) || (pressed & 0x4)) {
			just_pressed = 0;
			screenmutex++;
			if (mode == SET_HOUR) {
				hour = (hour+1) % 24;
				time_h = hour;

				glcdSetAddress(MENU_INDENT + 10*6, 2);
				print_timehour(hour, INVERTED);
				glcdSetAddress(MENU_INDENT + 18*6, 2);
				if (time_format == TIME_12H) {
					glcdWriteChar(' ', NORMAL);
					if (time_h >= 12) {
						glcdWriteChar('P', INVERTED);
					} else {
						glcdWriteChar('A', INVERTED);
					}
				}
			}
			if (mode == SET_MIN) {
				min = (min+1) % 60;
				if(time_format == TIME_12H) {
					glcdSetAddress(MENU_INDENT + 13*6, 2);
				} else {
					glcdSetAddress(MENU_INDENT + 15*6, 2);
				}
				printnumber(min, INVERTED);
			}
			if (mode == SET_SEC) {
				sec = (sec+1) % 60;
				if(time_format == TIME_12H) {
					glcdSetAddress(MENU_INDENT + 16*6, 2);
				} else {
					glcdSetAddress(MENU_INDENT + 18*6, 2);
				}
				printnumber(sec, INVERTED);
			}
			screenmutex--;
			if (pressed & 0x4)
				_delay_ms(200);
		}
	}
}

void print_timehour(uint8_t h, uint8_t inverted) 
{
	if (time_format == TIME_12H) {
		if (((h + 23)%12 + 1) >= 10 ) {
			printnumber((h + 23)%12 + 1, inverted);
		} else {
			glcdWriteChar(' ', NORMAL);
			glcdWriteChar('0' + (h + 23)%12 + 1, inverted);
		}
	} else {
		glcdWriteChar(' ', NORMAL);
		glcdWriteChar(' ', NORMAL);
		printnumber(h, inverted);
	}
}

void print_alarmhour(uint8_t h, uint8_t inverted) 
{
	if (time_format == TIME_12H) {
		glcdSetAddress(MENU_INDENT + 18*6, 1);
		if (h >= 12) 
			glcdWriteChar('P', NORMAL);
		else
			glcdWriteChar('A', NORMAL);
		glcdWriteChar('M', NORMAL);
		glcdSetAddress(MENU_INDENT + 12*6, 1);

		if (((h + 23)%12 + 1) >= 10 ) {
			printnumber((h + 23)%12 + 1, inverted);
		} else {
			glcdWriteChar(' ', NORMAL);
			glcdWriteChar('0' + (h + 23)%12 + 1, inverted);
		}
	} else {
		glcdSetAddress(MENU_INDENT + 12*6, 1);
		printnumber(h, inverted);
	}
}
