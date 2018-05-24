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
 * libGDSII is a C++ library for working with GDSII data files.
 * Homer Reid   11/2017
 */
#ifndef LIBGDSII_H
#define LIBGDSII_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#include <string>
#include <vector>
#include <set>
#include <sstream>

using namespace std;

/***************************************************************/
/* convenient shorthand typedefs *******************************/
/***************************************************************/
#ifndef iVec
  typedef vector<int>    iVec;
#endif
#ifndef dVec
  typedef vector<double> dVec;
#endif
#ifndef bVec
  typedef vector<bool> bVec;
#endif
#ifndef sVec
  typedef vector<char *> sVec;
#endif
#ifndef strVec
  typedef vector<string> strVec;
#endif

/***************************************************************/
/* Data structures used to process GDSII files:                */
/*  (a) GDSIIElement and GDSIIStruct are used to store info    */
/*      on the geometry as described in the GDSII file, with   */
/*      nesting hierarchy intact                               */
/*  (b) Flattening the hierarchy (i.e. eliminating all SREFS   */
/*      and AREFS to instantiate all objects and text directly)*/
/*      yields a table of Entity structures, organized by the  */
/*      layer on which they appear. An Entity is simply just   */ 
/*      a polygon (collection of vertices, with an optional    */ 
/*      label) or a text string (with a single vertex as       */ 
/*      reference point/location). An EntityList is a          */ 
/*      collection of Entities, in no particular order, all on */ 
/*      the same layer. An EntityTable is a collection of      */ 
/*      (LayerIndex, EntityList) pairs.                        */ 
/***************************************************************/
enum ElementType { BOUNDARY, PATH, SREF, AREF, TEXT, NODE, BOX };

typedef struct GDSIIElement
 { 
   ElementType Type;
   int Layer, DataType, TextType, PathType;
   iVec XY;
   string *SName;
   int Width, Columns, Rows;
   int nsRef;
   string *Text;
   bool Refl, AbsMag, AbsAngle;
   double Mag, Angle;
   iVec PropAttrs;
   strVec PropValues;
 } GDSIIElement;

typedef struct GDSIIStruct
 { 
   vector<GDSIIElement *> Elements;
   bool IsPCell;
   bool IsReferenced;
   string *Name;

 } GDSIIStruct;

typedef struct Entity
 { dVec XY;
   char *Text;
   char *Label;
   bool Closed;
 } Entity;

typedef vector<Entity>     EntityList;
typedef vector<EntityList> EntityTable;

/**********************************************************************/
/* GDSIIData is the main class that reads and stores a GDSII geometry.*/
/**********************************************************************/
namespace libGDSII
{
  /***************************************************************/
  /* GDSIIData describes the content of a single GDSII file. *****/
  /***************************************************************/
  class GDSIIData
   {
     /*--------------------------------------------------------*/
     /*- API methods                                           */
     /*--------------------------------------------------------*/
     public:
      
       // construct from a binary GDSII file 
       GDSIIData(const string FileName);
       ~GDSIIData();

       void WriteDescription(const char *FileName=0);

     /*--------------------------------------------------------*/
     /* API data fields                                        */
     /*--------------------------------------------------------*/
      string *ErrMsg; // non-null upon failure of constructor or other API routine

     /*--------------------------------------------------------*/
     /* methods intended for internal use                      */
     /*--------------------------------------------------------*/
// private:
    // constructor helper methods
      void ReadGDSIIFile(const string FileName);
      int GetStructByName(string Name);
      void Flatten(double UnitInMM=1.0);

     /*--------------------------------------------------------*/
     /* variables intended for internal use                    */
     /*--------------------------------------------------------*/

     // general info on the GDSII file
     string *LibName;
     string *GDSIIFileName;
     double FileUnits[2], UnitInMeters;
     set<int> LayerSet; 
     iVec Layers;

     // list of structures (hierarchical, i.e. pre-flattening)
     vector<GDSIIStruct *> Structs;

     // table of entities (flattened)
     // ETable[nl] = unsorted vector of entities on layer # Layers[nl]
     EntityTable ETable;

     /*--------------------------------------------------------*/
     /*- utility routines -------------------------------------*/
     /*--------------------------------------------------------*/
     static bool Verbose;
     static char *LogFileName;
     static void Log(const char *format, ...);
     static void ErrExit(const char *format, ...);
     static void Warn(const char *format, ...);
     static char *vstrappend(char *s, const char *format, ...);
     static char *vstrdup(const char *format, ...);
   };

/***************************************************************/
/* non-class-method geometric primitives ***********************/
/***************************************************************/
bool PointInPolygon(dVec Vertices, double X, double Y);

/***************************************************************/
/* non-class method utility routines                           */
/***************************************************************/
bool DumpGDSIIFile(const char *FileName);
void WriteGMSHEntity(Entity E, int Layer, char *geoFileName, FILE **pgeoFile,
                     char *ppFileName=0, FILE **pppFile=0);
void WriteGMSHFile(EntityTable ETable, iVec Layers, char *FileBase, bool SeparateLayers=false);

} /* namespace libGDSII */

#endif /* LIBGDSII_H*/
