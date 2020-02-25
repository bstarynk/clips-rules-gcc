   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/01/16            */
   /*                                                     */
   /*          LOGICAL DEPENDENCIES HEADER FILE           */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provide support routines for managing truth      */
/*   CL_maintenance using the logical conditional element.      */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed LOGICAL_DEPENDENCIES compilation flag. */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Rule with exists CE has incorrect activation.  */
/*            DR0867                                         */
/*                                                           */
/*      6.30: Added support for hashed memories.             */
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

#ifndef _H_lgcldpnd

#pragma once

#define _H_lgcldpnd

struct dependency
{
  void *dPtr;
  struct dependency *next;
};

#include "entities.h"
#include "match.h"

bool CL_AddLogical_Dependencies (Environment *, PatternEntity *, bool);
void RemoveEntity_Dependencies (Environment *, PatternEntity *);
void RemovePM_Dependencies (Environment *, PartialMatch *);
void DestroyPM_Dependencies (Environment *, PartialMatch *);
void CL_RemoveLogicalSupport (Environment *, PartialMatch *);
void CL_ForceLogical_Retractions (Environment *);
void CL_Dependencies (Environment *, PatternEntity *);
void CL_Dependents (Environment *, PatternEntity *);
void CL_DependenciesCommand (Environment *, UDFContext *, UDFValue *);
void CL_DependentsCommand (Environment *, UDFContext *, UDFValue *);
void ReturnEntity_Dependencies (Environment *, PatternEntity *);
PartialMatch *CL_FindLogicalBind (struct joinNode *, PartialMatch *);

#endif /* _H_lgcldpnd */
