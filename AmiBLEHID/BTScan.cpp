// ------------------------------------------------------------------------------------------------------------------------
// BTScan.cpp
// Class containing bluetooth scanning logic 
//
// Initially based on this example: https://github.com/esp32beans/BLE_HID_Client
// ------------------------------------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <BTScan.h>

// Scan callbacks class
// ========================================================================================================================

class BTScanCallbacks: public NimBLEScanCallbacks
{
private:

    const NimBLEAdvertisedDevice* m_deviceToConnect = nullptr;

    bool m_enableBinding = false;

    // --------------------------------------------------------------------------------------------------------------------
    // onDiscovered
    // --------------------------------------------------------------------------------------------------------------------

    void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override 
    {
        //Serial.printf("Device discovered: %s\n", advertisedDevice->toString().c_str() );
    }


    // --------------------------------------------------------------------------------------------------------------------
    // onResult
    // --------------------------------------------------------------------------------------------------------------------

    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override 
    {
        const char HID_SERVICE[] = "1812";

        int advType = advertisedDevice->getAdvType();

        if (advertisedDevice->isConnectable())
        {    
            int  numBonds = NimBLEDevice::getNumBonds();
            bool isBonded = false;

            for (int i = 0; i < numBonds; i++)
            {                                    
                if ( NimBLEDevice::getBondedAddress(i) == advertisedDevice->getAddress() )
                {
                    isBonded = true;                            
                }                        
            }
            
            // If we're connected to a bonded device, it may not be advertising with any details, we have to recognise it by address and 
            // try to connect. This is the case with the Logitech MX Anywhere 3, although most devices seem to advertise with service ID
            // and other details even once bounded.
            if (advertisedDevice->haveServiceUUID() || isBonded)
            {
                if (advertisedDevice->isAdvertisingService(NimBLEUUID(HID_SERVICE)) || isBonded)
                {            
                    if ( isBonded )
                    {
                        Serial.printf("AdvType %d: Bonded HID device:   %s\n", advType, advertisedDevice->toString().c_str() );            
                    }
                    else
                    {
                        Serial.printf("AdvType %d: Unbonded HID device: %s\n", advType, advertisedDevice->toString().c_str() );                                
                    }

                    if ( m_enableBinding || isBonded )
                    {
                        // stop scan before connecting
                        NimBLEDevice::getScan()->stop();

                        // Store ref to device
                        m_deviceToConnect = advertisedDevice;            
                    }
                }
                else
                {
                    // No HID service
                    Serial.printf("AdvType %d: Non-HID device:      %s\n", advType, advertisedDevice->getAddress().toString().c_str() );
                }
            }    
            else
            {
                // No service ID
                Serial.printf("AdvType %d: Unknown device:      %s\n", advType, advertisedDevice->getAddress().toString().c_str() );
            }
        }
    
    };


    // --------------------------------------------------------------------------------------------------------------------
    // onScanEnd
    // --------------------------------------------------------------------------------------------------------------------

    void onScanEnd(const NimBLEScanResults& results, int reason) override 
    {
        Serial.printf("Scan Ended, reason: %d, device count: %d\n", reason, results.getCount());
    }

public:

    // --------------------------------------------------------------------------------------------------------------------
    // reset
    // --------------------------------------------------------------------------------------------------------------------

    void reset()
    {
        m_deviceToConnect = nullptr;
    }

    // --------------------------------------------------------------------------------------------------------------------
    // enableBinding
    // --------------------------------------------------------------------------------------------------------------------

    void enableBinding( bool enable )
    {
        m_enableBinding = enable;
    }

    // --------------------------------------------------------------------------------------------------------------------
    // isBindingEnabled
    // --------------------------------------------------------------------------------------------------------------------

    bool isBindingEnabled()
    {
        return m_enableBinding;
    }

    // --------------------------------------------------------------------------------------------------------------------
    // getDeviceToConnect
    // --------------------------------------------------------------------------------------------------------------------

    const NimBLEAdvertisedDevice* getDeviceToConnect()
    {
        return m_deviceToConnect;
    }
};



// Main scanner class
// ========================================================================================================================

// ------------------------------------------------------------------------------------------------------------------------
// Constructor
// ------------------------------------------------------------------------------------------------------------------------

BTScan::BTScan()
{
    m_scanCallbacks = new BTScanCallbacks();

    // Initialize NimBLE, no device name spcified as we are not advertising
    NimBLEDevice::init("");

    // Enable bonding    
    NimBLEDevice::setSecurityAuth(true, true, false);

    // Set the transmit power (+9db), default is 3db
    //NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    // Set up scan:
    NimBLEScan* pScan = NimBLEDevice::getScan();

    // Register callbacks for when advertisers are found
    pScan->setScanCallbacks(m_scanCallbacks);

    // Set scan interval (how often) and window (how long) in milliseconds
    //pScan->setInterval(100);
    //pScan->setWindow(100);

    // Active scan will gather scan response data from advertisers but will use more energy from both devices   
        
    pScan->setActiveScan(true);
}


// ------------------------------------------------------------------------------------------------------------------------
// Destructor
// ------------------------------------------------------------------------------------------------------------------------

BTScan::~BTScan()
{
    if ( m_scanCallbacks!=nullptr )
    {
        delete m_scanCallbacks;
        m_scanCallbacks = nullptr;
    }
}


// ------------------------------------------------------------------------------------------------------------------------
// start
// ------------------------------------------------------------------------------------------------------------------------

void BTScan::start( int scanDurationMillisecs, bool continueScan )
{
    // Start scanning for advertisers for the scan time specified, 0 = forever
    NimBLEScan* pScan = NimBLEDevice::getScan();

    m_scanCallbacks->reset();
    pScan->start(scanDurationMillisecs, continueScan, !continueScan );
}


// ------------------------------------------------------------------------------------------------------------------------
// stop
// ------------------------------------------------------------------------------------------------------------------------

void BTScan::stop()
{
    NimBLEScan* pScan = NimBLEDevice::getScan();
    if ( pScan )
    {
        pScan->stop();
    }
}


// ------------------------------------------------------------------------------------------------------------------------
// enableBinding
// ------------------------------------------------------------------------------------------------------------------------

void BTScan::enableBinding( bool enable )
{
    m_scanCallbacks->enableBinding( enable );
}

// --------------------------------------------------------------------------------------------------------------------
// isBindingEnabled
// --------------------------------------------------------------------------------------------------------------------

bool BTScan::isBindingEnabled()
{
    return m_scanCallbacks->isBindingEnabled();
}

// ------------------------------------------------------------------------------------------------------------------------
// isScanning
// ------------------------------------------------------------------------------------------------------------------------

bool BTScan::isScanning()
{
    NimBLEScan* pScan = NimBLEDevice::getScan();   
    return pScan->isScanning();
}


// ------------------------------------------------------------------------------------------------------------------------
// getDeviceToConnect
// ------------------------------------------------------------------------------------------------------------------------

const NimBLEAdvertisedDevice* BTScan::getDeviceToConnect()
{
    return m_scanCallbacks->getDeviceToConnect();
}

