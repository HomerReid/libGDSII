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
#include <math.h>

#include <string>
#include <vector>

using namespace std;

enum ElementType { BOUNDARY, PATH, SREF, AREF, TEXT, NODE, BOX };

typedef struct GDSIIElement
 { 
   ElementType Type;
   int Layer;
   int DataType;
   vector<int> XY;
   string *SName;
   int Width;
   int Columns; 
   int Rows;
   int nsRef;
   string *Text;
 } GDSIIElement;

typedef struct GDSIIStruct
 { 
   vector<GDSIIElement *>Elements;
   bool IsReferenced;
   string *Name;

 } GDSIIStruct;

namespace libGDSII
{
  class GDSIIData
   {
     /*--------------------------------------------------------*/
     /*- API methods                                           */
     /*--------------------------------------------------------*/
     public:
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

     /*--------------------------------------------------------*/
     /* variables intended for internal use                    */
     /*--------------------------------------------------------*/

     // general info on the GDSII file
     string *LibName;
     double Unit, FileUnits[2];

     // list of structures
     vector<GDSIIStruct *> Structs;
   };


/***************************************************************/
/* non-class method utility routines                           */
/***************************************************************/
bool DumpGDSIIFile(const char *FileName);

} /* namespace libGDSII */

#endif /* LIBGDSII_H*/
