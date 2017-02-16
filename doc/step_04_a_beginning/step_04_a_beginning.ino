#include <Gamebuino.h>

// #define SHOW_TITLE_SCREEN
#define MONITOR_RAM

Gamebuino gb;

#ifdef MONITOR_RAM
    int MIN_FREE_RAM = 0xffff;
    
    void update_min_free_ram()
    {
        if (gb.getFreeRam() < MIN_FREE_RAM) MIN_FREE_RAM = gb.getFreeRam();
    }
    
    #define MONITOR_RAM_UPDATE update_min_free_ram();
#else
    #define MONITOR_RAM_UPDATE
#endif

void title_screen()
{
    MONITOR_RAM_UPDATE
    #ifdef SHOW_TITLE_SCREEN
        gb.titleScreen(F("CRUISER"));
    #endif
    gb.battery.show = false;
}

void setup()
{
    MONITOR_RAM_UPDATE
    gb.begin();
    title_screen();
}

void loop()
{
    MONITOR_RAM_UPDATE
    if (gb.buttons.pressed(BTN_C))
        title_screen();
    if (gb.update())
    {
        gb.display.println(F("Hello Gamebuino"));
        #ifdef MONITOR_RAM
            gb.display.println(2048 - MIN_FREE_RAM);
        #endif
        gb.display.drawPixel(42, 24);
        gb.display.drawLine(42, 47, 83, 24);
    }
}

