   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/11/16             */
   /*                                                     */
   /*              INCREMENTAL RESET MODULE               */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides functionality for the incremental       */
/*   reset of the pattern and join networks when a new       */
/*   rule is added.                                          */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Removed INCREMENTAL_RESET compilation flag.    */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Added support for hashed alpha memories and    */
/*            other join network changes.                    */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Modified EnvSetCL_IncrementalCL_Reset to check for   */
/*            the existance of rules.                        */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.31: Fix for slow incremental reset of rule with    */
/*            several dozen nand joins.                      */
/*                                                           */
/*      6.40: Added Env prefix to GetCL_EvaluationError and     */
/*            SetCL_EvaluationError functions.                  */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            Incremental reset is always enabled.           */
/*                                                           */
/*************************************************************/

#include "setup.h"

#include <stdio.h>

#if DEFRULE_CONSTRUCT

#include "agenda.h"
#include "argacces.h"
#include "constant.h"
#include "drive.h"
#include "engine.h"
#include "envrnmnt.h"
#include "evaluatn.h"
#include "pattern.h"
#include "router.h"
#include "reteutil.h"

#include "incrrset.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if (! RUN_TIME) && (! BLOAD_ONLY)
   static void                    MarkNetworkForCL_IncrementalCL_Reset(Environment *,Defrule *,bool);
   static void                    MarkJoinsForCL_IncrementalCL_Reset(Environment *,struct joinNode *,bool);
   static void                    CheckForPrimableJoins(Environment *,Defrule *,struct joinNode *);
   static void                    PrimeJoinFromLeftMemory(Environment *,struct joinNode *);
   static void                    PrimeJoinFromRightMemory(Environment *,struct joinNode *);
   static void                    MarkPatternForCL_IncrementalCL_Reset(Environment *,unsigned short,
                                                                 struct patternNodeHeader *,bool);
#endif

/**************************************************************/
/* CL_IncrementalCL_Reset: Incrementally resets the specified rule. */
/**************************************************************/
void CL_IncrementalCL_Reset(
  Environment *theEnv,
  Defrule *tempRule)
  {
#if (! RUN_TIME) && (! BLOAD_ONLY)
   Defrule *tempPtr;
   struct patternParser *theParser;

   /*=====================================================*/
   /* Mark the pattern and join network data structures   */
   /* associated with the rule being incrementally reset. */
   /*=====================================================*/

   MarkNetworkForCL_IncrementalCL_Reset(theEnv,tempRule,true);

   /*==========================*/
   /* Begin incremental reset. */
   /*==========================*/

   EngineData(theEnv)->CL_IncrementalCL_ResetInProgress = true;

   /*============================================================*/
   /* If the new rule shares patterns or joins with other rules, */
   /* then it is necessary to update its join network based on   */
   /* existing partial matches it shares with other rules.       */
   /*============================================================*/

   for (tempPtr = tempRule;
        tempPtr != NULL;
        tempPtr = tempPtr->disjunct)
     { CheckForPrimableJoins(theEnv,tempPtr,tempPtr->lastJoin); }

   /*===============================================*/
   /* Filter existing data entities through the new */
   /* portions of the pattern and join networks.    */
   /*===============================================*/

   for (theParser = PatternData(theEnv)->ListOfPatternParsers;
        theParser != NULL;
        theParser = theParser->next)
     {
      if (theParser->incrementalCL_ResetFunction != NULL)
        { (*theParser->incrementalCL_ResetFunction)(theEnv); }
     }

   /*========================*/
   /* End incremental reset. */
   /*========================*/

   EngineData(theEnv)->CL_IncrementalCL_ResetInProgress = false;

   /*====================================================*/
   /* Remove the marks in the pattern and join networks. */
   /*====================================================*/

   MarkNetworkForCL_IncrementalCL_Reset(theEnv,tempRule,false);
#endif
  }

#if (! RUN_TIME) && (! BLOAD_ONLY)

/**********************************************************************/
/* MarkNetworkForCL_IncrementalCL_Reset: Coordinates marking the initialize */
/*   flags in the pattern and join networks both before and after an  */
/*   incremental reset.                                               */
/**********************************************************************/
static void MarkNetworkForCL_IncrementalCL_Reset(
  Environment *theEnv,
  Defrule *tempRule,
  bool value)
  {
   /*============================================*/
   /* Loop through each of the rule's disjuncts. */
   /*============================================*/

   for (;
        tempRule != NULL;
        tempRule = tempRule->disjunct)
     { MarkJoinsForCL_IncrementalCL_Reset(theEnv,tempRule->lastJoin,value); }
  }

/**********************************************************************/
/* MarkJoinsForCL_IncrementalCL_Reset: Coordinates marking the initialize */
/*   flags in the pattern and join networks both before and after an  */
/*   incremental reset.                                               */
/**********************************************************************/
static void MarkJoinsForCL_IncrementalCL_Reset(
  Environment *theEnv,
  struct joinNode *joinPtr,
  bool value)
  {
   struct patternNodeHeader *patternPtr;

   for (;
        joinPtr != NULL;
        joinPtr = GetPreviousJoin(joinPtr))
     {
      if (joinPtr->ruleToActivate != NULL)
        {
         joinPtr->marked = false;
         joinPtr->initialize = value;
         continue;
        }

      //if (joinPtr->joinFromTheRight)
      //  { MarkJoinsForCL_IncrementalCL_Reset(theEnv,(struct joinNode *) joinPtr->rightSideEntryStructure,value); }

      /*================*/
      /* Mark the join. */
      /*================*/

      joinPtr->marked = false; /* GDR 6.05 */

      if (joinPtr->initialize)
        {
         joinPtr->initialize = value;
         if (joinPtr->joinFromTheRight == false)
           {
            patternPtr = (struct patternNodeHeader *) GetPatternForJoin(joinPtr);
            if (patternPtr != NULL)
              { MarkPatternForCL_IncrementalCL_Reset(theEnv,joinPtr->rhsType,patternPtr,value); }
           }
        }
     }
  }

/*******************************************************************************/
/* CheckForPrimableJoins: Updates the joins of a rule for an incremental reset */
/*   if portions of that rule are shared with other rules that have already    */
/*   been incrementally reset. A join for a new rule will be updated if it is  */
/*   marked for initialization and either its parent join or its associated    */
/*   entry pattern node has not been marked for initialization. The function   */
/*   PrimeJoin is used to update joins which meet these criteria.              */
/*******************************************************************************/
static void CheckForPrimableJoins(
  Environment *theEnv,
  Defrule *tempRule,
  struct joinNode *joinPtr)
  {
   /*========================================*/
   /* Loop through each of the rule's joins. */
   /*========================================*/

   for (;
        joinPtr != NULL;
        joinPtr = GetPreviousJoin(joinPtr))
     {
      /*===============================*/
      /* Update the join if necessary. */
      /*===============================*/

      if ((joinPtr->initialize) && (! joinPtr->marked))
        {
         if (joinPtr->firstJoin == true)
           {
			if (joinPtr->joinFromTheRight == false)
              {
               if ((joinPtr->rightSideEntryStructure == NULL) ||
                   (joinPtr->patternIsNegated) ||
                   (((struct patternNodeHeader *) joinPtr->rightSideEntryStructure)->initialize == false))
                 {
                  PrimeJoinFromLeftMemory(theEnv,joinPtr);
                  joinPtr->marked = true;
                 }
              }
            else
              {
               PrimeJoinFromRightMemory(theEnv,joinPtr);
               joinPtr->marked = true;
              }
           }
         else if (joinPtr->lastLevel->initialize == false)
           {
            PrimeJoinFromLeftMemory(theEnv,joinPtr);
            joinPtr->marked = true;
           }
         else if ((joinPtr->joinFromTheRight) &&
                (((struct joinNode *) joinPtr->rightSideEntryStructure)->initialize == false))
           {
            PrimeJoinFromRightMemory(theEnv,joinPtr);
            joinPtr->marked = true;
           }
        }

      //if (joinPtr->joinFromTheRight)
      //  { CheckForPrimableJoins(theEnv,tempRule,(struct joinNode *) joinPtr->rightSideEntryStructure); }
     }
  }

/****************************************************************************/
/* PrimeJoinFromLeftMemory: Updates a join in a rule for an incremental     */
/*   reset. Joins are updated by "priming" them only if the join (or its    */
/*   associated pattern) is shared with other rules that have already been  */
/*   incrementally reset. A join for a new rule will be updated if it is    */
/*   marked for initialization and either its parent join or its associated */
/*   entry pattern node has not been marked for initialization.             */
/****************************************************************************/
static void PrimeJoinFromLeftMemory(
  Environment *theEnv,
  struct joinNode *joinPtr)
  {
   struct partialMatch *theList, *linker;
   struct alphaMemoryHash *listOfHashNodes;
   unsigned long b;
   unsigned long hashValue;
   struct betaMemory *theMemory;
   struct partialMatch *notParent;
   struct joinLink *tempLink;

   /*===========================================================*/
   /* If the join is the first join of a rule, then send all of */
   /* the partial matches from the alpha memory of the pattern  */
   /* associated with this join to the join for processing and  */
   /* the priming process is then complete.                     */
   /*===========================================================*/

   if (joinPtr->firstJoin == true)
     {
      if (joinPtr->rightSideEntryStructure == NULL)
        { NetworkCL_Assert(theEnv,joinPtr->rightMemory->beta[0],joinPtr); }
      else if (joinPtr->patternIsNegated)
        {
         notParent = joinPtr->leftMemory->beta[0];

         if (joinPtr->secondaryNetworkTest != NULL)
           {
            if (CL_EvaluateSecondaryNetworkTest(theEnv,notParent,joinPtr) == false)
              { return; }
           }

         for (listOfHashNodes = ((struct patternNodeHeader *) joinPtr->rightSideEntryStructure)->firstHash;
              listOfHashNodes != NULL;
              listOfHashNodes = listOfHashNodes->nextHash)
           {
            if (listOfHashNodes->alphaMemory != NULL)
              {
               CL_AddBlockedLink(notParent,listOfHashNodes->alphaMemory);
               return;
              }
           }

         CL_EPMDrive(theEnv,notParent,joinPtr,NETWORK_ASSERT);
        }
      else
        {
         for (listOfHashNodes = ((struct patternNodeHeader *) joinPtr->rightSideEntryStructure)->firstHash;
              listOfHashNodes != NULL;
              listOfHashNodes = listOfHashNodes->nextHash)
           {
            for (theList = listOfHashNodes->alphaMemory;
                 theList != NULL;
                 theList = theList->nextInMemory)
              { NetworkCL_Assert(theEnv,theList,joinPtr); }
           }
        }
      return;
     }

   /*========================================*/
   /* Find another beta memory from which we */
   /* can retrieve the partial matches.      */
   /*========================================*/

   tempLink = joinPtr->lastLevel->nextLinks;

   while (tempLink != NULL)
     {
      if ((tempLink->join != joinPtr) &&
          (tempLink->join->initialize == false))
        { break; }

      tempLink = tempLink->next;
     }

   if (tempLink == NULL) return;

   if (tempLink->enterDirection == LHS)
     { theMemory = tempLink->join->leftMemory; }
   else
     { theMemory = tempLink->join->rightMemory; }

   /*============================================*/
   /* CL_Send all partial matches from the selected */
   /* beta memory to the new join.               */
   /*============================================*/

   for (b = 0; b < theMemory->size; b++)
     {
      for (theList = theMemory->beta[b];
           theList != NULL;
           theList = theList->nextInMemory)
        {
         linker = CL_CopyPartialMatch(theEnv,theList);

         if (joinPtr->leftHash != NULL)
           { hashValue = CL_BetaMemoryHashValue(theEnv,joinPtr->leftHash,linker,NULL,joinPtr); }
         else
           { hashValue = 0; }

         CL_UpdateBetaPMLinks(theEnv,linker,theList->leftParent,theList->rightParent,joinPtr,hashValue,LHS);

         NetworkCL_AssertLeft(theEnv,linker,joinPtr,NETWORK_ASSERT);
        }
     }
  }

/****************************************************************************/
/* PrimeJoinFromRightMemory: Updates a join in a rule for an incremental    */
/*   reset. Joins are updated by "priming" them only if the join (or its    */
/*   associated pattern) is shared with other rules that have already been  */
/*   incrementally reset. A join for a new rule will be updated if it is    */
/*   marked for initialization and either its parent join or its associated */
/*   entry pattern node has not been marked for initialization.             */
/****************************************************************************/
static void PrimeJoinFromRightMemory(
  Environment *theEnv,
  struct joinNode *joinPtr)
  {
   struct partialMatch *theList, *linker;
   unsigned long b;
   struct betaMemory *theMemory;
   unsigned long hashValue;
   struct joinLink *tempLink;
   struct partialMatch *notParent;

   /*=======================================*/
   /* This should be a join from the right. */
   /*=======================================*/

   if (joinPtr->joinFromTheRight == false)
     { return; }

   /*========================================*/
   /* Find another beta memory from which we */
   /* can retrieve the partial matches.      */
   /*========================================*/

   tempLink = ((struct joinNode *) joinPtr->rightSideEntryStructure)->nextLinks;
   while (tempLink != NULL)
     {
      if ((tempLink->join != joinPtr) &&
          (tempLink->join->initialize == false))
        { break; }

      tempLink = tempLink->next;
     }

   if (tempLink == NULL)
     {
      if (joinPtr->firstJoin &&
          (joinPtr->rightMemory->beta[0] == NULL) &&
          (! joinPtr->patternIsExists))
        {
         notParent = joinPtr->leftMemory->beta[0];

         if (joinPtr->secondaryNetworkTest != NULL)
           {
            if (CL_EvaluateSecondaryNetworkTest(theEnv,notParent,joinPtr) == false)
              { return; }
           }

         CL_EPMDrive(theEnv,notParent,joinPtr,NETWORK_ASSERT);
        }

      return;
     }

   if (tempLink->enterDirection == LHS)
     { theMemory = tempLink->join->leftMemory; }
   else
     { theMemory = tempLink->join->rightMemory; }

   /*============================================*/
   /* CL_Send all partial matches from the selected */
   /* beta memory to the new join.               */
   /*============================================*/

   for (b = 0; b < theMemory->size; b++)
     {
      for (theList = theMemory->beta[b];
           theList != NULL;
           theList = theList->nextInMemory)
        {
         linker = CL_CopyPartialMatch(theEnv,theList);

         if (joinPtr->rightHash != NULL)
           { hashValue = CL_BetaMemoryHashValue(theEnv,joinPtr->rightHash,linker,NULL,joinPtr); }
         else
           { hashValue = 0; }

         CL_UpdateBetaPMLinks(theEnv,linker,theList->leftParent,theList->rightParent,joinPtr,hashValue,RHS);
         NetworkCL_Assert(theEnv,linker,joinPtr);
        }
     }

   if (joinPtr->firstJoin &&
       (joinPtr->rightMemory->beta[0] == NULL) &&
       (! joinPtr->patternIsExists))
     {
      notParent = joinPtr->leftMemory->beta[0];

      if (joinPtr->secondaryNetworkTest != NULL)
        {
         if (CL_EvaluateSecondaryNetworkTest(theEnv,notParent,joinPtr) == false)
           { return; }
        }

      CL_EPMDrive(theEnv,notParent,joinPtr,NETWORK_ASSERT);
     }
  }

/*********************************************************************/
/* MarkPatternForCL_IncrementalCL_Reset: Given a pattern node and its type */
/*   (fact, instance, etc.), calls the appropriate function to mark  */
/*   the pattern for an incremental reset. Used to mark the pattern  */
/*   nodes both before and after an incremental reset.               */
/*********************************************************************/
static void MarkPatternForCL_IncrementalCL_Reset(
  Environment *theEnv,
  unsigned short rhsType,
  struct patternNodeHeader *theHeader,
  bool value)
  {
   struct patternParser *tempParser;

   tempParser = CL_GetPatternParser(theEnv,rhsType);

   if (tempParser != NULL)
     {
      if (tempParser->markIRPatternFunction != NULL)
        { (*tempParser->markIRPatternFunction)(theEnv,theHeader,value); }
     }
  }

#endif

#endif /* DEFRULE_CONSTRUCT */
