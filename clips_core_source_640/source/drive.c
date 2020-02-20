   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/01/16             */
   /*                                                     */
   /*                    DRIVE MODULE                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Handles join network activity associated with    */
/*   with the addition of a data entity such as a fact or    */
/*   instance.                                               */
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
/*      6.24: Removed INCREMENTAL_RESET and                  */
/*            LOGICAL_DEPENDENCIES compilation flags.        */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Rule with exists CE has incorrect activation.  */
/*            DR0867                                         */
/*                                                           */
/*      6.30: Added support for hashed memories.             */
/*                                                           */
/*            Added additional developer statistics to help  */
/*            analyze join network perfoCL_rmance.              */
/*                                                           */
/*            Removed pseudo-facts used in not CE.           */
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
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "setup.h"

#if DEFRULE_CONSTRUCT

#include "agenda.h"
#include "constant.h"
#include "engine.h"
#include "envrnmnt.h"
#include "incrrset.h"
#include "lgcldpnd.h"
#include "memalloc.h"
#include "prntutil.h"
#include "reteutil.h"
#include "retract.h"
#include "router.h"

#include "drive.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    EmptyDrive(Environment *,struct joinNode *,struct partialMatch *,int);
   static void                    JoinNetErrorMessage(Environment *,struct joinNode *);

/************************************************/
/* NetworkCL_Assert: Primary routine for filtering */
/*   a partial match through the join network.  */
/************************************************/
void NetworkCL_Assert(
  Environment *theEnv,
  struct partialMatch *binds,
  struct joinNode *join)
  {
   /*=========================================================*/
   /* If an incremental reset is being perfoCL_rmed and the join */
   /* is not part of the network to be reset, then return.    */
   /*=========================================================*/

#if (! BLOAD_ONLY) && (! RUN_TIME)
   if (EngineData(theEnv)->CL_IncrementalCL_ResetInProgress && (join->initialize == false)) return;
#endif

   /*==================================================*/
   /* Use a special routine if this is the first join. */
   /*==================================================*/

   if (join->firstJoin)
     {
      EmptyDrive(theEnv,join,binds,NETWORK_ASSERT);
      return;
     }

   /*================================*/
   /* Enter the join from the right. */
   /*================================*/

   NetworkCL_AssertRight(theEnv,binds,join,NETWORK_ASSERT);

   return;
  }

/*****************************************************/
/* NetworkCL_AssertRight: Primary routine for filtering */
/*   a partial match through the join network from   */
/*   the RHS of a join.                              */
/*****************************************************/
void NetworkCL_AssertRight(
  Environment *theEnv,
  struct partialMatch *rhsBinds,
  struct joinNode *join,
  int operation)
  {
   struct partialMatch *lhsBinds, *nextBind;
   bool exprResult, restore = false;
   struct partialMatch *oldLHSBinds = NULL;
   struct partialMatch *oldRHSBinds = NULL;
   struct joinNode *oldJoin = NULL;

   /*=========================================================*/
   /* If an incremental reset is being perfoCL_rmed and the join */
   /* is not part of the network to be reset, then return.    */
   /*=========================================================*/

#if (! BLOAD_ONLY) && (! RUN_TIME)
   if (EngineData(theEnv)->CL_IncrementalCL_ResetInProgress && (join->initialize == false)) return;
#endif

   if (join->firstJoin)
     {
      EmptyDrive(theEnv,join,rhsBinds,operation);
      return;
     }

   /*=====================================================*/
   /* The partial matches entering from the LHS of a join */
   /* are stored in the left beta memory of the join.     */
   /*=====================================================*/

   lhsBinds = CL_GetLeftBetaMemory(join,rhsBinds->hashValue);

#if DEVELOPER
   if (lhsBinds != NULL)
     { EngineData(theEnv)->rightToLeftLoops++; }
#endif

   /*====================================*/
   /* Set up the evaluation environment. */
   /*====================================*/

   if (lhsBinds != NULL)
     {
      oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
      oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
      oldJoin = EngineData(theEnv)->GlobalJoin;
      EngineData(theEnv)->GlobalRHSBinds = rhsBinds;
      EngineData(theEnv)->GlobalJoin = join;
      restore = true;
     }

   /*===================================================*/
   /* Compare each set of binds on the opposite side of */
   /* the join with the set of binds that entered this  */
   /* join. If the binds don't mismatch, then perfoCL_rm   */
   /* the appropriate action for the logic of the join. */
   /*===================================================*/

   while (lhsBinds != NULL)
     {
      nextBind = lhsBinds->nextInMemory;
      join->memoryCompares++;

      /*===========================================================*/
      /* Initialize some variables pointing to the partial matches */
      /* in the LHS and RHS of the join.                           */
      /*===========================================================*/

      if (lhsBinds->hashValue != rhsBinds->hashValue)
        {
#if DEVELOPER
         if (join->leftMemory->size == 1)
           { EngineData(theEnv)->betaHashListSkips++; }
         else
           { EngineData(theEnv)->betaHashHTSkips++; }

         if (lhsBinds->marker != NULL)
           { EngineData(theEnv)->unneededMarkerCompare++; }
#endif
         lhsBinds = nextBind;
         continue;
        }

      /*===============================================================*/
      /* If there already is an associated RHS partial match stored in */
      /* the LHS partial match from the beta memory of this join, then */
      /* the exists/nand CE has already been satisfied and we can move */
      /* on to the next partial match found in the beta memory.        */
      /*===============================================================*/

      if (lhsBinds->marker != NULL)
        {
#if DEVELOPER
         EngineData(theEnv)->unneededMarkerCompare++;
#endif
         lhsBinds = nextBind;
         continue;
        }

      /*===================================================*/
      /* If the join has no expression associated with it, */
      /* then the new partial match derived from the LHS   */
      /* and RHS partial matches is valid.                 */
      /*===================================================*/

      if (join->networkTest == NULL)
        { exprResult = true; }

      /*=========================================================*/
      /* If the join has an expression associated with it, then  */
      /* evaluate the expression to deteCL_rmine if the new partial */
      /* match derived from the LHS and RHS partial matches is   */
      /* valid (i.e. variable bindings are consistent and        */
      /* predicate expressions evaluate to true).                */
      /*=========================================================*/

      else
        {
#if DEVELOPER
         EngineData(theEnv)->rightToLeftComparisons++;
#endif
         EngineData(theEnv)->GlobalLHSBinds = lhsBinds;
         exprResult = CL_EvaluateJoinExpression(theEnv,join->networkTest,join);
         if (CL_EvaluationData(theEnv)->CL_EvaluationError)
           {
            if (join->patternIsNegated) exprResult = true;
            SetCL_EvaluationError(theEnv,false);
           }

#if DEVELOPER
         if (exprResult)
           { EngineData(theEnv)->rightToLeftSucceeds++; }
#endif
        }

      if ((join->secondaryNetworkTest != NULL) && exprResult)
        {
         EngineData(theEnv)->GlobalLHSBinds = lhsBinds;
         exprResult = CL_EvaluateJoinExpression(theEnv,join->secondaryNetworkTest,join);
         if (CL_EvaluationData(theEnv)->CL_EvaluationError)
           { SetCL_EvaluationError(theEnv,false); }
        }

      /*====================================================*/
      /* If the join expression evaluated to true (i.e.     */
      /* there were no conflicts between variable bindings, */
      /* all tests were satisfied, etc.), then perfoCL_rm the  */
      /* appropriate action given the logic of this join.   */
      /*====================================================*/

      if (exprResult != false)
        {
         if (join->patternIsExists)
           {
            CL_AddBlockedLink(lhsBinds,rhsBinds);
            CL_PPDrive(theEnv,lhsBinds,NULL,join,operation);
           }
         else if (join->patternIsNegated || join->joinFromTheRight)
           {
            CL_AddBlockedLink(lhsBinds,rhsBinds);
            if (lhsBinds->children != NULL)
              { CL_PosEntryCL_RetractBeta(theEnv,lhsBinds,lhsBinds->children,operation); }
            /*
            if (lhsBinds->dependents != NULL)
              { CL_RemoveLogicalSupport(theEnv,lhsBinds); }
            */
           }
         else
           { CL_PPDrive(theEnv,lhsBinds,rhsBinds,join,operation); }
        }

      /*====================================*/
      /* Move on to the next partial match. */
      /*====================================*/

      lhsBinds = nextBind;
     }

   /*=========================================*/
   /* Restore the old evaluation environment. */
   /*=========================================*/

   if (restore)
     {
      EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
      EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
      EngineData(theEnv)->GlobalJoin = oldJoin;
     }

   return;
  }

/****************************************************/
/* NetworkCL_AssertLeft: Primary routine for filtering */
/*   a partial match through the join network when  */
/*   entering through the left side of a join.      */
/****************************************************/
void NetworkCL_AssertLeft(
  Environment *theEnv,
  struct partialMatch *lhsBinds,
  struct joinNode *join,
  int operation)
  {
   struct partialMatch *rhsBinds;
   bool exprResult, restore = false;
   unsigned long entryHashValue;
   struct partialMatch *oldLHSBinds = NULL;
   struct partialMatch *oldRHSBinds = NULL;
   struct joinNode *oldJoin = NULL;

   if ((operation == NETWORK_RETRACT) && CL_PartialMatchWillBeDeleted(theEnv,lhsBinds))
     { return; }

   /*=========================================================*/
   /* If an incremental reset is being perfoCL_rmed and the join */
   /* is not part of the network to be reset, then return.    */
   /*=========================================================*/

#if (! BLOAD_ONLY) && (! RUN_TIME)
   if (EngineData(theEnv)->CL_IncrementalCL_ResetInProgress && (join->initialize == false)) return;
#endif

   /*===================================*/
   /* The only action for the last join */
   /* of a rule is to activate it.      */
   /*===================================*/

   if (join->ruleToActivate != NULL)
     {
      CL_AddActivation(theEnv,join->ruleToActivate,lhsBinds);
      return;
     }

   /*=====================================*/
   /* Handle a join handling a test CE at */
   /* the beginning of a not/and group.   */
   /*=====================================*/

   if (join->rightSideEntryStructure == NULL)
     {
      exprResult = true;

      if (join->networkTest != NULL)
        {
         oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
         oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
         oldJoin = EngineData(theEnv)->GlobalJoin;

         EngineData(theEnv)->GlobalLHSBinds = lhsBinds;
         EngineData(theEnv)->GlobalRHSBinds = NULL;
         EngineData(theEnv)->GlobalJoin = join;

         exprResult = CL_EvaluateJoinExpression(theEnv,join->networkTest,join);
         if (CL_EvaluationData(theEnv)->CL_EvaluationError)
           { SetCL_EvaluationError(theEnv,false); }

         EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
         EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
         EngineData(theEnv)->GlobalJoin = oldJoin;
        }

      if (exprResult)
        { CL_PPDrive(theEnv,lhsBinds,NULL,join,operation); }

      return;
     }

   /*==================================================*/
   /* Initialize some variables used to indicate which */
   /* side is being compared to the new partial match. */
   /*==================================================*/

   entryHashValue = lhsBinds->hashValue;
   if (join->joinFromTheRight)
     { rhsBinds = CL_GetRightBetaMemory(join,entryHashValue); }
   else
     { rhsBinds = CL_GetAlphaMemory(theEnv,(struct patternNodeHeader *) join->rightSideEntryStructure,entryHashValue); }

#if DEVELOPER
   if (rhsBinds != NULL)
     { EngineData(theEnv)->leftToRightLoops++; }
#endif

   /*====================================*/
   /* Set up the evaluation environment. */
   /*====================================*/

   if ((rhsBinds != NULL) || (join->secondaryNetworkTest != NULL))
     {
      oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
      oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
      oldJoin = EngineData(theEnv)->GlobalJoin;
      EngineData(theEnv)->GlobalLHSBinds = lhsBinds;
      EngineData(theEnv)->GlobalJoin = join;
      restore = true;
     }

   /*===================================================*/
   /* Compare each set of binds on the opposite side of */
   /* the join with the set of binds that entered this  */
   /* join. If the binds don't mismatch, then perfoCL_rm   */
   /* the appropriate action for the logic of the join. */
   /*===================================================*/

   while (rhsBinds != NULL)
     {
      if ((operation == NETWORK_RETRACT) && CL_PartialMatchWillBeDeleted(theEnv,rhsBinds))
        {
         rhsBinds = rhsBinds->nextInMemory;
         continue;
        }

      join->memoryCompares++;

      /*===================================================*/
      /* If the join has no expression associated with it, */
      /* then the new partial match derived from the LHS   */
      /* and RHS partial matches is valid.                 */
      /*===================================================*/

      if (join->networkTest == NULL)
        { exprResult = true; }

      /*=========================================================*/
      /* If the join has an expression associated with it, then  */
      /* evaluate the expression to deteCL_rmine if the new partial */
      /* match derived from the LHS and RHS partial matches is   */
      /* valid (i.e. variable bindings are consistent and        */
      /* predicate expressions evaluate to true).                */
      /*=========================================================*/

      else
        {
#if DEVELOPER
         EngineData(theEnv)->leftToRightComparisons++;
#endif
         EngineData(theEnv)->GlobalRHSBinds = rhsBinds;

         exprResult = CL_EvaluateJoinExpression(theEnv,join->networkTest,join);
         if (CL_EvaluationData(theEnv)->CL_EvaluationError)
           {
            if (join->patternIsNegated) exprResult = true;
            SetCL_EvaluationError(theEnv,false);
           }

#if DEVELOPER
         if (exprResult)
           { EngineData(theEnv)->leftToRightSucceeds++; }
#endif
        }

      // Bug Fix - Need to evaluate secondary network test for exists CE 0881

      if ((join->secondaryNetworkTest != NULL) && exprResult && join->patternIsExists)
        {
         EngineData(theEnv)->GlobalRHSBinds = rhsBinds;
         exprResult = CL_EvaluateJoinExpression(theEnv,join->secondaryNetworkTest,join);
         if (CL_EvaluationData(theEnv)->CL_EvaluationError)
           { SetCL_EvaluationError(theEnv,false); }
        }

      /*====================================================*/
      /* If the join expression evaluated to true (i.e.     */
      /* there were no conflicts between variable bindings, */
      /* all tests were satisfied, etc.), then perfoCL_rm the  */
      /* appropriate action given the logic of this join.   */
      /*====================================================*/

      if (exprResult != false)
        {
         /*==============================================*/
         /* Use the CL_PPDrive routine when the join isn't  */
         /* associated with a not CE and it doesn't have */
         /* a join from the right.                       */
         /*==============================================*/

         if ((join->patternIsNegated == false) &&
             (join->patternIsExists == false) &&
             (join->joinFromTheRight == false))
           { CL_PPDrive(theEnv,lhsBinds,rhsBinds,join,operation); }

         /*==================================================*/
         /* At most, one partial match will be generated for */
         /* a match from the right memory of an exists CE.   */
         /*==================================================*/

         else if (join->patternIsExists)
           {
            CL_AddBlockedLink(lhsBinds,rhsBinds);
            CL_PPDrive(theEnv,lhsBinds,NULL,join,operation);
            EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
            EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
            EngineData(theEnv)->GlobalJoin = oldJoin;
            return;
           }

         /*===========================================================*/
         /* If the new partial match entered from the LHS of the join */
         /* and the join is either associated with a not CE or the    */
         /* join has a join from the right, then mark the LHS partial */
         /* match indicating that there is a RHS partial match        */
         /* preventing this join from being satisfied. Once this has  */
         /* happened, the other RHS partial matches don't have to be  */
         /* tested since it only takes one partial match to prevent   */
         /* the LHS from being satisfied.                             */
         /*===========================================================*/

         else
           {
            CL_AddBlockedLink(lhsBinds,rhsBinds);
            break;
           }
        }

      /*====================================*/
      /* Move on to the next partial match. */
      /*====================================*/

      rhsBinds = rhsBinds->nextInMemory;
     }

   /*==================================================================*/
   /* If a join with an associated not CE or join from the right was   */
   /* entered from the LHS side of the join, and the join expression   */
   /* failed for all sets of matches for the new bindings on the LHS   */
   /* side (there was no RHS partial match preventing the LHS partial  */
   /* match from being satisfied), then the LHS partial match appended */
   /* with an pseudo-fact that represents the instance of the not      */
   /* pattern or join from the right that was satisfied should be sent */
   /* to the joins below this join.                                    */
   /*==================================================================*/

   if ((join->patternIsNegated || join->joinFromTheRight) &&
       (! join->patternIsExists) &&
       (lhsBinds->marker == NULL))
     {
      if (join->secondaryNetworkTest != NULL)
        {
         EngineData(theEnv)->GlobalRHSBinds = NULL;

         exprResult = CL_EvaluateJoinExpression(theEnv,join->secondaryNetworkTest,join);
         if (CL_EvaluationData(theEnv)->CL_EvaluationError)
           { SetCL_EvaluationError(theEnv,false); }

         if (exprResult)
            { CL_PPDrive(theEnv,lhsBinds,NULL,join,operation); }
		}
      else
        { CL_PPDrive(theEnv,lhsBinds,NULL,join,operation); }
     }

   /*=========================================*/
   /* Restore the old evaluation environment. */
   /*=========================================*/

   if (restore)
     {
      EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
      EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
      EngineData(theEnv)->GlobalJoin = oldJoin;
     }

   return;
  }

/*******************************************************/
/* CL_EvaluateJoinExpression: CL_Evaluates join expressions. */
/*   PerfoCL_rms a faster evaluation for join expressions */
/*   than if CL_EvaluateExpression was used directly.     */
/*******************************************************/
bool CL_EvaluateJoinExpression(
  Environment *theEnv,
  struct expr *joinExpr,
  struct joinNode *joinPtr)
  {
   UDFValue theResult;
   bool andLogic, result = true;

   /*======================================*/
   /* A NULL expression evaluates to true. */
   /*======================================*/

   if (joinExpr == NULL) return true;

   /*====================================================*/
   /* Initialize some variables which allow this routine */
   /* to avoid calling the "and" and "or" functions if   */
   /* they are the first part of the expression to be    */
   /* evaluated. Most of the join expressions do not use */
   /* deeply nested and/or functions so this technique   */
   /* speeds up evaluation.                              */
   /*====================================================*/

   if (joinExpr->value == ExpressionData(theEnv)->PTR_AND)
     {
      andLogic = true;
      joinExpr = joinExpr->argList;
     }
   else if (joinExpr->value == ExpressionData(theEnv)->PTR_OR)
     {
      andLogic = false;
      joinExpr = joinExpr->argList;
     }
   else
     { andLogic = true; }

   /*=========================================*/
   /* CL_Evaluate each of the expressions linked */
   /* together in the join expression.        */
   /*=========================================*/

   while (joinExpr != NULL)
     {
      /*================================*/
      /* CL_Evaluate a primitive function. */
      /*================================*/

      if ((CL_EvaluationData(theEnv)->PrimitivesArray[joinExpr->type] == NULL) ?
          false :
          CL_EvaluationData(theEnv)->PrimitivesArray[joinExpr->type]->evaluateFunction != NULL)
        {
         struct expr *oldArgument;

         oldArgument = CL_EvaluationData(theEnv)->CurrentExpression;
         CL_EvaluationData(theEnv)->CurrentExpression = joinExpr;
         result = (*CL_EvaluationData(theEnv)->PrimitivesArray[joinExpr->type]->evaluateFunction)(theEnv,joinExpr->value,&theResult);
         CL_EvaluationData(theEnv)->CurrentExpression = oldArgument;
        }

      /*=============================*/
      /* CL_Evaluate the "or" function. */
      /*=============================*/

      else if (joinExpr->value == ExpressionData(theEnv)->PTR_OR)
        {
         result = false;
         if (CL_EvaluateJoinExpression(theEnv,joinExpr,joinPtr) == true)
           {
            if (CL_EvaluationData(theEnv)->CL_EvaluationError)
              { return false; }
            result = true;
           }
         else if (CL_EvaluationData(theEnv)->CL_EvaluationError)
           { return false; }
        }

      /*==============================*/
      /* CL_Evaluate the "and" function. */
      /*==============================*/

      else if (joinExpr->value == ExpressionData(theEnv)->PTR_AND)
        {
         result = true;
         if (CL_EvaluateJoinExpression(theEnv,joinExpr,joinPtr) == false)
           {
            if (CL_EvaluationData(theEnv)->CL_EvaluationError)
              { return false; }
            result = false;
           }
         else if (CL_EvaluationData(theEnv)->CL_EvaluationError)
           { return false; }
        }

      /*==========================================================*/
      /* CL_Evaluate all other expressions using CL_EvaluateExpression. */
      /*==========================================================*/

      else
        {
         CL_EvaluateExpression(theEnv,joinExpr,&theResult);

         if (CL_EvaluationData(theEnv)->CL_EvaluationError)
           {
            JoinNetErrorMessage(theEnv,joinPtr);
            return false;
           }

         if (theResult.value == FalseSymbol(theEnv))
           { result = false; }
         else
           { result = true; }
        }

      /*====================================*/
      /* Handle the short cut evaluation of */
      /* the "and" and "or" functions.      */
      /*====================================*/

      if ((andLogic == true) && (result == false))
        { return false; }
      else if ((andLogic == false) && (result == true))
        { return true; }

      /*==============================================*/
      /* Move to the next expression to be evaluated. */
      /*==============================================*/

      joinExpr = joinExpr->nextArg;
     }

   /*=================================================*/
   /* Return the result of evaluating the expression. */
   /*=================================================*/

   return(result);
  }

/*********************************/
/* CL_EvaluateSecondaryNetworkTest: */
/*********************************/
bool CL_EvaluateSecondaryNetworkTest(
  Environment *theEnv,
  struct partialMatch *leftMatch,
  struct joinNode *joinPtr)
  {
   bool joinExpr;
   struct partialMatch *oldLHSBinds;
   struct partialMatch *oldRHSBinds;
   struct joinNode *oldJoin;

   if (joinPtr->secondaryNetworkTest == NULL)
     { return true; }

#if DEVELOPER
   EngineData(theEnv)->rightToLeftComparisons++;
#endif
   oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
   oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
   oldJoin = EngineData(theEnv)->GlobalJoin;
   EngineData(theEnv)->GlobalLHSBinds = leftMatch;
   EngineData(theEnv)->GlobalRHSBinds = NULL;
   EngineData(theEnv)->GlobalJoin = joinPtr;

   joinExpr = CL_EvaluateJoinExpression(theEnv,joinPtr->secondaryNetworkTest,joinPtr);
   CL_EvaluationData(theEnv)->CL_EvaluationError = false;

   EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
   EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
   EngineData(theEnv)->GlobalJoin = oldJoin;

   return(joinExpr);
  }

/************************/
/* CL_BetaMemoryHashValue: */
/************************/
unsigned long CL_BetaMemoryHashValue(
  Environment *theEnv,
  struct expr *hashExpr,
  struct partialMatch *lbinds,
  struct partialMatch *rbinds,
  struct joinNode *joinPtr)
  {
   UDFValue theResult;
   struct partialMatch *oldLHSBinds;
   struct partialMatch *oldRHSBinds;
   struct joinNode *oldJoin;
   unsigned long hashValue = 0;
   unsigned long multiplier = 1;
   union
     {
      void *vv;
      unsigned long liv;
     } fis;

   /*======================================*/
   /* A NULL expression evaluates to zero. */
   /*======================================*/

   if (hashExpr == NULL)
     { return 0; }

   /*=========================================*/
   /* Initialize some of the global variables */
   /* used when evaluating expressions.       */
   /*=========================================*/

   oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
   oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
   oldJoin = EngineData(theEnv)->GlobalJoin;
   EngineData(theEnv)->GlobalLHSBinds = lbinds;
   EngineData(theEnv)->GlobalRHSBinds = rbinds;
   EngineData(theEnv)->GlobalJoin = joinPtr;

   /*=========================================*/
   /* CL_Evaluate each of the expressions linked */
   /* together in the join expression.        */
   /*=========================================*/

   while (hashExpr != NULL)
     {
      /*================================*/
      /* CL_Evaluate a primitive function. */
      /*================================*/

      if ((CL_EvaluationData(theEnv)->PrimitivesArray[hashExpr->type] == NULL) ?
          false :
          CL_EvaluationData(theEnv)->PrimitivesArray[hashExpr->type]->evaluateFunction != NULL)
        {
         struct expr *oldArgument;

         oldArgument = CL_EvaluationData(theEnv)->CurrentExpression;
         CL_EvaluationData(theEnv)->CurrentExpression = hashExpr;
         (*CL_EvaluationData(theEnv)->PrimitivesArray[hashExpr->type]->evaluateFunction)(theEnv,hashExpr->value,&theResult);
         CL_EvaluationData(theEnv)->CurrentExpression = oldArgument;
        }

      /*==========================================================*/
      /* CL_Evaluate all other expressions using CL_EvaluateExpression. */
      /*==========================================================*/

      else
        { CL_EvaluateExpression(theEnv,hashExpr,&theResult); }

      switch (theResult.header->type)
        {
         case STRING_TYPE:
         case SYMBOL_TYPE:
         case INSTANCE_NAME_TYPE:
           hashValue += theResult.lexemeValue->bucket * multiplier;
           break;

         case INTEGER_TYPE:
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

      /*==============================================*/
      /* Move to the next expression to be evaluated. */
      /*==============================================*/

      hashExpr = hashExpr->nextArg;
	  multiplier = multiplier * 509;
	 }

   /*=======================================*/
   /* Restore some of the global variables. */
   /*=======================================*/

   EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
   EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
   EngineData(theEnv)->GlobalJoin = oldJoin;

   /*=================================================*/
   /* Return the result of evaluating the expression. */
   /*=================================================*/

   return hashValue;
  }

/*******************************************************************/
/* CL_PPDrive: Handles the merging of an alpha memory partial match   */
/*   with a beta memory partial match for a join that has positive */
/*   LHS entry and positive RHS entry. The partial matches being   */
/*   merged have previously been checked to deteCL_rmine that they    */
/*   satisify the constraints for the join. Once merged, the new   */
/*   partial match is sent to each child join of the join from     */
/*   which the merge took place.                                   */
/*******************************************************************/
void CL_PPDrive(
  Environment *theEnv,
  struct partialMatch *lhsBinds,
  struct partialMatch *rhsBinds,
  struct joinNode *join,
  int operation)
  {
   struct partialMatch *linker;
   struct joinLink *listOfJoins;
   unsigned long hashValue;

   /*================================================*/
   /* CL_Send the new partial match to all child joins. */
   /*================================================*/

   listOfJoins = join->nextLinks;
   if (listOfJoins == NULL) return;

   /*===============================================================*/
   /* In the current implementation, all children of this join must */
   /* be entered from the same side (either all left or all right). */
   /*===============================================================*/

   while (listOfJoins != NULL)
     {
      /*==================================================*/
      /* Merge the alpha and beta memory partial matches. */
      /*==================================================*/

      linker = MergePartialCL_Matches(theEnv,lhsBinds,rhsBinds);

      /*================================================*/
      /* DeteCL_rmine the hash value of the partial match. */
      /*================================================*/

      if (listOfJoins->enterDirection == LHS)
        {
         if (listOfJoins->join->leftHash != NULL)
           { hashValue = CL_BetaMemoryHashValue(theEnv,listOfJoins->join->leftHash,linker,NULL,listOfJoins->join); }
         else
           { hashValue = 0; }
        }
      else
        {
         if (listOfJoins->join->rightHash != NULL)
           { hashValue = CL_BetaMemoryHashValue(theEnv,listOfJoins->join->rightHash,linker,NULL,listOfJoins->join); }
         else
           { hashValue = 0; }
        }

      /*=======================================================*/
      /* Add the partial match to the beta memory of the join. */
      /*=======================================================*/

      CL_UpdateBetaPMLinks(theEnv,linker,lhsBinds,rhsBinds,listOfJoins->join,hashValue,listOfJoins->enterDirection);

      if (listOfJoins->enterDirection == LHS)
        { NetworkCL_AssertLeft(theEnv,linker,listOfJoins->join,operation); }
      else
        { NetworkCL_AssertRight(theEnv,linker,listOfJoins->join,operation); }

      listOfJoins = listOfJoins->next;
     }

   return;
  }

/***********************************************************************/
/* CL_EPMDrive: Drives an empty partial match to the next level of joins. */
/*   An empty partial match is usually associated with a negated CE    */
/*   that is the first CE of a rule.                                   */
/***********************************************************************/
void CL_EPMDrive(
  Environment *theEnv,
  struct partialMatch *parent,
  struct joinNode *join,
  int operation)
  {
   struct partialMatch *linker;
   struct joinLink *listOfJoins;

   listOfJoins = join->nextLinks;
   if (listOfJoins == NULL) return;

   while (listOfJoins != NULL)
     {
      linker = CL_CreateEmptyPartialMatch(theEnv);

      CL_UpdateBetaPMLinks(theEnv,linker,parent,NULL,listOfJoins->join,0,listOfJoins->enterDirection);

      if (listOfJoins->enterDirection == LHS)
        { NetworkCL_AssertLeft(theEnv,linker,listOfJoins->join,operation); }
      else
        { NetworkCL_AssertRight(theEnv,linker,listOfJoins->join,operation); }

      listOfJoins = listOfJoins->next;
     }
  }

/***************************************************************/
/* EmptyDrive: Handles the entry of a alpha memory partial     */
/*   match from the RHS of a join that is the first join of    */
/*   a rule (i.e. a join that cannot be entered from the LHS). */
/***************************************************************/
static void EmptyDrive(
  Environment *theEnv,
  struct joinNode *join,
  struct partialMatch *rhsBinds,
  int operation)
  {
   struct partialMatch *linker, *existsParent = NULL, *notParent;
   struct joinLink *listOfJoins;
   bool joinExpr;
   unsigned long hashValue;
   struct partialMatch *oldLHSBinds;
   struct partialMatch *oldRHSBinds;
   struct joinNode *oldJoin;

   /*======================================================*/
   /* DeteCL_rmine if the alpha memory partial match satifies */
   /* the join expression. If it doesn't then no further   */
   /* action is taken.                                     */
   /*======================================================*/

   if (join->networkTest != NULL)
     {

#if DEVELOPER
      EngineData(theEnv)->rightToLeftComparisons++;
#endif
      oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
      oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
      oldJoin = EngineData(theEnv)->GlobalJoin;
      EngineData(theEnv)->GlobalLHSBinds = NULL;
      EngineData(theEnv)->GlobalRHSBinds = rhsBinds;
      EngineData(theEnv)->GlobalJoin = join;

      joinExpr = CL_EvaluateJoinExpression(theEnv,join->networkTest,join);
      CL_EvaluationData(theEnv)->CL_EvaluationError = false;

      EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
      EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
      EngineData(theEnv)->GlobalJoin = oldJoin;

      if (joinExpr == false) return;
     }

   if (join->secondaryNetworkTest != NULL)
     {
#if DEVELOPER
      EngineData(theEnv)->rightToLeftComparisons++;
#endif
      oldLHSBinds = EngineData(theEnv)->GlobalLHSBinds;
      oldRHSBinds = EngineData(theEnv)->GlobalRHSBinds;
      oldJoin = EngineData(theEnv)->GlobalJoin;
      EngineData(theEnv)->GlobalLHSBinds = NULL;
      EngineData(theEnv)->GlobalRHSBinds = rhsBinds;
      EngineData(theEnv)->GlobalJoin = join;

      joinExpr = CL_EvaluateJoinExpression(theEnv,join->secondaryNetworkTest,join);
      CL_EvaluationData(theEnv)->CL_EvaluationError = false;

      EngineData(theEnv)->GlobalLHSBinds = oldLHSBinds;
      EngineData(theEnv)->GlobalRHSBinds = oldRHSBinds;
      EngineData(theEnv)->GlobalJoin = oldJoin;

      if (joinExpr == false) return;
     }

   /*========================================================*/
   /* Handle a negated first pattern or join from the right. */
   /*========================================================*/

   if (join->patternIsNegated || (join->joinFromTheRight && (! join->patternIsExists))) /* reorder to remove patternIsExists test */
     {
      notParent = join->leftMemory->beta[0];
      if (notParent->marker != NULL)
        { return; }

      CL_AddBlockedLink(notParent,rhsBinds);

      if (notParent->children != NULL)
        { CL_PosEntryCL_RetractBeta(theEnv,notParent,notParent->children,operation); }
      /*
      if (notParent->dependents != NULL)
		{ CL_RemoveLogicalSupport(theEnv,notParent); }
        */

      return;
     }

   /*=====================================================*/
   /* For exists CEs used as the first pattern of a rule, */
   /* a special partial match in the left memory of the   */
   /* join is used to track the RHS partial match         */
   /* satisfying the CE.                                  */
   /*=====================================================*/
  /* TBD reorder */
   if (join->patternIsExists)
     {
      existsParent = join->leftMemory->beta[0];
      if (existsParent->marker != NULL)
        { return; }
      CL_AddBlockedLink(existsParent,rhsBinds);
     }

   /*============================================*/
   /* CL_Send the partial match to all child joins. */
   /*============================================*/

   listOfJoins = join->nextLinks;
   if (listOfJoins == NULL) return;

   while (listOfJoins != NULL)
     {
      /*===================================================================*/
      /* An exists CE as the first pattern of a rule can generate at most  */
      /* one partial match, so if there's already a partial match in the   */
      /* beta memory nothing further needs to be done. Since there are no  */
      /* variable bindings which child joins can use for indexing, the     */
      /* partial matches will always be stored in the bucket with index 0. */
      /* Although an exists is associated with a specific fact/instance    */
      /* (through its rightParent link) that allows it to be satisfied,    */
      /* the bindings in the partial match will be empty for this CE.      */
      /*===================================================================*/

      if (join->patternIsExists)
        { linker = CL_CreateEmptyPartialMatch(theEnv); }

      /*=============================================================*/
      /* Othewise just copy the partial match from the alpha memory. */
      /*=============================================================*/

      else
        { linker = CL_CopyPartialMatch(theEnv,rhsBinds); }

      /*================================================*/
      /* DeteCL_rmine the hash value of the partial match. */
      /*================================================*/

      if (listOfJoins->enterDirection == LHS)
        {
         if (listOfJoins->join->leftHash != NULL)
           { hashValue = CL_BetaMemoryHashValue(theEnv,listOfJoins->join->leftHash,linker,NULL,listOfJoins->join); }
         else
           { hashValue = 0; }
        }
      else
        {
         if (listOfJoins->join->rightHash != NULL)
           { hashValue = CL_BetaMemoryHashValue(theEnv,listOfJoins->join->rightHash,linker,NULL,listOfJoins->join); }
         else
           { hashValue = 0; }
        }

      /*=======================================================*/
      /* Add the partial match to the beta memory of the join. */
      /*=======================================================*/

      if (join->patternIsExists)
        { CL_UpdateBetaPMLinks(theEnv,linker,existsParent,NULL,listOfJoins->join,hashValue,listOfJoins->enterDirection); }
      else
        { CL_UpdateBetaPMLinks(theEnv,linker,NULL,rhsBinds,listOfJoins->join,hashValue,listOfJoins->enterDirection); }

      if (listOfJoins->enterDirection == LHS)
        { NetworkCL_AssertLeft(theEnv,linker,listOfJoins->join,operation); }
      else
        { NetworkCL_AssertRight(theEnv,linker,listOfJoins->join,operation); }

      listOfJoins = listOfJoins->next;
     }
  }

/********************************************************************/
/* JoinNetErrorMessage: Prints an infoCL_rmational message indicating  */
/*   which join of a rule generated an error when a join expression */
/*   was being evaluated.                                           */
/********************************************************************/
static void JoinNetErrorMessage(
  Environment *theEnv,
  struct joinNode *joinPtr)
  {
   CL_PrintErrorID(theEnv,"DRIVE",1,true);
   CL_WriteString(theEnv,STDERR,"This error occurred in the join network.\n");
   CL_WriteString(theEnv,STDERR,"   Problem resides in associated join\n"); /* TBD generate test case for join with JFTR */
   CL_TraceErrorToRule(theEnv,joinPtr,"      ");
   CL_WriteString(theEnv,STDERR,"\n");
  }

#endif /* DEFRULE_CONSTRUCT */
