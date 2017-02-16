#include <Gamebuino.h>

// #define SHOW_TITLE_SCREEN
#define MONITOR_RAM

#ifdef MONITOR_RAM
    
    extern uint8_t _end;
    
    void stack_paint()
    {
        uint8_t *p = &_end;
    
        while (p < (uint8_t*)&p)
        {
            *p = 0xc5;
            p++;
        }
    } 
    
    uint16_t max_ram_usage()
    {
        const uint8_t *p = &_end;
        uint16_t c = 2048;
    
        while (*p == 0xc5 && p < (uint8_t*)&p)
        {
            p++;
            c--;
        }
    
        return c;
    } 
#endif

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
    #ifdef MONITOR_RAM
        stack_paint();
    #endif
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
        #ifdef MONITOR_RAM
            gb.display.print(max_ram_usage());
            gb.display.print(" bytes");
        #endif
        gb.display.drawPixel(42, 24);
        gb.display.drawLine(42, 47, 83, 24);
    }
}

