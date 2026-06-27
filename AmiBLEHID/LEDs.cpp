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
                col = (_millisecTimer&1023) > 512 ? 0x0000ff : 0;
                break;

            case LEDMODE_BTBIND:                
                col = ((_millisecTimer*3)&511) > 300 ? 0x0000ff : 0;
                break;

            case LEDMODE_BTCONNECTING:
                col = 0x0000ff;
                break;

            case LEDMODE_CONTROLLER_ACTIVE:            
                col = _buttonPressed ? 0x006000 : 0x002000;     // dim green
                break;

            case LEDMODE_CD32CONTROLLER_ACTIVE:
                col = _buttonPressed ? 0x300050 : 0x100020;     // dim purple
                break;
                 
            case LEDMODE_MOUSE_ACTIVE:
                col = _buttonPressed ? 0x006060 : 0x002020;     // dim blue
                break;

            case LEDMODE_IDLE:
                col = 0x220000;
                break;

            case LEDMODE_DISCONNECTED:
                col = 0x802000;
                break;

            case LEDMODE_HARDRESET:
                col =  ((_millisecTimer*3)&511) > 300 ? 0x800000 : 0;
                break;


            case LED_MOUSERATE_0:    col = 0x00A000;  break; 
            case LED_MOUSERATE_1:    col = 0x208000;  break; 
            case LED_MOUSERATE_2:    col = 0x606000;  break; 
            case LED_MOUSERATE_3:    col = 0x802000;  break; 
            case LED_MOUSERATE_4:    col = 0xA00000;  break; 

            case LED_GAMEPADMODE_0:  col = 0x00A000;  break; 
            case LED_GAMEPADMODE_1:  col = 0x606000;  break; 
            case LED_GAMEPADMODE_2:  col = 0xA00000;  break; 

            default:
                col = 0xff0000;
                break;
        }

        _leds[led] = col;
    }

//    Hmm, is this not needed? Seems to be updating without?
//    FastLED.show();   
}


// ------------------------------------------------------------------------------------------------------------------------
// setState
// ------------------------------------------------------------------------------------------------------------------------

void LEDs::setState( LEDId id, LEDState state )
{
    _ledStates[id] = state;
}

void LEDs::setButtonIndicator( bool pressed )
{
    _buttonPressed = pressed;
}

