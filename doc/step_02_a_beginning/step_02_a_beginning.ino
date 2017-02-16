#include <Gamebuino.h>

Gamebuino gb;

void title_screen()
{
    gb.titleScreen(F("CRUISER"));
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

