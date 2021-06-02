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


