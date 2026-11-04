#pragma once
#include <stdint.h>
#include "cue_buttons.h"
namespace Remote {
constexpr uint32_t EVENT_TTL_MS=700,TOKEN_TTL_MS=1500,LEASE_MS=3000;
struct Gate {
 struct Token {uint32_t id=0,at=0;} tokens[3];
 uint32_t lastId=0;uint8_t lastAction=0,lastResult=0;
 enum Decision { REJECT,NEW,DUPLICATE };
 void issue(uint32_t token,uint32_t now){tokens[2]=tokens[1];tokens[1]=tokens[0];tokens[0]={token,now};}
 Decision inspect(uint32_t id,uint8_t action,uint32_t token,uint32_t deadline,uint32_t now)const{
  if(!id||!validAction(action))return REJECT;
  if(id==lastId)return action==lastAction?DUPLICATE:REJECT;
  if(lastId&&(int32_t)(id-lastId)<=0)return REJECT;
  int32_t left=(int32_t)(deadline-now);if(left<0||left>1000)return REJECT;
  for(const auto &t:tokens)if(t.id&&t.id==token&&now-t.at<=TOKEN_TTL_MS)return NEW;
  return REJECT;
 }
 void remember(uint32_t id,uint8_t action,uint8_t result){lastId=id;lastAction=action;lastResult=result;}
};
class Outbox {
public:
 struct Item {uint32_t id=0,token=0,deadline=0,born=0,next=0;Action action=NONE;uint8_t attempts=0;bool sent=false;};
private:
 Item items[4];uint8_t head=0,count=0;uint32_t sequence=0;
public:
 void clear(){head=count=0;}
 bool push(Action action,uint32_t token,uint32_t masterNow,uint32_t now){
  if(count==4||!validAction(action)||!token)return false;if(!++sequence)++sequence;
  auto &e=items[(head+count)%4];e=Item{};e.id=sequence;e.token=token;e.deadline=masterNow+EVENT_TTL_MS;e.born=e.next=now;e.action=action;count++;return true;
 }
 Item *due(uint32_t now){while(count&&now-items[head].born>=EVENT_TTL_MS){head=(head+1)%4;count--;}if(!count)return nullptr;auto &e=items[head];return e.attempts<4&&(int32_t)(now-e.next)>=0?&e:nullptr;}
 void attempted(Item &e,uint32_t now){e.attempts++;e.next=now+120;}
 bool acknowledge(uint32_t id){if(!count||items[head].id!=id)return false;head=(head+1)%4;count--;return true;}
 uint8_t size()const{return count;}
};
}
