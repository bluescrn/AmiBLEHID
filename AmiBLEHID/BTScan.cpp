// ------------------------------------------------------------------------------------------------------------------------
// BTScan.cpp
// Class containing bluetooth scanning logic 
//
// Initially based on this example: https://github.com/esp32beans/BLE_HID_Client
// ------------------------------------------------------------------------------------------------------------------------

#include <NimBLEDevice.h>
#include <BTScan.h>

// Scan callbacks class
// ========================================================================================================================

class BTScanCallbacks: public NimBLEScanCallbacks
{
private:

    const NimBLEAdvertisedDevice* m_deviceToConnect = nullptr;

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

//        if (  (advertisedDevice->getAdvType() == BLE_HCI_ADV_TYPE_ADV_DIRECT_IND_HD) ||
//              (advertisedDevice->getAdvType() == BLE_HCI_ADV_TYPE_ADV_DIRECT_IND_LD) )
        {
            if (advertisedDevice->isConnectable())
            {    
                if ( advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(NimBLEUUID(HID_SERVICE)))
                {
                    Serial.printf("Advertised HID device found: %s\n", advertisedDevice->toString().c_str() );            

                    // stop scan before connecting
                    NimBLEDevice::getScan()->stop();

                    // Store ref to device
                    m_deviceToConnect = advertisedDevice;            
                }    
                else
                {
                    Serial.printf("Advertised Non-HID device found: %s\n", advertisedDevice->toString().c_str() );            
                }
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
    //NimBLEDevice::setSecurityAuth(true, true, false);

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

