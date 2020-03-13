   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  09/16/17             */
   /*                                                     */
   /*          PROCEDURAL FUNCTIONS PARSER MODULE         */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable exp to theExp         */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Local variables set with the bind function     */
/*            persist until a reset/clear command is issued. */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Fixed linkage issue when BLOAD_ONLY compiler   */
/*            flag is set to 1.                              */
/*                                                           */
/*      6.31: Fixed 'while' function bug with optional use   */
/*            of 'do' keyword.                               */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*************************************************************/

#include <stdio.h>

#include "setup.h"

#include "argacces.h"
#include "constrnt.h"
#include "cstrnchk.h"
#include "cstrnops.h"
#include "cstrnutl.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "modulutl.h"
#include "multifld.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"
#include "utility.h"

#include "prcdrpsr.h"

#if DEFGLOBAL_CONSTRUCT
#include "globldef.h"
#include "globlpsr.h"
#endif

#define PRCDRPSR_DATA 12

struct procedureParserData
{
  struct BindInfo *ListOfParsedBindNames;
};

#define ProcedureParserData(theEnv) ((struct procedureParserData *) GetEnvironmentData(theEnv,PRCDRPSR_DATA))

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void DeallocateProceduralFunctionData (Environment *);
static struct expr *WhileParse (Environment *, struct expr *, const char *);
static struct expr *LoopForCountParse (Environment *, struct expr *,
				       const char *);
static void ReplaceLoopCountVars (Environment *, CLIPSLexeme *, Expression *,
				  int);
static struct expr *IfParse (Environment *, struct expr *, const char *);
static struct expr *PrognParse (Environment *, struct expr *, const char *);
static struct expr *BindParse (Environment *, struct expr *, const char *);
static int AddBindName (Environment *, CLIPSLexeme *, CONSTRAINT_RECORD *);
static struct expr *ReturnParse (Environment *, struct expr *, const char *);
static struct expr *BreakParse (Environment *, struct expr *, const char *);
static struct expr *SwitchParse (Environment *, struct expr *, const char *);

/*****************************/
/* CL_ProceduralFunctionParsers */
/*****************************/
void
CL_ProceduralFunctionParsers (Environment * theEnv)
{
  CL_AllocateEnvironmentData (theEnv, PRCDRPSR_DATA,
			      sizeof (struct procedureParserData),
			      DeallocateProceduralFunctionData);

  CL_AddFunctionParser (theEnv, "bind", BindParse);
  CL_AddFunctionParser (theEnv, "progn", PrognParse);
  CL_AddFunctionParser (theEnv, "if", IfParse);
  CL_AddFunctionParser (theEnv, "while", WhileParse);
  CL_AddFunctionParser (theEnv, "loop-for-count", LoopForCountParse);
  CL_AddFunctionParser (theEnv, "return", ReturnParse);
  CL_AddFunctionParser (theEnv, "break", BreakParse);
  CL_AddFunctionParser (theEnv, "switch", SwitchParse);
}

/*************************************************************/
/* DeallocateProceduralFunctionData: Deallocates environment */
/*    data for procedural functions.                         */
/*************************************************************/
static void
DeallocateProceduralFunctionData (Environment * theEnv)
{
  struct BindInfo *temp_bind;

  while (ProcedureParserData (theEnv)->ListOfParsedBindNames != NULL)
    {
      temp_bind = ProcedureParserData (theEnv)->ListOfParsedBindNames->next;
      rtn_struct (theEnv, BindInfo,
		  ProcedureParserData (theEnv)->ListOfParsedBindNames);
      ProcedureParserData (theEnv)->ListOfParsedBindNames = temp_bind;
    }
}

/***********************/
/* CL_GetParsedBindNames: */
/***********************/
struct BindInfo *
CL_GetParsedBindNames (Environment * theEnv)
{
  return (ProcedureParserData (theEnv)->ListOfParsedBindNames);
}

/***********************/
/* CL_SetParsedBindNames: */
/***********************/
void
CL_SetParsedBindNames (Environment * theEnv, struct BindInfo *newValue)
{
  ProcedureParserData (theEnv)->ListOfParsedBindNames = newValue;
}

/*************************/
/* CL_ClearParsedBindNames: */
/*************************/
void
CL_ClearParsedBindNames (Environment * theEnv)
{
  struct BindInfo *temp_bind;

  while (ProcedureParserData (theEnv)->ListOfParsedBindNames != NULL)
    {
      temp_bind = ProcedureParserData (theEnv)->ListOfParsedBindNames->next;
      CL_RemoveConstraint (theEnv,
			   ProcedureParserData
			   (theEnv)->ListOfParsedBindNames->constraints);
      rtn_struct (theEnv, BindInfo,
		  ProcedureParserData (theEnv)->ListOfParsedBindNames);
      ProcedureParserData (theEnv)->ListOfParsedBindNames = temp_bind;
    }
}

/*************************/
/* CL_ParsedBindNamesEmpty: */
/*************************/
bool
CL_ParsedBindNamesEmpty (Environment * theEnv)
{
  if (ProcedureParserData (theEnv)->ListOfParsedBindNames != NULL)
    return false;

  return true;
}

/*********************************************************/
/* WhileParse: purpose is to parse the while statement.  */
/*   The parse of the statement is the return value.     */
/*   Syntax: (while <expression> do <action>+)           */
/*********************************************************/
static struct expr *
WhileParse (Environment * theEnv, struct expr *parse, const char *infile)
{
  struct token theToken;
  bool read_first_token;

   /*===============================*/
  /* Process the while expression. */
   /*===============================*/

  CL_SavePPBuffer (theEnv, " ");

  parse->argList = CL_ParseAtomOrExpression (theEnv, infile, NULL);
  if (parse->argList == NULL)
    {
      CL_ReturnExpression (theEnv, parse);
      return NULL;
    }

   /*====================================*/
  /* Process the do keyword if present. */
   /*====================================*/

  CL_GetToken (theEnv, infile, &theToken);
  if ((theToken.tknType == SYMBOL_TOKEN)
      && (strcmp (theToken.lexemeValue->contents, "do") == 0))
    {
      read_first_token = true;
      CL_PPBackup (theEnv);
      CL_SavePPBuffer (theEnv, " ");
      CL_SavePPBuffer (theEnv, theToken.printFo_rm);
      CL_IncrementIndentDepth (theEnv, 3);
      CL_PPCRAndIndent (theEnv);
    }
  else
    {
      read_first_token = false;
      CL_PPBackup (theEnv);
      CL_IncrementIndentDepth (theEnv, 3);
      CL_PPCRAndIndent (theEnv);
      CL_SavePPBuffer (theEnv, theToken.printFo_rm);
    }

   /*============================*/
  /* Process the while actions. */
   /*============================*/

  if (ExpressionData (theEnv)->svContexts->rtn == true)
    {
      ExpressionData (theEnv)->ReturnContext = true;
    }
  ExpressionData (theEnv)->BreakContext = true;

  parse->argList->nextArg =
    CL_GroupActions (theEnv, infile, &theToken, read_first_token, NULL,
		     false);

  if (parse->argList->nextArg == NULL)
    {
      CL_ReturnExpression (theEnv, parse);
      return NULL;
    }

  CL_PPBackup (theEnv);
  CL_PPBackup (theEnv);
  CL_SavePPBuffer (theEnv, theToken.printFo_rm);

   /*=======================================================*/
  /* Check for the closing right parenthesis of the while. */
   /*=======================================================*/

  if (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "while function");
      CL_ReturnExpression (theEnv, parse);
      return NULL;
    }

  CL_DecrementIndentDepth (theEnv, 3);

  return parse;
}

/******************************************************************************************/
/* LoopForCountParse: purpose is to parse the loop-for-count statement.                   */
/*   The parse of the statement is the return value.                                      */
/*   Syntax: (loop-for-count <range> [do] <action>+)                                      */
/*           <range> ::= (<sf-var> [<start-integer-expression>] <end-integer-expression>) */
/******************************************************************************************/
static struct expr *
LoopForCountParse (Environment * theEnv,
		   struct expr *parse, const char *infile)
{
  struct token theToken;
  CLIPSLexeme *loopVar = NULL;
  Expression *tmpexp;
  bool read_first_paren;
  struct BindInfo *oldBindList, *newBindList, *prev;

   /*======================================*/
  /* Process the loop counter expression. */
   /*======================================*/

  CL_SavePPBuffer (theEnv, " ");
  CL_GetToken (theEnv, infile, &theToken);

  /* ==========================================
     Simple fo_rm: loop-for-count <end> [do] ...
     ========================================== */
  if (theToken.tknType != LEFT_PARENTHESIS_TOKEN)
    {
      parse->argList =
	CL_GenConstant (theEnv, CL_INTEGER_TYPE,
			CL_CreateInteger (theEnv, 1LL));
      parse->argList->nextArg =
	CL_ParseAtomOrExpression (theEnv, infile, &theToken);
      if (parse->argList->nextArg == NULL)
	{
	  CL_ReturnExpression (theEnv, parse);
	  return NULL;
	}
    }
  else
    {
      CL_GetToken (theEnv, infile, &theToken);
      if (theToken.tknType != SF_VARIABLE_TOKEN)
	{
	  if (theToken.tknType != SYMBOL_TOKEN)
	    goto LoopForCountParseError;
	  parse->argList =
	    CL_GenConstant (theEnv, CL_INTEGER_TYPE,
			    CL_CreateInteger (theEnv, 1LL));
	  parse->argList->nextArg =
	    CL_Function2Parse (theEnv, infile,
			       theToken.lexemeValue->contents);
	  if (parse->argList->nextArg == NULL)
	    {
	      CL_ReturnExpression (theEnv, parse);
	      return NULL;
	    }
	}

      /* =============================================================
         Complex fo_rm: loop-for-count (<var> [<start>] <end>) [do] ...
         ============================================================= */
      else
	{
	  loopVar = theToken.lexemeValue;
	  CL_SavePPBuffer (theEnv, " ");
	  parse->argList = CL_ParseAtomOrExpression (theEnv, infile, NULL);
	  if (parse->argList == NULL)
	    {
	      CL_ReturnExpression (theEnv, parse);
	      return NULL;
	    }

	  if (CL_CheckArgumentAgainstRestriction
	      (theEnv, parse->argList, INTEGER_BIT))
	    goto LoopForCountParseError;

	  CL_SavePPBuffer (theEnv, " ");
	  CL_GetToken (theEnv, infile, &theToken);
	  if (theToken.tknType == RIGHT_PARENTHESIS_TOKEN)
	    {
	      CL_PPBackup (theEnv);
	      CL_PPBackup (theEnv);
	      CL_SavePPBuffer (theEnv, theToken.printFo_rm);
	      tmpexp =
		CL_GenConstant (theEnv, CL_INTEGER_TYPE,
				CL_CreateInteger (theEnv, 1LL));
	      tmpexp->nextArg = parse->argList;
	      parse->argList = tmpexp;
	    }
	  else
	    {
	      parse->argList->nextArg =
		CL_ParseAtomOrExpression (theEnv, infile, &theToken);
	      if (parse->argList->nextArg == NULL)
		{
		  CL_ReturnExpression (theEnv, parse);
		  return NULL;
		}
	      CL_GetToken (theEnv, infile, &theToken);
	      if (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
		goto LoopForCountParseError;
	    }
	  CL_SavePPBuffer (theEnv, " ");
	}
    }

  if (CL_CheckArgumentAgainstRestriction
      (theEnv, parse->argList->nextArg, INTEGER_BIT))
    goto LoopForCountParseError;

   /*====================================*/
  /* Process the do keyword if present. */
   /*====================================*/

  CL_GetToken (theEnv, infile, &theToken);
  if ((theToken.tknType == SYMBOL_TOKEN)
      && (strcmp (theToken.lexemeValue->contents, "do") == 0))
    {
      read_first_paren = true;
      CL_PPBackup (theEnv);
      CL_SavePPBuffer (theEnv, " ");
      CL_SavePPBuffer (theEnv, theToken.printFo_rm);
      CL_IncrementIndentDepth (theEnv, 3);
      CL_PPCRAndIndent (theEnv);
    }
  else if (theToken.tknType == LEFT_PARENTHESIS_TOKEN)
    {
      read_first_paren = false;
      CL_PPBackup (theEnv);
      CL_IncrementIndentDepth (theEnv, 3);
      CL_PPCRAndIndent (theEnv);
      CL_SavePPBuffer (theEnv, theToken.printFo_rm);
    }
  else
    goto LoopForCountParseError;

   /*=====================================*/
  /* Process the loop-for-count actions. */
   /*=====================================*/
  if (ExpressionData (theEnv)->svContexts->rtn == true)
    ExpressionData (theEnv)->ReturnContext = true;
  ExpressionData (theEnv)->BreakContext = true;
  oldBindList = CL_GetParsedBindNames (theEnv);
  CL_SetParsedBindNames (theEnv, NULL);
  parse->argList->nextArg->nextArg =
    CL_GroupActions (theEnv, infile, &theToken, read_first_paren, NULL,
		     false);

  if (parse->argList->nextArg->nextArg == NULL)
    {
      CL_SetParsedBindNames (theEnv, oldBindList);
      CL_ReturnExpression (theEnv, parse);
      return NULL;
    }
  newBindList = CL_GetParsedBindNames (theEnv);
  prev = NULL;
  while (newBindList != NULL)
    {
      if ((loopVar == NULL) ? false :
	  (strcmp (newBindList->name->contents, loopVar->contents) == 0))
	{
	  CL_ClearParsedBindNames (theEnv);
	  CL_SetParsedBindNames (theEnv, oldBindList);
	  CL_PrintErrorID (theEnv, "PRCDRPSR", 1, true);
	  CL_WriteString (theEnv, STDERR,
			  "Cannot rebind loop variable in function loop-for-count.\n");
	  CL_ReturnExpression (theEnv, parse);
	  return NULL;
	}
      prev = newBindList;
      newBindList = newBindList->next;
    }
  if (prev == NULL)
    CL_SetParsedBindNames (theEnv, oldBindList);
  else
    prev->next = oldBindList;
  if (loopVar != NULL)
    ReplaceLoopCountVars (theEnv, loopVar, parse->argList->nextArg->nextArg,
			  0);
  CL_PPBackup (theEnv);
  CL_PPBackup (theEnv);
  CL_SavePPBuffer (theEnv, theToken.printFo_rm);

   /*================================================================*/
  /* Check for the closing right parenthesis of the loop-for-count. */
   /*================================================================*/

  if (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "loop-for-count function");
      CL_ReturnExpression (theEnv, parse);
      return NULL;
    }

  CL_DecrementIndentDepth (theEnv, 3);

  return (parse);

LoopForCountParseError:
  CL_SyntaxErrorMessage (theEnv, "loop-for-count function");
  CL_ReturnExpression (theEnv, parse);
  return NULL;
}

/*************************/
/* ReplaceLoopCountVars: */
/*************************/
static void
ReplaceLoopCountVars (Environment * theEnv,
		      CLIPSLexeme * loopVar, Expression * theExp, int depth)
{
  while (theExp != NULL)
    {
      if ((theExp->type != SF_VARIABLE) ? false :
	  (strcmp (theExp->lexemeValue->contents, loopVar->contents) == 0))
	{
	  theExp->type = FCALL;
	  theExp->value = CL_FindFunction (theEnv, "(get-loop-count)");
	  theExp->argList =
	    CL_GenConstant (theEnv, CL_INTEGER_TYPE,
			    CL_CreateInteger (theEnv, depth));
	}
      else if (theExp->argList != NULL)
	{
	  if ((theExp->type != FCALL) ? false :
	      (theExp->value ==
	       (void *) CL_FindFunction (theEnv, "loop-for-count")))
	    ReplaceLoopCountVars (theEnv, loopVar, theExp->argList,
				  depth + 1);
	  else
	    ReplaceLoopCountVars (theEnv, loopVar, theExp->argList, depth);
	}
      theExp = theExp->nextArg;
    }
}

/*********************************************************/
/* IfParse: purpose is to parse the if statement.  The  */
/*   parse of the statement is the return value.         */
/*   Syntax: (if <expression> then <action>+             */
/*               [ else <action>+ ] )                    */
/*********************************************************/
static struct expr *
IfParse (Environment * theEnv, struct expr *top, const char *infile)
{
  struct token theToken;

   /*============================*/
  /* Process the if expression. */
   /*============================*/

  CL_SavePPBuffer (theEnv, " ");

  top->argList = CL_ParseAtomOrExpression (theEnv, infile, NULL);

  if (top->argList == NULL)
    {
      CL_ReturnExpression (theEnv, top);
      return NULL;
    }

   /*========================================*/
  /* Keyword 'then' must follow expression. */
   /*========================================*/

  CL_IncrementIndentDepth (theEnv, 3);
  CL_PPCRAndIndent (theEnv);

  CL_GetToken (theEnv, infile, &theToken);
  if ((theToken.tknType != SYMBOL_TOKEN)
      || (strcmp (theToken.lexemeValue->contents, "then") != 0))
    {
      CL_SyntaxErrorMessage (theEnv, "if function");
      CL_ReturnExpression (theEnv, top);
      return NULL;
    }

   /*==============================*/
  /* Process the if then actions. */
   /*==============================*/

  CL_PPCRAndIndent (theEnv);
  if (ExpressionData (theEnv)->svContexts->rtn == true)
    ExpressionData (theEnv)->ReturnContext = true;
  if (ExpressionData (theEnv)->svContexts->brk == true)
    ExpressionData (theEnv)->BreakContext = true;
  top->argList->nextArg =
    CL_GroupActions (theEnv, infile, &theToken, true, "else", false);

  if (top->argList->nextArg == NULL)
    {
      CL_ReturnExpression (theEnv, top);
      return NULL;
    }

  top->argList->nextArg =
    CL_RemoveUnneededProgn (theEnv, top->argList->nextArg);

   /*===========================================*/
  /* A ')' signals an if then without an else. */
   /*===========================================*/

  if (theToken.tknType == RIGHT_PARENTHESIS_TOKEN)
    {
      CL_DecrementIndentDepth (theEnv, 3);
      CL_PPBackup (theEnv);
      CL_PPBackup (theEnv);
      CL_SavePPBuffer (theEnv, theToken.printFo_rm);
      return (top);
    }

   /*=============================================*/
  /* Keyword 'else' must follow if then actions. */
   /*=============================================*/

  if ((theToken.tknType != SYMBOL_TOKEN)
      || (strcmp (theToken.lexemeValue->contents, "else") != 0))
    {
      CL_SyntaxErrorMessage (theEnv, "if function");
      CL_ReturnExpression (theEnv, top);
      return NULL;
    }

   /*==============================*/
  /* Process the if else actions. */
   /*==============================*/

  CL_PPCRAndIndent (theEnv);
  top->argList->nextArg->nextArg =
    CL_GroupActions (theEnv, infile, &theToken, true, NULL, false);

  if (top->argList->nextArg->nextArg == NULL)
    {
      CL_ReturnExpression (theEnv, top);
      return NULL;
    }

  top->argList->nextArg->nextArg =
    CL_RemoveUnneededProgn (theEnv, top->argList->nextArg->nextArg);

   /*======================================================*/
  /* Check for the closing right parenthesis of the if. */
   /*======================================================*/

  if (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "if function");
      CL_ReturnExpression (theEnv, top);
      return NULL;
    }

   /*===========================================*/
  /* A ')' signals an if then without an else. */
   /*===========================================*/

  CL_PPBackup (theEnv);
  CL_PPBackup (theEnv);
  CL_SavePPBuffer (theEnv, ")");
  CL_DecrementIndentDepth (theEnv, 3);
  return (top);
}

/********************************************************/
/* PrognParse: purpose is to parse the progn statement. */
/*   The parse of the statement is the return value.    */
/*   Syntax:  (progn <expression>*)                     */
/********************************************************/
static struct expr *
PrognParse (Environment * theEnv, struct expr *top, const char *infile)
{
  struct token tkn;
  struct expr *tmp;

  CL_ReturnExpression (theEnv, top);
  ExpressionData (theEnv)->BreakContext =
    ExpressionData (theEnv)->svContexts->brk;
  ExpressionData (theEnv)->ReturnContext =
    ExpressionData (theEnv)->svContexts->rtn;
  CL_IncrementIndentDepth (theEnv, 3);
  CL_PPCRAndIndent (theEnv);
  tmp = CL_GroupActions (theEnv, infile, &tkn, true, NULL, false);
  CL_DecrementIndentDepth (theEnv, 3);
  CL_PPBackup (theEnv);
  CL_PPBackup (theEnv);
  CL_SavePPBuffer (theEnv, tkn.printFo_rm);
  return (tmp);
}

/***********************************************************/
/* BindParse: purpose is to parse the bind statement. The */
/*   parse of the statement is the return value.           */
/*   Syntax:  (bind ?var <expression>)                     */
/***********************************************************/
static struct expr *
BindParse (Environment * theEnv, struct expr *top, const char *infile)
{
  struct token theToken;
  CLIPSLexeme *variableName;
  struct expr *texp;
  CONSTRAINT_RECORD *theConstraint = NULL;
#if DEFGLOBAL_CONSTRUCT
  Defglobal *theGlobal;
  unsigned int count;
#endif

  CL_SavePPBuffer (theEnv, " ");

   /*=============================================*/
  /* Next token must be the name of the variable */
  /* to be bound.                                */
   /*=============================================*/

  CL_GetToken (theEnv, infile, &theToken);
  if ((theToken.tknType != SF_VARIABLE_TOKEN) &&
      (theToken.tknType != GBL_VARIABLE_TOKEN))
    {
      if ((theToken.tknType != MF_VARIABLE_TOKEN)
	  || ExpressionData (theEnv)->SequenceOpMode)
	{
	  CL_SyntaxErrorMessage (theEnv, "bind function");
	  CL_ReturnExpression (theEnv, top);
	  return NULL;
	}
    }

   /*==============================*/
  /* Process the bind expression. */
   /*==============================*/

  top->argList = CL_GenConstant (theEnv, SYMBOL_TYPE, theToken.value);
  variableName = theToken.lexemeValue;

#if DEFGLOBAL_CONSTRUCT
  if ((theToken.tknType == GBL_VARIABLE_TOKEN) ?
      ((theGlobal = (Defglobal *)
	CL_FindImportedConstruct (theEnv, "defglobal", NULL,
				  variableName->contents, &count, true,
				  NULL)) != NULL) : false)
    {
      top->argList->type = DEFGLOBAL_PTR;
      top->argList->value = theGlobal;
    }
  else if (theToken.tknType == GBL_VARIABLE_TOKEN)
    {
      CL_GlobalReferenceErrorMessage (theEnv, variableName->contents);
      CL_ReturnExpression (theEnv, top);
      return NULL;
    }
#endif

  texp = get_struct (theEnv, expr);
  texp->argList = texp->nextArg = NULL;
  if (CL_CollectArguments (theEnv, texp, infile) == NULL)
    {
      CL_ReturnExpression (theEnv, top);
      return NULL;
    }

  top->argList->nextArg = texp->argList;
  rtn_struct (theEnv, expr, texp);

#if DEFGLOBAL_CONSTRUCT
  if (top->argList->type == DEFGLOBAL_PTR)
    return (top);
#endif

  if (top->argList->nextArg != NULL)
    {
      theConstraint =
	CL_ExpressionToConstraintRecord (theEnv, top->argList->nextArg);
    }

  AddBindName (theEnv, variableName, theConstraint);

  return (top);
}

/********************************************/
/* ReturnParse: Parses the return function. */
/********************************************/
static struct expr *
ReturnParse (Environment * theEnv, struct expr *top, const char *infile)
{
  bool error_flag = false;
  struct token theToken;

  if (ExpressionData (theEnv)->svContexts->rtn == true)
    ExpressionData (theEnv)->ReturnContext = true;
  if (ExpressionData (theEnv)->ReturnContext == false)
    {
      CL_PrintErrorID (theEnv, "PRCDRPSR", 2, true);
      CL_WriteString (theEnv, STDERR,
		      "The return function is not valid in this context.\n");
      CL_ReturnExpression (theEnv, top);
      return NULL;
    }
  ExpressionData (theEnv)->ReturnContext = false;

  CL_SavePPBuffer (theEnv, " ");

  top->argList = CL_ArgumentParse (theEnv, infile, &error_flag);
  if (error_flag)
    {
      CL_ReturnExpression (theEnv, top);
      return NULL;
    }
  else if (top->argList == NULL)
    {
      CL_PPBackup (theEnv);
      CL_PPBackup (theEnv);
      CL_SavePPBuffer (theEnv, ")");
    }
  else
    {
      CL_SavePPBuffer (theEnv, " ");
      CL_GetToken (theEnv, infile, &theToken);
      if (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
	{
	  CL_SyntaxErrorMessage (theEnv, "return function");
	  CL_ReturnExpression (theEnv, top);
	  return NULL;
	}
      CL_PPBackup (theEnv);
      CL_PPBackup (theEnv);
      CL_SavePPBuffer (theEnv, ")");
    }
  return (top);
}

/***************/
/* BreakParse: */
/***************/
static struct expr *
BreakParse (Environment * theEnv, struct expr *top, const char *infile)
{
  struct token theToken;

  if (ExpressionData (theEnv)->svContexts->brk == false)
    {
      CL_PrintErrorID (theEnv, "PRCDRPSR", 2, true);
      CL_WriteString (theEnv, STDERR,
		      "The break function not valid in this context.\n");
      CL_ReturnExpression (theEnv, top);
      return NULL;
    }

  CL_SavePPBuffer (theEnv, " ");
  CL_GetToken (theEnv, infile, &theToken);
  if (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "break function");
      CL_ReturnExpression (theEnv, top);
      return NULL;
    }
  CL_PPBackup (theEnv);
  CL_PPBackup (theEnv);
  CL_SavePPBuffer (theEnv, ")");
  return (top);
}

/****************/
/* SwitchParse: */
/****************/
static struct expr *
SwitchParse (Environment * theEnv, struct expr *top, const char *infile)
{
  struct token theToken;
  Expression *theExp, *chk;
  int default_count = 0;

   /*============================*/
  /* Process the switch value   */
   /*============================*/
  CL_IncrementIndentDepth (theEnv, 3);
  CL_SavePPBuffer (theEnv, " ");
  top->argList = theExp = CL_ParseAtomOrExpression (theEnv, infile, NULL);
  if (theExp == NULL)
    goto SwitchParseError;

   /*========================*/
  /* Parse case statements. */
   /*========================*/
  CL_GetToken (theEnv, infile, &theToken);
  while (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      CL_PPBackup (theEnv);
      CL_PPCRAndIndent (theEnv);
      CL_SavePPBuffer (theEnv, theToken.printFo_rm);
      if (theToken.tknType != LEFT_PARENTHESIS_TOKEN)
	goto SwitchParseErrorAndMessage;
      CL_GetToken (theEnv, infile, &theToken);
      CL_SavePPBuffer (theEnv, " ");
      if ((theToken.tknType == SYMBOL_TOKEN) &&
	  (strcmp (theToken.lexemeValue->contents, "case") == 0))
	{
	  if (default_count != 0)
	    goto SwitchParseErrorAndMessage;
	  theExp->nextArg = CL_ParseAtomOrExpression (theEnv, infile, NULL);
	  CL_SavePPBuffer (theEnv, " ");
	  if (theExp->nextArg == NULL)
	    goto SwitchParseError;
	  for (chk = top->argList->nextArg; chk != theExp->nextArg;
	       chk = chk->nextArg)
	    {
	      if ((chk->type == theExp->nextArg->type) &&
		  (chk->value == theExp->nextArg->value) &&
		  CL_IdenticalExpression (chk->argList,
					  theExp->nextArg->argList))
		{
		  CL_PrintErrorID (theEnv, "PRCDRPSR", 3, true);
		  CL_WriteString (theEnv, STDERR,
				  "Duplicate case found in switch function.\n");
		  goto SwitchParseError;
		}
	    }
	  CL_GetToken (theEnv, infile, &theToken);
	  if ((theToken.tknType != SYMBOL_TOKEN) ? true :
	      (strcmp (theToken.lexemeValue->contents, "then") != 0))
	    goto SwitchParseErrorAndMessage;
	}
      else if ((theToken.tknType == SYMBOL_TOKEN) &&
	       (strcmp (theToken.lexemeValue->contents, "default") == 0))
	{
	  if (default_count)
	    goto SwitchParseErrorAndMessage;
	  theExp->nextArg = CL_GenConstant (theEnv, CL_VOID_TYPE, NULL);
	  default_count = 1;
	}
      else
	goto SwitchParseErrorAndMessage;
      theExp = theExp->nextArg;
      if (ExpressionData (theEnv)->svContexts->rtn == true)
	ExpressionData (theEnv)->ReturnContext = true;
      if (ExpressionData (theEnv)->svContexts->brk == true)
	ExpressionData (theEnv)->BreakContext = true;
      CL_IncrementIndentDepth (theEnv, 3);
      CL_PPCRAndIndent (theEnv);
      theExp->nextArg =
	CL_GroupActions (theEnv, infile, &theToken, true, NULL, false);
      CL_DecrementIndentDepth (theEnv, 3);
      ExpressionData (theEnv)->ReturnContext = false;
      ExpressionData (theEnv)->BreakContext = false;
      if (theExp->nextArg == NULL)
	goto SwitchParseError;
      theExp = theExp->nextArg;
      CL_PPBackup (theEnv);
      CL_PPBackup (theEnv);
      CL_SavePPBuffer (theEnv, theToken.printFo_rm);
      CL_GetToken (theEnv, infile, &theToken);
    }
  CL_DecrementIndentDepth (theEnv, 3);
  return (top);

SwitchParseErrorAndMessage:
  CL_SyntaxErrorMessage (theEnv, "switch function");
SwitchParseError:
  CL_ReturnExpression (theEnv, top);
  CL_DecrementIndentDepth (theEnv, 3);
  return NULL;
}

/**************************/
/* CL_SearchParsedBindNames: */
/**************************/
unsigned short
CL_SearchParsedBindNames (Environment * theEnv, CLIPSLexeme * name_sought)
{
  struct BindInfo *var_ptr;
  unsigned short theIndex = 1;

  var_ptr = ProcedureParserData (theEnv)->ListOfParsedBindNames;
  while (var_ptr != NULL)
    {
      if (var_ptr->name == name_sought)
	{
	  return theIndex;
	}
      var_ptr = var_ptr->next;
      theIndex++;
    }

  return 0;
}

/************************/
/* CL_FindBindConstraints: */
/************************/
struct constraintRecord *
CL_FindBindConstraints (Environment * theEnv, CLIPSLexeme * nameSought)
{
  struct BindInfo *theVariable;

  theVariable = ProcedureParserData (theEnv)->ListOfParsedBindNames;
  while (theVariable != NULL)
    {
      if (theVariable->name == nameSought)
	{
	  return (theVariable->constraints);
	}
      theVariable = theVariable->next;
    }

  return NULL;
}

/********************************************************/
/* CL_CountParsedBindNames: Counts the number of variables */
/*   names that have been bound using the bind function */
/*   in the current context (e.g. the RHS of a rule).   */
/********************************************************/
unsigned short
CL_CountParsedBindNames (Environment * theEnv)
{
  struct BindInfo *theVariable;
  unsigned short theIndex = 0;

  theVariable = ProcedureParserData (theEnv)->ListOfParsedBindNames;
  while (theVariable != NULL)
    {
      theVariable = theVariable->next;
      theIndex++;
    }

  return theIndex;
}

/****************************************************************/
/* AddBindName: Adds a variable name used as the first argument */
/*   of the bind function to the list of variable names parsed  */
/*   within the current semantic context (e.g. RHS of a rule).  */
/****************************************************************/
static int
AddBindName (Environment * theEnv,
	     CLIPSLexeme * variableName, CONSTRAINT_RECORD * theConstraint)
{
  CONSTRAINT_RECORD *tmpConstraint;
  struct BindInfo *currentBind, *lastBind;
  int theIndex = 1;

   /*=========================================================*/
  /* Look for the variable name in the list of bind variable */
  /* names already parsed. If it is found, then return the   */
  /* index to the variable and union the new constraint      */
  /* info_rmation with the old constraint info_rmation.        */
   /*=========================================================*/

  lastBind = NULL;
  currentBind = ProcedureParserData (theEnv)->ListOfParsedBindNames;
  while (currentBind != NULL)
    {
      if (currentBind->name == variableName)
	{
	  if (theConstraint != NULL)
	    {
	      tmpConstraint = currentBind->constraints;
	      currentBind->constraints =
		CL_UnionConstraints (theEnv, theConstraint,
				     currentBind->constraints);
	      CL_RemoveConstraint (theEnv, tmpConstraint);
	      CL_RemoveConstraint (theEnv, theConstraint);
	    }

	  return (theIndex);
	}
      lastBind = currentBind;
      currentBind = currentBind->next;
      theIndex++;
    }

   /*===============================================================*/
  /* If the variable name wasn't found, then add it to the list of */
  /* variable names and store the constraint info_rmation with it.  */
   /*===============================================================*/

  currentBind = get_struct (theEnv, BindInfo);
  currentBind->name = variableName;
  currentBind->constraints = theConstraint;
  currentBind->next = NULL;

  if (lastBind == NULL)
    ProcedureParserData (theEnv)->ListOfParsedBindNames = currentBind;
  else
    lastBind->next = currentBind;

  return (theIndex);
}

/*************************/
/* CL_RemoveParsedBindName: */
/*************************/
void
CL_RemoveParsedBindName (Environment * theEnv, CLIPSLexeme * bname)
{
  struct BindInfo *prv, *tmp;

  prv = NULL;
  tmp = ProcedureParserData (theEnv)->ListOfParsedBindNames;
  while ((tmp != NULL) ? (tmp->name != bname) : false)
    {
      prv = tmp;
      tmp = tmp->next;
    }
  if (tmp != NULL)
    {
      if (prv == NULL)
	ProcedureParserData (theEnv)->ListOfParsedBindNames = tmp->next;
      else
	prv->next = tmp->next;

      CL_RemoveConstraint (theEnv, tmp->constraints);
      rtn_struct (theEnv, BindInfo, tmp);
    }
}
