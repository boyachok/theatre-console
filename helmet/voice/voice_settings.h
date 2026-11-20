#pragma once
#include <stdint.h>
#include "helmet_voice_wire.h"
enum AudioMode {DRY,ROBOT,TEST};
struct Settings {
  bool enabled=false;AudioMode mode=DRY;
  Show::VoiceTone tone{};
  float gain=8,volume=.15f,frequency=85,mix=1,drive=0,gateDb=-70;
  bool notch=true,vocoder=false,autotune=false,monotone=false;
  bool speechChip=false;
  bool reduceThumps=false;
  int breathReduction=0;
  float wave=0,toneHz=4500,formant=0,clarity=.5f;
  int tuneKey=0,tuneScale=0;
  float retuneMs=0,tuneStrength=1,transpose=0;
};
inline int algorithmId(const Settings &settings) {
  if(settings.speechChip) return 4;
  if(settings.monotone) return 3;
  if(settings.autotune) return 2;
  return settings.vocoder ? 1 : 0;
}
// Exact legacy V2 layout. Its NVS key is retained during migration.
struct StoredProfile {
  uint32_t magic,version,flags,mode;
  float gain,volume,frequency,mix,drive,gateDb,wave,toneHz,formant,clarity;
  uint32_t checksum;
};
