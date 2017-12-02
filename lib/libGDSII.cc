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
 * libGDSII.cc -- main source file for libGDSII
 * Homer Reid   11/2017
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <string>
#include <sstream>

#include "libGDSII.h"

using namespace std;
namespace libGDSII {

/***************************************************************/
/* GDSIIData constructor: create a new GDSIIData instance from */
/* a binary GDSII file.                                        */
/***************************************************************/
GDSIIData::GDSIIData(const string FileName)
{ 
  // initialize class data
  LibName   = 0;
  Unit      = 1.0e-6;
  FileUnits[0]=FileUnits[1]=0.0;

  ReadGDSIIFile(FileName);

  // at this point ErrMsg is non-null if an error occurred
  if (ErrMsg) return;
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

} // namespace libGSDII
