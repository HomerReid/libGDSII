/* Copyright (C) 2005-2017 Massachusetts Institute of Technology
%
%  This program is free software; you can redistribute it and/or modify
%  it under the terms of the GNU General Public License as published by
%  the Free Software Foundation; either version 2, or (at your option)
%  any later version.
%
%  This program is distributed in the hope that it will be useful,
%  but WITHOUT ANY WARRANTY; without even the implied warranty of
%  MERCHANTABILITY or FITNESS FOR A PARTICULAh  PURPOSE.  See the
%  GNU General Public License for more details.
%
%  You should have received a copy of the GNU General Public License
%  along with this program; if not, write to the Free Software Foundation,
%  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/*
 * libGDSII.cc -- main source file for libGDSII
 * Homer Reid   11/2017
 */

#include "libGDSII.h"

#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

using namespace std;
namespace libGDSII {

/***************************************************************/
/* GDSIIData constructor: create a new GDSIIData instance from */
/* a binary GDSII file.                                        */
/***************************************************************/
GDSIIData::GDSIIData(const string FileName)
{ 
  // initialize class data
  LibName       = 0;
  UnitInMeters  = 1.0e-6;
  FileUnits[0]  = FileUnits[1]=0.0;
  GDSIIFileName = new string(FileName);

  ReadGDSIIFile(FileName);

  // at this point ErrMsg is non-null if an error occurred
  if (ErrMsg) return;
}

GDSIIData::~GDSIIData()
{
  if (GDSIIFileName) delete GDSIIFileName;
  if (ErrMsg) delete ErrMsg;
  for(size_t ns=0; ns<Structs.size(); ns++)
   { for(size_t ne=0; ne<Structs[ns]->Elements.size(); ne++)
      { if (Structs[ns]->Elements[ne]->SName) delete Structs[ns]->Elements[ne]->SName;
        if (Structs[ns]->Elements[ne]->Text)  delete Structs[ns]->Elements[ne]->Text;
        delete Structs[ns]->Elements[ne];
      }
     if (Structs[ns]->Name) delete Structs[ns]->Name;
     delete Structs[ns];
   }

  for(size_t nl=0; nl<ETable.size(); nl++)
   for(size_t ne=0; ne<ETable[nl].size(); ne++)
    { if (ETable[nl][ne].Text) free(ETable[nl][ne].Text);
      if (ETable[nl][ne].Label) free(ETable[nl][ne].Label);
    }
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
int GDSIIData::GetStructByName(string Name)
{ for(size_t ns=0; ns<Structs.size(); ns++)
   if ( Name == *(Structs[ns]->Name) )
    return ns;
  return -1;
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
iVec GDSIIData::GetLayers()
{ return Layers; }

PolygonList GDSIIData::GetPolygons(const char *Text, int Layer)
{
  PolygonList Polygons;
  
  // first pass to find text strings matching Text, if it is non-NULL
  int TextLayer=-1;
  double TextXY[2]={HUGE_VAL, HUGE_VAL};
  if (Text)
   { for(size_t nl=0; nl<Layers.size() && TextLayer==-1; nl++)
      { if (Layer!=-1 && Layers[nl]!=Layer) continue;
        for(size_t ne=0; ne<ETable[nl].size() && TextLayer==-1; ne++)
         if ( ETable[nl][ne].Text && !strcmp(ETable[nl][ne].Text,Text) )
          { TextLayer  = Layers[nl];
            TextXY[0]  = ETable[nl][ne].XY[0];
            TextXY[1]  = ETable[nl][ne].XY[1];
          }
      }
     if (TextLayer==-1) return Polygons; // text string not found, return empty list
   }

  if (TextLayer!=-1) Layer=TextLayer;

  // second pass to find matching polygons
  for(size_t nl=0; nl<Layers.size(); nl++)
   { if (Layer!=-1 && Layers[nl]!=Layer) continue;
     for(size_t ne=0; ne<ETable[nl].size(); ne++)
      { if (ETable[nl][ne].Text!=0) continue; // we want only polygons here
        if (TextLayer==-1 || PointInPolygon(ETable[nl][ne].XY, TextXY[0], TextXY[1]))
         Polygons.push_back(ETable[nl][ne].XY);
      }
   }
  return Polygons;
}

PolygonList GDSIIData::GetPolygons(int Layer) 
 { return GetPolygons(0,Layer); }

/***************************************************************/
/* the next few routines implement a mechanism by which an API */
/* code can make multiple calls to GetPolygons() for a given   */
/* GDSII file without requiring the API code to keep track of  */
/* an instance of GDSIIData, but also without re-reading the   */
/* file each time;                                             */
/***************************************************************/
static GDSIIData *CachedGDSIIData=0;

void ClearGDSIICache()
{ if (CachedGDSIIData) delete CachedGDSIIData;
  CachedGDSIIData=0;
}

void OpenGDSIIFile(const char *GDSIIFileName)
{ 
  if (CachedGDSIIData && !strcmp(CachedGDSIIData->GDSIIFileName->c_str(),GDSIIFileName) )
   return;
  else if (CachedGDSIIData) 
   ClearGDSIICache();
  CachedGDSIIData = new GDSIIData(GDSIIFileName);
  if (CachedGDSIIData->ErrMsg)
   GDSIIData::ErrExit(CachedGDSIIData->ErrMsg->c_str());
}
  
PolygonList GetPolygons(const char *GDSIIFile, const char *Label, int Layer)
{ OpenGDSIIFile(GDSIIFile);
  return CachedGDSIIData->GetPolygons(Label,Layer);
}

PolygonList GetPolygons(const char *GDSIIFile, int Layer)
 { return GetPolygons(GDSIIFile, 0, Layer); }

/***************************************************************/
/* find the value of s at which the line p+s*d intersects the  */
/* line segment connecting v1 to v2 (in 2 dimensions)          */
/* algorithm: solve the 2x2 linear system p+s*d = a+t*b        */
/* where s,t are scalars and p,d,a,b are 2-vectors with        */
/* a=v1, b=v2-v1                                               */
/***************************************************************/
bool intersect_line_with_segment(double px, double py, double dx, double dy,
                                 double *v1, double *v2, double *s)
{
  double ax = v1[0],        ay  = v1[1];
  double bx = v2[0]-v1[0],  by  = v2[1]-v1[1];
  double M00  = dx,         M10 = dy;
  double M01  = -1.0*bx,    M11 = -1.0*by;
  double RHSx = ax - px,    RHSy = ay - py;
  double DetM = M00*M11 - M01*M10;
  double L2 = bx*bx + by*by; // squared length of edge
  if ( fabs(DetM) < 1.0e-10*L2 ) // d zero or nearly parallel to edge-->no intersection
   return false;

  double t = (M00*RHSy-M10*RHSx)/DetM;
  if (t<0.0 || t>1.0) // intersection of lines does not lie between vertices
   return false;

  if (s) *s = (M11*RHSx-M01*RHSy)/DetM;
  return true;
}

// like the previous routine, but only count intersections if s>=0
bool intersect_ray_with_segment(double px, double py, double dx, double dy,
                                double *v1, double *v2, double *s)
{ double ss=0.0; if (s==0) s=&ss;
  return (intersect_line_with_segment(px,py,dx,dy,v1,v2,s) && *s>0.0);
}

/***************************************************************/
/* 2D point-in-polygon test: return 1 if the point lies within */
/* the polygon with the given vertices, 0 otherwise.           */
// method: cast a plumb line in the negative y direction from  */
/* p to infinity and count the number of edges intersected;    */
/* point lies in polygon iff this is number is odd.            */
/***************************************************************/
bool PointInPolygon(dVec Vertices, double X, double Y)
{
  size_t NV = Vertices.size() / 2;
  if (NV<3) return false;
  int num_side_intersections=0;
  for(size_t nv=0; nv<NV; nv++)
   { int nvp1 = (nv+1)%NV;
     double v1[2], v2[2];
     v1[0] = Vertices[2*nv+0  ]; v1[1]=Vertices[2*nv+  1];
     v2[0] = Vertices[2*nvp1+0]; v2[1]=Vertices[2*nvp1+1];
     if (intersect_ray_with_segment(X, Y, 0, -1.0, v1, v2, 0))
      num_side_intersections++;
   }
  return (num_side_intersections%2)==1;
}

/***************************************************************/
/* utility routines from libhrutil, duplicated here to avoid   */
/* that dependency                                             */
/***************************************************************/
bool GDSIIData::Verbose=false;
char *GDSIIData::LogFileName=0;
#define MAXSTR 1000
void GDSIIData::Log(const char *format, ...)
{
  va_list ap;
  va_start(ap,format);
  char buffer[MAXSTR];
  vsnprintf(buffer,MAXSTR,format,ap);
  va_end(ap);

  FILE *f=0;
  if (LogFileName && !strcmp(LogFileName,"stderr"))
   f=stderr;
  else if (LogFileName && !strcmp(LogFileName,"stdout"))
   f=stdout;
  else if (LogFileName)
   f=fopen(LogFileName,"a");
  if (!f) return;

  time_t MyTime;
  struct tm *MyTm;
  MyTime=time(0);
  MyTm=localtime(&MyTime);
  char TimeString[30];
  strftime(TimeString,30,"%D::%T",MyTm);
  fprintf(f,"%s: %s\n",TimeString,buffer);

  if (f!=stderr && f!=stdout) fclose(f);
}

void GDSIIData::ErrExit(const char *format, ...)
{
  va_list ap; 
  char buffer[MAXSTR];

  va_start(ap,format);
  vsnprintf(buffer,MAXSTR,format,ap);
  va_end(ap);

  fprintf(stderr,"error: %s (aborting)\n",buffer);
  Log("error: %s (aborting)",buffer);

  exit(1);
}

void GDSIIData::Warn(const char *format, ...)
{
  va_list ap;
  char buffer[MAXSTR];

  va_start(ap,format);
  vsnprintf(buffer,MAXSTR,format,ap);
  va_end(ap);

  if (Verbose) 
   fprintf(stderr,"**warning: %s \n",buffer);
  Log("warning: %s \n",buffer);

}

char *GDSIIData::vstrappend(char *s, const char *format, ...)
{
  va_list ap;
  char buffer[MAXSTR];

  va_start(ap,format);
  vsnprintf(buffer,MAXSTR,format,ap);
  va_end(ap);

  if (s==0)
   s=strdup(buffer);
  else
   { int NS=strlen(s), NB=strlen(buffer);
     s = (char *)realloc(s, NS+NB+1);
     strcpy(s + NS, buffer);
   }
  return s;
}

char *GDSIIData::vstrdup(const char *format, ...)
{
  va_list ap;
  char buffer[MAXSTR];

  va_start(ap,format);
  vsnprintf(buffer,MAXSTR,format,ap);
  va_end(ap);
  return strdup(buffer);
}

} // namespace libGSDII

/***************************************************************/
/* crutch function to play nice with autotools *****************/
/***************************************************************/
extern "C" {
void libGDSIIExists(){}
}
