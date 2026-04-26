// ------------------------------------------------------------------------------------------------------------------------
// LEDs.h
// Class to update status LEDs
// ------------------------------------------------------------------------------------------------------------------------

#include <FastLED.h>

enum LEDId
{
    LED_STATUS = 0,
    LED_MODE   = 1
};

enum LEDState 
{
    LEDMODE_OFF                 = 0,    
    LEDMODE_BTSCAN              = 1,
    LEDMODE_BTCONNECTING        = 2,
    LEDMODE_CONTROLLER_ACTIVE   = 3,
    LEDMODE_MOUSE_ACTIVE        = 4,
    LEDMODE_IDLE                = 5,
    LEDMODE_DISCONNECTED        = 6
};

class LEDs
{
  
private:

    CRGB     _leds[2];
    LEDState _ledStates[2];
    int      _millisecTimer;

public:


    void init();
    void process();
    void setState( LEDId id, LEDState state );

    LEDs();
    ~LEDs();    
};