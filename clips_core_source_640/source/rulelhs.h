   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/30/16            */
   /*                                                     */
   /*           DEFRULE LHS PARSING HEADER FILE           */
   /*******************************************************/

/*************************************************************/
/* Purpose: Coordinates parsing of the LHS conditional       */
/*   elements of a rule.                                     */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Added const qualifiers to remove C++           */
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
/*************************************************************/

#ifndef _H_rulelhs

#pragma once

#define _H_rulelhs

#include "expressn.h"
#include "pattern.h"
#include "reorder.h"
#include "scanner.h"

struct lhsParseNode *CL_ParseRuleLHS (Environment *, const char *,
				      struct token *, const char *, bool *);
void CL_PropagatePatternType (struct lhsParseNode *, struct patternParser *);

#endif
