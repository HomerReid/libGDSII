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
 * ReadGDSIIFile.cc -- GDSII file reader for libGDSII
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

typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned long  DWORD;

/***************************************************************/
/* some data structures used in this file only *****************/
/***************************************************************/

/*--------------------------------------------------------------*/
/* storage for a single data record in the GDSII file           */
/*--------------------------------------------------------------*/
typedef struct GDSIIRecord
 {
   BYTE RType; // record type

   // could use a union for the following, but I don't bother
   bool Bits[16];
   iVec iVal;
   dVec dVal;
   string *sVal;
   size_t NumVals;

 } GDSIIRecord;

/*--------------------------------------------------------------*/
/*- 'ParseState' data structure maintained while reading .GDSII */
/*- file, updated after each record is read                     */
/*--------------------------------------------------------------*/
class GDSIIData; // forward reference 
typedef struct ParseState 
 { 
   GDSIIData *Data;
   int NumRecords;
   enum { INITIAL,
          INHEADER,  INLIB,  INSTRUCT, INELEMENT,
          DONE
        } Status;
   GDSIIStruct *CurrentStruct;
   GDSIIElement *CurrentElement;

 } ParseState;

typedef string *(*RecordHandler)(GDSIIRecord Record, ParseState *PState);

const char *ElTypeNames[]=
 {"BOUNDARY", "PATH", "SREF", "AREF", "TEXT", "NODE", "BOX"};

/***************************************************************/
/* Handlers for specific types of data records in GDSII files. */
/***************************************************************/
string *handleHEADER(GDSIIRecord Record, ParseState *PState)
{
  (void) Record;
  if (PState->Status!=ParseState::INITIAL)
   return new string("unexpected record before HEADER");
  PState->Status=ParseState::INHEADER;
  return 0;
}

string *handleBGNLIB(GDSIIRecord Record, ParseState *PState)
{
  (void) Record;
  if (PState->Status!=ParseState::INHEADER)
   return new string("unexpected record BGNLIB");
  PState->Status=ParseState::INLIB;
  return 0;
}

string *handleLIBNAME(GDSIIRecord Record, ParseState *PState)
{ 
  if (PState->Status!=ParseState::INLIB)
   return new string("unexpected record LIBNAME");
  PState->Data->LibName = new string( *(Record.sVal) );
  return 0;
}

string *handleUNITS(GDSIIRecord Record, ParseState *PState)
{ 
  PState->Data->FileUnits[0] = Record.dVal[0];
  PState->Data->FileUnits[1] = Record.dVal[1];
  PState->Data->UnitInMeters =
   PState->Data->FileUnits[1] / PState->Data->FileUnits[0];
  return 0;
}

string *handleENDLIB(GDSIIRecord Record, ParseState *PState)
{ 
  (void) Record;
  if (PState->Status!=ParseState::INLIB)
   return new string("unexpected record ENDLIB");
  PState->Status=ParseState::DONE;
  return 0;
}

string *handleBGNSTR(GDSIIRecord Record, ParseState *PState)
{
  (void) Record;
  if (PState->Status!=ParseState::INLIB)
   return new string("unexpected record BGNSTR");

  // add a new structure
  GDSIIStruct *s  = new GDSIIStruct;
  s->IsReferenced = false;
  s->IsPCell      = false;
  PState->CurrentStruct = s;
  PState->Data->Structs.push_back(s);

  PState->Status=ParseState::INSTRUCT;

  return 0;
}

string *handleSTRNAME(GDSIIRecord Record, ParseState *PState)
{
  if (PState->Status!=ParseState::INSTRUCT)
   return new string("unexpected record STRNAME");
  PState->CurrentStruct->Name = new string( *(Record.sVal) );
  if( strcasestr( Record.sVal->c_str(), "CONTEXT_INFO") )
   PState->CurrentStruct->IsPCell=true;
  return 0;
}

string *handleENDSTR(GDSIIRecord Record, ParseState *PState)
{
  (void) Record;
  if (PState->Status!=ParseState::INSTRUCT)
   return new string("unexpected record ENDSTR");
  PState->Status=ParseState::INLIB;
  return 0;
}

string *handleElement(GDSIIRecord Record, ParseState *PState, ElementType ElType)
{
  (void) Record;
  if (PState->Status!=ParseState::INSTRUCT)
   return new string(   string("unexpected record") + ElTypeNames[ElType] );
  
  // add a new element
  GDSIIElement *e = new GDSIIElement;
  e->Type     = ElType;
  e->Layer    = 0;
  e->DataType = 0;
  e->TextType = 0;
  e->PathType = 0;
  e->SName    = 0;
  e->Width    = 0;
  e->Columns  = 0;
  e->Rows     = 0;
  e->Text     = 0;
  e->Refl     = false;
  e->AbsMag   = false;
  e->AbsAngle = false;
  e->Mag      = 1.0;
  e->Angle    = 0.0;
  e->nsRef    = -1;
  PState->CurrentElement = e;
  PState->CurrentStruct->Elements.push_back(e);

  PState->Status=ParseState::INELEMENT;
  return 0;
}

string *handleBOUNDARY(GDSIIRecord Record, ParseState *PState)
{ return handleElement(Record, PState, BOUNDARY); }

string *handlePATH(GDSIIRecord Record, ParseState *PState)
{ return handleElement(Record, PState, PATH); }

string *handleSREF(GDSIIRecord Record, ParseState *PState)
{ return handleElement(Record, PState, SREF); }

string *handleAREF(GDSIIRecord Record, ParseState *PState)
{ return handleElement(Record, PState, AREF); }

string *handleTEXT(GDSIIRecord Record, ParseState *PState)
{ return handleElement(Record, PState, TEXT); }

string *handleNODE(GDSIIRecord Record, ParseState *PState)
{ return handleElement(Record, PState, NODE); }

string *handleBOX(GDSIIRecord Record, ParseState *PState)
{ return handleElement(Record, PState, BOX); }

string *handleLAYER(GDSIIRecord Record, ParseState *PState)
{
  if (PState->Status!=ParseState::INELEMENT)
   return new string("unexpected record LAYER");
  PState->CurrentElement->Layer = Record.iVal[0];
  PState->Data->LayerSet.insert(Record.iVal[0]);
  
  return 0;
}

string *handleDATATYPE(GDSIIRecord Record, ParseState *PState)
{
  if (PState->Status!=ParseState::INELEMENT)
   return new string("unexpected record DATATYPE");
  PState->CurrentElement->DataType = Record.iVal[0];
  return 0;
}

string *handleTEXTTYPE(GDSIIRecord Record, ParseState *PState)
{ 
  if (    PState->Status!=ParseState::INELEMENT
       || PState->CurrentElement->Type!=TEXT
     )
   return new string("unexpected record TEXTTYPE");
  PState->CurrentElement->TextType = Record.iVal[0];
  return 0;
}

string *handlePATHTYPE(GDSIIRecord Record, ParseState *PState)
{ 
  if (    PState->Status!=ParseState::INELEMENT )
   return new string("unexpected record PATHTYPE");
  PState->CurrentElement->PathType = Record.iVal[0];
  return 0;
}

string *handleSTRANS(GDSIIRecord Record, ParseState *PState)
{ 
  if ( PState->Status!=ParseState::INELEMENT )
   return new string("unexpected record STRANS");
  PState->CurrentElement->Refl     = Record.Bits[0];
  PState->CurrentElement->AbsMag   = Record.Bits[13];
  PState->CurrentElement->AbsAngle = Record.Bits[14];
  return 0;
}

string *handleMAG(GDSIIRecord Record, ParseState *PState)
{ 
  if ( PState->Status!=ParseState::INELEMENT )
   return new string("unexpected record MAG");
  PState->CurrentElement->Mag = Record.dVal[0];
  return 0;
}

string *handleANGLE(GDSIIRecord Record, ParseState *PState)
{ 
  if ( PState->Status!=ParseState::INELEMENT )
   return new string("unexpected record ANGLE");
  PState->CurrentElement->Angle = Record.dVal[0];
  return 0;
}

string *handlePROPATTR(GDSIIRecord Record, ParseState *PState)
{ 
  if ( PState->Status!=ParseState::INELEMENT )
   return new string("unexpected record PROPATTR");
  GDSIIElement *e=PState->CurrentElement;
  e->PropAttrs.push_back(Record.iVal[0]);
  e->PropValues.push_back("");
  return 0;
}

string *handlePROPVALUE(GDSIIRecord Record, ParseState *PState)
{
  if ( PState->Status!=ParseState::INELEMENT )
   return new string("unexpected record PROPVALUE");
  GDSIIElement *e=PState->CurrentElement;
  int n=e->PropAttrs.size();
  if (n==0)
   return new string("PROPVALUE without PROPATTR");
  e->PropValues[n-1]=string( *(Record.sVal) );

  if( strcasestr( Record.sVal->c_str(), "CONTEXT_INFO") )
   PState->CurrentStruct->IsPCell=true;

  return 0;
}

string *handleXY(GDSIIRecord Record, ParseState *PState)
{
  if (PState->Status!=ParseState::INELEMENT)
   return new string("unexpected record XY");
  PState->CurrentElement->XY.reserve(Record.NumVals);
  for(size_t n=0; n<Record.NumVals; n++)
   PState->CurrentElement->XY.push_back(Record.iVal[n]);
  return 0;
}

string *handleSNAME(GDSIIRecord Record, ParseState *PState)
{
  if (PState->Status!=ParseState::INELEMENT)
   return new string("unexpected record SNAME");
  PState->CurrentElement->SName = new string( *(Record.sVal) );
  return 0;
}

string *handleSTRING(GDSIIRecord Record, ParseState *PState)
{
  if (PState->Status!=ParseState::INELEMENT)
   return new string("unexpected record STRING");
  PState->CurrentElement->Text = new string( *(Record.sVal) );
  return 0;
}

string *handleCOLROW(GDSIIRecord Record, ParseState *PState)
{
  if (PState->Status!=ParseState::INELEMENT)
   return new string("unexpected record COLROW");
  PState->CurrentElement->Columns = Record.iVal[0];
  PState->CurrentElement->Rows    = Record.iVal[1];
  return 0;
}

string *handleWIDTH(GDSIIRecord Record, ParseState *PState)
{
  if (PState->Status!=ParseState::INELEMENT)
   return new string("unexpected record Width");
  PState->CurrentElement->Width   = Record.iVal[0];
  return 0;
}

string *handleENDEL(GDSIIRecord Record, ParseState *PState)
{
  (void) Record;
  if (PState->Status!=ParseState::INELEMENT)
   return new string("unexpected record ENDEL");
  PState->Status = ParseState::INSTRUCT;
  return 0;
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
 /*0x09*/  {"PATH",         NO_DATA,     handlePATH},
 /*0x0a*/  {"SREF",         NO_DATA,     handleSREF},
 /*0x0b*/  {"AREF",         NO_DATA,     handleAREF},
 /*0x0c*/  {"TEXT",         NO_DATA,     handleTEXT},
 /*0x0d*/  {"LAYER",        INTEGER_2,   handleLAYER},
 /*0x0e*/  {"DATATYPE",     INTEGER_2,   handleDATATYPE},
 /*0x0f*/  {"WIDTH",        INTEGER_4,   handleWIDTH},
 /*0x10*/  {"XY",           INTEGER_4,   handleXY},
 /*0x11*/  {"ENDEL",        NO_DATA,     handleENDEL},
 /*0x12*/  {"SNAME",        STRING,      handleSNAME},
 /*0x13*/  {"COLROW",       INTEGER_2,   handleCOLROW},
 /*0x14*/  {"TEXTNODE",     NO_DATA,     0},
 /*0x15*/  {"NODE",         NO_DATA,     0},
 /*0x16*/  {"TEXTTYPE",     INTEGER_2,   handleTEXTTYPE},
 /*0x17*/  {"PRESENTATION", BITARRAY,    0},
 /*0x18*/  {"UNUSED",       NO_DATA,     0},
 /*0x19*/  {"STRING",       STRING,      handleSTRING},
 /*0x1a*/  {"STRANS",       BITARRAY,    handleSTRANS},
 /*0x1b*/  {"MAG",          REAL_8,      handleMAG},
 /*0x1c*/  {"ANGLE",        REAL_8,      handleANGLE},
 /*0x1d*/  {"UNUSED",       NO_DATA,     0},
 /*0x1e*/  {"UNUSED",       NO_DATA,     0},
 /*0x1f*/  {"REFLIBS",      STRING,      0},
 /*0x20*/  {"FONTS",        STRING,      0},
 /*0x21*/  {"PATHTYPE",     INTEGER_2,   handlePATHTYPE},
 /*0x22*/  {"GENERATIONS",  INTEGER_2,   0},
 /*0x23*/  {"ATTRTABLE",    STRING,      0},
 /*0x24*/  {"STYPTABLE",    STRING,      0},
 /*0x25*/  {"STRTYPE",      INTEGER_2,   0},
 /*0x26*/  {"ELFLAGS",      BITARRAY,    0},
 /*0x27*/  {"ELKEY",        INTEGER_4,   0},
 /*0x1d*/  {"LINKTYPE",     NO_DATA,     0},
 /*0x1e*/  {"LINKKEYS",     NO_DATA,     0},
 /*0x2a*/  {"NODETYPE",     INTEGER_2,   0},
 /*0x2b*/  {"PROPATTR",     INTEGER_2,   handlePROPATTR},
 /*0x2c*/  {"PROPVALUE",    STRING,      handlePROPVALUE},
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
/***************************************************************/
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


// The allowed characters are [a-zA-Z?$_].
// Non-allowed characters at the end of the string are removed.
// Non-allowed characters not at the end of the string are converted to underscores.
bool IsAllowedChar(char c)
{ c=tolower(c);
  return ('a' <= c && c <= 'z') || c=='$' || c=='_' || c=='?';
}

string *MakeGDSIIString(char *Original, int Size)
{ 
  if (Size==0) return new string("");

  if (Size>32) Size=32;
  char RawString[33];
  strncpy(RawString, Original, Size);
  RawString[Size]=0;
  int L = strlen(RawString);
  while ( L>0 && !IsAllowedChar(RawString[L-1]) )
   RawString[--L] = 0;
  for(int n=0; n<L; n++) 
   if (!IsAllowedChar(RawString[n])) RawString[n]='_';
  return new string(RawString);
}

/***************************************************************/
/* read a single GDSII data record from the current file position */
/***************************************************************/
GDSIIRecord ReadGDSIIRecord(FILE *f, string **ErrMsg)
{
  /*--------------------------------------------------------------*/
  /* read the 4-byte file header and check that the data type     */
  /* agrees with what it should be based on the record type       */
  /*--------------------------------------------------------------*/
  BYTE Header[4];
  if ( 4 != fread(Header, 1, 4, f) )
   { *ErrMsg = new string("unexpected end of file");
     return GDSIIRecord(); // end of file
   }

  size_t RecordSize = Header[0]*256 + Header[1];
  BYTE RType        = Header[2];
  BYTE DType        = Header[3];
  
  if (RType > MAX_RTYPE)
   { *ErrMsg = new string("unknown record type");
     return GDSIIRecord();
   }
    
  if ( DType != RecordTypes[RType].DType )
   { ostringstream ss;
     ss << RecordTypes[RType].Name
        << ": data type disagrees with record type ("
        << DType
        << " != "
        << RecordTypes[RType].DType
        << ")";
     *ErrMsg = new string(ss.str());
     return GDSIIRecord();
   }

  /*--------------------------------------------------------------*/
  /*- attempt to read payload ------------------------------------*/
  /*--------------------------------------------------------------*/
  size_t PayloadSize = RecordSize - 4;
  BYTE *Payload=0;
  if (PayloadSize>0)
   { Payload = new BYTE[PayloadSize];
     if (Payload==0)
      { *ErrMsg = new string("out of memory");
        return GDSIIRecord();
      }
     if ( PayloadSize != fread((void *)Payload, 1, PayloadSize, f) )
      { delete[] Payload;
        *ErrMsg = new string("unexpected end of file");
        return GDSIIRecord();
      }
   }
 
  /*--------------------------------------------------------------*/
  /* allocate space for the record and process payload data       */
  /*--------------------------------------------------------------*/
  GDSIIRecord Record;
  Record.RType   = RType;
  Record.NumVals = 0;
  Record.sVal    = 0;

  switch(DType)
   { case NO_DATA:
       break;

     case BITARRAY:
      { Record.NumVals=1;
        WORD W = *(WORD *)Payload;
        for(unsigned nf=0, Flag=1; nf<16; nf++, Flag*=2)
         Record.Bits[nf] = (W & Flag);
      };
     break;

     case STRING:
      Record.NumVals=1;
      Record.sVal = MakeGDSIIString( (char *)Payload, PayloadSize );
      break;

     case INTEGER_2:
     case INTEGER_4:
      { size_t DataSize = (DType==INTEGER_2) ? 2 : 4;
        Record.NumVals  = PayloadSize / DataSize;
        BYTE *B=(BYTE *)Payload; 
        for(size_t nv=0; nv<Record.NumVals; nv++, B+=DataSize)
         Record.iVal.push_back( ConvertInt(B, RecordTypes[RType].DType) );
      };
     break;

     case REAL_4:
     case REAL_8:
      { size_t DataSize  = (DType==REAL_4) ? 4 : 8;
        Record.NumVals   = PayloadSize / DataSize;
        BYTE *B=(BYTE *)Payload; 
        for(size_t nv=0; nv<Record.NumVals; nv++, B+=DataSize)
         Record.dVal.push_back(ConvertReal(B, RecordTypes[RType].DType));
      };
     break;

     default:
       *ErrMsg = new string("unknown data type " + DType);
       return GDSIIRecord();
   };

  // success 
  *ErrMsg=0;
  delete[] Payload;
  return Record;

}

/***************************************************************/
/* get string description of GDSII record   ********************/
/***************************************************************/
string *GetRecordDescription(GDSIIRecord Record, bool Verbose=true)
{
  char Name[15];
  sprintf(Name,"%12s",RecordTypes[Record.RType].Name);
  ostringstream ss;
  ss << Name;

  if (Record.NumVals>0)
   ss << " ( " << Record.NumVals << ") ";
  
  if (!Verbose)
   return new string(ss.str());

  ss << " = ";
  switch(RecordTypes[Record.RType].DType)
   { 
     case INTEGER_2:    
     case INTEGER_4:    
      for(size_t nv=0; nv<Record.NumVals; nv++)
       ss << Record.iVal[nv] << " ";
      break;

     case REAL_4:
     case REAL_8:
      for(size_t nv=0; nv<Record.NumVals; nv++)
       ss << Record.dVal[nv] << " ";
      break;

     case BITARRAY:
      for(size_t n=0; n<16; n++)
       ss << (Record.Bits[n] ? '1' : '0');
      break;

     case STRING:
      if (Record.sVal)
       ss << *(Record.sVal);
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
void InitializeParseState(ParseState *PState, GDSIIData *Data)
{
  PState->Data           = Data;
  PState->NumRecords     = 0;
  PState->CurrentStruct  = 0;
  PState->CurrentElement = 0;
  PState->Status         = ParseState::INITIAL;
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
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
    }

   /*--------------------------------------------------------------*/
   /*- read records one at a time until we hit ENDLIB              */
   /*--------------------------------------------------------------*/
   ParseState PState;
   InitializeParseState(&PState, this);
   while( PState.Status != ParseState::DONE && !ErrMsg )
    { 
      // try to read the record
      GDSIIRecord Record=ReadGDSIIRecord(f, &ErrMsg);
      if (ErrMsg)
       return;

      // try to process the record if a handler is present
      PState.NumRecords++;
      RecordHandler Handler = RecordTypes[Record.RType].Handler;
      if ( Handler )
       ErrMsg = Handler(Record, &PState);
      else 
       Warn("ignoring unsupported record %s",RecordTypes[Record.RType].Name);
    }
   fclose(f);
   if (ErrMsg) return;
 
   // convert layer set to vector
   for(set<int>::iterator it=LayerSet.begin(); it!=LayerSet.end(); it++)
    Layers.push_back(*it);

   /*--------------------------------------------------------------*/
   /*- Go back through the hierarchy to note which structures are  */
   /*- referenced by others.                                       */
   /*--------------------------------------------------------------*/
   for(size_t ns=0; ns<Structs.size(); ns++)
    for(size_t ne=0; ne<Structs[ns]->Elements.size(); ne++)
     { GDSIIElement *e=Structs[ns]->Elements[ne];
       if(e->Type==SREF || e->Type==AREF)
        { e->nsRef = GetStructByName( *(e->SName) );
          if (e->nsRef==-1)
           Warn("reference to unknown struct %s ",e->SName->c_str());
          else 
           Structs[e->nsRef]->IsReferenced=true;
        }
     }

   /*--------------------------------------------------------------*/
   /*- Flatten hierarchy to obtain simple unstructured lists       */
   /*- of polygons and text labels on each layer.                  */
   /*--------------------------------------------------------------*/
   Flatten();

   //printf("Read %i data records from file %s.\n", PState.NumRecords,FileName.c_str());
}

/***************************************************************/
/* Write text description of GDSII file to FileName.           */
/***************************************************************/
void GDSIIData::WriteDescription(const char *FileName)
{
  FILE *f = (FileName == 0 ? stdout : fopen(FileName,"w") );

  fprintf(f,"*\n");
  fprintf(f,"* File %s: \n",GDSIIFileName->c_str());
  if (LibName)
   fprintf(f,"* Library %s: \n",LibName->c_str());
  fprintf(f,"* Unit=%e meters (file units = {%e,%e})\n",UnitInMeters,FileUnits[0],FileUnits[1]);
  fprintf(f,"*\n");

  fprintf(f,"**************************************************\n");

  fprintf(f,"** Library %s:\n",LibName->c_str());
  fprintf(f,"**************************************************\n");
  for(size_t ns=0; ns<Structs.size(); ns++)
   { 
     GDSIIStruct *s=Structs[ns];
     fprintf(f,"--------------------------------------------------\n");
     fprintf(f,"** Struct %i: %s\n",(int )ns,s->Name->c_str());
     fprintf(f,"--------------------------------------------------\n");

    for(size_t ne=0; ne<s->Elements.size(); ne++)
     { GDSIIElement *e=s->Elements[ne];
       fprintf(f,"  Element %i: %s (layer %i, datatype %i)\n",
                    (int )ne, ElTypeNames[e->Type], e->Layer, e->DataType);
       if (e->Type==PATH || e->Type==TEXT)
        fprintf(f,"    (width %i, pathtype %i)\n",e->Width, e->PathType);
       if (e->Text)
        fprintf(f,"    (text %s)\n",e->Text->c_str());
       if (e->SName)
        fprintf(f,"    (structure %s)\n",e->SName->c_str());
       if (e->Mag!=1.0 || e->Angle!=0.0)
        fprintf(f,"    (mag %g, angle %g)\n",e->Mag,e->Angle);
       if (e->Columns!=0 || e->Rows!=0)          
        fprintf(f,"    (%i x %i array)\n",e->Columns,e->Rows);
       for(size_t n=0; n<e->PropAttrs.size(); n++)
        fprintf(f,"    (attribute %i: %s)\n",e->PropAttrs[n],e->PropValues[n].c_str());
       fprintf(f,"     XY: ");

       for(size_t nxy=0; nxy<e->XY.size(); nxy++)
        fprintf(f,"%i ",e->XY[nxy]);
       fprintf(f,"\n\n");
     }
   }
  if (FileName)
   fclose(f);
}

/***************************************************************/
/* non-class utility method to print a raw dump of all data    */
/* records in a GDSII file                                     */
/***************************************************************/
bool DumpGDSIIFile(const char *GDSIIFileName)
{
  FILE *f=fopen(GDSIIFileName,"r");
  if (!f)
   { fprintf(stderr,"error: could not open %s (aborting)\n",GDSIIFileName);
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
     GDSIIRecord Record=ReadGDSIIRecord(f, &ErrMsg);
     if (ErrMsg)
      { fprintf(stderr,"error: %s (aborting)\n",ErrMsg->c_str());
        return false;
      }

     string *RStr = GetRecordDescription(Record);
     printf("Record %i: %s\n",NumRecords++,RStr->c_str());
     delete RStr;

     if (Record.RType==RTYPE_ENDLIB)
      Done=true;
   }
  fclose(f);

  printf("Read %i data records from file %s.\n",NumRecords,GDSIIFileName);
  return true;
}

} // namespace libGSDII
