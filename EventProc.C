{
//
// Module:      EventProc.C
//
// Description:  Take Generated data files for ALPHA ASIC process to spit out serially  
//               Ping-pong between banks A and B
//               
//
// Revision history:  Created 7-MAY-2021  == GSV
//                    

#include "Riostream.h"

gROOT->Reset();
gStyle->SetMarkerStyle(8);

TRandom2* random = new TRandom2;

// Events to process and File I/O
Int_t eSmpl, nEvents = 1000; // number of Events to process
char inputFileName[200];  // input Event file
ifstream in;
sprintf(inputFileName,"Events_%d.dat",nEvents);  
in.open(inputFileName);
in >> eSmpl;
if(eSmpl != nEvents) {
   printf("Event mismatch!!\n");
} esle {
   printf("Loaded Events_%d.dat successfully\n",nEvents); 
}  
// Output serial stream
char outputFileName[200];
ofstream out;
sprintf(outputFileName,"EventStream.dat");  
out.open(outputFileName);

// Master Processing Settings
Int_t evts2proc = 1; // can be less than total in general

Int_t nBank = 2; // Banks A and B
Int_t nCh = 16; // SuperKEKB bunches
Int_t nSmpl = 256; // number of storage samples

Int_t adcVal[nBank][nCh][nSmpl] = 0;

Int_t iSmpl;  // Write Strobe Address
Int_t iCoarseTime;  // Course time counter

// Event Header and Footer settings
UShort_t Header[8];
Header[0] = 41466;// 0xA1FA
//printf("Header word = %d",Header[0]);
Header[1] = 0;  
// Bank A = 0, Bank B adds 256, lower 8 bits are the sample number when trigger arrives
Header[2] = 0; // Coarse counter upper word
Header[3] = 0; // Coarse counter lower word
Header[4] = 0; // Trigger number
// Sampling parameters
Int_t SamplesAfterTrigger = 17;  //
Int_t LookBackSamples = 34;
Int_t startSmpl; // starting readout sample
Header[5] = 256*SamplesAfterTrigger + LookBackSamples;
//printf("Acquisition word = %d",Header[5]);
Int_t Nsamples = 34; // samples to be read
Header[6];  // second byte will be starting sample number -- will be added for each event
Int_t Hdr6 = 256*Nsamples;  // will hold the added sample number
Header[7] = 0;  // reserved:  First Byte is Number of missed triggers; Second Byte is State Machine(s) status

//UShort_t = ShiftWord = 0;  // bits to be stripped

UShort_t Footer = 3690; // 0x0EGA (OmEGA)
Int_t ROW = 8 + 16*Nsamples + 1; // number of readout words
Int_t ROB = ROW*16;  // number of Read Out Bits

ULong64_t iTime = 0;  // Absolute time
Int_t iEvent = 1;  // so matches

/*
  4 states of operation
  1) Sampling
  2) Trigger  
  3) Pending/writes after and ADC time emulation
  4) Readout In Progress [RIP]
*/
Int_t Sampling = 1; // starting state
Int_t Trigger = 0; // 
Float_t trigTest, trigProb = 1e-5; // how frequent is trigger
Int_t tDelay;  // counter for delay in responding to trigger
Int_t Pending = 0; //
Int_t evtSmpl, evtCCtime = 0;
Int_t nADCclock = 4096; // Wilkinson clocks to complete
Int_t RIP = 0;
Bool_t roBank, HDR, ADC, FTR, datBit = 0;
Int_t nword, index, roc = 0;
Int_t iCh;
Int_t curWord = 0; // pointer to actual sample number

Int_t dEvent, dSmpl;  // triggered event related
    
// arrow of time marches onward for 4 states
//for(iTime=0; iTime<1000000; iTime++) {
Int_t MaxTime = 10000000000;
for(iTime=0; iTime<MaxTime; iTime++) {
// process next 256MHz clock tick
iSmpl++; //increment sample strobe (simplify sampling and readout)
//   printf("iSmpl = %d\n",iSmpl);
if(iSmpl > 255) {
   iSmpl = 0;  // wrap around when get to end
   iCoarseTime++;
}

// 1
if(Sampling) {
   trigTest = random->Uniform(0,1);
   if(trigTest<trigProb) {
      if(iEvent%10==0) printf("Trigger at time %d\n",iTime);
      Sampling = 0;
      Trigger = 1; 
      tDelay = SamplesAfterTrigger;
   }  
}
// 2
if(Trigger) {
   in >> dEvent;
   // latch trigger parameters
   evtSmpl = iSmpl;
   evtCCtime = iCoarseTime;
   // set readout sample
   startSmpl = evtSmpl + SamplesAfterTrigger - LookBackSamples;
   if(startSmpl < 0) startSmpl += 256;   // handle wrap-around
   if(startSmpl > 255) startSmpl -= 256;
   if(iEvent%10==0) printf("Processing Event %d at coarsetime %d, sample %d at Readout starting Sample = %d\n",iEvent,evtCCtime,evtSmpl,startSmpl);
   Header[6] = Hdr6 + startSmpl;  // second byte is starting sample number 
   Header[1] = evtSmpl + 256*roBank;
   Header[2] = iCoarseTime/256;
   Header[3] = iCoarseTime & 255;
//   printf("\n Header1 = %d\n", Header[1]);
//   printf("Header2 = %d\n", Header[2]);
//   printf("Header3 = %d\n\n", Header[3]);
   // fill header info
   for(Int_t iBank=0; iBank<nBank; iBank++) {
      for(Int_t iSmpl=0; iSmpl<nSmpl; iSmpl++) {
         in >> dSmpl;
//         printf("Bank = %d, Sample # %d\n",iBank,dSmpl);
         for(iCh=0; iCh<nCh; iCh++) {
            in >> adcVal[iBank][iCh][iSmpl];
//            printf(" %d ",adcVal[iBank][iCh][iSmpl]);
         }
//         printf("\n",iBank,dSmpl);
      }
   }
   Trigger = 0; // move to pending
   Pending = 1;
   tDelay = nADCclock;  // set for conversion
}

// 3 -- ADC conversion emulation (data already captured at trigger externally)
if(Pending) {   
   tDelay--;
   if(tDelay==0) {
      Pending = 0;
      RIP = 1;  // move on to reading out
      if(iEvent%10==0) printf("ADC conversion complete at time %d\n",iTime);
      HDR = 1; // start to send Header
      roc = 0;
   }
}

// 4 RIP -- Readout In Progress
datBit = 0;
if(RIP) { 
//   printf("at here\n");
   // calculate bit to be shipped
   nword = roc/16;  
   index = 15 - roc%16; // bit to be shipped 
   roc++;  
// send header
   if(HDR) {
      if(nword == 8 && index == 15) {
         HDR = 0; 
         ADC = 1; // move to ADC data
         if(iEvent%10==0) printf("Header complete at time %d\n",iTime);         
      } 
      datBit = Header[nword] & (1 << index);       
   }
// send data
   if(ADC) {
      // subtract off the 8 header words -- to see if done
      curWord = startSmpl + ((nword-8)/16);
// handle wrap-around
      if(curWord > 255) curWord -= 256;
      iCh = (nword-8)%16;
//      if(roBank == 1 & index == 0) printf("curWord = %d; iCh = %d\n",curWord,iCh);
//      if(index == 0) printf("Event = %d; roBank = %d; curWord = %d; iCh = %d; ADC = %d\n",iEvent,roBank,curWord,iCh,adcVal[roBank][iCh][curWord]);
      if( ((nword-8) == 16*Nsamples) && (index == 15) ) {
         ADC = 0; 
         FTR = 1; // move to Footer
         if(iEvent%10==0) printf("ADC complete at time %d\n",iTime); 
      } 
      datBit = adcVal[roBank][iCh][curWord] & (1 << index);
   }
// send footer   
   if(FTR) {
       datBit = Footer & (1 << index);     
       if(nword == (8 + 16*Nsamples + 1) ) {
          FTR = 0;
          RIP = 0;
          roc = 0;
          Sampling = 1; // done sending, return to sampling 
          if(iEvent == nEvents) iTime = MaxTime +1;
          iEvent++; // increment event loop
          roBank = !roBank; // switch to next bank
          if(iEvent%10==0) printf("Footer/event complete at time %d\n\n",iTime); 
       }
   }
}      
   
out << datBit << " ";
} // time step loop
printf("\n Processed %d events\n\n",iEvent-1);


in.close();
out.close();
printf("Calling it a day!\n");
} // end it all for now -- close nicely



