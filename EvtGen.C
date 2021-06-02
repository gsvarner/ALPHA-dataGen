{
//
// Module:      EvtGen.C
//
// Description:  Take Pedestals for ALPHA ASIC as input and  
//               Simulate noise contribution, by event (where rand for banks A & B)
//               
//
// Revision history:  Created 7-MAY-2021  == GSV
//                    

#include "Riostream.h"

gROOT->Reset();
gStyle->SetMarkerStyle(8);

char inputFileName[200];
ifstream in;
char outputFileName[200];
ofstream out;

// Histograms definitions
char title1[100],title2[100],title3[100],title4[100];
char name1[100],name2[100],name3[100],name4[100];

// Bank A
sprintf(title1,"ALPHA added noise - Bank A");   // Left end title
sprintf(name1,"Event 2: 4.5 ADC added noise");  // final sample position dependence
h1 = new TH2F(name1, title1, 256, 0, 256,16,0,16);
h1.GetXaxis()->SetTitle("Sample number [of 256]");
h1.GetYaxis()->SetTitle("Channel Number");

sprintf(title3,"Event 2:  ALPHA - Bank A");   // Left end title
sprintf(name3,"4.5 count noise RMS");  // final sample position dependence
h3 = new TH1F(name3, title3, 200,900,1100);
h3.GetXaxis()->SetTitle("Pedestal value [ADC counts]");
h3->SetFillColor(3);

// Bank B
sprintf(title2,"ALPHA added noise - Bank B");   // Left end title
sprintf(name2,"Event 2: 4.5 ADC added noise");  // final sample position dependence
h2 = new TH2F(name2, title2, 256, 0, 256,16,0,16);
h2.GetXaxis()->SetTitle("Sample number [of 256]");
h2.GetYaxis()->SetTitle("Channel Number");

sprintf(title4,"Event 2:  ALPHA - Bank B");   // Left end title
sprintf(name4,"4.5 count noise RMS");  // final sample position dependence
h4 = new TH1F(name4, title4, 200,900,1100);
h4.GetXaxis()->SetTitle("Pedestal value [ADC counts]");
h4->SetFillColor(3);

Int_t n = 30;
Int_t x[n], y[n];
Int_t iCt = 0;
//TGraph *gr1 = new TGraph(n,x,y);

TRandom2* random = new TRandom2;

// Master Simulation Settings
Int_t nEvents = 1000; // number of Events to process
Int_t nBank = 2; // Banks A and B
Int_t nCh = 16; // SuperKEKB bunches
Int_t nSmpl = 256; // number of storage samples

Float_t noiseSpread = 4.5;  // nominal RMS noise

Int_t pedVal[nBank][nCh][nSmpl] = 0;

// Input generated Pedestal values
sprintf(inputFileName,"peds.dat",nEvents);  
in.open(inputFileName);
Int_t dSmpl;

for(Int_t iBank=0; iBank<nBank; iBank++) {
   for(Int_t iSmpl=0; iSmpl<nSmpl; iSmpl++) {
      in >> dSmpl;
//      printf("Bank = %d, Sample # %d\n",iBank,dSmpl);
      for(Int_t iCh=0; iCh<nCh; iCh++) {
         in >> pedVal[iBank][iCh][iSmpl];
//         if(iBank == 0 & iCh == 0 & iSmpl == 44) printf("Pedestal = %d\n", pedVal[iBank][iCh][iSmpl]);
      }
//      out << endl;
   }
}

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

Float_t datVal = 0; // calculation placeholder
Int_t ddatVal[2][16][256] = 0;  // display placeholder
// Generate Events
sprintf(outputFileName,"Events_%d.dat",nEvents);  
out.open(outputFileName);
out << nEvents << endl; // events to process
for(Int_t iEvent=0; iEvent<nEvents; iEvent++) {
if(iEvent%10 == 0) printf("Event %d processing\n",iEvent+1);

// add noise 
for(Int_t iBank=0; iBank<nBank; iBank++) {
   for(Int_t iCh=0; iCh<nCh; iCh++) {
      for(Int_t iSmpl=0; iSmpl<nSmpl; iSmpl++) {
         datVal = (random->Gaus(0,noiseSpread)); 
         ddatVal[iBank][iCh][iSmpl] = pedVal[iBank][iCh][iSmpl] + datVal;
//         if(iBank == 0 & iCh == 0 & iSmpl == 44) printf("Pedestal = %d, ddatVal = %d\n", pedVal[iBank][iCh][iSmpl],ddatVal[iBank][iCh][iSmpl]);
         h[iBank][iCh][iSmpl]->Fill(ddatVal[iBank][iCh][iSmpl]);
//         printf("pVal = %d \n",pVal);
//         if(iBank==0) h1->Fill(iSmpl,iCh,ddatVal);        
         if(iEvent == 2) {
            if(iBank==0 & iCh == 0 & iSmpl > n & iSmpl < (n +31) ) {
               x[iCt] = iSmpl;
               y[iCt] = ddatVal;
               iCt++;
            }
            if(iBank==0) h1->Fill(iSmpl,iCh,datVal);
            if(iBank==0) h3->Fill(ddatVal[iBank][iCh][iSmpl]);
//         if(iBank==1) h2->Fill(iSmpl,iCh,ddatVal);
            if(iBank==1) h2->Fill(iSmpl,iCh,datVal);
            if(iBank==1) h4->Fill(ddatVal[iBank][iCh][iSmpl]);
         }
      }
   }
}
// write out event
out << iEvent+1 << endl; // event separator
for(Int_t iBank=0; iBank<nBank; iBank++) {
   for(Int_t iSmpl=0; iSmpl<nSmpl; iSmpl++) {
      out << iSmpl << " ";
      for(Int_t iCh=0; iCh<nCh; iCh++) {
         out << " " << ddatVal[iBank][iCh][iSmpl];
      }
      out << endl;
   }
}

} // end Event

out.close();

// view histos
TCanvas c1("c1","multipads",1000,600);
c1.Divide(1,1,0,0);

gStyle->SetOptStat(1111); 
//gPad->SetLogy(1);
c1.cd();
h[1][13][255]->Draw();
char plotName[100];
sprintf(plotName,"proof_1_13_255.png");
c1->Print(plotName);

} // end it all for now

//////////////////////////////////////////////////////////////////////////////////////////////////

