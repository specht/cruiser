#include <Gamebuino.h>

// #define SHOW_TITLE_SCREEN

Gamebuino gb;

void title_screen()
{
    #ifdef SHOW_TITLE_SCREEN
        gb.titleScreen(F("CRUISER"));
    #endif
    gb.battery.show = false;
}

void setup()
{
    gb.begin();
    title_screen();
}

void loop()
{
    if (gb.buttons.pressed(BTN_C))
        title_screen();
    if (gb.update())
    {
        gb.display.println(F("Hello Gamebuino"));
        gb.display.drawPixel(42, 24);
        gb.display.drawLine(42, 47, 83, 24);
    }
}

