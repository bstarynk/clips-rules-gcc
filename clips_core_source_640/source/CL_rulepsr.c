   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  06/28/17             */
   /*                                                     */
   /*                 RULE PARSING MODULE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose:  Parses a defrule construct.                     */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed DYNAMIC_SALIENCE, INCREMENTAL_RESET,   */
/*            and LOGICAL_DEPENDENCIES compilation flags.    */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            CL_GetConstructNameAndComment API change.         */
/*                                                           */
/*            Added support for hashed memories.             */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
/*                                                           */
/*      6.31: DR#882 Logical retraction not working if       */
/*            logical CE starts with test CE.                */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFRULE_CONSTRUCT

#include <stdio.h>
#include <string.h>

#include "analysis.h"
#include "constant.h"
#include "constrct.h"
#include "cstrcpsr.h"
#include "cstrnchk.h"
#include "cstrnops.h"
#include "engine.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "incrrset.h"
#include "memalloc.h"
#include "modulutl.h"
#include "pattern.h"
#include "prccode.h"
#include "prcdrpsr.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "rulebld.h"
#include "rulebsc.h"
#include "rulecstr.h"
#include "ruledef.h"
#include "ruledlt.h"
#include "rulelhs.h"
#include "scanner.h"
#include "symbol.h"
#include "watch.h"

#include "lgcldpnd.h"

#if DEFTEMPLATE_CONSTRUCT
#include "tmpltfun.h"
#endif

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "bload.h"
#endif

#include "rulepsr.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if (! RUN_TIME) && (! BLOAD_ONLY)
static struct expr *ParseRuleRHS (Environment *, const char *);
static int ReplaceRHSVariable (Environment *, struct expr *, void *);
static Defrule *ProcessRuleLHS (Environment *, struct lhsParseNode *,
				struct expr *, CLIPSLexeme *, bool *);
static Defrule *CreateNewDisjunct (Environment *, CLIPSLexeme *,
				   unsigned short, struct expr *,
				   unsigned int, unsigned, struct joinNode *);
static unsigned short RuleComplexity (Environment *, struct lhsParseNode *);
static unsigned short ExpressionComplexity (Environment *, struct expr *);
static int LogicalAnalysis (Environment *, struct lhsParseNode *);
static void AddToDefruleList (Defrule *);
#endif

/****************************************************/
/* CL_ParseDefrule: Coordinates all actions necessary  */
/*   for the parsing and creation of a defrule into */
/*   the current environment.                       */
/****************************************************/
bool
CL_ParseDefrule (Environment * theEnv, const char *readSource)
{
#if (! RUN_TIME) && (! BLOAD_ONLY)
  CLIPSLexeme *ruleName;
  struct lhsParseNode *theLHS;
  struct expr *actions;
  struct token theToken;
  Defrule *topDisjunct, *tempPtr;
  struct defruleModule *theModuleItem;
  bool error;

   /*================================================*/
  /* Flush the buffer which stores the pretty print */
  /* representation for a rule.  Add the already    */
  /* parsed keyword defrule to this buffer.         */
   /*================================================*/

  CL_SetPPBufferStatus (theEnv, true);
  CL_FlushPPBuffer (theEnv);
  CL_SavePPBuffer (theEnv, "(defrule ");

   /*=========================================================*/
  /* Rules cannot be loaded when a binary load is in effect. */
   /*=========================================================*/

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
  if ((CL_Bloaded (theEnv) == true)
      && (!ConstructData (theEnv)->CL_CheckSyntaxMode))
    {
      Cannot_LoadWith_BloadMessage (theEnv, "defrule");
      return true;
    }
#endif

   /*================================================*/
  /* Parse the name and comment fields of the rule, */
  /* deleting the rule if it already exists.        */
   /*================================================*/

#if DEBUGGING_FUNCTIONS
  DefruleData (theEnv)->DeletedRuleDebugFlags = 0;
#endif

  ruleName =
    CL_GetConstructNameAndComment (theEnv, readSource, &theToken, "defrule",
				   (CL_FindConstructFunction *)
				   CL_FindDefruleInModule,
				   (DeleteConstructFunction *) CL_Undefrule,
				   "*", false, true, true, false);

  if (ruleName == NULL)
    return true;

   /*============================*/
  /* Parse the LHS of the rule. */
   /*============================*/

  theLHS =
    CL_ParseRuleLHS (theEnv, readSource, &theToken, ruleName->contents,
		     &error);
  if (error)
    {
      CL_ReturnPackedExpression (theEnv,
				 PatternData (theEnv)->SalienceExpression);
      PatternData (theEnv)->SalienceExpression = NULL;
      return true;
    }

   /*============================*/
  /* Parse the RHS of the rule. */
   /*============================*/

  CL_ClearParsedBindNames (theEnv);
  ExpressionData (theEnv)->ReturnContext = true;
  actions = ParseRuleRHS (theEnv, readSource);

  if (actions == NULL)
    {
      CL_ReturnPackedExpression (theEnv,
				 PatternData (theEnv)->SalienceExpression);
      PatternData (theEnv)->SalienceExpression = NULL;
      CL_ReturnLHSParseNodes (theEnv, theLHS);
      return true;
    }

   /*=======================*/
  /* Process the rule LHS. */
   /*=======================*/

  topDisjunct = ProcessRuleLHS (theEnv, theLHS, actions, ruleName, &error);

  CL_ReturnExpression (theEnv, actions);
  CL_ClearParsedBindNames (theEnv);
  CL_ReturnLHSParseNodes (theEnv, theLHS);

  if (error)
    {
      CL_ReturnPackedExpression (theEnv,
				 PatternData (theEnv)->SalienceExpression);
      PatternData (theEnv)->SalienceExpression = NULL;
      return true;
    }

   /*==============================================*/
  /* If we're only checking syntax, don't add the */
  /* successfully parsed defrule to the KB.       */
   /*==============================================*/

  if (ConstructData (theEnv)->CL_CheckSyntaxMode)
    {
      CL_ReturnPackedExpression (theEnv,
				 PatternData (theEnv)->SalienceExpression);
      PatternData (theEnv)->SalienceExpression = NULL;
      return false;
    }

  PatternData (theEnv)->SalienceExpression = NULL;

   /*======================================*/
  /* CL_Save the nice printout of the rules. */
   /*======================================*/

  CL_SavePPBuffer (theEnv, "\n");
  if (CL_GetConserveMemory (theEnv) == true)
    {
      topDisjunct->header.ppFo_rm = NULL;
    }
  else
    {
      topDisjunct->header.ppFo_rm = CL_CopyPPBuffer (theEnv);
    }

   /*=======================================*/
  /* Store a pointer to the rule's module. */
   /*=======================================*/

  theModuleItem = (struct defruleModule *)
    CL_GetModuleItem (theEnv, NULL,
		      CL_FindModuleItem (theEnv, "defrule")->moduleIndex);

  for (tempPtr = topDisjunct; tempPtr != NULL; tempPtr = tempPtr->disjunct)
    {
      tempPtr->header.whichModule =
	(struct defmoduleItemHeader *) theModuleItem;
      tempPtr->header.ppFo_rm = topDisjunct->header.ppFo_rm;
    }

   /*===============================================*/
  /* Rule completely parsed. Add to list of rules. */
   /*===============================================*/

  AddToDefruleList (topDisjunct);

   /*========================================================================*/
  /* If a rule is redefined, then we want to restore its breakpoint status. */
   /*========================================================================*/

#if DEBUGGING_FUNCTIONS
  if (BitwiseTest (DefruleData (theEnv)->DeletedRuleDebugFlags, 0))
    {
      CL_SetBreak (topDisjunct);
    }
  if (BitwiseTest (DefruleData (theEnv)->DeletedRuleDebugFlags, 1) ||
      (CL_Get_WatchItem (theEnv, "activations") == 1))
    {
      CL_DefruleSet_WatchActivations (topDisjunct, true);
    }
  if (BitwiseTest (DefruleData (theEnv)->DeletedRuleDebugFlags, 2) ||
      (CL_Get_WatchItem (theEnv, "rules") == 1))
    {
      CL_DefruleSet_WatchFirings (topDisjunct, true);
    }
#endif

   /*================================*/
  /* Perfo_rm the incremental reset. */
   /*================================*/

  CL_Incremental_Reset (theEnv, topDisjunct);

   /*=============================================*/
  /* Return false to indicate no errors occured. */
   /*=============================================*/

#endif
  return false;
}

#if (! RUN_TIME) && (! BLOAD_ONLY)

/**************************************************************/
/* ProcessRuleLHS: Processes each of the disjuncts of a rule. */
/**************************************************************/
static Defrule *
ProcessRuleLHS (Environment * theEnv,
		struct lhsParseNode *theLHS,
		struct expr *actions, CLIPSLexeme * ruleName, bool *error)
{
  struct lhsParseNode *tempNode = NULL;
  Defrule *topDisjunct = NULL, *currentDisjunct, *lastDisjunct = NULL;
  struct expr *newActions, *packPtr;
  int logicalJoin;
  unsigned short localVarCnt;
  unsigned short complexity;
  struct joinNode *lastJoin;
  bool emptyLHS;

   /*================================================*/
  /* Initially set the parsing error flag to false. */
   /*================================================*/

  *error = false;

   /*===========================================================*/
  /* The top level of the construct representing the LHS of a  */
  /* rule is assumed to be an OR.  If the implied OR is at the */
  /* top level of the pattern construct, then remove it.       */
   /*===========================================================*/

  if (theLHS == NULL)
    {
      emptyLHS = true;
    }
  else
    {
      emptyLHS = false;
      if (theLHS->pnType == OR_CE_NODE)
	theLHS = theLHS->right;
    }

   /*=========================================*/
  /* Loop through each disjunct of the rule. */
   /*=========================================*/

  localVarCnt = CL_CountParsedBindNames (theEnv);

  while ((theLHS != NULL) || (emptyLHS == true))
    {
      /*===================================*/
      /* Analyze the LHS of this disjunct. */
      /*===================================*/

      if (emptyLHS)
	{
	  tempNode = NULL;
	}
      else
	{
	  if (theLHS->pnType == AND_CE_NODE)
	    tempNode = theLHS->right;
	  else if (theLHS->pnType == PATTERN_CE_NODE)
	    tempNode = theLHS;
	}

      if (CL_VariableAnalysis (theEnv, tempNode))
	{
	  *error = true;
	  CL_ReturnDefrule (theEnv, topDisjunct);
	  return NULL;
	}

      /*=========================================*/
      /* Perfo_rm entity dependent post analysis. */
      /*=========================================*/

      if (CL_PostPatternAnalysis (theEnv, tempNode))
	{
	  *error = true;
	  CL_ReturnDefrule (theEnv, topDisjunct);
	  return NULL;
	}

      /*========================================================*/
      /* Print out developer info_rmation if it's being watched. */
      /*========================================================*/

#if DEVELOPER && DEBUGGING_FUNCTIONS
      if (CL_Get_WatchItem (theEnv, "rule-analysis") == 1)
	{
	  DumpRuleAnalysis (theEnv, tempNode);
	}
#endif

      /*======================================================*/
      /* Check to see if there are any RHS constraint errors. */
      /*======================================================*/

      if (CL_CheckRHSForConstraintErrors (theEnv, actions, tempNode))
	{
	  *error = true;
	  CL_ReturnDefrule (theEnv, topDisjunct);
	  return NULL;
	}

      /*=================================================*/
      /* Replace variable references in the RHS with the */
      /* appropriate variable retrieval functions.       */
      /*=================================================*/

      newActions = CL_CopyExpression (theEnv, actions);
      if (CL_ReplaceProcVars
	  (theEnv, "RHS of defrule", newActions, NULL, NULL,
	   ReplaceRHSVariable, tempNode))
	{
	  *error = true;
	  CL_ReturnDefrule (theEnv, topDisjunct);
	  CL_ReturnExpression (theEnv, newActions);
	  return NULL;
	}

      /*===================================================*/
      /* Remove any test CEs from the LHS and attach their */
      /* expression to the closest preceeding non-negated  */
      /* join at the same not/and depth.                   */
      /*===================================================*/

      CL_AttachTestCEsToPatternCEs (theEnv, tempNode);

      /*========================================*/
      /* Check to see that logical CEs are used */
      /* appropriately in the LHS of the rule.  */
      /*========================================*/

      if ((logicalJoin = LogicalAnalysis (theEnv, tempNode)) < 0)
	{
	  *error = true;
	  CL_ReturnDefrule (theEnv, topDisjunct);
	  CL_ReturnExpression (theEnv, newActions);
	  return NULL;
	}

      /*==================================*/
      /* We're finished for this disjunct */
      /* if we're only checking syntax.   */
      /*==================================*/

      if (ConstructData (theEnv)->CL_CheckSyntaxMode)
	{
	  CL_ReturnExpression (theEnv, newActions);
	  if (emptyLHS)
	    {
	      emptyLHS = false;
	    }
	  else
	    {
	      theLHS = theLHS->bottom;
	    }
	  continue;
	}

      /*=================================*/
      /* Install the disjunct's actions. */
      /*=================================*/

      CL_ExpressionInstall (theEnv, newActions);
      packPtr = CL_PackExpression (theEnv, newActions);
      CL_ReturnExpression (theEnv, newActions);

      /*===============================================================*/
      /* Create the pattern and join data structures for the new rule. */
      /*===============================================================*/

      lastJoin =
	CL_ConstructJoins (theEnv, logicalJoin, tempNode, 1, NULL, true,
			   true);

      /*===================================================================*/
      /* Dete_rmine the rule's complexity for use with conflict resolution. */
      /*===================================================================*/

      complexity = RuleComplexity (theEnv, tempNode);

      /*=====================================================*/
      /* Create the defrule data structure for this disjunct */
      /* and put it in the list of disjuncts for this rule.  */
      /*=====================================================*/

      currentDisjunct =
	CreateNewDisjunct (theEnv, ruleName, localVarCnt, packPtr, complexity,
			   (unsigned) logicalJoin, lastJoin);

      /*============================================================*/
      /* Place the disjunct in the list of disjuncts for this rule. */
      /* If the disjunct is the first disjunct, then increment the  */
      /* reference counts for the dynamic salience (the expression  */
      /* for the dynamic salience is only stored with the first     */
      /* disjuncts and the other disjuncts refer back to the first  */
      /* disjunct for their dynamic salience value.                 */
      /*============================================================*/

      if (topDisjunct == NULL)
	{
	  topDisjunct = currentDisjunct;
	  CL_ExpressionInstall (theEnv, topDisjunct->dynamicSalience);
	}
      else
	lastDisjunct->disjunct = currentDisjunct;

      /*===========================================*/
      /* Move on to the next disjunct of the rule. */
      /*===========================================*/

      lastDisjunct = currentDisjunct;

      if (emptyLHS)
	{
	  emptyLHS = false;
	}
      else
	{
	  theLHS = theLHS->bottom;
	}
    }

  return (topDisjunct);
}

/************************************************************************/
/* CreateNewDisjunct: Creates and initializes a defrule data structure. */
/************************************************************************/
static Defrule *
CreateNewDisjunct (Environment * theEnv,
		   CLIPSLexeme * ruleName,
		   unsigned short localVarCnt,
		   struct expr *theActions,
		   unsigned int complexity,
		   unsigned logicalJoin, struct joinNode *lastJoin)
{
  struct joinNode *tempJoin;
  Defrule *newDisjunct;

   /*===================================================*/
  /* Create and initialize the defrule data structure. */
   /*===================================================*/

  newDisjunct = get_struct (theEnv, defrule);
  newDisjunct->header.ppFo_rm = NULL;
  newDisjunct->header.next = NULL;
  newDisjunct->header.usrData = NULL;
  newDisjunct->logicalJoin = NULL;
  newDisjunct->disjunct = NULL;
  newDisjunct->header.constructType = DEFRULE;
  newDisjunct->header.env = theEnv;
  newDisjunct->header.name = ruleName;
  IncrementLexemeCount (newDisjunct->header.name);
  newDisjunct->actions = theActions;
  newDisjunct->salience = PatternData (theEnv)->GlobalSalience;
  newDisjunct->afterBreakpoint = 0;
  newDisjunct->watchActivation = 0;
  newDisjunct->watchFiring = 0;
  newDisjunct->executing = 0;
  newDisjunct->complexity = complexity;
  newDisjunct->auto_Focus = PatternData (theEnv)->GlobalAuto_Focus;
  newDisjunct->dynamicSalience = PatternData (theEnv)->SalienceExpression;
  newDisjunct->localVarCnt = localVarCnt;

   /*=====================================*/
  /* Add a pointer to the rule's module. */
   /*=====================================*/

  newDisjunct->header.whichModule =
    (struct defmoduleItemHeader *)
    CL_GetModuleItem (theEnv, NULL,
		      CL_FindModuleItem (theEnv, "defrule")->moduleIndex);

   /*============================================================*/
  /* Attach the rule's last join to the defrule data structure. */
   /*============================================================*/

  lastJoin->ruleToActivate = newDisjunct;
  newDisjunct->lastJoin = lastJoin;

   /*=================================================*/
  /* Dete_rmine the rule's logical join if it exists. */
   /*=================================================*/

  tempJoin = lastJoin;
  while (tempJoin != NULL)
    {
      if (tempJoin->depth == logicalJoin)
	{
	  newDisjunct->logicalJoin = tempJoin;
	  tempJoin->logicalJoin = true;
	}
      tempJoin = tempJoin->lastLevel;
    }

   /*==================================================*/
  /* Return the newly created defrule data structure. */
   /*==================================================*/

  return (newDisjunct);
}

/****************************************************************/
/* ReplaceExpressionVariables: Replaces all symbolic references */
/*   to variables (local and global) found in an expression on  */
/*   the RHS of a rule with expressions containing function     */
/*   calls to retrieve the variable's value. Makes the final    */
/*   modifications necessary for handling the modify and        */
/*   duplicate commands.                                        */
/****************************************************************/
static int
ReplaceRHSVariable (Environment * theEnv, struct expr *list, void *VtheLHS)
{
  struct lhsParseNode *theVariable;

   /*=======================================*/
  /* Handle modify and duplicate commands. */
   /*=======================================*/

#if DEFTEMPLATE_CONSTRUCT
  if (list->type == FCALL)
    {
      if (list->value == (void *) CL_FindFunction (theEnv, "modify"))
	{
	  if (CL_UpdateModifyDuplicate (theEnv, list, "modify", VtheLHS) ==
	      false)
	    {
	      return -1;
	    }
	}
      else if (list->value == (void *) CL_FindFunction (theEnv, "duplicate"))
	{
	  if (CL_UpdateModifyDuplicate (theEnv, list, "duplicate", VtheLHS) ==
	      false)
	    {
	      return -1;
	    }
	}

      return 0;
    }

#endif

  if ((list->type != SF_VARIABLE) && (list->type != MF_VARIABLE))
    {
      return 0;
    }

   /*===============================================================*/
  /* Check to see if the variable is bound on the LHS of the rule. */
   /*===============================================================*/

  theVariable =
    CL_FindVariable (list->lexemeValue, (struct lhsParseNode *) VtheLHS);
  if (theVariable == NULL)
    return 0;

   /*================================================*/
  /* Replace the variable reference with a function */
  /* call to retrieve the variable.                 */
   /*================================================*/

  if (theVariable->patternType != NULL)
    {
      (*theVariable->patternType->replaceGetJNValueFunction) (theEnv, list,
							      theVariable,
							      LHS);
    }
  else
    {
      return 0;
    }

   /*==============================================================*/
  /* Return 1 to indicate the variable was successfully replaced. */
   /*==============================================================*/

  return 1;
}

/*******************************************************/
/* ParseRuleRHS: Coordinates all the actions necessary */
/*   for parsing the RHS of a rule.                    */
/*******************************************************/
static struct expr *
ParseRuleRHS (Environment * theEnv, const char *readSource)
{
  struct expr *actions;
  struct token theToken;

   /*=========================================================*/
  /* Process the actions on the right hand side of the rule. */
   /*=========================================================*/

  CL_SavePPBuffer (theEnv, "\n   ");
  CL_SetIndentDepth (theEnv, 3);

  actions =
    CL_GroupActions (theEnv, readSource, &theToken, true, NULL, false);

  if (actions == NULL)
    return NULL;

   /*=============================*/
  /* Refo_rmat the closing token. */
   /*=============================*/

  CL_PPBackup (theEnv);
  CL_PPBackup (theEnv);
  CL_SavePPBuffer (theEnv, theToken.printFo_rm);

   /*======================================================*/
  /* Check for the closing right parenthesis of the rule. */
   /*======================================================*/

  if (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "defrule");
      CL_ReturnExpression (theEnv, actions);
      return NULL;
    }

   /*========================*/
  /* Return the rule's RHS. */
   /*========================*/

  return (actions);
}

/************************************************************/
/* RuleComplexity: Returns the complexity of a rule for use */
/*   by the LEX and MEA conflict resolution strategies.     */
/************************************************************/
static unsigned short
RuleComplexity (Environment * theEnv, struct lhsParseNode *theLHS)
{
  struct lhsParseNode *thePattern, *tempPattern;
  unsigned short complexity = 0;

  while (theLHS != NULL)
    {
      complexity += 1;		/* Add 1 for each pattern. */
      complexity += ExpressionComplexity (theEnv, theLHS->networkTest);
      thePattern = theLHS->right;
      while (thePattern != NULL)
	{
	  if (thePattern->multifieldSlot)
	    {
	      tempPattern = thePattern->bottom;
	      while (tempPattern != NULL)
		{
		  complexity +=
		    ExpressionComplexity (theEnv, tempPattern->networkTest);
		  tempPattern = tempPattern->right;
		}
	    }
	  else
	    {
	      complexity +=
		ExpressionComplexity (theEnv, thePattern->networkTest);
	    }
	  thePattern = thePattern->right;
	}
      theLHS = theLHS->bottom;
    }

  return complexity;
}

/********************************************************************/
/* ExpressionComplexity: Dete_rmines the complexity of a expression. */
/********************************************************************/
static unsigned short
ExpressionComplexity (Environment * theEnv, struct expr *exprPtr)
{
  unsigned short complexity = 0;

  while (exprPtr != NULL)
    {
      if (exprPtr->type == FCALL)
	{
	 /*=========================================*/
	  /* Logical combinations do not add to the  */
	  /* complexity, but their arguments do.     */
	 /*=========================================*/

	  if ((exprPtr->value == ExpressionData (theEnv)->PTR_AND) ||
	      (exprPtr->value == ExpressionData (theEnv)->PTR_NOT) ||
	      (exprPtr->value == ExpressionData (theEnv)->PTR_OR))
	    {
	      complexity += ExpressionComplexity (theEnv, exprPtr->argList);
	    }

	 /*=========================================*/
	  /* else other function calls increase the  */
	  /* complexity, but their arguments do not. */
	 /*=========================================*/

	  else
	    {
	      complexity++;
	    }
	}
      else
	if ((CL_EvaluationData (theEnv)->PrimitivesArray[exprPtr->type] !=
	     NULL) ? CL_EvaluationData (theEnv)->PrimitivesArray[exprPtr->
								 type]->
	    addsToRuleComplexity : false)
	{
	  complexity++;
	}

      exprPtr = exprPtr->nextArg;
    }

  return complexity;
}

/********************************************/
/* LogicalAnalysis: Analyzes the use of the */
/*   logical CE within the LHS of a rule.   */
/********************************************/
static int
LogicalAnalysis (Environment * theEnv, struct lhsParseNode *patternList)
{
  bool firstLogical, logicalsFound = false;
  int logicalJoin = 1;
  bool gap = false;

  if (patternList == NULL)
    return 0;

  firstLogical = patternList->logical;

   /*===================================================*/
  /* Loop through each pattern in the LHS of the rule. */
   /*===================================================*/

  for (; patternList != NULL; patternList = patternList->bottom)
    {
      /*=======================================*/
      /* Skip anything that isn't a pattern CE */
      /* or is embedded within a not/and CE.   */
      /*=======================================*/

      if (((patternList->pnType != PATTERN_CE_NODE) &&
	   (patternList->pnType != TEST_CE_NODE)) ||
	  (patternList->endNandDepth != 1))
	{
	  continue;
	}

      /*=====================================================*/
      /* If the pattern CE is not contained within a logical */
      /* CE, then set the gap flag to true indicating that   */
      /* any subsequent pattern CE found within a logical CE */
      /* represents a gap between logical CEs which is an    */
      /* error.                                              */
      /*=====================================================*/

      if (patternList->logical == false)
	{
	  gap = true;
	  continue;
	}

      /*=================================================*/
      /* If a logical CE is encountered and the first CE */
      /* of the rule isn't a logical CE, then indicate   */
      /* that the first CE must be a logical CE.         */
      /*=================================================*/

      if (!firstLogical)
	{
	  CL_PrintErrorID (theEnv, "RULEPSR", 1, true);
	  CL_WriteString (theEnv, STDERR,
			  "Logical CEs must be placed first in a rule\n");
	  return -1;
	}

      /*===================================================*/
      /* If a break within the logical CEs was found and a */
      /* new logical CE is encountered, then indicate that */
      /* there can't be any gaps between logical CEs.      */
      /*===================================================*/

      if (gap)
	{
	  CL_PrintErrorID (theEnv, "RULEPSR", 2, true);
	  CL_WriteString (theEnv, STDERR,
			  "Gaps may not exist between logical CEs\n");
	  return -1;
	}

      /*===========================================*/
      /* Increment the count of logical CEs found. */
      /*===========================================*/

      if (patternList->pnType == PATTERN_CE_NODE)
	{
	  logicalJoin++;
	}
      else if (logicalJoin == 1)
	{
	  logicalJoin++;
	}

      logicalsFound = true;
    }

   /*============================================*/
  /* If logical CEs were found, then return the */
  /* join number where the logical info_rmation  */
  /* will be stored in the join network.        */
   /*============================================*/

  if (logicalsFound)
    {
      return logicalJoin;
    }

   /*=============================*/
  /* Return zero indicating that */
  /* no logical CE was found.    */
   /*=============================*/

  return 0;
}

/*****************************************************************/
/* CL_FindVariable: Searches for the last occurence of a variable   */
/*  in the LHS of a rule that is visible from the RHS of a rule. */
/*  The last occurence of the variable on the LHS side of the    */
/*  rule will have the strictest constraints (because it will    */
/*  have been intersected with all of the other constraints for  */
/*  the variable on the LHS of the rule). The strictest          */
/*  constraints are useful for perfo_rming type checking on the   */
/*  RHS of the rule.                                             */
/*****************************************************************/
struct lhsParseNode *
CL_FindVariable (CLIPSLexeme * name, struct lhsParseNode *theLHS)
{
  struct lhsParseNode *theFields, *tmpFields = NULL;
  struct lhsParseNode *theReturnValue = NULL;

   /*==============================================*/
  /* Loop through each CE in the LHS of the rule. */
   /*==============================================*/

  for (; theLHS != NULL; theLHS = theLHS->bottom)
    {
      /*==========================================*/
      /* Don't bother searching for the variable  */
      /* in anything other than a pattern CE that */
      /* is not contained within a not CE.        */
      /*==========================================*/

      if ((theLHS->pnType != PATTERN_CE_NODE) ||
	  (theLHS->negated == true) ||
	  (theLHS->exists == true) || (theLHS->beginNandDepth > 1))
	{
	  continue;
	}

      /*=====================================*/
      /* Check the pattern address variable. */
      /*=====================================*/

      if (theLHS->value == (void *) name)
	{
	  theReturnValue = theLHS;
	}

      /*============================================*/
      /* Check for the variable inside the pattern. */
      /*============================================*/

      theFields = theLHS->right;
      while (theFields != NULL)
	{
	 /*=================================================*/
	  /* Go one level deeper to check a multifield slot. */
	 /*=================================================*/

	  if (theFields->multifieldSlot)
	    {
	      tmpFields = theFields;
	      theFields = theFields->bottom;
	    }

	 /*=================================*/
	  /* See if the field being examined */
	  /* is the variable being sought.   */
	 /*=================================*/

	  if (theFields == NULL)
	    {			/* Do Nothing */
	    }
	  else if (((theFields->pnType == SF_VARIABLE_NODE) ||
		    (theFields->pnType == MF_VARIABLE_NODE)) &&
		   (theFields->value == (void *) name))
	    {
	      theReturnValue = theFields;
	    }

	 /*============================*/
	  /* Move on to the next field. */
	 /*============================*/

	  if (theFields == NULL)
	    {
	      theFields = tmpFields;
	      tmpFields = NULL;
	    }
	  else if ((theFields->right == NULL) && (tmpFields != NULL))
	    {
	      theFields = tmpFields;
	      tmpFields = NULL;
	    }
	  theFields = theFields->right;
	}
    }

   /*=========================================================*/
  /* Return a pointer to the LHS location where the variable */
  /* was found (or a NULL pointer if it wasn't).             */
   /*=========================================================*/

  return (theReturnValue);
}

/**********************************************************/
/* AddToDefruleList: Adds a defrule to the list of rules. */
/**********************************************************/
static void
AddToDefruleList (Defrule * rulePtr)
{
  Defrule *tempRule;
  struct defruleModule *theModuleItem;

  theModuleItem = (struct defruleModule *) rulePtr->header.whichModule;

  if (theModuleItem->header.lastItem == NULL)
    {
      theModuleItem->header.firstItem = &rulePtr->header;
    }
  else
    {
      tempRule = (Defrule *) theModuleItem->header.lastItem;	// Note: Only the first disjunct
      tempRule->header.next = &rulePtr->header;	// points to the next rule
    }

  theModuleItem->header.lastItem = &rulePtr->header;
}

#if DEVELOPER && DEBUGGING_FUNCTIONS

/************************************************************/
/* DumpRuleAnalysis: Displays the info_rmation about network */
/*   expressions generated from the analysis of the rule.   */
/************************************************************/
void
DumpRuleAnalysis (Environment * theEnv, struct lhsParseNode *tempNode)
{
  struct lhsParseNode *traceNode;
  char buffer[20];

  CL_WriteString (theEnv, STDOUT, "\n");
  for (traceNode = tempNode; traceNode != NULL; traceNode = traceNode->bottom)
    {
      if (traceNode->userCE)
	{
	  CL_gensprintf (buffer, "UCE %2d (%2d %2d): ", traceNode->whichCE,
			 traceNode->beginNandDepth, traceNode->endNandDepth);
	}
      else
	{
	  CL_gensprintf (buffer, "SCE %2d (%2d %2d): ", traceNode->whichCE,
			 traceNode->beginNandDepth, traceNode->endNandDepth);
	}

      CL_WriteString (theEnv, STDOUT, buffer);

      CL_PrintExpression (theEnv, STDOUT, traceNode->networkTest);
      CL_WriteString (theEnv, STDOUT, "\n");

      if (traceNode->externalNetworkTest != NULL)
	{
	  CL_WriteString (theEnv, STDOUT, "      ENT: ");
	  CL_PrintExpression (theEnv, STDOUT, traceNode->externalNetworkTest);
	  CL_WriteString (theEnv, STDOUT, "\n");
	}

      if (traceNode->secondaryNetworkTest != NULL)
	{
	  CL_WriteString (theEnv, STDOUT, "      SNT: ");
	  CL_PrintExpression (theEnv, STDOUT,
			      traceNode->secondaryNetworkTest);
	  CL_WriteString (theEnv, STDOUT, "\n");
	}

      if (traceNode->externalRightHash != NULL)
	{
	  CL_WriteString (theEnv, STDOUT, "      ERH: ");
	  CL_PrintExpression (theEnv, STDOUT, traceNode->externalRightHash);
	  CL_WriteString (theEnv, STDOUT, "\n");
	}

      if (traceNode->externalLeftHash != NULL)
	{
	  CL_WriteString (theEnv, STDOUT, "      ELH: ");
	  CL_PrintExpression (theEnv, STDOUT, traceNode->externalLeftHash);
	  CL_WriteString (theEnv, STDOUT, "\n");
	}

      if (traceNode->leftHash != NULL)
	{
	  CL_WriteString (theEnv, STDOUT, "       LH: ");
	  CL_PrintExpression (theEnv, STDOUT, traceNode->leftHash);
	  CL_WriteString (theEnv, STDOUT, "\n");
	}

      if (traceNode->rightHash != NULL)
	{
	  CL_WriteString (theEnv, STDOUT, "       RH: ");
	  CL_PrintExpression (theEnv, STDOUT, traceNode->rightHash);
	  CL_WriteString (theEnv, STDOUT, "\n");
	}

      if (traceNode->betaHash != NULL)
	{
	  CL_WriteString (theEnv, STDOUT, "       BH: ");
	  CL_PrintExpression (theEnv, STDOUT, traceNode->betaHash);
	  CL_WriteString (theEnv, STDOUT, "\n");
	}
    }
}
#endif

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */

#endif /* DEFRULE_CONSTRUCT */
