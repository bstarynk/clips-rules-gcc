   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*             DEFRULE LHS PARSING MODULE              */
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
/*      6.40: Added Env prefix to Get_EvaluationError and     */
/*            Set_EvaluationError functions.                  */
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

#include "setup.h"

#if (! RUN_TIME) && (! BLOAD_ONLY) &&  DEFRULE_CONSTRUCT

#include <stdio.h>
#include <string.h>

#include "agenda.h"
#include "argacces.h"
#include "constant.h"
#include "constrct.h"
#include "constrnt.h"
#include "cstrnchk.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "pattern.h"
#include "pprint.h"
#include "prntutil.h"
#include "reorder.h"
#include "router.h"
#include "ruledef.h"
#include "scanner.h"
#include "symbol.h"

#include "rulelhs.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static struct lhsParseNode *RuleBodyParse (Environment *, const char *,
					   struct token *, const char *,
					   bool *);
static void DeclarationParse (Environment *, const char *, const char *,
			      bool *);
static struct lhsParseNode *LHSPattern (Environment *, const char *,
					TokenType, const char *, bool *, bool,
					struct token *, const char *);
static struct lhsParseNode *ConnectedPatternParse (Environment *,
						   const char *,
						   struct token *, bool *);
static struct lhsParseNode *GroupPatterns (Environment *, const char *,
					   TokenType, const char *, bool *);
static struct lhsParseNode *TestPattern (Environment *, const char *, bool *);
static struct lhsParseNode *AssignmentParse (Environment *, const char *,
					     CLIPSLexeme *, bool *);
static void TagLHSLogicalNodes (struct lhsParseNode *);
static struct lhsParseNode *SimplePatternParse (Environment *, const char *,
						struct token *, bool *);
static void ParseSalience (Environment *, const char *, const char *, bool *);
static void ParseAuto_Focus (Environment *, const char *, bool *);

/*******************************************************************/
/* CL_ParseRuleLHS: Coordinates all the actions necessary for parsing */
/*   the LHS of a rule including the reordering of pattern         */
/*   conditional elements to confo_rm with the KB Rete topology.    */
/*******************************************************************/
struct lhsParseNode *
CL_ParseRuleLHS (Environment * theEnv,
		 const char *readSource,
		 struct token *theToken, const char *ruleName, bool *error)
{
  struct lhsParseNode *theLHS;
  bool result;

  *error = false;

   /*========================================*/
  /* Initialize salience parsing variables. */
   /*========================================*/

  PatternData (theEnv)->GlobalSalience = 0;
  PatternData (theEnv)->GlobalAuto_Focus = false;
  PatternData (theEnv)->SalienceExpression = NULL;

   /*============================*/
  /* Set the indentation depth. */
   /*============================*/

  CL_SetIndentDepth (theEnv, 3);

   /*=====================================================*/
  /* Get the raw representation for the LHS of the rule. */
   /*=====================================================*/

  theLHS = RuleBodyParse (theEnv, readSource, theToken, ruleName, error);

  if (*error)
    return NULL;

   /*====================================================*/
  /* Reorder the raw representation so that it consists */
  /* of at most a single top level OR CE containing one */
  /* or more AND CEs.                                   */
   /*====================================================*/

  theLHS = CL_ReorderPatterns (theEnv, theLHS, &result);

   /*================================*/
  /* Return the LHS representation. */
   /*================================*/

  return (theLHS);
}

/*********************************************************/
/* RuleBodyParse: Parses the LHS of a rule, but does not */
/*   reorder any of the LHS patterns to confo_rm with the */
/*   KB Rete Topology.                                   */
/*                                                       */
/* <rule-body> ::= [<declaration>]                       */
/*                 <conditional-element>*                */
/*                 =>                                    */
/*********************************************************/
static struct lhsParseNode *
RuleBodyParse (Environment * theEnv,
	       const char *readSource,
	       struct token *theToken, const char *ruleName, bool *error)
{
  struct lhsParseNode *theNode, *otherNodes;

   /*=============================*/
  /* Set the error return value. */
   /*=============================*/

  *error = false;

   /*==================================================*/
  /* If we're already at the separator, "=>", between */
  /* the LHS and RHS, then the LHS is empty.          */
   /*==================================================*/

  if ((theToken->tknType == SYMBOL_TOKEN) ?
      (strcmp (theToken->lexemeValue->contents, "=>") == 0) : false)
    {
      return NULL;
    }

   /*===========================================*/
  /* Parse the first pattern as a special case */
  /* (the declare statement is allowed).       */
   /*===========================================*/

  theNode =
    LHSPattern (theEnv, readSource, SYMBOL_TOKEN, "=>", error, true, theToken,
		ruleName);

  if (*error == true)
    {
      CL_ReturnLHSParseNodes (theEnv, theNode);
      return NULL;
    }

  CL_PPCRAndIndent (theEnv);

   /*======================================*/
  /* Parse the other patterns in the LHS. */
   /*======================================*/

  otherNodes = GroupPatterns (theEnv, readSource, SYMBOL_TOKEN, "=>", error);

  if (*error == true)
    {
      CL_ReturnLHSParseNodes (theEnv, theNode);
      return NULL;
    }

   /*================================================*/
  /* Construct the final LHS by combining the first */
  /* pattern with the re_maining patterns.           */
   /*================================================*/

  if (theNode == NULL)
    {
      theNode = otherNodes;
    }
  else
    {
      theNode->bottom = otherNodes;
    }

   /*=======================*/
  /* Return the final LHS. */
   /*=======================*/

  return (theNode);
}

/********************************************************/
/* DeclarationParse: Parses a defrule declaration.      */
/*                                                      */
/* <declaration> ::= (declare <rule-property>+)         */
/*                                                      */
/* <rule-property> ::= (salience <integer-expression>)  */
/* <rule-property> ::= (auto-focus TRUE | FALSE)        */
/********************************************************/
static void
DeclarationParse (Environment * theEnv,
		  const char *readSource, const char *ruleName, bool *error)
{
  struct token theToken;
  struct expr *packPtr;
  bool notDone = true;
  bool salienceParsed = false, auto_FocusParsed = false;

   /*===========================*/
  /* Next token must be a '('. */
   /*===========================*/

  CL_SavePPBuffer (theEnv, " ");

  CL_GetToken (theEnv, readSource, &theToken);
  if (theToken.tknType != LEFT_PARENTHESIS_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "declare statement");
      *error = true;
      return;
    }

   /*==========================================*/
  /* Continue parsing until there are no more */
  /* valid rule property declarations.        */
   /*==========================================*/

  while (notDone)
    {
      /*=============================================*/
      /* The name of a rule property must be symbol. */
      /*=============================================*/

      CL_GetToken (theEnv, readSource, &theToken);
      if (theToken.tknType != SYMBOL_TOKEN)
	{
	  CL_SyntaxErrorMessage (theEnv, "declare statement");
	  *error = true;
	}

      /*==============================================*/
      /* Parse a salience declaration if encountered. */
      /*==============================================*/

      else if (strcmp (theToken.lexemeValue->contents, "salience") == 0)
	{
	  if (salienceParsed)
	    {
	      CL_AlreadyParsedErrorMessage (theEnv, "salience declaration",
					    NULL);
	      *error = true;
	    }
	  else
	    {
	      ParseSalience (theEnv, readSource, ruleName, error);
	      salienceParsed = true;
	    }
	}

      /*=================================================*/
      /* Parse an auto-focus declaration if encountered. */
      /* A global flag is used to indicate if the        */
      /* auto-focus feature for a rule was parsed.       */
      /*=================================================*/

      else if (strcmp (theToken.lexemeValue->contents, "auto-focus") == 0)
	{
	  if (auto_FocusParsed)
	    {
	      CL_AlreadyParsedErrorMessage (theEnv, "auto-focus declaration",
					    NULL);
	      *error = true;
	    }
	  else
	    {
	      ParseAuto_Focus (theEnv, readSource, error);
	      auto_FocusParsed = true;
	    }
	}

      /*==========================================*/
      /* Otherwise the symbol does not correspond */
      /* to a valid rule property.                */
      /*==========================================*/

      else
	{
	  CL_SyntaxErrorMessage (theEnv, "declare statement");
	  *error = true;
	}

      /*=====================================*/
      /* Return if an error was encountered. */
      /*=====================================*/

      if (*error)
	{
	  CL_ReturnExpression (theEnv,
			       PatternData (theEnv)->SalienceExpression);
	  PatternData (theEnv)->SalienceExpression = NULL;
	  return;
	}

      /*=======================================*/
      /* Both the salience and auto-focus rule */
      /* properties are closed with a ')'.     */
      /*=======================================*/

      CL_GetToken (theEnv, readSource, &theToken);
      if (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
	{
	  CL_PPBackup (theEnv);
	  CL_SavePPBuffer (theEnv, " ");
	  CL_SavePPBuffer (theEnv, theToken.printFo_rm);
	  CL_ReturnExpression (theEnv,
			       PatternData (theEnv)->SalienceExpression);
	  PatternData (theEnv)->SalienceExpression = NULL;
	  CL_SyntaxErrorMessage (theEnv, "declare statement");
	  *error = true;
	  return;
	}

      /*=============================================*/
      /* The declare statement is closed with a ')'. */
      /*=============================================*/

      CL_GetToken (theEnv, readSource, &theToken);
      if (theToken.tknType == RIGHT_PARENTHESIS_TOKEN)
	notDone = false;
      else if (theToken.tknType != LEFT_PARENTHESIS_TOKEN)
	{
	  CL_ReturnExpression (theEnv,
			       PatternData (theEnv)->SalienceExpression);
	  PatternData (theEnv)->SalienceExpression = NULL;
	  CL_SyntaxErrorMessage (theEnv, "declare statement");
	  *error = true;
	  return;
	}
      else
	{
	  CL_PPBackup (theEnv);
	  CL_SavePPBuffer (theEnv, " (");
	}
    }

   /*==========================================*/
  /* Return the value of the salience through */
  /* the global variable SalienceExpression.  */
   /*==========================================*/

  packPtr =
    CL_PackExpression (theEnv, PatternData (theEnv)->SalienceExpression);
  CL_ReturnExpression (theEnv, PatternData (theEnv)->SalienceExpression);
  PatternData (theEnv)->SalienceExpression = packPtr;
  return;
}

/************************************************************/
/* ParseSalience: Parses the rest of a defrule salience     */
/*   declaration once the salience keyword has been parsed. */
/************************************************************/
static void
ParseSalience (Environment * theEnv,
	       const char *readSource, const char *ruleName, bool *error)
{
  int salience;
  UDFValue salienceValue;

   /*==============================*/
  /* Get the salience expression. */
   /*==============================*/

  CL_SavePPBuffer (theEnv, " ");

  PatternData (theEnv)->SalienceExpression =
    CL_ParseAtomOrExpression (theEnv, readSource, NULL);
  if (PatternData (theEnv)->SalienceExpression == NULL)
    {
      *error = true;
      return;
    }

   /*============================================================*/
  /* CL_Evaluate the expression and dete_rmine if it is an integer. */
   /*============================================================*/

  Set_EvaluationError (theEnv, false);
  if (CL_EvaluateExpression
      (theEnv, PatternData (theEnv)->SalienceExpression, &salienceValue))
    {
      SalienceInfo_rmationError (theEnv, "defrule", ruleName);
      *error = true;
      return;
    }

  if (salienceValue.header->type != CL_INTEGER_TYPE)
    {
      CL_SalienceNonIntegerError (theEnv);
      *error = true;
      return;
    }

   /*=======================================================*/
  /* Salience number must be in the range -10000 to 10000. */
   /*=======================================================*/

  salience = (int) salienceValue.integerValue->contents;

  if ((salience > MAX_DEFRULE_SALIENCE) || (salience < MIN_DEFRULE_SALIENCE))
    {
      CL_SalienceRangeError (theEnv, MIN_DEFRULE_SALIENCE,
			     MAX_DEFRULE_SALIENCE);
      *error = true;
      return;
    }

   /*==========================================*/
  /* If the expression is a constant integer, */
  /* don't bother storing the expression.     */
   /*==========================================*/

  if (PatternData (theEnv)->SalienceExpression->type == CL_INTEGER_TYPE)
    {
      CL_ReturnExpression (theEnv, PatternData (theEnv)->SalienceExpression);
      PatternData (theEnv)->SalienceExpression = NULL;
    }

  PatternData (theEnv)->GlobalSalience = salience;
}

/**************************************************************/
/* ParseAuto_Focus: Parses the rest of a defrule auto-focus    */
/*   declaration once the auto-focus keyword has been parsed. */
/**************************************************************/
static void
ParseAuto_Focus (Environment * theEnv, const char *readSource, bool *error)
{
  struct token theToken;

   /*========================================*/
  /* The auto-focus value must be a symbol. */
   /*========================================*/

  CL_SavePPBuffer (theEnv, " ");

  CL_GetToken (theEnv, readSource, &theToken);
  if (theToken.tknType != SYMBOL_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "auto-focus statement");
      *error = true;
      return;
    }

   /*====================================================*/
  /* The auto-focus value must be either TRUE or FALSE. */
  /* If a valid value is parsed, then set the value of  */
  /* the global variable GlobalAuto_Focus.               */
   /*====================================================*/

  if (strcmp (theToken.lexemeValue->contents, "TRUE") == 0)
    {
      PatternData (theEnv)->GlobalAuto_Focus = true;
    }
  else if (strcmp (theToken.lexemeValue->contents, "FALSE") == 0)
    {
      PatternData (theEnv)->GlobalAuto_Focus = false;
    }
  else
    {
      CL_SyntaxErrorMessage (theEnv, "auto-focus statement");
      *error = true;
    }
}

/*****************************************************************/
/* LHSPattern: Parses a single conditional element found on the  */
/*   LHS of a rule. Conditonal element types include pattern CEs */
/*   (which may be assigned to a variable), test CEs, not CEs,   */
/*   logical CEs, and CEs, and or CEs.                           */
/*                                                               */
/* <conditional-element> ::= <pattern-CE> |                      */
/*                           <assigned-pattern-CE> |             */
/*                           <not-CE> | <and-CE> | <or-CE> |     */
/*                           <logical-CE> | <test-CE> |          */
/*                           <forall-CE> | <exists-CE>           */
/*****************************************************************/
static struct lhsParseNode *
LHSPattern (Environment * theEnv,
	    const char *readSource,
	    TokenType te_rminator,
	    const char *te_rminatorString,
	    bool *error,
	    bool allowDeclaration,
	    struct token *firstToken, const char *ruleName)
{
  struct token theToken;
  struct lhsParseNode *theNode;

   /*=========================================================*/
  /* Check to see if the first token has already been read.  */
  /* This should only occur for the first pattern in a rule. */
   /*=========================================================*/

  if (firstToken == NULL)
    CL_GetToken (theEnv, readSource, &theToken);
  else
    CL_CopyToken (&theToken, firstToken);

   /*=====================================================*/
  /* A left parenthesis begins all CEs and declarations. */
   /*=====================================================*/

  if (theToken.tknType == LEFT_PARENTHESIS_TOKEN)
    {
      /*================================================*/
      /* The first field of a pattern must be a symbol. */
      /*================================================*/

      CL_GetToken (theEnv, readSource, &theToken);
      if (theToken.tknType != SYMBOL_TOKEN)
	{
	  CL_SyntaxErrorMessage (theEnv, "the first field of a pattern");
	  *error = true;
	  return NULL;
	}

      /*====================================*/
      /* If this is the first CE of a rule, */
      /* then a declare statement is valid. */
      /*====================================*/

      if (allowDeclaration &&
	  (strcmp (theToken.lexemeValue->contents, "declare") == 0))
	{
	  if (ruleName == NULL)
	    CL_SystemError (theEnv, "RULELHS", 1);
	  DeclarationParse (theEnv, readSource, ruleName, error);
	  theNode = NULL;
	}

      /*==================================*/
      /* Otherwise check for a *test* CE. */
      /*==================================*/

      else if (strcmp (theToken.lexemeValue->contents, "test") == 0)
	{
	  theNode = TestPattern (theEnv, readSource, error);
	}

      /*============================================*/
      /* Otherwise check for an *and*, *or*, *not*, */
      /* *logical*, *exists*, or *forall* CE.       */
      /*============================================*/

      else if ((strcmp (theToken.lexemeValue->contents, "and") == 0) ||
	       (strcmp (theToken.lexemeValue->contents, "logical") == 0) ||
	       (strcmp (theToken.lexemeValue->contents, "not") == 0) ||
	       (strcmp (theToken.lexemeValue->contents, "exists") == 0) ||
	       (strcmp (theToken.lexemeValue->contents, "forall") == 0) ||
	       (strcmp (theToken.lexemeValue->contents, "or") == 0))
	{
	  theNode =
	    ConnectedPatternParse (theEnv, readSource, &theToken, error);
	}

      /*=================================*/
      /* Otherwise parse a *pattern* CE. */
      /*=================================*/

      else
	{
	  theNode = SimplePatternParse (theEnv, readSource, &theToken, error);
	}
    }

   /*=======================================*/
  /* Check for a pattern address variable. */
   /*=======================================*/

  else if (theToken.tknType == SF_VARIABLE_TOKEN)
    {
      theNode =
	AssignmentParse (theEnv, readSource, theToken.lexemeValue, error);
    }

   /*=================================================*/
  /* Check for the group te_rminator (either a "=>"   */
  /* separating the LHS from the RHS or a ")" ending */
  /* a CE containing other CEs such as an *and* CE). */
   /*=================================================*/

  else if ((theToken.tknType == te_rminator) ?
	   (strcmp (theToken.printFo_rm, te_rminatorString) == 0) : false)
    {
      return NULL;
    }

   /*====================================*/
  /* Otherwise invalid syntax was used. */
   /*====================================*/

  else
    {
      CL_SyntaxErrorMessage (theEnv, "defrule");
      *error = true;
      return NULL;
    }

   /*================================*/
  /* If an error occurred, free any */
  /* allocated data structures.     */
   /*================================*/

  if (*error == true)
    {
      CL_ReturnLHSParseNodes (theEnv, theNode);
      return NULL;
    }

   /*=========================*/
  /* Return the LHS pattern. */
   /*=========================*/

  return (theNode);
}

/*********************************************************************/
/* ConnectedPatternParse:  Handles parsing of the connected          */
/*   conditional elements (i.e. those conditional elements that may  */
/*   contain one or more other conditional elements).  The connected */
/*   conditional elements include the *and*, *or*, *not*, *logical*, */
/*   *exists*, and *forall* CEs. This routine is entered with the    */
/*   parsing pointing to the name of the connected CE. It is exited  */
/*   with the parser pointing to the closing right parenthesis of    */
/*   the connected CE.                                               */
/*                                                                   */
/* <and-CE>      ::= (and <conditional-element>+)                    */
/*                                                                   */
/* <or-CE>       ::= (or <conditional-element>+)                     */
/*                                                                   */
/* <logical-CE>  ::= (logical <conditional-element>+)                */
/*                                                                   */
/* <not-CE>      ::= (not <conditional-element>)                     */
/*                                                                   */
/* <exists-CE>   ::= (exists <conditional-element>+)                 */
/*                                                                   */
/* <forall-CE>   ::= (forall <conditional-element>                   */
/*                           <conditional-element>+)                 */
/*********************************************************************/
static struct lhsParseNode *
ConnectedPatternParse (Environment * theEnv,
		       const char *readSource,
		       struct token *theToken, bool *error)
{
  ParseNodeType connectorValue = AND_CE_NODE;
  struct lhsParseNode *theNode, *tempNode, *theGroup;
  const char *errorCE = NULL;
  bool logical = false;
  bool tempValue;

   /*==========================================================*/
  /* Use appropriate spacing for pretty printing of the rule. */
   /*==========================================================*/

  CL_IncrementIndentDepth (theEnv, 5);
  if (strcmp (theToken->lexemeValue->contents, "or") == 0)
    {
      connectorValue = OR_CE_NODE;
      errorCE = "the or conditional element";
      CL_SavePPBuffer (theEnv, "  ");
    }
  else if (strcmp (theToken->lexemeValue->contents, "and") == 0)
    {
      connectorValue = AND_CE_NODE;
      errorCE = "the and conditional element";
      CL_SavePPBuffer (theEnv, " ");
    }
  else if (strcmp (theToken->lexemeValue->contents, "not") == 0)
    {
      connectorValue = NOT_CE_NODE;
      errorCE = "the not conditional element";
      CL_SavePPBuffer (theEnv, " ");
    }
  else if (strcmp (theToken->lexemeValue->contents, "exists") == 0)
    {
      connectorValue = EXISTS_CE_NODE;
      errorCE = "the exists conditional element";
      CL_PPCRAndIndent (theEnv);
    }
  else if (strcmp (theToken->lexemeValue->contents, "forall") == 0)
    {
      connectorValue = FORALL_CE_NODE;
      errorCE = "the forall conditional element";
      CL_PPCRAndIndent (theEnv);
    }
  else if (strcmp (theToken->lexemeValue->contents, "logical") == 0)
    {
      connectorValue = AND_CE_NODE;
      errorCE = "the logical conditional element";
      logical = true;
      CL_PPCRAndIndent (theEnv);
    }

   /*=====================================================*/
  /* The logical CE cannot be contained within a not CE. */
   /*=====================================================*/

  if (PatternData (theEnv)->WithinNotCE && logical)
    {
      CL_PrintErrorID (theEnv, "RULELHS", 1, true);
      CL_WriteString (theEnv, STDERR,
		      "The logical CE cannot be used within a not/exists/forall CE.\n");
      *error = true;
      return NULL;
    }

   /*=====================================================*/
  /* Remember if we're currently within a *not* CE and   */
  /* then check to see if we're entering a new *not* CE. */
   /*=====================================================*/

  tempValue = PatternData (theEnv)->WithinNotCE;
  if ((connectorValue == NOT_CE_NODE) ||
      (connectorValue == EXISTS_CE_NODE) ||
      (connectorValue == FORALL_CE_NODE))
    {
      PatternData (theEnv)->WithinNotCE = true;
    }

   /*===========================================*/
  /* Parse all of the CEs contained with the   */
  /* CE. A ) will te_rminate the end of the CE. */
   /*===========================================*/

  theGroup =
    GroupPatterns (theEnv, readSource, RIGHT_PARENTHESIS_TOKEN, ")", error);

   /*====================================*/
  /* Restore the "with a *not* CE" flag */
  /* and reset the indentation depth.   */
   /*====================================*/

  PatternData (theEnv)->WithinNotCE = tempValue;
  CL_DecrementIndentDepth (theEnv, 5);

   /*============================================*/
  /* If an error occured while parsing, return. */
   /*============================================*/

  if (*error == true)
    {
      CL_ReturnLHSParseNodes (theEnv, theGroup);
      return NULL;
    }

   /*=========================================================*/
  /* If we parsed a *logical* CE, then mark the logical flag */
  /* for all of the CEs contained within the logical CE.     */
   /*=========================================================*/

  if (logical)
    TagLHSLogicalNodes (theGroup);

   /*=====================================================*/
  /* All the connected CEs must contain at least one CE. */
   /*=====================================================*/

  if (theGroup == NULL)
    {
      CL_SyntaxErrorMessage (theEnv, errorCE);
      *error = true;
      return NULL;
    }

   /*============================================*/
  /* A not CE may not contain more than one CE. */
   /*============================================*/

  if ((connectorValue == NOT_CE_NODE) && (theGroup->bottom != NULL))
    {
      CL_SyntaxErrorMessage (theEnv, errorCE);
      CL_ReturnLHSParseNodes (theEnv, theGroup);
      *error = true;
      return NULL;
    }

   /*============================================*/
  /* A forall CE must contain at least two CEs. */
   /*============================================*/

  if ((connectorValue == FORALL_CE_NODE) && (theGroup->bottom == NULL))
    {
      CL_SyntaxErrorMessage (theEnv, errorCE);
      CL_ReturnLHSParseNodes (theEnv, theGroup);
      *error = true;
      return NULL;
    }

   /*========================================================*/
  /* Remove an "and" and "or" CE that only contains one CE. */
   /*========================================================*/

  if (((connectorValue == AND_CE_NODE) || (connectorValue == OR_CE_NODE)) &&
      (theGroup->bottom == NULL))
    {
      theGroup->logical = logical;
      return (theGroup);
    }

   /*===========================================================*/
  /* Create the top most node which connects the CEs together. */
   /*===========================================================*/

  theNode = CL_GetLHSParseNode (theEnv);
  theNode->logical = logical;

   /*======================================================*/
  /* Attach and/or/not CEs directly to the top most node. */
   /*======================================================*/

  if ((connectorValue == AND_CE_NODE) ||
      (connectorValue == OR_CE_NODE) || (connectorValue == NOT_CE_NODE))
    {
      theNode->pnType = connectorValue;
      theNode->right = theGroup;
    }

   /*=================================================================*/
  /* Wrap two not CEs around the patterns contained in an exists CE. */
   /*=================================================================*/

  else if (connectorValue == EXISTS_CE_NODE)
    {
      theNode->pnType = NOT_CE_NODE;

      theNode->right = CL_GetLHSParseNode (theEnv);
      theNode->right->pnType = NOT_CE_NODE;
      theNode->right->logical = logical;

      if (theGroup->bottom != NULL)
	{
	  theNode->right->right = CL_GetLHSParseNode (theEnv);
	  theNode->right->right->pnType = AND_CE_NODE;
	  theNode->right->right->logical = logical;
	  theNode->right->right->right = theGroup;
	}
      else
	{
	  theNode->right->right = theGroup;
	}
    }

   /*==================================================*/
  /* For a forall CE, wrap a not CE around all of the */
  /* CEs and a not CE around the 2nd through nth CEs. */
   /*==================================================*/

  else if (connectorValue == FORALL_CE_NODE)
    {
      theNode->pnType = NOT_CE_NODE;

      tempNode = theGroup->bottom;
      theGroup->bottom = NULL;

      theNode->right = CL_GetLHSParseNode (theEnv);
      theNode->right->pnType = AND_CE_NODE;
      theNode->right->logical = logical;
      theNode->right->right = theGroup;

      theGroup = tempNode;

      theNode->right->right->bottom = CL_GetLHSParseNode (theEnv);
      theNode->right->right->bottom->pnType = NOT_CE_NODE;
      theNode->right->right->bottom->logical = logical;

      tempNode = theNode->right->right->bottom;

      if (theGroup->bottom == NULL)
	{
	  tempNode->right = theGroup;
	}
      else
	{
	  tempNode->right = CL_GetLHSParseNode (theEnv);
	  tempNode->right->pnType = AND_CE_NODE;
	  tempNode->right->logical = logical;
	  tempNode->right->right = theGroup;
	}
    }

   /*================*/
  /* Return the CE. */
   /*================*/

  return (theNode);
}

/***********************************************/
/* GroupPatterns: Groups a series of connected */
/*   conditional elements together.            */
/***********************************************/
static struct lhsParseNode *
GroupPatterns (Environment * theEnv,
	       const char *readSource,
	       TokenType te_rminator,
	       const char *te_rminatorString, bool *error)
{
  struct lhsParseNode *lastNode, *newNode, *theNode;

  lastNode = theNode = NULL;

  while (true)
    {
      /*==================*/
      /* Get the next CE. */
      /*==================*/

      newNode =
	LHSPattern (theEnv, readSource, te_rminator, te_rminatorString, error,
		    false, NULL, NULL);

      /*=======================================================*/
      /* If an error occurred, release any LHS data structures */
      /* previously allocated by this routine.                 */
      /*=======================================================*/

      if (*error)
	{
	  CL_ReturnLHSParseNodes (theEnv, theNode);
	  return NULL;
	}

      /*===============================================*/
      /* A NULL value for the CE just parsed indicates */
      /* that the te_rminator for the group of patterns */
      /* was encountered (either a "=>" or a ")".      */
      /*===============================================*/

      if (newNode == NULL)
	{
	  CL_PPBackup (theEnv);
	  CL_PPBackup (theEnv);
	  if (te_rminator == RIGHT_PARENTHESIS_TOKEN)
	    {
	      CL_SavePPBuffer (theEnv, te_rminatorString);
	    }
	  else
	    {
	      CL_PPCRAndIndent (theEnv);
	      CL_SavePPBuffer (theEnv, te_rminatorString);
	    }

	  return (theNode);
	}

      /*============================*/
      /* Add the new CE to the list */
      /* of CEs being grouped.      */
      /*============================*/

      if (lastNode == NULL)
	{
	  theNode = newNode;
	}
      else
	{
	  lastNode->bottom = newNode;
	}

      lastNode = newNode;

      /*======================================*/
      /* Fix the pretty print representation. */
      /*======================================*/

      CL_PPCRAndIndent (theEnv);
    }
}

/**************************************************************/
/* TestPattern: Handles parsing of test conditional elements. */
/*                                                            */
/* <test-CE> ::= (test <function-call>)                       */
/**************************************************************/
static struct lhsParseNode *
TestPattern (Environment * theEnv, const char *readSource, bool *error)
{
  struct lhsParseNode *theNode;
  struct token theToken;
  struct expr *theExpression;

   /*================================================*/
  /* Create the data specification for the test CE. */
   /*================================================*/

  CL_SavePPBuffer (theEnv, " ");
  theNode = CL_GetLHSParseNode (theEnv);
  theNode->pnType = TEST_CE_NODE;
  theExpression = CL_Function0Parse (theEnv, readSource);
  theNode->expression = CL_ExpressionToLHSParseNodes (theEnv, theExpression);
  CL_ReturnExpression (theEnv, theExpression);

  if (theNode->expression == NULL)
    {
      *error = true;
      CL_ReturnLHSParseNodes (theEnv, theNode);
      return NULL;
    }

   /*=========================================================*/
  /* Check for the closing right parenthesis of the test CE. */
   /*=========================================================*/

  CL_GetToken (theEnv, readSource, &theToken);
  if (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "test conditional element");
      *error = true;
      CL_ReturnLHSParseNodes (theEnv, theNode);
      return NULL;
    }

   /*=====================*/
  /* Return the test CE. */
   /*=====================*/

  return (theNode);
}

/****************************************************************/
/* AssignmentParse: Finishes the parsing of pattern conditional */
/*   elements that have been bound to a  variable.              */
/*                                                              */
/* <assigned-pattern-CE> ::= ?<variable-symbol> <- <pattern-CE> */
/****************************************************************/
static struct lhsParseNode *
AssignmentParse (Environment * theEnv,
		 const char *readSource,
		 CLIPSLexeme * factAddress, bool *error)
{
  struct lhsParseNode *theNode;
  struct token theToken;

   /*=====================================================*/
  /* Patterns cannot be bound if they are with a not CE. */
   /*=====================================================*/

  if (PatternData (theEnv)->WithinNotCE)
    {
      CL_PrintErrorID (theEnv, "RULELHS", 2, true);
      CL_WriteString (theEnv, STDERR,
		      "A pattern CE cannot be bound to a pattern-address within a not CE\n");
      *error = true;
      return NULL;
    }

   /*===============================================*/
  /* Check for binder token, "<-", after variable. */
   /*===============================================*/

  CL_SavePPBuffer (theEnv, " ");

  CL_GetToken (theEnv, readSource, &theToken);

  if ((theToken.tknType ==
       SYMBOL_TOKEN) ? (strcmp (theToken.lexemeValue->contents,
				"<-") != 0) : true)
    {
      CL_SyntaxErrorMessage (theEnv, "binding patterns");
      *error = true;
      return NULL;
    }

  CL_SavePPBuffer (theEnv, " ");

   /*================================================*/
  /* Check for opening left parenthesis of pattern. */
   /*================================================*/

  CL_GetToken (theEnv, readSource, &theToken);
  if (theToken.tknType != LEFT_PARENTHESIS_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "binding patterns");
      *error = true;
      return NULL;
    }

   /*======================================================*/
  /* Parse the pattern and return the data specification. */
   /*======================================================*/

  CL_GetToken (theEnv, readSource, &theToken);

  theNode = SimplePatternParse (theEnv, readSource, &theToken, error);

  if (*error == true)
    {
      CL_ReturnLHSParseNodes (theEnv, theNode);
      return NULL;
    }

   /*=============================================*/
  /* Store the name of the variable to which the */
  /* pattern is bound and return the pattern.    */
   /*=============================================*/

  theNode->value = factAddress;
  return (theNode);
}

/************************************************************/
/* TagLHSLogicalNodes: Marks all *and*, *or*, and *not* CEs */
/*   contained within a logical CE as having the properties */
/*   associated with a logical CE.                          */
/************************************************************/
static void
TagLHSLogicalNodes (struct lhsParseNode *nodePtr)
{
  while (nodePtr != NULL)
    {
      nodePtr->logical = true;
      if ((nodePtr->pnType == AND_CE_NODE) ||
	  (nodePtr->pnType == OR_CE_NODE) || (nodePtr->pnType == NOT_CE_NODE))
	{
	  TagLHSLogicalNodes (nodePtr->right);
	}
      nodePtr = nodePtr->bottom;
    }
}

/***********************************************************/
/* SimplePatternParse: Parses a simple pattern (an opening */
/*   parenthesis followed by one or more fields followed   */
/*   by a closing parenthesis).                            */
/*                                                         */
/* <pattern-CE> ::= <ordered-pattern-CE> |                 */
/*                  <template-pattern-CE>                  */
/***********************************************************/
static struct lhsParseNode *
SimplePatternParse (Environment * theEnv,
		    const char *readSource,
		    struct token *theToken, bool *error)
{
  struct lhsParseNode *theNode;
  struct patternParser *tempParser;

   /*=================================================*/
  /* The first field of a pattern must be a symbol.  */
  /* In addition, the symbols ":" and "=" can not    */
  /* be used because they have special significance. */
   /*=================================================*/

  if (theToken->tknType != SYMBOL_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "the first field of a pattern");
      *error = true;
      return NULL;
    }
  else if ((strcmp (theToken->lexemeValue->contents, "=") == 0) ||
	   (strcmp (theToken->lexemeValue->contents, ":") == 0))
    {
      CL_SyntaxErrorMessage (theEnv, "the field field of a pattern");
      *error = true;
      return NULL;
    }

   /*===============================================*/
  /* Construct the topmost node of the pattern CE. */
   /*===============================================*/

  theNode = CL_GetLHSParseNode (theEnv);
  theNode->pnType = PATTERN_CE_NODE;
  theNode->negated = false;
  theNode->exists = false;

   /*======================================================*/
  /* Search for a pattern parser that claims the pattern. */
   /*======================================================*/

  for (tempParser = PatternData (theEnv)->ListOfPatternParsers;
       tempParser != NULL; tempParser = tempParser->next)
    {
      if ((*tempParser->recognizeFunction) (theToken->lexemeValue))
	{
	  theNode->patternType = tempParser;
	  theNode->right =
	    (*tempParser->parseFunction) (theEnv, readSource, theToken);
	  if (theNode->right == NULL)
	    {
	      *error = true;
	      CL_ReturnLHSParseNodes (theEnv, theNode);
	      return NULL;
	    }

	  CL_PropagatePatternType (theNode, tempParser);
	  return (theNode);
	}
    }

   /*=================================*/
  /* If a pattern parser couldn't be */
  /* found, then signal an error.    */
   /*=================================*/

  *error = true;
  CL_SyntaxErrorMessage (theEnv, "the field field of a pattern");
  CL_ReturnLHSParseNodes (theEnv, theNode);
  return NULL;
}

/**************************************************************/
/* CL_PropagatePatternType: Sets the selfPattern field for all   */
/*   lhsParseNodes in a linked list of those data structures. */
/**************************************************************/
void
CL_PropagatePatternType (struct lhsParseNode *theLHS,
			 struct patternParser *theParser)
{
  while (theLHS != NULL)
    {
      theLHS->patternType = theParser;
      if (theLHS->right != NULL)
	CL_PropagatePatternType (theLHS->right, theParser);
      if (theLHS->expression != NULL)
	CL_PropagatePatternType (theLHS->expression, theParser);
      theLHS = theLHS->bottom;
    }
}

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) &&  DEFRULE_CONSTRUCT */
