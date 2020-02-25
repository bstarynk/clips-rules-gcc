   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/30/16            */
   /*                                                     */
   /*            FACT BLOAD/BSAVE HEADER FILE             */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Added support for hashed alpha memories.       */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#ifndef _H_factbin

#pragma once

#define _H_factbin

#include "factbld.h"

#define FACTBIN_DATA 62

struct factBinaryData
{
  struct factPatternNode *FactPatternArray;
  unsigned long NumberOfPatterns;
};

#define FactBinaryData(theEnv) ((struct factBinaryData *) GetEnvironmentData(theEnv,FACTBIN_DATA))

void CL_FactBinarySetup (Environment *);

#define CL_BsaveFactPatternIndex(patPtr) ((patPtr == NULL) ? ULONG_MAX : ((struct factPatternNode *) patPtr)->bsaveID)
#define CL_BloadFactPatternPointer(i) ((struct factPatternNode *) ((i == ULONG_MAX) ? NULL : &FactBinaryData(theEnv)->FactPatternArray[i]))

#endif
