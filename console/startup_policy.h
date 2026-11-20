#pragma once
// One power-on intent only. Reconnect, publication and browser requests never re-arm it.
class StartupPolicy {
 bool initialized=false,pending=false;
public:
 void begin(bool validPublished){if(initialized)return;initialized=true;pending=validPublished;}
 void cancel(){pending=false;}
 bool waiting()const{return pending;}
 bool take(bool ready){if(!pending||!ready)return false;pending=false;return true;}
};
