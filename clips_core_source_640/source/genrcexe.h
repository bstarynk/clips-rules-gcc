   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
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
/*      6.24: Removed IMPERATIVE_METHODS compilation flag.   */
/*                                                           */
/*      6.30: Changed garbage collection algorithm.          */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
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
/*************************************************************/

#ifndef _H_genrcexe

#pragma once

#define _H_genrcexe

#if DEFGENERIC_CONSTRUCT

#include "evaluatn.h"
#include "expressn.h"
#include "genrcfun.h"

void CL_GenericDispatch (Environment *, Defgeneric *, Defmethod *,
			 Defmethod *, Expression *, UDFValue *);
void CL_UnboundMethodErr (Environment *, const char *);
bool CL_IsMethodApplicable (Environment *, Defmethod *);

bool CL_NextMethodP (Environment *);
void CL_NextMethodPCommand (Environment *, UDFContext *, UDFValue *);
void CL_CallNextMethod (Environment *, UDFContext *, UDFValue *);
void CL_CallSpecificMethod (Environment *, UDFContext *, UDFValue *);
void CL_OverrideNextMethod (Environment *, UDFContext *, UDFValue *);

void CL_GetGenericCurrentArgument (Environment *, UDFContext *, UDFValue *);

#endif /* DEFGENERIC_CONSTRUCT */

#endif /* _H_genrcexe */
