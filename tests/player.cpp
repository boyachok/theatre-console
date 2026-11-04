#include <cassert>
#include <cstdio>
#include "../control/player.h"
#include "../control/protocol.h"
int main() {
  assert(sizeof(UartStats)==76&&sizeof(Packet)==72&&sizeof(DebugPacket)==100&&sizeof(PwmPacket)==64);
  Packet probe=packet(PROBE,123,456);assert(probe.protocol==4&&probe.size==72&&probe.session==123&&probe.seq==456);
  LegacyPacket old=legacyPacket(probe);assert(old.protocol==3&&old.size==64&&old.session==123&&old.seq==456);
  assert(expandPacket(old).protocol==3&&expandPacket(old).seq==456);
  Packet echo=packet(ECHO,123);assert(reportedPWMFrequency(echo)==0);
  setPWMFrequency(echo,25000);assert(reportedPWMFrequency(echo)==25000);
  setPWMFrequency(echo,156250);assert(reportedPWMFrequency(echo)==156250);
  echo.protocol=3;assert(!reportedPWMFrequency(echo));
  Packet command=packet(COMMAND,123);command.value=255;setPWMFrequency(command,25000);
  assert(command.value==255&&!reportedPWMFrequency(command));
  assert(validControl(UART_CLEAR,0,0)&&!validControl(UART_CLEAR,0,1));
  assert(validControl(UART_POLL,0,1)&&!validControl(UART_POLL,0,2));
  Player p;p.begin();p.autoPoll=false;p.tick();auto s=p.diagnostics();
  assert(s.txFrames==1&&s.txBytes==4&&s.txPin==17&&s.rxPin==18);
  assert(s.txLen==4&&s.lastTx[0]==0xAA&&s.lastTx[1]==4&&s.lastTx[3]==0xAE);
  // A wire from TX to RX returns our own command, NOT a valid player status reply.
  for(uint8_t c:mockUARTOut)mockUARTIn.push_back(c);
  mockNow=1;p.tick();s=p.diagnostics();
  assert(s.rxBytes==4&&s.validFrames==1&&s.loopbacks==1&&s.replies==0&&!p.ready());
  assert(s.rxLen==4&&s.lastRx[3]==0xAE&&s.polling==0);
  const uint8_t status[]={0xAA,1,1,1,0xAD};
  for(uint8_t c:status)mockUARTIn.push_back(c);
  mockNow=10;p.tick();s=p.diagnostics();assert(s.replies==1&&p.state==1&&s.replyAge==0);
  assert(!p.ready()); // Second boot STOP still pending.
  mockNow=2000;p.tick();assert(p.ready());
  uint32_t tx=s.txFrames;mockNow=2100;p.tick();tx=p.diagnostics().txFrames;
  mockNow=2500;p.tick();assert(p.diagnostics().txFrames==tx); // Polling really paused.
  const uint8_t bad[]={0xAA,1,1,1,0};
  for(uint8_t c:bad)mockUARTIn.push_back(c);
  p.tick();assert(p.diagnostics().checksumErrors==1);
  mockUARTIn.push_back(0xAA);mockUARTIn.push_back(1);mockUARTIn.push_back(100);
  p.tick();assert(p.diagnostics().lengthErrors==1);
  mockUARTIn.push_back(0xAA);mockUARTIn.push_back(1);p.tick();
  mockNow+=101;p.tick();assert(p.diagnostics().frameTimeouts==1);
  // RX history is chronological and bounded at the last 16 bytes.
  for(int i=0;i<30;i++)mockUARTIn.push_back(i);
  p.tick();s=p.diagnostics();assert(s.rxLen==16&&s.lastRx[0]==14&&s.lastRx[15]==29);
  p.clearStats();s=p.diagnostics();assert(!s.rxBytes&&!s.txBytes&&!s.validFrames&&!s.checksumErrors&&!s.rxLen);
  assert(p.state==1&&p.ready()); // Debug counters do not alter safety/playback state.
  p.poll();mockWriteLimit=0;p.tick();assert(p.diagnostics().txFailures==1);mockWriteLimit=1000;
  puts("UART TX/RX, real replies vs loopback, pause, frame errors, history and partial-write tests passed");
}
