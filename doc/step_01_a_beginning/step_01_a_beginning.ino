#include <Gamebuino.h>

Gamebuino gb;

void setup()
{
    gb.begin();
    gb.titleScreen(F("CRUISER"));
}

void loop()
{
    if (gb.update())
    {
        gb.display.print("HELLO GAMEBUINO");
    }
}

