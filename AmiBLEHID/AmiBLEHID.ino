// ------------------------------------------------------------------------------------------------------------------------
// AmiBLEHID
// - Connects a Bluetooth mouse or gamepad to Amiga joystick port.
//
// Intended for the WaveShare ESP32-H2-Mini, without outputs to joystick port connected via a 74HC05 open-collector hex inverter
//
// Board should be set to 'ESP32H2 Dev Module'
// Tools -> USB CDC on boot should be enabled to get serial console output
//
// Required libraries/packages:
// - NimBLE-Arduino - works with 2.5.0)
// - FastLED - works with with 3.10.3 (3.10.4 didn't work)
// - Works with esp32 board package 3.3.10
//
// Turn off Sketch->Optimize for debugging. Any performance gain might help with interrupt responsiveness for CD32 support?
// ------------------------------------------------------------------------------------------------------------------------

#define PIN_U           0
#define PIN_D           1
#define PIN_L           2
#define PIN_R           3
#define PIN_A           4
#define PIN_B           5

#define PIN_Y2          0
#define PIN_X1          1
#define PIN_Y1          2
#define PIN_X2          3

#define PIN_CD32_LATCH  12
#define PIN_CD32_CLOCK  11

#define PIN_BTN_RESET   22
#define PIN_BTN_MODE    25

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <BTScan.h>
#include <BTHIDConn.h>
#include <Preferences.h>
#include <LEDs.h>

void IRAM_ATTR onQuadratureTimer();

// ------------------------------------------------------------------------------------------------------------------------
// States
// ------------------------------------------------------------------------------------------------------------------------

#define ANALOG_STICK_DEADZONE 64                // Analog sticks are scaled to a +/-256 range. Big deadzone/threshold when using for digital input
#define MOUSE_STICK_DEADZONE  40                // Smaller deadzone when using it for mouse

#define NUM_MOUSE_RATES       5

const int k_mouseRates[NUM_MOUSE_RATES] = {48, 64, 96, 160, 224};
const int k_defaultMouseRateIdx = 1;

const int k_hardResetHoldTime   = 140 * 3;      // works out at approx 3 secs. 

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

Preferences     _preferences;
BTScan*         _btScan                  = nullptr;
BTHIDConn*      _btHIDConn               = nullptr;
State           _state                   = State_Init;
int             _scanDuration            = 60*1000;

hw_timer_t     *_quadratureTimer         = NULL;
bool            _quadratureTimerStarted  = false;

int             _currMouseRateIdx        = 0;
GamepadMode     _currGamepadMode         = GamepadMode::Default;

LEDs            _statusLeds;
int             _resetHeldTimer          = 0;

volatile bool   _cd32Polling             = false;
volatile int    _cd32ButtonShiftRegister = 0;
volatile int    _cd32buttonState         = 0;
volatile int    _cd32ticksSincePolled    = 0;

int             _mouseDeltaX             = 0;
int             _mouseDeltaY             = 0;
int             _mouseRateX              = 0;
int             _mouseRateY              = 0;
int             _clampedMouseRateX       = 0;
int             _clampedMouseRateY       = 0;
int             _mouseQuadX              = 0;
int             _mouseQuadY              = 0;
int             _numQuadratureTicks      = 0;

const uint8_t   _quad0[4]                = {0,1,1,0};
const uint8_t   _quad1[4]                = {0,0,1,1};
const int       _quadCounterShiftDown    = 12;

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
    _quadratureTimerStarted = true;
    
    pinMode( PIN_U, OUTPUT );
    pinMode( PIN_D, OUTPUT );
    pinMode( PIN_L, OUTPUT );
    pinMode( PIN_R, OUTPUT );
    pinMode( PIN_A, OUTPUT );
    pinMode( PIN_B, OUTPUT );

    pinMode( PIN_BTN_RESET, INPUT_PULLUP );
    pinMode( PIN_BTN_MODE,  INPUT_PULLUP );

    pinMode( PIN_CD32_LATCH, INPUT_PULLUP );
    pinMode( PIN_CD32_CLOCK, INPUT_PULLUP );

    setupInterrupts();
    
    zeroOutputs();

    Serial.begin(115200);
    
    delay(500);

    _statusLeds.init();

    // Give serial monitor a chance to connect
    delayWithLEDUpdates( 1000 );
    
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
    _btScan->enableBinding( NimBLEDevice::getNumBonds()==0 );
    _btScan->start(_scanDuration);

    _btHIDConn = new BTHIDConn();

    _state = State_Scanning;
}



// ------------------------------------------------------------------------------------------------------------------------
// Delay, but calling LED update function every 16ms
// ------------------------------------------------------------------------------------------------------------------------

int _millisUntilLEDUpdate = 0;

void delayWithLEDUpdates( int millis )
{
    if ( _millisUntilLEDUpdate<millis )
    {
        FastLED.delay( _millisUntilLEDUpdate );
        millis-=_millisUntilLEDUpdate;
        _millisUntilLEDUpdate = 16;
        _statusLeds.process();
    }
    
    while( millis>16 )
    {
        FastLED.delay( 16 );
        _statusLeds.process();
        millis-=16;
        _millisUntilLEDUpdate = 16;
    }

    FastLED.delay(millis);
    _millisUntilLEDUpdate -= millis;    
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
                bool binding = _btScan->isBindingEnabled();
                _statusLeds.setState(LED_STATUS, binding ? LEDMODE_BTBIND : LEDMODE_BTSCAN);
                _statusLeds.setState(LED_MODE,   LEDMODE_OFF);
                zeroOutputs();
                
                // Found a device yet?
                const NimBLEAdvertisedDevice* foundDevice = _btScan->getDeviceToConnect();

                if ( foundDevice )
                {
                    Serial.println("Device found, attempting to connect!");

                    _statusLeds.setState(LED_STATUS, LEDMODE_BTCONNECTING);                 
                    delayWithLEDUpdates(16);

                    if ( _btHIDConn->connect(foundDevice) )
                    {
                        _state = State_Connecting;
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
                    _statusLeds.setState(LED_STATUS, LEDMODE_DISCONNECTED); 
                    _statusLeds.setState(LED_MODE,   LEDMODE_DISCONNECTED);
                    delayWithLEDUpdates(500);

                    _state = State_Scanning;
                    Serial.println("Connection lost, restarting scan!");
                    _btScan->enableBinding( NimBLEDevice::getNumBonds()==0 );
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
            _statusLeds.setState(LED_STATUS, LEDMODE_IDLE);
            _statusLeds.setState(LED_MODE,   LEDMODE_OFF);
            break;
    }        

    // 3ms delay, refresh approx 4x per 60hz frame. Bluetooth will be the limiting factor
    delayWithLEDUpdates(3);

    if ( digitalRead(PIN_BTN_RESET)==0 )
    {
        _resetHeldTimer++;
    }

    if ( _resetHeldTimer>0 && (digitalRead(PIN_BTN_RESET)!=0) || _resetHeldTimer > k_hardResetHoldTime )
    {                        
        _btHIDConn->disconnect();
        _btScan->stop();
        
        
        if ( _resetHeldTimer > k_hardResetHoldTime )
        {    
            _statusLeds.setState(LED_STATUS, LEDMODE_HARDRESET);
            _statusLeds.setState(LED_MODE,   LEDMODE_HARDRESET);        
            delayWithLEDUpdates(750);

            while ( digitalRead(PIN_BTN_RESET)==0 )
            {
                delayWithLEDUpdates(10);
            }

            Serial.println("Reset button held, clearing bonds, restarting scan!");                            
            _btHIDConn->deleteAllBonds();                        
        }
        else
        {
            _statusLeds.setState(LED_STATUS, LEDMODE_DISCONNECTED);
            _statusLeds.setState(LED_MODE,   LEDMODE_DISCONNECTED);        
            delayWithLEDUpdates(750);

            int numBonds = NimBLEDevice::getNumBonds();            
            Serial.printf("Reset button, restarting scan (%d bonds)\n", NimBLEDevice::getNumBonds() );                                                    
        }                    

        _state = State_Scanning;        
        _btScan->enableBinding ( true );
        _btScan->start( _scanDuration, false );            
        _resetHeldTimer = 0;
    }    

}


// ------------------------------------------------------------------------------------------------------------------------
// Check for held input used to change modes
// ------------------------------------------------------------------------------------------------------------------------

bool _prevModeDecButton = false;
bool _prevModeIncButton = false;

int modeCycleCheck( bool decButton, bool incButton )
{
    int dir = 0;

    if ( incButton && !_prevModeIncButton )    
    {
        dir = 1;
    }

    if ( decButton && !_prevModeDecButton )
    {
        dir = 1;
    }

    _prevModeDecButton = decButton;
    _prevModeIncButton = incButton;

    return dir;
}


// ------------------------------------------------------------------------------------------------------------------------
// Gamepad Update
// ------------------------------------------------------------------------------------------------------------------------

// This was an attempt to minimize the impact of interrupt latency by shifting the data bits out in a tight
// loop within this ISR rather than trigger another interrupt from the clock pin. Don't think it's made a lot
// of difference though, as jittery latency on the triggering of this ISR can throw off the timing
//#define CD32_SHIFT_POLLING

static void IRAM_ATTR cd32_latch_isr(void *arg)
{
    if ( !digitalRead( PIN_CD32_LATCH ) )
    {                   
        // Must zero this pin as it's the clock input
        digitalWrite(PIN_A, 0);

        // Copy button state to shift reg var, output first bit
        digitalWrite(PIN_B, _cd32buttonState&1 );

        _cd32ButtonShiftRegister = _cd32buttonState>>1;                
        _cd32Polling             = true;
        _cd32ticksSincePolled    = 0;

#ifdef CD32_SHIFT_POLLING        
        // Loop until all bits haev been shifted out, or uintil our failsafe counter hits zero. 
        int  failsafe  = 500;
        int  shiftBits = 6;
        bool prevClock = false;
        
        while(failsafe>0 && shiftBits>0)
        {
            failsafe--;
            bool clock = digitalRead(PIN_CD32_CLOCK);
            if ( prevClock !=clock )
            {
                if ( !clock )
                {
                    digitalWrite(PIN_B, (_cd32ButtonShiftRegister&1) );         
                    _cd32ButtonShiftRegister>>=1;
                    shiftBits--;                    
                }

                prevClock = clock;
            }
        }        
#endif

    }
    else
    {
        // Restore standard button state        
        digitalWrite(PIN_B, (_cd32buttonState&1) ? 1 : 0 );
        digitalWrite(PIN_A, (_cd32buttonState&2) ? 1 : 0 );
        _cd32Polling = false;
    }     
}

static void IRAM_ATTR cd32_clock_isr( void* arg )
{        
    digitalWrite(PIN_B, (_cd32ButtonShiftRegister&1) );         
    _cd32ButtonShiftRegister>>=1;
}


void setupInterrupts()
{
    // Using ESP-IDF functions to set up ISRs may reduce latency slightly over the Arduino wrapper
    gpio_install_isr_service( ESP_INTR_FLAG_LEVEL3 );

    gpio_config_t io_conf;
    io_conf.pin_bit_mask = (1ULL << (gpio_num_t)PIN_CD32_LATCH);
    io_conf.mode         = GPIO_MODE_INPUT;  
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    io_conf.intr_type    = GPIO_INTR_ANYEDGE;
        
    gpio_config(&io_conf);    
    gpio_isr_handler_add((gpio_num_t)PIN_CD32_LATCH, cd32_latch_isr, NULL);

#ifndef CD32_SHIFT_POLLING
    io_conf.pin_bit_mask = (1ULL << (gpio_num_t)PIN_CD32_CLOCK);
    io_conf.mode         = GPIO_MODE_INPUT;  
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    io_conf.intr_type    = GPIO_INTR_NEGEDGE;
        
    gpio_config(&io_conf);    
    gpio_isr_handler_add((gpio_num_t)PIN_CD32_CLOCK, cd32_clock_isr, NULL);
#endif            
}


void update_gamepad()
{
    // Is controller being polled as a CD32 controller?
    _cd32ticksSincePolled++;
    bool cd32mode = (_cd32ticksSincePolled<250);

    // Stop the timer in CD32 mode to try and help keep interrupts responsive
    // (Mouse emulation is disabled if CD32 pad polling is occuring)
    if (cd32mode == _quadratureTimerStarted)
    {
        if ( _quadratureTimerStarted )
        {
            _quadratureTimerStarted = false;
            timerStop(_quadratureTimer);
        }
        else
        {
            _quadratureTimerStarted = true;
            timerStart(_quadratureTimer);
        }
    }
    
    int deadzone = ANALOG_STICK_DEADZONE;
    int x = _btHIDConn->getGamepadLeftStickXAxis();
    int y = _btHIDConn->getGamepadLeftStickYAxis();    

    //Serial.printf("%d, %d, %d\n",  _btHIDConn->getGamepadLeftStickXAxis(),  _btHIDConn->getGamepadLeftStickYAxis(),  _btHIDConn->getGamepadHatSwitchDir() ); 
    
    bool joyr = (x> deadzone) || (_btHIDConn->getGamepadDigitalXAxis()>0);
    bool joyl = (x<-deadzone) || (_btHIDConn->getGamepadDigitalXAxis()<0);
    bool joyu = (y<-deadzone) || (_btHIDConn->getGamepadDigitalYAxis()<0);
    bool joyd = (y> deadzone) || (_btHIDConn->getGamepadDigitalYAxis()>0);

    if ( joyr && joyl ) joyr=joyl=false;
    if ( joyu && joyd ) joyu=joyd=false;

    bool btna = _btHIDConn->getGamePadButton(0); // A on Xbox pad
    bool btnb = _btHIDConn->getGamePadButton(1); // B on Xbox pad

    // If in up-to-jump mode, the B button is jump, and up is disabled so it's not accidentally triggered
    // For some games, e.g. with up to climb ladders, we may want to allow up too?
    if ( _currGamepadMode == GamepadMode::UpToJump && (!cd32mode) )
    {
        joyu = btnb;
        btnb = _btHIDConn->getGamePadButton(3);  // X on Xbox pad
    }

    if ( cd32mode )
    {
        int _buttonState = 0;

        // Right analog mapped to CD32 buttons, basically for Cecconoid
        int  rx = _btHIDConn->getGamepadRightStickXAxis();
        int  ry = _btHIDConn->getGamepadRightStickYAxis();    
        bool rjoyr = (rx> deadzone);
        bool rjoyl = (rx<-deadzone);
        bool rjoyu = (ry<-deadzone);
        bool rjoyd = (ry> deadzone);

        btna |= rjoyd;
        btnb |= rjoyr;

        if ( btna ) _buttonState |= 2;
        if ( btnb ) _buttonState |= 1;
        if ( _btHIDConn->getGamePadButton(4) || rjoyu )  _buttonState |= 4;
        if ( _btHIDConn->getGamePadButton(3) || rjoyl )  _buttonState |= 8;
        if ( _btHIDConn->getGamePadButton(7) )  _buttonState |= 16;
        if ( _btHIDConn->getGamePadButton(6) )  _buttonState |= 32;
        if ( _btHIDConn->getGamePadButton(11))  _buttonState |= 64;

        noInterrupts();
        _cd32buttonState = _buttonState;    
        if (!_cd32Polling)
        {
            digitalWrite(PIN_A, btna); 
            digitalWrite(PIN_B, btnb); 
        }
        interrupts();

        _clampedMouseRateX  = 0;
        _clampedMouseRateY  = 0;

        digitalWrite(PIN_U, joyu); 
        digitalWrite(PIN_D, joyd); 
        digitalWrite(PIN_L, joyl); 
        digitalWrite(PIN_R, joyr); 
    }
    else
    {   
        // Right analog acts as mouse. All we need to do is set the mouse speed, as the timer for the quadrature output
        // is still running even in gamepad mode. Just don't set the UDLR pins is mouse is active
        int mx = _btHIDConn->getGamepadRightStickXAxis();
        int my = _btHIDConn->getGamepadRightStickYAxis();

        // More comfortable buttons when using right stick for mouse emulation
        btna |= _btHIDConn->getGamePadButton(6);     // L Bumper on Xbox pad
        btnb |= _btHIDConn->getGamePadButton(7);     // R Bumper on Xbox pad        

        if ( mx>MOUSE_STICK_DEADZONE || mx<-MOUSE_STICK_DEADZONE ||
            my>MOUSE_STICK_DEADZONE || my<-MOUSE_STICK_DEADZONE )
        {
            if ( mx> MOUSE_STICK_DEADZONE ) mx -= MOUSE_STICK_DEADZONE;
            if ( mx<-MOUSE_STICK_DEADZONE ) mx += MOUSE_STICK_DEADZONE;

            if ( my> MOUSE_STICK_DEADZONE ) my -= MOUSE_STICK_DEADZONE;
            if ( my<-MOUSE_STICK_DEADZONE ) my += MOUSE_STICK_DEADZONE;

            // Scaling. A 50:50 blend between linear and input squared, 
            // to increase precision near centre. Pure squared was too much
            int smx = (mx * mx)>>8;        
            if ( mx<0 ) smx=-smx;
            smx = (smx+mx);
            
            int smy = (my * my)>>8;
            if ( my<0 ) smy=-smy;  
            smy = (smy+my);

            noInterrupts();
            _clampedMouseRateX  = smx;
            _clampedMouseRateY  = smy;
            interrupts();
        }
        else
        {
            _clampedMouseRateX  = 0;
            _clampedMouseRateY  = 0;

            digitalWrite(PIN_U, joyu); 
            digitalWrite(PIN_D, joyd); 
            digitalWrite(PIN_L, joyl); 
            digitalWrite(PIN_R, joyr); 
        }
    
        // Buttons are same for mouse/joystick
        //
        // Maybe want some debouncing on these to prevent state changes within 1 frame of each other?
        // Although I suspect low bluetooth polling rates will make that less of a problem        
        digitalWrite(PIN_A, btna); 
        digitalWrite(PIN_B, btnb); 
    }
        
    _statusLeds.setButtonIndicator( btna | btnb );
    _statusLeds.setState(LED_STATUS, cd32mode ? LEDMODE_CD32CONTROLLER_ACTIVE : LEDMODE_CONTROLLER_ACTIVE);        

    // Check for mode switch (up-to-jumps)
    int inc = modeCycleCheck( _btHIDConn->getGamePadButton(10), (_btHIDConn->getGamePadButton(11) && !cd32mode) || digitalRead(PIN_BTN_MODE)==0 );

    if ( inc!=0 )
    {        
        _currGamepadMode=(GamepadMode)(_currGamepadMode+inc);

        if ( _currGamepadMode>=GamepadMode::Invalid )
        {
            _currGamepadMode = GamepadMode::Default;             
        }
        else if (_currGamepadMode<(GamepadMode)0)
        {
            _currGamepadMode = (GamepadMode)(GamepadMode::Invalid-1);
        }
        
        saveSettings();
    }    

    _statusLeds.setState(LED_MODE, (LEDState)(LED_GAMEPADMODE_0+_currGamepadMode) );
}


// ------------------------------------------------------------------------------------------------------------------------
// Mouse Update
// ------------------------------------------------------------------------------------------------------------------------

void IRAM_ATTR onQuadratureTimer()
{    
    if ( _clampedMouseRateX!=0 )
    {        
        _mouseQuadX  += _clampedMouseRateX;        
        int idx = (_mouseQuadX>>_quadCounterShiftDown) & 3;
        digitalWrite(PIN_X1, _quad0[idx]);
        digitalWrite(PIN_X2, _quad1[idx]);
    }

    if ( _clampedMouseRateY!=0 )
    {        
        _mouseQuadY  -= _clampedMouseRateY; 
        int idx = (_mouseQuadY>>_quadCounterShiftDown) & 3;
        digitalWrite(PIN_Y1, _quad0[idx]);
        digitalWrite(PIN_Y2, _quad1[idx]);
    }    

    _numQuadratureTicks++;
}

void update_mouse()
{
    if (!_quadratureTimerStarted)
    {
        _quadratureTimerStarted = true;
        timerStart(_quadratureTimer);
    }

    int maxMouseRate = (1<<_quadCounterShiftDown);

    int mx  = _btHIDConn->getMouseDeltaX();
    int my  = _btHIDConn->getMouseDeltaY();
    _btHIDConn->resetMouseDeltas();

    int lmb = _btHIDConn->getMouseButton(0);
    int rmb = _btHIDConn->getMouseButton(1);

    // Apply rate scaling here
    int rate = k_mouseRates[_currMouseRateIdx];    

    _mouseDeltaX += (mx<<3)*rate;
    _mouseDeltaY += (my<<3)*rate;
    
    noInterrupts();
    int smoothingShift = 1;
    _mouseRateX = (_mouseDeltaX / _numQuadratureTicks) >> smoothingShift;
    _mouseRateY = (_mouseDeltaY / _numQuadratureTicks) >> smoothingShift;

    // Enforce max rate so we don't skip phases of the quadrature pulse sequence    
    _clampedMouseRateX = _mouseRateX;
    _clampedMouseRateY = _mouseRateY;    
    if ( _clampedMouseRateX > maxMouseRate ) _clampedMouseRateX =  maxMouseRate;
    if ( _clampedMouseRateX <-maxMouseRate ) _clampedMouseRateX = -maxMouseRate;
    if ( _clampedMouseRateY > maxMouseRate ) _clampedMouseRateY =  maxMouseRate;
    if ( _clampedMouseRateY <-maxMouseRate ) _clampedMouseRateY = -maxMouseRate;    
    

    _mouseDeltaX -= _mouseRateX * _numQuadratureTicks;
    if ( _mouseRateX>0 && _mouseDeltaX<0 ) _mouseDeltaX = 0;
    if ( _mouseRateX<0 && _mouseDeltaX>0 ) _mouseDeltaX = 0;
    
    _mouseDeltaY -= _mouseRateY * _numQuadratureTicks;
    if ( _mouseRateY>0 && _mouseDeltaY<0 ) _mouseDeltaY = 0;
    if ( _mouseRateY<0 && _mouseDeltaY>0 ) _mouseDeltaY = 0;

    _numQuadratureTicks = 0;
    interrupts();

    digitalWrite(PIN_A, lmb); 
    digitalWrite(PIN_B, rmb); 

    // White LED for active mouse
    _statusLeds.setButtonIndicator( lmb | rmb );
    _statusLeds.setState(LED_STATUS, LEDMODE_MOUSE_ACTIVE);    

    // Check for mode switch (cycle mouse speeds)
    int inc = modeCycleCheck( false, _btHIDConn->getMouseButton(2) || digitalRead(PIN_BTN_MODE)==0 );

    if ( inc!=0 )
    {        
        _mouseRateX = _mouseRateY = 0;
        if ( ++_currMouseRateIdx>=NUM_MOUSE_RATES )
        {
             _currMouseRateIdx = 0;
        }        
        saveSettings();
    }

    _statusLeds.setState(LED_MODE, (LEDState)(LED_MOUSERATE_0+_currMouseRateIdx) );    
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

