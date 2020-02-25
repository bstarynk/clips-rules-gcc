   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/30/16            */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            If the last construct in a loaded file is a    */
/*            deffunction or defmethod with no closing right */
/*            parenthesis, an error should be issued, but is */
/*            not. DR0872                                    */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            CL_GetConstructNameAndComment API change.         */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Used CL_gensprintf instead of sprintf.            */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Fixed linkage issue when BLOAD_AND_SAVE        */
/*            compiler flag is set to 0.                     */
/*                                                           */
/*            Fixed typing issue when OBJECT_SYSTEM          */
/*            compiler flag is set to 0.                     */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#ifndef _H_genrcpsr

#pragma once

#define _H_genrcpsr

#if DEFGENERIC_CONSTRUCT && (! BLOAD_ONLY) && (! RUN_TIME)

#include "genrcfun.h"

bool CL_ParseDefgeneric (Environment *, const char *);
bool CL_ParseDefmethod (Environment *, const char *);
Defmethod *CL_AddMethod (Environment *, Defgeneric *, Defmethod *, int,
			 unsigned short, Expression *, unsigned short,
			 unsigned short, CLIPSLexeme *, Expression *, char *,
			 bool);
void CL_PackRestrictionTypes (Environment *, RESTRICTION *, Expression *);
void CL_DeleteTempRestricts (Environment *, Expression *);
Defmethod *CL_FindMethodByRestrictions (Defgeneric *, Expression *, int,
					CLIPSLexeme *, int *);

#endif /* DEFGENERIC_CONSTRUCT && (! BLOAD_ONLY) && (! RUN_TIME) */

#endif /* _H_genrcpsr */
