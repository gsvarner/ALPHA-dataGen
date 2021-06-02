{
//
// Module:      FPGAemul.C
//
// Description:  Emulate FPGA data capture  
//               
//               
//
// Revision history:  Created 20-MAY-2021  == GSV
//                    

#include "Riostream.h"

gROOT->Reset();
gStyle->SetMarkerStyle(8);

TRandom2* random = new TRandom2;

// Input generated Pedestal values
Int_t nBank = 2; // Banks A and B
Int_t nCh = 16; // SuperKEKB bunches
Int_t nSmpl = 256; // number of storage samples
Int_t pedVal[nBank][nCh][nSmpl] = 0;

// pedestal file for comparison
char inputFileName[200];  // input Event file
ifstream in;
sprintf(inputFileName,"peds.dat");  
in.open(inputFileName);
Int_t dSmpl;

for(Int_t iBank=0; iBank<nBank; iBank++) {
   for(Int_t iSmpl=0; iSmpl<nSmpl; iSmpl++) {
      in >> dSmpl;
      for(Int_t iCh=0; iCh<nCh; iCh++) {
         in >> pedVal[iBank][iCh][iSmpl];
      }
   }
}
in.close();

printf("pedestals grabbed!\n");

// Events to process and File I/O
Int_t eSmpl, nEvents = 1000; // number of Events to process

sprintf(inputFileName,"EventStream.dat");  
in.open(inputFileName);

// Output serial stream
char outputFileName[200];
ofstream out;
sprintf(outputFileName,"RecoveredEvents.txt");  
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
UShort_t Header[8];  // values to be read in
UShort_t refHeader[8];  // Reference header for comparison
refHeader[0] = 41466;// 0xA1FA
//printf("Header word = %d",Header[0]);
refHeader[1] = 0;  
// Bank A = 0, Bank B adds 256, lower 8 bits are the sample number when trigger arrives
refHeader[2] = 0; // Coarse counter upper word
refHeader[3] = 0; // Coarse counter lower word
refHeader[4] = 0; // Trigger number
// Sampling parameters -- target values for reference
Int_t refSamplesAfterTrigger = 17;  //
Int_t refLookBackSamples = 34;
Int_t ref_startSmpl; // starting readout sample
refHeader[5] = 256*refSamplesAfterTrigger + refLookBackSamples;
Int_t SamplesAfterTrigger, refLookBackSamples, Nsamples; // actual values to be filled 
//printf("Acquisition word = %d",Header[5]);
Int_t refNsamples = 34; // samples to be read
refHeader[6] = 256*Nsamples + 0;  // second byte will be starting sample number
refHeader[7] = 0;  // reserved:  First Byte is Number of missed triggers; Second Byte is State Machine(s) status

UShort_t Footer;  // values to be read in
UShort_t refFooter = 3690; // 0x0EGA (OmEGA)
Int_t raw[6000] = 0;
Int_t ROW = 8 + 16*Nsamples + 1; // number of readout words
Int_t ROB = ROW*16;  // number of Read Out Bits

// read vars
Int_t nword, index, roc = 0;
Int_t HDR, ADC, FTR = 0;
Int_t gameOn = 0;
Int_t curWord, iCh;
Int_t iEvent = 1;
Int_t banner = 0; // whether to display full
Int_t roBank, startSmpl, samplesAfterTrigger, lookBackSample;

// building histos
char name[100], title[100];
TH1F *h[2][16][256];
for(Int_t i=0; i<2; i++) {
   for(Int_t j=0; j<16; j++) {               
      for(Int_t k=0; k<256; k++) {
         sprintf(title,"Bank %d, Ch %d, Sample %d",i,j,k);   // Left end title
         sprintf(name,"Index %d %d %d",i,j,k);  // final sample position dependence
         h[i][j][k] = new TH1F(name, title, 200,900,1100);
         h[i][j][k].GetXaxis()->SetTitle("Pedestal value [ADC counts]");
         h[i][j][k]->SetFillColor(3);         
      }
   }
}

// study Pedestal precision
char nameP[100],titleP[100];
sprintf(titleP,"Pedestal Estimate -- 1000 Triggered Events");   // Left end title
sprintf(nameP,"Truth Difference");  // final sample position dependence
hPed = new TH1F(nameP, titleP, 21,-10,10);
hPed.GetXaxis()->SetTitle("Pedestal difference [ADC counts]");
hPed->SetFillColor(3);
// Ped anal vars
Int_t n = 20;
Int_t iPed, iCnt = 0;
Float_t x[n], y[n];
Double_t hMean =0.0;
Double_t hRMS = 0.0;
Float_t diffPed = 0;


// starting RO loop
Int_t iTime = 0;
Bool_t inBit;
Int_t MaxTime = 10000000000;
while(iTime<MaxTime) {
//   if(iTime%10000 == 0) printf("Pass %d\n",iTime);
   iTime++; 
   in>>inBit;
//   printf("inBit = %d\n",inBit);
//   if(inBit != 0) printf("non-zero value found @ %d\n",iTime);
   if(inBit && !gameOn) {
      if(iEvent%10==0) printf("\n For Event %d, the game is afoot at sample %d\n",iEvent,iTime);
      gameOn = 1;
      HDR = 1;
      roc = 0;
      for(Int_t i=0; i<2; i++) {
         for(Int_t j=0; j<16; j++) {               
            for(Int_t k=0; k<256; k++) {
                adcVal[i][j][k] = 0; 
            }
         }
      }
      for(Int_t i=0; i<8; i++) {
         Header[i] = 0;
      } 
      for(Int_t i=0; i<8; i++) {
         raw[i] = 0;
      } 
   }
   if(gameOn) {  
      nword = roc/16;  
      index = 15 - roc%16; // bit to be placed 
      roc++;  
// read header
      if(HDR) {
         if(nword == 8 && index == 15) {
            HDR = 0; 
            ADC = 1; // move to ADC data
            Footer = 0;
            if(iEvent%10==0) printf("Header complete at time %d\n",iTime); 
            banner = 1;        
         } 
         if(nword < 8) Header[nword] += inBit* 2**index;       
      }
// print relevant header information here
      if(banner == 1) {
         for(Int_t i=0; i<8; i++) {
            if(iEvent%10==0) printf("Header word %d = %d  (0x%x)\n",i,Header[i],Header[i]);
         }
         roBank = (Header[1] >> 8) & 1;
         startSmpl = Header[6] & 255;
         Nsamples = Header[6] >> 8;
         if(iEvent%10==0) printf("Reading out %d samples, starting at Bank %d, sample %d\n",Nsamples, roBank, startSmpl);
         banner = 0;
      }    
//       
// send data
      if(ADC) {
         // subtract off the 8 header words -- to see if done
         curWord = startSmpl + (nword-8)/16;
         if(curWord > 255) curWord -= 256;
         iCh = (nword-8)%16;
         if( ((nword-8) == 16*Nsamples) && (index == 15) ) {
            ADC = 0; 
            FTR = 1; // move to Footer
            if(iEvent%10==0) printf("ADC complete at time %d\n",iTime); 
         } 
         adcVal[roBank][iCh][curWord] += inBit* 2**index;
         if(ADC == 1 & index == 0) { 
            // printf("Channel %d, Sample %d = %d\n",iCh,curWord,adcVal[roBank][iCh][curWord]);
            h[roBank][iCh][curWord]->Fill(adcVal[roBank][iCh][curWord]);
            
         }
      }
// send footer   
      if(FTR) {
          Footer += inBit* 2**index;     
          if( (nword-8) == (16*Nsamples + 1) ) {
             FTR = 0;
             if(iEvent == nEvents) iTime = MaxTime +1;
             if(iEvent%50 == 0) {
                for(Int_t iBank=0; iBank<nBank; iBank++) {
                   for(Int_t iSmpl=0; iSmpl<nSmpl; iSmpl++) {
                      in >> dSmpl;
                      for(Int_t iCh=0; iCh<nCh; iCh++) {
                         hMean = h[iBank][iCh][iSmpl]->GetMean();
                         diffPed = pedVal[iBank][iCh][iSmpl] - hMean;
                         hPed->Fill(diffPed);
                      }
                   }
                }
                hRMS = hPed->GetRMS();
                x[iCnt] = iEvent;
                y[iCnt] = hRMS;

                iCnt++;
                if(iCnt<20) hPed->Reset();
                printf("iCnt = %d\n",iCnt);
             }
             iEvent++; // increment event loop
             gameOn = 0;
             if(iEvent%10==0) printf("Footer/event complete at time %d\n",iTime); 
          }
          if(iEvent%10==0 & index == 0) printf("Footer word = %d  (0x%x)\n",Footer,Footer);
      }
      raw[nword] += inBit* 2**index; 
//      if(index == 0) printf("raw[%d] = %x\n",nword,raw[nword]);
   }      


} // end of bit scan loop
printf("\n Think we're done processing here.\n\n");

in.close();

// view histos
TCanvas c1("c1","multipads",1000,600);
c1.Divide(1,1,0,0);

gStyle->SetOptStat(1111); 
//gPad->SetLogy(1);
c1.cd();
hPed->Draw();
char plotName[100];
sprintf(plotName,"pedDiffSummary.png");
c1->Print(plotName);


TCanvas c2("c2","multipads",1000,600);
c2.Divide(1,1,0,0);

TGraph *gr1 = new TGraph(n,x,y);

gStyle->SetOptStat(1111); 
//gPad->SetLogy(1);
c2.cd();

gr1->Draw();
gr1->GetXaxis()->SetTitle("Number triggered Events");
gr1->GetYaxis()->SetTitle("RMS Pedestal difference");
gr1->SetTitle("How many events needed to acquire Pedestals?");
gr1->SetMarkerColor(4);
gr1->Draw("ALP");

char plotName[100];
sprintf(plotName,"pedDiffRunning.png");
c2->Print(plotName);

} // supposedly the end



