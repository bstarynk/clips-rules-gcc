   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/28/17             */
   /*                                                     */
   /*                 RETE UTILITY MODULE                 */
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
/*      6.31: Bug fix to prevent rule activations for        */
/*            partial matches being deleted.                 */
/*                                                           */
/*      6.40: Added Env prefix to CL_Get_HaltExecution and       */
/*            Set_HaltExecution functions.                    */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Incremental reset is always enabled.           */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#include <stdio.h>

#include "setup.h"

#if DEFRULE_CONSTRUCT

#include "drive.h"
#include "engine.h"
#include "envrnmnt.h"
#include "incrrset.h"
#include "match.h"
#include "memalloc.h"
#include "moduldef.h"
#include "pattern.h"
#include "prntutil.h"
#include "retract.h"
#include "router.h"
#include "rulecom.h"

#include "reteutil.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void CL_TraceErrorToRuleDriver (Environment *, struct joinNode *,
				       const char *, int, bool);
static struct alphaMemoryHash *FindAlphaMemory (Environment *,
						struct patternNodeHeader *,
						unsigned long);
static unsigned long AlphaMemoryHashValue (struct patternNodeHeader *,
					   unsigned long);
static void UnlinkAlphaMemory (Environment *, struct patternNodeHeader *,
			       struct alphaMemoryHash *);
static void UnlinkAlphaMemoryBucketSiblings (Environment *,
					     struct alphaMemoryHash *);
static void InitializePMLinks (struct partialMatch *);
static void UnlinkBetaPartialMatchfromAlphaAndBetaLineage (struct partialMatch
							   *);
static int CountPriorPatterns (struct joinNode *);
static void ResizeBetaMemory (Environment *, struct betaMemory *);
static void CL_ResetBetaMemory (Environment *, struct betaMemory *);
#if (CONSTRUCT_COMPILER || BLOAD_AND_BSAVE) && (! RUN_TIME)
static void TagNetworkTraverseJoins (Environment *, unsigned long *,
				     unsigned long *, struct joinNode *);
#endif

/***********************************************************/
/* CL_PrintPartialMatch: Prints out the list of fact indices  */
/*   and/or instance names associated with a partial match */
/*   or rule instantiation.                                */
/***********************************************************/
void
CL_PrintPartialMatch (Environment * theEnv,
		      const char *logicalName, struct partialMatch *list)
{
  struct patternEntity *matchingItem;
  unsigned short i;

  for (i = 0; i < list->bcount;)
    {
      if ((get_nth_pm_match (list, i) != NULL) &&
	  (get_nth_pm_match (list, i)->matchingItem != NULL))
	{
	  matchingItem = get_nth_pm_match (list, i)->matchingItem;
	  (*matchingItem->theInfo->base.short_PrintFunction) (theEnv,
							      logicalName,
							      matchingItem);
	}
      else
	{
	  CL_WriteString (theEnv, logicalName, "*");
	}
      i++;
      if (i < list->bcount)
	CL_WriteString (theEnv, logicalName, ",");
    }
}

/**********************************************/
/* CL_CopyPartialMatch:  Copies a partial match. */
/**********************************************/
struct partialMatch *
CL_CopyPartialMatch (Environment * theEnv, struct partialMatch *list)
{
  struct partialMatch *linker;
  unsigned short i;

  linker =
    get_var_struct (theEnv, partialMatch,
		    sizeof (struct genericMatch) * (list->bcount - 1));

  InitializePMLinks (linker);
  linker->betaMemory = true;
  linker->busy = false;
  linker->rhsMemory = false;
  linker->deleting = false;
  linker->bcount = list->bcount;
  linker->hashValue = 0;

  for (i = 0; i < linker->bcount; i++)
    linker->binds[i] = list->binds[i];

  return (linker);
}

/****************************/
/* CL_CreateEmptyPartialMatch: */
/****************************/
struct partialMatch *
CL_CreateEmptyPartialMatch (Environment * theEnv)
{
  struct partialMatch *linker;

  linker = get_struct (theEnv, partialMatch);

  InitializePMLinks (linker);
  linker->betaMemory = true;
  linker->busy = false;
  linker->rhsMemory = false;
  linker->deleting = false;
  linker->bcount = 1;
  linker->hashValue = 0;
  linker->binds[0].gm.theValue = NULL;

  return (linker);
}

/**********************/
/* InitializePMLinks: */
/**********************/
static void
InitializePMLinks (struct partialMatch *theMatch)
{
  theMatch->nextInMemory = NULL;
  theMatch->prevInMemory = NULL;
  theMatch->nextRightChild = NULL;
  theMatch->prevRightChild = NULL;
  theMatch->nextLeftChild = NULL;
  theMatch->prevLeftChild = NULL;
  theMatch->children = NULL;
  theMatch->rightParent = NULL;
  theMatch->leftParent = NULL;
  theMatch->blockList = NULL;
  theMatch->nextBlocked = NULL;
  theMatch->prevBlocked = NULL;
  theMatch->marker = NULL;
  theMatch->dependents = NULL;
}

/**********************/
/* CL_UpdateBetaPMLinks: */
/**********************/
void
CL_UpdateBetaPMLinks (Environment * theEnv,
		      struct partialMatch *thePM,
		      struct partialMatch *lhsBinds,
		      struct partialMatch *rhsBinds,
		      struct joinNode *join,
		      unsigned long hashValue, int side)
{
  unsigned long betaLocation;
  struct betaMemory *theMemory;

  if (side == LHS)
    {
      theMemory = join->leftMemory;
      thePM->rhsMemory = false;
    }
  else
    {
      theMemory = join->rightMemory;
      thePM->rhsMemory = true;
    }

  thePM->hashValue = hashValue;

   /*================================*/
  /* Update the node's linked list. */
   /*================================*/

  betaLocation = hashValue % theMemory->size;

  if (side == LHS)
    {
      thePM->nextInMemory = theMemory->beta[betaLocation];
      if (theMemory->beta[betaLocation] != NULL)
	{
	  theMemory->beta[betaLocation]->prevInMemory = thePM;
	}
      theMemory->beta[betaLocation] = thePM;
    }
  else
    {
      if (theMemory->last[betaLocation] != NULL)
	{
	  theMemory->last[betaLocation]->nextInMemory = thePM;
	  thePM->prevInMemory = theMemory->last[betaLocation];
	}
      else
	{
	  theMemory->beta[betaLocation] = thePM;
	}

      theMemory->last[betaLocation] = thePM;
    }

  theMemory->count++;
  if (side == LHS)
    {
      join->memoryLeftAdds++;
    }
  else
    {
      join->memoryRightAdds++;
    }

  thePM->owner = join;

   /*======================================*/
  /* Update the alpha memory linked list. */
   /*======================================*/

  if (rhsBinds != NULL)
    {
      thePM->nextRightChild = rhsBinds->children;
      if (rhsBinds->children != NULL)
	{
	  rhsBinds->children->prevRightChild = thePM;
	}
      rhsBinds->children = thePM;
      thePM->rightParent = rhsBinds;
    }

   /*=====================================*/
  /* Update the beta memory linked list. */
   /*=====================================*/

  if (lhsBinds != NULL)
    {
      thePM->nextLeftChild = lhsBinds->children;
      if (lhsBinds->children != NULL)
	{
	  lhsBinds->children->prevLeftChild = thePM;
	}
      lhsBinds->children = thePM;
      thePM->leftParent = lhsBinds;
    }

  if (!DefruleData (theEnv)->BetaMemoryResizingFlag)
    {
      return;
    }

  if ((theMemory->size > 1) && (theMemory->count > (theMemory->size * 11)))
    {
      ResizeBetaMemory (theEnv, theMemory);
    }
}

/**********************************************************/
/* CL_AddBlockedLink: Adds a link between a partial match in */
/*   the beta memory of a join (with a negated RHS) and a */
/*   partial match in its right memory that prevents the  */
/*   partial match from being satisfied and propagated to */
/*   the next join in the rule.                           */
/**********************************************************/
void
CL_AddBlockedLink (struct partialMatch *thePM, struct partialMatch *rhsBinds)
{
  thePM->marker = rhsBinds;
  thePM->nextBlocked = rhsBinds->blockList;
  if (rhsBinds->blockList != NULL)
    {
      rhsBinds->blockList->prevBlocked = thePM;
    }
  rhsBinds->blockList = thePM;
}

/*************************************************************/
/* CL_RemoveBlockedLink: Removes a link between a partial match */
/*   in the beta memory of a join (with a negated RHS) and a */
/*   partial match in its right memory that prevents the     */
/*   partial match from being satisfied and propagated to    */
/*   the next join in the rule.                              */
/*************************************************************/
void
CL_RemoveBlockedLink (struct partialMatch *thePM)
{
  struct partialMatch *blocker;

  if (thePM->prevBlocked == NULL)
    {
      blocker = (struct partialMatch *) thePM->marker;
      blocker->blockList = thePM->nextBlocked;
    }
  else
    {
      thePM->prevBlocked->nextBlocked = thePM->nextBlocked;
    }

  if (thePM->nextBlocked != NULL)
    {
      thePM->nextBlocked->prevBlocked = thePM->prevBlocked;
    }

  thePM->nextBlocked = NULL;
  thePM->prevBlocked = NULL;
  thePM->marker = NULL;
}

/***********************************/
/* CL_UnlinkBetaPMFromNodeAndLineage: */
/***********************************/
void
CL_UnlinkBetaPMFromNodeAndLineage (Environment * theEnv,
				   struct joinNode *join,
				   struct partialMatch *thePM, int side)
{
  unsigned long betaLocation;
  struct betaMemory *theMemory;

  if (side == LHS)
    {
      theMemory = join->leftMemory;
    }
  else
    {
      theMemory = join->rightMemory;
    }

   /*=============================================*/
  /* Update the nextInMemory/prevInMemory links. */
   /*=============================================*/

  theMemory->count--;

  if (side == LHS)
    {
      join->memoryLeftDeletes++;
    }
  else
    {
      join->memoryRightDeletes++;
    }

  betaLocation = thePM->hashValue % theMemory->size;

  if ((side == RHS) && (theMemory->last[betaLocation] == thePM))
    {
      theMemory->last[betaLocation] = thePM->prevInMemory;
    }

  if (thePM->prevInMemory == NULL)
    {
      betaLocation = thePM->hashValue % theMemory->size;
      theMemory->beta[betaLocation] = thePM->nextInMemory;
    }
  else
    {
      thePM->prevInMemory->nextInMemory = thePM->nextInMemory;
    }

  if (thePM->nextInMemory != NULL)
    {
      thePM->nextInMemory->prevInMemory = thePM->prevInMemory;
    }

  thePM->nextInMemory = NULL;
  thePM->prevInMemory = NULL;

  UnlinkBetaPartialMatchfromAlphaAndBetaLineage (thePM);

  if (!DefruleData (theEnv)->BetaMemoryResizingFlag)
    {
      return;
    }

  if ((theMemory->count == 0) && (theMemory->size > 1))
    {
      CL_ResetBetaMemory (theEnv, theMemory);
    }
}

/*************************/
/* CL_UnlinkNonLeftLineage: */
/*************************/
void
CL_UnlinkNonLeftLineage (Environment * theEnv,
			 struct joinNode *join,
			 struct partialMatch *thePM, int side)
{
  unsigned long betaLocation;
  struct betaMemory *theMemory;
  struct partialMatch *tempPM;

  if (side == LHS)
    {
      theMemory = join->leftMemory;
    }
  else
    {
      theMemory = join->rightMemory;
    }

   /*=============================================*/
  /* Update the nextInMemory/prevInMemory links. */
   /*=============================================*/

  theMemory->count--;

  if (side == LHS)
    {
      join->memoryLeftDeletes++;
    }
  else
    {
      join->memoryRightDeletes++;
    }

  betaLocation = thePM->hashValue % theMemory->size;

  if ((side == RHS) && (theMemory->last[betaLocation] == thePM))
    {
      theMemory->last[betaLocation] = thePM->prevInMemory;
    }

  if (thePM->prevInMemory == NULL)
    {
      betaLocation = thePM->hashValue % theMemory->size;
      theMemory->beta[betaLocation] = thePM->nextInMemory;
    }
  else
    {
      thePM->prevInMemory->nextInMemory = thePM->nextInMemory;
    }

  if (thePM->nextInMemory != NULL)
    {
      thePM->nextInMemory->prevInMemory = thePM->prevInMemory;
    }

   /*=========================*/
  /* Update the alpha lists. */
   /*=========================*/

  if (thePM->prevRightChild == NULL)
    {
      if (thePM->rightParent != NULL)
	{
	  thePM->rightParent->children = thePM->nextRightChild;
	  if (thePM->nextRightChild != NULL)
	    {
	      thePM->rightParent->children = thePM->nextRightChild;
	      thePM->nextRightChild->rightParent = thePM->rightParent;
	    }
	}
    }
  else
    {
      thePM->prevRightChild->nextRightChild = thePM->nextRightChild;
    }

  if (thePM->nextRightChild != NULL)
    {
      thePM->nextRightChild->prevRightChild = thePM->prevRightChild;
    }

   /*===========================*/
  /* Update the blocked lists. */
   /*===========================*/

  if (thePM->prevBlocked == NULL)
    {
      tempPM = (struct partialMatch *) thePM->marker;

      if (tempPM != NULL)
	{
	  tempPM->blockList = thePM->nextBlocked;
	}
    }
  else
    {
      thePM->prevBlocked->nextBlocked = thePM->nextBlocked;
    }

  if (thePM->nextBlocked != NULL)
    {
      thePM->nextBlocked->prevBlocked = thePM->prevBlocked;
    }

  if (!DefruleData (theEnv)->BetaMemoryResizingFlag)
    {
      return;
    }

  if ((theMemory->count == 0) && (theMemory->size > 1))
    {
      CL_ResetBetaMemory (theEnv, theMemory);
    }
}

/*******************************************************************/
/* UnlinkBetaPartialMatchfromAlphaAndBetaLineage: Removes the      */
/*   lineage links from a beta memory partial match. This removes  */
/*   the links between this partial match and its left and right   */
/*   memory parents. It also removes the links between this        */
/*   partial match and any of its children in other beta memories. */
/*******************************************************************/
static void
UnlinkBetaPartialMatchfromAlphaAndBetaLineage (struct partialMatch *thePM)
{
  struct partialMatch *tempPM;

   /*=========================*/
  /* Update the alpha lists. */
   /*=========================*/

  if (thePM->prevRightChild == NULL)
    {
      if (thePM->rightParent != NULL)
	{
	  thePM->rightParent->children = thePM->nextRightChild;
	}
    }
  else
    {
      thePM->prevRightChild->nextRightChild = thePM->nextRightChild;
    }

  if (thePM->nextRightChild != NULL)
    {
      thePM->nextRightChild->prevRightChild = thePM->prevRightChild;
    }

  thePM->rightParent = NULL;
  thePM->nextRightChild = NULL;
  thePM->prevRightChild = NULL;

   /*========================*/
  /* Update the beta lists. */
   /*========================*/

  if (thePM->prevLeftChild == NULL)
    {
      if (thePM->leftParent != NULL)
	{
	  thePM->leftParent->children = thePM->nextLeftChild;
	}
    }
  else
    {
      thePM->prevLeftChild->nextLeftChild = thePM->nextLeftChild;
    }

  if (thePM->nextLeftChild != NULL)
    {
      thePM->nextLeftChild->prevLeftChild = thePM->prevLeftChild;
    }

  thePM->leftParent = NULL;
  thePM->nextLeftChild = NULL;
  thePM->prevLeftChild = NULL;

   /*===========================*/
  /* Update the blocked lists. */
   /*===========================*/

  if (thePM->prevBlocked == NULL)
    {
      tempPM = (struct partialMatch *) thePM->marker;

      if (tempPM != NULL)
	{
	  tempPM->blockList = thePM->nextBlocked;
	}
    }
  else
    {
      thePM->prevBlocked->nextBlocked = thePM->nextBlocked;
    }

  if (thePM->nextBlocked != NULL)
    {
      thePM->nextBlocked->prevBlocked = thePM->prevBlocked;
    }

  thePM->marker = NULL;
  thePM->nextBlocked = NULL;
  thePM->prevBlocked = NULL;

   /*===============================================*/
  /* Remove parent reference from the child links. */
   /*===============================================*/

  if (thePM->children != NULL)
    {
      if (thePM->rhsMemory)
	{
	  for (tempPM = thePM->children; tempPM != NULL;
	       tempPM = tempPM->nextRightChild)
	    {
	      tempPM->rightParent = NULL;
	    }
	}
      else
	{
	  for (tempPM = thePM->children; tempPM != NULL;
	       tempPM = tempPM->nextLeftChild)
	    {
	      tempPM->leftParent = NULL;
	    }
	}

      thePM->children = NULL;
    }
}

/********************************************************/
/* MergePartial_Matches: Merges two partial matches. The */
/*   second match should either be NULL (indicating a   */
/*   negated CE) or contain a single match.             */
/********************************************************/
struct partialMatch *
MergePartial_Matches (Environment * theEnv,
		      struct partialMatch *lhsBind,
		      struct partialMatch *rhsBind)
{
  struct partialMatch *linker;
  static struct partialMatch mergeTemplate = { 1 };	/* betaMemory is true, re_mainder are 0 or NULL */

   /*=================================*/
  /* Allocate the new partial match. */
   /*=================================*/

  linker =
    get_var_struct (theEnv, partialMatch,
		    sizeof (struct genericMatch) * lhsBind->bcount);

   /*============================================*/
  /* Set the flags to their appropriate values. */
   /*============================================*/

  memcpy (linker, &mergeTemplate,
	  sizeof (struct partialMatch) - sizeof (struct genericMatch));

  linker->deleting = false;
  linker->bcount = lhsBind->bcount + 1;

   /*========================================================*/
  /* Copy the bindings of the partial match being extended. */
   /*========================================================*/

  memcpy (linker->binds, lhsBind->binds,
	  sizeof (struct genericMatch) * lhsBind->bcount);

   /*===================================*/
  /* Add the binding of the rhs match. */
   /*===================================*/

  if (rhsBind == NULL)
    {
      linker->binds[lhsBind->bcount].gm.theValue = NULL;
    }
  else
    {
      linker->binds[lhsBind->bcount].gm.theValue =
	rhsBind->binds[0].gm.theValue;
    }

  return linker;
}

/*******************************************************************/
/* CL_InitializePatternHeader: Initializes a pattern header structure */
/*   (used by the fact and instance pattern matchers).             */
/*******************************************************************/
void
CL_InitializePatternHeader (Environment * theEnv,
			    struct patternNodeHeader *theHeader)
{
#if MAC_XCD
#pragma unused(theEnv)
#endif
  theHeader->firstHash = NULL;
  theHeader->lastHash = NULL;
  theHeader->entryJoin = NULL;
  theHeader->rightHash = NULL;
  theHeader->singlefieldNode = false;
  theHeader->multifieldNode = false;
  theHeader->stopNode = false;
#if (! RUN_TIME)
  theHeader->initialize = true;
#else
  theHeader->initialize = false;
#endif
  theHeader->marked = false;
  theHeader->beginSlot = false;
  theHeader->endSlot = false;
  theHeader->selector = false;
}

/******************************************************************/
/* CL_CreateAlphaMatch: Given a pointer to an entity (such as a fact */
/*   or instance) which matched a pattern, this function creates  */
/*   a partial match suitable for storing in the alpha memory of  */
/*   the pattern network. Note that the multifield markers which  */
/*   are passed as a calling argument are copied (thus the caller */
/*   is still responsible for freeing these data structures).     */
/******************************************************************/
struct partialMatch *
CL_CreateAlphaMatch (Environment * theEnv,
		     void *theEntity,
		     struct multifieldMarker *markers,
		     struct patternNodeHeader *theHeader,
		     unsigned long hashOffset)
{
  struct partialMatch *theMatch;
  struct alphaMatch *afbtemp;
  unsigned long hashValue;
  struct alphaMemoryHash *theAlphaMemory;

   /*==================================================*/
  /* Create the alpha match and intialize its values. */
   /*==================================================*/

  theMatch = get_struct (theEnv, partialMatch);
  InitializePMLinks (theMatch);
  theMatch->betaMemory = false;
  theMatch->busy = false;
  theMatch->deleting = false;
  theMatch->bcount = 1;
  theMatch->hashValue = hashOffset;

  afbtemp = get_struct (theEnv, alphaMatch);
  afbtemp->next = NULL;
  afbtemp->matchingItem = (struct patternEntity *) theEntity;

  if (markers != NULL)
    {
      afbtemp->markers = CL_CopyMultifieldMarkers (theEnv, markers);
    }
  else
    {
      afbtemp->markers = NULL;
    }

  theMatch->binds[0].gm.theMatch = afbtemp;

   /*============================================*/
  /* Find the alpha memory of the pattern node. */
   /*============================================*/

  hashValue = AlphaMemoryHashValue (theHeader, hashOffset);
  theAlphaMemory = FindAlphaMemory (theEnv, theHeader, hashValue);
  afbtemp->bucket = hashValue;

   /*============================================*/
  /* Create an alpha memory if it wasn't found. */
   /*============================================*/

  if (theAlphaMemory == NULL)
    {
      theAlphaMemory = get_struct (theEnv, alphaMemoryHash);
      theAlphaMemory->bucket = hashValue;
      theAlphaMemory->owner = theHeader;
      theAlphaMemory->alphaMemory = NULL;
      theAlphaMemory->endOfQueue = NULL;
      theAlphaMemory->nextHash = NULL;

      theAlphaMemory->next =
	DefruleData (theEnv)->AlphaMemoryTable[hashValue];
      if (theAlphaMemory->next != NULL)
	{
	  theAlphaMemory->next->prev = theAlphaMemory;
	}

      theAlphaMemory->prev = NULL;
      DefruleData (theEnv)->AlphaMemoryTable[hashValue] = theAlphaMemory;

      if (theHeader->firstHash == NULL)
	{
	  theHeader->firstHash = theAlphaMemory;
	  theHeader->lastHash = theAlphaMemory;
	  theAlphaMemory->prevHash = NULL;
	}
      else
	{
	  theHeader->lastHash->nextHash = theAlphaMemory;
	  theAlphaMemory->prevHash = theHeader->lastHash;
	  theHeader->lastHash = theAlphaMemory;
	}
    }

   /*====================================*/
  /* Store the alpha match in the alpha */
  /* memory of the pattern node.        */
   /*====================================*/

  theMatch->prevInMemory = theAlphaMemory->endOfQueue;
  if (theAlphaMemory->endOfQueue == NULL)
    {
      theAlphaMemory->alphaMemory = theMatch;
      theAlphaMemory->endOfQueue = theMatch;
    }
  else
    {
      theAlphaMemory->endOfQueue->nextInMemory = theMatch;
      theAlphaMemory->endOfQueue = theMatch;
    }

   /*===================================================*/
  /* Return a pointer to the newly create alpha match. */
   /*===================================================*/

  return (theMatch);
}

/*******************************************/
/* CL_CopyMultifieldMarkers: Copies a list of */
/*   multifieldMarker data structures.     */
/*******************************************/
struct multifieldMarker *
CL_CopyMultifieldMarkers (Environment * theEnv,
			  struct multifieldMarker *theMarkers)
{
  struct multifieldMarker *head = NULL, *lastMark = NULL, *newMark;

  while (theMarkers != NULL)
    {
      newMark = get_struct (theEnv, multifieldMarker);
      newMark->next = NULL;
      newMark->whichField = theMarkers->whichField;
      newMark->where = theMarkers->where;
      newMark->startPosition = theMarkers->startPosition;
      newMark->range = theMarkers->range;

      if (lastMark == NULL)
	{
	  head = newMark;
	}
      else
	{
	  lastMark->next = newMark;
	}
      lastMark = newMark;

      theMarkers = theMarkers->next;
    }

  return (head);
}

/***************************************************************/
/* CL_FlushAlphaBetaMemory: Returns all partial matches in a list */
/*   of partial matches either directly to the pool of free    */
/*   memory or to the list of GarbagePartial_Matches. Partial   */
/*   matches stored in alpha memories must be placed on the    */
/*   list of GarbagePartial_Matches.                            */
/***************************************************************/
void
CL_FlushAlphaBetaMemory (Environment * theEnv, struct partialMatch *pfl)
{
  struct partialMatch *pfltemp;

  while (pfl != NULL)
    {
      pfltemp = pfl->nextInMemory;

      UnlinkBetaPartialMatchfromAlphaAndBetaLineage (pfl);
      CL_ReturnPartialMatch (theEnv, pfl);

      pfl = pfltemp;
    }
}

/*****************************************************************/
/* CL_DestroyAlphaBetaMemory: Returns all partial matches in a list */
/*   of partial matches directly to the pool of free memory.     */
/*****************************************************************/
void
CL_DestroyAlphaBetaMemory (Environment * theEnv, struct partialMatch *pfl)
{
  struct partialMatch *pfltemp;

  while (pfl != NULL)
    {
      pfltemp = pfl->nextInMemory;
      CL_DestroyPartialMatch (theEnv, pfl);
      pfl = pfltemp;
    }
}

/******************************************************/
/* CL_FindEntityInPartialMatch: Searches for a specified */
/*   data entity in a partial match.                  */
/******************************************************/
bool
CL_FindEntityInPartialMatch (struct patternEntity *theEntity,
			     struct partialMatch *thePartialMatch)
{
  unsigned short i;

  for (i = 0; i < thePartialMatch->bcount; i++)
    {
      if (thePartialMatch->binds[i].gm.theMatch == NULL)
	continue;
      if (thePartialMatch->binds[i].gm.theMatch->matchingItem == theEntity)
	{
	  return true;
	}
    }

  return false;
}

/***********************************************************************/
/* CL_GetPatternNumberFromJoin: Given a pointer to a join associated with */
/*   a pattern CE, returns an integer representing the position of the */
/*   pattern CE in the rule (e.g. first, second, third).               */
/***********************************************************************/
int
CL_GetPatternNumberFromJoin (struct joinNode *joinPtr)
{
  int whichOne = 0;

  while (joinPtr != NULL)
    {
      if (joinPtr->joinFromTheRight)
	{
	  joinPtr = (struct joinNode *) joinPtr->rightSideEntryStructure;
	}
      else
	{
	  whichOne++;
	  joinPtr = joinPtr->lastLevel;
	}
    }

  return (whichOne);
}

/************************************************************************/
/* CL_TraceErrorToRule: Prints an error message when a error occurs as the */
/*   result of evaluating an expression in the pattern network. Used to */
/*   indicate which rule caused the problem.                            */
/************************************************************************/
void
CL_TraceErrorToRule (Environment * theEnv,
		     struct joinNode *joinPtr, const char *indentSpaces)
{
  int patternCount;

  CL_MarkRuleNetwork (theEnv, 0);

  patternCount = CountPriorPatterns (joinPtr->lastLevel) + 1;

  CL_TraceErrorToRuleDriver (theEnv, joinPtr, indentSpaces, patternCount,
			     false);

  CL_MarkRuleNetwork (theEnv, 0);
}

/**************************************************************/
/* CL_TraceErrorToRuleDriver: Driver code for printing out which */
/*   rule caused a pattern or join network error.             */
/**************************************************************/
static void
CL_TraceErrorToRuleDriver (Environment * theEnv,
			   struct joinNode *joinPtr,
			   const char *indentSpaces,
			   int priorRightJoinPatterns,
			   bool enteredJoinFromRight)
{
  const char *name;
  int priorPatternCount;
  struct joinLink *theLinks;

  if ((joinPtr->joinFromTheRight) && enteredJoinFromRight)
    {
      priorPatternCount = CountPriorPatterns (joinPtr->lastLevel);
    }
  else
    {
      priorPatternCount = 0;
    }

  if (joinPtr->marked)
    {				/* Do Nothing */
    }
  else if (joinPtr->ruleToActivate != NULL)
    {
      joinPtr->marked = 1;
      name = CL_DefruleName (joinPtr->ruleToActivate);
      CL_WriteString (theEnv, STDERR, indentSpaces);

      CL_WriteString (theEnv, STDERR, "Of pattern #");
      CL_WriteInteger (theEnv, STDERR,
		       priorRightJoinPatterns + priorPatternCount);
      CL_WriteString (theEnv, STDERR, " in rule ");
      CL_WriteString (theEnv, STDERR, name);
      CL_WriteString (theEnv, STDERR, "\n");
    }
  else
    {
      joinPtr->marked = 1;

      theLinks = joinPtr->nextLinks;
      while (theLinks != NULL)
	{
	  CL_TraceErrorToRuleDriver (theEnv, theLinks->join, indentSpaces,
				     priorRightJoinPatterns +
				     priorPatternCount,
				     (theLinks->enterDirection == RHS));
	  theLinks = theLinks->next;
	}
    }
}

/***********************/
/* CountPriorPatterns: */
/***********************/
static int
CountPriorPatterns (struct joinNode *joinPtr)
{
  int count = 0;

  while (joinPtr != NULL)
    {
      if (joinPtr->joinFromTheRight)
	{
	  count +=
	    CountPriorPatterns ((struct joinNode *)
				joinPtr->rightSideEntryStructure);
	}
      else
	{
	  count++;
	}

      joinPtr = joinPtr->lastLevel;
    }

  return (count);
}

/********************************************************/
/* CL_MarkRuleNetwork: Sets the marked flag in each of the */
/*   joins in the join network to the specified value.  */
/********************************************************/
void
CL_MarkRuleNetwork (Environment * theEnv, bool value)
{
  Defrule *rulePtr, *disjunctPtr;
  struct joinNode *joinPtr;
  Defmodule *modulePtr;

   /*===========================*/
  /* Loop through each module. */
   /*===========================*/

  CL_SaveCurrentModule (theEnv);
  for (modulePtr = CL_GetNextDefmodule (theEnv, NULL);
       modulePtr != NULL; modulePtr = CL_GetNextDefmodule (theEnv, modulePtr))
    {
      CL_SetCurrentModule (theEnv, modulePtr);

      /*=========================*/
      /* Loop through each rule. */
      /*=========================*/

      rulePtr = CL_GetNextDefrule (theEnv, NULL);
      while (rulePtr != NULL)
	{
	 /*=============================*/
	  /* Mark each join for the rule */
	  /* with the specified value.   */
	 /*=============================*/

	  for (disjunctPtr = rulePtr; disjunctPtr != NULL;
	       disjunctPtr = disjunctPtr->disjunct)
	    {
	      joinPtr = disjunctPtr->lastJoin;
	      CL_MarkRuleJoins (joinPtr, value);
	    }

	 /*===========================*/
	  /* Move on to the next rule. */
	 /*===========================*/

	  rulePtr = CL_GetNextDefrule (theEnv, rulePtr);
	}

    }

  CL_RestoreCurrentModule (theEnv);
}

/******************/
/* CL_MarkRuleJoins: */
/******************/
void
CL_MarkRuleJoins (struct joinNode *joinPtr, bool value)
{
  while (joinPtr != NULL)
    {
      if (joinPtr->joinFromTheRight)
	{
	  CL_MarkRuleJoins ((struct joinNode *)
			    joinPtr->rightSideEntryStructure, value);
	}

      joinPtr->marked = value;
      joinPtr = joinPtr->lastLevel;
    }
}

/*****************************************/
/* CL_GetAlphaMemory: Retrieves the list of */
/*   matches from an alpha memory.       */
/*****************************************/
struct partialMatch *
CL_GetAlphaMemory (Environment * theEnv,
		   struct patternNodeHeader *theHeader,
		   unsigned long hashOffset)
{
  struct alphaMemoryHash *theAlphaMemory;
  unsigned long hashValue;

  hashValue = AlphaMemoryHashValue (theHeader, hashOffset);
  theAlphaMemory = FindAlphaMemory (theEnv, theHeader, hashValue);

  if (theAlphaMemory == NULL)
    {
      return NULL;
    }

  return theAlphaMemory->alphaMemory;
}

/*****************************************/
/* CL_GetLeftBetaMemory: Retrieves the list */
/*   of matches from a beta memory.      */
/*****************************************/
struct partialMatch *
CL_GetLeftBetaMemory (struct joinNode *theJoin, unsigned long hashValue)
{
  unsigned long betaLocation;

  betaLocation = hashValue % theJoin->leftMemory->size;

  return theJoin->leftMemory->beta[betaLocation];
}

/******************************************/
/* CL_GetRightBetaMemory: Retrieves the list */
/*   of matches from a beta memory.       */
/******************************************/
struct partialMatch *
CL_GetRightBetaMemory (struct joinNode *theJoin, unsigned long hashValue)
{
  unsigned long betaLocation;

  betaLocation = hashValue % theJoin->rightMemory->size;

  return theJoin->rightMemory->beta[betaLocation];
}

/***************************************/
/* CL_ReturnLeftMemory: Sets the contents */
/*   of a beta memory to NULL.         */
/***************************************/
void
CL_ReturnLeftMemory (Environment * theEnv, struct joinNode *theJoin)
{
  if (theJoin->leftMemory == NULL)
    return;
  CL_genfree (theEnv, theJoin->leftMemory->beta,
	      sizeof (struct partialMatch *) * theJoin->leftMemory->size);
  rtn_struct (theEnv, betaMemory, theJoin->leftMemory);
  theJoin->leftMemory = NULL;
}

/***************************************/
/* CL_ReturnRightMemory: Sets the contents */
/*   of a beta memory to NULL.         */
/***************************************/
void
CL_ReturnRightMemory (Environment * theEnv, struct joinNode *theJoin)
{
  if (theJoin->rightMemory == NULL)
    return;
  CL_genfree (theEnv, theJoin->rightMemory->beta,
	      sizeof (struct partialMatch *) * theJoin->rightMemory->size);
  CL_genfree (theEnv, theJoin->rightMemory->last,
	      sizeof (struct partialMatch *) * theJoin->rightMemory->size);
  rtn_struct (theEnv, betaMemory, theJoin->rightMemory);
  theJoin->rightMemory = NULL;
}

/****************************************************************/
/* CL_DestroyBetaMemory: Destroys the contents of a beta memory in */
/*   preperation for the deallocation of a join. Destroying is  */
/*   perfo_rmed when the environment is being deallocated and it */
/*   is not necessary to leave the environment in a consistent  */
/*   state (as it would be if just a single rule were being     */
/*   deleted).                                                  */
/****************************************************************/
void
CL_DestroyBetaMemory (Environment * theEnv,
		      struct joinNode *theJoin, int side)
{
  unsigned long i;

  if (side == LHS)
    {
      if (theJoin->leftMemory == NULL)
	return;

      for (i = 0; i < theJoin->leftMemory->size; i++)
	{
	  CL_DestroyAlphaBetaMemory (theEnv, theJoin->leftMemory->beta[i]);
	}
    }
  else
    {
      if (theJoin->rightMemory == NULL)
	return;

      for (i = 0; i < theJoin->rightMemory->size; i++)
	{
	  CL_DestroyAlphaBetaMemory (theEnv, theJoin->rightMemory->beta[i]);
	}
    }
}

/*************************************************************/
/* CL_FlushBetaMemory: Flushes the contents of a beta memory in */
/*   preperation for the deallocation of a join. Flushing    */
/*   is perfo_rmed when the partial matches in the beta       */
/*   memory may still be in use because the environment will */
/*   re_main active.                                          */
/*************************************************************/
void
CL_FlushBetaMemory (Environment * theEnv, struct joinNode *theJoin, int side)
{
  unsigned long i;

  if (side == LHS)
    {
      if (theJoin->leftMemory == NULL)
	return;

      for (i = 0; i < theJoin->leftMemory->size; i++)
	{
	  CL_FlushAlphaBetaMemory (theEnv, theJoin->leftMemory->beta[i]);
	}
    }
  else
    {
      if (theJoin->rightMemory == NULL)
	return;

      for (i = 0; i < theJoin->rightMemory->size; i++)
	{
	  CL_FlushAlphaBetaMemory (theEnv, theJoin->rightMemory->beta[i]);
	}
    }
}

/***********************/
/* CL_BetaMemoryNotEmpty: */
/***********************/
bool
CL_BetaMemoryNotEmpty (struct joinNode *theJoin)
{
  if (theJoin->leftMemory != NULL)
    {
      if (theJoin->leftMemory->count > 0)
	{
	  return true;
	}
    }

  if (theJoin->rightMemory != NULL)
    {
      if (theJoin->rightMemory->count > 0)
	{
	  return true;
	}
    }

  return false;
}

/*********************************************/
/* RemoveAlphaMemory_Matches: Removes matches */
/*   from an alpha memory.                   */
/*********************************************/
void
RemoveAlphaMemory_Matches (Environment * theEnv,
			   struct patternNodeHeader *theHeader,
			   struct partialMatch *theMatch,
			   struct alphaMatch *theAlphaMatch)
{
  struct alphaMemoryHash *theAlphaMemory = NULL;
  unsigned long hashValue;

  if ((theMatch->prevInMemory == NULL) || (theMatch->nextInMemory == NULL))
    {
      hashValue = theAlphaMatch->bucket;
      theAlphaMemory = FindAlphaMemory (theEnv, theHeader, hashValue);
    }

  if (theMatch->prevInMemory != NULL)
    {
      theMatch->prevInMemory->nextInMemory = theMatch->nextInMemory;
    }
  else
    {
      theAlphaMemory->alphaMemory = theMatch->nextInMemory;
    }

  if (theMatch->nextInMemory != NULL)
    {
      theMatch->nextInMemory->prevInMemory = theMatch->prevInMemory;
    }
  else
    {
      theAlphaMemory->endOfQueue = theMatch->prevInMemory;
    }

   /*====================================*/
  /* Add the match to the garbage list. */
   /*====================================*/

  theMatch->nextInMemory = EngineData (theEnv)->GarbagePartial_Matches;
  EngineData (theEnv)->GarbagePartial_Matches = theMatch;

  if ((theAlphaMemory != NULL) && (theAlphaMemory->alphaMemory == NULL))
    {
      UnlinkAlphaMemory (theEnv, theHeader, theAlphaMemory);
    }
}

/***********************/
/* CL_DestroyAlphaMemory: */
/***********************/
void
CL_DestroyAlphaMemory (Environment * theEnv,
		       struct patternNodeHeader *theHeader, bool unlink)
{
  struct alphaMemoryHash *theAlphaMemory, *tempMemory;

  theAlphaMemory = theHeader->firstHash;

  while (theAlphaMemory != NULL)
    {
      tempMemory = theAlphaMemory->nextHash;
      CL_DestroyAlphaBetaMemory (theEnv, theAlphaMemory->alphaMemory);
      if (unlink)
	{
	  UnlinkAlphaMemoryBucketSiblings (theEnv, theAlphaMemory);
	}
      rtn_struct (theEnv, alphaMemoryHash, theAlphaMemory);
      theAlphaMemory = tempMemory;
    }

  theHeader->firstHash = NULL;
  theHeader->lastHash = NULL;
}

/*********************/
/* CL_FlushAlphaMemory: */
/*********************/
void
CL_FlushAlphaMemory (Environment * theEnv,
		     struct patternNodeHeader *theHeader)
{
  struct alphaMemoryHash *theAlphaMemory, *tempMemory;

  theAlphaMemory = theHeader->firstHash;

  while (theAlphaMemory != NULL)
    {
      tempMemory = theAlphaMemory->nextHash;
      CL_FlushAlphaBetaMemory (theEnv, theAlphaMemory->alphaMemory);
      UnlinkAlphaMemoryBucketSiblings (theEnv, theAlphaMemory);
      rtn_struct (theEnv, alphaMemoryHash, theAlphaMemory);
      theAlphaMemory = tempMemory;
    }

  theHeader->firstHash = NULL;
  theHeader->lastHash = NULL;
}

/********************/
/* FindAlphaMemory: */
/********************/
static struct alphaMemoryHash *
FindAlphaMemory (Environment * theEnv,
		 struct patternNodeHeader *theHeader, unsigned long hashValue)
{
  struct alphaMemoryHash *theAlphaMemory;

  theAlphaMemory = DefruleData (theEnv)->AlphaMemoryTable[hashValue];

  if (theAlphaMemory != NULL)
    {
      while ((theAlphaMemory != NULL) && (theAlphaMemory->owner != theHeader))
	{
	  theAlphaMemory = theAlphaMemory->next;
	}
    }

  return theAlphaMemory;
}

/*************************/
/* AlphaMemoryHashValue: */
/*************************/
static unsigned long
AlphaMemoryHashValue (struct patternNodeHeader *theHeader,
		      unsigned long hashOffset)
{
  unsigned long hashValue;
  union
  {
    void *vv;
    unsigned uv;
  } fis;

  fis.uv = 0;
  fis.vv = theHeader;

  hashValue = fis.uv + hashOffset;
  hashValue = hashValue % ALPHA_MEMORY_HASH_SIZE;

  return hashValue;
}

/**********************/
/* UnlinkAlphaMemory: */
/**********************/
static void
UnlinkAlphaMemory (Environment * theEnv,
		   struct patternNodeHeader *theHeader,
		   struct alphaMemoryHash *theAlphaMemory)
{
   /*======================*/
  /* Unlink the siblings. */
   /*======================*/

  UnlinkAlphaMemoryBucketSiblings (theEnv, theAlphaMemory);

   /*================================*/
  /* Update firstHash and lastHash. */
   /*================================*/

  if (theAlphaMemory == theHeader->firstHash)
    {
      theHeader->firstHash = theAlphaMemory->nextHash;
    }

  if (theAlphaMemory == theHeader->lastHash)
    {
      theHeader->lastHash = theAlphaMemory->prevHash;
    }

   /*===============================*/
  /* Update nextHash and prevHash. */
   /*===============================*/

  if (theAlphaMemory->prevHash != NULL)
    {
      theAlphaMemory->prevHash->nextHash = theAlphaMemory->nextHash;
    }

  if (theAlphaMemory->nextHash != NULL)
    {
      theAlphaMemory->nextHash->prevHash = theAlphaMemory->prevHash;
    }

  rtn_struct (theEnv, alphaMemoryHash, theAlphaMemory);
}

/************************************/
/* UnlinkAlphaMemoryBucketSiblings: */
/************************************/
static void
UnlinkAlphaMemoryBucketSiblings (Environment * theEnv,
				 struct alphaMemoryHash *theAlphaMemory)
{
  if (theAlphaMemory->prev == NULL)
    {
      DefruleData (theEnv)->AlphaMemoryTable[theAlphaMemory->bucket] =
	theAlphaMemory->next;
    }
  else
    {
      theAlphaMemory->prev->next = theAlphaMemory->next;
    }

  if (theAlphaMemory->next != NULL)
    {
      theAlphaMemory->next->prev = theAlphaMemory->prev;
    }
}

/**************************/
/* CL_ComputeRightHashValue: */
/**************************/
unsigned long
CL_ComputeRightHashValue (Environment * theEnv,
			  struct patternNodeHeader *theHeader)
{
  struct expr *tempExpr;
  unsigned long hashValue = 0;
  unsigned long multiplier = 1;
  union
  {
    void *vv;
    unsigned long liv;
  } fis;

  if (theHeader->rightHash == NULL)
    {
      return hashValue;
    }

  for (tempExpr = theHeader->rightHash;
       tempExpr != NULL;
       tempExpr = tempExpr->nextArg, multiplier = multiplier * 509)
    {
      UDFValue theResult;
      struct expr *oldArgument;

      oldArgument = CL_EvaluationData (theEnv)->CurrentExpression;
      CL_EvaluationData (theEnv)->CurrentExpression = tempExpr;
      (*CL_EvaluationData (theEnv)->PrimitivesArray[tempExpr->type]->
       evaluateFunction) (theEnv, tempExpr->value, &theResult);
      CL_EvaluationData (theEnv)->CurrentExpression = oldArgument;

      switch (theResult.header->type)
	{
	case STRING_TYPE:
	case SYMBOL_TYPE:
	case INSTANCE_NAME_TYPE:
	  hashValue += (theResult.lexemeValue->bucket * multiplier);
	  break;

	case CL_INTEGER_TYPE:
	  hashValue += (theResult.integerValue->bucket * multiplier);
	  break;

	case FLOAT_TYPE:
	  hashValue += (theResult.floatValue->bucket * multiplier);
	  break;

	case FACT_ADDRESS_TYPE:
#if OBJECT_SYSTEM
	case INSTANCE_ADDRESS_TYPE:
#endif
	  fis.liv = 0;
	  fis.vv = theResult.value;
	  hashValue += fis.liv * multiplier;
	  break;

	case EXTERNAL_ADDRESS_TYPE:
	  fis.liv = 0;
	  fis.vv = theResult.externalAddressValue->contents;
	  hashValue += fis.liv * multiplier;
	  break;
	}
    }

  return hashValue;
}

/*********************/
/* ResizeBetaMemory: */
/*********************/
void
ResizeBetaMemory (Environment * theEnv, struct betaMemory *theMemory)
{
  struct partialMatch **oldArray, **lastAdd, *thePM, *nextPM;
  unsigned long i, oldSize, betaLocation;

  oldSize = theMemory->size;
  oldArray = theMemory->beta;

  theMemory->size = oldSize * 11;
  theMemory->beta =
    (struct partialMatch **) CL_genalloc (theEnv,
					  sizeof (struct partialMatch *) *
					  theMemory->size);

  lastAdd =
    (struct partialMatch **) CL_genalloc (theEnv,
					  sizeof (struct partialMatch *) *
					  theMemory->size);
  memset (theMemory->beta, 0,
	  sizeof (struct partialMatch *) * theMemory->size);
  memset (lastAdd, 0, sizeof (struct partialMatch *) * theMemory->size);

  for (i = 0; i < oldSize; i++)
    {
      thePM = oldArray[i];
      while (thePM != NULL)
	{
	  nextPM = thePM->nextInMemory;

	  thePM->nextInMemory = NULL;

	  betaLocation = thePM->hashValue % theMemory->size;
	  thePM->prevInMemory = lastAdd[betaLocation];

	  if (lastAdd[betaLocation] != NULL)
	    {
	      lastAdd[betaLocation]->nextInMemory = thePM;
	    }
	  else
	    {
	      theMemory->beta[betaLocation] = thePM;
	    }

	  lastAdd[betaLocation] = thePM;

	  thePM = nextPM;
	}
    }

  if (theMemory->last != NULL)
    {
      CL_genfree (theEnv, theMemory->last,
		  sizeof (struct partialMatch *) * oldSize);
      theMemory->last = lastAdd;
    }
  else
    {
      CL_genfree (theEnv, lastAdd,
		  sizeof (struct partialMatch *) * theMemory->size);
    }

  CL_genfree (theEnv, oldArray, sizeof (struct partialMatch *) * oldSize);
}

/********************/
/* CL_ResetBetaMemory: */
/********************/
static void
CL_ResetBetaMemory (Environment * theEnv, struct betaMemory *theMemory)
{
  struct partialMatch **oldArray, **lastAdd;
  unsigned long oldSize;

  if ((theMemory->size == 1) || (theMemory->size == INITIAL_BETA_HASH_SIZE))
    {
      return;
    }

  oldSize = theMemory->size;
  oldArray = theMemory->beta;

  theMemory->size = INITIAL_BETA_HASH_SIZE;
  theMemory->beta =
    (struct partialMatch **) CL_genalloc (theEnv,
					  sizeof (struct partialMatch *) *
					  theMemory->size);
  memset (theMemory->beta, 0,
	  sizeof (struct partialMatch *) * theMemory->size);
  CL_genfree (theEnv, oldArray, sizeof (struct partialMatch *) * oldSize);

  if (theMemory->last != NULL)
    {
      lastAdd =
	(struct partialMatch **) CL_genalloc (theEnv,
					      sizeof (struct partialMatch *) *
					      theMemory->size);
      memset (lastAdd, 0, sizeof (struct partialMatch *) * theMemory->size);
      CL_genfree (theEnv, theMemory->last,
		  sizeof (struct partialMatch *) * oldSize);
      theMemory->last = lastAdd;
    }
}

/********************/
/* CL_PrintBetaMemory: */
/********************/
unsigned long
CL_PrintBetaMemory (Environment * theEnv,
		    const char *logName,
		    struct betaMemory *theMemory,
		    bool indentFirst, const char *indentString, int output)
{
  struct partialMatch *listOf_Matches;
  unsigned long b, count = 0;

  if (CL_Get_HaltExecution (theEnv) == true)
    {
      return count;
    }

  for (b = 0; b < theMemory->size; b++)
    {
      listOf_Matches = theMemory->beta[b];

      while (listOf_Matches != NULL)
	{
	 /*=========================================*/
	  /* Check to see if the user is attempting  */
	  /* to stop the display of partial matches. */
	 /*=========================================*/

	  if (CL_Get_HaltExecution (theEnv) == true)
	    {
	      return count;
	    }

	 /*=========================================================*/
	  /* The first partial match may have already been indented. */
	  /* Subsequent partial matches will always be indented with */
	  /* the indentation string.                                 */
	 /*=========================================================*/

	  if (output == VERBOSE)
	    {
	      if (indentFirst)
		{
		  CL_WriteString (theEnv, logName, indentString);
		}
	      else
		{
		  indentFirst = true;
		}
	    }

	 /*==========================*/
	  /* Print the partial match. */
	 /*==========================*/

	  if (output == VERBOSE)
	    {
	      CL_PrintPartialMatch (theEnv, logName, listOf_Matches);
	      CL_WriteString (theEnv, logName, "\n");
	    }

	  count++;

	 /*============================*/
	  /* Move on to the next match. */
	 /*============================*/

	  listOf_Matches = listOf_Matches->nextInMemory;
	}
    }

  return count;
}

#if (CONSTRUCT_COMPILER || BLOAD_AND_BSAVE) && (! RUN_TIME)

/*************************************************************/
/* CL_TagRuleNetwork: Assigns each join in the join network and */
/*   each defrule data structure with a unique integer ID.   */
/*   Also counts the number of defrule and joinNode data     */
/*   structures currently in use.                            */
/*************************************************************/
void
CL_TagRuleNetwork (Environment * theEnv,
		   unsigned long *moduleCount,
		   unsigned long *ruleCount,
		   unsigned long *joinCount, unsigned long *linkCount)
{
  Defmodule *modulePtr;
  Defrule *rulePtr, *disjunctPtr;
  struct joinLink *theLink;

  *moduleCount = 0;
  *ruleCount = 0;
  *joinCount = 0;
  *linkCount = 0;

  CL_MarkRuleNetwork (theEnv, 0);

  for (theLink = DefruleData (theEnv)->LeftPrimeJoins;
       theLink != NULL; theLink = theLink->next)
    {
      theLink->bsaveID = *linkCount;
      (*linkCount)++;
    }

  for (theLink = DefruleData (theEnv)->RightPrimeJoins;
       theLink != NULL; theLink = theLink->next)
    {
      theLink->bsaveID = *linkCount;
      (*linkCount)++;
    }

   /*===========================*/
  /* Loop through each module. */
   /*===========================*/

  for (modulePtr = CL_GetNextDefmodule (theEnv, NULL);
       modulePtr != NULL; modulePtr = CL_GetNextDefmodule (theEnv, modulePtr))
    {
      (*moduleCount)++;
      CL_SetCurrentModule (theEnv, modulePtr);

      /*=========================*/
      /* Loop through each rule. */
      /*=========================*/

      rulePtr = CL_GetNextDefrule (theEnv, NULL);

      while (rulePtr != NULL)
	{
	 /*=============================*/
	  /* Loop through each disjunct. */
	 /*=============================*/

	  for (disjunctPtr = rulePtr; disjunctPtr != NULL;
	       disjunctPtr = disjunctPtr->disjunct)
	    {
	      disjunctPtr->header.bsaveID = *ruleCount;
	      (*ruleCount)++;
	      TagNetworkTraverseJoins (theEnv, joinCount, linkCount,
				       disjunctPtr->lastJoin);
	    }

	  rulePtr = CL_GetNextDefrule (theEnv, rulePtr);
	}
    }
}

/*******************************************************************/
/* TagNetworkTraverseJoins: Traverses the join network for a rule. */
/*******************************************************************/
static void
TagNetworkTraverseJoins (Environment * theEnv,
			 unsigned long *joinCount,
			 unsigned long *linkCount, struct joinNode *joinPtr)
{
  struct joinLink *theLink;
  for (; joinPtr != NULL; joinPtr = joinPtr->lastLevel)
    {
      if (joinPtr->marked == 0)
	{
	  joinPtr->marked = 1;
	  joinPtr->bsaveID = *joinCount;
	  (*joinCount)++;
	  for (theLink = joinPtr->nextLinks;
	       theLink != NULL; theLink = theLink->next)
	    {
	      theLink->bsaveID = *linkCount;
	      (*linkCount)++;
	    }
	}

      if (joinPtr->joinFromTheRight)
	{
	  TagNetworkTraverseJoins (theEnv, joinCount, linkCount,
				   (struct joinNode *)
				   joinPtr->rightSideEntryStructure);
	}
    }
}

#endif /* (CONSTRUCT_COMPILER || BLOAD_AND_BSAVE) && (! RUN_TIME) */

#endif /* DEFRULE_CONSTRUCT */
