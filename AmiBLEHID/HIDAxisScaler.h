// ------------------------------------------------------------------------------------------------------------------------
// HIDAxisScaler.cpp
// Helper class to scale axis values into a common range
// ------------------------------------------------------------------------------------------------------------------------

#include "hid_report_parser.h"

class HIDAxisScaler
{
    private:    
        int  _logicalMin;
        int  _logicalMax;
        int* _pAxisValue;

        int  _outputMin;
        int  _outputMax;
        bool _isHatSwitch;

    public:
        void Init( hid::Int32Fields::FieldProperties *properties, int outputMin, int outputMax, bool isHatSwitch=false );
        int ScaleValue( int srcValue );
};
