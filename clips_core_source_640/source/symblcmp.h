   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/01/16            */
   /*                                                     */
   /*        SYMBOL_TYPE CONSTRUCT COMPILER HEADER FILE        */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the constructs-to-c feature for       */
/*   atomic data values: symbols, integers, floats, and      */
/*   bit maps.                                               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added environment parameter to CL_GenClose.       */
/*                                                           */
/*            Corrected code to remove compiler warnings.    */
/*                                                           */
/*      6.30: Added support for path name argument to        */
/*            constructs-to-c.                               */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#ifndef _H_symblcmp

#pragma once

#define _H_symblcmp

#include <stdio.h>

#include "symbol.h"

void CL_PrintSymbolReference (Environment *, FILE *, CLIPSLexeme *);
void CL_PrintFloatReference (Environment *, FILE *, CLIPSFloat *);
void CL_PrintIntegerReference (Environment *, FILE *, CLIPSInteger *);
void CL_PrintBitMapReference (Environment *, FILE *, CLIPSBitMap *);
void CL_AtomicValuesToCode (Environment *, const char *, const char *,
			    char *);

#endif /* _H_symblcmp */
