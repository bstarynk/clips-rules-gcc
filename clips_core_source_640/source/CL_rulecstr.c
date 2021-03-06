   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*              RULE CONSTRAINTS MODULE                */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides routines for detecting constraint       */
/*   conflicts in the LHS and RHS of rules.                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Static constraint checking is always enabled.  */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if (! RUN_TIME) && (! BLOAD_ONLY) && DEFRULE_CONSTRUCT

#include <stdio.h>

#include "analysis.h"
#include "cstrnchk.h"
#include "cstrnops.h"
#include "cstrnutl.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "prcdrpsr.h"
#include "prntutil.h"
#include "reorder.h"
#include "router.h"
#include "rulepsr.h"

#include "rulecstr.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static bool CheckFor_UnmatchableConstraints (Environment *,
					     struct lhsParseNode *,
					     unsigned short);
static bool MultifieldCardinalityViolation (Environment *,
					    struct lhsParseNode *);
static struct lhsParseNode *UnionVariableConstraints (Environment *,
						      struct lhsParseNode *,
						      struct lhsParseNode *);
static struct lhsParseNode *AddToVariableConstraints (Environment *,
						      struct lhsParseNode *,
						      struct lhsParseNode *);
static void ConstraintConflictMessage (Environment *, CLIPSLexeme *,
				       unsigned short, unsigned short,
				       CLIPSLexeme *);
static bool CheckArgumentForConstraintError (Environment *, struct expr *,
					     struct expr *, unsigned int,
					     struct functionDefinition *,
					     struct lhsParseNode *);

/***********************************************************/
/* CheckFor_UnmatchableConstraints: Dete_rmines if a LHS CE  */
/*   node contains unmatchable constraints. Return true if */
/*   there are unmatchable constraints, otherwise false.   */
/***********************************************************/
static bool
CheckFor_UnmatchableConstraints (Environment * theEnv,
				 struct lhsParseNode *theNode,
				 unsigned short whichCE)
{
  if (CL_UnmatchableConstraint (theNode->constraints))
    {
      ConstraintConflictMessage (theEnv, theNode->lexemeValue, whichCE,
				 theNode->index, theNode->slot);
      return true;
    }

  return false;
}

/******************************************************/
/* ConstraintConflictMessage: Error message used when */
/*   a constraint restriction for a slot prevents any */
/*   value from matching the pattern constraint.      */
/******************************************************/
static void
ConstraintConflictMessage (Environment * theEnv,
			   CLIPSLexeme * variableName,
			   unsigned short thePattern,
			   unsigned short theField, CLIPSLexeme * theSlot)
{
   /*=========================*/
  /* Print the error header. */
   /*=========================*/

  CL_PrintErrorID (theEnv, "RULECSTR", 1, true);

   /*======================================================*/
  /* Print the variable name (if available) and CE number */
  /* for which the constraint violation occurred.         */
   /*======================================================*/

  if (variableName != NULL)
    {
      CL_WriteString (theEnv, STDERR, "Variable ?");
      CL_WriteString (theEnv, STDERR, variableName->contents);
      CL_WriteString (theEnv, STDERR, " in CE #");
      CL_WriteInteger (theEnv, STDERR, thePattern);
    }
  else
    {
      CL_WriteString (theEnv, STDERR, "Pattern #");
      CL_WriteInteger (theEnv, STDERR, thePattern);
    }

   /*=======================================*/
  /* Print the slot name or field position */
  /* in which the violation occurred.      */
   /*=======================================*/

  if (theSlot == NULL)
    {
      CL_WriteString (theEnv, STDERR, " field #");
      CL_WriteInteger (theEnv, STDERR, theField);
    }
  else
    {
      CL_WriteString (theEnv, STDERR, " slot '");
      CL_WriteString (theEnv, STDERR, theSlot->contents);
      CL_WriteString (theEnv, STDERR, "'");
    }

   /*======================================*/
  /* Print the rest of the error message. */
   /*======================================*/

  CL_WriteString (theEnv, STDERR,
		  " has constraint conflicts which make the pattern unmatchable.\n");
}

/***************************************************************/
/* MultifieldCardinalityViolation: Dete_rmines if a cardinality */
/*   violation has occurred for a LHS CE node.                 */
/***************************************************************/
static bool
MultifieldCardinalityViolation (Environment * theEnv,
				struct lhsParseNode *theNode)
{
  struct lhsParseNode *tmpNode;
  struct expr *tmpMax;
  long long minFields = 0;
  long long maxFields = 0;
  bool posInfinity = false;
  CONSTRAINT_RECORD *newConstraint, *tempConstraint;

   /*================================*/
  /* A single field slot can't have */
  /* a cardinality violation.       */
   /*================================*/

  if (theNode->multifieldSlot == false)
    return false;

   /*=============================================*/
  /* Dete_rmine the minimum and maximum number of */
  /* fields the slot could contain based on the  */
  /* slot constraints found in the pattern.      */
   /*=============================================*/

  for (tmpNode = theNode->bottom; tmpNode != NULL; tmpNode = tmpNode->right)
    {
      /*====================================================*/
      /* A single field variable increases both the minimum */
      /* and maximum number of fields by one.               */
      /*====================================================*/

      if ((tmpNode->pnType == SF_VARIABLE_NODE) ||
	  (tmpNode->pnType == SF_WILDCARD_NODE))
	{
	  minFields++;
	  maxFields++;
	}

      /*=================================================*/
      /* Otherwise a multifield wildcard or variable has */
      /* been encountered. If it is constrained then use */
      /* minimum and maximum number of fields constraint */
      /* associated with this LHS node.                  */
      /*=================================================*/

      else if (tmpNode->constraints != NULL)
	{
	 /*=======================================*/
	  /* The lowest minimum of all the min/max */
	  /* pairs will be the first in the list.  */
	 /*=======================================*/

	  if (tmpNode->constraints->minFields->value !=
	      SymbolData (theEnv)->NegativeInfinity)
	    {
	      minFields +=
		tmpNode->constraints->minFields->integerValue->contents;
	    }

	 /*=========================================*/
	  /* The greatest maximum of all the min/max */
	  /* pairs will be the last in the list.     */
	 /*=========================================*/

	  tmpMax = tmpNode->constraints->maxFields;
	  while (tmpMax->nextArg != NULL)
	    tmpMax = tmpMax->nextArg;
	  if (tmpMax->value == SymbolData (theEnv)->PositiveInfinity)
	    {
	      posInfinity = true;
	    }
	  else
	    {
	      maxFields += tmpMax->integerValue->contents;
	    }
	}

      /*================================================*/
      /* Otherwise an unconstrained multifield wildcard */
      /* or variable increases the maximum number of    */
      /* fields to positive infinity.                   */
      /*================================================*/

      else
	{
	  posInfinity = true;
	}
    }

   /*==================================================================*/
  /* Create a constraint record for the cardinality of the sum of the */
  /* cardinalities of the restrictions inside the multifield slot.    */
   /*==================================================================*/

  if (theNode->constraints == NULL)
    tempConstraint = CL_GetConstraintRecord (theEnv);
  else
    tempConstraint = CL_CopyConstraintRecord (theEnv, theNode->constraints);
  CL_ReturnExpression (theEnv, tempConstraint->minFields);
  CL_ReturnExpression (theEnv, tempConstraint->maxFields);
  tempConstraint->minFields =
    CL_GenConstant (theEnv, CL_INTEGER_TYPE,
		    CL_CreateInteger (theEnv, minFields));
  if (posInfinity)
    tempConstraint->maxFields =
      CL_GenConstant (theEnv, SYMBOL_TYPE,
		      SymbolData (theEnv)->PositiveInfinity);
  else
    tempConstraint->maxFields =
      CL_GenConstant (theEnv, CL_INTEGER_TYPE,
		      CL_CreateInteger (theEnv, maxFields));

   /*================================================================*/
  /* Dete_rmine the final cardinality for the multifield slot by     */
  /* intersecting the cardinality sum of the restrictions within    */
  /* the multifield slot with the original cardinality of the slot. */
   /*================================================================*/

  newConstraint =
    CL_IntersectConstraints (theEnv, theNode->constraints, tempConstraint);
  if (theNode->derivedConstraints)
    CL_RemoveConstraint (theEnv, theNode->constraints);
  CL_RemoveConstraint (theEnv, tempConstraint);
  theNode->constraints = newConstraint;
  theNode->derivedConstraints = true;

   /*===================================================================*/
  /* Dete_rmine if the final cardinality for the slot can be satisfied. */
   /*===================================================================*/

  if (CL_UnmatchableConstraint (newConstraint))
    return true;

  return false;
}

/***************************************************/
/* CL_ProcessConnectedConstraints: Examines a single  */
/*   connected constraint searching for constraint */
/*   violations.                                   */
/***************************************************/
bool
CL_ProcessConnectedConstraints (Environment * theEnv,
				struct lhsParseNode *theNode,
				struct lhsParseNode *multifieldHeader,
				struct lhsParseNode *patternHead)
{
  struct constraintRecord *orConstraints = NULL, *andConstraints;
  struct constraintRecord *tmpConstraints, *rvConstraints;
  struct lhsParseNode *orNode, *andNode;
  struct expr *tmpExpr;

   /*============================================*/
  /* Loop through all of the or (|) constraints */
  /* found in the connected constraint.         */
   /*============================================*/

  for (orNode = theNode->bottom; orNode != NULL; orNode = orNode->bottom)
    {
      /*=================================================*/
      /* Intersect all of the &'ed constraints together. */
      /*=================================================*/

      andConstraints = NULL;
      for (andNode = orNode; andNode != NULL; andNode = andNode->right)
	{
	  if (!andNode->negated)
	    {
	      if (andNode->pnType == RETURN_VALUE_CONSTRAINT_NODE)
		{
		  if (andNode->expression->pnType == FCALL_NODE)
		    {
		      rvConstraints =
			CL_FunctionCallToConstraintRecord (theEnv,
							   andNode->
							   expression->value);
		      tmpConstraints = andConstraints;
		      andConstraints =
			CL_IntersectConstraints (theEnv, andConstraints,
						 rvConstraints);
		      CL_RemoveConstraint (theEnv, tmpConstraints);
		      CL_RemoveConstraint (theEnv, rvConstraints);
		    }
		}
	      else if (CL_ConstantNode (andNode))
		{
		  tmpExpr =
		    CL_GenConstant (theEnv, CL_NodeTypeToType (andNode),
				    andNode->value);
		  rvConstraints =
		    CL_ExpressionToConstraintRecord (theEnv, tmpExpr);
		  tmpConstraints = andConstraints;
		  andConstraints =
		    CL_IntersectConstraints (theEnv, andConstraints,
					     rvConstraints);
		  CL_RemoveConstraint (theEnv, tmpConstraints);
		  CL_RemoveConstraint (theEnv, rvConstraints);
		  CL_ReturnExpression (theEnv, tmpExpr);
		}
	      else if (andNode->constraints != NULL)
		{
		  tmpConstraints = andConstraints;
		  andConstraints =
		    CL_IntersectConstraints (theEnv, andConstraints,
					     andNode->constraints);
		  CL_RemoveConstraint (theEnv, tmpConstraints);
		}
	    }
	}

      /*===========================================================*/
      /* Intersect the &'ed constraints with the slot constraints. */
      /*===========================================================*/

      tmpConstraints = andConstraints;
      andConstraints =
	CL_IntersectConstraints (theEnv, andConstraints,
				 theNode->constraints);
      CL_RemoveConstraint (theEnv, tmpConstraints);

      /*===============================================================*/
      /* Remove any negated constants from the list of allowed values. */
      /*===============================================================*/

      for (andNode = orNode; andNode != NULL; andNode = andNode->right)
	{
	  if ((andNode->negated) && CL_ConstantNode (andNode))
	    {
	      CL_RemoveConstantFromConstraint (theEnv,
					       CL_NodeTypeToType (andNode),
					       andNode->value,
					       andConstraints);
	    }
	}

      /*=======================================================*/
      /* Union the &'ed constraints with the |'ed constraints. */
      /*=======================================================*/

      tmpConstraints = orConstraints;
      orConstraints =
	CL_UnionConstraints (theEnv, orConstraints, andConstraints);
      CL_RemoveConstraint (theEnv, tmpConstraints);
      CL_RemoveConstraint (theEnv, andConstraints);
    }

   /*===============================================*/
  /* Replace the constraints for the slot with the */
  /* constraints derived from the connected        */
  /* constraints (which should be a subset.        */
   /*===============================================*/

  if (orConstraints != NULL)
    {
      if (theNode->derivedConstraints)
	CL_RemoveConstraint (theEnv, theNode->constraints);
      theNode->constraints = orConstraints;
      theNode->derivedConstraints = true;
    }

   /*==================================*/
  /* Check for constraint violations. */
   /*==================================*/

  if (CheckFor_UnmatchableConstraints (theEnv, theNode, patternHead->whichCE))
    {
      return true;
    }

   /*=========================================*/
  /* If the constraints are for a multifield */
  /* slot, check for cardinality violations. */
   /*=========================================*/

  if ((multifieldHeader != NULL) && (theNode->right == NULL))
    {
      if (MultifieldCardinalityViolation (theEnv, multifieldHeader))
	{
	  CL_ConstraintViolationErrorMessage (theEnv,
					      "The group of restrictions",
					      NULL, false,
					      patternHead->whichCE,
					      multifieldHeader->slot,
					      multifieldHeader->index,
					      CARDINALITY_VIOLATION,
					      multifieldHeader->constraints,
					      true);
	  return true;
	}
    }

   /*=======================================*/
  /* Return false indicating no constraint */
  /* violations were detected.             */
   /*=======================================*/

  return false;
}

/**************************************************/
/* CL_ConstraintReferenceErrorMessage: Generic error */
/*   message for LHS constraint violation errors  */
/*   that occur within an expression.             */
/**************************************************/
void
CL_ConstraintReferenceErrorMessage (Environment * theEnv,
				    CLIPSLexeme * theVariable,
				    struct lhsParseNode *theExpression,
				    int whichArgument,
				    int whichCE,
				    CLIPSLexeme * slotName, int theField)
{
  struct expr *temprv;

  CL_PrintErrorID (theEnv, "RULECSTR", 2, true);

   /*==========================*/
  /* Print the variable name. */
   /*==========================*/

  CL_WriteString (theEnv, STDERR, "Previous variable bindings of ?");
  CL_WriteString (theEnv, STDERR, theVariable->contents);
  CL_WriteString (theEnv, STDERR, " caused the type restrictions");

   /*============================*/
  /* Print the argument number. */
   /*============================*/

  CL_WriteString (theEnv, STDERR, "\nfor argument #");
  CL_WriteInteger (theEnv, STDERR, whichArgument);

   /*=======================*/
  /* Print the expression. */
   /*=======================*/

  CL_WriteString (theEnv, STDERR, " of the expression ");
  temprv = CL_LHSParseNodesToExpression (theEnv, theExpression);
  CL_ReturnExpression (theEnv, temprv->nextArg);
  temprv->nextArg = NULL;
  CL_PrintExpression (theEnv, STDERR, temprv);
  CL_WriteString (theEnv, STDERR, "\n");
  CL_ReturnExpression (theEnv, temprv);

   /*========================================*/
  /* Print out the index of the conditional */
  /* element and the slot name or field     */
  /* index where the violation occured.     */
   /*========================================*/

  CL_WriteString (theEnv, STDERR, "found in CE #");
  CL_WriteInteger (theEnv, STDERR, theExpression->whichCE);
  if (slotName == NULL)
    {
      if (theField > 0)
	{
	  CL_WriteString (theEnv, STDERR, " field #");
	  CL_WriteInteger (theEnv, STDERR, theField);
	}
    }
  else
    {
      CL_WriteString (theEnv, STDERR, " slot '");
      CL_WriteString (theEnv, STDERR, slotName->contents);
      CL_WriteString (theEnv, STDERR, "'");
    }

  CL_WriteString (theEnv, STDERR, " to be violated.\n");
}

/********************************************************/
/* AddToVariableConstraints: Adds the constraints for a */
/*   variable to a list of constraints. If the variable */
/*   is already in the list, the constraints for the    */
/*   variable are intersected with the new constraints. */
/********************************************************/
static struct lhsParseNode *
AddToVariableConstraints (Environment * theEnv,
			  struct lhsParseNode *oldList,
			  struct lhsParseNode *newItems)
{
  CONSTRAINT_RECORD *newConstraints;
  struct lhsParseNode *temp, *trace;

   /*=================================================*/
  /* Loop through each of the new constraints adding */
  /* it to the list if it's not already present or   */
  /* modifying the constraint if it is.              */
   /*=================================================*/

  while (newItems != NULL)
    {
      /*==========================================*/
      /* Get the next item since the next pointer */
      /* value (right) needs to be set to NULL.   */
      /*==========================================*/

      temp = newItems->right;
      newItems->right = NULL;

      /*===================================*/
      /* Search the list for the variable. */
      /*===================================*/

      for (trace = oldList; trace != NULL; trace = trace->right)
	{
	 /*=========================================*/
	  /* If the variable is already in the list, */
	  /* modify the constraint already there to  */
	  /* include the new constraint.             */
	 /*=========================================*/

	  if (trace->value == newItems->value)
	    {
	      newConstraints =
		CL_IntersectConstraints (theEnv, trace->constraints,
					 newItems->constraints);
	      CL_RemoveConstraint (theEnv, trace->constraints);
	      trace->constraints = newConstraints;
	      CL_ReturnLHSParseNodes (theEnv, newItems);
	      break;
	    }
	}

      /*=================================*/
      /* Add the variable constraints to */
      /* the list if it wasn't found.    */
      /*=================================*/

      if (trace == NULL)
	{
	  newItems->right = oldList;
	  oldList = newItems;
	}

      /*===========================*/
      /* Move on to the next item. */
      /*===========================*/

      newItems = temp;
    }

  return (oldList);
}

/***********************************************************/
/* UnionVariableConstraints: Unions two lists of variable  */
/*   constraints. If a variable appears in one list  but   */
/*   not the other, then the variable is unconstrained and */
/*   thus not included in the unioned list.                */
/***********************************************************/
static struct lhsParseNode *
UnionVariableConstraints (Environment * theEnv,
			  struct lhsParseNode *list1,
			  struct lhsParseNode *list2)
{
  struct lhsParseNode *list3 = NULL, *trace, *temp;

   /*===================================*/
  /* Loop through all of the variables */
  /* in the first list.                */
   /*===================================*/

  while (list1 != NULL)
    {
      /*=============================================*/
      /* Search for the variable in the second list. */
      /*=============================================*/

      for (trace = list2; trace != NULL; trace = trace->right)
	{
	 /*============================================*/
	  /* If the variable is found in both lists,    */
	  /* union the constraints and add the variable */
	  /* to the new list being constructed.         */
	 /*============================================*/

	  if (list1->value == trace->value)
	    {
	      temp = CL_GetLHSParseNode (theEnv);
	      temp->derivedConstraints = true;
	      temp->value = list1->value;
	      temp->constraints =
		CL_UnionConstraints (theEnv, list1->constraints,
				     trace->constraints);
	      temp->right = list3;
	      list3 = temp;
	      break;
	    }
	}

      /*==============================*/
      /* Move on to the next variable */
      /* in the first list.           */
      /*==============================*/

      temp = list1->right;
      list1->right = NULL;
      CL_ReturnLHSParseNodes (theEnv, list1);
      list1 = temp;
    }

   /*====================================*/
  /* Free the items in the second list. */
   /*====================================*/

  CL_ReturnLHSParseNodes (theEnv, list2);

   /*======================*/
  /* Return the new list. */
   /*======================*/

  return (list3);
}

/*****************************************************************/
/* CL_GetExpressionVarConstraints: Given an expression stored using */
/*   the LHS parse node data structures, dete_rmines and returns  */
/*   the constraints on variables caused by that expression. For */
/*   example, the expression (+ ?x 1) would imply a numeric type */
/*   constraint for the variable ?x since the addition function  */
/*   expects numeric arguments.                                  */
/*****************************************************************/
struct lhsParseNode *
CL_GetExpressionVarConstraints (Environment * theEnv,
				struct lhsParseNode *theExpression)
{
  struct lhsParseNode *list1 = NULL, *list2;

  for (; theExpression != NULL; theExpression = theExpression->bottom)
    {
      if (theExpression->right != NULL)
	{
	  list2 =
	    CL_GetExpressionVarConstraints (theEnv, theExpression->right);
	  list1 = AddToVariableConstraints (theEnv, list2, list1);
	}

      if (theExpression->pnType == SF_VARIABLE_NODE)
	{
	  list2 = CL_GetLHSParseNode (theEnv);
	  if (theExpression->referringNode != NULL)
	    {
	      list2->pnType = theExpression->referringNode->pnType;
	    }
	  else
	    {
	      list2->pnType = SF_VARIABLE_NODE;
	    }
	  list2->value = theExpression->value;
	  list2->derivedConstraints = true;
	  list2->constraints =
	    CL_CopyConstraintRecord (theEnv, theExpression->constraints);
	  list1 = AddToVariableConstraints (theEnv, list2, list1);
	}
    }

  return (list1);
}

/***********************************************/
/* CL_DeriveVariableConstraints: Derives the list */
/*   of variable constraints associated with a */
/*   single connected constraint.              */
/***********************************************/
struct lhsParseNode *
CL_DeriveVariableConstraints (Environment * theEnv,
			      struct lhsParseNode *theNode)
{
  struct lhsParseNode *orNode, *andNode;
  struct lhsParseNode *list1, *list2, *list3 = NULL;
  bool first = true;

   /*===============================*/
  /* Process the constraints for a */
  /* single connected constraint.  */
   /*===============================*/

  for (orNode = theNode->bottom; orNode != NULL; orNode = orNode->bottom)
    {
      /*=================================================*/
      /* Intersect all of the &'ed constraints together. */
      /*=================================================*/

      list2 = NULL;
      for (andNode = orNode; andNode != NULL; andNode = andNode->right)
	{
	  if ((andNode->pnType == RETURN_VALUE_CONSTRAINT_NODE) ||
	      (andNode->pnType == PREDICATE_CONSTRAINT_NODE))
	    {
	      list1 =
		CL_GetExpressionVarConstraints (theEnv, andNode->expression);
	      list2 = AddToVariableConstraints (theEnv, list2, list1);
	    }
	}

      if (first)
	{
	  list3 = list2;
	  first = false;
	}
      else
	{
	  list3 = UnionVariableConstraints (theEnv, list3, list2);
	}
    }

  return (list3);
}

/*******************************************/
/* CL_CheckRHSForConstraintErrors: Checks the */
/*   RHS of a rule for constraint errors.  */
/*******************************************/
bool
CL_CheckRHSForConstraintErrors (Environment * theEnv,
				struct expr *expressionList,
				struct lhsParseNode *theLHS)
{
  struct functionDefinition *theFunction;
  unsigned int i;
  struct expr *lastOne = NULL, *checkList, *tmpPtr;

  if (expressionList == NULL)
    return false;

  for (checkList = expressionList;
       checkList != NULL; checkList = checkList->nextArg)
    {
      expressionList = checkList->argList;
      i = 1;
      if (checkList->type == FCALL)
	{
	  lastOne = checkList;
	  theFunction = checkList->functionValue;
	}
      else
	{
	  theFunction = NULL;
	}

      while (expressionList != NULL)
	{
	  if (CheckArgumentForConstraintError
	      (theEnv, expressionList, lastOne, i, theFunction, theLHS))
	    {
	      return true;
	    }

	  i++;
	  tmpPtr = expressionList->nextArg;
	  expressionList->nextArg = NULL;
	  if (CL_CheckRHSForConstraintErrors (theEnv, expressionList, theLHS))
	    {
	      expressionList->nextArg = tmpPtr;
	      return true;
	    }
	  expressionList->nextArg = tmpPtr;
	  expressionList = expressionList->nextArg;
	}
    }

  return false;
}

/*************************************************************/
/* CheckArgumentForConstraintError: Checks a single argument */
/*   found in the RHS of a rule for constraint errors.       */
/*   Returns true if an error is detected, otherwise false.  */
/*************************************************************/
static bool
CheckArgumentForConstraintError (Environment * theEnv,
				 struct expr *expressionList,
				 struct expr *lastOne,
				 unsigned int i,
				 struct functionDefinition *theFunction,
				 struct lhsParseNode *theLHS)
{
  unsigned theRestriction2;
  CONSTRAINT_RECORD *constraint1, *constraint2, *constraint3, *constraint4;
  struct lhsParseNode *theVariable;
  struct expr *tmpPtr;
  bool rv = false;

   /*=============================================================*/
  /* Skip anything that isn't a variable or isn't an argument to */
  /* a user defined function (i.e. deffunctions and generic have */
  /* no constraint info_rmation so they aren't checked).          */
   /*=============================================================*/

  if ((expressionList->type != SF_VARIABLE) || (theFunction == NULL))
    {
      return (rv);
    }

   /*===========================================*/
  /* Get the restrictions for the argument and */
  /* convert them to a constraint record.      */
   /*===========================================*/

  theRestriction2 = CL_GetNthRestriction (theEnv, theFunction, i);
  constraint1 = CL_ArgumentTypeToConstraintRecord (theEnv, theRestriction2);

   /*================================================*/
  /* Look for the constraint record associated with */
  /* binding the variable in the LHS of the rule.   */
   /*================================================*/

  theVariable = CL_FindVariable (expressionList->lexemeValue, theLHS);
  if (theVariable != NULL)
    {
      if (theVariable->pnType == MF_VARIABLE_NODE)
	{
	  constraint2 = CL_GetConstraintRecord (theEnv);
	  CL_SetConstraintType (MULTIFIELD_TYPE, constraint2);
	}
      else if (theVariable->constraints == NULL)
	{
	  constraint2 = CL_GetConstraintRecord (theEnv);
	}
      else
	{
	  constraint2 =
	    CL_CopyConstraintRecord (theEnv, theVariable->constraints);
	}
    }
  else
    {
      constraint2 = NULL;
    }

   /*================================================*/
  /* Look for the constraint record associated with */
  /* binding the variable on the RHS of the rule.   */
   /*================================================*/

  constraint3 = CL_FindBindConstraints (theEnv, expressionList->lexemeValue);

   /*====================================================*/
  /* Union the LHS and RHS variable binding constraints */
  /* (the variable must satisfy one or the other).      */
   /*====================================================*/

  constraint3 = CL_UnionConstraints (theEnv, constraint3, constraint2);

   /*====================================================*/
  /* Intersect the LHS/RHS variable binding constraints */
  /* with the function argument restriction constraints */
  /* (the variable must satisfy both).                  */
   /*====================================================*/

  constraint4 = CL_IntersectConstraints (theEnv, constraint3, constraint1);

   /*====================================*/
  /* Check for unmatchable constraints. */
   /*====================================*/

  if (CL_UnmatchableConstraint (constraint4))
    {
      CL_PrintErrorID (theEnv, "RULECSTR", 3, true);
      CL_WriteString (theEnv, STDERR, "Previous variable bindings of ?");
      CL_WriteString (theEnv, STDERR, expressionList->lexemeValue->contents);
      CL_WriteString (theEnv, STDERR, " caused the type restrictions");
      CL_WriteString (theEnv, STDERR, "\nfor argument #");
      CL_WriteInteger (theEnv, STDERR, i);
      CL_WriteString (theEnv, STDERR, " of the expression ");
      tmpPtr = lastOne->nextArg;
      lastOne->nextArg = NULL;
      CL_PrintExpression (theEnv, STDERR, lastOne);
      lastOne->nextArg = tmpPtr;
      CL_WriteString (theEnv, STDERR,
		      " found in the rule's RHS to be violated.\n");

      rv = true;
    }

   /*===========================================*/
  /* Free the temporarily created constraints. */
   /*===========================================*/

  CL_RemoveConstraint (theEnv, constraint1);
  CL_RemoveConstraint (theEnv, constraint2);
  CL_RemoveConstraint (theEnv, constraint3);
  CL_RemoveConstraint (theEnv, constraint4);

   /*========================================*/
  /* Return true if unmatchable constraints */
  /* were detected, otherwise false.        */
   /*========================================*/

  return (rv);
}

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) && DEFRULE_CONSTRUCT */
