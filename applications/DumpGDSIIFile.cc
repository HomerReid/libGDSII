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
 * DumpGDSIIFile -- simple test/demo program for libGDSII library
 *               -- for working with GDSII files
 * Homer Reid       11/2017
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>

#include "libGDSII.h"

using namespace std;
using namespace libGDSII;

int main(int argc, char *argv[])
{
  /***************************************************************/
  /* check command-line parameters *******************************/
  /***************************************************************/
  if ( argc<2 )
   { printf("usage: DumpGDSIIFile File.gds [--raw]\n");
     exit(1);
   };
  string *GDSIIFile = new string(argv[1]);

  bool RawMode=false;
  if ( argc>=3 && !strcasecmp(argv[2],"--raw"))
   RawMode=true;

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  if (RawMode)
   { DumpGDSIIFile(GDSIIFile->c_str());
     exit(1);
   };

  /***************************************************************/
  /* attempt to read in the file *********************************/
  /***************************************************************/
  GDSIIData *gdsIIData = new GDSIIData(*GDSIIFile);
  if (gdsIIData->ErrMsg)
   { printf("error: %s (aborting)\n",gdsIIData->ErrMsg->c_str());
     exit(1);
   };

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  gdsIIData->WriteDescription();

}
