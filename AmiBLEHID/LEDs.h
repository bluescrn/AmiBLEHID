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
    LEDMODE_OFF                   = -1,    
    LEDMODE_BTSCAN                = 0,
    LEDMODE_BTBIND                = 1,
    LEDMODE_BTCONNECTING          = 2,
    LEDMODE_CONTROLLER_ACTIVE     = 3,
    LEDMODE_CD32CONTROLLER_ACTIVE = 4,
    LEDMODE_MOUSE_ACTIVE          = 5,
    LEDMODE_CD32MOUSE_ACTIVE      = 6,
    LEDMODE_IDLE                  = 7,
    LEDMODE_DISCONNECTED          = 8,
    LEDMODE_HARDRESET             = 9,

    LED_MOUSERATE_0             = 10,
    LED_MOUSERATE_1             = 11,
    LED_MOUSERATE_2             = 12,
    LED_MOUSERATE_3             = 13,
    LED_MOUSERATE_4             = 14,

    LED_GAMEPADMODE_0           = 20,
    LED_GAMEPADMODE_1           = 21,
    LED_GAMEPADMODE_2           = 22,
};

class LEDs
{
  
private:

    CRGB     _leds[2];
    LEDState _ledStates[2];
    int      _millisecTimer;
    bool     _buttonPressed;

public:


    void init();
    void process();
    void setState( LEDId id, LEDState state );
    void setButtonIndicator( bool pressed );

    LEDs();
    ~LEDs();    
};