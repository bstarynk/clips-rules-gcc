   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/01/16            */
   /*                                                     */
   /*           SYMBOL_TYPE BINARY SAVE HEADER FILE            */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the binary save/load feature for      */
/*    atomic data values.                                    */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#ifndef _H_symblbin

#pragma once

#define _H_symblbin

#include <stdio.h>

#include "symbol.h"

#define BitMapPointer(i) ((CLIPSBitMap *) (SymbolData(theEnv)->BitMapArray[i]))
#define SymbolPointer(i) ((CLIPSLexeme *) (SymbolData(theEnv)->SymbolArray[i]))
#define FloatPointer(i) ((CLIPSFloat *) (SymbolData(theEnv)->FloatArray[i]))
#define IntegerPointer(i) ((CLIPSInteger *) (SymbolData(theEnv)->IntegerArray[i]))

   void                    MarkNeededAtomicValues(Environment);
   void                    CL_WriteNeededAtomicValues(Environment *,FILE *);
   void                    CL_ReadNeededAtomicValues(Environment *);
   void                    CL_InitAtomicValueNeededFlags(Environment *);
   void                    CL_FreeAtomicValueStorage(Environment *);
   void                    CL_WriteNeededSymbols(Environment *,FILE *);
   void                    CL_WriteNeededFloats(Environment *,FILE *);
   void                    CL_WriteNeededIntegers(Environment *,FILE *);
   void                    CL_ReadNeededSymbols(Environment *);
   void                    CL_ReadNeededFloats(Environment *);
   void                    CL_ReadNeededIntegers(Environment *);

#endif /* _H_symblbin */



