// ------------------------------------------------------------------------------------------------------------------------
// BTHIDConn.cpp
// Class to handle the current bluetooth HID connection
//
// Initially based on this example: https://github.com/esp32beans/BLE_HID_Client
// ------------------------------------------------------------------------------------------------------------------------

#include <NimBLEDevice.h>
#include <BTHIDConn.h>

//#define FULL_LOGGING

// Main scanner class
// ========================================================================================================================

class BTClientCallbacks : public NimBLEClientCallbacks 
{
private:

    bool m_isConnected = false;


    void onConnect(NimBLEClient* pClient) override
    {
        Serial.println("Connected");

        // After connection we should change the parameters if we don't need fast response times.
        // These settings are 150ms interval, 0 latency, 450ms timout.
        // Timeout should be a multiple of the interval, minimum is 100ms.
        // I find a multiple of 3-5 * the interval works best for quick response/reconnect.
        // Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
        //pClient->updateConnParams(120,120,0,60);

        m_isConnected = true;
    };

    void onDisconnect(NimBLEClient* pClient, int reason) override 
    {
        Serial.printf("%s Disconnected, reason = %d\n", pClient->getPeerAddress().toString().c_str(), reason);
        m_isConnected = false;
    };

    // Called when the peripheral requests a change to the connection parameters.
    // Return true to accept and apply them or false to reject and keep the currently used parameters. Default will return true.
    // (Failing to accepts parameters may result in the remote device disconnecting?)
    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) 
    {        
        return true;
    };

    // Security handled here - Note: these are the same return values as defaults
    uint32_t onPassKeyRequest()
    {
        Serial.println("Client Passkey Request");
        return 123456;
    };

    bool onConfirmPIN(uint32_t pass_key)
    {
        Serial.print("The passkey YES/NO number: ");
        Serial.println(pass_key);
        // Return false if passkeys don't match
        return true;
    };

    // Pairing process complete, we can check the results in ble_gap_conn_desc
    void onAuthenticationComplete(ble_gap_conn_desc* desc)
    {
        if(!desc->sec_state.encrypted) 
        {
            Serial.println("Encrypt connection failed - disconnecting");
            // Find the client with the connection handle provided in desc
            NimBLEDevice::getClientByHandle(desc->conn_handle)->disconnect();
            return;
        }
    };


public:   

    bool isConnected()
    {
        return m_isConnected;
    }
};


// Main scanner class
// ========================================================================================================================

// ------------------------------------------------------------------------------------------------------------------------
// Constructor
// ------------------------------------------------------------------------------------------------------------------------

BTHIDConn::BTHIDConn()
{
    m_clientCallbacks = new BTClientCallbacks();
}


// ------------------------------------------------------------------------------------------------------------------------
// Destructor
// ------------------------------------------------------------------------------------------------------------------------

BTHIDConn::~BTHIDConn()
{
    if ( m_clientCallbacks!=nullptr )
    {
        delete m_clientCallbacks;
        m_clientCallbacks = nullptr;
    }
}


// ------------------------------------------------------------------------------------------------------------------------
// Notification handler callback
// ------------------------------------------------------------------------------------------------------------------------

void BTHIDConn::notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, uint8_t reportId, bool isNotify)
{        
    int res = m_parser.Parse(pData, length, reportId);

    if ( isMouse() )
    {
        m_mouseDeltaX += m_mouseAxes[hid::MouseConfig::X];
        m_mouseDeltaY += m_mouseAxes[hid::MouseConfig::Y];
    }
      
#ifdef FULL_LOGGING      
    Serial.printf( "HID reportId %d, parseResult %d, Data: ", reportId,res );
    for (size_t i = 0; i < length; i++) 
    {
        Serial.print(pData[i], HEX);
        Serial.print(',');
    }
    Serial.println();     
#endif    
}


// ------------------------------------------------------------------------------------------------------------------------
// isConnected
// ------------------------------------------------------------------------------------------------------------------------

bool BTHIDConn::isConnected()
{
    if (m_clientCallbacks)
    {
        return m_clientCallbacks->isConnected();
    }

    return false;
}

// ------------------------------------------------------------------------------------------------------------------------
// connect
// ------------------------------------------------------------------------------------------------------------------------

bool BTHIDConn::connect( const NimBLEAdvertisedDevice* device )
{
    NimBLEClient* pClient = nullptr;

    const char HID_SERVICE[]       = "1812";
    const char HID_INFORMATION[]   = "2A4A";
    const char HID_REPORT_MAP[]    = "2A4B";
    const char HID_CONTROL_POINT[] = "2A4C";
    const char HID_REPORT_DATA[]   = "2A4D";


    // Check if we have a client we should reuse first
    if( NimBLEDevice::getCreatedClientCount()>0 )
    {
        // Special case when we already know this device, we send false as the
        // second argument in connect() to prevent refreshing the service database.
        // This saves considerable time and power.
        pClient = NimBLEDevice::getClientByPeerAddress(device->getAddress());        

        if(pClient)
        {
            if(!pClient->connect(device, false)) 
            {
                Serial.println("Reconnect failed");
                return false;
            }
            else
            {
                Serial.println("Reconnected client");
            }
        }        
        else
        {
            // We don't already have a client that knows this device,
            // we will check for a client that is disconnected that we can use.        
            pClient = NimBLEDevice::getDisconnectedClient();
            
            if(pClient)
            {
                Serial.println("Found disconnected client");
            }
        }        
    }

    // No client to reuse? Create a new one. 
    if(!pClient)     
    {
        if(NimBLEDevice::getNumBonds() >= NIMBLE_MAX_CONNECTIONS) 
        {            
            Serial.println("Max clients reached! Full reset, clearing all bonded clients");
            NimBLEDevice::deleteAllBonds();               
        }

        int numBonds = NimBLEDevice::getNumBonds();
            
        // Show bond info
        if ( numBonds>0 )
        {
            Serial.printf("Num Bonds: %d\n", numBonds );
            for (int i = 0; i < numBonds; i++)
            {
                std::string addr = NimBLEDevice::getBondedAddress(i).toString();
                Serial.printf("- Bonded client %d: %s\n", i, addr.c_str() );            
            }
        }


        pClient = NimBLEDevice::createClient();
        Serial.println("New client");

        pClient->setClientCallbacks(m_clientCallbacks, false);

        // Set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout.
        // These settings are safe for 3 clients to connect reliably, can go faster if you have less
        // connections. Timeout should be a multiple of the interval, minimum is 100ms.
        // Min interval: 6 * 1.25ms = 7.5, Max interval: 12 * 1.25ms = 15, 0 latency, 15 * 10ms = 1500ms timeout                
        pClient->setConnectionParams(6,12,0,150);    

        // Set how long we are willing to wait for the connection to complete
        pClient->setConnectTimeout(5*1000);

        if (!pClient->connect(device)) 
        {
            // Created a client but failed to connect, don't need to keep it as it has no data
            NimBLEDevice::deleteClient(pClient);
            Serial.println("Failed to connect");
            return false;
        }
    }

    if(!pClient->isConnected()) 
    {
        if (!pClient->connect(device)) 
        {
            Serial.println("Failed to connect");
            return false;
        }
    }

    Serial.printf("Connected to: %s RSSI: %d\n", pClient->getPeerAddress().toString().c_str(), pClient->getRssi());

    // Now we can read/write/subscribe the charateristics of the services we are interested in
    NimBLERemoteService*        pSvc = nullptr;
    NimBLERemoteCharacteristic* pChr = nullptr;
    NimBLERemoteDescriptor*     pDsc = nullptr;

    uint8_t subscribeCount = 0;

    pSvc = pClient->getService(HID_SERVICE);

    if(pSvc)
    {
        // This returns the HID report descriptor like this
        // HID_REPORT_MAP 0x2a4b Value: 5,1,9,2,A1,1,9,1,A1,0,5,9,19,1,29,5,15,0,25,1,75,1,
        // Copy and paste the value digits to http://eleccelerator.com/usbdescreqparser/
        // to see the decoded report descriptor.        
        pChr = pSvc->getCharacteristic(HID_REPORT_MAP);

        if(pChr)
        {
            if(pChr->canRead()) 
            {
                std::string value = pChr->readValue();

                if ( value.empty() )
                {
                    Serial.println("Connection failed: failed to read HID REPORT MAP value!");
                    pClient->disconnect();
                    return false;
                }

                uint8_t *descriptorData   = (uint8_t *)value.data();
                int      descriptorLength = value.length();

#ifdef FULL_LOGGING
                Serial.print("HID_REPORT_MAP ");
                Serial.print(pChr->getUUID().toString().c_str());
                Serial.print(" Value: ");

                for (size_t i = 0; i < value.length(); i++) 
                {
                    Serial.print(descriptorData[i], HEX);
                    Serial.print(',');
                }
                Serial.println();                
#endif

                m_deviceTypes =  hid::detect_common_input_device_type( descriptorData, descriptorLength );
                bool parserOk = false;

                

                if (m_deviceTypes & hid::FLAG_GAMEPAD)
                {                                 
                    hid::GamepadConfig cfg;
                    auto buttons_ref = m_gamepadButtons.Ref();
                    auto axes_ref = m_gamepadAxes.Ref();  
                    auto cfg_root = cfg.Init( &buttons_ref, &axes_ref, true );
                    int res = m_parser.Init(cfg_root, descriptorData, descriptorLength );
                    Serial.printf("Device is Gamepad (reportId Mappings: %d)\n", m_parser.NumMappings());
                    parserOk = (res==0);                    
                }
                else if (m_deviceTypes & hid::FLAG_MOUSE)
                {                    
                    hid::MouseConfig cfg;
                    auto buttons_ref = m_mouseButtons.Ref();
                    auto axes_ref = m_mouseAxes.Ref();  
                    auto cfg_root = cfg.Init( &buttons_ref, &axes_ref, true );
                    int res = m_parser.Init(cfg_root, descriptorData, descriptorLength );
                    Serial.printf("Device is mouse (reportId Mappings: %d)\n", m_parser.NumMappings());
                    parserOk = (res==0);                    
                }
                else
                {
                    Serial.printf("Unexpected device type. Can't init parser. Disconnecting");
                    pClient->disconnect();
                    return false;                    
                }                

                if (!parserOk)            
                {
                    Serial.printf("Parser init returned error. Disconnecting");
                    pClient->disconnect();
                }
                else
                {
                    Serial.println("HID Report descriptor parsed OK");
                }
            }
            else 
            {
                Serial.println("Connection failed: HID REPORT MAP can't be read!");
                pClient->disconnect();
                return false;
            }
        }
        else 
        {
            Serial.println("Connection failed: HID REPORT MAP not found!");
            pClient->disconnect();
            return false;
        }

        // Subscribe to characteristics HID_REPORT_DATA. One real device reports 2 with the same UUID but
        // different handles. Using getCharacteristic() results in subscribing to only one.
        const std::vector<NimBLERemoteCharacteristic*>&charvector = pSvc->getCharacteristics(true);

        for (auto &it: charvector) 
        {           
            if (it->getUUID() == NimBLEUUID(HID_REPORT_DATA)) 
            {                
                if (it->canNotify()) 
                {                    
                    // Read the ReportID associated with this notification. Why such a struggle to find where this is??                    
                    NimBLERemoteDescriptor *reportIdDesc = it->getDescriptor( BLEUUID((uint16_t)0x2908) );
                    if ( reportIdDesc )
                    {
                        reportIdDesc->readValue();                                        
                        NimBLEAttValue reportIdAttribVal = reportIdDesc->getValue();

                        if ( reportIdAttribVal.length()>0 )
                        {
                            uint8_t reportId = reportIdAttribVal.data()[0];                                                

                            Serial.printf("Subscribing to notifications for UUID %s (handle:%d reportID:%d)\n", it->getUUID().toString().c_str(), it->getHandle(), reportId );
                            //Serial.printf( "%s (reportId = %d)\n", it->toString().c_str(), reportId );

                            if(!it->subscribe(true, [=,this](NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) { notifyCB(pRemoteCharacteristic, pData, length, reportId, isNotify); } )) 
                            {
                                // Disconnect if subscribe failed 
                                Serial.println("Connection failed: Subscribe notification failed!");
                                pClient->disconnect();
                                return false;
                            }
                            else
                            {
                                subscribeCount++;
                            }
                        }
                    }
                }
            }
        }      
    }
  
    if ( subscribeCount>0 )
    {
        Serial.printf("Successfully connected and subscribed to %d notification(s)\n", subscribeCount );
    }

    return true;
}



// --------------------------------------------------------------------------------------------------------------------
// Accessors
// --------------------------------------------------------------------------------------------------------------------

const int hatSwitchXAxis[9] = { 0, 0, 1, 1, 1, 0,-1,-1,-1};
const int hatSwitchYAxis[9] = { 0,-1,-1, 0, 1, 1, 1, 0,-1};

int BTHIDConn::getGamepadDigitalXAxis()
{
    int hat = m_gamepadAxes[hid::GamepadConfig::HAT_SWITCH];
    if ( hat<0 || hat>8 ) hat = 0;
    return hatSwitchXAxis[hat];
}

int BTHIDConn::getGamepadDigitalYAxis()
{
    int hat = m_gamepadAxes[hid::GamepadConfig::HAT_SWITCH];
    if ( hat<0 || hat>8 ) hat = 0;
    return hatSwitchYAxis[hat];
}

int BTHIDConn::getGamepadLeftStickXAxis()
{
    return m_gamepadAxes[hid::GamepadConfig::X];        
}

int BTHIDConn::getGamepadLeftStickYAxis()
{
    return m_gamepadAxes[hid::GamepadConfig::Y];    
}

bool BTHIDConn::getGamePadButton( int idx )
{
    return m_gamepadButtons[idx];    
}

int BTHIDConn::getMouseDeltaX()
{
    return m_mouseDeltaX;
}

int BTHIDConn::getMouseDeltaY()
{
    return m_mouseDeltaY;
}

void BTHIDConn::resetMouseDeltas()
{
    m_mouseDeltaX = m_mouseDeltaY = 0;
}

bool BTHIDConn::getMouseButton( int idx )
{
    return m_mouseButtons[idx];
}


    

