   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/18/16            */
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
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Changed integer type/precision.                */
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
/*            UDF redesign.                                  */
/*                                                           */
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*************************************************************/

#ifndef _H_insmult

#pragma once

#define _H_insmult

#include "evaluatn.h"

void CL_SetupInstanceMultifieldCommands (Environment *);
void CL_MVSlotReplaceCommand (Environment *, UDFContext *, UDFValue *);
void CL_MVSlotInsertCommand (Environment *, UDFContext *, UDFValue *);
void CL_MVSlotDeleteCommand (Environment *, UDFContext *, UDFValue *);
void CL_DirectMVReplaceCommand (Environment *, UDFContext *, UDFValue *);
void CL_DirectMVInsertCommand (Environment *, UDFContext *, UDFValue *);
void CL_DirectMVDeleteCommand (Environment *, UDFContext *, UDFValue *);

#endif /* _H_insmult */
