// ------------------------------------------------------------------------------------------------------------------------
// BTHIDConn.cpp
// Class to handle a bluetooth HID connection
// ------------------------------------------------------------------------------------------------------------------------

#include <NimBLEDevice.h>
#include "HIDAxisScaler.h"
#include "hid_report_parser.h"


class BTClientCallbacks;

class BTHIDConn
{

private:

    BTClientCallbacks* m_clientCallbacks;

    uint8_t                                         m_deviceTypes;
    hid::SelectiveInputReportParser                 m_parser;    
    hid::BitField<hid::MouseConfig::NUM_BUTTONS>    m_mouseButtons;
	hid::Int32Array<hid::MouseConfig::NUM_AXES>     m_mouseAxes;
    hid::BitField<hid::GamepadConfig::NUM_BUTTONS>  m_gamepadButtons;
	hid::Int32Array<hid::GamepadConfig::NUM_AXES>   m_gamepadAxes;

    HIDAxisScaler m_axisScalerX0;    
    HIDAxisScaler m_axisScalerY0;
    HIDAxisScaler m_axisScalerX1;
    HIDAxisScaler m_axisScalerY1;
    HIDAxisScaler m_axisScalerHat;

    int m_mouseDeltaX;
    int m_mouseDeltaY;    
    
    // False until we've recieved first state update (to ensure axes init to centre position)
    bool m_stateValid;

public:

    bool connect( const NimBLEAdvertisedDevice* device );
    void disconnect();
    void notifyCB( NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, uint8_t reportId, bool isNotify);        
    bool isConnected();    

    void deleteAllBonds();

    bool isGamepad() { return (m_deviceTypes & hid::FLAG_GAMEPAD); }
    bool isMouse()   { return (m_deviceTypes & hid::FLAG_MOUSE);   }

    int  getGamepadDigitalXAxis();
    int  getGamepadDigitalYAxis();
    int  getGamepadHatSwitchDir();
    int  getGamepadLeftStickXAxis();
    int  getGamepadLeftStickYAxis();
    int  getGamepadRightStickXAxis();
    int  getGamepadRightStickYAxis();
    bool getGamePadButton( int idx );

    int  getMouseDeltaX();
    int  getMouseDeltaY();
    void resetMouseDeltas();
    bool getMouseButton( int idx );
    
    BTHIDConn();
    ~BTHIDConn();    
};