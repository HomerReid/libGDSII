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
 * GDSII2GMSH.cc -- convert GDSII file to GMSH geometry file
 * Homer Reid       12/2017
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <string>
#include <sstream>

#include "libGDSII.h"
#include "GDSIIOptions.h"

using namespace std;
using namespace libGDSII;

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

void ApplyGTransform(GTransform GT, double X, double Y, double *XP, double *YP)
{
   X *= GT.Mag;
   Y *= (GT.Refl ? -1.0 : 1.0)*GT.Mag;
  *XP = GT.X0 + GT.CosTheta*X - GT.SinTheta*Y;
  *YP = GT.Y0 + GT.SinTheta*X + GT.CosTheta*Y;
}

/***************************************************************/
/* data structure that keeps track of the status of the GMSH   */
/* file output process                                         */
/***************************************************************/
typedef struct StatusData
{ 
  int NumNodes, NumLines, NumSurfaces, RefDepth;
  bool PPFormat;
  double UnitInMM, LayerThickness;
  string GeoFileName, PPFileName;
  FILE *GeoFile, *PPFile;
  int CurrentLayer;
  GTVec GTStack;
} StatusData;

void InitStatusData(StatusData *SD)
{
  SD->NumNodes=0;
  SD->NumLines=0;
  SD->NumSurfaces=0;
  SD->RefDepth=0;
  SD->PPFormat=false;
  SD->UnitInMM=1.0;
  SD->LayerThickness=0.0;
  SD->GeoFile = 0;
  SD->PPFile  = 0;
  SD->CurrentLayer = -1;
  SD->GTStack.clear();
}

void OpenGeoFile(StatusData *SD)
{ if (SD->GeoFile) return;
  SD->GeoFile=fopen(SD->GeoFileName.c_str(),"w");
  if (!SD->GeoFile) 
   GDSIIData::ErrExit("could not open file %s",SD->GeoFileName.c_str());
}

void OpenPPFile(StatusData *SD)
{ if (SD->PPFile) return;
  SD->PPFile=fopen(SD->PPFileName.c_str(),"w");
  if (!SD->PPFile) 
   GDSIIData::ErrExit("could not open file %s",SD->PPFileName.c_str());
}

void GetPhysicalXY(StatusData *SD, double X, double Y, double *pXP, double *pYP)
{
 double XP=X, YP=Y;
 for(unsigned n=SD->GTStack.size(); n>0; n--)
  { ApplyGTransform(SD->GTStack[n-1],X,Y,&XP,&YP);
    X=XP;
    Y=YP;
  }
 *pXP = SD->UnitInMM * XP;
 *pYP = SD->UnitInMM * YP;
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void WriteBoundary(StatusData *SD, GDSIIData *Data, int ns, int ne)
{
  GDSIIStruct *s  = Data->Structs[ns];
  GDSIIElement *e = s->Elements[ne];
  vector<int> XY  = e->XY;
  int NXY         = XY.size() / 2;

  double Z0       = e->Layer * SD->LayerThickness;
  double Unit     = SD->UnitInMM;

  if (SD->CurrentLayer!=-1 && SD->CurrentLayer!=e->Layer) return;
  OpenGeoFile(SD);

  fprintf(SD->GeoFile,"// Struct %s element #%i (boundary)\n",s->Name->c_str(),ne);

  int Node0=SD->NumNodes;
  for(int n=0; n<NXY-1; n++)
   { double X, Y;
     GetPhysicalXY(SD, XY[2*n+0], XY[2*n+1], &X, &Y);
     fprintf(SD->GeoFile,"Point(%i)={%e,%e,%e};\n",++(SD->NumNodes),X,Y,Z0);
   }
 
  int Line0=SD->NumLines;
  for(int n=0; n<NXY-2; n++)
   fprintf(SD->GeoFile,"Line(%i)={%i,%i}; \n",++(SD->NumLines),Node0+1+n,Node0+n+2);
  fprintf(SD->GeoFile,"Line(%i)={%i,%i}; \n",++(SD->NumLines),Node0+NXY-1,Node0+1);

  fprintf(SD->GeoFile,"Line Loop(%i)={",(SD->NumSurfaces)++);
  for(int n=0; n<NXY-2; n++)
   fprintf(SD->GeoFile,"%i,",Line0+n+1);
  fprintf(SD->GeoFile,"%i};\n",Line0+NXY-1);
  fprintf(SD->GeoFile,"Plane Surface(%i)={%i};\n",SD->NumSurfaces-1,SD->NumSurfaces-1);

  //fprintf(SD->GeoFile,"Extrude{ 0, 0, %e } { Surface{%i}; }\n",LayerThickness,NumSurfaces-1);
  fprintf(SD->GeoFile,"\n");
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void WritePath(StatusData *SD, GDSIIData *Data, int ns, int ne)
{
  GDSIIStruct *s  = Data->Structs[ns];
  GDSIIElement *e = s->Elements[ne];
  vector<int> XY  = e->XY;
  int NXY         = XY.size() / 2;

  double Z0       = SD->LayerThickness;
  double Unit     = SD->UnitInMM;
  double DeltaZ   = 1.0*Unit;
  double W        = e->Width*Unit;

  if (SD->CurrentLayer!=-1 && SD->CurrentLayer!=e->Layer) return;
  OpenGeoFile(SD);

  fprintf(SD->GeoFile,"// Struct %s element #%i (path)\n",s->Name->c_str(),ne);
 
  int Line0 = SD->NumLines;

  for(int n=0; n<NXY; n++)
   { 
     // coordinates of center point
     double X1, Y1; 
     GetPhysicalXY(SD, XY[2*n+0],   XY[2*n+1],   &X1, &Y1);

     if (W==0.0)
      { 
        fprintf(SD->GeoFile,"Point(%i)={%e,%e,%e}; \n",(SD->NumNodes)++,X1,Y1,Z0);
        if (n>0) fprintf(SD->GeoFile,"Line(%i)={%i,%i};\n",(SD->NumLines)++,SD->NumNodes-2,SD->NumNodes-1);
      }
     else if (n<(NXY-1))
      {  
        int np1 = (n+1)%NXY;
        double X2, Y2;
        GetPhysicalXY(SD, XY[2*np1+0], XY[2*np1+1], &X2, &Y2);

        // unit vector in width direction
        double DX = X2-X1, DY=Y2-Y1, DNorm = sqrt(DX*DX + DY*DY);
        if (DNorm==0.0) DNorm=1.0;
        double XHat = +1.0*DY / DNorm;
        double YHat = -1.0*DX / DNorm;

        fprintf(SD->GeoFile,"Point(%i)={%e,%e,%e};\n",SD->NumNodes++,X1-0.5*W*XHat,Y1-0.5*W*YHat,Z0);
        fprintf(SD->GeoFile,"Point(%i)={%e,%e,%e};\n",SD->NumNodes++,X2-0.5*W*XHat,Y2-0.5*W*YHat,Z0);
        fprintf(SD->GeoFile,"Point(%i)={%e,%e,%e};\n",SD->NumNodes++,X2+0.5*W*XHat,Y2+0.5*W*YHat,Z0);
        fprintf(SD->GeoFile,"Point(%i)={%e,%e,%e};\n",SD->NumNodes++,X1+0.5*W*XHat,Y1+0.5*W*YHat,Z0);

        fprintf(SD->GeoFile,"Line(%i)={%i,%i};\n",SD->NumLines++,SD->NumNodes-4,SD->NumNodes-3);
        fprintf(SD->GeoFile,"Line(%i)={%i,%i};\n",SD->NumLines++,SD->NumNodes-3,SD->NumNodes-2);
        fprintf(SD->GeoFile,"Line(%i)={%i,%i};\n",SD->NumLines++,SD->NumNodes-2,SD->NumNodes-1);
        fprintf(SD->GeoFile,"Line(%i)={%i,%i};\n",SD->NumLines++,SD->NumNodes-1,SD->NumNodes-4);

        if (n<(NXY-2)) continue;

        fprintf(SD->GeoFile,"Line Loop(%i)={",SD->NumSurfaces++);
        for(int m=0; m<NXY-2; m++)
         fprintf(SD->GeoFile,"%i,",Line0+m+1);
        fprintf(SD->GeoFile,"%i};\n",Line0+NXY-1);
        fprintf(SD->GeoFile,"Plane Surface(%i)={%i};\n",SD->NumSurfaces-1,SD->NumSurfaces-1);
      }
   }
}

void WriteStruct(StatusData *SD, GDSIIData *Data, int ns, bool ASRef=false);

void WriteASRef(StatusData *SD, GDSIIData *Data, int ns, int ne)
{
  SD->RefDepth++;

  GDSIIStruct *s  = Data->Structs[ns];
  GDSIIElement *e = s->Elements[ne];
  vector<int> XY  = e->XY;

  int nsRef = e->nsRef;
  if ( nsRef==-1 || nsRef>=((int)(Data->Structs.size())) )
   GDSIIData::ErrExit("structure %s, element %i: REF to unknown structure %s",s->Name,ne,e->SName);
    
  double Mag   = (e->Type==SREF) ? e->Mag   : 1.0;
  double Angle = (e->Type==SREF) ? e->Angle : 0.0;
  bool   Refl  = (e->Type==SREF) ? e->Refl  : false;

  GTransform GT;
  GT.CosTheta=cos(Angle*M_PI/180.0);
  GT.SinTheta=sin(Angle*M_PI/180.0);
  GT.Mag=Mag;
  GT.Refl=Refl;
  SD->GTStack.push_back(GT);
  GTransform *CurrentTransform = &(SD->GTStack[SD->GTStack.size()-1]);

  double Unit = SD->UnitInMM;

  int NC=1, NR=1;
  double XYCenter[2], DeltaXYC[2]={0,0}, DeltaXYR[2]={0,0};
  XYCenter[0] = (double)XY[0];
  XYCenter[1] = (double)XY[1];
  if (e->Type == AREF)
   { 
     NC = e->Columns;
     NR = e->Rows;
     DeltaXYC[0] = (XY[2] - XYCenter[0]) / NC;
     DeltaXYC[1] = (XY[3] - XYCenter[1]) / NC;
     DeltaXYR[0] = (XY[4] - XYCenter[0]) / NR;
     DeltaXYR[1] = (XY[5] - XYCenter[1]) / NR;
   }
         
  for(int nc=0; nc<NC; nc++)
   for(int nr=0; nr<NR; nr++)
    { 
      CurrentTransform->X0 = XYCenter[0] + nc*DeltaXYC[0] + nr*DeltaXYR[0];
      CurrentTransform->Y0 = XYCenter[1] + nc*DeltaXYC[1] + nr*DeltaXYR[1];
      WriteStruct(SD, Data, nsRef, true);
    }

  SD->GTStack.pop_back();
  SD->RefDepth--;
}

void WriteText(StatusData *SD, GDSIIData *Data, int ns, int ne)
{  
  GDSIIStruct *s  = Data->Structs[ns];
  GDSIIElement *e = s->Elements[ne];
  vector<int> XY  = e->XY;

  if (SD->CurrentLayer!=-1 && SD->CurrentLayer!=e->Layer) return;
    
  double X, Y;
  GetPhysicalXY(SD, e->XY[0], e->XY[1], &X, &Y);

  double Z = e->Layer * SD->LayerThickness;

  OpenPPFile(SD);
  fprintf(SD->PPFile,"View \"Layer %i, TextType %i\" {\n",e->Layer,e->TextType);
  fprintf(SD->PPFile,"T3 (%e,%e,%e,0) {\"%s\"};\n",X,Y,Z,e->Text->c_str());
  fprintf(SD->PPFile,"};\n");

  // TODO handle magnigification and reflection
  if (e->Angle!=0.0)
   { double CosTheta=cos(e->Angle*M_PI/180.0);
     double SinTheta=sin(e->Angle*M_PI/180.0);
     fprintf(SD->PPFile,"v0=PostProcessing.NbViews-1;\n");
     fprintf(SD->PPFile,"View[v0].TransformXX=%e;\n",CosTheta);
     fprintf(SD->PPFile,"View[v0].TransformXY=%e;\n",-1.0*SinTheta);
     fprintf(SD->PPFile,"View[v0].TransformYX=%e;\n",1.0*SinTheta);
     fprintf(SD->PPFile,"View[v0].TransformYY=%e;\n",CosTheta);
   }
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void WriteElement(StatusData *SD, GDSIIData *Data, int ns, int ne)
{
  GDSIIStruct *s  = Data->Structs[ns];
  GDSIIElement *e = s->Elements[ne];

  switch(e->Type)
   { 
     /*--------------------------------------------------------------*/
     /*--------------------------------------------------------------*/
     /*--------------------------------------------------------------*/
     case BOUNDARY:
      WriteBoundary(SD, Data, ns, ne);
      break;

     case PATH:
      WritePath(SD, Data, ns, ne);
      break;

     case SREF:
     case AREF:
      WriteASRef(SD, Data, ns, ne);
      break;

     case TEXT:
      WriteText(SD, Data, ns, ne);
      break;

     default:
      ; // all other element types ignored for now
   };
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void WriteStruct(StatusData *SD, GDSIIData *Data, int ns, bool ASRef)
{
  GDSIIStruct *s=Data->Structs[ns];

  if (s->IsPCell) return;
  if (ASRef==false && s->IsReferenced) return;
  
  if (SD->PPFormat)
   fprintf(SD->GeoFile,"View \"{%s}\" {\n",s->Name->c_str());
  
  for(size_t ne=0; ne<s->Elements.size(); ne++)
   WriteElement(SD, Data, ns, ne);

  if (SD->PPFormat)
   fprintf(SD->GeoFile,"};\n");
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
int main(int argc, char *argv[])
{
  /***************************************************************/
  /* parse command-line options **********************************/
  /***************************************************************/
  GDSIIOptions *Options = ProcessGDSIIOptions(argc, argv);

  GDSIIData *gdsIIData = Options->gdsIIData;
  Options->gdsIIData->WriteDescription();

  StatusData SD;
  InitStatusData(&SD);

  SD.UnitInMM = Options->UnitInMM * gdsIIData->UnitInMeters;

  if (Options->SeparateLayers)
   SD.LayerThickness=0.0;
  else
   SD.LayerThickness = SD.UnitInMM;

  string FileBase(Options->FileBase);
  SD.GeoFileName = FileBase + ".geo";
  SD.PPFileName  = FileBase + ".pp";

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  iVec LayerNames;
  EntityTable ETable = gdsIIData->Flatten(LayerNames);
  char FlatFileBase[100];
  snprintf(FlatFileBase,100,"%s.flat",FileBase.c_str());
  WriteGMSHFile(ETable, LayerNames, FlatFileBase, Options->SeparateLayers);

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  int LList[1]={-1};
  set<int> LayersToWrite = (Options->SeparateLayers ? gdsIIData->Layers : set<int>(LList,LList+1));
  for(set<int>::iterator it=LayersToWrite.begin(); it!=LayersToWrite.end(); it++)
   { int Layer = SD.CurrentLayer = *it;
     char LayerStr[20]="";
     if (Layer!=-1) snprintf(LayerStr,20,".Layer%i",Layer);
     SD.GeoFileName = FileBase + LayerStr + ".geo";
     for(size_t ns=0; ns<gdsIIData->Structs.size(); ns++)
      WriteStruct(&SD,gdsIIData,ns,false);
     if (SD.GeoFile) 
      { printf("Wrote GMSH geometry file %s.\n",SD.GeoFileName.c_str());
        fclose(SD.GeoFile);
        SD.GeoFile=0;
      }
   }
  if (SD.PPFile)
   { printf("Wrote GMSH post-processing file %s.\n",SD.PPFileName.c_str());
     fclose(SD.PPFile);
   }
  printf("Thank you for your support.\n");

}
