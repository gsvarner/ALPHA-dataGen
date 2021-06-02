{
//
// Module:      StripTest.C
//
// Description:  Simple check of how best to strip off the bits  
//               
//
// Revision history:  Created 14-MAY-2021  == GSV
//                    

#include "Riostream.h"

gROOT->Reset();
gStyle->SetMarkerStyle(8);

TRandom2* random = new TRandom2;

// Event Header and Footer settings
UShort_t Header[8];
Header[0] = 41466;// 0xA1FA
Bool_t datBit = 0;

for(Int_t i=0; i<16; i++){
   datBit = Header[0] & (1 << (15-i)); 
   printf("Bit %d = %d\n",i,datBit);  
}

} // end it all here

