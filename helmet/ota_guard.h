#pragma once
#include <stdint.h>
#include <stddef.h>
namespace HelmetOta {
// Wrong-role/size/chip protection, NOT a firmware signature. Trusted LAN only.
inline constexpr char ROLE_MARKER[]="THEATRE_HELMET_OTA_ROLE_V1";
constexpr size_t MAX_IMAGE=0x600000;
class ImageGuard {
 size_t bytes=0,matched=0,limit=MAX_IMAGE;bool found=false,bad=false;uint8_t segments=0;
 size_t prefix[sizeof(ROLE_MARKER)]{};
public:
 ImageGuard(){for(size_t i=1,j=0;i<sizeof(ROLE_MARKER)-1;i++){while(j&&ROLE_MARKER[i]!=ROLE_MARKER[j])j=prefix[j-1];if(ROLE_MARKER[i]==ROLE_MARKER[j])j++;prefix[i]=j;}}
 void reset(size_t maximum=MAX_IMAGE){bytes=matched=0;limit=maximum<=MAX_IMAGE?maximum:MAX_IMAGE;found=false;bad=!limit;segments=0;}
 bool feed(const uint8_t *data,size_t n){
  if(bad||(!data&&n)||bytes>limit||n>limit-bytes){bad=true;return false;}
  for(size_t i=0;i<n;i++){
   size_t at=bytes+i;if(at==0&&data[i]!=0xe9)bad=true;
   if(at==1){segments=data[i];if(!segments||segments>16)bad=true;}
   if((at==12&&data[i]!=9)||(at==13&&data[i]!=0))bad=true; // ESP32-S3 image header.
   if(!found){while(matched&&data[i]!=(uint8_t)ROLE_MARKER[matched])matched=prefix[matched-1];if(data[i]==(uint8_t)ROLE_MARKER[matched])matched++;if(matched==sizeof(ROLE_MARKER)-1)found=true;}
  }
  bytes+=n;return !bad;
 }
 bool acceptable()const{return !bad&&bytes>=256&&segments&&found;}
 size_t size()const{return bytes;}
};
}
