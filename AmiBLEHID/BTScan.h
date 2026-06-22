// ------------------------------------------------------------------------------------------------------------------------
// BTScan.cpp
// Class containing bluetooth scanning logic 
// ------------------------------------------------------------------------------------------------------------------------

#include <NimBLEDevice.h>

class BTScanCallbacks;

class BTScan
{

private:

    BTScanCallbacks* m_scanCallbacks;

public:

    void start( int scanDurationMillisecs=0, bool continueScan=false );
    void stop();
    bool isScanning();
    void enableBinding( bool enable );
    bool isBindingEnabled();
    const NimBLEAdvertisedDevice* getDeviceToConnect();    

    BTScan();
    ~BTScan();    
};