/*
 * project.c
 *
 * Main file
 *
 * Author: Peter Sutton. Modified by <YOUR NAME HERE>
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>

#include "ledmatrix.h"
#include "scrolling_char_display.h"
#include "buttons.h"
#include "serialio.h"
#include "terminalio.h"
#include "score.h"
#include "timer0.h"
#include "game.h"
#include "health.h"
#include "joystick.h"
#include "memory.h"

#define F_CPU 8000000L
#include <util/delay.h>

// Function prototypes - these are defined below (after main()) in the order
// given here
void initialise_hardware(void);
void splash_screen(void);
void new_game(void);
void play_game(void);
void handle_pause(void);
void handle_game_over(void);

// ASCII code for Escape character
#define ESCAPE_CHAR 27

/////////////////////////////// main //////////////////////////////////
int main(void) {
	// Setup hardware and call backs. This will turn on 
	// interrupts.
	initialise_hardware();
	
	// Show the splash screen message. Returns when display
	// is complete
	splash_screen();
	
	while(1) {
		new_game();
		play_game();
		handle_game_over();
	}
}

void initialise_hardware(void) {
	ledmatrix_setup();
	init_button_interrupts();
	// Setup serial port for 19200 baud communication with no echo
	// of incoming characters
	init_serial_stdio(19200,0);
	
	init_timer0();
	// Turn on global interrupts
	sei();
}

void splash_screen(void) {
	// Clear terminal screen and output a message
	clear_terminal();
	move_cursor(10,10);
	printf_P(PSTR("Asteroids"));
	move_cursor(10,12);
	printf_P(PSTR("CSSE2010/7201 project by Minh Anh Bui 45041899"));

	// Output the scrolling message to the LED matrix
	// and wait for a push button to be pushed.
	ledmatrix_clear();
	while(1) {
		set_scrolling_display_text("ASTEROIDS 45041899", COLOUR_GREEN);
		// Scroll the message until it has scrolled off the 
		// display or a button is pushed
		while(scroll_display()) {
			_delay_ms(150);
			if(button_pushed() != NO_BUTTON_PUSHED) {
				return;
			}
		}
	}
}

void new_game(void) {
	// Initialise the game and display
	initialise_game();
	
	// Clear the serial terminal
	clear_terminal();
	
	//Print the rank
	retrieve();

	// Initialise the score
	init_score();
	
	//Initialise health
	set_health();
	
	// Clear a button push or serial input if any are waiting
	// (The cast to void means the return value is ignored.)
	(void)button_pushed();
	clear_serial_input_buffer();
}

void play_game(void) {
	uint32_t current_time, last_move_time;
	int8_t button;
	char serial_input, escape_sequence_char;
	uint8_t characters_into_escape_sequence = 0;
	
	// Get the current time and remember this as the last time the projectiles
    // were moved.
	current_time = get_current_time();
	last_move_time = current_time;
			
	//Set up current score, current life so 
	//they are also displayed at value 0
	volatile uint32_t current_score = 1;
	uint8_t current_life = 1;
	
	//Asteroid will fall faster when reach next_level
	uint32_t delay = 4444;
	uint32_t last_drop = current_time;
	uint8_t next_level = 5;
	
	//The game use freeze to delay order from the joystick
	uint32_t freeze = current_time;
	
	// We play the game until it's over
	while(!is_game_over()) {
		// Check for input - which could be a button push or serial input.
		// Serial input may be part of an escape sequence, e.g. ESC [ D
		// is a left cursor key press. At most one of the following three
		// variables will be set to a value other than -1 if input is available.
		// (We don't initalise button to -1 since button_pushed() will return -1
		// if no button pushes are waiting to be returned.)
		// Button pushes take priority over serial input. If there are both then
		// we'll retrieve the serial input the next time through this loop
		serial_input = -1;
		escape_sequence_char = -1;
		button = button_pushed();
		
		//Check if the base is hit by asteroid
		hit_base();
		
		if(button == NO_BUTTON_PUSHED) {
			// No push button was pushed, see if there is any serial input
			if(serial_input_available()) {
				// Serial data was available - read the data from standard input
				serial_input = fgetc(stdin);
				// Check if the character is part of an escape sequence
				if(characters_into_escape_sequence == 0 && serial_input == ESCAPE_CHAR) {
					// We've hit the first character in an escape sequence (escape)
					characters_into_escape_sequence++;
					serial_input = -1; // Don't further process this character
				} else if(characters_into_escape_sequence == 1 && serial_input == '[') {
					// We've hit the second character in an escape sequence
					characters_into_escape_sequence++;
					serial_input = -1; // Don't further process this character
				} else if(characters_into_escape_sequence == 2) {
					// Third (and last) character in the escape sequence
					escape_sequence_char = serial_input;
					serial_input = -1;  // Don't further process this character - we
										// deal with it as part of the escape sequence
					characters_into_escape_sequence = 0;
				} else {
					// Character was not part of an escape sequence (or we received
					// an invalid second character in the sequence). We'll process 
					// the data in the serial_input variable.
					characters_into_escape_sequence = 0;
				}
			}
		}
		current_time = get_current_time();
		char joystick_order =' ';
		//Check joystick input
		if (current_time >= freeze + 200)
		{
			joystick_order = joystick();
			freeze=current_time;
		}

		// Process the input. 
		if(button==3 || escape_sequence_char=='D' || serial_input=='L' || serial_input=='l' || joystick_order=='L') {
			// Button 3 pressed OR left cursor key escape sequence completed OR
			// letter L (lowercase or uppercase) pressed - attempt to move left
			move_base(MOVE_LEFT);
		} else if(button==2 || escape_sequence_char=='A' || serial_input==' '|| joystick_order=='A') {
			// Button 2 pressed or up cursor key escape sequence completed OR
			// space bar pressed - attempt to fire projectile
			fire_projectile();
		} else if(button==1 || escape_sequence_char=='B') {
			// Button 1 pressed OR down cursor key escape sequence completed
			// Ignore at present
		} else if(button==0 || escape_sequence_char=='C' || serial_input=='R' || serial_input=='r'|| joystick_order=='R') {
			// Button 0 pressed OR right cursor key escape sequence completed OR
			// letter R (lowercase or uppercase) pressed - attempt to move right
			move_base(MOVE_RIGHT);
		} else if(serial_input == 'p' || serial_input == 'P') {
			// pause/unpause the game until 'p' or 'P' is pressed again
			serial_input = -1;
			move_cursor(5,5);
			printf("Paused");
			while(serial_input != 'p' && serial_input != 'P') {
				//Unless unpause is pressed, do nothing
				serial_input = fgetc(stdin);
			}
			move_cursor(5,5);
			printf("Fire!!");
		} 
		// else - invalid input or we're part way through an escape sequence -
		// do nothing
		
		if (current_score>=next_level && delay>100)
		{
			next_level+=5;
			delay = delay-100;
		}
		
		if (current_time >= last_drop+delay)
		{
			falling_asteroids();
			last_drop=current_time;
		}
		
		if(!is_game_over() && current_time >= last_move_time + 500) {
			// 500ms (0.5 second) has passed since the last time we moved
			// the projectiles - move them - and keep track of the time we 
			// moved them			
			advance_projectiles();
			last_move_time = current_time;
		}
		
		//Display the score in terminal
		if (current_score != get_score())
		{
			current_score = get_score();
			move_cursor(20, 8);
			printf("Score: %5lu", current_score);
		}
		
		//Display the health in terminal
		if (current_life != get_health())
		{
			current_life = get_health();
			move_cursor(20, 10);
			printf("Life: %6d", get_health());
		}
	}
	// We get here if the game is over.
}

void handle_game_over() {
	check_record();
	move_cursor(10,14);
	printf_P(PSTR("GAME OVER"));
	move_cursor(10,15);
	printf_P(PSTR("Press a button to start again"));
	while(button_pushed() == NO_BUTTON_PUSHED) {
		; // wait
	}
}
