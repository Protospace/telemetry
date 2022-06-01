/*
 * Code for the Protospace Learn to Solder project board from Zac
 * 2022-04-25
 * 
 * here is the code if anyone is interested.
 * It’s mostly cloned from here GitHub 
 * - tobyoxborrow/gameoflife-arduino: Conway's Game of Life 
 * for Arduino and MAX7219 driving an 8x8 LED matrix and 
 * then modified to work with the board and the ATTiny85
 * 
 * Arduino Setup:
 * Board: ATtiny Microcontrollers -> ATtiny25/45/85
 * Processor: ATtiny85
 * Clock: Internal 1 MHz
 * 
 * 
 */

#include "LedControl.h"
#include <SoftwareSerial.h>

#define DIN_PIN 0   // "DIN" data in pin
#define CLK_PIN 2   // "CLK" clock pin
#define CS_PIN 1    // "CS" pin

// grid dimensions. should not be larger than 8x8
#define MAX_Y 8
#define MAX_X 8

// how many turns per game before starting a new game
// you can also use the reset button on the board
//Zac had 60
#define TURNS_MAX 100

// how many turns to wait if there are no changes before starting a new game
// Zac had 2
#define NO_CHANGES_RESET 10

int TURNS = 0;      // counter for turns
int NO_CHANGES = 0; // counter for turns without changes

// game state. 0 is dead cell, 1 is live cell
boolean grid[MAX_Y][MAX_X] = {
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};

LedControl lc = LedControl(DIN_PIN, CS_PIN, CLK_PIN, 1);
int analogPin = A2;
int pot_read = 0;
int delay_turn = 60;

//                      RX   TX
SoftwareSerial mySerial(PB3, PB2);

void setup() {
	// seed random from unused analog pin
	mySerial.begin(9600);
	randomSeed(analogRead(3));
	pinMode(analogPin, INPUT);
	pinMode(PB3, INPUT);
	pinMode(PB2, OUTPUT);
	// initialise the LED matrix

	lc.shutdown(0, false);
	lc.setIntensity(0, 0);
	lc.clearDisplay(0);

	//reset_grid();
	display_grid();

	//  Serial.begin(9600);
	//  debug_grid();
}

char junk = 'a';

void loop() {
	if (mySerial.available()) {
		for (int j = 0; j < 8; j++) {
			char in = mySerial.read();
			grid[0][j] = (bool) in & (0x1 << j);
		}
	}

	play_gol();
	display_grid();

	delay(200);


	//if (mySerial.available() >= 8) {
	//	for (int i = 0; i < 8; i++) {
	//		for (int j = 0; j < 8; j++) {
	//			char in = mySerial.read();
	//			grid[i][j] = (bool) in & (0x1 << j);
	//		}
	//	}
	//}

	//for (int i = 0; i < 50; i++) {
	//	play_gol();
	//	display_grid();

	//	delay(100);
	//}
}

// play game of life
void play_gol() {
	/*
	   1. Any live cell with fewer than two neighbours dies, as if by loneliness.
	   2. Any live cell with more than three neighbours dies, as if by overcrowding.
	   3. Any live cell with two or three neighbours lives, unchanged, to the next generation.
	   4. Any dead cell with exactly three neighbours comes to life.
	 */

	boolean new_grid[MAX_Y][MAX_X] = {
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0},
	};

	for (int y = 0; y < MAX_Y; y++) {
		for (int x = 0; x < MAX_X; x++) {
			int neighboughs = count_neighboughs(y, x);
			if (grid[y][x] == 1) {
				if ((neighboughs == 2) || (neighboughs == 3)) {
					new_grid[y][x] = 1;
				} else {
					new_grid[y][x] = 0;
				}
			} else {
				if (neighboughs == 3) {
					new_grid[y][x] = 1;
				} else {
					new_grid[y][x] = 0;
				}
			}
		}
	}

	// update the current grid from the new grid and count how many changes occured
	int changes = 0;
	for (int y = 0; y < MAX_Y; y++) {
		for (int x = 0; x < MAX_X; x++) {
			if (new_grid[y][x] != grid[y][x]) {
				changes++;
			}
			grid[y][x] = new_grid[y][x];
		}
	}

	// update global counter when no changes occured
	if (changes == 0) {
		NO_CHANGES++;
	}
}

// count the number of neighbough live cells for a given cell
int count_neighboughs(int y, int x) {
	int count = 0;

	// -- Row above us ---
	if (y > 0) {
		// above left
		if (x > 0) {
			count += grid[y-1][x-1];
		}
		// above
		count += grid[y-1][x];
		// above right
		if ((x + 1) < 8) {
			count += grid[y-1][x+1];
		}
	}

	// -- Same row -------
	// left
	if (x > 0) {
		count += grid[y][x-1];
	}
	// right
	if ((x + 1) < 8) {
		count += grid[y][x+1];
	}

	// -- Row below us ---
	if ((y + 1) < 8) {
		// below left
		if (x > 0) {
			count += grid[y+1][x-1];
		}
		// below
		count += grid[y+1][x];
		// below right
		if ((x + 1) < 8) {
			count += grid[y+1][x+1];
		}
	}

	return count;
}

// reset the grid
// we could set it all to zero then flip some bits on
// but that leads to some predictable games I see quite frequently
// instead, keep previous game state and flip some bits on

void reset_grid() {
	NO_CHANGES = 0;
	TURNS = 0;

	for (int y = 0; y < MAX_Y; y++) {
		for (int x = 0; x < MAX_X; x++) {
			if (random(0, MAX_X) <= 1) {
				grid[y][x] = 1;
			}
		}
	}
}

// display the current grid to the LED matrix
void display_grid() {
	for (int y = 0; y < MAX_Y; y++) {
		for (int x = 0; x < MAX_X; x++) {
			lc.setLed(0, y, x, grid[y][x]);
		}
	}
}

/*
// dump the state of the current grid to the serial connection
void debug_grid() {
for (int y = 0; y < MAX_Y; y++) {
Serial.print("y(");
Serial.print(y);
Serial.print("): ");

for (int x = 0; x < MAX_X; x++) {
Serial.print(grid[y][x]);
Serial.print(", ");
}

Serial.println("");
}
Serial.println("");
}
 */
