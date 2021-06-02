{
//
// Module:      pedGen.C
//
// Description:  Assign Pedestals to the ALPHA ASIC 
//               Simulate 2 banks of 256 samples on each of 16 channels (per ASIC)
//               
//
// Revision history:  Created 3-MAY-2021  == GSV
//                    

#include "Riostream.h"

gROOT->Reset();
gStyle->SetMarkerStyle(8);

char outputFileName[200];
ofstream out;

// Histograms definitions
char title1[100],title2[100],title3[100],title4[100];
char name1[100],name2[100],name3[100],name4[100];

// Bank A
sprintf(title1,"ALPHA Pedestals - Bank A");   // Left end title
sprintf(name1,"Assuming 1000 count ped nom");  // final sample position dependence
h1 = new TH2F(name1, title1, 256, 0, 256,16,0,16);
h1.GetXaxis()->SetTitle("Sample number [of 256]");
h1.GetYaxis()->SetTitle("Channel Number");

sprintf(title3,"ALPHA Pedestals - Bank A");   // Left end title
sprintf(name3,"19 count RMS");  // final sample position dependence
h3 = new TH1F(name3, title3, 200,900,1100);
h3.GetXaxis()->SetTitle("Pedestal value [ADC counts]");
h3->SetFillColor(3);

// Bank B
sprintf(title2,"ALPHA Pedestals - Bank B");   // Left end title
sprintf(name2,"Assuming 1000 count ped nom");  // final sample position dependence
h2 = new TH2F(name2, title2, 256, 0, 256,16,0,16);
h2.GetXaxis()->SetTitle("Sample number [of 256]");
h2.GetYaxis()->SetTitle("Channel Number");

sprintf(title4,"ALPHA Pedestals - Bank B");   // Left end title
sprintf(name4,"19 count RMS");  // final sample position dependence
h4 = new TH1F(name4, title4, 200,900,1100);
h4.GetXaxis()->SetTitle("Pedestal value [ADC counts]");
h4->SetFillColor(3);

TRandom2* random = new TRandom2;

// Master Simulation Settings
Int_t nBank = 2; // Banks A and B
Int_t nCh = 16; // SuperKEKB bunches
Int_t nSmpl = 256; // number of storage samples

Int_t pedVal[nBank][nCh][nSmpl] = 0;

Int_t nomPed = 1000;  // nominal pedesal offset
Float_t pedSpread = 19.0;  // nominal RMS spread
Int_t pVal = 0;  // nominal pedestal value holder 

for(Int_t iBank=0; iBank<nBank; iBank++) {
   for(Int_t iCh=0; iCh<nCh; iCh++) {
      for(Int_t iSmpl=0; iSmpl<nSmpl; iSmpl++) {
         pVal = nomPed + pedSpread*(random->Gaus(0,1)); 
         pedVal[iBank][iCh][iSmpl] = pVal;
//         printf("pVal = %d \n",pVal);
         if(iBank==0) h1->Fill(iSmpl,iCh,pVal);
         if(iBank==0) h3->Fill(pVal);
         if(iBank==1) h2->Fill(iSmpl,iCh,pVal);
         if(iBank==1) h4->Fill(pVal);
      }
   }
}

// output Pedestal values into file
// open output waveform data file
//sprintf(outputFileName,"output/%s.stuff",runNum);  
sprintf(outputFileName,"peds.dat");  
out.open(outputFileName);

for(Int_t iBank=0; iBank<nBank; iBank++) {
   for(Int_t iSmpl=0; iSmpl<nSmpl; iSmpl++) {
      out << iSmpl << " ";
      for(Int_t iCh=0; iCh<nCh; iCh++) {
         out << " " << pedVal[iBank][iCh][iSmpl];
      }
      out << endl;
   }
}

out.close();

TCanvas c1("c1","multipads",1000,600);
c1.Divide(1,1,0,0);

gStyle->SetOptStat(11); 
//gPad->SetLogy(1);
c1.cd();
h1->Draw("colz");

char plotName[100];
sprintf(plotName,"pedVals_BankA.png");
c1->Print(plotName);

TCanvas c2("c2","multipads",1000,600);
c2.Divide(1,1,0,0);

c2.cd();
h2->Draw("colz");
sprintf(plotName,"pedVals_BankB.png");
c2->Print(plotName);

gStyle->SetOptStat(1111);          
TCanvas c3("c3","multipads",1000,400);
c3.Divide(2,1,0,0);

c3.cd(1);
h3->Draw();
c3.cd(2);
h4->Draw();

sprintf(plotName,"pedDistrib.png");
c3->Print(plotName);

} // end it all for now


Float_t PMThitRate = 10;  // MHz
Float_t ASIChitRate = PMThitRate/2; // 2 IRSX/PMT
Float_t meanTimeHit = 1000.0/ASIChitRate;  // [ns]/MHz
Float_t sampleWindow = 47.1; // ns -- roughly 1 SSTin period
Float_t hitProb = (sampleWindow/meanTimeHit);
Float_t chanHitProb = hitProb/8; // for 8 channels
printf("hitProb = %f, chanHitProb = %f\n\n",hitProb,chanHitProb);

// Trigger conditions
Float_t L1rate = 30; // kHz
Float_t L1T = 1000.0/L1rate; // us
Float_t L1prob = 0.048/L1T; // L1 probability per SSTin period
Int_t L1latency = 120; // roughly 5us

printf("L1prob = %f per SSTin period with %f avg periods between\n\n",L1prob,(1/L1prob));

// Register Array
Int_t regIRSX[256][4] = 0; // [0] jump; [1] HitNum; [2] HeapJump; [3] HeapADDR

// Pointers
Int_t WRitePTR = 0;  // WRite Pointer [WR_ADDR]
Int_t HitPTR = 0;  // Hit Pointer (randomly seed)
Int_t TrigPTR = 0;  // Trig Pointer (randomly seed)
Int_t DonePTR = 0; // ADC conversion release emulator

// Processing variables
Int_t WR_ADDR = 0;
Int_t nHit = 0;
Int_t trigWin; // holder for number of windows after L1Trig to scan for hit match
Int_t trigHoldOff; // holder for number of windows holdoff after L1Trig (GDL)
Int_t iHit; // holder for hits pending in Stack
Int_t hitStack[260][2];  // hit stack
Int_t iOverflow = 0;

// Flyback management
Int_t REVmarker = 0;  
Int_t iPhase = 0;
Int_t HeapADDR = 256 - 32; // Heap Space

for(ULong64_t iBunch=0; iBunch<nRFbuckets; iBunch++) {
   if((iBunch % nBX) == 0) {
      REVmarker = 1; // flag transition
      iPhase = iBunch%24; // determine offset phase
      iOrbit = iBunch/nBX;
      if(iOrbit%1000 == 0) printf("Processing Orbit = %d\n\n",iOrbit);
   }

   if(REVmarker == 1 && ((iBunch%24)==0) ) {
      WRitePTR = 0; // 
//      printf("iBunch = %d, REVmarker zeroed\n",iBunch);
      REVmarker = 0; // reset
   }    
   if((iBunch%24)==0)) {
      // step through four phases of SSTin

      // 1) Update WR_ADDR
      WR_ADDR = 217; // scratch space default if full
      if(regIRSX[WRitePTR][0]==0) {
         WR_ADDR = WRitePTR; 
         } else {
         if(regIRSX[WRitePTR][2]==0) {
            WR_ADDR = HeapADDR;
            regIRSX[WRitePTR][3] = HeapADDR;
            HeapADDR++;
            if(HeapADDR == 256) HeapADDR -= 32; // upper address space
         }
      }

      // 2) Update Hits (from ASIC)
      HitPTR = 213;  // case WRitePTR = 0;   
      if(WRitePTR > 0) HitPTR = WRitePTR - 1; // store previous cycle
      nHit = 0;
      for(Int_t i=0; i<8; i++) {
         if(random->Uniform(0,1) < chanHitProb) nHit++;
      } 
      if(regIRSX[HitPTR][0]==0) {
         regIRSX[HitPTR][1] = nHit;
      } else {
         if(regIRSX[HitPTR][2]==0) {
            regIRSX[HitPTR][3] = nHit;
         }
      }

      // 3) Update Trigger   
      TrigPTR = WRitePTR - L1latency;
      if(TrigPTR<0) TrigPTR = 213 + WRitePTR - L1latency;
      if(TrigPTR<0) printf("ERROR TrigPTR = %d\n", TrigPTR);
      // check Trig this SSTin period
      // first if still processing L1 hit  
      if(trigWin > 0) {
         if(regIRSX[TrigPTR][0]==0) {
            if(regIRSX[TrigPTR][1] >0) {
               regIRSX[TrigPTR][0] = 1;
//               printf("Trigger Match: iBunch = %d, iOrbit = %d, TrigPTR = %d\n", iBunch, iOrbit, TrigPTR);
               iHit++;
//               printf("Trigger Match:  iOrbit = %d, TrigPTR = %d, iHit = %d\n", iOrbit, TrigPTR, iHit);
               hitStack[iHit][1] = 400;  // guess for now for conv & readout time
               hitStack[iHit][0] = TrigPTR;  // sample window onto Stack
            } else {
               if(regIRSX[TrigPTR][2]==0) {
                  if(regIRSX[regIRSX[TrigPTR][3]][1] >0) {
                     regIRSX[TrigPTR][2] = 1; 
                     iHit++;
//                     printf("Jump Trigger Match:  iOrbit = %d, TrigPTR = %d, iHit = %d\n", iOrbit, TrigPTR, iHit);
                     hitStack[iHit][1] = 400;  // guess for now for conv & readout time
                     hitStack[iHit][0] = TrigPTR + 1000;  // sample window onto Stack                     
                  }
               }
            }
         }
      } 
      // check for new trigger           
      if(random->Uniform(0,1) < L1prob & trigHoldOff < 1) {
         if(regIRSX[TrigPTR][0]>0 & regIRSX[TrigPTR][2]>0) {
            printf("Buffer Overflow!  iOrbit = %d, TrigPTR = %d, iHit = %d\n", iOrbit, TrigPTR, iHit);
            iOverflow++;
         }
         nHit = regIRSX[TrigPTR][1];
//         printf("L1 Trig: iBunch = %d, iOrbit = %d, TrigPTR = %d, nHit = %d\n", iBunch, iOrbit, TrigPTR, nHit);
         trigHoldOff = 8;  // GDL enforced min time between L1
         trigWin = 2; // additional windows to process
         if(regIRSX[TrigPTR][0]==0) {
            if(regIRSX[TrigPTR][1] >0) {
               regIRSX[TrigPTR][0] = 1;
//               printf("Trigger Match: iBunch = %d, iOrbit = %d, TrigPTR = %d\n", iBunch, iOrbit, TrigPTR);
               iHit++;
//               printf("Trigger Match:  iOrbit = %d, TrigPTR = %d, iHit = %d\n", iOrbit, TrigPTR, iHit);               
               hitStack[iHit][1] = 400;  // guess for now for conv & readout time
               hitStack[iHit][0] = TrigPTR;  // sample window onto Stack
            } else {
               if(regIRSX[TrigPTR][2]==0) {
                  if(regIRSX[regIRSX[TrigPTR][3]][1] >0) {
                     regIRSX[TrigPTR][2] = 1; 
                     iHit++;                    
//                     printf("Jump Trigger Match:  iOrbit = %d, TrigPTR = %d, iHit = %d\n", iOrbit, TrigPTR, iHit);                     
                     hitStack[iHit][1] = 400;  // guess for now for conv & readout time
                     hitStack[iHit][0] = TrigPTR + 1000;  // sample window onto Stack
//                     regIRSX[TrigPTR][2] = 0;  // for now -- need to sort out upper clear 
                  }
               }
            }
         }
      }
      if(trigWin>0) trigWin--;
      if(trigHoldOff>0) trigHoldOff--;
      
      // 4) Check if Conversion/transfer Done
      if(iHit>0) {
        if(hitStack[iHit][1]>0) hitStack[iHit][1]--; // continue Digi and Readout
        // check if the time has come
        if(hitStack[iHit][1] == 0) {

           if(hitStack[iHit][0]<500) {
              DonePTR = hitStack[iHit][0]; // clear base hit
              regIRSX[DonePTR][0] = 0; // clear base Jump
           }
           if(hitStack[iHit][0]>500) {
              DonePTR = hitStack[iHit][0] - 1000; // clear Heap hit
              regIRSX[DonePTR][2] = 0; // clear Heap Jump
           }           
           iHit--;
        }
      }
      
      WRitePTR++; // advance for next cycle
      if(iHit>0) h0->Fill(iHit);
   } // end skip over SSTin period cycle
}

printf("Simulation finished with %d seconds of realtime and %d Buffer overflows\n",nSec,iOverflow);

TCanvas c1("c1","multipads",1000,600);
c1.Divide(1,1,0,0);

gStyle->SetOptStat(111111); 
gPad->SetLogy(1);
h0->Draw();

char plotName[100];
sprintf(plotName,"sumStats_100s.png");
c1->Print(plotName);

//gPad->SetLogy(0);

} // end it all for now

Int_t NTOPMOD = 16;
Int_t NBS = 4;
Int_t NCAR = 4;
Int_t NASIC = 4;
Int_t NCHAN = 8;
Int_t NDT = 256;
Float_t dT[NTOPMOD][NBS][NCAR][NASIC][NCHAN][NDT];


char rNum[20];
char runNum[200];

sprintf(runNum,"test1000_10Hz");

/*
cout << "Input Run number:" << endl;
cin >> rNum;
*/

// Histograms definitions
char title[100],title1[100],title2[100],title3[100],title4[100];
char name[100],name1[40],name2[40],name3[40],name4[40],name5[40],name6[40],name7[40],name8[40];

sprintf(title,"PNNL DAQ: %s -- Feature Extracted",runNum);   // Left end title

sprintf(name,"All Waveform samples");  // final sample position dependence
h0 = new TH1F(name, title, 300, -300, 300);
h0.GetXaxis()->SetTitle("Sample value [ADC]");
h0.GetYaxis()->SetTitle("Entries/Pedestal Subtracted ADC value");
h0->SetFillColor(3);

if(diagNost1) {

sprintf(name1,"Frame");  // final sample position dependence
h1 = new TH1F(name1, title, 320, 0, 160);
h1.GetXaxis()->SetTitle("Frame Number");
h1.GetYaxis()->SetTitle("Entries/Frame Number");
h1->SetFillColor(1);

sprintf(name2,"HSLB");  // final sample position dependence
h2 = new TH1F(name2, title, 8, 0, 4);
h2.GetXaxis()->SetTitle("HSLB Number");
h2.GetYaxis()->SetTitle("Entries/HSLB Number");
h2->SetFillColor(2);

sprintf(name3,"Carrier");  // final sample position dependence
h3 = new TH1F(name3, title, 8, 0, 4);
h3.GetXaxis()->SetTitle("Carrier Number");
h3.GetYaxis()->SetTitle("Entries/Carrier Number");
h3->SetFillColor(3);

sprintf(name4,"IRSX");  // final sample position dependence
h4 = new TH1F(name4, title, 8, 0, 4);
h4.GetXaxis()->SetTitle("IRSX Number");
h4.GetYaxis()->SetTitle("Entries/IRSX Number");
h4->SetFillColor(4);

sprintf(name5,"convertedAddr");  // final sample position dependence
h5 = new TH1F(name5, title, 128, 0, 64);
h5.GetXaxis()->SetTitle("Starting Window Number");
h5.GetYaxis()->SetTitle("Entries/Starting Window Number");
h5->SetFillColor(5);

sprintf(name6,"channel");  // final sample position dependence
h6 = new TH1F(name6, title, 16, 0, 8);
h6.GetXaxis()->SetTitle("Channel Number");
h6.GetYaxis()->SetTitle("Entries/Channel Number");
h6->SetFillColor(6);

Int_t uTimeOffset = 1464115604;
sprintf(name7,"TT_utime");  // final sample position dependence
h7 = new TH1F(name7, title, 20, 0, 10);
h7.GetXaxis()->SetTitle("Unix Time - 1464115604 [second]");
h7.GetYaxis()->SetTitle("Hits/Unix Time second");
h7->SetFillColor(7);

sprintf(name8,"Pos Peak Time [bins] > 0");  // final sample position dependence
h8 = new TH1F(name8, title, 1024, 0, 1024);
h8.GetXaxis()->SetTitle("Sample Time [bins: should be 1..1023]");
//h6.GetYaxis()->SetTitle("Entries/Channel Number");
h8->SetFillColor(8);

}  // end Header entries diagnostics

if(diagNost2) {

char name11[40],name21[40],name31[40],name41[40],name51[40],name61[40];
Int_t pedBins = 120;
Int_t pedMin = -1200;
Int_t pedMax = 1200;
// diagnostics on pedestal
sprintf(name11,"Frame");  // final sample position dependence
h11 = new TH2D(name11, title, 320, 0, 160, pedBins, pedMin, pedMax);
h11.GetXaxis()->SetTitle("Frame Number");
h11.GetYaxis()->SetTitle("Sample Value[ADC]/Frame Number");

sprintf(name11,"Frame X");  // final sample position dependence
h11x = new TH2D(name11, title, 320, 0, 160, pedBins, pedMin, pedMax);
h11x.GetXaxis()->SetTitle("Frame Number");
h11x.GetYaxis()->SetTitle("Sample Value[ADC]/Frame Number with unphysical Window cut");

sprintf(name21,"HSLB");  // final sample position dependence
h21 = new TH2D(name21, title, 8, 0, 4, pedBins, pedMin, pedMax);
h21.GetXaxis()->SetTitle("HSLB Number");
h21.GetYaxis()->SetTitle("Sample Value[ADC]/HSLB Number");

sprintf(name21,"HSLB X");  // final sample position dependence
h21x = new TH2D(name21, title, 8, 0, 4, pedBins, pedMin, pedMax);
h21x.GetXaxis()->SetTitle("HSLB Number with unphysical Window cut");
h21x.GetYaxis()->SetTitle("Sample Value[ADC]/HSLB Number");

sprintf(name31,"Carrier");  // final sample position dependence
h31 = new TH2D(name31, title, 8, 0, 4, pedBins, pedMin, pedMax);
h31.GetXaxis()->SetTitle("Carrier Number");
h31.GetYaxis()->SetTitle("Sample Value[ADC]/Carrier Number");

sprintf(name31,"Carrier X");  // final sample position dependence
h31x = new TH2D(name31, title, 8, 0, 4, pedBins, pedMin, pedMax);
h31x.GetXaxis()->SetTitle("Carrier Number with unphysical Window cut");
h31x.GetYaxis()->SetTitle("Sample Value[ADC]/Carrier Number");

sprintf(name41,"IRSX");  // final sample position dependence
h41 = new TH2D(name41, title, 8, 0, 4, pedBins, pedMin, pedMax);
h41.GetXaxis()->SetTitle("IRSX Number");
h41.GetYaxis()->SetTitle("Sample Value[ADC]/IRSX Number");

sprintf(name41,"IRSX X");  // final sample position dependence
h41x = new TH2D(name41, title, 8, 0, 4, pedBins, pedMin, pedMax);
h41x.GetXaxis()->SetTitle("IRSX Number with unphysical Window cut");
h41x.GetYaxis()->SetTitle("Sample Value[ADC]/IRSX Number");

sprintf(name51,"convertedAddr");  // final sample position dependence
h51 = new TH2D(name51, title, 128, 0, 64, pedBins, pedMin, pedMax);
h51.GetXaxis()->SetTitle("Starting Window Number");
h51.GetYaxis()->SetTitle("Sample Value[ADC]/Starting Window Number");

sprintf(name61,"channel");  // final sample position dependence
h61 = new TH2D(name61, title, 16, 0, 8, pedBins, pedMin, pedMax);
h61.GetXaxis()->SetTitle("Channel Number");
h61.GetYaxis()->SetTitle("Sample Value[ADC]/Channel Number");

sprintf(name61,"channel X");  // final sample position dependence
h61x = new TH2D(name61, title, 16, 0, 8, pedBins, pedMin, pedMax);
h61x.GetXaxis()->SetTitle("Channel Number with unphysical Window cut");
h61x.GetYaxis()->SetTitle("Sample Value[ADC]/Channel Number");

}  // end Pedestal entries diagnostics

if(diagNost3) {

char name12[40],name22[40],name32[40],name42[40],name52[40],name62[40];
Int_t hitBins = 1024;
Int_t hitMin = 0;
Int_t hitMax = 1024;
// diagnostics on pedestal
sprintf(name12,"Hit pos vs. Frame");  // final sample position dependence
h12 = new TH2D(name12, title, 320, 0, 160, hitBins, hitMin, hitMax);
h12.GetXaxis()->SetTitle("Frame Number");
h12.GetYaxis()->SetTitle("Peak Sample pos [bins]/Frame Number");

sprintf(name22,"Hit pos vs. HSLB");  // final sample position dependence
h22 = new TH2D(name22, title, 8, 0, 4, hitBins, hitMin, hitMax);
h22.GetXaxis()->SetTitle("HSLB Number");
h22.GetYaxis()->SetTitle("Peak Sample pos [bins]/HSLB Number");

sprintf(name32,"Hit pos vs. Carrier");  // final sample position dependence
h32 = new TH2D(name32, title, 8, 0, 4, hitBins, hitMin, hitMax);
h32.GetXaxis()->SetTitle("Carrier Number");
h32.GetYaxis()->SetTitle("Peak Sample pos [bins]/Carrier Number");

sprintf(name42,"Hit pos vs. IRSX");  // final sample position dependence
h42 = new TH2D(name42, title, 8, 0, 4, hitBins, hitMin, hitMax);
h42.GetXaxis()->SetTitle("IRSX Number");
h42.GetYaxis()->SetTitle("Peak Sample pos [bins]/IRSX Number");

sprintf(name52,"Hit pos vs. convertedAddr");  // final sample position dependence
h52 = new TH2D(name52, title, 128, 0, 64, hitBins, hitMin, hitMax);
h52.GetXaxis()->SetTitle("Starting Window Number");
h52.GetYaxis()->SetTitle("Peak Sample pos [bins]/Starting Window Number");

sprintf(name62,"Hit pos vs. Channel");  // final sample position dependence
h62 = new TH2D(name62, title, 16, 0, 8, hitBins, hitMin, hitMax);
h62.GetXaxis()->SetTitle("Channel Number");
h62.GetYaxis()->SetTitle("Peak Sample pos [bins]/Channel Number");

}  // end Hit position entries diagnostics


// open data file
//sprintf(runNum,"run%s",rNum);   
//sprintf(inputFileName,"data/run%s_.raw",runNum);   
//sprintf(inputFileName,"data/run2810_fe3_pulse.txt");
sprintf(inputFileName,"%s.txt",runNum);    
in.open(inputFileName);
if(!in.good()) {
  cout << "No such filename as " << inputFileName << " !!!" << endl;
  return;
}

// open output waveform data file
sprintf(outputFileName,"output/%s.stuff",runNum);  
out.open(outputFileName);
/*
// open output feature-extracted data file
sprintf(outputFileName,"%s.pedSub_hitsChkM04Timing",runNum);  
out2.open(outputFileName);
// lean and mean output
sprintf(outputFileName,"%s.pedSub_hitsM04timing",runNum);  
out3.open(outputFileName);
*/

Int_t waveRaw[CHANNUM][WVSAMPLNUM];  // traces to process
Int_t rChk,rEvent=0;
Int_t iHit,iWindow=0;
Int_t Thresh = 50; // arming threshold for hit extraction
Int_t sOfs,wVal,w2Val,pwVal,iIRSX=0;
Int_t hitLE,hitTE,noHit,tHit,pulseWidth,goodPhot,nPEtot=0;
Int_t hitCh[8],hitCand[8][100],hitCandLE[8],hitCandTE[8],pPeak[8];
char s[200];
// CFD related
Int_t sPeak[8];
Float_t ThreshCFD,tHitCFD,tExtrap[8],tCFD[8];
// precision timing related
Int_t rOfs,rIndex,rI;
Float_t tOfs,dTv;

TCanvas c1("c1","multipads",1000,600);
c1.Divide(1,1,0,0);
char grTitle[100];
char plotName[100];
char hitTxt[10];
c1.cd();

const Int_t n = 60;
Int_t x[n], y[n];
 
gStyle->SetOptFit(1111);


// FE related

Int_t nHits = 10;  // allowed number of rising and falling -- initially 1 rising [0] and 1 falling [1]
Int_t Frame,HSLB,Carrier,IRSX,convertedAddr,channel,TT_utime;
Int_t Samp_peak[nHits],V_peak[nHits];
Int_t Samp_rise[nHits],V_rise1[nHits],V_rise2[nHits];
Int_t Samp_fall[nHits],V_fall1[nHits],V_fall2[nHits];
Int_t qualityFlag[nHits];
Int_t nSamp_peak[nHits],nV_peak[nHits];
Int_t nSamp_rise[nHits],nV_rise1[nHits],nV_rise2[nHits];
Int_t nSamp_fall[nHits],nV_fall1[nHits],nV_fall2[nHits];
Int_t nQualityFlag[nHits];
Int_t waveADC[1024];
Int_t PEtot, prevFrame = 0;
Int_t j, iWVsmpl, yTrig, iCalNum = 0;

Frame = 0;
//while (in.good() & !in.eof() & iEvt <100000)
while (iEvt <125)
//while (in.good() & !in.eof() & Frame <21)
//while (in.good() & !in.eof() & iHit < 1)
{       
  in >> s >> Frame >> HSLB >> Carrier >> IRSX >> convertedAddr >> channel >> TT_utime; // "Section 1"
  printf("Event %d\n",iEvt);
  if(diagNost1) {
     h1->Fill(Frame);
     h2->Fill(HSLB);
     h3->Fill(Carrier);
     h4->Fill(IRSX);
     h5->Fill(convertedAddr);
     h6->Fill(channel);
     h7->Fill(TT_utime-uTimeOffset);     
  } 
   
  // "Section 2" -- positive peak
  in >> Samp_peak[0] >> V_peak[0];
  in >> Samp_rise[0] >> V_rise1[0] >> V_rise2[0];
  in >> Samp_fall[0] >> V_fall1[0] >> V_fall2[0];
  in >> qualityFlag[0];
  // "Section 3" -- negative peak
  in >> nSamp_peak[0] >> nV_peak[0];
  in >> nSamp_rise[0] >> nV_rise1[0] >> nV_rise2[0];
  in >> nSamp_fall[0] >> nV_fall1[0] >> nV_fall2[0];  
  in >> nQualityFlag[0];
  if(qualityFlag[0]==1) PEtot++;
  
  if(diagNost1 & qualityFlag[0]>0) h8->Fill(Samp_peak[0]);
//  if(diagNost1 ) h8->Fill(nSamp_peak[0]);
  
  if(diagNost3 & qualityFlag[0]==1) {
     h12->Fill(Frame,nSamp_peak[0]);
     h22->Fill(HSLB,nSamp_peak[0]);
     h32->Fill(Carrier,nSamp_peak[0]);
     h42->Fill(IRSX,nSamp_peak[0]);
     h52->Fill(convertedAddr,nSamp_peak[0]);
     h62->Fill(channel,nSamp_peak[0]);   
  } 

  if(deBug1 & qualityFlag[0]==1 & Samp_peak[0]>0) {
     printf("**************** Hit Candidate  = %d  *************************\n",iEvt);
     printf("Frame = %d\n",Frame);
     printf("HSLB = %d\n",HSLB);     
     printf("Carrier = %d\n",Carrier);
     printf("IRSX = %d\n",IRSX);   
     printf("convertedAddr = %d\n",convertedAddr);     
     printf("channel = %d\n",channel);
     printf("TT_utime = %d\n\n",TT_utime);    
     
     printf("Positive peak %d\n",Samp_peak[0]);
     printf("Sample rising %d, Vr1, 2 = %d %d\n",Samp_rise[0],V_rise1[0],V_rise2[0]);       
     printf("Sample falling %d, Vf1, 2 = %d %d\n\n",Samp_fall[0],V_fall1[0],V_fall2[0]); 
     printf("Quality Flag = %d\n\n",qualityFlag[0]);
          
     printf("Negative peak %d\n",nSamp_peak[0]);         
     printf("Sample rising %d, Vr1, 2 = %d %d\n",nSamp_rise[0],nV_rise1[0],nV_rise2[0]);       
     printf("Sample falling %d, Vf1, 2 = %d %d\n",nSamp_fall[0],nV_fall1[0],nV_fall2[0]);
     printf("Quality Flag = %d\n\n",qualityFlag[0]);
     printf("**************** Hit Candidate  = %d  *************************\n\n",iEvt);  
  }        
     
  for(Int_t iCh=0; iCh<256; iCh++) {
    in >> iWVsmpl;
    if(convertedAddr<512 & qualityFlag[0]==1) h0->Fill(iWVsmpl);
    waveADC[iCh] = iWVsmpl;
    if(deBug2 & qualityFlag[0]==1 & Samp_peak[0]>0) {
       printf("sample %d = %d\n",iCh,iWVsmpl);
    }
    if(diagNost2 & qualityFlag[0]==1) {
       h11->Fill(Frame,iIRSX);
       if(convertedAddr<64) h11x->Fill(Frame,iIRSX);
       h21->Fill(HSLB,iIRSX);
       if(convertedAddr<64) h21x->Fill(HSLB,iIRSX);
       h31->Fill(Carrier,iIRSX);
       if(convertedAddr<64) h31x->Fill(Carrier,iIRSX);
       h41->Fill(IRSX,iIRSX);
       if(convertedAddr<64) h41x->Fill(IRSX,iIRSX);
       h51->Fill(convertedAddr,iIRSX);
       h61->Fill(channel,iIRSX);  
       if(convertedAddr<64) h61x->Fill(channel,iIRSX);  
    }  
  }
// plot candidate results
  if(qualityFlag[0]==1 & Samp_peak[0]> 0 & Samp_peak[0] < 1000) {
     iCalNum++;
     j = 0;
     for(Int_t i=-30; i<30; i++) {
        x[j] = Samp_peak[0]+i; 
        y[j] = 0;
        if(x[j]>-1 & x[j]<1024) y[j] = waveADC[x[j]]; 
        printf("x = %d, y = %d\n",x[j],y[j]);
        j++;
     }
     gr = new TGraph(n,x,y);
     gr->SetLineColor(2);
     gr->SetLineWidth(4);
     gr->SetMarkerColor(4);
     gr->SetMarkerStyle(21);
     sprintf(grTitle,"Hit Candidate %d of Event %d ASIC = %d Chan = %d",iCalNum,iEvt+1,IRSX,channel);
     gr->SetTitle(grTitle);
     gr->GetXaxis()->SetTitle("Sample Number");
     gr->GetYaxis()->SetTitle("ADC Value");

     gr->Draw("ACP"); 
     c1->Update();       
//     TPaveText *pt = new TPaveText(nSamp_peak[0]-3,nV_peak[0],nSamp_peak[0]+3,V_peak[0]+10,"brNDC");
     TPaveText *pt = new TPaveText(0.45,0.2,0.55,0.4,"brNDC");
//     pt->SetFillColor(18);
//     pt->SetTextAlign(12);
     pt->AddText("Pos");
     pt->AddText("Peak");
     pt->AddText("pos.");     
//     pt->AddText("nSamp_peak");
     sprintf(hitTxt,"%d",Samp_peak[0]);
     pt->AddText(hitTxt);
     pt->Draw();
     TPaveText *pt2 = new TPaveText(0.1,0.75,0.25,0.85,"brNDC2");
     sprintf(hitTxt,"V_peak %d",V_peak[0]);
     pt2->AddText(hitTxt);
     pt2->Draw();
 //    yTrig = nV_peak[0]/2;
 //    TF1 *f=new TF1("f",yTrig,nSamp_peak[0]-5,nSamp_peak[0]+5);
//     f->Draw("lc");      
 //    f->Draw();      
     
          
     sprintf(plotName,"output/DumpHits2/Hit_%d_%d_%d.png",IRSX,channel,iCalNum);
     c1->Print(plotName);
  }



     
  iEvt++;
/*
  if(prevFrame == Frame) iHit++;
  if(prevFrame != Frame) {
     iHit = 1;
     prevFrame = Frame;
//     printf("**************** End Frame = %d, hit = %d  *************************\n\n",Frame,iHit);
  }
*/

} // while loop

printf("\n Processed %d Events with %d photon candidates\n\n",iEvt,PEtot);

} // end it all for now

TCanvas c1("c1","multipads",1000,600);
c1.Divide(1,1,0,0);
char plotName[100];
c1.cd(); 
 
gStyle->SetOptFit(1111);
//h1->Draw();
h0->Fit("gaus");
sprintf(plotName,"output/Peds/%s_PedestalSubtracted.png",runNum);
c1->Print(plotName); 

gPad->SetLogy();
h0->Fit("gaus");
sprintf(plotName,"output/Peds/%s_PedestalSubtracted_logY.png",runNum);
c1->Print(plotName); 

gPad->SetLogy(0);

gStyle->SetOptStat(111111); 
 
if(diagNost1) {

h1->Draw();
sprintf(plotName,"output/header/%s_Frame.png",runNum);
c1->Print(plotName);

h2->Draw();
sprintf(plotName,"output/header/%s_HSLB.png",runNum);
c1->Print(plotName);

h3->Draw();
sprintf(plotName,"output/header/%s_Carrier.png",runNum);
c1->Print(plotName);

h4->Draw();
sprintf(plotName,"output/header/%s_IRSX.png",runNum);
c1->Print(plotName);

h5->Draw();
sprintf(plotName,"output/header/%s_convertedAddr.png",runNum);
c1->Print(plotName);

h6->Draw();
sprintf(plotName,"output/header/%s_channel.png",runNum);
c1->Print(plotName);

h7->Draw();
sprintf(plotName,"output/header/%s_TT_utime.png",runNum);
c1->Print(plotName);

gPad->SetLogy();
h8->Draw();
sprintf(plotName,"output/header/%s_PosHitTime.png",runNum);
c1->Print(plotName);
gPad->SetLogy(0);

}  // end Header diagnostic plots

if(diagNost2) {

h11->Draw("colz");
sprintf(plotName,"output/PedChecks/%s_Ped_vs_Frame.png",runNum);
c1->Print(plotName);

h11x->Draw("colz");
sprintf(plotName,"output/PedChecks/%s_Ped_vs_Frame_cutWindow.png",runNum);
c1->Print(plotName);

h21->Draw("colz");
sprintf(plotName,"output/PedChecks/%s_Ped_vs_HSLB.png",runNum);
c1->Print(plotName);

h21x->Draw("colz");
sprintf(plotName,"output/PedChecks/%s_Ped_vs_HSLB_cutWindow.png",runNum);
c1->Print(plotName);

h31->Draw("colz");
sprintf(plotName,"output/PedChecks/%s_Ped_vs_Carrier.png",runNum);
c1->Print(plotName);

h31x->Draw("colz");
sprintf(plotName,"output/PedChecks/%s_Ped_vs_Carrier_cutWindow.png",runNum);
c1->Print(plotName);

h41->Draw("colz");
sprintf(plotName,"output/PedChecks/%s_Ped_vs_IRSX.png",runNum);
c1->Print(plotName);

h41x->Draw("colz");
sprintf(plotName,"output/PedChecks/%s_Ped_vs_IRSX_cutWindow.png",runNum);
c1->Print(plotName);

h51->Draw("colz");
sprintf(plotName,"output/PedChecks/%s_Ped_vs_convertedAddr.png",runNum);
c1->Print(plotName);

h61->Draw("colz");
sprintf(plotName,"output/PedChecks/%s_Ped_vs_channel.png",runNum);
c1->Print(plotName);

h61x->Draw("colz");
sprintf(plotName,"output/PedChecks/%s_Ped_vs_channel_cutWindow.png",runNum);
c1->Print(plotName);

}  // end pedestal diagnostic plots

if(diagNost3) {

h12->Draw("colz");
sprintf(plotName,"output/HitPos/%s_HitPos_vs_Frame.png",runNum);
c1->Print(plotName);

h22->Draw("colz");
sprintf(plotName,"output/HitPos/%s_HitPos_vs_HSLB.png",runNum);
c1->Print(plotName);

h32->Draw("colz");
sprintf(plotName,"output/HitPos/%s_HitPos_vs_Carrier.png",runNum);
c1->Print(plotName);

h42->Draw("colz");
sprintf(plotName,"output/HitPos/%s_HitPos_vs_IRSX.png",runNum);
c1->Print(plotName);

h52->Draw("colz");
sprintf(plotName,"output/HitPos/%s_HitPos_vs_convertedAddr.png",runNum);
c1->Print(plotName);

h62->Draw("colz");
sprintf(plotName,"output/HitPos/%s_HitPos_vs_channel.png",runNum);
c1->Print(plotName);

}  // end pedestal diagnostic plots


out.close();
//out2.close();
//out3.close();

} // end it all for now

      if(iEvt%10==0) printf("processing Event = %d\n",iEvt);
	    // first pass to define the ROI 
	    if(iWVsmpl>20&& iWVsmpl<1000) {
	      //	      printf("goodPhot = %d\n",goodPhot);
	      hitCh[goodPhot]=iCh;
	      sPeak[goodPhot]=0;
	      pPeak[goodPhot]=0;
	      for(Int_t i=-20; i<21; i++) {
		out2 << waveRaw[iCh][iWVsmpl+i]+100*iCh << endl;
		hitCand[goodPhot][i+20]=waveRaw[iCh][iWVsmpl+i];
		if(waveRaw[iCh][iWVsmpl+i]>pPeak[goodPhot]) {
		  pPeak[goodPhot] = waveRaw[iCh][iWVsmpl+i];
		  sPeak[goodPhot] = iWVsmpl+i;
		}
	      }
	    }
	    // second pass for CFD
	    ThreshCFD = 0.5*pPeak[goodPhot];  // 50% for now
	    for(Int_t i=sPeak[goodPhot]; i>(sPeak[goodPhot]-20); i--) {
	      if(waveRaw[iCh][i-1]<ThreshCFD && waveRaw[iCh][i]>ThreshCFD) {
		     tExtrap[goodPhot] = (ThreshCFD-waveRaw[iCh][i-1])/(waveRaw[iCh][i]-waveRaw[iCh][i-1]);
             rI = i%256;
		     dTv = dT[4][rSCROD][rCar][rASIC][iCh][rI]-dT[4][rSCROD][rCar][rASIC][iCh][rI-1];
             tCFD[goodPhot] = dT[4][rSCROD][rCar][rASIC][iCh][rI-1] + tExtrap[goodPhot]*dTv;  //
//		     tCFD[goodPhot] = i-1 + tExtrap[goodPhot];  // 
	      }
	    }
	  }
	}
	// trailing edge detect
	if(pwVal>Thresh && wVal>Thresh && w2Val<Thresh) {
	  hitTE=iWVsmpl;
	  hitCandTE[goodPhot]=iWVsmpl;
	  if(iCh<7 || (goodPhot>0&&noHit==0)) {
	    out << iEvt+1 << " ";
	    out << rSCROD << " " << rCar << " " << rASIC << " " << iCh;
	    out << " " << rWin << " " << iWVsmpl << " ";
	    out << waveRaw[iCh][iWVsmpl-1] << " ";
	    out << waveRaw[iCh][iWVsmpl] << " ";
	    out << waveRaw[iCh][iWVsmpl+1] << "  -" << endl;  // Output line-by-line
	  }
	}
	if(hitLE>0 && hitTE>0 && noHit==0) {
	  noHit=1;
	  pulseWidth = hitTE-hitLE;
	  //	  if(iCh<7 && pulseWidth > 2 && pulseWidth<10) goodPhot++;
	  if(iCh<7) goodPhot++;
	  // detect Cal edge
	  if(iCh==7 && goodPhot>0 && pulseWidth > 10 && pulseWidth < 20) {
	    out << "Cal pulseWidth = " << pulseWidth << " at ";
	    out << hitLE << " sample time" << endl;
	    for(Int_t i=0; i<goodPhot; i++) {
	      if((hitCandTE[i]-hitCandLE[i])>3) {
		 out << "PhoLE = " << hitCandLE[i];
		 out << ", PhoTE = " << hitCandTE[i];
		 out << ", PhoPeak = " << pPeak[i];
		 out << ", photWidth = ";
		 out << (hitCandTE[i]-hitCandLE[i]);
		 out << ", and Time = ";
		 tHit = (hitCandLE[i]-hitCandLE[goodPhot]+500); // hit time
		 out << tHit;
		 tHitCFD = (tCFD[i]-tCFD[goodPhot]+500.0); // hit time -- revised to CFD
		 out << endl;
		 // lean output
		 if(tHit>0) {
		   nPEtot++;
		   out3 << iEvt+1 << " ";
		   out3 << rSCROD << " " << rCar << " " << rASIC;
		   out3 << " " << hitCh[i];
		   out3 << " " << rWin << " "; // general header
		   out3 << pPeak[i] << " ";
		   out3 << tHitCFD << " " << hitLE;
		   out3 << endl;
		 }
	      }
	    } 
	    out << endl;
	  }
	}
      }

    }
  }
  //  printf("rEvent = %d, iEvt = %d\n",rEvent,iEvt);
  //  if(iEvt%2== 0) printf("Processing event %d samples\n",iEvt+1);
  //  if(iIRSX%64 == 0) printf("Processed %d ASICs\n",iIRSX);
} // end while
in.close();

printf("\n Processed %d Events with %d photon candidates\n\n",iEvt,nPEtot);


out.close();
//out2.close();
//out3.close();

} // end it all for now

