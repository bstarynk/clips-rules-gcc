   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/30/16            */
   /*                                                     */
   /*              RETE UTILITY HEADER FILE               */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides a set of utility functions useful to    */
/*   other modules.                                          */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed INCREMENTAL_RESET compilation flag.    */
/*                                                           */
/*            Rule with exists CE has incorrect activation.  */
/*            DR0867                                         */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Support for join network changes.              */
/*                                                           */
/*            Support for using an asterick (*) to indicate  */
/*            that existential patterns are matched.         */
/*                                                           */
/*            Support for partial match changes.             */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added support for hashed memories.             */
/*                                                           */
/*            Removed pseudo-facts used in not CEs.          */
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
/*************************************************************/

#ifndef _H_reteutil

#pragma once

#define _H_reteutil

#include "evaluatn.h"
#include "match.h"
#include "network.h"

#define NETWORK_ASSERT  0
#define NETWORK_RETRACT 1

   void                           CL_PrintPartialMatch(Environment *,const char *,struct partialMatch *);
   struct partialMatch           *CL_CopyPartialMatch(Environment *,struct partialMatch *);
   struct partialMatch           *MergePartialCL_Matches(Environment *,struct partialMatch *,struct partialMatch *);
   long                           IncrementPseudoCL_FactIndex(void);
   struct partialMatch           *CL_GetAlphaMemory(Environment *,struct patternNodeHeader *,unsigned long);
   struct partialMatch           *CL_GetLeftBetaMemory(struct joinNode *,unsigned long);
   struct partialMatch           *CL_GetRightBetaMemory(struct joinNode *,unsigned long);
   void                           CL_ReturnLeftMemory(Environment *,struct joinNode *);
   void                           CL_ReturnRightMemory(Environment *,struct joinNode *);
   void                           CL_DestroyBetaMemory(Environment *,struct joinNode *,int);
   void                           CL_FlushBetaMemory(Environment *,struct joinNode *,int);
   bool                           CL_BetaMemoryNotEmpty(struct joinNode *);
   void                           RemoveAlphaMemoryCL_Matches(Environment *,struct patternNodeHeader *,struct partialMatch *,
                                                                  struct alphaMatch *);
   void                           CL_DestroyAlphaMemory(Environment *,struct patternNodeHeader *,bool);
   void                           CL_FlushAlphaMemory(Environment *,struct patternNodeHeader *);
   void                           CL_FlushAlphaBetaMemory(Environment *,struct partialMatch *);
   void                           CL_DestroyAlphaBetaMemory(Environment *,struct partialMatch *);
   int                            CL_GetPatternNumberFromJoin(struct joinNode *);
   struct multifieldMarker       *CL_CopyMultifieldMarkers(Environment *,struct multifieldMarker *);
   struct partialMatch           *CL_CreateAlphaMatch(Environment *,void *,struct multifieldMarker *,
                                                          struct patternNodeHeader *,unsigned long);
   void                           CL_TraceErrorToRule(Environment *,struct joinNode *,const char *);
   void                           CL_InitializePatternHeader(Environment *,struct patternNodeHeader *);
   void                           CL_MarkRuleNetwork(Environment *,bool);
   void                           CL_TagRuleNetwork(Environment *,unsigned long *,unsigned long *,unsigned long *,unsigned long *);
   bool                           CL_FindEntityInPartialMatch(struct patternEntity *,struct partialMatch *);
   unsigned long                  CL_ComputeRightHashValue(Environment *,struct patternNodeHeader *);
   void                           CL_UpdateBetaPMLinks(Environment *,struct partialMatch *,struct partialMatch *,struct partialMatch *,
                                                       struct joinNode *,unsigned long,int);
   void                           CL_UnlinkBetaPMFromNodeAndLineage(Environment *,struct joinNode *,struct partialMatch *,int);
   void                           CL_UnlinkNonLeftLineage(Environment *,struct joinNode *,struct partialMatch *,int);
   struct partialMatch           *CL_CreateEmptyPartialMatch(Environment *);
   void                           CL_MarkRuleJoins(struct joinNode *,bool);
   void                           CL_AddBlockedLink(struct partialMatch *,struct partialMatch *);
   void                           CL_RemoveBlockedLink(struct partialMatch *);
   unsigned long                  CL_PrintBetaMemory(Environment *,const char *,struct betaMemory *,bool,const char *,int);

#endif /* _H_reteutil */



