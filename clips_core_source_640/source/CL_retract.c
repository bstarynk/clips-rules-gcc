   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/28/17             */
   /*                                                     */
   /*                   RETRACT MODULE                    */
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
/*      6.31: Bug fix to prevent rule activations for        */
/*            partial matches being deleted.                 */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "setup.h"

#if DEFRULE_CONSTRUCT

#include "agenda.h"
#include "argacces.h"
#include "constant.h"
#include "drive.h"
#include "engine.h"
#include "envrnmnt.h"
#include "lgcldpnd.h"
#include "match.h"
#include "memalloc.h"
#include "network.h"
#include "prntutil.h"
#include "reteutil.h"
#include "router.h"
#include "symbol.h"

#include "retract.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    ReturnMarkers(Environment *,struct multifieldMarker *);
   static bool                    FindNextConflictingMatch(Environment *,struct partialMatch *,
                                                           struct partialMatch *,
                                                           struct joinNode *,struct partialMatch *,int);
   static bool                    PartialMatchDefunct(Environment *,struct partialMatch *);
   static void                    NegEntry_RetractAlpha(Environment *,struct partialMatch *,int);
   static void                    NegEntry_RetractBeta(Environment *,struct joinNode *,struct partialMatch *,
                                                      struct partialMatch *,int);

/************************************************************/
/* CL_Network_Retract:  CL_Retracts a data entity (such as a fact  */
/*   or instance) from the pattern and join networks given  */
/*   a pointer to the list of patterns which the data       */
/*   entity matched.                                        */
/************************************************************/
void CL_Network_Retract(
  Environment *theEnv,
  struct patternMatch *listOfMatchedPatterns)
  {
   struct patternMatch *tempMatch, *nextMatch;

   tempMatch = listOfMatchedPatterns;
   while (tempMatch != NULL)
     {
      nextMatch = tempMatch->next;

      tempMatch->theMatch->deleting = true;
      
      if (tempMatch->theMatch->children != NULL)
        { CL_PosEntry_RetractAlpha(theEnv,tempMatch->theMatch,NETWORK_RETRACT); }

      if (tempMatch->theMatch->blockList != NULL)
        { NegEntry_RetractAlpha(theEnv,tempMatch->theMatch,NETWORK_RETRACT); }

      /*===================================================*/
      /* Remove from the alpha memory of the pattern node. */
      /*===================================================*/

      RemoveAlphaMemory_Matches(theEnv,tempMatch->matchingPattern,
                               tempMatch->theMatch,
                               tempMatch->theMatch->binds[0].gm.theMatch);

      rtn_struct(theEnv,patternMatch,tempMatch);

      tempMatch = nextMatch;
     }
  }

/*************************/
/* CL_PosEntry_RetractAlpha: */
/*************************/
void CL_PosEntry_RetractAlpha(
  Environment *theEnv,
  struct partialMatch *alphaMatch,
  int operation)
  {
   struct partialMatch *betaMatch, *tempMatch;
   struct joinNode *joinPtr;

   betaMatch = alphaMatch->children;
   while (betaMatch != NULL)
     {
      joinPtr = (struct joinNode *) betaMatch->owner;

      if (betaMatch->children != NULL)
        { CL_PosEntry_RetractBeta(theEnv,betaMatch,betaMatch->children,operation); }

      if (betaMatch->rhsMemory)
        { NegEntry_RetractAlpha(theEnv,betaMatch,operation); }

      /* Remove the beta match. */

	  if ((joinPtr->ruleToActivate != NULL) ?
		  (betaMatch->marker != NULL) : false)
		{ CL_RemoveActivation(theEnv,(struct activation *) betaMatch->marker,true,true); }

	  tempMatch = betaMatch->nextRightChild;

	  if (betaMatch->rhsMemory)
		{ CL_UnlinkBetaPMFromNodeAndLineage(theEnv,joinPtr,betaMatch,RHS); }
	  else
		{ CL_UnlinkBetaPMFromNodeAndLineage(theEnv,joinPtr,betaMatch,LHS); }

      CL_DeletePartial_Matches(theEnv,betaMatch);

      betaMatch = tempMatch;
     }
  }

/*************************/
/* NegEntry_RetractAlpha: */
/*************************/
static void NegEntry_RetractAlpha(
  Environment *theEnv,
  struct partialMatch *alphaMatch,
  int operation)
  {
   struct partialMatch *betaMatch;
   struct joinNode *joinPtr;

   betaMatch = alphaMatch->blockList;
   while (betaMatch != NULL)
     {
      joinPtr = (struct joinNode *) betaMatch->owner;

      if ((! joinPtr->patternIsNegated) &&
          (! joinPtr->patternIsExists) &&
          (! joinPtr->joinFromTheRight))
        {
         CL_SystemError(theEnv,"RETRACT",117);
         betaMatch = betaMatch->nextBlocked;
         continue;
        }

      NegEntry_RetractBeta(theEnv,joinPtr,alphaMatch,betaMatch,operation);
      betaMatch = alphaMatch->blockList;
     }
  }

/************************/
/* NegEntry_RetractBeta: */
/************************/
static void NegEntry_RetractBeta(
  Environment *theEnv,
  struct joinNode *joinPtr,
  struct partialMatch *alphaMatch,
  struct partialMatch *betaMatch,
  int operation)
  {
   /*======================================================*/
   /* Try to find another RHS partial match which prevents */
   /* the LHS partial match from being satisifed.          */
   /*======================================================*/

   CL_RemoveBlockedLink(betaMatch);

   if (FindNextConflictingMatch(theEnv,betaMatch,alphaMatch->nextInMemory,joinPtr,alphaMatch,operation))
     { return; }
   else if (joinPtr->patternIsExists)
     {
      if (betaMatch->children != NULL)
        { CL_PosEntry_RetractBeta(theEnv,betaMatch,betaMatch->children,operation); }
      return;
     }
   else if (joinPtr->firstJoin && (joinPtr->patternIsNegated || joinPtr->joinFromTheRight) && (! joinPtr->patternIsExists))
     {
      if (joinPtr->secondaryNetworkTest != NULL)
        {
         if (CL_EvaluateSecondaryNetworkTest(theEnv,betaMatch,joinPtr) == false)
           { return; }
        }

      CL_EPMDrive(theEnv,betaMatch,joinPtr,operation);

      return;
     }

   if (joinPtr->secondaryNetworkTest != NULL)
     {
      if (CL_EvaluateSecondaryNetworkTest(theEnv,betaMatch,joinPtr) == false)
        { return; }
     }

   /*=========================================================*/
   /* If the LHS partial match now has no RHS partial matches */
   /* that conflict with it, then it satisfies the conditions */
   /* of the RHS not CE. Create a partial match and send it   */
   /* to the joins below.                                     */
   /*=========================================================*/

   /*===============================*/
   /* Create the new partial match. */
   /*===============================*/

   if ((operation == NETWORK_RETRACT) && CL_PartialMatchWillBeDeleted(theEnv,betaMatch))
     { return; }

   CL_PPDrive(theEnv,betaMatch,NULL,joinPtr,operation);
  }

/************************/
/* CL_PosEntry_RetractBeta: */
/************************/
void CL_PosEntry_RetractBeta(
  Environment *theEnv,
  struct partialMatch *parentMatch,
  struct partialMatch *betaMatch,
  int operation)
  {
   struct partialMatch *tempMatch;

   while (betaMatch != NULL)
     {
      if (betaMatch->children != NULL)
        {
         betaMatch = betaMatch->children;
         continue;
        }

      if (betaMatch->nextLeftChild != NULL)
        { tempMatch = betaMatch->nextLeftChild; }
      else
        {
         tempMatch = betaMatch->leftParent;
         betaMatch->leftParent->children = NULL;
        }

      if (betaMatch->blockList != NULL)
        { NegEntry_RetractAlpha(theEnv,betaMatch,operation); }
      else if ((((struct joinNode *) betaMatch->owner)->ruleToActivate != NULL) ?
               (betaMatch->marker != NULL) : false)
        { CL_RemoveActivation(theEnv,(struct activation *) betaMatch->marker,true,true); }

      if (betaMatch->rhsMemory)
        { CL_UnlinkNonLeftLineage(theEnv,(struct joinNode *) betaMatch->owner,betaMatch,RHS); }
      else
        { CL_UnlinkNonLeftLineage(theEnv,(struct joinNode *) betaMatch->owner,betaMatch,LHS); }

      if (betaMatch->dependents != NULL) CL_RemoveLogicalSupport(theEnv,betaMatch);
      CL_ReturnPartialMatch(theEnv,betaMatch);

      if (tempMatch == parentMatch) return;
      betaMatch = tempMatch;
     }
  }

/******************************************************************/
/* FindNextConflictingMatch: Finds the next conflicting partial   */
/*    match in the right memory of a join that prevents a partial */
/*    match in the beta memory of the join from being satisfied.  */
/******************************************************************/
static bool FindNextConflictingMatch(
  Environment *theEnv,
  struct partialMatch *theBind,
  struct partialMatch *possibleConflicts,
  struct joinNode *theJoin,
  struct partialMatch *skipMatch,
  int operation)
  {
   bool result, restore = false;
   struct partialMatch *oldLHSBinds = NULL;
   struct partialMatch *oldRHSBinds = NULL;
   struct joinNode *oldJoin = NULL;

   /*====================================*/
   /* Check each of the possible partial */
   /* matches which could conflict.      */
   /*====================================*/

#if DEVELOPER
   if (possibleConflicts != NULL)
     { EngineData(theEnv)->leftToRightLoops++; }
#endif
   /*====================================*/
   /* Set up the evaluation environment. */
   /*====================================*/

   if (possibleConflicts != NULL)
     {
      oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
      oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
      oldJoin = EngineData(theEnv)->GlobalJoin;
      EngineData(theEnv)->GlobalLHSBinds = theBind;
      EngineData(theEnv)->GlobalJoin = theJoin;
      restore = true;
     }

   for (;
        possibleConflicts != NULL;
        possibleConflicts = possibleConflicts->nextInMemory)
     {
      theJoin->memoryCompares++;

      /*=====================================*/
      /* Initially indicate that the partial */
      /* match doesn't conflict.             */
      /*=====================================*/

      result = false;

      if (skipMatch == possibleConflicts)
        { /* Do Nothing */ }

       /*======================================================*/
       /* 6.05 Bug Fix. It is possible that a pattern entity   */
       /* (e.g. instance) in a partial match is 'out of date'  */
       /* with respect to the lazy evaluation scheme use by    */
       /* negated patterns. In other words, the object may     */
       /* have changed since it was last pushed through the    */
       /* network, and thus the partial match may be invalid.  */
       /* If so, the partial match must be ignored here.       */
       /*======================================================*/

      else if (PartialMatchDefunct(theEnv,possibleConflicts))
        { /* Do Nothing */ }

      else if ((operation == NETWORK_RETRACT) && CL_PartialMatchWillBeDeleted(theEnv,possibleConflicts))
        { /* Do Nothing */ }

      /*================================================*/
      /* If the join doesn't have a network expression  */
      /* to be evaluated, then partial match conflicts. */
      /*================================================*/

      else if (theJoin->networkTest == NULL)
        { result = true; }

      /*=================================================*/
      /* Otherwise, if the join has a network expression */
      /* to evaluate, then evaluate it.                  */
      /*=================================================*/

      else
        {
#if DEVELOPER
         if (theJoin->networkTest)
           {
            EngineData(theEnv)->leftToRightComparisons++;
            EngineData(theEnv)->findNextConflictingComparisons++;
           }
#endif
         EngineData(theEnv)->GlobalRHSBinds = possibleConflicts;

         result = CL_EvaluateJoinExpression(theEnv,theJoin->networkTest,theJoin);
         if (CL_EvaluationData(theEnv)->CL_EvaluationError)
           {
            result = true;
            CL_EvaluationData(theEnv)->CL_EvaluationError = false;
           }

#if DEVELOPER
         if (result != false)
          { EngineData(theEnv)->leftToRightSucceeds++; }
#endif
        }

      /*==============================================*/
      /* If the network expression evaluated to true, */
      /* then partial match being examined conflicts. */
      /* Point the beta memory partial match to the   */
      /* conflicting partial match and return true to */
      /* indicate a conflict was found.               */
      /*==============================================*/

      if (result != false)
        {
         CL_AddBlockedLink(theBind,possibleConflicts);
         EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
         EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
         EngineData(theEnv)->GlobalJoin = oldJoin;
         return true;
        }
     }

   if (restore)
     {
      EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
      EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
      EngineData(theEnv)->GlobalJoin = oldJoin;
     }

   /*========================*/
   /* No conflict was found. */
   /*========================*/

   return false;
  }

/***********************************************************/
/* PartialMatchDefunct: Dete_rmines if any pattern entities */
/*   contained within the partial match have changed since */
/*   this partial match was generated. Assumes counterf is */
/*   false.                                                */
/***********************************************************/
static bool PartialMatchDefunct(
  Environment *theEnv,
  struct partialMatch *thePM)
  {
   unsigned short i;
   struct patternEntity * thePE;
   
   if (thePM->deleting) return true;

   for (i = 0 ; i < thePM->bcount ; i++)
     {
      if (thePM->binds[i].gm.theMatch == NULL) continue;
      thePE = thePM->binds[i].gm.theMatch->matchingItem;
      if (thePE && thePE->theInfo->synchronized &&
          !(*thePE->theInfo->synchronized)(theEnv,thePE))
        return true;
     }
   return false;
  }

/*****************************************************************/
/* CL_PartialMatchWillBeDeleted: Dete_rmines if any pattern entities */
/*   contained within the partial match were deleted as part of  */
/*   a retraction/deletion. When rules have multiple patterns    */
/*   that can be matched by the same fact it's possible that a   */
/*   partial match encountered in the join network has not yet   */
/*   deleted and so should not be considered as valid.           */
/*****************************************************************/
bool CL_PartialMatchWillBeDeleted(
  Environment *theEnv,
  struct partialMatch *thePM)
  {
   unsigned short i;
   struct patternEntity * thePE;

   if (thePM == NULL) return false;
   
   if (thePM->deleting) return true;

   for (i = 0 ; i < thePM->bcount ; i++)
     {
      if (thePM->binds[i].gm.theMatch == NULL) continue;
      thePE = thePM->binds[i].gm.theMatch->matchingItem;
      if (thePE && thePE->theInfo->isDeleted &&
          (*thePE->theInfo->isDeleted)(theEnv,thePE))
        return true;
     }

   return false;
  }

/***************************************************/
/* CL_DeletePartial_Matches: Returns a list of partial */
/*   matches to the pool of free memory.           */
/***************************************************/
void CL_DeletePartial_Matches(
  Environment *theEnv,
  struct partialMatch *listOfPMs)
  {
   struct partialMatch *nextPM;

   while (listOfPMs != NULL)
     {
      /*============================================*/
      /* Remember the next partial match to delete. */
      /*============================================*/

      nextPM = listOfPMs->nextInMemory;

      /*================================================*/
      /* Remove the links between the partial match and */
      /* any data entities that it is attached to as a  */
      /* result of a logical CE.                        */
      /*================================================*/

      if (listOfPMs->dependents != NULL) CL_RemoveLogicalSupport(theEnv,listOfPMs);

      /*==========================================================*/
      /* If the partial match is being deleted from a beta memory */
      /* and the partial match isn't associated with a satisfied  */
      /* not CE, then it can be immediately returned to the pool  */
      /* of free memory. Otherwise, it's could be in use (either  */
      /* to retrieve variables from the LHS or by the activation  */
      /* of the rule). Since a not CE creates a "pseudo" data     */
      /* entity, the beta partial match which stores this pseudo  */
      /* data entity can not be deleted immediately (for the same */
      /* reason an alpha memory partial match can't be deleted    */
      /* immediately).                                            */
      /*==========================================================*/

      CL_ReturnPartialMatch(theEnv,listOfPMs);

      /*====================================*/
      /* Move on to the next partial match. */
      /*====================================*/

      listOfPMs = nextPM;
     }
  }

/**************************************************************/
/* CL_ReturnPartialMatch: Returns the data structures associated */
/*   with a partial match to the pool of free memory.         */
/**************************************************************/
void CL_ReturnPartialMatch(
  Environment *theEnv,
  struct partialMatch *waste)
  {
   /*==============================================*/
   /* If the partial match is in use, then put it  */
   /* on a garbage list to be processed later when */
   /* the partial match is not in use.             */
   /*==============================================*/

   if (waste->busy)
     {
      waste->nextInMemory = EngineData(theEnv)->GarbagePartial_Matches;
      EngineData(theEnv)->GarbagePartial_Matches = waste;
      return;
     }

   /*======================================================*/
   /* If we're dealing with an alpha memory partial match, */
   /* then return the multifield markers associated with   */
   /* the partial match (if any) along with the alphaMatch */
   /* data structure.                                      */
   /*======================================================*/

   if (waste->betaMemory == false)
     {
      if (waste->binds[0].gm.theMatch->markers != NULL)
        { ReturnMarkers(theEnv,waste->binds[0].gm.theMatch->markers); }
      CL_rm(theEnv,waste->binds[0].gm.theMatch,sizeof(struct alphaMatch));
     }

   /*=================================================*/
   /* Remove any links between the partial match and  */
   /* a data entity that were created with the use of */
   /* the logical CE.                                 */
   /*=================================================*/

   if (waste->dependents != NULL) RemovePM_Dependencies(theEnv,waste);

   /*======================================================*/
   /* Return the partial match to the pool of free memory. */
   /*======================================================*/

   rtn_var_struct(theEnv,partialMatch,sizeof(struct genericMatch *) *
                  (waste->bcount - 1),
                  waste);
  }

/***************************************************************/
/* CL_DestroyPartialMatch: Returns the data structures associated */
/*   with a partial match to the pool of free memory.          */
/***************************************************************/
void CL_DestroyPartialMatch(
  Environment *theEnv,
  struct partialMatch *waste)
  {
   /*======================================================*/
   /* If we're dealing with an alpha memory partial match, */
   /* then return the multifield markers associated with   */
   /* the partial match (if any) along with the alphaMatch */
   /* data structure.                                      */
   /*======================================================*/

   if (waste->betaMemory == false)
     {
      if (waste->binds[0].gm.theMatch->markers != NULL)
        { ReturnMarkers(theEnv,waste->binds[0].gm.theMatch->markers); }
      CL_rm(theEnv,waste->binds[0].gm.theMatch,sizeof(struct alphaMatch));
     }

   /*=================================================*/
   /* Remove any links between the partial match and  */
   /* a data entity that were created with the use of */
   /* the logical CE.                                 */
   /*=================================================*/

   if (waste->dependents != NULL) DestroyPM_Dependencies(theEnv,waste);

   /*======================================================*/
   /* Return the partial match to the pool of free memory. */
   /*======================================================*/

   rtn_var_struct(theEnv,partialMatch,sizeof(struct genericMatch *) *
                  (waste->bcount - 1),
                  waste);
  }

/******************************************************/
/* ReturnMarkers: Returns a linked list of multifield */
/*   markers associated with a data entity matching a */
/*   pattern to the pool of free memory.              */
/******************************************************/
static void ReturnMarkers(
  Environment *theEnv,
  struct multifieldMarker *waste)
  {
   struct multifieldMarker *temp;

   while (waste != NULL)
     {
      temp = waste->next;
      rtn_struct(theEnv,multifieldMarker,waste);
      waste = temp;
     }
  }

/*************************************************************/
/* CL_FlushGarbagePartial_Matches:  Returns partial matches and  */
/*   associated structures that were removed as part of a    */
/*   retraction. It is necessary to postpone returning these */
/*   structures to memory because RHS actions retrieve their */
/*   variable bindings directly from the fact and instance   */
/*   data structures through the alpha memory bindings.      */
/*************************************************************/
void CL_FlushGarbagePartial_Matches(
  Environment *theEnv)
  {
   struct partialMatch *pmPtr;
   struct alphaMatch *amPtr;

   /*===================================================*/
   /* Return the garbage partial matches collected from */
   /* the alpha memories of the pattern networks.       */
   /*===================================================*/

   while (EngineData(theEnv)->GarbageAlpha_Matches != NULL)
     {
      amPtr = EngineData(theEnv)->GarbageAlpha_Matches->next;
      rtn_struct(theEnv,alphaMatch,EngineData(theEnv)->GarbageAlpha_Matches);
      EngineData(theEnv)->GarbageAlpha_Matches = amPtr;
     }

   /*==============================================*/
   /* Return the garbage partial matches collected */
   /* from the beta memories of the join networks. */
   /*==============================================*/

   while (EngineData(theEnv)->GarbagePartial_Matches != NULL)
     {
      /*=====================================================*/
      /* Remember the next garbage partial match to process. */
      /*=====================================================*/

      pmPtr = EngineData(theEnv)->GarbagePartial_Matches->nextInMemory;

      /*============================================*/
      /* Dispose of the garbage partial match being */
      /* examined and move on to the next one.      */
      /*============================================*/

      EngineData(theEnv)->GarbagePartial_Matches->busy = false;
      CL_ReturnPartialMatch(theEnv,EngineData(theEnv)->GarbagePartial_Matches);
      EngineData(theEnv)->GarbagePartial_Matches = pmPtr;
     }
  }

#endif /* DEFRULE_CONSTRUCT */

