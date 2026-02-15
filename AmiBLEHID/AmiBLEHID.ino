// ------------------------------------------------------------------------------------------------------------------------
// AmiBLEHID
// - Connects a Bluetooth mouse or gamepad to Amiga joystick port.
//
// Intended for the WaveShare ESP32-H2-Mini, without outputs to joystick port connected via a 74HC05 open-collector hex inverter
//
// Board should be set to 'ESP32H2 Dev Module'
// Tools -> USB CDC on boot should be enabled to get serial console output
//
// Required libraries:
// - NimBLE-Arduino (tested with 2.3.7)
// - FastLED (tested with 3.10.3)
// ------------------------------------------------------------------------------------------------------------------------

#include <FastLED.h>

// FastLED (Just a test until I get v2 PCB)
#define FASTLED_PIN         10
#define FASTLED_TYPE        WS2811
#define FASTLED_COLOR_ORDER GRB
#define FASTLED_NUM_LEDS    2
#define FASTLED_BRIGHTNESS  16

// On-board LED
#define LED_PIN 8

#define PIN_U 0
#define PIN_D 1
#define PIN_L 2
#define PIN_R 3
#define PIN_A 4
#define PIN_B 5

#define PIN_Y2 0
#define PIN_X1 1
#define PIN_Y1 2
#define PIN_X2 3

#include <NimBLEDevice.h>
#include <BTScan.h>
#include <BTHIDConn.h>
#include <EEPROM.h>
#include <Preferences.h>

void IRAM_ATTR onQuadratureTimer();

// ------------------------------------------------------------------------------------------------------------------------
// States
// ------------------------------------------------------------------------------------------------------------------------

#define ANALOG_STICK_DEADZONE 12500     // Xbox sticks are +/-32768. Big deadzone needed to avoid unwanted diagonals

#define NUM_MOUSE_RATES 4

const int k_mouseRates[NUM_MOUSE_RATES] = {4, 8, 12, 16};
const int k_defaultMouseRateIdx = 1;
const int k_settingsSaveVersion = 1;

CRGB _leds[FASTLED_NUM_LEDS];

// ------------------------------------------------------------------------------------------------------------------------
// States
// ------------------------------------------------------------------------------------------------------------------------

enum State 
{
    State_Init,           // Initialising
    State_Scanning,       // Scanning for devices
    State_Connecting,     // Connecting
    State_Connected,      // Connected to a HID device
    State_Idle            // Scan timed out, no device found (wait for button to rescan)
};

enum GamepadMode
{
    Default,
    UpToJump,
    Invalid    
};


// ------------------------------------------------------------------------------------------------------------------------
// Main vars
// ------------------------------------------------------------------------------------------------------------------------

Preferences  _preferences;
BTScan*      _btScan            = nullptr;
BTHIDConn*   _btHIDConn         = nullptr;
State        _state             = State_Init;
int          _scanDuration      = 60*1000;

hw_timer_t  *_quadratureTimer   = NULL;

int          _currMouseRateIdx  = 0;
GamepadMode  _currGamepadMode   = GamepadMode::Default;


// ------------------------------------------------------------------------------------------------------------------------
// Main setup function
// ------------------------------------------------------------------------------------------------------------------------

void setup ()
{    
    // Attempting to run the timer at much more than 6KHz causes it to break
    //
    // This corresponds to about 120 output state changes per frame at 50hz, and apparently the Amiga uses an internal 
    // counter with a limit of 128 counts per frame. I thought a 'count' would require a full pulse, but maybe it's a
    // state change, any of the four quadrature phases?
    
    _quadratureTimer = timerBegin(6000);

    timerAttachInterrupt(_quadratureTimer, &onQuadratureTimer);
    timerAlarm(_quadratureTimer, 1, true, 0);
    timerStart(_quadratureTimer);

    rgbLedWrite(LED_PIN, 32, 0, 32); 

    pinMode( PIN_U, OUTPUT );
    pinMode( PIN_D, OUTPUT );
    pinMode( PIN_L, OUTPUT );
    pinMode( PIN_R, OUTPUT );
    pinMode( PIN_A, OUTPUT );
    pinMode( PIN_B, OUTPUT );
    zeroOutputs();

    Serial.begin(115200);
    
    // Give serial monitor a chance to connect
    delay( 2000 );
    
    FastLED.addLeds<FASTLED_TYPE, FASTLED_PIN, FASTLED_COLOR_ORDER>(_leds, FASTLED_NUM_LEDS).setCorrection( TypicalLEDStrip );
    FastLED.setBrightness( FASTLED_BRIGHTNESS );    
    
    _leds[0] = 0x00ffff;
    _leds[1] = 0xff00ff;
    FastLED.show();

    rgbLedWrite(LED_PIN, 32, 32, 32); 

    Serial.println("");
    Serial.println("");
    Serial.println("");
    Serial.println("----------------------------------------");
    Serial.println("AmiBLEHID");
    Serial.println("----------------------------------------");
    loadSettings();
    Serial.println("Starting NimBLE Client");    
    Serial.println("");
  
    _btScan = new BTScan();
    _btScan->start(_scanDuration);

    _btHIDConn = new BTHIDConn();

    _state = State_Scanning;
}


// ------------------------------------------------------------------------------------------------------------------------
// Load settings from prefs store
// ------------------------------------------------------------------------------------------------------------------------

void loadSettings()
{
    _preferences.begin("AmiBLEHID", true);

    _currMouseRateIdx = _preferences.getUInt("MouseRate",   k_defaultMouseRateIdx );
    _currGamepadMode  = (GamepadMode)_preferences.getUInt("GamepadMode", 0 );

    if ( _currMouseRateIdx > NUM_MOUSE_RATES )
    {
         _currMouseRateIdx = 0;
    }    

    _preferences.end();
    saveSettings();
}

// ------------------------------------------------------------------------------------------------------------------------
// Save settings to prefs store
// ------------------------------------------------------------------------------------------------------------------------

void saveSettings()
{
    _preferences.begin("AmiBLEHID", false);

    Serial.printf("Save Settings: Mouse Rate %d, Gamepad Mode %d\n", _currMouseRateIdx, _currGamepadMode );

    _preferences.putUInt( "MouseRate",   _currMouseRateIdx );
    _preferences.putUInt( "GamepadMode", _currGamepadMode );

    _preferences.end();
}



// ------------------------------------------------------------------------------------------------------------------------
// Main loop
// ------------------------------------------------------------------------------------------------------------------------

void loop ()
{        
    switch( _state )
    {
        case State_Scanning:
            {
                // Blink on-board LED blue
                delay(150);
                rgbLedWrite(LED_PIN, 0, 0, 0); 
                delay(200);
                rgbLedWrite(LED_PIN, 0, 0, 64); 

                zeroOutputs();
                
                // Found a device yet?
                const NimBLEAdvertisedDevice* foundDevice = _btScan->getDeviceToConnect();

                if ( foundDevice )
                {
                    Serial.println("Device found, attempting to connect!");
                    
                    rgbLedWrite(LED_PIN, 32, 64, 0);

                    if ( _btHIDConn->connect(foundDevice) )
                    {
                        _state = State_Connected;
                    }            
                    else
                    {
                        Serial.println("Failed to connect, resuming scan!");
                        _btScan->start( _scanDuration, true );
                    }
                }            
                else if (!_btScan->isScanning())
                {
                    _state = State_Idle;
                }
            }
            break;

        case State_Connecting:
            {
                rgbLedWrite(LED_PIN, 32, 64, 0);
                delay(15);

                if ( _btHIDConn->isConnected() )
                {                
                    _state = State_Connected;
                }
            }

            break;

        case State_Connected:
            {
                if ( !_btHIDConn->isConnected() )
                {
                    zeroOutputs();

                    _state = State_Scanning;
                    Serial.println("Connection lost, restarting scan!");
                    rgbLedWrite(LED_PIN, 0, 64, 0); 
                    delay(1000);
                    _btScan->start( _scanDuration, false );
                }
                else
                {
                    if ( _btHIDConn->isGamepad() )
                    {
                        update_gamepad();               
                    }

                    if ( _btHIDConn->isMouse() )
                    {
                        update_mouse();
                    }
                }
            }

            break;

        case State_Idle:
            rgbLedWrite(LED_PIN, 0, 64, 0); 
            delay(15);
            break;
    }    
}


// ------------------------------------------------------------------------------------------------------------------------
// Do LED flashes on switching mode
// ------------------------------------------------------------------------------------------------------------------------

void modeCycleLEDFlash( int numFlashes )
{
    zeroOutputs();

    delay(250);
    for( int i=0; i<numFlashes; i++ )
    {
        rgbLedWrite(LED_PIN, 64, 255, 0); 
        delay(150);
        rgbLedWrite(LED_PIN, 0, 0, 0); 
        delay(175);
    }
    delay(100);
}


// ------------------------------------------------------------------------------------------------------------------------
// Check for held input used to change modes
// ------------------------------------------------------------------------------------------------------------------------

int _modeCycleHoldTimer = 0;

bool modeCycleCheck( bool button )
{
    if ( button )
    {
        // Gets called every 4ms or so
        _modeCycleHoldTimer++;

        if ( _modeCycleHoldTimer == 100 )
        {
            return true;
        }
    }
    else
    {
        _modeCycleHoldTimer = 0;
    }

    return false;
}


// ------------------------------------------------------------------------------------------------------------------------
// Gamepad Update
// ------------------------------------------------------------------------------------------------------------------------

void update_gamepad()
{
    int deadzone = ANALOG_STICK_DEADZONE;
    int x = _btHIDConn->getGamepadLeftStickXAxis() - 32768;
    int y = _btHIDConn->getGamepadLeftStickYAxis() - 32768;    
    
    bool joyr = (x> deadzone) || (_btHIDConn->getGamepadDigitalXAxis()>0);
    bool joyl = (x<-deadzone) || (_btHIDConn->getGamepadDigitalXAxis()<0);
    bool joyu = (y<-deadzone) || (_btHIDConn->getGamepadDigitalYAxis()<0);
    bool joyd = (y> deadzone) || (_btHIDConn->getGamepadDigitalYAxis()>0);

    bool btna = _btHIDConn->getGamePadButton(0);    // A on Xbox pad
    bool btnb = _btHIDConn->getGamePadButton(1);    // B on Xbox pad

    // If in up-to-jump mode, the B button is jump, and up is disabled so it's not accidentally triggered
    // For some games, e.g. with up to climb ladders, we may want to allow up too?
    if ( _currGamepadMode == GamepadMode::UpToJump )
    {
        joyu = btnb;
        btnb = _btHIDConn->getGamePadButton(3);  // X on Xbox pad
    }
    
    digitalWrite(PIN_U, joyu); 
    digitalWrite(PIN_D, joyd); 
    digitalWrite(PIN_L, joyl); 
    digitalWrite(PIN_R, joyr); 

    digitalWrite(PIN_A, btna); 
    digitalWrite(PIN_B, btnb); 

    // Green LED (green = Xbox color...)
    rgbLedWrite(LED_PIN, 32, 0, 0); 

    // Check for mode switch (up-to-jumps)
    if ( modeCycleCheck( _btHIDConn->getGamePadButton(10) ||    // Start or Sel on Xbox pad
                         _btHIDConn->getGamePadButton(11) ))
    {        
        _currGamepadMode=(GamepadMode)(_currGamepadMode+1);
        if ( _currGamepadMode>=GamepadMode::Invalid )
        {
            _currGamepadMode = GamepadMode::Default;             
        }
        modeCycleLEDFlash(_currGamepadMode+1);
        saveSettings();
    }    

    // 4ms delay, refresh 4x per 60hz frame. Bluetooth will be the limiting factor
    delay(4);
}


// ------------------------------------------------------------------------------------------------------------------------
// Mouse Update
// ------------------------------------------------------------------------------------------------------------------------

int _mouseDeltaX        = 0;
int _mouseDeltaY        = 0;
int _mouseRateX         = 0;
int _mouseRateY         = 0;
int _clampedMouseRateX  = 0;
int _clampedMouseRateY  = 0;
int _mouseQuadX         = 0;
int _mouseQuadY         = 0;

const uint8_t quad0[4] = {0,1,1,0};
const uint8_t quad1[4] = {0,0,1,1};

const int     quadCounterShiftDown = 14;

void IRAM_ATTR onQuadratureTimer()
{    
    if ( _clampedMouseRateX!=0 )
    {        
        _mouseQuadX  += _clampedMouseRateX;        
        int idx = (_mouseQuadX>>quadCounterShiftDown) & 3;
        digitalWrite(PIN_X1, quad0[idx]);
        digitalWrite(PIN_X2, quad1[idx]);
    }

    if ( _clampedMouseRateY!=0 )
    {        
        _mouseQuadY  -= _clampedMouseRateY; 
        int idx = (_mouseQuadY>>quadCounterShiftDown) & 3;
        digitalWrite(PIN_Y1, quad0[idx]);
        digitalWrite(PIN_Y2, quad1[idx]);
    }    
}

void update_mouse()
{
    int maxMouseRate = (1<<quadCounterShiftDown);

    int mx  = _btHIDConn->getMouseDeltaX();
    int my  = _btHIDConn->getMouseDeltaY();
    _btHIDConn->resetMouseDeltas();

    int lmb = _btHIDConn->getMouseButton(0);
    int rmb = _btHIDConn->getMouseButton(1);

    // Apply rate scaling here
    int rate = k_mouseRates[_currMouseRateIdx];
    _mouseDeltaX += (mx<<4)*rate;
    _mouseDeltaY += (my<<4)*rate;

    // Consume 1/4 of our reamaining delta per timer tick
    // Gives a slight smoothing/deceleration
    _mouseRateX = _mouseDeltaX >> 2;
    _mouseRateY = _mouseDeltaY >> 2;

    noInterrupts();
    // Enforce max rate so we don't skip phases of the quadrature pulse sequence    
    _clampedMouseRateX = _mouseRateX;
    _clampedMouseRateY = _mouseRateY;    
    if ( _clampedMouseRateX > maxMouseRate ) _clampedMouseRateX =  maxMouseRate;
    if ( _clampedMouseRateX <-maxMouseRate ) _clampedMouseRateX = -maxMouseRate;
    if ( _clampedMouseRateY > maxMouseRate ) _clampedMouseRateY =  maxMouseRate;
    if ( _clampedMouseRateY <-maxMouseRate ) _clampedMouseRateY = -maxMouseRate;    
    interrupts();

    _mouseDeltaX -= _mouseRateX;
    if ( _mouseRateX>0 && _mouseDeltaX<0 ) _mouseDeltaX = 0;
    if ( _mouseRateX<0 && _mouseDeltaX>0 ) _mouseDeltaX = 0;
    
    _mouseDeltaY -= _mouseRateY;
    if ( _mouseRateY>0 && _mouseDeltaY<0 ) _mouseDeltaY = 0;
    if ( _mouseRateY<0 && _mouseDeltaY>0 ) _mouseDeltaY = 0;

    //Serial.printf("MouseXY %d, %d QuadPosXY %d, %d\n", _clampedMouseRateX, _clampedMouseRateY, _mouseQuadX>>(quadCounterShiftDown+2), _mouseQuadY>>(quadCounterShiftDown+2) );

    digitalWrite(PIN_A, lmb); 
    digitalWrite(PIN_B, rmb); 

    rgbLedWrite(LED_PIN, 32, 32, 16); 

    // Check for mode switch (cycle mouse speeds)
    if ( modeCycleCheck(_btHIDConn->getMouseButton(2)) )
    {        
        _mouseRateX = _mouseRateY = 0;

        if ( ++_currMouseRateIdx>=NUM_MOUSE_RATES )
        {
             _currMouseRateIdx = 0;
        }
        modeCycleLEDFlash(_currMouseRateIdx+1);
        saveSettings();
    }

    // 4ms delay, refresh 4x per 60hz frame. Bluetooth will be the limiting factor
    delay(4);
}


// ------------------------------------------------------------------------------------------------------------------------
// Zero all outputs
// ------------------------------------------------------------------------------------------------------------------------

void zeroOutputs()
{
    // Zero these out to prevent the timer interrupt updating the mouse quadrature outputs
    // (Currently we leave the timer running, and don't stop it when switching mode or in gamepad mode)
    noInterrupts();    
    _clampedMouseRateY = 0;
    _clampedMouseRateX = 0;
    interrupts();

    digitalWrite(PIN_U, 0); 
    digitalWrite(PIN_D, 0); 
    digitalWrite(PIN_L, 0); 
    digitalWrite(PIN_R, 0); 
    digitalWrite(PIN_A, 0); 
    digitalWrite(PIN_B, 0); 
}

