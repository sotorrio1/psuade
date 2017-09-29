// ************************************************************************
// Copyright (c) 2007   Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
// Written by the PSUADE team.
// All rights reserved.
//
// Please see the COPYRIGHT_and_LICENSE file for the copyright notice,
// disclaimer, contact information and the GNU Lesser General Public License.
//
// PSUADE is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License (as published by the Free Software
// Foundation) version 2.1 dated February 1999.
//
// PSUADE is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the terms and conditions of the GNU General
// Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
// ************************************************************************
// Functions for the class LegendreRegression
// AUTHOR : CHARLES TONG
// DATE   : 2010
// ************************************************************************
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "LegendreRegression.h"
#include "Main/Psuade.h"
#include "Util/sysdef.h"
#include "Util/PsuadeUtil.h"
#include "PDFLib/PDFBase.h"
#include "PDFLib/PDFNormal.h"

#define PABS(x) (((x) > 0.0) ? (x) : -(x))

extern "C" {
   void dgels_(char *, int *, int *, int *, double *A, int *LDA,
               double *B, int *LDB, double *WORK, int *LWORK, int *INFO);
   void dgesvd_(char *, char *, int *, int *, double *, int *, double *,
               double *, int *, double *, int *, double *, int *, int *);
}

// ************************************************************************
// Constructor 
// ------------------------------------------------------------------------
LegendreRegression::LegendreRegression(int nInputs,int nSamples):
                                       FuncApprox(nInputs,nSamples)
{
   int  ii;
   char line[101];
   faID_ = PSUADE_RS_REGRL;
   pOrder_ = -1;
   numPerms_ = 0;
   pcePerms_ = NULL;
   regCoeffs_ = new double[numPerms_];
   for (ii = 0; ii < numPerms_; ii++) regCoeffs_[ii] = 0.0;
   regStdevs_ = new double[numPerms_+1];
   for (ii = 0; ii < numPerms_; ii++) regStdevs_[ii] = 0.0;
   fuzzyC_ = NULL;
   normalizeFlag_ = 0;
   if (psRSExpertMode_ == 1)
   {
      printf("Normalize the input parameters to [-1, 1]? (y - yes) ");
      fgets(line, 100, stdin);
      if (line[0] == 'y') normalizeFlag_ = 1;
   }
}

// ************************************************************************
// destructor
// ------------------------------------------------------------------------
LegendreRegression::~LegendreRegression()
{
   int ii;
   if (pcePerms_ != NULL)
   {
      for (ii = 0; ii < numPerms_; ii++) 
         if (pcePerms_[ii] != NULL) delete [] pcePerms_[ii];
      delete [] pcePerms_;
   }
   if (regCoeffs_ != NULL) delete [] regCoeffs_;
   if (regStdevs_ != NULL) delete [] regStdevs_;
   if (fuzzyC_    != NULL)
   {
      for (ii = 0; ii < numPerms_; ii++)
         if (fuzzyC_[ii] != NULL) delete [] fuzzyC_[ii];
      delete [] fuzzyC_;
   }
}

// ************************************************************************
// Generate lattice data based on the input set
// ------------------------------------------------------------------------
int LegendreRegression::genNDGridData(double *X, double *Y, int *N2,
                                      double **X2, double **Y2)
{
   int     totPts, sampleID, inputID, status;
   double  *HX, *Xloc;

   status = analyze(X, Y);
   if (status != 0)
   {
      printf("LegendreRegression::genNDGridData - ERROR detected.\n");
      (*N2) = 0;
      return -1;
   }

   if ((*N2) == -999) return 0;
   if (nInputs_ > 21)
   {
      printf("LegendreRegression::genNDGridData INFO - nInputs > 21.\n");
      printf("                    No lattice points generated.\n");
      (*N2) = 0;
      return 0;
   }

   if (nInputs_ == 21 && nPtsPerDim_ >    2) nPtsPerDim_ =  2;
   if (nInputs_ == 20 && nPtsPerDim_ >    2) nPtsPerDim_ =  2;
   if (nInputs_ == 19 && nPtsPerDim_ >    2) nPtsPerDim_ =  2;
   if (nInputs_ == 18 && nPtsPerDim_ >    2) nPtsPerDim_ =  2;
   if (nInputs_ == 17 && nPtsPerDim_ >    2) nPtsPerDim_ =  2;
   if (nInputs_ == 16 && nPtsPerDim_ >    2) nPtsPerDim_ =  2;
   if (nInputs_ == 15 && nPtsPerDim_ >    2) nPtsPerDim_ =  2;
   if (nInputs_ == 14 && nPtsPerDim_ >    2) nPtsPerDim_ =  2;
   if (nInputs_ == 13 && nPtsPerDim_ >    3) nPtsPerDim_ =  3;
   if (nInputs_ == 12 && nPtsPerDim_ >    3) nPtsPerDim_ =  3;
   if (nInputs_ == 11 && nPtsPerDim_ >    3) nPtsPerDim_ =  3;
   if (nInputs_ == 10 && nPtsPerDim_ >    4) nPtsPerDim_ =  4;
   if (nInputs_ ==  9 && nPtsPerDim_ >    5) nPtsPerDim_ =  5;
   if (nInputs_ ==  8 && nPtsPerDim_ >    6) nPtsPerDim_ =  6;
   if (nInputs_ ==  7 && nPtsPerDim_ >    8) nPtsPerDim_ =  8;
   if (nInputs_ ==  6 && nPtsPerDim_ >   10) nPtsPerDim_ = 10;
   if (nInputs_ ==  5 && nPtsPerDim_ >   16) nPtsPerDim_ = 16;
   if (nInputs_ ==  4 && nPtsPerDim_ >   32) nPtsPerDim_ = 32;
   if (nInputs_ ==  3 && nPtsPerDim_ >   64) nPtsPerDim_ = 64;
   if (nInputs_ ==  2 && nPtsPerDim_ > 1024) nPtsPerDim_ = 1024;
   if (nInputs_ ==  1 && nPtsPerDim_ > 8192) nPtsPerDim_ = 8192;
   totPts = nPtsPerDim_;
   for (inputID = 1; inputID < nInputs_; inputID++)
      totPts = totPts * nPtsPerDim_;
   HX = new double[nInputs_];
   for (inputID = 0; inputID < nInputs_; inputID++)
      HX[inputID] = (upperBounds_[inputID] - lowerBounds_[inputID]) /
                    (double) (nPtsPerDim_ - 1);

   (*X2) = new double[nInputs_ * totPts];
   (*Y2) = new double[totPts];
   (*N2) = totPts;
   Xloc  = new double[nInputs_];

   for (inputID = 0; inputID < nInputs_; inputID++)
      Xloc[inputID] = lowerBounds_[inputID];

   for (sampleID = 0; sampleID < totPts; sampleID++)
   {
      for (inputID = 0; inputID < nInputs_; inputID++ )
         (*X2)[sampleID*nInputs_+inputID] = Xloc[inputID];
      for (inputID = 0; inputID < nInputs_; inputID++ )
      {
         Xloc[inputID] += HX[inputID];
         if (Xloc[inputID] < upperBounds_[inputID] ||
              PABS(Xloc[inputID] - upperBounds_[inputID]) < 1.0E-7) break;
         else Xloc[inputID] = lowerBounds_[inputID];
      }
      (*Y2)[sampleID] = evaluatePoint(&((*X2)[sampleID*nInputs_]));
   }

   delete [] Xloc;
   delete [] HX;
   return 0;
}

// ************************************************************************
// Generate 1D mesh results (set all other inputs to nominal values)
// ------------------------------------------------------------------------
int LegendreRegression::gen1DGridData(double *X, double *Y, int ind1,
                                      double *settings, int *NN, 
                                      double **XX, double **YY)
{
   int    totPts, mm, nn;
   double HX, *Xloc;

   (*NN) = -999;
   genNDGridData(X, Y, NN, NULL, NULL);

   totPts = nPtsPerDim_;
   HX = (upperBounds_[ind1] - lowerBounds_[ind1]) / (nPtsPerDim_ - 1); 

   (*NN) = totPts;
   (*XX) = new double[totPts];
   (*YY) = new double[totPts];
   Xloc  = new double[nInputs_];
   for (nn = 0; nn < nInputs_; nn++) Xloc[nn] = settings[nn]; 
    
   for (mm = 0; mm < nPtsPerDim_; mm++) 
   {
      Xloc[ind1] = HX * mm + lowerBounds_[ind1];
      (*XX)[mm] = Xloc[ind1];
      (*YY)[mm] = evaluatePoint(Xloc);
   }

   delete [] Xloc;
   return 0;
}

// ************************************************************************
// Generate 2D mesh results (set all other inputs to nominal values)
// ------------------------------------------------------------------------
int LegendreRegression::gen2DGridData(double *X, double *Y, int ind1,
                                      int ind2, double *settings, int *NN, 
                                      double **XX, double **YY)
{
   int    totPts, mm, nn, index;
   double *HX, *Xloc;

   (*NN) = -999;
   genNDGridData(X, Y, NN, NULL, NULL);

   totPts = nPtsPerDim_ * nPtsPerDim_;
   HX    = new double[2];
   HX[0] = (upperBounds_[ind1] - lowerBounds_[ind1]) / (nPtsPerDim_ - 1); 
   HX[1] = (upperBounds_[ind2] - lowerBounds_[ind2]) / (nPtsPerDim_ - 1); 

   (*NN) = totPts;
   (*XX) = new double[totPts * 2];
   (*YY) = new double[totPts];
   Xloc  = new double[nInputs_];
   for (nn = 0; nn < nInputs_; nn++) Xloc[nn] = settings[nn]; 
    
   for (mm = 0; mm < nPtsPerDim_; mm++) 
   {
      for (nn = 0; nn < nPtsPerDim_; nn++)
      {
         index = mm * nPtsPerDim_ + nn;
         Xloc[ind1] = HX[0] * mm + lowerBounds_[ind1];
         Xloc[ind2] = HX[1] * nn + lowerBounds_[ind2];
         (*XX)[index*2]   = Xloc[ind1];
         (*XX)[index*2+1] = Xloc[ind2];
         (*YY)[index] = evaluatePoint(Xloc);
      }
   }

   delete [] Xloc;
   delete [] HX;
   return 0;
}

// ************************************************************************
// Generate 3D mesh results (setting others to some nominal values) 
// ------------------------------------------------------------------------
int LegendreRegression::gen3DGridData(double *X, double *Y, int ind1,
                                      int ind2, int ind3, double *settings, 
                                      int *NN, double **XX, double **YY)
{
   int    totPts, mm, nn, pp, index;
   double *HX, *Xloc;

   (*NN) = -999;
   genNDGridData(X, Y, NN, NULL, NULL);

   totPts = nPtsPerDim_ * nPtsPerDim_ * nPtsPerDim_;
   HX    = new double[3];
   HX[0] = (upperBounds_[ind1] - lowerBounds_[ind1]) / (nPtsPerDim_ - 1); 
   HX[1] = (upperBounds_[ind2] - lowerBounds_[ind2]) / (nPtsPerDim_ - 1); 
   HX[2] = (upperBounds_[ind3] - lowerBounds_[ind3]) / (nPtsPerDim_ - 1); 

   (*NN) = totPts;
   (*XX) = new double[totPts * 3];
   (*YY) = new double[totPts];
   Xloc  = new double[nInputs_];
   for (nn = 0; nn < nInputs_; nn++) Xloc[nn] = settings[nn]; 
    
   for (mm = 0; mm < nPtsPerDim_; mm++) 
   {
      for (nn = 0; nn < nPtsPerDim_; nn++)
      {
         for (pp = 0; pp < nPtsPerDim_; pp++)
         {
            index = mm * nPtsPerDim_ * nPtsPerDim_ + nn * nPtsPerDim_ + pp;
            Xloc[ind1] = HX[0] * mm + lowerBounds_[ind1];
            Xloc[ind2] = HX[1] * nn + lowerBounds_[ind2];
            Xloc[ind3] = HX[2] * pp + lowerBounds_[ind3];
            (*XX)[index*3]   = Xloc[ind1];
            (*XX)[index*3+1] = Xloc[ind2];
            (*XX)[index*3+2] = Xloc[ind3];
            (*YY)[index] = evaluatePoint(Xloc);
         }
      }
   }

   delete [] Xloc;
   delete [] HX;
   return 0;
}

// ************************************************************************
// Generate 4D mesh results (setting others to some nominal values) 
// ------------------------------------------------------------------------
int LegendreRegression::gen4DGridData(double *X, double *Y, int ind1, 
                                      int ind2, int ind3, int ind4, 
                                      double *settings, int *NN, 
                                      double **XX, double **YY)
{
   int    totPts, mm, nn, pp, qq, index;
   double *HX, *Xloc;

   (*NN) = -999;
   genNDGridData(X, Y, NN, NULL, NULL);

   totPts = nPtsPerDim_ * nPtsPerDim_ * nPtsPerDim_ * nPtsPerDim_;
   HX    = new double[4];
   HX[0] = (upperBounds_[ind1] - lowerBounds_[ind1]) / (nPtsPerDim_ - 1); 
   HX[1] = (upperBounds_[ind2] - lowerBounds_[ind2]) / (nPtsPerDim_ - 1); 
   HX[2] = (upperBounds_[ind3] - lowerBounds_[ind3]) / (nPtsPerDim_ - 1); 
   HX[3] = (upperBounds_[ind4] - lowerBounds_[ind4]) / (nPtsPerDim_ - 1); 

   (*NN) = totPts;
   (*XX) = new double[totPts * 4];
   (*YY) = new double[totPts];
   Xloc  = new double[nInputs_];
   for (nn = 0; nn < nInputs_; nn++) Xloc[nn] = settings[nn]; 
    
   for (mm = 0; mm < nPtsPerDim_; mm++) 
   {
      for (nn = 0; nn < nPtsPerDim_; nn++)
      {
         for (pp = 0; pp < nPtsPerDim_; pp++)
         {
            for (qq = 0; qq < nPtsPerDim_; qq++)
            {
               index = mm*nPtsPerDim_*nPtsPerDim_*nPtsPerDim_ +
                       nn*nPtsPerDim_*nPtsPerDim_ + pp * nPtsPerDim_ + qq;
               Xloc[ind1] = HX[0] * mm + lowerBounds_[ind1];
               Xloc[ind2] = HX[1] * nn + lowerBounds_[ind2];
               Xloc[ind3] = HX[2] * pp + lowerBounds_[ind3];
               Xloc[ind4] = HX[3] * qq + lowerBounds_[ind4];
               (*XX)[index*4]   = Xloc[ind1];
               (*XX)[index*4+1] = Xloc[ind2];
               (*XX)[index*4+2] = Xloc[ind3];
               (*XX)[index*4+3] = Xloc[ind4];
               (*YY)[index] = evaluatePoint(Xloc);
            }
         }
      }
   }

   delete [] Xloc;
   delete [] HX;
   return 0;
}

// ************************************************************************
// Evaluate a given point
// ------------------------------------------------------------------------
double LegendreRegression::evaluatePoint(double *X)
{
   int    ii, nn;
   double Y, multiplier, **LTable, normalX;

   if (regCoeffs_ == NULL) return 0.0;
   LTable = new double*[nInputs_];
   for (ii = 0; ii < nInputs_; ii++) LTable[ii] = new double[pOrder_+1];
   Y = 0.0;
   for (nn = 0; nn < numPerms_; nn++)
   {
      for (ii = 0; ii < nInputs_; ii++)
      {
         if (normalizeFlag_ == 0)
            EvalLegendrePolynomials(X[ii], LTable[ii]);
         else
         {
            normalX = X[ii] - lowerBounds_[ii];
            normalX /= (upperBounds_[ii] - lowerBounds_[ii]);
            normalX = normalX * 2.0 - 1.0;
            EvalLegendrePolynomials(normalX, LTable[ii]);
         }
      }
      multiplier = 1.0;
      for (ii = 0; ii < nInputs_; ii++)
         multiplier *= LTable[ii][pcePerms_[nn][ii]];
      Y += regCoeffs_[nn]* multiplier;
   }
   for (ii = 0; ii < nInputs_; ii++) delete [] LTable[ii];
   delete [] LTable;
   return Y;
}

// ************************************************************************
// Evaluate a number of points
// ------------------------------------------------------------------------
double LegendreRegression::evaluatePoint(int npts, double *X, double *Y)
{
   int kk;
   for (kk = 0; kk < npts; kk++)
      Y[kk] = evaluatePoint(&X[kk*nInputs_]);
   return 0.0;
}

// ************************************************************************
// Evaluate a given point and also its standard deviation
// ------------------------------------------------------------------------
double LegendreRegression::evaluatePointFuzzy(double *X, double &std)
{
   int    iOne=1;
   double Y, stdev;
   evaluatePointFuzzy(iOne, X, &Y, &stdev);
   std = stdev;
   return Y;
}

// ************************************************************************
// Evaluate a number of points and also their standard deviations
// ------------------------------------------------------------------------
double LegendreRegression::evaluatePointFuzzy(int npts,double *X,double *Y,
                                              double *Ystd)
{
   int    mm, nn, cc, kk, nTimes=100;
   double *Ys, C1, C2, mean, stds, *uppers, *lowers;
   double *regStore;
   PDFBase **PDFPtrs;

   if (regCoeffs_ == NULL) return 0.0;

   if (fuzzyC_ == NULL)
   {
      fuzzyC_ = new double*[numPerms_];
      for (nn = 0; nn < numPerms_; nn++) fuzzyC_[nn] = new double[nTimes];
      regStore = new double[numPerms_];
      for (nn = 0; nn < numPerms_; nn++) regStore[nn] = regCoeffs_[nn];

      PDFPtrs = new PDFBase*[numPerms_+1];
      uppers = new double[numPerms_+1];
      lowers = new double[numPerms_+1];
      for (mm = 0; mm <= numPerms_; mm++) 
      {
         mean = regCoeffs_[mm];
         stds = regStdevs_[mm];
         uppers[mm] = mean + 2.0 * stds;
         lowers[mm] = mean - 2.0 * stds;
         PDFPtrs[mm] = (PDFBase *) new PDFNormal(mean, stds);
      }
      Ys = new double[nTimes*npts];

      for (cc = 0; cc < nTimes; cc++)
      {
         for (mm = 0; mm < numPerms_; mm++)
         {
            C1 = PSUADE_drand();
            PDFPtrs[numPerms_]->invCDF(1, &C1, &C2, lowers[numPerms_], 
                                     uppers[numPerms_]);
            fuzzyC_[mm][cc] = regCoeffs_[mm] = C2;
         }
         evaluatePoint(npts, X, &Ys[cc*npts]);
      }
      for (kk = 0; kk < npts; kk++)
      {
         mean = 0.0;
         for (cc = 0; cc < nTimes; cc++) mean += Ys[cc*npts+kk];
         Y[kk] /= (double) nTimes;
         stds = 0.0;
         for (cc = 0; cc < nTimes; cc++)
            stds += (Ys[cc*npts+kk] - mean) * (Ys[cc*npts+kk] - mean);
         Ystd[kk] = sqrt(stds / (nTimes - 1));
      }

      for (mm = 0; mm <= numPerms_; mm++) delete PDFPtrs[mm];
      delete [] PDFPtrs;
      delete [] uppers;
      delete [] lowers;
      for (nn = 0; nn < numPerms_; nn++) regCoeffs_[nn] = regStore[nn];
      delete [] regStore;
      delete [] Ys;
      return 0.0;
   }
   else
   {
      regStore = new double[numPerms_];
      for (nn = 0; nn < numPerms_; nn++) regStore[nn] = regCoeffs_[nn];
      Ys = new double[nTimes*npts];
      for (cc = 0; cc < nTimes; cc++)
      {
         for (mm = 0; mm < numPerms_; mm++) regCoeffs_[mm] = fuzzyC_[mm][cc];
         evaluatePoint(npts, X, &Ys[cc*npts]);
      }
      for (kk = 0; kk < npts; kk++)
      {
         mean = 0.0;
         for (cc = 0; cc < nTimes; cc++) mean += Ys[cc*npts+kk];
         Y[kk] /= (double) nTimes;
         stds = 0.0;
         for (cc = 0; cc < nTimes; cc++)
            stds += (Ys[cc*npts+kk] - mean) * (Ys[cc*npts+kk] - mean);
         Ystd[kk] = sqrt(stds / (nTimes - 1));
      }
      for (nn = 0; nn < numPerms_; nn++) regCoeffs_[nn] = regStore[nn];
      delete [] Ys;
      delete [] regStore;
      return 0.0;
   }
}

// ************************************************************************
// perform mean/variance analysis
// ------------------------------------------------------------------------
int LegendreRegression::analyze(double *X, double *Y)
{
   int    N, M, mm, nn, wlen, info, NRevised;
   double *B, *XX, SSresid, SStotal, R2, *XTX, var, *Bvar;
   double esum, ymax, *WW, *SS, *AA, *UU, *VV;
   char   jobu  = 'A', jobvt = 'A';
   char   pString[100];
   FILE   *fp;

   if (nInputs_ <= 0 || nSamples_ <= 0)
   {
      printf("LegendreRegression::analyze ERROR - invalid arguments.\n");
      exit(1);
   } 
   if (nSamples_ <= nInputs_)
   {
      printf("LegendreRegression::analyze ERROR - sample size too small.\n");
      return -1;
   }
   GenPermutations();
   
   if (outputLevel_ >= 0)
   {
      printAsterisks(0);
      printf("*                Legendre Regression Analysis\n");
      printf("* R-square gives a measure of the goodness of the model.\n");
      printf("* R-square should be close to 1 if it is a good model.\n");
      printf("* Turn on rs_expert mode to output regression matrix.\n");
      printf("* Set print level to 5 to output regression error splot.\n");
      printDashes(0);
      printf("* Turn on rs_expert mode to scale the inputs to [-1, 1].\n");
      printf("* With this, statistics such as mean, variances, and\n");
      printf("* conditional variances are readily available.\n");
      printEquals(0);
   }
   N = loadXMatrix(X, &XX);
   M = nSamples_;

   wlen = 5 * M;
   AA = new double[M*N];
   UU = new double[M*M];
   SS = new double[N];
   VV = new double[M*N];
   WW = new double[wlen];
   B  = new double[N];
   for (mm = 0; mm < M; mm++)
      for (nn = 0; nn < N; nn++)
         AA[mm+nn*M] = sqrt(weights_[mm]) * XX[mm+nn*M];

   if (psRSExpertMode_ == 1 && outputLevel_ > 0)
   {
      fp = fopen("Legendre_regression_matrix.m", "w");
      fprintf(fp, "%% the sample matrix where svd is computed\n");
      fprintf(fp, "%% the last column is the right hand side\n");
      fprintf(fp, "AA = [\n");
      for (mm = 0; mm < M; mm++)
      {
         for (nn = 0; nn < N; nn++) 
            fprintf(fp, "%16.6e ", AA[mm+nn*M]);
         fprintf(fp, "%16.6e \n",Y[mm]);
      }
      fprintf(fp, "];\n");
      fprintf(fp, "A = AA(:,1:%d);\n", N);
      fprintf(fp, "B = AA(:,%d);\n", N+1);
      fclose(fp);
      printf("Regression matrix written to Legendre_regression_matrix.m\n");
   }

   if (outputLevel_ > 3) printf("Running SVD ...\n");
   dgesvd_(&jobu, &jobvt, &M, &N, AA, &M, SS, UU, &M, VV, &N, WW,
           &wlen, &info);
   if (outputLevel_ > 3) printf("SVD completed: status = %d (should be 0).\n",info);

   if (info != 0)
   {
      printf("* LegendreRegression Info: dgesvd returns a nonzero (%d).\n",
             info);
      printf("* LegendreRegression terminates further processing.\n");
      delete [] XX;
      delete [] AA;
      delete [] UU;
      delete [] SS;
      delete [] VV;
      delete [] WW;
      delete [] B;
      return -1;
   }

   if (SS[0] == 0.0) NRevised = 0;
   else
   {
      NRevised = N;
      for (nn = 1; nn < N; nn++)
         if (SS[nn-1] > 0 && SS[nn]/SS[nn-1] < 1.0e-8) NRevised--;
   }
   if (NRevised < N)
   {
      printf("* LegendreRegression ERROR: true rank of sample = %d(need %d)\n",
             NRevised, N);
      printf("*                           Try lower order.\n");
      delete [] XX;
      delete [] AA;
      delete [] UU;
      delete [] SS;
      delete [] VV;
      delete [] WW;
      delete [] B;
      return -1;
   }
   if (psRSExpertMode_ == 1)
   {
      printf("LegendreRegression: singular values for the Vandermonde matrix\n");
      printf("The VERY small ones may cause poor numerical accuracy,\n");
      printf("but not keeping them may ruin the approximation power.\n");
      printf("So, select them judiciously.\n");
      for (nn = 0; nn < N; nn++)
         printf("Singular value %5d = %e\n", nn+1, SS[nn]);
      sprintf(pString, "How many to keep (1 - %d) ? ", N);
      NRevised = getInt(1,N,pString);
      for (nn = NRevised; nn < N; nn++) SS[nn] = 0.0;
   }
   else
   {
      NRevised = N;
      for (nn = 1; nn < N; nn++)
      {
         if (SS[nn-1] > 0 && SS[nn]/SS[nn-1] < 1.0e-8)
         {
            SS[nn] = 0.0;
            NRevised--;
         }
      }
   }
   for (mm = 0; mm < NRevised; mm++)
   {
      WW[mm] = 0.0;
      for (nn = 0; nn < M; nn++)
         WW[mm] += UU[mm*M+nn] * sqrt(weights_[nn]) * Y[nn];
   }
   for (nn = 0; nn < NRevised; nn++) WW[nn] /= SS[nn];
   for (nn = NRevised; nn < N; nn++) WW[nn] = 0.0;
   for (mm = 0; mm < N; mm++)
   {
      B[mm] = 0.0;
      for (nn = 0; nn < NRevised; nn++) B[mm] += VV[mm*N+nn] * WW[nn];
   }
   delete [] AA;
   delete [] SS;
   delete [] UU;
   delete [] VV;

   if (outputLevel_ >= 5)
   {
      fp = fopen("regression_error_file.m", "w");
      fprintf(fp, "%% This file contains errors of each data point.\n");
   }
   else fp = NULL;

   esum = ymax = 0.0;
   for (mm = 0; mm < nSamples_; mm++)
   {
      WW[mm] = 0.0;
      for (nn = 0; nn < N; nn++)
         WW[mm] = WW[mm] + XX[mm+nn*nSamples_] * B[nn];
      WW[mm] = WW[mm] - Y[mm];
      esum = esum + WW[mm] * WW[mm] * weights_[mm];
      if (fp != NULL) 
         fprintf(fp, "%6d %24.16e\n",mm+1,WW[mm]*sqrt(weights_[mm]));
      if (PABS(Y[mm]) > ymax) ymax = PABS(Y[mm]);
   }
   esum /= (double) nSamples_;
   esum = sqrt(esum);
   printf("* Regression:: LS mean error = %10.3e (max=%10.3e)\n",
          esum, ymax); 

   if (fp != NULL)
   {
      fclose(fp);
      printf("FILE regression_error_file contains data errors.\n");
   }

   computeSS(N, XX, Y, B, SSresid, SStotal);
   R2  = (SStotal - SSresid) / SStotal;
   if (nSamples_ > N) var = SSresid / (double) (nSamples_ - N);
   else               var = 0.0;

   Bvar = new double[N];

   if (outputLevel_ >= 0)
   {
      if (outputLevel_ >= 1)
      {
         computeXTX(N, XX, &XTX);
         computeCoeffVariance(N, XTX, var, Bvar);
      }
      else
      {
         XTX = NULL;
         for (mm = 0; mm < numPerms_; mm++) Bvar[mm] = 0.0;
      }
      printRC(N, B, Bvar, XX, Y);
      printf("* Regression R-square = %10.3e\n", R2);
      if (M-N-1 > 0)
         printf("* adjusted   R-square = %10.3e\n",
                1.0 - (1.0 - R2) * ((M - 1) / (M - N - 1)));
      if (outputLevel_ > 1) printSRC(X, B, SStotal);
   }
 
   if (regCoeffs_ != NULL) delete [] regCoeffs_;
   regCoeffs_ = B;
   delete [] XX;
   if (XTX != NULL) delete [] XTX;
   delete [] Bvar;
   delete [] WW;
   return 0;
}

// *************************************************************************
// load the X matrix
// -------------------------------------------------------------------------
int LegendreRegression::loadXMatrix(double *X, double **XXOut)
{
   int    M, N=0, ss, ii, nn;
   double *XX=NULL, multiplier, **LTable, normalX;

   if (normalizeFlag_ == 1)
   {
      for (ii = 0; ii < nInputs_; ii++)
      {
         multiplier = upperBounds_[ii] - lowerBounds_[ii];
         if (multiplier == 0.0)
         {
            normalizeFlag_ = 0;
            printf("Legendre INFO: inputs not normalized since bounds not set.\n");
            break;
         }
      }
   }
   M = nSamples_;
   N = numPerms_;
   XX = new double[M*N];
   LTable = new double*[nInputs_];
   for (ii = 0; ii < nInputs_; ii++) LTable[ii] = new double[pOrder_+1];
   for (ss = 0; ss < nSamples_; ss++)
   {
      for (ii = 0; ii < nInputs_; ii++)
      {
         if (normalizeFlag_ == 0) normalX = X[ss*nInputs_+ii]; 
         else
         {
            normalX = X[ss*nInputs_+ii] - lowerBounds_[ii];
            normalX /= (upperBounds_[ii] - lowerBounds_[ii]);
            normalX = normalX * 2.0 - 1.0;
         }
         EvalLegendrePolynomials(normalX, LTable[ii]);
      }
      for (nn = 0; nn < numPerms_; nn++)
      {
         multiplier = 1.0;
         for (ii = 0; ii < nInputs_; ii++)
            multiplier *= LTable[ii][pcePerms_[nn][ii]];
         XX[nSamples_*nn+ss] = multiplier;
      }
   }
   (*XXOut) = XX;
   for (ii = 0; ii < nInputs_; ii++) delete [] LTable[ii];
   delete [] LTable;
   return N;
}

// *************************************************************************
// form X^T X 
// -------------------------------------------------------------------------
int LegendreRegression::computeXTX(int N, double *X, double **XXOut)
{
   int    nn, nn2, mm;
   double *XX, coef;

   XX = new double[nSamples_*N];
   for (nn = 0; nn < N; nn++)
   {
      for (nn2 = 0; nn2 < N; nn2++)
      {
         coef = 0.0;
         for (mm = 0; mm < nSamples_; mm++)
            coef += X[nn*nSamples_+mm] * weights_[mm] * X[nn2*nSamples_+mm];
         XX[nn*N+nn2] = coef;
      }
   }
   (*XXOut) = XX;
   return 0;
}

// *************************************************************************
// compute SS (sum of squares) statistics
// -------------------------------------------------------------------------
int LegendreRegression::computeSS(int N, double *XX, double *Y,
                              double *B, double &SSresid, double &SStotal)
{
   int    nn, mm;
   double *R, ymean;
                                                                                
   SSresid = SStotal = ymean = 0.0;
   R = new double[nSamples_];
   for (mm = 0; mm < nSamples_; mm++)
   {
      R[mm] = Y[mm];
      for (nn = 0; nn < N; nn++) R[mm] -= (XX[mm+nn*nSamples_] * B[nn]);
      SSresid += R[mm] * Y[mm] * weights_[mm];
      ymean += (sqrt(weights_[mm]) * Y[mm]);
   }
   ymean /= (double) nSamples_;
   SStotal = - ymean * ymean * (double) nSamples_;
   for (mm = 0; mm < nSamples_; mm++)
      SStotal += weights_[mm] * Y[mm] * Y[mm];
   delete [] R;
   return 0;
}

// *************************************************************************
// compute coefficient variance ((diagonal of sigma^2 (X' X)^(-1))
// -------------------------------------------------------------------------
int LegendreRegression::computeCoeffVariance(int N,double *XX,double var,
                                         double *B)
{
   int    i, j, lwork, iOne=1, info;
   double *B2, *work, *XT;
   char   trans[1];

   (*trans) = 'N';
   B2 = new double[N];
   XT = new double[N*N];
   lwork = 2 * N * N;
   work  = new double[lwork];
   for (i = 0; i < N; i++)
   {
      for (j = 0; j < N*N; j++) XT[j] = XX[j];
      for (j = 0; j < N; j++) B2[j] = 0.0;
      B2[i] = var;
      dgels_(trans, &N, &N, &iOne, XT, &N, B2, &N, work, &lwork, &info);
      if (info != 0)
         printf("LegendreRegression WARNING: dgels returns error %d.\n",info);
      if (B2[i] < 0) B[i] = -sqrt(-B2[i]);
      else           B[i] = sqrt(B2[i]);
   }
   delete [] B2;
   delete [] work;
   delete [] XT;
   return info;
}

// *************************************************************************
// print statistics
// -------------------------------------------------------------------------
int LegendreRegression::printRC(int N,double *B,double *Bvar,double *XX,
                                double *Y)
{
   int    ii, jj, kk, maxTerms, flag;
   double coef, ddata, variance;
   FILE   *fp;

   maxTerms = 0;
   for (ii = 0; ii < numPerms_; ii++) 
      if (pcePerms_[ii][0] > maxTerms) maxTerms = pcePerms_[ii][0];

   printDashes(0);
   printf("*      ");
   for (ii = 0; ii < nInputs_; ii++) printf("     ");
   printf("           coefficient  std. error  t-value\n");

   for (ii = 0; ii < numPerms_; ii++)
   {
      if (PABS(Bvar[ii]) < 1.0e-15) coef = 0.0;
      else                          coef = B[ii] / Bvar[ii]; 
      {
         printf("* Input orders: ");
         for (jj = 0; jj < nInputs_; jj++)
            printf(" %3d ", pcePerms_[ii][jj]);
         printf("= %11.3e %11.3e %11.3e\n", B[ii], Bvar[ii], coef);
      }
   }
   printDashes(0);
   flag = 1;
   for (ii = 0; ii < nInputs_; ii++)
      if (upperBounds_[ii] != 1.0) flag = 0;
   for (ii = 0; ii < nInputs_; ii++)
      if (lowerBounds_[ii] != -1.0) flag = 0;
   if (normalizeFlag_ == 1 || flag == 1)
   {
      printf("Mean     = %12.4e\n", B[0]);
      coef = 0.0;
      for (jj = 1; jj < numPerms_; jj++) 
      {
         ddata = B[jj];
         for (kk = 0; kk < nInputs_; kk++)
            ddata /= sqrt(1.0+pcePerms_[jj][kk]*2); 
         coef = coef + ddata * ddata;
      }
      printf("variance = %12.4e\n", coef);
      variance = coef;
      fp = fopen("matlablegendre.m", "w");
      fwritePlotCLF(fp);
      fprintf(fp, "A = [\n");
      for (ii = 0; ii < nInputs_; ii++)
      {
         coef = 0.0;
         for (jj = 1; jj < numPerms_; jj++)
         {
            flag = 1;
            for (kk = 0; kk < nInputs_; kk++)
               if (kk != ii && pcePerms_[jj][kk] != 0) flag = 0;
            if (flag == 1)
            {
               ddata = B[jj];
               for (kk = 0; kk < nInputs_; kk++)
                  ddata /= sqrt(1.0+pcePerms_[jj][kk]*2); 
               coef = coef + ddata * ddata;
            }
         }
         fprintf(fp, "%e\n", coef/variance);
         printf("Conditional variance %4d = %12.4e\n", ii+1, coef);
      }
      fprintf(fp, "];\n");
      fprintf(fp, "bar(A, 0.8);\n");
      fwritePlotAxes(fp);
      fwritePlotTitle(fp, "Legendre VCE Rankings");
      fwritePlotXLabel(fp, "Input parameters");
      fwritePlotYLabel(fp, "Rank Metric");
      fclose(fp);
      printf("Legendre VCE ranking is now in matlablegendre.m.\n");
   }
   printDashes(0);
   return 0;
}

// *************************************************************************
// print standardized regression coefficients
// -------------------------------------------------------------------------
int LegendreRegression::printSRC(double *X, double *B, double SStotal)
{
   int    nn, mm, ii;
   double denom, xmean, coef, Bmax, coef1, *B2;

   printEquals(0);
   printf("* Standardized Regression Coefficients (SRC)\n");
   printf("* When R-square is acceptable (order assumption holds), the\n");
   printf("* absolute values of SRCs provide variable importance.\n"); 
   printDashes(0);
   printf("* based on nSamples = %d\n", nSamples_);

   B2 = new double[nSamples_];
   denom = sqrt(SStotal / (double) (nSamples_ - 1));
   Bmax  = 0.0;
   for (nn = 0; nn < numPerms_; nn++)
   {
      coef = 1.0;
      for (ii = 0; ii < nInputs_; ii++)
      {
         xmean = 0.0;
         for (mm = 0; mm < nSamples_; mm++) xmean += X[mm*nInputs_+ii];
         xmean /= (double) nSamples_;
         coef1 = 0.0;
         for (mm = 0; mm < nSamples_; mm++)
            coef1 += (X[mm*nInputs_+ii]-xmean)*(X[mm*nInputs_+ii]-xmean);
         coef1 = sqrt(coef1 / (double) (nSamples_ - 1));
         coef *= coef1;
      }
      B2[nn] = B[nn] * coef / denom;
      if (PABS(B2[nn]) > Bmax) Bmax = PABS(B2[nn]);
   }
   for (nn = 0; nn < numPerms_; nn++)
   {
      if (PABS(B2[nn]) > 1.0e-12 * Bmax)
      {
         printf("* Input orders: ");
         for (ii = 0; ii < nInputs_; ii++)
            printf(" %2d ",pcePerms_[nn][ii]);
         printf("= %12.4e \n", B2[nn]);
      }
   }
   delete [] B2;
   printAsterisks(0);
   return 0;
}

// *************************************************************************
// generate all combinations of a multivariate Legendre expansion
// This code is a direct translation from Burkardt's matlab code)
// -------------------------------------------------------------------------
int LegendreRegression::GenPermutations()
{
   int  ii, kk, orderTmp, rvTmp;
   char pString[500];

   if (pOrder_ <= 0)
   {
      numPerms_ = 0;
      pOrder_ = 0;
      while (numPerms_ < nSamples_)
      { 
         pOrder_++;
         numPerms_ = 1;
         if (nInputs_ < pOrder_)
         {
            for (ii = nInputs_+pOrder_; ii > nInputs_; ii--)
               numPerms_ = numPerms_ * ii / (nInputs_+pOrder_-ii+1);
         }
         else
         {
            for (ii = nInputs_+pOrder_; ii > pOrder_; ii--)
               numPerms_ = numPerms_ * ii / (nInputs_+pOrder_-ii+1);
         }
      }
      if (numPerms_ > nSamples_) pOrder_--;
      printf("Legendre polynomial maximum order = %d\n", pOrder_);
      sprintf(pString, "Desired order (>=1 and <= %d) ? ", pOrder_);
      pOrder_ = getInt(1, pOrder_, pString);
   }
   numPerms_ = 1;
   if (nInputs_ < pOrder_)
   {
      for (ii = nInputs_+pOrder_; ii > nInputs_; ii--)
         numPerms_ = numPerms_ * ii / (nInputs_+pOrder_-ii+1);
   }
   else
   {
      for (ii = nInputs_+pOrder_; ii > pOrder_; ii--)
         numPerms_ = numPerms_ * ii / (nInputs_+pOrder_-ii+1);
   }
   printf("LegendreRegression: order of polynomials = %d\n", pOrder_);
   printf("LegendreRegression: number of permutations = %d\n",numPerms_);
   
   pcePerms_ = new int*[numPerms_];
   for (ii = 0; ii < numPerms_; ii++) pcePerms_[ii] = new int[nInputs_];

   numPerms_ = 0;
   for (kk = 0; kk <= pOrder_; kk++)
   {
      orderTmp = kk;
      rvTmp = 0;
      pcePerms_[numPerms_][0] = orderTmp;
      for (ii = 1; ii < nInputs_; ii++) pcePerms_[numPerms_][ii] = 0;
      while (pcePerms_[numPerms_][nInputs_-1] != kk)
      {
         numPerms_++;
         for (ii = 0; ii < nInputs_; ii++)
            pcePerms_[numPerms_][ii] = pcePerms_[numPerms_-1][ii];
         if (orderTmp > 1) rvTmp = 1;
         else              rvTmp++;
         pcePerms_[numPerms_][rvTmp-1] = 0;
         orderTmp = pcePerms_[numPerms_-1][rvTmp-1];
         pcePerms_[numPerms_][0] = orderTmp - 1;
         pcePerms_[numPerms_][rvTmp] = pcePerms_[numPerms_-1][rvTmp] + 1;
      }
      numPerms_++;
   }
   return 0;
}

// *************************************************************************
// Purpose: evaluate 1D Legendre polynomials (normalized)
// -------------------------------------------------------------------------
int LegendreRegression::EvalLegendrePolynomials(double X, double *LTable)
{
   int    ii;
   LTable[0] = 1.0;
   if (pOrder_ >= 1)
   {
      LTable[1] = X;
      for (ii = 2; ii <= pOrder_; ii++)
         LTable[ii] = ((2 * ii - 1) * X * LTable[ii-1] -
                       (ii - 1) * LTable[ii-2]) / ii;
   }
   return 0;
}

// ************************************************************************
// set parameters
// ------------------------------------------------------------------------
double LegendreRegression::setParams(int targc, char **targv)
{
   pOrder_ = *(int *) targv[0];
   if (pOrder_ <= 0)
   {
      pOrder_ = -1;
      printf("LegendreRegression setParams: pOrder not valid.\n");
   }
   else printf("LegendreRegression setParams: pOrder set to %d.\n", pOrder_);
   return 0.0;
}
