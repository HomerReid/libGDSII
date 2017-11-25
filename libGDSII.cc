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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <string>
#include <sstream>

#include "libGDSII.h"

namespace libGDSII {

typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned long  DWORD;

/***************************************************************/
/* some data structures used in this file only *****************/
/***************************************************************/
typedef struct GDSIIRecord
 {
   BYTE RType; // record type

   // could use a union for the following, but I don't bother
   bool Bits[16];
   int *iVal;
   double *dVal;
   string *sVal;
   int NumVals;

 } GDSIIRecord;

typedef enum ParseState
 { INITIAL,  INHEADER,  INLIB,  INSTRUCT, INELEMENT, DONE };

class GDSIIData; // forward reference 
typedef struct ParseState 
 { 
   GDSIIData *Data;
   int NumRecords;

 } ParseState;

typedef *string (*Handler)(GDSIIRecord *Record, ParseState *PState);

/***************************************************************/
/* Handlers for specific types of data records in GDSII files  */
/***************************************************************/
string *handleHEADER(GDSIIRecord *Record, ParseState *PState)
{
  if (PState->State!=ParseState.INITIAL)
   return new string("unexpected record before HEADER");

  // save date info

  PState->State=SawHEADER;
  return 0;
}

string *handleBGNLIB(GDSIIRecord *Record, ParseState *PState)
{
  if (PState->State!=SawHEADER)
   return new string("unexpected record BGNLIB");

  PState->State=SawBGNLIB;

  // save date info

  return 0;
}

string *handleLIBNAME(GDSIIRecord *Record, ParseState *PState)
{ if (PState->State!=SawHEADER)
   return new string("unexpected record LIBNAME");
  PState->Data->LibName = new string(Record->sVal);
  return 0;
}

string *handleUNITS(GDSIIRecord *Record, ParseState *PState)
{ 
  PState->Data->FileUnits[0] = Record->dVal[0];
  PState->Data->FileUnits[1] = Record->dVal[1];
  PState->Date->Unit = PState->Data->FileUnits[1] / PState->Data->FileUnits[0];
  return 0;
}

string *handleENDLIB(GDSIIRecord *Record, ParseState *PState)
{ if 
}

/***************************************************************/
/* table of GDSII data types ***********************************/
/***************************************************************/
enum DataType{ NO_DATA,    // 0x00
               BITARRAY,   // 0x01
               INTEGER_2,  // 0x02
               INTEGER_4,  // 0x03
               REAL_4,     // 0x04
               REAL_8,     // 0x05
               STRING      // 0x06
              };

/***************************************************************/
/* table of GDS record types, gleeped directly from the text of*/
/* the buchanan email                                          */
/***************************************************************/
typedef struct RecordType
 { const char    *Name;
   DataType       DType;
   RecordHandler  Handler;
 } RecordType;

const static RecordType RecordTypes[]={
 /*0x00*/  {"HEADER",       INTEGER_2,   handleHEADER},
 /*0x01*/  {"BGNLIB",       INTEGER_2,   handleBGNLIB},
 /*0x02*/  {"LIBNAME",      STRING,      handleLIBNAME},
 /*0x03*/  {"UNITS",        REAL_8,      handleUNITS},
 /*0x04*/  {"ENDLIB",       NO_DATA,     handleENDLIB},
 /*0x05*/  {"BGNSTR",       INTEGER_2,   handleBGNSTR},
 /*0x06*/  {"STRNAME",      STRING,      handleSTRNAME},
 /*0x07*/  {"ENDSTR",       NO_DATA,     handleENDSTR},
 /*0x08*/  {"BOUNDARY",     NO_DATA,     handleBOUNDARY},
 /*0x09*/  {"PATH",         NO_DATA,     0},
 /*0x0a*/  {"SREF",         NO_DATA,     0},
 /*0x0b*/  {"AREF",         NO_DATA,     0},
 /*0x0c*/  {"TEXT",         NO_DATA,     0},
 /*0x0d*/  {"LAYER",        INTEGER_2,   0},
 /*0x0e*/  {"DATATYPE",     INTEGER_2,   0},
 /*0x0f*/  {"WIDTH",        INTEGER_4,   0},
 /*0x10*/  {"XY",           INTEGER_4,   0},
 /*0x11*/  {"ENDEL",        NO_DATA,     0},
 /*0x12*/  {"SNAME",        STRING,      0},
 /*0x13*/  {"COLROW",       INTEGER_2,   0},
 /*0x14*/  {"TEXTNODE",     NO_DATA,     0},
 /*0x15*/  {"NODE",         NO_DATA,     0},
 /*0x16*/  {"TEXTTYPE",     INTEGER_2,   0},
 /*0x17*/  {"PRESENTATION", BITARRAY,    0},
 /*0x18*/  {"UNUSED",       NO_DATA,     0},
 /*0x19*/  {"STRING",       STRING,      0},
 /*0x1a*/  {"STRANS",       BITARRAY,    0},
 /*0x1b*/  {"MAG",          REAL_8,      0},
 /*0x1c*/  {"ANGLE",        REAL_8,      0},
 /*0x1d*/  {"UNUSED",       NO_DATA,     0},
 /*0x1e*/  {"UNUSED",       NO_DATA,     0},
 /*0x1f*/  {"REFLIBS",      STRING,      0},
 /*0x20*/  {"FONTS",        STRING,      0},
 /*0x21*/  {"PATHTYPE",     INTEGER_2,   0},
 /*0x22*/  {"GENERATIONS",  INTEGER_2,   0},
 /*0x23*/  {"ATTRTABLE",    STRING,      0},
 /*0x24*/  {"STYPTABLE",    STRING,      0},
 /*0x25*/  {"STRTYPE",      INTEGER_2,   0},
 /*0x26*/  {"ELFLAGS",      BITARRAY,    0},
 /*0x27*/  {"ELKEY",        INTEGER_4,   0},
 /*0x1d*/  {"LINKTYPE",     NO_DATA,     0},
 /*0x1e*/  {"LINKKEYS",     NO_DATA,     0},
 /*0x2a*/  {"NODETYPE",     INTEGER_2,   0},
 /*0x2b*/  {"PROPATTR",     INTEGER_2,   0},
 /*0x2c*/  {"PROPVALUE",    STRING,      0},
 /*0x2d*/  {"BOX",          NO_DATA,     0},
 /*0x2e*/  {"BOXTYPE",      INTEGER_2,   0},
 /*0x2f*/  {"PLEX",         INTEGER_4,   0},
 /*0x30*/  {"BGNEXTN",      INTEGER_4,   0},
 /*0x31*/  {"ENDTEXTN",     INTEGER_4,   0},
 /*0x32*/  {"TAPENUM",      INTEGER_2,   0},
 /*0x33*/  {"TAPECODE",     INTEGER_2,   0},
 /*0x34*/  {"STRCLASS",     BITARRAY,    0},
 /*0x35*/  {"RESERVED",     INTEGER_4,   0},
 /*0x36*/  {"FORMAT",       INTEGER_2,   0},
 /*0x37*/  {"MASK",         STRING,      0},
 /*0x38*/  {"ENDMASKS",     NO_DATA,     0},
 /*0x39*/  {"LIBDIRSIZE",   INTEGER_2,   0},
 /*0x3a*/  {"SRFNAME",      STRING,      0},
 /*0x3b*/  {"LIBSECUR",     INTEGER_2,   0}
};

#define RTYPE_HEADER		0x00
#define RTYPE_BGNLIB		0x01
#define RTYPE_LIBNAME		0x02
#define RTYPE_UNITS		0x03
#define RTYPE_ENDLIB		0x04
#define RTYPE_BGNSTR		0x05
#define RTYPE_STRNAME		0x06
#define RTYPE_ENDSTR		0x07
#define RTYPE_BOUNDARY		0x08
#define RTYPE_PATH		0x09
#define RTYPE_SREF		0x0a
#define RTYPE_AREF		0x0b
#define RTYPE_TEXT		0x0c
#define RTYPE_LAYER		0x0d
#define RTYPE_DATATYPE		0x0e
#define RTYPE_WIDTH		0x0f
#define RTYPE_XY		0x10
#define RTYPE_ENDEL		0x11
#define RTYPE_SNAME		0x12
#define RTYPE_COLROW		0x13
#define RTYPE_TEXTNODE		0x14
#define RTYPE_NODE		0x15
#define RTYPE_TEXTTYPE		0x16
#define RTYPE_PRESENTATION	0x17
#define RTYPE_UNUSED		0x18
#define RTYPE_STRING		0x19
#define RTYPE_STRANS		0x1a
#define RTYPE_MAG		0x1b
#define RTYPE_ANGLE		0x1c
#define RTYPE_UNUSED2		0x1d
#define RTYPE_UNUSED3		0x1e
#define RTYPE_REFLIBS		0x1f
#define RTYPE_FONTS		0x20
#define RTYPE_PATHTYPE		0x21
#define RTYPE_GENERATIONS	0x22
#define RTYPE_ATTRTABLE		0x23
#define RTYPE_STYPTABLE		0x24
#define RTYPE_STRTYPE		0x25
#define RTYPE_ELFLAGS		0x26
#define RTYPE_ELKEY		0x27
#define RTYPE_LINKTYPE		0x1d
#define RTYPE_LINKKEYS		0x1e
#define RTYPE_NODETYPE		0x2a
#define RTYPE_PROPATTR		0x2b
#define RTYPE_PROPVALUE		0x2c
#define RTYPE_BOX		0x2d
#define RTYPE_BOXTYPE		0x2e
#define RTYPE_PLEX		0x2f
#define RTYPE_BGNEXTN		0x30
#define RTYPE_ENDTEXTN		0x31
#define RTYPE_TAPENUM		0x32
#define RTYPE_TAPECODE		0x33
#define RTYPE_STRCLASS		0x34
#define RTYPE_RESERVED		0x35
#define RTYPE_FORMAT		0x36
#define RTYPE_MASK		0x37
#define RTYPE_ENDMASKS		0x38
#define RTYPE_LIBDIRSIZE	0x39
#define RTYPE_SRFNAME		0x3a
#define RTYPE_LIBSECUR		0x3b
#define MAX_RTYPE     		0x3b

/***************************************************************/
/* *************************************************************/
/***************************************************************/
int ConvertInt(BYTE *Bytes, DataType DType)
{ 
  unsigned long long i = Bytes[0]*256 + Bytes[1];
  if (DType==INTEGER_4)
   i = i*256*256 + Bytes[2]*256 + Bytes[3];
  if (Bytes[0] & 0x80) // sign bit
   return -1*( (DType==INTEGER_2 ? 0x010000 : 0x100000000) - i );
  return i;
}

double ConvertReal(BYTE *Bytes, DataType DType)
{ 
  double Sign  = (Bytes[0] & 0x80) ? -1.0 : +1.0;
  int Exponent = (Bytes[0] & 0x7F) - 64;
  int NumMantissaBytes = (DType==REAL_4 ? 3 : 7);
  int NumMantissaBits  = 8*NumMantissaBytes;
  double Mantissa=0.0;
  for(int n=0; n<NumMantissaBytes; n++)
   Mantissa = Mantissa*256 + ((double)(Bytes[1+n]));
  return Sign * Mantissa * pow(2.0, 4*Exponent - NumMantissaBits);
}

/***************************************************************/
/* read a single GDSII data record from the current file position */
/***************************************************************/
GDSIIRecord *ReadGDSIIRecord(FILE *f, string **ErrMsg)
{
  /*--------------------------------------------------------------*/
  /* read the 4-byte file header and check that the data type     */
  /* agrees with what it should be based on the record type       */
  /*--------------------------------------------------------------*/
  BYTE Header[4];
  if ( 4 != fread(Header, 1, 4, f) )
   { *ErrMsg = new string("unexpected end of file");
     return 0; // end of file
   };

  size_t RecordSize = Header[0]*256 + Header[1];
  BYTE RType        = Header[2];
  BYTE DType        = Header[3];
  
  if (RType > MAX_RTYPE)
   { *ErrMsg = new string("unknown record type");
     return 0;
   };
    
  if ( DType != RecordTypes[RType].DType )
   { ostringstream ss;
     ss << RecordTypes[RType].Name
        << ": data type disagrees with record type ("
        << DType
        << " != "
        << RecordTypes[RType].DType;
     *ErrMsg = new string(ss.str());
     return 0;
   };

  /*--------------------------------------------------------------*/
  /*- attempt to read payload ------------------------------------*/
  /*--------------------------------------------------------------*/
  size_t PayloadSize = RecordSize - 4;
  void *Payload=0;
  if (PayloadSize>0)
   { Payload = malloc(PayloadSize);
     if (Payload==0)
      { *ErrMsg = new string("out of memory");
        return 0;
      };
     if ( PayloadSize != fread(Payload, 1, PayloadSize, f) )
      { free(Payload);
        *ErrMsg = new string("unexpected end of file");
        return 0;
      };
   };
 
  /*--------------------------------------------------------------*/
  /* allocate space for the record and process payload data       */
  /*--------------------------------------------------------------*/
  GDSIIRecord *Record = (GDSIIRecord *)malloc(sizeof(GDSIIRecord));
  Record->RType   = RType;
  Record->sVal    = 0;
  Record->iVal    = 0;
  Record->dVal    = 0;
  Record->NumVals = 0;

  switch(DType)
   { case NO_DATA:
       break;

     case BITARRAY:
      { Record->NumVals=1;
        WORD W = *(WORD *)Payload;
        for(int nf=0, Flag=1; nf<16; nf++, Flag<<1)
         Record->Bits[nf] = (W & Flag);
      };
     break;

     case STRING:
      Record->NumVals=1;
      Record->sVal = new string( (char *)Payload );
      break;

     case INTEGER_2:
     case INTEGER_4:
      { size_t DataSize  = (DType==INTEGER_2) ? 2 : 4;
        Record->NumVals  = PayloadSize / DataSize;
        Record->iVal     = (int *)malloc(Record->NumVals * sizeof(int));
        if (Record->iVal==0)
         goto OutOfMemory;
        BYTE *B=(BYTE *)Payload; 
        for(size_t nv=0; nv<Record->NumVals; nv++, B+=DataSize)
         Record->iVal[nv] = ConvertInt(B, RecordTypes[RType].DType);
      };
     break;

     case REAL_4:
     case REAL_8:
      { size_t DataSize  = (DType==REAL_4) ? 4 : 8;
        Record->NumVals   = PayloadSize / DataSize;
        Record->dVal = (double *)malloc(Record->NumVals * sizeof(double));
        if (Record->dVal==0)
         goto OutOfMemory;
        BYTE *B=(BYTE *)Payload; 
        for(size_t nv=0; nv<Record->NumVals; nv++, B+=DataSize)
         Record->dVal[nv] = ConvertReal(B, RecordTypes[RType].DType);
      };
     break;

     default:
       *ErrMsg = new string("unknown data type " + DType);
       return 0;
   };

  // success 
  *ErrMsg=0;
  free(Payload);
  return Record;

  // failure
OutOfMemory:
  *ErrMsg = new string("out of memory");
  free(Payload);
  return Record;

}

void DestroyGDSIIRecord(GDSIIRecord *Record)
{ if (Record->iVal) free(Record->iVal);
  if (Record->dVal) free(Record->dVal);
  if (Record->sVal) delete(Record->sVal);
  free(Record);
}

/***************************************************************/
/* return string description of GDSII record   *****************/
/***************************************************************/
string *GetRecordDescription(GDSIIRecord *Record, bool Verbose=true)
{
  char Name[15];
  sprintf(Name,"%12s",RecordTypes[Record->RType].Name);
  ostringstream ss;
  ss << Name;

  if (Record->NumVals>0)
   ss << " ( " << Record->NumVals << ") ";
  
  if (!Verbose)
   return new string(ss.str());

  ss << " = ";
  switch(RecordTypes[Record->RType].DType)
   { 
     case INTEGER_2:    
     case INTEGER_4:    
      for(int nv=0; nv<Record->NumVals; nv++)
       ss << Record->iVal[nv] << " ";
      break;

     case REAL_4:
     case REAL_8:
      for(int nv=0; nv<Record->NumVals; nv++)
       ss << Record->dVal[nv] << " ";
      break;

     case BITARRAY:
      for(int n=0; n<16; n++)
       ss << Record->Bits[n] ? '1' : '0';
      break;

     case STRING:
      if (Record->sVal)
       ss << *(Record->sVal);
      else
       ss << "(null)";
      break; 

     case NO_DATA:
     default:
      break; 
   };

  return new string(ss.str());

}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
#define CHECK_RECORD(f, ErrMsg,
  { GDSIIRecord *Record=ReadGDSIIRecord(f, &ErrMsg); \
    if (ErrMsg)                                      \
     return;
    if (Record->
  }

// 
void GDSIIData::ReadGDSIIFile(const string FileName)
 {
   ErrMsg=0;

   /*--------------------------------------------------------------*/
   /*- try to open the file ---------------------------------------*/
   /*--------------------------------------------------------------*/
   FILE *f=fopen(FileName.c_str(),"r");
   if (!f)
    { ErrMsg = new string("could not open " + FileName);
      return;
    };

   /*--------------------------------------------------------------*/
   /*- read records one at a time until we encounter a record of   */
   /*- type ENDLIB                                                 */
   /*--------------------------------------------------------------*/
   int NumRecords=0;
   ParseState PState;
   PState.Data       = this;
   PState.NumRecords = 0;
   PState.SawHeader  = false;
   PState.SawBgnLib  = false;
   PState.SawEndLib  = false;
   while( PState.SawEndLib == false )
    { 
      // try to read the record
      GDSIIRecord *Record=ReadGDSIIRecord(f, &ErrMsg);
      if (ErrMsg)
       return;

      // try to process the record if a handler is present)
      State.NumRecords++;
      RecordHandler Handler = RecordTypes[Record->RType].Handler;
      if ( Handler )
       ErrMsg = Handler(Record, &PState);
      if (ErrMsg)
       return;

      // write logging info 
      string *RStr = GetRecordDescription(Record);
      if (RawLogFile)
       fprintf("Record %i: %s\n",NumRecords,RStr->c_str());
      delete RStr;

      DestroyGDSIIRecord(Record);
    };
   fclose(f);

   if (ErrMsg==0)
    printf("Read %i data records from file %s.\n",Data.NumRecords,FileName.c_str());

 }

/***************************************************************/
/* GDSIIData constructor: create a new GDSIIData instance from */
/* a binary GDSII file.                                        */
/***************************************************************/
GDSIIData::GDSIIData(const string FileName)
 { 
   // initialize class data
   LibName   = 0;
   Unit      = 1.0e-6;
   FileUnits[0]=FileUnits=[1]=0.0;

   ReadGDSIIFile(FileName);
   if (ErrMsg) return;
 }

/***************************************************************/
/* non-class utility method to print a raw dump of all data    */
/* records in a GDSII file                                     */
/***************************************************************/
bool DumpGDSIIFile(const char *FileName)
{
  FILE *f=fopen(FileName,"r");
  if (!f)
   { fprintf(stderr,"error: could not open %s (aborting)\n",FileName);
     return false;
   };

  /*--------------------------------------------------------------*/
  /*- read records one at a time ---------------------------------*/
  /*--------------------------------------------------------------*/
  int NumRecords=0;
  bool Done=false;
  while(!Done)
   { 
     string *ErrMsg=0;
     GDSIIRecord *Record=ReadGDSIIRecord(f, &ErrMsg);
     if (ErrMsg)
      { fprintf(stderr,"error: %s (aborting)\n",ErrMsg->c_str());
        return false;
      };

     string *RStr = GetRecordDescription(Record);
     printf("Record %i: %s\n",NumRecords++,RStr->c_str());
     delete RStr;

     if (Record->RType==RTYPE_ENDLIB)
      Done=true;

     DestroyGDSIIRecord(Record);
   };
  fclose(f);

  printf("Read %i data records from file %s.\n",NumRecords,FileName);
  return true;
}

} // namespace libGSDII
