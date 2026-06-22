// ------------------------------------------------------------------------------------------------------------------------
// HIDAxisScaler.cpp
// Helper class to scale axis values into a common range
// ------------------------------------------------------------------------------------------------------------------------

#include <HIDAxisScaler.h>
#include "hid_report_parser.h"


void HIDAxisScaler::Init( hid::Int32Fields::FieldProperties *properties, int outputMin, int outputMax, bool isHatSwitch )
{
    _logicalMin  = _outputMin = outputMin;
    _logicalMax  = _outputMax = outputMax;
    _isHatSwitch = isHatSwitch;

    if ( properties )
    {
        _logicalMin  = properties->logical_min;
        _logicalMax  = properties->logical_max;
    }    
}


int HIDAxisScaler::ScaleValue( int srcValue )
{
    if ( _isHatSwitch )
    {
        if ( srcValue<_logicalMin )
        {
            return 0;
        }        
    }

    int t = (((srcValue - _logicalMin)<<12)+0x7ff)/(_logicalMax-_logicalMin);        
    return _outputMin + (((_outputMax-_outputMin)*t)>>12);    
}
