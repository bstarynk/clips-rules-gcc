   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  09/22/17             */
   /*                                                     */
   /*            FACT-SET QUERIES PARSER MODULE           */
   /*******************************************************/

/*************************************************************/
/* Purpose: Fact_set Queries Parsing Routines                */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Gary D. Riley                                        */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Added fact-set queries.                        */
/*                                                           */
/*            Changed name of variable exp to theExp         */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Fixed memory leaks when error occurred.        */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Added code to keep track of pointers to        */
/*            constructs that are contained externally to    */
/*            to constructs, DanglingConstructs.             */
/*                                                           */
/*      6.31: Error check for non-symbolic slot names.       */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if FACT_SET_QUERIES

#include <string.h>

#include "envrnmnt.h"
#include "exprnpsr.h"
#include "extnfunc.h"
#include "factqury.h"
#include "modulutl.h"
#include "prcdrpsr.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"
#include "strngrtr.h"

#include "factqpsr.h"

/* =========================================
   *****************************************
                   CONSTANTS
   =========================================
   ***************************************** */
#define FACT_SLOT_REF ':'

/* =========================================
   *****************************************
      INTERNALLY VISIBLE FUNCTION HEADERS
   =========================================
   ***************************************** */

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static Expression *ParseQueryRestrictions (Environment *, Expression *,
					   const char *, struct token *);
static bool ReplaceTemplateNameWithReference (Environment *, Expression *);
static bool ParseQueryTestExpression (Environment *, Expression *,
				      const char *);
static bool CL_ParseQueryActionExpression (Environment *, Expression *,
					   const char *, Expression *,
					   struct token *);
static bool CL_ReplaceFactVariables (Environment *, Expression *,
				     Expression *, bool, int);
static bool ReplaceSlotReference (Environment *, Expression *, Expression *,
				  struct functionDefinition *, int);
static bool IsQueryFunction (Expression *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***********************************************************************
  NAME         : CL_Fact_ParseQueryNoAction
  DESCRIPTION  : Parses the following functions :
                   (any-factp)
                   (find-first-fact)
                   (find-all-facts)
  INPUTS       : 1) The address of the top node of the query function
                 2) The logical name of the input
  RETURNS      : The completed expression chain, or NULL on errors
  SIDE EFFECTS : The expression chain is extended, or the "top" node
                 is deleted on errors
  NOTES        : H/L Syntax :

                 (<function> <query-block>)

                 <query-block>  :== (<fact-var>+) <query-expression>
                 <fact-var> :== (<var-name> <template-name>+)

                 Parses into following fo_rm :

                 <query-function>
                      |
                      V
                 <query-expression>  ->

                 <template-1a> -> <template-1b> -> (QDS) ->

                 <template-2a> -> <template-2b> -> (QDS) -> ...
 ***********************************************************************/
Expression *
CL_Fact_ParseQueryNoAction (Environment * theEnv,
			    Expression * top, const char *readSource)
{
  Expression *factQuerySetVars;
  struct token queryInputToken;

  factQuerySetVars =
    ParseQueryRestrictions (theEnv, top, readSource, &queryInputToken);
  if (factQuerySetVars == NULL)
    {
      return NULL;
    }

  CL_IncrementIndentDepth (theEnv, 3);
  CL_PPCRAndIndent (theEnv);

  if (ParseQueryTestExpression (theEnv, top, readSource) == false)
    {
      CL_DecrementIndentDepth (theEnv, 3);
      CL_ReturnExpression (theEnv, factQuerySetVars);
      return NULL;
    }

  CL_DecrementIndentDepth (theEnv, 3);

  CL_GetToken (theEnv, readSource, &queryInputToken);
  if (queryInputToken.tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "fact-set query function");
      CL_ReturnExpression (theEnv, top);
      CL_ReturnExpression (theEnv, factQuerySetVars);
      return NULL;
    }

  if (CL_ReplaceFactVariables
      (theEnv, factQuerySetVars, top->argList, true, 0))
    {
      CL_ReturnExpression (theEnv, top);
      CL_ReturnExpression (theEnv, factQuerySetVars);
      return NULL;
    }

  CL_ReturnExpression (theEnv, factQuerySetVars);

  return top;
}

/***********************************************************************
  NAME         : CL_Fact_ParseQueryAction
  DESCRIPTION  : Parses the following functions :
                   (do-for-fact)
                   (do-for-all-facts)
                   (delayed-do-for-all-facts)
  INPUTS       : 1) The address of the top node of the query function
                 2) The logical name of the input
  RETURNS      : The completed expression chain, or NULL on errors
  SIDE EFFECTS : The expression chain is extended, or the "top" node
                 is deleted on errors
  NOTES        : H/L Syntax :

                 (<function> <query-block> <query-action>)

                 <query-block>  :== (<fact-var>+) <query-expression>
                 <fact-var> :== (<var-name> <template-name>+)

                 Parses into following fo_rm :

                 <query-function>
                      |
                      V
                 <query-expression> -> <query-action>  ->

                 <template-1a> -> <template-1b> -> (QDS) ->

                 <template-2a> -> <template-2b> -> (QDS) -> ...
 ***********************************************************************/
Expression *
CL_Fact_ParseQueryAction (Environment * theEnv,
			  Expression * top, const char *readSource)
{
  Expression *factQuerySetVars;
  struct token queryInputToken;

  factQuerySetVars =
    ParseQueryRestrictions (theEnv, top, readSource, &queryInputToken);
  if (factQuerySetVars == NULL)
    {
      return NULL;
    }

  CL_IncrementIndentDepth (theEnv, 3);
  CL_PPCRAndIndent (theEnv);

  if (ParseQueryTestExpression (theEnv, top, readSource) == false)
    {
      CL_DecrementIndentDepth (theEnv, 3);
      CL_ReturnExpression (theEnv, factQuerySetVars);
      return NULL;
    }

  CL_PPCRAndIndent (theEnv);

  if (CL_ParseQueryActionExpression
      (theEnv, top, readSource, factQuerySetVars, &queryInputToken) == false)
    {
      CL_DecrementIndentDepth (theEnv, 3);
      CL_ReturnExpression (theEnv, factQuerySetVars);
      return NULL;
    }

  CL_DecrementIndentDepth (theEnv, 3);

  if (queryInputToken.tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "fact-set query function");
      CL_ReturnExpression (theEnv, top);
      CL_ReturnExpression (theEnv, factQuerySetVars);
      return NULL;
    }

  if (CL_ReplaceFactVariables
      (theEnv, factQuerySetVars, top->argList, true, 0))
    {
      CL_ReturnExpression (theEnv, top);
      CL_ReturnExpression (theEnv, factQuerySetVars);
      return NULL;
    }

  if (CL_ReplaceFactVariables
      (theEnv, factQuerySetVars, top->argList->nextArg, false, 0))
    {
      CL_ReturnExpression (theEnv, top);
      CL_ReturnExpression (theEnv, factQuerySetVars);
      return NULL;
    }

  CL_ReturnExpression (theEnv, factQuerySetVars);

  return top;
}

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************************
  NAME         : ParseQueryRestrictions
  DESCRIPTION  : Parses the template restrictions for a query
  INPUTS       : 1) The top node of the query expression
                 2) The logical name of the input
                 3) Caller's token buffer
  RETURNS      : The fact-variable expressions
  SIDE EFFECTS : Entire query expression deleted on errors
                 Nodes allocated for restrictions and fact
                   variable expressions
                 Template restrictions attached to query-expression
                   as arguments
  NOTES        : Expects top != NULL
 ***************************************************************/
static Expression *
ParseQueryRestrictions (Environment * theEnv,
			Expression * top,
			const char *readSource, struct token *queryInputToken)
{
  Expression *factQuerySetVars = NULL, *lastFactQuerySetVars = NULL,
    *templateExp = NULL, *lastTemplateExp, *tmp, *lastOne = NULL;
  bool error = false;

  CL_SavePPBuffer (theEnv, " ");

  CL_GetToken (theEnv, readSource, queryInputToken);
  if (queryInputToken->tknType != LEFT_PARENTHESIS_TOKEN)
    {
      goto ParseQueryRestrictionsError1;
    }

  CL_GetToken (theEnv, readSource, queryInputToken);
  if (queryInputToken->tknType != LEFT_PARENTHESIS_TOKEN)
    {
      goto ParseQueryRestrictionsError1;
    }

  while (queryInputToken->tknType == LEFT_PARENTHESIS_TOKEN)
    {
      CL_GetToken (theEnv, readSource, queryInputToken);
      if (queryInputToken->tknType != SF_VARIABLE_TOKEN)
	{
	  goto ParseQueryRestrictionsError1;
	}

      tmp = factQuerySetVars;
      while (tmp != NULL)
	{
	  if (tmp->value == queryInputToken->value)
	    {
	      CL_PrintErrorID (theEnv, "FACTQPSR", 1, false);
	      CL_WriteString (theEnv, STDERR,
			      "Duplicate fact member variable name in function ");
	      CL_WriteString (theEnv, STDERR,
			      ExpressionFunctionCallName (top)->contents);
	      CL_WriteString (theEnv, STDERR, ".\n");
	      goto ParseQueryRestrictionsError2;
	    }

	  tmp = tmp->nextArg;
	}

      tmp = CL_GenConstant (theEnv, SF_VARIABLE, queryInputToken->value);
      if (factQuerySetVars == NULL)
	{
	  factQuerySetVars = tmp;
	}
      else
	{
	  lastFactQuerySetVars->nextArg = tmp;
	}

      lastFactQuerySetVars = tmp;
      CL_SavePPBuffer (theEnv, " ");

      templateExp = CL_ArgumentParse (theEnv, readSource, &error);

      if (error)
	{
	  goto ParseQueryRestrictionsError2;
	}

      if (templateExp == NULL)
	{
	  goto ParseQueryRestrictionsError1;
	}

      if (ReplaceTemplateNameWithReference (theEnv, templateExp) == false)
	{
	  goto ParseQueryRestrictionsError2;
	}

      lastTemplateExp = templateExp;
      CL_SavePPBuffer (theEnv, " ");

      while ((tmp = CL_ArgumentParse (theEnv, readSource, &error)) != NULL)
	{
	  if (ReplaceTemplateNameWithReference (theEnv, tmp) == false)
	    goto ParseQueryRestrictionsError2;
	  lastTemplateExp->nextArg = tmp;
	  lastTemplateExp = tmp;
	  CL_SavePPBuffer (theEnv, " ");
	}

      if (error)
	{
	  goto ParseQueryRestrictionsError2;
	}

      CL_PPBackup (theEnv);
      CL_PPBackup (theEnv);
      CL_SavePPBuffer (theEnv, ")");

      tmp =
	CL_GenConstant (theEnv, SYMBOL_TYPE,
			FactQueryData (theEnv)->QUERY_DELIMITER_SYMBOL);

      lastTemplateExp->nextArg = tmp;
      lastTemplateExp = tmp;

      if (top->argList == NULL)
	{
	  top->argList = templateExp;
	}
      else
	{
	  lastOne->nextArg = templateExp;
	}

      lastOne = lastTemplateExp;
      templateExp = NULL;
      CL_SavePPBuffer (theEnv, " ");
      CL_GetToken (theEnv, readSource, queryInputToken);
    }

  if (queryInputToken->tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      goto ParseQueryRestrictionsError1;
    }

  CL_PPBackup (theEnv);
  CL_PPBackup (theEnv);
  CL_SavePPBuffer (theEnv, ")");
  return (factQuerySetVars);

ParseQueryRestrictionsError1:
  CL_SyntaxErrorMessage (theEnv, "fact-set query function");

ParseQueryRestrictionsError2:
  CL_ReturnExpression (theEnv, templateExp);
  CL_ReturnExpression (theEnv, top);
  CL_ReturnExpression (theEnv, factQuerySetVars);
  return NULL;
}

/***************************************************
  NAME         : ReplaceTemplateNameWithReference
  DESCRIPTION  : In parsing an fact-set query,
                 this function replaces a constant
                 template name with an actual pointer
                 to the template
  INPUTS       : The expression
  RETURNS      : True if all OK, otherwise false
                 if template cannot be found
  SIDE EFFECTS : The expression type and value are
                 modified if template is found
  NOTES        : Searches current and imported
                 modules for reference
 ***************************************************/
static bool
ReplaceTemplateNameWithReference (Environment * theEnv, Expression * theExp)
{
  const char *theTemplateName;
  void *theDeftemplate;
  unsigned int count;

  if (theExp->type == SYMBOL_TYPE)
    {
      theTemplateName = theExp->lexemeValue->contents;

      theDeftemplate = (Deftemplate *)
	CL_FindImportedConstruct (theEnv, "deftemplate", NULL,
				  theTemplateName, &count, true, NULL);

      if (theDeftemplate == NULL)
	{
	  CL_CantFindItemErrorMessage (theEnv, "deftemplate", theTemplateName,
				       true);
	  return false;
	}

      if (count > 1)
	{
	  CL_AmbiguousReferenceErrorMessage (theEnv, "deftemplate",
					     theTemplateName);
	  return false;
	}

      theExp->type = DEFTEMPLATE_PTR;
      theExp->value = theDeftemplate;

#if (! RUN_TIME) && (! BLOAD_ONLY)
      if (!ConstructData (theEnv)->ParsingConstruct)
	{
	  ConstructData (theEnv)->DanglingConstructs++;
	}
#endif
    }

  return true;
}

/*************************************************************
  NAME         : ParseQueryTestExpression
  DESCRIPTION  : Parses the test-expression for a query
  INPUTS       : 1) The top node of the query expression
                 2) The logical name of the input
  RETURNS      : True if all OK, false otherwise
  SIDE EFFECTS : Entire query-expression deleted on errors
                 Nodes allocated for new expression
                 Test shoved in front of class-restrictions on
                    query argument list
  NOTES        : Expects top != NULL
 *************************************************************/
static bool
ParseQueryTestExpression (Environment * theEnv,
			  Expression * top, const char *readSource)
{
  Expression *qtest;
  bool error;
  struct BindInfo *oldBindList;

  error = false;
  oldBindList = CL_GetParsedBindNames (theEnv);
  CL_SetParsedBindNames (theEnv, NULL);

  qtest = CL_ArgumentParse (theEnv, readSource, &error);

  if (error == true)
    {
      CL_ClearParsedBindNames (theEnv);
      CL_SetParsedBindNames (theEnv, oldBindList);
      CL_ReturnExpression (theEnv, top);
      return false;
    }

  if (qtest == NULL)
    {
      CL_ClearParsedBindNames (theEnv);
      CL_SetParsedBindNames (theEnv, oldBindList);
      CL_SyntaxErrorMessage (theEnv, "fact-set query function");
      CL_ReturnExpression (theEnv, top);
      return false;
    }

  qtest->nextArg = top->argList;
  top->argList = qtest;

  if (CL_ParsedBindNamesEmpty (theEnv) == false)
    {
      CL_ClearParsedBindNames (theEnv);
      CL_SetParsedBindNames (theEnv, oldBindList);
      CL_PrintErrorID (theEnv, "FACTQPSR", 2, false);
      CL_WriteString (theEnv, STDERR,
		      "Binds are not allowed in fact-set query in function ");
      CL_WriteString (theEnv, STDERR,
		      ExpressionFunctionCallName (top)->contents);
      CL_WriteString (theEnv, STDERR, ".\n");
      CL_ReturnExpression (theEnv, top);
      return false;
    }

  CL_SetParsedBindNames (theEnv, oldBindList);

  return true;
}

/*************************************************************
  NAME         : CL_ParseQueryActionExpression
  DESCRIPTION  : Parses the action-expression for a query
  INPUTS       : 1) The top node of the query expression
                 2) The logical name of the input
                 3) List of query parameters
  RETURNS      : True if all OK, false otherwise
  SIDE EFFECTS : Entire query-expression deleted on errors
                 Nodes allocated for new expression
                 Action shoved in front of template-restrictions
                    and in back of test-expression on query
                    argument list
  NOTES        : Expects top != NULL && top->argList != NULL
 *************************************************************/
static bool
CL_ParseQueryActionExpression (Environment * theEnv,
			       Expression * top,
			       const char *readSource,
			       Expression * factQuerySetVars,
			       struct token *queryInputToken)
{
  Expression *qaction, *tmpFactSetVars;
  struct BindInfo *oldBindList, *newBindList, *prev;

  oldBindList = CL_GetParsedBindNames (theEnv);
  CL_SetParsedBindNames (theEnv, NULL);

  ExpressionData (theEnv)->BreakContext = true;
  ExpressionData (theEnv)->ReturnContext =
    ExpressionData (theEnv)->svContexts->rtn;

  qaction =
    CL_GroupActions (theEnv, readSource, queryInputToken, true, NULL, false);

  CL_PPBackup (theEnv);
  CL_PPBackup (theEnv);
  CL_SavePPBuffer (theEnv, queryInputToken->printFo_rm);

  ExpressionData (theEnv)->BreakContext = false;

  if (qaction == NULL)
    {
      CL_ClearParsedBindNames (theEnv);
      CL_SetParsedBindNames (theEnv, oldBindList);
      CL_SyntaxErrorMessage (theEnv, "fact-set query function");
      CL_ReturnExpression (theEnv, top);
      return false;
    }

  qaction->nextArg = top->argList->nextArg;
  top->argList->nextArg = qaction;

  newBindList = CL_GetParsedBindNames (theEnv);
  prev = NULL;
  while (newBindList != NULL)
    {
      tmpFactSetVars = factQuerySetVars;
      while (tmpFactSetVars != NULL)
	{
	  if (tmpFactSetVars->value == (void *) newBindList->name)
	    {
	      CL_ClearParsedBindNames (theEnv);
	      CL_SetParsedBindNames (theEnv, oldBindList);
	      CL_PrintErrorID (theEnv, "FACTQPSR", 3, false);
	      CL_WriteString (theEnv, STDERR,
			      "Cannot rebind fact-set member variable ");
	      CL_WriteString (theEnv, STDERR,
			      tmpFactSetVars->lexemeValue->contents);
	      CL_WriteString (theEnv, STDERR, " in function ");
	      CL_WriteString (theEnv, STDERR,
			      ExpressionFunctionCallName (top)->contents);
	      CL_WriteString (theEnv, STDERR, ".\n");
	      CL_ReturnExpression (theEnv, top);
	      return false;
	    }
	  tmpFactSetVars = tmpFactSetVars->nextArg;
	}
      prev = newBindList;
      newBindList = newBindList->next;
    }

  if (prev == NULL)
    {
      CL_SetParsedBindNames (theEnv, oldBindList);
    }
  else
    {
      prev->next = oldBindList;
    }

  return true;
}

/***********************************************************************************
  NAME         : CL_ReplaceFactVariables
  DESCRIPTION  : Replaces all references to fact-variables within an
                   fact query-function with function calls to query-fact
                   (which references the fact array at run-time)
  INPUTS       : 1) The fact-variable list
                 2) A boolean expression containing variable references
                 3) A flag indicating whether to allow slot references of the type
                    <fact-query-variable>:<slot-name> for direct slot access
                    or not
                 4) Nesting depth of query functions
  RETURNS      : Nothing useful
  SIDE EFFECTS : If a SF_VARIABLE node is found and is on the list of fact
                   variables, it is replaced with a query-fact function call.
  NOTES        : Other SF_VARIABLE(S) are left alone for replacement by other
                   parsers.  This implies that a user may use defgeneric,
                   defrule, and defmessage-handler variables within a query-function
                   where they do not conflict with fact-variable names.
 ***********************************************************************************/
static bool
CL_ReplaceFactVariables (Environment * theEnv,
			 Expression * vlist,
			 Expression * bexp, bool sdirect, int ndepth)
{
  Expression *eptr;
  struct functionDefinition *rindx_func, *rslot_func;
  int posn;

  rindx_func = CL_FindFunction (theEnv, "(query-fact)");
  rslot_func = CL_FindFunction (theEnv, "(query-fact-slot)");
  while (bexp != NULL)
    {
      if (bexp->type == SF_VARIABLE)
	{
	  eptr = vlist;
	  posn = 0;
	  while ((eptr != NULL) ? (eptr->value != bexp->value) : false)
	    {
	      eptr = eptr->nextArg;
	      posn++;
	    }
	  if (eptr != NULL)
	    {
	      bexp->type = FCALL;
	      bexp->value = rindx_func;
	      eptr =
		CL_GenConstant (theEnv, INTEGER_TYPE,
				CL_CreateInteger (theEnv, ndepth));
	      eptr->nextArg =
		CL_GenConstant (theEnv, INTEGER_TYPE,
				CL_CreateInteger (theEnv, posn));
	      bexp->argList = eptr;
	    }
	  else if (sdirect == true)
	    {
	      if (ReplaceSlotReference
		  (theEnv, vlist, bexp, rslot_func, ndepth))
		{
		  return true;
		}
	    }
	}
      if (bexp->argList != NULL)
	{
	  if (IsQueryFunction (bexp))
	    {
	      if (CL_ReplaceFactVariables
		  (theEnv, vlist, bexp->argList, sdirect, ndepth + 1))
		{
		  return true;
		}
	    }
	  else
	    {
	      if (CL_ReplaceFactVariables
		  (theEnv, vlist, bexp->argList, sdirect, ndepth))
		{
		  return true;
		}
	    }
	}
      bexp = bexp->nextArg;
    }

  return false;
}

/*************************************************************************
  NAME         : ReplaceSlotReference
  DESCRIPTION  : Replaces fact-set query function variable
                   references of the fo_rm: <fact-variable>:<slot-name>
                   with function calls to get these fact-slots at run
                   time
  INPUTS       : 1) The fact-set variable list
                 2) The expression containing the variable
                 3) The address of the fact slot access function
                 4) Nesting depth of query functions
  RETURNS      : Nothing useful
  SIDE EFFECTS : If the variable is a slot reference, then it is replaced
                   with the appropriate function-call.
  NOTES        : None
 *************************************************************************/
static bool
ReplaceSlotReference (Environment * theEnv,
		      Expression * vlist,
		      Expression * theExp,
		      struct functionDefinition *func, int ndepth)
{
  size_t len;
  int posn;
  bool oldpp;
  size_t i;
  const char *str;
  Expression *eptr;
  struct token itkn;

  str = theExp->lexemeValue->contents;
  len = strlen (str);
  if (len < 3)
    {
      return false;
    }

  for (i = len - 2; i >= 1; i--)
    {
      if ((str[i] == FACT_SLOT_REF) ? (i >= 1) : false)
	{
	  eptr = vlist;
	  posn = 0;
	  while (eptr && ((i != strlen (eptr->lexemeValue->contents)) ||
			  strncmp (eptr->lexemeValue->contents, str,
				   (STD_SIZE) i)))
	    {
	      eptr = eptr->nextArg;
	      posn++;
	    }
	  if (eptr != NULL)
	    {
	      CL_OpenStringSource (theEnv, "query-var", str + i + 1, 0);
	      oldpp = CL_GetPPBufferStatus (theEnv);
	      CL_SetPPBufferStatus (theEnv, false);
	      CL_GetToken (theEnv, "query-var", &itkn);
	      CL_SetPPBufferStatus (theEnv, oldpp);
	      CL_CloseStringSource (theEnv, "query-var");

	      if (itkn.tknType != SYMBOL_TOKEN)
		{
		  CL_InvalidVarSlotErrorMessage (theEnv, str);
		  Set_EvaluationError (theEnv, true);
		  return true;
		}

	      theExp->type = FCALL;
	      theExp->value = func;
	      theExp->argList =
		CL_GenConstant (theEnv, INTEGER_TYPE,
				CL_CreateInteger (theEnv, ndepth));
	      theExp->argList->nextArg =
		CL_GenConstant (theEnv, INTEGER_TYPE,
				CL_CreateInteger (theEnv, posn));
	      theExp->argList->nextArg->nextArg =
		CL_GenConstant (theEnv, CL_TokenTypeToType (itkn.tknType),
				itkn.value);
	      theExp->argList->nextArg->nextArg->nextArg =
		CL_GenConstant (theEnv, SYMBOL_TYPE,
				CL_CreateSymbol (theEnv, str));
	      return false;
	    }
	}
    }

  return false;
}

/********************************************************************
  NAME         : IsQueryFunction
  DESCRIPTION  : Dete_rmines if an expression is a query function call
  INPUTS       : The expression
  RETURNS      : True if query function call, false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ********************************************************************/
static bool
IsQueryFunction (Expression * theExp)
{
  int (*fptr) (void);

  if (theExp->type != FCALL)
    return false;
  fptr = (int (*)(void)) ExpressionFunctionPointer (theExp);

  if (fptr == (int (*)(void)) CL_Any_Facts)
    return true;
  if (fptr == (int (*)(void)) CL_QueryFindFact)
    return true;
  if (fptr == (int (*)(void)) QueryFindAll_Facts)
    return true;
  if (fptr == (int (*)(void)) CL_QueryDoForFact)
    return true;
  if (fptr == (int (*)(void)) QueryDoForAll_Facts)
    return true;
  if (fptr == (int (*)(void)) CL_DelayedQueryDoForAll_Facts)
    return true;

  return false;
}

#endif
