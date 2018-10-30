/* Copyright (C) 2005-2017 Massachusetts Institute of Technology
%
%  This program is free software; you can redistribute it and/or modify
%  it under the terms of the GNU General Public License as published by
%  the Free Software Foundation; either version 2, or (at your option)
%  any later version.
%
%  This program is distributed in the hope that it will be useful,
%  but WITHOUT ANY WARRANTY; without even the implied warranty of
%  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
%  GNU General Public License for more details.
%
%  You should have received a copy of the GNU General Public License
%  along with this program; if not, write to the Free Software Foundation,
%  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/*
 * Flatten.cc  -- flatten hierarchical GDSII data set
 * Homer Reid   12/2017-4/2018
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <string>
#include <sstream>

#include "libGDSII.h"

using namespace std;

namespace libGDSII{

/***************************************************************/
/***************************************************************/
/***************************************************************/
typedef struct GTransform
 { double X0, Y0;
   double CosTheta, SinTheta;
   double Mag;
   bool Refl;
 } GTransform;

typedef vector<GTransform> GTVec;

static void ApplyGTransform(GTransform GT, double X, double Y, double *XP, double *YP)
{
   X *= GT.Mag;
   Y *= (GT.Refl ? -1.0 : 1.0)*GT.Mag;
  double NewX = GT.X0 + GT.CosTheta*X - GT.SinTheta*Y;
  double NewY = GT.Y0 + GT.SinTheta*X + GT.CosTheta*Y;
  *XP = NewX;
  *YP = NewY;
}

/***************************************************************/
/* data structure that keeps track of the status of the GMSH   */
/* file output process                                         */
/***************************************************************/
typedef struct StatusData
{ int CurrentLayer;
  double IJ2XY; // scale factor converting GDSII integer-value vertex indices to real-valued coordinates in the chosen length units
  EntityList EntitiesThisLayer;
  GTVec GTStack;
  int RefDepth;
} StatusData;

static void InitStatusData(StatusData *SD, double CoordinateLengthUnit, double PixelLengthUnit)
{ SD->CurrentLayer=-1;
  SD->IJ2XY = PixelLengthUnit / CoordinateLengthUnit;
  SD->RefDepth=0;
}

//FIXME 
static void GetPhysicalXY(StatusData *SD, double X, double Y, double *pXP, double *pYP)
{
  double XP=X, YP=Y;
  for(unsigned n=SD->GTStack.size(); n>0; n--)
   { ApplyGTransform(SD->GTStack[n-1],X,Y,&XP,&YP);
     X=XP;
     Y=YP;
   }
  *pXP = SD->IJ2XY * XP;
  *pYP = SD->IJ2XY * YP;
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void AddBoundary(StatusData *SD, GDSIIData *Data, int ns, int ne)
{
  GDSIIStruct *s  = Data->Structs[ns];
  GDSIIElement *e = s->Elements[ne];
  if (SD->CurrentLayer!=e->Layer) return;

  vector<int> IXY = e->XY;
  int NXY         = IXY.size() / 2;

  char Label[1000];
  snprintf(Label,1000,"Struct %s element #%i (boundary)",s->Name->c_str(),ne);

  Entity E;
  E.XY.resize(IXY.size() - 2);
  E.Text   = 0;
  E.Label  = strdup(Label);
  E.Closed = true;
  for(int n=0; n<NXY-1; n++)
   GetPhysicalXY(SD, IXY[2*n+0], IXY[2*n+1], &(E.XY[2*n]), &(E.XY[2*n+1]));

  SD->EntitiesThisLayer.push_back(E);
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void AddPath(StatusData *SD, GDSIIData *Data, int ns, int ne)
{
  GDSIIStruct *s  = Data->Structs[ns];
  GDSIIElement *e = s->Elements[ne];

  if (SD->CurrentLayer!=e->Layer) return;
  char Label[1000];
  snprintf(Label,1000,"Struct %s element #%i (path)",s->Name->c_str(),ne);

  vector<int> IXY = e->XY;
  int NXY         = IXY.size() / 2;

  double IJ2XY    = SD->IJ2XY;
  double W        = e->Width*IJ2XY;

  Entity E;
  E.Text   = 0;
  E.Label  = strdup(Label);
  E.Closed = (W!=0.0);
  int NumNodes = (W==0.0 ? NXY : 2*NXY);
  E.XY.resize(2*NumNodes); 

  if (W==0.0)
   for(int n=0; n<NXY; n++)
    GetPhysicalXY(SD, IXY[2*n+0], IXY[2*n+1], &(E.XY[2*n+0]),&(E.XY[2*n+1]));
  else
   for(int n=0; n<NXY-1; n++)
    { 
      double X1, Y1, X2, Y2;
      GetPhysicalXY(SD, IXY[2*n+0],      IXY[2*n+1],     &X1, &Y1);
      GetPhysicalXY(SD, IXY[2*(n+1)+0],  IXY[2*(n+1)+1], &X2, &Y2);

      // unit vector in width direction
      double DX = X2-X1, DY=Y2-Y1, DNorm = sqrt(DX*DX + DY*DY);
      if (DNorm==0.0) DNorm=1.0;
      double XHat = +1.0*DY / DNorm;
      double YHat = -1.0*DX / DNorm;

      E.XY[2*n+0]  = X1-0.5*W*XHat;  E.XY[2*n+1]  = Y1-0.5*W*YHat;
      int nn = 2*NXY-1-n;
      E.XY[2*nn+0] = X1+0.5*W*XHat;  E.XY[2*nn+1] = Y1+0.5*W*YHat;

      if (n==NXY-2)
       { nn=NXY-1;
         E.XY[2*nn+0] = X2-0.5*W*XHat;  E.XY[2*nn+1] = Y2-0.5*W*YHat;
         E.XY[2*nn+2] = X2+0.5*W*XHat;  E.XY[2*nn+3] = Y2+0.5*W*YHat;
       }
    }
  SD->EntitiesThisLayer.push_back(E);
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void AddText(StatusData *SD, GDSIIData *Data, int ns, int ne)
{  
  GDSIIStruct *s  = Data->Structs[ns];
  GDSIIElement *e = s->Elements[ne];
  if (SD->CurrentLayer!=e->Layer) return;

  char Label[1000];
  snprintf(Label,1000,"Struct %s element #%i (texttype %i)",s->Name->c_str(),ne,e->TextType);

  vector<int> IXY  = e->XY;
    
  double X, Y;
  GetPhysicalXY(SD, IXY[0], IXY[1], &X, &Y);

  Entity E;
  E.XY.push_back(X);
  E.XY.push_back(Y);
  E.Text   = strdup(e->Text->c_str());
  E.Label  = strdup(Label);
  E.Closed = false;
  SD->EntitiesThisLayer.push_back(E);
}

void AddStruct(StatusData *SD, GDSIIData *Data, int ns, bool ASRef=false);

void AddASRef(StatusData *SD, GDSIIData *Data, int ns, int ne)
{
  SD->RefDepth++;

  GDSIIStruct *s   = Data->Structs[ns];
  GDSIIElement *e  = s->Elements[ne];
  vector<int> IXY  = e->XY;

  int nsRef = e->nsRef;
  if ( nsRef==-1 || nsRef>=((int)(Data->Structs.size())) )
   GDSIIData::ErrExit("structure %i (%s), element %i: REF to unknown structure %s",ns,s->Name,ne,e->SName);
    
  double Mag   = (e->Type==SREF) ? e->Mag   : 1.0;
  double Angle = (e->Type==SREF) ? e->Angle : 0.0;
  bool   Refl  = (e->Type==SREF) ? e->Refl  : false;

  GTransform GT;
  GT.CosTheta=cos(Angle*M_PI/180.0);
  GT.SinTheta=sin(Angle*M_PI/180.0);
  GT.Mag=Mag;
  GT.Refl=Refl;
  SD->GTStack.push_back(GT);
  int CurrentGT = SD->GTStack.size()-1;

  int NC=1, NR=1;
  double XYCenter[2], DeltaXYC[2]={0,0}, DeltaXYR[2]={0,0};
  XYCenter[0] = (double)IXY[0];
  XYCenter[1] = (double)IXY[1];
  if (e->Type == AREF)
   { 
     NC = e->Columns;
     NR = e->Rows;
     DeltaXYC[0] = ((double)IXY[2] - XYCenter[0]) / NC;
     DeltaXYC[1] = ((double)IXY[3] - XYCenter[1]) / NC;
     DeltaXYR[0] = ((double)IXY[4] - XYCenter[0]) / NR;
     DeltaXYR[1] = ((double)IXY[5] - XYCenter[1]) / NR;
   }

  for(int nc=0; nc<NC; nc++)
   for(int nr=0; nr<NR; nr++)
    { 
      SD->GTStack[CurrentGT].X0 = XYCenter[0] + nc*DeltaXYC[0] + nr*DeltaXYR[0];
      SD->GTStack[CurrentGT].Y0 = XYCenter[1] + nc*DeltaXYC[1] + nr*DeltaXYR[1];
      AddStruct(SD, Data, nsRef, true);
    }

  SD->GTStack.pop_back();
  SD->RefDepth--;
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void AddElement(StatusData *SD, GDSIIData *Data, int ns, int ne)
{
  GDSIIStruct *s  = Data->Structs[ns];
  GDSIIElement *e = s->Elements[ne];

  switch(e->Type)
   { 
     /*--------------------------------------------------------------*/
     /*--------------------------------------------------------------*/
     /*--------------------------------------------------------------*/
     case BOUNDARY:
      AddBoundary(SD, Data, ns, ne);
      break;

     case PATH:
      AddPath(SD, Data, ns, ne);
      break;

     case SREF:
     case AREF:
      AddASRef(SD, Data, ns, ne);
      break;

     case TEXT:
      AddText(SD, Data, ns, ne);
      break;

     default:
      ; // all other element types ignored for now
   };
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void AddStruct(StatusData *SD, GDSIIData *Data, int ns, bool ASRef)
{
  GDSIIStruct *s=Data->Structs[ns];

  if (s->IsPCell) return;
  if (ASRef==false && s->IsReferenced) return;
  
  for(size_t ne=0; ne<s->Elements.size(); ne++)
   AddElement(SD, Data, ns, ne);
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void GDSIIData::Flatten(double CoordinateLengthUnit)
{
  if (CoordinateLengthUnit==0.0)
   { CoordinateLengthUnit = 1.0e-6;
     char *s=getenv("LIBGDSII_LENGTH_UNIT");
     if (s && 1==sscanf(s,"%le",&CoordinateLengthUnit))
      Log("Setting libGDSII length unit to %g meters.\n",CoordinateLengthUnit);
   }

  StatusData SD;
  InitStatusData(&SD, CoordinateLengthUnit, FileUnits[1]);
  
  for(size_t nl=0; nl<Layers.size(); nl++)
   { SD.CurrentLayer = Layers[nl];
     SD.EntitiesThisLayer.clear();
     for(size_t ns=0; ns<Structs.size(); ns++)
      AddStruct(&SD,this,ns,false);
     ETable.push_back(SD.EntitiesThisLayer);
   }
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void WriteGMSHEntity(Entity E, int Layer,
                     const char *geoFileName, FILE **pgeoFile,
                     const char *ppFileName, FILE **pppFile)
{   if ( (E.Text && !ppFileName) || (!E.Text && !geoFileName) ) return;
  
  if (E.Text)
   { FILE *ppFile = *pppFile;
     if (!ppFile)
      ppFile = *pppFile = fopen(ppFileName,"w");
     fprintf(ppFile,"View \"Layer %i %s\" {\n",Layer,E.Label);
     fprintf(ppFile,"T3 (%e,%e,%e,0) {\"%s\"};\n",E.XY[0],E.XY[1],0.0,E.Text);
     fprintf(ppFile,"};\n");
   }
  else
   { FILE *geoFile = *pgeoFile;
     if (!geoFile)
      geoFile = *pgeoFile = fopen(geoFileName,"w");
     fprintf(geoFile,"// Layer %i %s \n",Layer,E.Label);
     if (!geoFile) { fprintf(stderr,"could not open file %s (aborting)\n",geoFileName); exit(1); }

     static int NumLines=0, NumSurfaces=0, NumNodes=0;

     int Node0 = NumNodes, Line0=NumLines, NXY = E.XY.size() / 2;
    
     for(int n=0; n<NXY; n++)
      fprintf(geoFile,"Point(%i)={%e,%e,%e};\n",NumNodes++,E.XY[2*n+0],E.XY[2*n+1],0.0);
     for(int n=0; n<NXY-1; n++)
      fprintf(geoFile,"Line(%i)={%i,%i};\n",NumLines++,Node0+n,Node0+((n+1)%NXY));

     if (E.Closed)
      { fprintf(geoFile,"Line(%i)={%i,%i};\n",NumLines++,Node0+NXY-1,Node0);
        fprintf(geoFile,"Line Loop(%i)={",NumSurfaces++);
        for(int n=0; n<NXY; n++)
         fprintf(geoFile,"%i%s",Line0+n,(n==NXY-1) ? "};\n" : ",");
        fprintf(geoFile,"Plane Surface(%i)={%i};\n",NumSurfaces-1,NumSurfaces-1);
      }
     fprintf(geoFile,"\n");
   }
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void WriteGMSHFile(EntityTable ETable, iVec Layers, const char *FileBase, bool SeparateLayers)
{
  char ppFileName[100];
  snprintf(ppFileName,100,"%s.pp",FileBase);
  FILE  *ppFile = 0;

  char geoFileName[100];
  FILE  *geoFile = 0;
  if (!SeparateLayers)
   snprintf(geoFileName,100,"%s.geo",FileBase);

  for(size_t nl=0; nl<Layers.size(); nl++)
   { 
     int Layer = Layers[nl];

     if (SeparateLayers)
      snprintf(geoFileName,100,"%s.Layer%i.geo",FileBase,Layer);

     for(size_t ne=0; ne<ETable[nl].size(); ne++)
      WriteGMSHEntity(ETable[nl][ne], Layer, geoFileName, &geoFile, ppFileName, &ppFile);

     if (SeparateLayers && geoFile)
      { fclose(geoFile);
        geoFile=0;
        printf("Wrote GMSH geometry file for layer %i to %s.\n",Layer,geoFileName);
      }
   }
 
  if (geoFile)
   { fclose(geoFile);
     printf("Wrote GMSH geometry file to %s.\n",geoFileName);
   }
  if (ppFile)
   { fclose(ppFile);
     printf("Wrote GMSH post-processing file to %s.\n",ppFileName);
   }
  printf("Thank you for your support.\n");
  
}

} // namespace libGDSII
