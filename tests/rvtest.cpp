#include "../Source/dsp/RhodesFX.h"
#include <cstdio>
int main(){ brh::SpringReverb r; r.prepare(48000); double e=0; float pk=0;
 for(int i=0;i<48000*20;i++){ float l= i==0?1.f:0.f, rr=l; r.process(l,rr,1.0f,1.0f); if(i>48000*15) e+=l*l; pk=std::max(pk,std::abs(l)); }
 printf("max-decay spring: peak %.3f, energy 15-20s %.3g\n",pk,e); }
