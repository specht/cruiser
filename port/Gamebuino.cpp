#include "Gamebuino.h"
#include <GL/glut.h> 
#include <GL/gl.h> 
#include <stdarg.h>

unsigned long micros()
{
    return clock();
}

unsigned long millis()
{
    return clock() / 1000;
}

Buttons::Buttons()
{
    for (int i = 0; i < 256; i++)
        key_down[i] = false;
}

char button_to_key(int button)
{
    switch (button)
    {
        case BTN_A:
            return 'k';
        case BTN_B:
            return 'l';
        case BTN_C:
            return 'r';
        case BTN_UP:
            return 'w';
        case BTN_DOWN:
            return 's';
        case BTN_LEFT:
            return 'a';
        case BTN_RIGHT:
            return 'd';
    }
    return 0;
}

bool Buttons::pressed(int button)
{
    return key_down[button_to_key(button)];
}

bool Buttons::released(int button)
{
    return !key_down[button_to_key(button)];
}

bool Buttons::held(int button, int duration)
{
    return false;
}

bool Buttons::repeat(int button, int count)
{
    return key_down[button_to_key(button)];
}

void Display::print(const char* s)
{
//     printf("%s", s);
}

void Display::println(const char* s)
{
//     printf("%s\n", s);
}

void Display::print(long l)
{
//     printf("%d", i);
}

void Display::print(int i)
{
//     printf("%d", i);
}

void Display::print(float f)
{
//     printf("%1.2f", f);
}

void Display::print(double d)
{
//     printf("%1.2f", d);
}

void Display::drawLine(int x0, int y0, int x1, int y1)
{
    glBegin(GL_LINES);
    glVertex2i(x0 >> 4, y0 >> 4);
    glVertex2i(x1 >> 4, y1 >> 4);
    glEnd();
}

void Display::drawLine(float x0, float y0, float x1, float y1)
{
    glBegin(GL_LINES);
    glVertex2f(x0, y0);
    glVertex2f(x1, y1);
    glEnd();
}

void Display::drawPixel(int x, int y)
{
    glBegin(GL_LINE_LOOP);
    glVertex2f(x + 0.2, y + 0.2);
    glVertex2f(x + 0.8, y + 0.2);
    glVertex2f(x + 0.8, y + 0.8);
    glVertex2f(x + 0.2, y + 0.8);
    glEnd();
}

word Gamebuino::getFreeRam()
{
    return 0;
}

void Gamebuino::titleScreen(const char* name)
{
}

void Gamebuino::begin()
{
}

bool Gamebuino::update()
{
    while (true)
    {
        clock_t c = clock();
        if (((float)(c - last_clock) / CLOCKS_PER_SEC) > 0.05)
        {
            last_clock = c;
            break;
        }
    }
    return true;
}

byte pgm_read_byte(const byte* const addr)
{
    return *addr;
}

word pgm_read_word(const word* const addr)
{
    return *addr;
}

void* memcpy_P(void* dst, const void* src, size_t size)
{
    return memcpy(dst, src, size);
}

void* pgm_read_ptr(const byte* const* addr)
{
    return (void*)(*addr);
}

void* pgm_read_ptr(const word* const* addr)
{
    return (void*)(*addr);
}

void LOG(const char* s, ...)
{
    va_list arglist;
    va_start(arglist, s);
    vprintf(s, arglist);
    va_end(arglist);
}
