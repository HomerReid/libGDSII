/*
   Copyright (C) 2005-2017 Massachusetts Institute of Technology
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
  
/*
 * GDSIIConvert.cc  --  read in a GDSII geometry, flatten its hierarchy,
 * Homer Reid       --  and convert to various output formats
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <math.h>

#include "libGDSII.h"

using namespace std;
using namespace libGDSII;

/***************************************************************/
/***************************************************************/
/***************************************************************/
void Usage(const char *ErrorMessage, ...)
{
  if (ErrorMessage)
   { va_list ap;
     va_start(ap,ErrorMessage);
     char buffer[1000];
     vsnprintf(buffer,1000,ErrorMessage,ap);
     va_end(ap);
     printf("error: %s (aborting)\n",buffer);
   }

  printf("Usage: GDSIIConvert File.GDS [options]\n");
  printf("Options: \n");
  printf("\n");
  printf(" ** Output formats: ** \n");
  printf("   --raw              raw dump of file data records\n");
  printf("   --analyze          detailed listing of hierarchical structure \n");
  printf("   --GMSH             Export GMSH geometry to FileBase.geo (text strings to FileBase.pp)\n");
  printf("   --scuff-rf         Write .port file defining RF ports for scuff-RF (implies --gmsh)\n");
  printf("\n");
  printf(" ** Other flags: **\n");
  printf("   --MetalLayer     12  define layer 12 as a metal layer (may be specified multiple times)\n");
  printf("   --LengthUnit     xx  set output length unit in mm (default = 1)\n");
  printf("   --FileBase       xx  set base name for output files\n");
  printf("   --verbose            produce more output\n");
  printf("   --SeparateLayers     write separate output files for objects on each layer\n");
  exit(1);
}

typedef struct GDSIIOptions
 { char *GDSIIFile;
   bool Raw, Analyze, WriteGMSH, WritePorts;
   double UnitInMM;
   char *FileBase;
   bool Verbose;
   bool SeparateLayers;
   iVec MetalLayers;
 } GDSIIOptions;

/***************************************************************/
/***************************************************************/
/***************************************************************/
GDSIIOptions *ProcessGDSIIOptions(int argc, char *argv[])
{
  if ( argc<2 )
   Usage(const_cast<char *>("no GDSII file specified"));

  /***************************************************************/
  /* process command-line options  *******************************/
  /***************************************************************/
  GDSIIOptions *Options   = new GDSIIOptions;
  Options->GDSIIFile      = 0;
  Options->Raw            = false;
  Options->Analyze        = false;
  Options->WriteGMSH      = false;
  Options->WritePorts     = false;
  Options->UnitInMM       = 1.0;
  Options->FileBase       = 0;
  Options->Verbose        = false;
  Options->SeparateLayers = false;

  int narg=1;
  for(; narg<argc; narg++)
   { 
     char *Extension = strrchr(argv[narg],'.');

     // try to process the argument as one of our boolean flags
     bool ProcessedArg=true;
     if (!strcasecmp(argv[narg],"--raw"))
      Options->Raw=true;
     else if (!strcasecmp(argv[narg],"--analyze"))
      Options->Analyze=true;
     else if (!strcasecmp(argv[narg],"--GMSH"))
      Options->WriteGMSH=true;
     else if (!strcasecmp(argv[narg],"--scuff-rf"))
      Options->WriteGMSH=Options->WritePorts=true;
     else if (!strcasecmp(argv[narg],"--Verbose"))
      Options->Verbose=true;
     else if (!strcasecmp(argv[narg],"--SeparateLayers"))
      Options->SeparateLayers=true; 
     else if (Extension && !strncasecmp(Extension,".gds", 4)) // try to process as GDSII filename
      { if (Options->GDSIIFile!=0)
         GDSIIData::ErrExit("more than one GDSII file specified (%s,%s)",argv[1],Options->GDSIIFile);
        Options->GDSIIFile = argv[narg];
      }
     else 
      ProcessedArg=false;
     if (ProcessedArg)
      { argv[narg]=0;
        continue;
      }

     // all remaining options take an argument 
     if ( narg+1 >= argc )
      { string ErrMsg=string("no argument given for option ") + argv[narg];
        Usage(strdup(ErrMsg.c_str()));
      }

     ProcessedArg=true;
     if (!strcasecmp(argv[narg],"--FileBase"))
      Options->FileBase=argv[++narg];
     else if (!strcasecmp(argv[narg],"--GDSFile") || !strcasecmp(argv[narg],"--GDSIIFile"))
      Options->GDSIIFile=argv[++narg];
     else if (!strcasecmp(argv[narg],"--LogFile"))
      GDSIIData::LogFileName=strdup(argv[++narg]);
     else if (!strcasecmp(argv[narg],"--LengthUnit"))
      sscanf(argv[++narg],"%le",&Options->UnitInMM);
     else if (!strcasecmp(argv[narg],"--MetalLayer"))
      { int nml; if (1==sscanf(argv[++narg],"%i",&nml)) Options->MetalLayers.push_back(nml);
      }
     else
      Usage("unknown argument %s",argv[narg]);
   }

  if (Options->GDSIIFile==0)
   Usage("no GDSII file specified");

  if (Options->FileBase==0)
   { Options->FileBase = strdup(Options->GDSIIFile);
     char *s=strrchr(Options->FileBase,'.');
     if (s) s[0]=0;
     s=strrchr(s,'/');
     if (s) Options->FileBase=s+1;
   }

  return Options;
}

/***************************************************************/
/* Attempt to interpret a text string as a port terminal label.*/
/* If we successfully identify the string as labeling the      */
/*  \pm terminal of port N, return \pm N. (Note that port      */
/* indices start at 1; there is no port 0.)                    */
/* Otherwise return 0.                                         */
/*                                                             */
/* Examples of port terminal labels:                           */
/*  PORT 3+                                                    */
/*  port 2P                                                    */
/*  Port 1m                                                    */
/*  port 7-                                                    */
/***************************************************************/
int DetectPortTerminalLabel(char *Text)
{ 
  if (strncasecmp(Text,"PORT ",5)) return 0;
  int N;
  char PM;
  if (2!=sscanf(Text+5,"%i%c",&N,&PM))
   { GDSIIData::Warn("warning: %s is not a valid port terminal label (ignoring)",Text); return 0; }
   
  if (N<=0) 
   { GDSIIData::Warn("warning: in port terminal label %s: %i is not a valid port index (ignoring)",Text,N); return 0; }

  if (strchr("Pp+",PM))
   return +N;
  if (strchr("MmNn-",PM))
   return -N;

  GDSIIData::Warn("In port terminal label %s: %c is not a valid polarity indicator (ignoring)",Text,PM);
  return 0;
}

/********************************************************************/
/* Routine for exporting geometry to .geo (GMSH geometry),          */
/* .pp (GMSH post-processing), and .ports (SCUFF-RM ports) files    */
/********************************************************************/
#define ZPORT 0.0 // z-coordinate of polygon vertices used to detect edge ports 
void WriteGeometryAndPorts(GDSIIData *gdsIIData, GDSIIOptions *Options)
{
  iVec Layers = gdsIIData->Layers;

  /***************************************************************/
  /* first pass to write text strings to GMSH .pp file           */
  /***************************************************************/
  char *ppFileName = GDSIIData::vstrdup("%s.pp", Options->FileBase);
  FILE *ppFile=0;
  int NumTextStrings=0;
  for(size_t nl=0; nl<Layers.size(); nl++)
   for(size_t ne=0; ne<gdsIIData->ETable[nl].size(); ne++)
    if (gdsIIData->ETable[nl][ne].Text)
     { WriteGMSHEntity(gdsIIData->ETable[nl][ne], Layers[nl], 0, 0, ppFileName, &ppFile);
       NumTextStrings++;
     }
  if (ppFile)
   { fclose(ppFile);
     printf("Wrote %i text strings to %s.\n",NumTextStrings,ppFileName);
   }

  /***************************************************************/
  /* second pass to identify port definitions                    */
  /***************************************************************/
  bVec IsPortLayer(Layers.size(), false);
  if (Options->WritePorts)
   { 
     sVec PortStrings[2];
     int NumPorts=0, TotalPortTerminals=0;
     for(size_t nl=0; nl<Layers.size(); nl++)
      { 
        /*******************************************************************/
        /* For each text string in this layer that labels a port terminal, */
        /* look for a polygon on this layer that contains its base point.  */
        /*******************************************************************/
        EntityList Entities = gdsIIData->ETable[nl]; // list of all entities on this layer
        int PortTerminalsThisLayer=0;
        for(size_t ne=0; ne<Entities.size(); ne++)
         { 
           Entity EText=Entities[ne];
           if ( EText.Text==0 ) continue;
           int PortTerminal = DetectPortTerminalLabel(EText.Text);
           if (PortTerminal==0) continue;
           IsPortLayer[nl] = true;
           int Pol     = ( PortTerminal>0 ?  0 :  1);
           int PortNum = PortTerminal * (Pol ? -1 : 1);
           if (PortNum > NumPorts)
            { NumPorts = PortNum;
              PortStrings[0].resize(NumPorts,0);
              PortStrings[1].resize(NumPorts,0);
            }
           bool PolygonFound=false; 
           for(size_t nep=0; !PolygonFound && nep<Entities.size(); nep++)
            { Entity EPolygon = Entities[nep];
              if ( PointInPolygon(EPolygon.XY, EText.XY[0], EText.XY[1]) )
               { char *Line = GDSIIData::vstrdup("    %s ",Pol ? "NEGATIVE" : "POSITIVE");
                 for(size_t nv=0; nv<EPolygon.XY.size()/2; nv++)
                  Line = GDSIIData::vstrappend(Line, "%+g %+g %+g ",EPolygon.XY[2*nv+0],EPolygon.XY[2*nv+1],ZPORT);
                 PortStrings[Pol][PortNum-1] = GDSIIData::vstrappend(PortStrings[Pol][PortNum-1],"%s\n",Line);
   	         Entities[nep].XY.clear(); //  ensure this polygon won't be detected again
        	 PolygonFound=true;
                 PortTerminalsThisLayer++;
               }
            }
           if (!PolygonFound)
            GDSIIData::Warn("port-terminal label %s on layer %i is not contained in any polygon (ignoring)",EText.Text,Layers[nl]);
         }
        printf("... %i port terminals on layer %i\n",PortTerminalsThisLayer,Layers[nl]);
        TotalPortTerminals+=PortTerminalsThisLayer;
      }

     if (TotalPortTerminals==0)
      GDSIIData::Warn("no labeled port-terminal polygons detected");

     /***************************************************************/
     /* write .ports file *******************************************/
     /***************************************************************/
     char *PortFileName = GDSIIData::vstrdup("%s.ports", Options->FileBase);
     FILE *f=fopen(PortFileName,"w");
     for(int np=0; np<NumPorts; np++)
      fprintf(f,"PORT\n\n%s\n%s\nENDPORT\n\n",PortStrings[0][np],PortStrings[1][np]);
     fclose(f);
     printf("Wrote %i port definitions (%i terminals) to %s.\n",NumPorts,TotalPortTerminals,PortFileName);
   } // if (Options->WritePorts)

  /***************************************************************/
  /* final loop over layers on which ports were *not* found      */
  /* to write any structures on those layers to a GMSH .geo file */
  /***************************************************************/
  char *geoFileName = GDSIIData::vstrdup("%s.geo", Options->FileBase);
  FILE *geoFile=0;
  int TotalPolygons=0;
  for(size_t nl=0; nl<Layers.size(); nl++)
   { 
     if (IsPortLayer[nl]) continue; 

     bool IsMetalLayer=true;
     if (Options->MetalLayers.size()>0)
      { IsMetalLayer=false;
        for(size_t nml=0; !IsMetalLayer && nml<Options->MetalLayers.size(); nml++)
         if (Layers[nl] == Options->MetalLayers[nml])
          IsMetalLayer=true;
      }
     if (!IsMetalLayer)
      { printf("Skipping non-metal layer %3i...",Layers[nl]);
        continue;
      }

     printf("Detecting metallization structures on layer %3i: ",Layers[nl]);
     EntityList Entities = gdsIIData->ETable[nl]; // list of all entities on this layer
     int PolygonsThisLayer=0;
     for(size_t ne=0; ne<Entities.size(); ne++)
      { Entity E=Entities[ne];
        if ( E.Text ) continue;
        WriteGMSHEntity(E, Layers[nl], geoFileName, &geoFile);
        PolygonsThisLayer++;
      }
     printf("... %i polygons on layer %i\n",PolygonsThisLayer,Layers[nl]);
     TotalPolygons+=PolygonsThisLayer;
   }
  if (geoFile)
   { fclose(geoFile);
     printf("Wrote %i metallization polygons to %s.\n",TotalPolygons,geoFileName);
   }
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
#define MEMORY_USAGE_SLOTS 7
#define MAXSTR 1000
unsigned long GetMemoryUsage(unsigned long *MemoryUsage)
{
  if (MemoryUsage)
   memset(MemoryUsage, 0, MEMORY_USAGE_SLOTS*sizeof(MemoryUsage[0]));
#if defined(_WIN32)
  // don't know portable way to get memory usage on windows
#else
#if 1
  FILE *f=fopen("/proc/self/status","r");
  if (f)
   { char Line[MAXSTR];
     while(fgets(Line, MAXSTR, f))
      { unsigned long mem;
        if (1==sscanf(Line,"VmPeak: %lu kB",&mem))
         MemoryUsage[0]=mem;
        else if (1==sscanf(Line,"VmSize: %lu kB",&mem))
         MemoryUsage[1]=mem;
        else if (1==sscanf(Line,"VmHWM: %lu kB",&mem))
         MemoryUsage[2]=mem;
        else if (1==sscanf(Line,"VmRSS: %lu kB",&mem))
         MemoryUsage[3]=mem;
        else if (1==sscanf(Line,"VmData: %lu kB",&mem))
         MemoryUsage[4]=mem;
        else if (1==sscanf(Line,"VmPTE:  %lu kB",&mem))
         MemoryUsage[5]=mem;
        else if (1==sscanf(Line,"VmPMD:  %lu kB",&mem))
         MemoryUsage[6]=mem;
      } 
     fclose(f);
   }
#else
  FILE *f=fopen("/proc/self/statm","r");
  if (f)
   { 
     char Line[MAXSTR];
     char *result=fgets(Line,MAXSTR,f);
     fclose(f);
     if (result==0) return 0;

     unsigned long Mem[MEMORY_USAGE_SLOTS];
     sscanf(Line,"%lu %lu %lu %lu %lu %lu %lu",
                  Mem+0, Mem+1, Mem+2, Mem+3, Mem+4, Mem+5, Mem+6);

     long sz = sysconf(_SC_PAGESIZE);
     if (MemoryUsage)
      for(int n=0; n<MEMORY_USAGE_SLOTS; n++)
       MemoryUsage[n] = sz*Mem[n];

     return sz*Mem[0];
   }
#endif
#endif
  return 0;
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
int main(int argc, char *argv[])
{
  /***************************************************************/
  /* parse command-line options                                  */
  /***************************************************************/
  GDSIIOptions *Options = ProcessGDSIIOptions(argc, argv);

  /***************************************************************/
  /* dump raw file data (before reading GDSII data) if requested */
  /***************************************************************/
  if (Options->Raw) DumpGDSIIFile(Options->GDSIIFile);

#if 1
{
  if (GDSIIData::LogFileName==0) GDSIIData::LogFileName=strdup("/tmp/GDSIIConvert.log");
  unsigned long MemBefore[MEMORY_USAGE_SLOTS];
  unsigned long MemDuring[MEMORY_USAGE_SLOTS];
  unsigned long MemAfter[MEMORY_USAGE_SLOTS];
  GetMemoryUsage(MemBefore);
  GDSIIData *gdsIIData  = new GDSIIData( string(Options->GDSIIFile) );
  GetMemoryUsage(MemDuring);
  delete gdsIIData;
  GetMemoryUsage(MemAfter);
  for(int n=0; n<MEMORY_USAGE_SLOTS; n++)
   GDSIIData::Log("Mem[%i] before,during,after={%lu,%lu,%lu},delta=%lu",
n,MemBefore[n],MemDuring[n],MemAfter[n],MemAfter[n]-MemBefore[n]);
}
#endif
  
  /***************************************************************/
  /* try to read in the GDSII file                               */
  /***************************************************************/
  GDSIIData *gdsIIData  = new GDSIIData( string(Options->GDSIIFile) );
  if (gdsIIData->ErrMsg)
   { printf("error: %s (aborting)\n",gdsIIData->ErrMsg->c_str());
     exit(1);
   }

  /***************************************************************/
  /* output geometry statistics if requested                     */
  /***************************************************************/
  if (Options->Analyze)
   gdsIIData->WriteDescription();
  
  /****************************************************************/
  /* Flatten hierarchy, then write geometry and (optionally) ports*/
  /****************************************************************/
  if (Options->WriteGMSH)
   WriteGeometryAndPorts(gdsIIData, Options);


  printf("Thank you for your support.\n");

}
