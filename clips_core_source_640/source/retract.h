   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/30/16            */
   /*                                                     */
   /*                RETRACT HEADER FILE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose:  Handles join network activity associated with   */
/*   with the removal of a data entity such as a fact or     */
/*   instance.                                               */
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
/*            Added additional developer statistics to help  */
/*            analyze join network perfo_rmance.              */
/*                                                           */
/*            Removed pseudo-facts used in not CEs.          */
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

#ifndef _H_retract

#pragma once

#define _H_retract

#include "match.h"
#include "network.h"

struct rdriveinfo
{
  struct partialMatch *link;
  struct joinNode *jlist;
  struct rdriveinfo *next;
};

void CL_Network_Retract (Environment *, struct patternMatch *);
void CL_ReturnPartialMatch (Environment *, struct partialMatch *);
void CL_DestroyPartialMatch (Environment *, struct partialMatch *);
void CL_FlushGarbagePartial_Matches (Environment *);
void CL_DeletePartial_Matches (Environment *, struct partialMatch *);
void CL_PosEntry_RetractBeta (Environment *, struct partialMatch *,
			      struct partialMatch *, int);
void CL_PosEntry_RetractAlpha (Environment *, struct partialMatch *, int);
bool CL_PartialMatchWillBeDeleted (Environment *, struct partialMatch *);

#endif /* _H_retract */
