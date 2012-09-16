#define halt(x)  while (1)

#define DEBUGGING 0
#define DEBUG(x)  if (DEBUGGING) { x; }
#define DEBUGP(x) DEBUG(putstring_nl(x))

// Software options. Uncomment to enable.
//BACKLIGHT_ADJUST - Allows software control of backlight, assuming you mounted your 100ohm resistor in R2'.
#define BACKLIGHT_ADJUST 1

//OPTION_DOW_DATELONG - Allows showing Day of Week, and Longer format Dates, 
//Like " sat","0807","2010", or " aug","  07","2010" or " sat"," aug","  07","2010".
//#define OPTION_DOW_DATELONG 1

// This is a tradeoff between sluggish and too fast to see
#define MAX_BALL_SPEED 5 // note this is in vector arith.
#define ball_radius 2 // in pixels

// If the angle is too shallow or too narrow, the game is boring
#define MIN_BALL_ANGLE 20

// how fast to proceed the animation, note that the redrawing
// takes some time too so you dont want this too small or itll
// 'hiccup' and appear jittery
#define ANIMTICK_MS 75

// Beeep!
#define ALARM_FREQ 4000
#define ALARM_MSONOFF 300
 
#define MAXSNOOZE 600 // 10 minutes

// how many seconds we will wait before turning off menus
#define INACTIVITYTIMEOUT 10 

/*************************** DISPLAY PARAMETERS */

// how many pixels to indent the menu items
#define MENU_INDENT 8

#define DIGIT_W 32
#define HSEGMENT_H 6
#define HSEGMENT_W 18
#define VSEGMENT_H 20
//#define VSEGMENT_H 25
#define VSEGMENT_W 6
#define DIGITSPACING 4
#define DOTRADIUS 4

#define DISPLAY_H10_X  0
#define DISPLAY_H1_X  VSEGMENT_W + HSEGMENT_W + 2 + DIGITSPACING
#define DISPLAY_M10_X  GLCD_XPIXELS - 2*VSEGMENT_W - 2*HSEGMENT_W - 4 - DIGITSPACING
#define DISPLAY_M1_X  GLCD_XPIXELS - VSEGMENT_W - HSEGMENT_W - 2
#define DISPLAY_TIME_Y 0


/* not used
#define ALARMBOX_X 20
#define ALARMBOX_Y 24
#define ALARMBOX_W 80
#define ALARMBOX_H 20
*/

/*************************** PIN DEFINITIONS */
// there's more in KS0108.h for the display

#define ALARM_DDR DDRB
#define ALARM_PIN PINB
#define ALARM_PORT PORTB
#define ALARM 6

#define PIEZO_PORT PORTC
#define PIEZO_PIN  PINC
#define PIEZO_DDR DDRC
#define PIEZO 3


/*************************** ENUMS */

// Whether we are displaying time (99% of the time)
// alarm (for a few sec when alarm switch is flipped)
// date/year (for a few sec when + is pressed)
#define SCORE_MODE_TIME 0
#define SCORE_MODE_DATE 1
#define SCORE_MODE_YEAR 2
#define SCORE_MODE_ALARM 3
#define SCORE_MODE_DOW 4
#define SCORE_MODE_DATELONG_MON 5
#define SCORE_MODE_DATELONG_DAY 6

// Constants for how to display time & date
#define REGION_US 0
#define REGION_EU 1
#define DOW_REGION_US 2
#define DOW_REGION_EU 3
#define DATELONG 4
#define DATELONG_DOW 5
#define TIME_12H 0
#define TIME_24H 1

//Contstants for calcualting the Timer2 interrupt return rate.
//Desired rate, is to have the i2ctime read out about 6 times
//a second, and a few other values about once a second.
#define OCR2B_BITSHIFT 0
#define OCR2B_PLUS 1
#define OCR2A_VALUE 16
#define TIMER2_RETURN 80
//#define TIMER2_RETURN (8000000 / (OCR2A_VALUE * 1024 * 6))

// displaymode
#define NONE 99
#define SHOW_TIME 0
#define SHOW_DATE 1
#define SHOW_ALARM 2
#define SET_TIME 3
#define SET_ALARM 4
#define TOGGLE_BRIGHTS 5
#define SET_BRIGHTNESS 6
#define SET_VOLUME 7
#define TOGGLE_REDS 8
#define SHOW_SNOOZE 9
#define SET_SNOOZE 10

#define SET_MONTH 11
#define SET_DAY 12
#define SET_YEAR 13
#define TOGGLE_LAMP 14 
#define TOGGLE_DEMO_MODE 15

#define SET_HOUR 101
#define SET_MIN 102
#define SET_SEC 103

#define SET_REG 104

#define SET_BRT 105
#define SET_TOG_BTS 106
#define SET_TOG_RDS 107
#define SET_TOG_LAMP 108
#define SET_TOG_DEMO 109

//DO NOT set EE_INITIALIZED to 0xFF / 255,  as that is
//the state the eeprom will be in, when totally erased.
#define EE_INITIALIZED 0xC3
#define EE_INIT 0
#define EE_ALARM_HOUR 1
#define EE_ALARM_MIN 2
#define EE_BRIGHT 3
#define EE_VOLUME 4
#define EE_REGION 5
#define EE_TIME_FORMAT 6
#define EE_SNOOZE 7
/*****  For Holly's LED strip project *****/
#define NUM_LEDS ((int16_t) (32 * 5))
#define LED_SCALE 8

/*************************** FUNCTION PROTOTYPES */

/*****  For Holly's LED strip project *****/
// removed "static" declaration for fxs called in config.c
void turn_lamp_on(void);
void turn_lamp_off(void);
void brightest_lights(void);
static void ramp_up(void);
static void ramp_down(void);
void nightlight(void);
void all_off(void);
void ramp_fast(void);
static void send_pixels(void);
static void send_byte(uint8_t v);
static inline void clock(const uint8_t value);
static void latch(void);
static void LEDsetup(void);
void set_begin_seq(int, int);

extern int lamp_state; //used by config.c and ratt.c

/****** Monochron and sevenchron stuff ******/
uint8_t leapyear(uint16_t y);
void clock_init(void);
void initbuttons(void);
void tick(void);
void setsnooze(void);
void initanim(void);
void initdisplay(uint8_t inverted);
void step(void);
void setscore(void);
void draw(uint8_t inverted);

void set_alarm(void);
void set_time(void);
//void set_region(void);
//void set_date(void);
void set_backlight(void);
void print_timehour(uint8_t h, uint8_t inverted);
void print_alarmhour(uint8_t h, uint8_t inverted);
void display_menu(void);
void drawArrow(uint8_t x, uint8_t y, uint8_t l);
void setalarmstate(void);
//void beep(uint16_t freq, uint8_t duration);
void printnumber(uint8_t n, uint8_t inverted);


float random_angle_rads(void);

void init_crand(void);
uint8_t dotw(uint8_t mon, uint8_t day, uint8_t yr);

uint8_t i2bcd(uint8_t x);

uint8_t readi2ctime(void);

void writei2ctime(uint8_t sec, uint8_t min, uint8_t hr, uint8_t day,
		  uint8_t date, uint8_t mon, uint8_t yr);

//void print_date(uint8_t month, uint8_t day, uint8_t year, uint8_t mode);
void print_brights_status(int brights_state);
void print_reds_status(int reds_state);

void draw7seg(uint8_t x, uint8_t y, uint8_t segs, uint8_t inverted);
void drawsegment(uint8_t s, uint8_t x, uint8_t y, uint8_t inverted);
void drawdigit(uint8_t d, uint8_t x, uint8_t y, uint8_t inverted);
void drawvseg(uint8_t x, uint8_t y, uint8_t inverted);
void drawhseg(uint8_t x, uint8_t y, uint8_t inverted);
void drawdot(uint8_t x, uint8_t y, uint8_t inverted);
