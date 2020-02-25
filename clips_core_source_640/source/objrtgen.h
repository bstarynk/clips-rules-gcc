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
/*      6.24: Converted INSTANCE_PATTERN_MATCHING to         */
/*            DEFRULE_CONSTRUCT.                             */
/*                                                           */
/*      6.30: Added support for hashed memories and other    */
/*            join network changes.                          */
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

#ifndef _H_objrtgen

#pragma once

#define _H_objrtgen

#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM && (! RUN_TIME) && (! BLOAD_ONLY)

#include "expressn.h"
#include "reorder.h"

void CL_ReplaceGetJNObjectValue (Environment *, Expression *,
				 struct lhsParseNode *, int);
Expression *CL_GenGetJNObjectValue (Environment *, struct lhsParseNode *,
				    int);
Expression *CL_ObjectJNVariableComparison (Environment *,
					   struct lhsParseNode *,
					   struct lhsParseNode *, bool);
Expression *CL_GenObjectPNConstantCompare (Environment *,
					   struct lhsParseNode *);
void CL_ReplaceGetPNObjectValue (Environment *, Expression *,
				 struct lhsParseNode *);
Expression *CL_GenGetPNObjectValue (Environment *, struct lhsParseNode *);
Expression *CL_ObjectPNVariableComparison (Environment *,
					   struct lhsParseNode *,
					   struct lhsParseNode *);
void CL_GenObjectLengthTest (Environment *, struct lhsParseNode *);
void CL_GenObjectZeroLengthTest (Environment *, struct lhsParseNode *);

#endif /* DEFRULE_CONSTRUCT && OBJECT_SYSTEM && (! RUN_TIME) && (! BLOAD_ONLY) */

#endif /* _H_objrtgen */
