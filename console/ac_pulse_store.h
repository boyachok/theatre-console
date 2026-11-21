#pragma once
#include <Preferences.h>
#include "control_types.h"
#include "ac_pulse.h"
namespace AcPulse {
class Store {
 Preferences prefs;bool initialized=false,opened=false;
public:
 uint16_t pulseUs=DEFAULT_US;bool fault=false;
 void begin(){if(initialized)return;initialized=true;opened=prefs.begin("stage-ac",false);if(!opened){fault=true;return;}size_t n=prefs.getBytesLength("pulse-v1");if(!n)return;Record r;if(n!=sizeof(r)||prefs.getBytes("pulse-v1",&r,sizeof(r))!=sizeof(r)||!r.validRecord()){fault=true;return;}pulseUs=r.pulseUs;}
 Result save(uint16_t us){if(!valid(us))return INVALID;if(!opened)return IO_ERROR;if(us==pulseUs&&!fault)return APPLIED;Record r;r.pulseUs=us;r.seal();if(prefs.putBytes("pulse-v1",&r,sizeof(r))!=sizeof(r))return IO_ERROR;pulseUs=us;fault=false;return APPLIED;}
};
}
