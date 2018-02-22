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

using namespace std;
using namespace libGDSII;

/***************************************************************/
/* global variables ********************************************/
/***************************************************************/
int NumNodes=0;
int NumLines=0;
int NumSurfaces=0;
int RefDepth=0;
bool PPFormat=false;
double Unit=1.0;
double LayerThickness=0.0;

/***************************************************************/
/***************************************************************/
/***************************************************************/
void ErrExit(const char *format, ...)
{
  va_list ap; 
  char buffer[1000];

  va_start(ap,format);
  vsnprintf(buffer,1000,format,ap);
  va_end(ap);

  fprintf(stderr,"error: %s (aborting)\n",buffer);

  exit(1);
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void WriteBoundary(FILE *f, GDSIIData *Data, int ns, int ne, int Offset[2])
{
  GDSIIStruct *s  = Data->Structs[ns];
  GDSIIElement *e = s->Elements[ne];
  vector<int> XY  = e->XY;
  int NXY         = XY.size() / 2;

  double Z0       = e->Layer * LayerThickness;

  fprintf(f,"// Struct %s element #%i (boundary)\n",s->Name->c_str(),ne);

  int Node0=NumNodes;
  for(int n=0; n<NXY-1; n++)
   { double X = Unit * (XY[2*n+0] + Offset[0]);
     double Y = Unit * (XY[2*n+1] + Offset[1]);
     fprintf(f,"Point(%i)={%e,%e,%e};\n",++NumNodes,X,Y,Z0);
   };
 
  int Line0=NumLines;
  for(int n=0; n<NXY-2; n++)
   fprintf(f,"Line(%i)={%i,%i}; \n",++NumLines,Node0+1+n,Node0+n+2);
  fprintf(f,"Line(%i)={%i,%i}; \n",++NumLines,Node0+NXY-1,Node0+1);

  fprintf(f,"Line Loop(%i)={",NumSurfaces++);
  for(int n=0; n<NXY-2; n++)
   fprintf(f,"%i,",Line0+n+1);
  fprintf(f,"%i};\n",Line0+NXY-1);
  fprintf(f,"Plane Surface(%i)={%i};\n",NumSurfaces-1,NumSurfaces-1);

  //fprintf(f,"Extrude{ 0, 0, %e } { Surface{%i}; }\n",LayerThickness,NumSurfaces-1);
  fprintf(f,"\n");
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void WritePath(FILE *f, GDSIIData *Data, int ns, int ne, int Offset[2])
{
  GDSIIStruct *s  = Data->Structs[ns];
  GDSIIElement *e = s->Elements[ne];
  vector<int> XY  = e->XY;
  int NXY         = XY.size() / 2;

  double Z0       = LayerThickness;
  double DeltaZ   = 1.0*Unit;
  double W        = e->Width*Unit;

  fprintf(f,"// Struct %s element #%i (path)\n",s->Name->c_str(),ne);

  for(int n=0; n<NXY; n++)
   { 
     // coordinates of center point
     int np1 = (n+1)%NXY;
     double X1 = Unit * (XY[2*n+0] + Offset[0]);
     double Y1 = Unit * (XY[2*n+1] + Offset[1]);
     double X2 = Unit * (XY[2*np1+0] + Offset[0]);
     double Y2 = Unit * (XY[2*np1+1] + Offset[1]);

     if (W==0.0)
      { fprintf(f,"Point(%i)={%e,%e,%e}; \n",NumNodes++,X1,Y1,Z0);
        fprintf(f,"Line(%i)={%i,%i}; \n",NumLines++,NumNodes-1,(n==NXY-1) ? NumNodes-NXY : NumNodes);
        //fprintf(f,"Extrude{0,0,%e} { Line(%i); } \n",LayerThickness,NumLines-1);
      }
     else
      { 
        // unit vector in width direction
        double DX = X2-X1, DY=Y2-Y1, DNorm = sqrt(DX*DX + DY*DY);
        if (DNorm==0.0) DNorm=1.0;
        double XHat = +1.0*DY / DNorm;
        double YHat = -1.0*DX / DNorm;

        fprintf(f,"Point(%i)={%e,%e,%e};\n",NumNodes++,X1-W*XHat,Y1-W*YHat,Z0);
        fprintf(f,"Point(%i)={%e,%e,%e};\n",NumNodes++,X2-W*XHat,Y1-W*YHat,Z0);
        fprintf(f,"Point(%i)={%e,%e,%e};\n",NumNodes++,X2+W*XHat,Y1-W*YHat,Z0);
        fprintf(f,"Point(%i)={%e,%e,%e};\n",NumNodes++,X2+W*XHat,Y2+W*YHat,Z0);

        fprintf(f,"Line(%i)={%i,%i};\n",NumLines++,NumNodes-3,NumNodes-2);
        fprintf(f,"Line(%i)={%i,%i};\n",NumLines++,NumNodes-2,NumNodes-1);
        fprintf(f,"Line(%i)={%i,%i};\n",NumLines++,NumNodes-1,NumNodes-0);
        fprintf(f,"Line(%i)={%i,%i};\n",NumLines++,NumNodes-0,NumNodes-3);

        fprintf(f,"Plane Surface(%i)={%i,%i,%i,%i};\n",NumSurfaces++,NumLines-3,NumLines-2,NumLines-1,NumLines);
        //fprintf(f,"Extrude{0,0,%e} { Surface(%i); } \n",LayerThickness,NumSurfaces-1);
      };

   };
}

void WriteStruct(FILE *f, GDSIIData *Data, int ns, int Offset[2]);

void WriteASRef(FILE *f, GDSIIData *Data, int ns, int ne, int Offset[2])
{
  RefDepth++;

  GDSIIStruct *s  = Data->Structs[ns];
  GDSIIElement *e = s->Elements[ne];
  vector<int> XY  = e->XY;

  int nsRef = e->nsRef;
  if ( nsRef==-1 || nsRef>=((int)(Data->Structs.size())) )
   ErrExit("structure %s, element %i: REF to unknown structure %s",s->Name,ne,e->SName);

  int XY0[2], DeltaXYC[2]={0,0}, DeltaXYR[2]={0,0}, NC=1, NR=1;
  XY0[0]      = XY[0];
  XY0[1]      = XY[1];
  if (e->Type == AREF)
   { 
     NC = e->Columns;
     NR = e->Rows;
     DeltaXYC[0] = (XY[2] - XY0[0]) / NC;
     DeltaXYC[1] = (XY[3] - XY0[1]) / NC;
     DeltaXYR[0] = (XY[4] - XY0[0]) / NR;
     DeltaXYR[1] = (XY[5] - XY0[1]) / NR;
   };
         
  for(int nc=0; nc<NC; nc++)
   for(int nr=0; nr<NR; nr++)
    { 
      int ArrayOffset[2];
      ArrayOffset[0] = Offset[0] + XY0[0] + nc*DeltaXYC[0] + nr*DeltaXYR[0];
      ArrayOffset[1] = Offset[1] + XY0[1] + nc*DeltaXYC[1] + nr*DeltaXYR[1];
      WriteStruct(f, Data, nsRef, ArrayOffset);
    };

  RefDepth--;
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void WriteElement(FILE *f, GDSIIData *Data, int ns, int ne, int Offset[2])
{
  GDSIIStruct *s  = Data->Structs[ns];
  GDSIIElement *e = s->Elements[ne];

  switch(e->Type)
   { 
     /*--------------------------------------------------------------*/
     /*--------------------------------------------------------------*/
     /*--------------------------------------------------------------*/
     case BOUNDARY:
      WriteBoundary(f, Data, ns, ne, Offset);
      break;

     case PATH:
      WritePath(f, Data, ns, ne, Offset);
      break;

     case SREF:
     case AREF:
      WriteASRef(f, Data, ns, ne, Offset);
      break;

     //case TEXT:
     // WriteText(f, Data, ns, ne, Offset);
     // break;

     default:
      ; // all other element types ignored for now
   };
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void WriteStruct(FILE *f, GDSIIData *Data, int ns, int Offset[2])
{
  GDSIIStruct *s=Data->Structs[ns];

  // if we are at RefDepth=0 and the structure is referenced by an
  // SREF or AREF, then we don't write it.
  if (RefDepth==0 && s->IsReferenced)
   return;
  
  if (PPFormat)
   fprintf(f,"View \"{%s}\" {\n",s->Name->c_str());
  
  for(size_t ne=0; ne<s->Elements.size(); ne++)
   WriteElement(f, Data, ns, ne, Offset);

  if (PPFormat)
   fprintf(f,"};\n");
}

void WriteStruct(FILE *f, GDSIIData *Data, int ns)
{ int Offset[2]={0,0};
  WriteStruct(f, Data, ns, Offset);
}

void Usage(string ErrorMessage)
{
  printf("\n");
  if (ErrorMessage.size()!=0)
   printf("error: %s (aborting)\n",ErrorMessage.c_str());
  printf("usage: GDSII2GMSH File.gds [options]\n\n");
  printf("options:\n\n");
  printf(" --outfile xx    (output file name)\n\n");
  exit(1);
}

// given "/home/homer/MyFile.txt," return "MyFile"
string GetFileBase(string FileName)
{
  size_t Start = FileName.rfind("/");
  if (Start==string::npos)
   Start=0;
  else
   Start++;
  size_t End = FileName.rfind(".");
  if (End==string::npos)
   End=FileName.size();
  return FileName.substr(Start, End-Start);
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
int main(int argc, char *argv[])
{
  /***************************************************************/
  /* parse command-line options **********************************/
  /***************************************************************/
  if ( argc<2 )
   Usage("");
  string GDSIIFile = string(argv[1]);
  string GeoFile;
  bool Absolute=false;
  for(int narg=2; narg<argc; narg++)
   { 
     if (!strcasecmp(argv[narg],"--outfile"))
      GeoFile=string(argv[++narg]);
     else if (!strcasecmp(argv[narg],"--LayerThickness"))
      sscanf(argv[++narg],"%le",&LayerThickness);
     else if (!strcasecmp(argv[narg],"--absolute"))
      Absolute=true;
     else
      Usage( string("unknown argument ") + argv[narg] );
   };

  /***************************************************************/
  /* attempt to read GDSII file  *********************************/
  /***************************************************************/
  GDSIIData *gdsIIData = new GDSIIData(GDSIIFile);
  if (gdsIIData->ErrMsg)
   { printf("error: %s (aborting)\n",gdsIIData->ErrMsg->c_str());
     exit(1);
   };

  if (Absolute)
   Unit = gdsIIData->Unit;

  if (LayerThickness==0.0)
   LayerThickness = Unit;
  
  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  if (GeoFile.empty())
   GeoFile = GetFileBase(GDSIIFile) + ".geo";
  FILE *f=fopen(GeoFile.c_str(),"w");
  for(size_t ns=0; ns<gdsIIData->Structs.size(); ns++)
   WriteStruct(f,gdsIIData,ns);
  fclose(f);

  printf("GMSH geometry file written to %s.\n",GeoFile.c_str());
  printf("Thank you for your support.\n");

}
