#include <cassert>
#include <cstdio>
#include <cstring>
#include "../control/lan_wire.h"
int main(){
  const uint8_t a[]={0xA0,1,2,3,4,5},b[]={0xA0,1,2,3,4,6},c[]={0xA0,1,2,3,4,7},all[]={255,255,255,255,255,255};
  uint8_t payload[100]{},wire[120]{};for(int i=0;i<100;i++)payload[i]=i;
  size_t n=encodeLAN(a,b,payload,64,wire);assert(n==84);LanHeader h{};
  assert(decodeLAN(wire,n,h)&&h.length==64&&!memcmp(h.mac,a,6));
  assert(addressedTo(h,b)&&!addressedTo(h,c)); // Stale DHCP IP cannot route to the wrong MAC.
  assert(!memcmp(wire+sizeof(h),payload,64));
  for(size_t i=0;i<n;i++)assert(!decodeLAN(wire,i,h));
  assert(!decodeLAN(wire,n+1,h));
  uint8_t saved=wire[0];wire[0]^=1;assert(!decodeLAN(wire,n,h));wire[0]=saved;
  saved=wire[4];wire[4]=99;assert(!decodeLAN(wire,n,h));wire[4]=saved;
  wire[5]=1;assert(!decodeLAN(wire,n,h));wire[5]=0;
  wire[6]=100;assert(!decodeLAN(wire,n,h));wire[6]=64;
  assert(encodeLAN(a,all,payload,100,wire)==120&&decodeLAN(wire,120,h)&&addressedTo(h,c));
  assert(!encodeLAN(all,b,payload,64,wire));
  assert(!encodeLAN(a,b,payload,63,wire));
  assert(encodeLAN(a,b,payload,72,wire)==92&&decodeLAN(wire,92,h)&&h.length==72);
  const uint8_t zero[6]{};assert(!encodeLAN(zero,b,payload,64,wire));
  LanPeers peers;assert(peers.lookup(a)==0);assert(peers.observe(a,0x01020304,10));
  assert(peers.lookup(a)==0x01020304);assert(peers.observe(a,0x05060708,20));
  assert(peers.lookup(a)==0x05060708&&!peers.observe(b,0,20));
  assert(!peers.observe(all,42,20));peers.clear();assert(!peers.lookup(a));
  uint8_t ids[17][6]{};
  for(int i=0;i<17;i++){ids[i][0]=0xA0;ids[i][5]=i+1;}
  for(int i=0;i<16;i++)assert(peers.observe(ids[i],i+1,i));
  peers.observe(ids[0],100,1000);peers.observe(ids[16],200,1001);
  assert(peers.lookup(ids[0])==100&&!peers.lookup(ids[1])&&peers.lookup(ids[16])==200);
  puts("LAN framing, bounds, destination identity, DHCP updates and peer eviction checks passed");
}
