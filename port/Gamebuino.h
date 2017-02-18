#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#define SCREEN_SCALE 8
#define LINE_COORDINATE_TYPE int
#define LOG_ALREADY_DEFINED

#define byte uint8_t
#define word uint16_t
#define PROGMEM
#define F(x) (x)
#define PI M_PI

#define LCDWIDTH 84
#define LCDHEIGHT 48

#define swap(x, y) do { typeof(x) SWAP = x; x = y; y = SWAP; } while (0)

#define BTN_A 1
#define BTN_B 2
#define BTN_C 3
#define BTN_UP 4
#define BTN_DOWN 5
#define BTN_LEFT 6
#define BTN_RIGHT 7

unsigned long micros();
class Buttons {
public:
    Buttons();
    bool pressed(int button);
    bool released(int button);
    bool held(int button, int duration);
    bool repeat(int button, int count);
    bool key_down[256];
};

class Display {
public:
    void print(const char* s = "");
    void println(const char* s = "");
    void print(long l);
    void print(int i);
    void print(float f);
    void print(double d);
    void drawLine(int x0, int y0, int x1, int y1);
    void drawLine(float x0, float y0, float x1, float y1);
    void drawPixel(int x, int y);
};

class Battery {
public:
    bool show;
};

class Gamebuino {
public:
    word getFreeRam();
    void titleScreen(const char* name);
    void begin();
    bool update();
    Battery battery;
    Buttons buttons;
    Display display;
    clock_t last_clock;
};

byte pgm_read_byte(const byte* const addr);
word pgm_read_word(const word* const addr);
void* pgm_read_ptr(const byte* const* addr);
void* pgm_read_ptr(const word* const* addr);

void LOG(const char* s, ...);
