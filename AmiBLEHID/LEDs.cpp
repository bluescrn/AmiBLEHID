// ------------------------------------------------------------------------------------------------------------------------
// LEDs.cpp
// Class to update status LEDs
// ------------------------------------------------------------------------------------------------------------------------

#include <LEDs.h>

// FastLED for new WS2812B LEDs
#define FASTLED_PIN         10
#define FASTLED_TYPE        WS2811
#define FASTLED_COLOR_ORDER GRB
#define FASTLED_BRIGHTNESS  4

// On-board LED
#define LED_PIN 8

// ------------------------------------------------------------------------------------------------------------------------
// Constructor
// ------------------------------------------------------------------------------------------------------------------------

LEDs::LEDs()
{
}


// ------------------------------------------------------------------------------------------------------------------------
// Destructor
// ------------------------------------------------------------------------------------------------------------------------

LEDs::~LEDs()
{
}

// ------------------------------------------------------------------------------------------------------------------------
// init
// ------------------------------------------------------------------------------------------------------------------------

void LEDs::init()
{
    rgbLedWrite(LED_PIN, 0, 0, 0); 
    
    FastLED.addLeds<FASTLED_TYPE, FASTLED_PIN, FASTLED_COLOR_ORDER>(_leds, 2).setCorrection( TypicalLEDStrip );
    FastLED.setBrightness( FASTLED_BRIGHTNESS );    
    
    setState( LED_STATUS, LEDMODE_OFF );
    setState( LED_MODE,   LEDMODE_OFF );
    process();

    _millisecTimer = 0;
}


// ------------------------------------------------------------------------------------------------------------------------
// Process
// - Called roughly once every 16ms. Keeping it simple for now, not handling variable timesteps
// ------------------------------------------------------------------------------------------------------------------------

void LEDs::process()
{    
    _millisecTimer+=16;

    for( int led=0; led<2; led++ )
    {
        CRGB col = 0;

        switch( _ledStates[led] )
        {
            case LEDMODE_OFF:
                col = 0;
                break;
            
            case LEDMODE_BTSCAN:                
                col = (_millisecTimer&255) > 80 ? 0x0000ff : 0;
                break;

            case LEDMODE_BTCONNECTING:
                col = 0x20ff80;
                break;

            case LEDMODE_CONTROLLER_ACTIVE:
                col = 0x00ff00;     // todo - slow throb blue
                break;

            case LEDMODE_MOUSE_ACTIVE:
                col = 0xffffff;     // todo - slow throb green
                break;

            case LEDMODE_IDLE:
                col = 0xffa000;
                break;

            case LEDMODE_DISCONNECTED:
                col = 0xff0000;     // tood - flash?
                break;

            default:
                col = 0xff0000;
                break;
        }

        _leds[led] = col;
    }

    FastLED.show();   
}


// ------------------------------------------------------------------------------------------------------------------------
// setState
// ------------------------------------------------------------------------------------------------------------------------

void LEDs::setState( LEDId id, LEDState state )
{
    _ledStates[id] = state;
}
