   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  09/28/17             */
   /*                                                     */
   /*                  FACT QUERY MODULE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose: Query Functions for Objects                      */
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
/*      6.24: Corrected errors when compiling as a C++ file. */
/*            DR0868                                         */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Changed garbage collection algorithm.          */
/*                                                           */
/*            Fixes for run-time use of query functions.     */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.31: Retrieval for fact query slot function         */
/*            generates an error if the fact has been        */
/*            retracted.                                     */
/*                                                           */
/*            Functions delayed-do-for-all-facts and         */
/*            do-for-fact increment the busy count of        */
/*            matching fact sets so that actions can         */
/*            detect retracted facts.                        */
/*                                                           */
/*            Matching fact sets containing retracted facts  */
/*            are pruned.                                    */
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
/*            Added CL_GCBlockStart and CL_GCBlockEnd functions    */
/*            for garbage collection blocks.                 */
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

#include "argacces.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "exprnpsr.h"
#include "modulutl.h"
#include "tmpltutl.h"
#include "insfun.h"
#include "factqpsr.h"
#include "prcdrfun.h"
#include "prntutil.h"
#include "router.h"
#include "utility.h"

#include "factqury.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void PushQueryCore (Environment *);
static void PopQueryCore (Environment *);
static QUERY_CORE *FindQueryCore (Environment *, long long);
static QUERY_TEMPLATE *Dete_rmineQueryTemplates (Environment *, Expression *,
						 const char *, unsigned *);
static QUERY_TEMPLATE *Fo_rmChain (Environment *, const char *, Deftemplate *,
				   UDFValue *);
static void DeleteQueryTemplates (Environment *, QUERY_TEMPLATE *);
static bool TestForFirstInChain (Environment *, QUERY_TEMPLATE *, unsigned);
static bool TestForFirstFactInTemplate (Environment *, Deftemplate *,
					QUERY_TEMPLATE *, unsigned);
static void TestEntireChain (Environment *, QUERY_TEMPLATE *, unsigned);
static void TestEntireTemplate (Environment *, Deftemplate *,
				QUERY_TEMPLATE *, unsigned);
static void AddSolution (Environment *);
static void PopQuerySoln (Environment *);

/****************************************************
  NAME         : CL_SetupFactQuery
  DESCRIPTION  : Initializes fact query H/L
                   functions and parsers
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Sets up kernel functions and parsers
  NOTES        : None
 ****************************************************/
void
CL_SetupFactQuery (Environment * theEnv)
{
  CL_AllocateEnvironmentData (theEnv, FACT_QUERY_DATA,
			      sizeof (struct factQueryData), NULL);

#if RUN_TIME
  FactQueryData (theEnv)->QUERY_DELIMITER_SYMBOL =
    CL_FindSymbolHN (theEnv, QUERY_DELIMITER_STRING, SYMBOL_BIT);
#endif

#if ! RUN_TIME
  FactQueryData (theEnv)->QUERY_DELIMITER_SYMBOL =
    CL_CreateSymbol (theEnv, QUERY_DELIMITER_STRING);
  IncrementLexemeCount (FactQueryData (theEnv)->QUERY_DELIMITER_SYMBOL);

  CL_AddUDF (theEnv, "(query-fact)", "f", 0, UNBOUNDED, NULL, CL_GetQueryFact,
	     "CL_GetQueryFact", NULL);

  CL_AddUDF (theEnv, "(query-fact-slot)", "*", 0, UNBOUNDED, NULL,
	     CL_GetQueryFactSlot, "CL_GetQueryFactSlot", NULL);

  CL_AddUDF (theEnv, "any-factp", "b", 0, UNBOUNDED, NULL, CL_Any_Facts,
	     "CL_Any_Facts", NULL);

  CL_AddUDF (theEnv, "find-fact", "m", 0, UNBOUNDED, NULL, CL_QueryFindFact,
	     "CL_QueryFindFact", NULL);

  CL_AddUDF (theEnv, "find-all-facts", "m", 0, UNBOUNDED, NULL,
	     QueryFindAll_Facts, "QueryFindAll_Facts", NULL);

  CL_AddUDF (theEnv, "do-for-fact", "*", 0, UNBOUNDED, NULL,
	     CL_QueryDoForFact, "CL_QueryDoForFact", NULL);

  CL_AddUDF (theEnv, "do-for-all-facts", "*", 0, UNBOUNDED, NULL,
	     QueryDoForAll_Facts, "QueryDoForAll_Facts", NULL);

  CL_AddUDF (theEnv, "delayed-do-for-all-facts", "*", 0, UNBOUNDED, NULL,
	     CL_DelayedQueryDoForAll_Facts, "CL_DelayedQueryDoForAll_Facts",
	     NULL);
#endif

  CL_AddFunctionParser (theEnv, "any-factp", CL_Fact_ParseQueryNoAction);
  CL_AddFunctionParser (theEnv, "find-fact", CL_Fact_ParseQueryNoAction);
  CL_AddFunctionParser (theEnv, "find-all-facts", CL_Fact_ParseQueryNoAction);
  CL_AddFunctionParser (theEnv, "do-for-fact", CL_Fact_ParseQueryAction);
  CL_AddFunctionParser (theEnv, "do-for-all-facts", CL_Fact_ParseQueryAction);
  CL_AddFunctionParser (theEnv, "delayed-do-for-all-facts",
			CL_Fact_ParseQueryAction);
}

/*************************************************************
  NAME         : CL_GetQueryFact
  DESCRIPTION  : Internal function for referring to fact
                    array on fact-queries
  INPUTS       : None
  RETURNS      : The name of the specified fact-set member
  SIDE EFFECTS : None
  NOTES        : H/L Syntax : ((query-fact) <index>)
 *************************************************************/
void
CL_GetQueryFact (Environment * theEnv,
		 UDFContext * context, UDFValue * returnValue)
{
  QUERY_CORE *core;

  core = FindQueryCore (theEnv, GetFirstArgument ()->integerValue->contents);

  returnValue->factValue =
    core->solns[GetFirstArgument ()->nextArg->integerValue->contents];
}

/***************************************************************************
  NAME         : CL_GetQueryFactSlot
  DESCRIPTION  : Internal function for referring to slots of fact in
                    fact array on fact-queries
  INPUTS       : The caller's result buffer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Caller's result buffer set appropriately
  NOTES        : H/L Syntax : ((query-fact-slot) <index> <slot-name>)
 **************************************************************************/
void
CL_GetQueryFactSlot (Environment * theEnv,
		     UDFContext * context, UDFValue * returnValue)
{
  Fact *theFact;
  UDFValue temp;
  QUERY_CORE *core;
  unsigned short position;
  const char *varSlot;

  returnValue->lexemeValue = FalseSymbol (theEnv);

  core = FindQueryCore (theEnv, GetFirstArgument ()->integerValue->contents);
  theFact = core->solns[GetFirstArgument ()->nextArg->integerValue->contents];
  varSlot =
    GetFirstArgument ()->nextArg->nextArg->nextArg->lexemeValue->contents;

   /*=========================================*/
  /* Accessing the slot value of a retracted */
  /* fact generates an error.                */
   /*=========================================*/

  if (theFact->garbage)
    {
      CL_FactVarSlotErrorMessage1 (theEnv, theFact, varSlot);
      Set_EvaluationError (theEnv, true);
      return;
    }

   /*=========================*/
  /* Retrieve the slot name. */
   /*=========================*/

  CL_EvaluateExpression (theEnv, GetFirstArgument ()->nextArg->nextArg,
			 &temp);
  if (temp.header->type != SYMBOL_TYPE)
    {
      CL_InvalidVarSlotErrorMessage (theEnv, varSlot);
      Set_EvaluationError (theEnv, true);
      return;
    }

   /*==================================================*/
  /* Make sure the slot exists (the symbol implied is */
  /* used for the implied slot of an ordered fact).   */
   /*==================================================*/

  if (theFact->whichDeftemplate->implied)
    {
      if (strcmp (temp.lexemeValue->contents, "implied") != 0)
	{
	  CL_FactVarSlotErrorMessage2 (theEnv, theFact, varSlot);
	  Set_EvaluationError (theEnv, true);
	  return;
	}
      position = 0;
    }

  else if (CL_FindSlot (theFact->whichDeftemplate,
			(CLIPSLexeme *) temp.value, &position) == NULL)
    {
      CL_FactVarSlotErrorMessage2 (theEnv, theFact, varSlot);
      Set_EvaluationError (theEnv, true);
      return;
    }

  returnValue->value = theFact->theProposition.contents[position].value;
  if (returnValue->header->type == MULTIFIELD_TYPE)
    {
      returnValue->begin = 0;
      returnValue->range = returnValue->multifieldValue->length;
    }
}

/* =============================================================================
   =============================================================================
   Following are the instance query functions :

     any-factp         : Dete_rmines if any facts satisfy the query
     find-fact         : Finds first (set of) fact(s) which satisfies
                               the query and stores it in a multi-field
     find-all-facts    : Finds all (sets of) facts which satisfy the
                               the query and stores them in a multi-field
     do-for-fact       : Executes a given action for the first (set of)
                               fact(s) which satisfy the query
     do-for-all-facts  : Executes an action for all facts which satisfy
                               the query as they are found
     delayed-do-for-all-facts : Same as above - except that the list of facts
                               which satisfy the query is fo_rmed before any
                               actions are executed

     Fact candidate search algorithm :

     All pe_rmutations of first restriction template facts with other
       restriction template facts (Rightmost are varied first)

     For any one template, fact are examined in the order they were defined

     Example :
     (deftemplate a (slot v))
     (deftemplate b (slot v))
     (deftemplate c (slot v))
     (assert (a (v a1)))
     (assert (a (v a2)))
     (assert (b (v b1)))
     (assert (b (v b2)))
     (assert (c (v c1)))
     (assert (c (v c2)))
     (assert (d (v d1)))
     (assert (d (v d2)))

     (any-factp ((?a a b) (?b c)) <query>)

     The pe_rmutations (?a ?b) would be examined in the following order :

     (a1 c1),(a1 c2),(a2 c1),(a2 c2),
     (b1 c1),(b1 c2),(b2 c1),(b2 c2)

   =============================================================================
   ============================================================================= */

/******************************************************************************
  NAME         : CL_Any_Facts
  DESCRIPTION  : Dete_rmines if there any existing facts which satisfy
                   the query
  INPUTS       : None
  RETURNS      : True if the query is satisfied, false otherwise
  SIDE EFFECTS : The query template-expressions are evaluated once,
                   and the query boolean-expression is evaluated
                   zero or more times (depending on fact restrictions
                   and how early the expression evaluates to true - if at all).
  NOTES        : H/L Syntax : See CL_Fact_ParseQueryNoAction()
 ******************************************************************************/
void
CL_Any_Facts (Environment * theEnv,
	      UDFContext * context, UDFValue * returnValue)
{
  QUERY_TEMPLATE *qtemplates;
  unsigned rcnt;
  bool testResult;

  qtemplates = Dete_rmineQueryTemplates (theEnv, GetFirstArgument ()->nextArg,
					 "any-factp", &rcnt);
  if (qtemplates == NULL)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
      return;
    }

  PushQueryCore (theEnv);
  FactQueryData (theEnv)->QueryCore = get_struct (theEnv, query_core);
  FactQueryData (theEnv)->QueryCore->solns =
    (Fact **) CL_gm2 (theEnv, (sizeof (Fact *) * rcnt));
  FactQueryData (theEnv)->QueryCore->query = GetFirstArgument ();
  testResult = TestForFirstInChain (theEnv, qtemplates, 0);
  FactQueryData (theEnv)->AbortQuery = false;
  CL_rm (theEnv, FactQueryData (theEnv)->QueryCore->solns,
	 (sizeof (Fact *) * rcnt));
  rtn_struct (theEnv, query_core, FactQueryData (theEnv)->QueryCore);
  PopQueryCore (theEnv);
  DeleteQueryTemplates (theEnv, qtemplates);
  returnValue->lexemeValue = CL_CreateBoolean (theEnv, testResult);
}

/******************************************************************************
  NAME         : CL_QueryFindFact
  DESCRIPTION  : Finds the first set of facts which satisfy the query and
                   stores their addresses in the user's multi-field variable
  INPUTS       : Caller's result buffer
  RETURNS      : True if the query is satisfied, false otherwise
  SIDE EFFECTS : The query template-expressions are evaluated once,
                   and the query boolean-expression is evaluated
                   zero or more times (depending on fact restrictions
                   and how early the expression evaulates to true - if at all).
  NOTES        : H/L Syntax : See CL_ParseQueryNoAction()
 ******************************************************************************/
void
CL_QueryFindFact (Environment * theEnv,
		  UDFContext * context, UDFValue * returnValue)
{
  QUERY_TEMPLATE *qtemplates;
  unsigned rcnt, i;

  returnValue->begin = 0;
  returnValue->range = 0;
  qtemplates = Dete_rmineQueryTemplates (theEnv, GetFirstArgument ()->nextArg,
					 "find-fact", &rcnt);
  if (qtemplates == NULL)
    {
      returnValue->value = CL_CreateMultifield (theEnv, 0L);
      return;
    }
  PushQueryCore (theEnv);
  FactQueryData (theEnv)->QueryCore = get_struct (theEnv, query_core);
  FactQueryData (theEnv)->QueryCore->solns = (Fact **)
    CL_gm2 (theEnv, (sizeof (Fact *) * rcnt));
  FactQueryData (theEnv)->QueryCore->query = GetFirstArgument ();
  if (TestForFirstInChain (theEnv, qtemplates, 0) == true)
    {
      returnValue->value = CL_CreateMultifield (theEnv, rcnt);
      returnValue->range = rcnt;
      for (i = 0; i < rcnt; i++)
	{
	  returnValue->multifieldValue->contents[i].value =
	    FactQueryData (theEnv)->QueryCore->solns[i];
	}
    }
  else
    returnValue->value = CL_CreateMultifield (theEnv, 0L);
  FactQueryData (theEnv)->AbortQuery = false;
  CL_rm (theEnv, FactQueryData (theEnv)->QueryCore->solns,
	 (sizeof (Fact *) * rcnt));
  rtn_struct (theEnv, query_core, FactQueryData (theEnv)->QueryCore);
  PopQueryCore (theEnv);
  DeleteQueryTemplates (theEnv, qtemplates);
}

/******************************************************************************
  NAME         : QueryFindAll_Facts
  DESCRIPTION  : Finds all sets of facts which satisfy the query and
                   stores their names in the user's multi-field variable

                 The sets are stored sequentially :

                   Number of sets = (Multi-field length) / (Set length)

                 The first set is if the first (set length) atoms of the
                   multi-field variable, and so on.
  INPUTS       : Caller's result buffer
  RETURNS      : Nothing useful
  SIDE EFFECTS : The query template-expressions are evaluated once,
                   and the query boolean-expression is evaluated
                   once for every fact set.
  NOTES        : H/L Syntax : See CL_ParseQueryNoAction()
 ******************************************************************************/
void
QueryFindAll_Facts (Environment * theEnv,
		    UDFContext * context, UDFValue * returnValue)
{
  QUERY_TEMPLATE *qtemplates;
  unsigned rcnt;
  size_t i, j;

  returnValue->begin = 0;
  returnValue->range = 0;
  qtemplates = Dete_rmineQueryTemplates (theEnv, GetFirstArgument ()->nextArg,
					 "find-all-facts", &rcnt);
  if (qtemplates == NULL)
    {
      returnValue->value = CL_CreateMultifield (theEnv, 0L);
      return;
    }
  PushQueryCore (theEnv);
  FactQueryData (theEnv)->QueryCore = get_struct (theEnv, query_core);
  FactQueryData (theEnv)->QueryCore->solns =
    (Fact **) CL_gm2 (theEnv, (sizeof (Fact *) * rcnt));
  FactQueryData (theEnv)->QueryCore->query = GetFirstArgument ();
  FactQueryData (theEnv)->QueryCore->action = NULL;
  FactQueryData (theEnv)->QueryCore->soln_set = NULL;
  FactQueryData (theEnv)->QueryCore->soln_size = rcnt;
  FactQueryData (theEnv)->QueryCore->soln_cnt = 0;
  TestEntireChain (theEnv, qtemplates, 0);
  FactQueryData (theEnv)->AbortQuery = false;
  returnValue->value =
    CL_CreateMultifield (theEnv,
			 FactQueryData (theEnv)->QueryCore->soln_cnt * rcnt);
  while (FactQueryData (theEnv)->QueryCore->soln_set != NULL)
    {
      for (i = 0, j = returnValue->range; i < rcnt; i++, j++)
	{
	  returnValue->multifieldValue->contents[j].value =
	    FactQueryData (theEnv)->QueryCore->soln_set->soln[i];
	}
      returnValue->range = j;
      PopQuerySoln (theEnv);
    }
  CL_rm (theEnv, FactQueryData (theEnv)->QueryCore->solns,
	 (sizeof (Fact *) * rcnt));
  rtn_struct (theEnv, query_core, FactQueryData (theEnv)->QueryCore);
  PopQueryCore (theEnv);
  DeleteQueryTemplates (theEnv, qtemplates);
}

/******************************************************************************
  NAME         : CL_QueryDoForFact
  DESCRIPTION  : Finds the first set of facts which satisfy the query and
                   executes a user-action with that set
  INPUTS       : None
  RETURNS      : Caller's result buffer
  SIDE EFFECTS : The query template-expressions are evaluated once,
                   and the query boolean-expression is evaluated
                   zero or more times (depending on fact restrictions
                   and how early the expression evaulates to true - if at all).
                   Also the action expression is executed zero or once.
                 Caller's result buffer holds result of user-action
  NOTES        : H/L Syntax : See CL_ParseQueryAction()
 ******************************************************************************/
void
CL_QueryDoForFact (Environment * theEnv,
		   UDFContext * context, UDFValue * returnValue)
{
  QUERY_TEMPLATE *qtemplates;
  unsigned i, rcnt;

  returnValue->value = FalseSymbol (theEnv);
  qtemplates =
    Dete_rmineQueryTemplates (theEnv, GetFirstArgument ()->nextArg->nextArg,
			      "do-for-fact", &rcnt);
  if (qtemplates == NULL)
    return;
  PushQueryCore (theEnv);
  FactQueryData (theEnv)->QueryCore = get_struct (theEnv, query_core);
  FactQueryData (theEnv)->QueryCore->solns =
    (Fact **) CL_gm2 (theEnv, (sizeof (Fact *) * rcnt));
  FactQueryData (theEnv)->QueryCore->query = GetFirstArgument ();
  FactQueryData (theEnv)->QueryCore->action = GetFirstArgument ()->nextArg;

  if (TestForFirstInChain (theEnv, qtemplates, 0) == true)
    {
      for (i = 0; i < rcnt; i++)
	{
	  FactQueryData (theEnv)->QueryCore->solns[i]->patternHeader.
	    busyCount++;
	}

      CL_EvaluateExpression (theEnv,
			     FactQueryData (theEnv)->QueryCore->action,
			     returnValue);

      for (i = 0; i < rcnt; i++)
	{
	  FactQueryData (theEnv)->QueryCore->solns[i]->patternHeader.
	    busyCount--;
	}
    }

  FactQueryData (theEnv)->AbortQuery = false;
  ProcedureFunctionData (theEnv)->BreakFlag = false;
  CL_rm (theEnv, FactQueryData (theEnv)->QueryCore->solns,
	 (sizeof (Fact *) * rcnt));
  rtn_struct (theEnv, query_core, FactQueryData (theEnv)->QueryCore);
  PopQueryCore (theEnv);
  DeleteQueryTemplates (theEnv, qtemplates);
}

/******************************************************************************
  NAME         : QueryDoForAll_Facts
  DESCRIPTION  : Finds all sets of facts which satisfy the query and
                   executes a user-function for each set as it is found
  INPUTS       : Caller's result buffer
  RETURNS      : Nothing useful
  SIDE EFFECTS : The query template-expressions are evaluated once,
                   and the query boolean-expression is evaluated
                   once for every fact set.  Also, the action is
                   executed for every fact set.
                 Caller's result buffer holds result of last action executed.
  NOTES        : H/L Syntax : See CL_Fact_ParseQueryAction()
 ******************************************************************************/
void
QueryDoForAll_Facts (Environment * theEnv,
		     UDFContext * context, UDFValue * returnValue)
{
  QUERY_TEMPLATE *qtemplates;
  unsigned rcnt;

  returnValue->lexemeValue = FalseSymbol (theEnv);

  qtemplates =
    Dete_rmineQueryTemplates (theEnv, GetFirstArgument ()->nextArg->nextArg,
			      "do-for-all-facts", &rcnt);
  if (qtemplates == NULL)
    return;

  PushQueryCore (theEnv);
  FactQueryData (theEnv)->QueryCore = get_struct (theEnv, query_core);
  FactQueryData (theEnv)->QueryCore->solns =
    (Fact **) CL_gm2 (theEnv, (sizeof (Fact *) * rcnt));
  FactQueryData (theEnv)->QueryCore->query = GetFirstArgument ();
  FactQueryData (theEnv)->QueryCore->action = GetFirstArgument ()->nextArg;
  FactQueryData (theEnv)->QueryCore->result = returnValue;
  CL_RetainUDFV (theEnv, FactQueryData (theEnv)->QueryCore->result);
  TestEntireChain (theEnv, qtemplates, 0);
  CL_ReleaseUDFV (theEnv, FactQueryData (theEnv)->QueryCore->result);

  FactQueryData (theEnv)->AbortQuery = false;
  ProcedureFunctionData (theEnv)->BreakFlag = false;
  CL_rm (theEnv, FactQueryData (theEnv)->QueryCore->solns,
	 (sizeof (Fact *) * rcnt));
  rtn_struct (theEnv, query_core, FactQueryData (theEnv)->QueryCore);
  PopQueryCore (theEnv);
  DeleteQueryTemplates (theEnv, qtemplates);
}

/******************************************************************************
  NAME         : CL_DelayedQueryDoForAll_Facts
  DESCRIPTION  : Finds all sets of facts which satisfy the query and
                   and exceutes a user-action for each set

                 This function differs from QueryDoForAll_Facts() in
                   that it fo_rms the complete list of query satisfactions
                   BEFORE executing any actions.
  INPUTS       : Caller's result buffer
  RETURNS      : Nothing useful
  SIDE EFFECTS : The query template-expressions are evaluated once,
                   and the query boolean-expression is evaluated
                   once for every fact set.  The action is executed
                   for evry query satisfaction.
                 Caller's result buffer holds result of last action executed.
  NOTES        : H/L Syntax : See CL_Fact_ParseQueryNoAction()
 ******************************************************************************/
void
CL_DelayedQueryDoForAll_Facts (Environment * theEnv,
			       UDFContext * context, UDFValue * returnValue)
{
  QUERY_TEMPLATE *qtemplates;
  unsigned rcnt;
  unsigned i;
  GCBlock gcb;
  QUERY_SOLN *theSet;

  returnValue->value = FalseSymbol (theEnv);
  qtemplates =
    Dete_rmineQueryTemplates (theEnv, GetFirstArgument ()->nextArg->nextArg,
			      "delayed-do-for-all-facts", &rcnt);
  if (qtemplates == NULL)
    return;

  PushQueryCore (theEnv);
  FactQueryData (theEnv)->QueryCore = get_struct (theEnv, query_core);
  FactQueryData (theEnv)->QueryCore->solns =
    (Fact **) CL_gm2 (theEnv, (sizeof (Fact *) * rcnt));
  FactQueryData (theEnv)->QueryCore->query = GetFirstArgument ();
  FactQueryData (theEnv)->QueryCore->action = NULL;
  FactQueryData (theEnv)->QueryCore->soln_set = NULL;
  FactQueryData (theEnv)->QueryCore->soln_size = rcnt;
  FactQueryData (theEnv)->QueryCore->soln_cnt = 0;
  TestEntireChain (theEnv, qtemplates, 0);
  FactQueryData (theEnv)->AbortQuery = false;
  FactQueryData (theEnv)->QueryCore->action = GetFirstArgument ()->nextArg;

   /*==============================================================*/
  /* Increment the busy count for all facts in the solution sets. */
   /*==============================================================*/

  CL_GCBlockStart (theEnv, &gcb);

  for (theSet = FactQueryData (theEnv)->QueryCore->soln_set;
       theSet != NULL; theSet = theSet->nxt)
    {
      for (i = 0; i < rcnt; i++)
	{
	  theSet->soln[i]->patternHeader.busyCount++;
	}
    }

   /*=====================*/
  /* Perfo_rm the action. */
   /*=====================*/

  for (theSet = FactQueryData (theEnv)->QueryCore->soln_set; theSet != NULL;)
    {
      for (i = 0; i < rcnt; i++)
	{
	  if (theSet->soln[i]->garbage)
	    {
	      goto nextSet;
	    }
	  FactQueryData (theEnv)->QueryCore->solns[i] = theSet->soln[i];
	}

      CL_EvaluateExpression (theEnv,
			     FactQueryData (theEnv)->QueryCore->action,
			     returnValue);

      if (CL_EvaluationData (theEnv)->CL_HaltExecution
	  || ProcedureFunctionData (theEnv)->BreakFlag
	  || ProcedureFunctionData (theEnv)->ReturnFlag)
	{
	  break;
	}

      CL_CleanCurrentGarbageFrame (theEnv, NULL);
      CL_CallPeriodicTasks (theEnv);

    nextSet:theSet = theSet->nxt;
    }

   /*==============================================================*/
  /* Decrement the busy count for all facts in the solution sets. */
   /*==============================================================*/

  for (theSet = FactQueryData (theEnv)->QueryCore->soln_set;
       theSet != NULL; theSet = theSet->nxt)
    {
      for (i = 0; i < rcnt; i++)
	{
	  theSet->soln[i]->patternHeader.busyCount--;
	}
    }

  CL_GCBlockEndUDF (theEnv, &gcb, returnValue);
  CL_CallPeriodicTasks (theEnv);

   /*==================================*/
  /* Deallocate the query structures. */
   /*==================================*/

  while (FactQueryData (theEnv)->QueryCore->soln_set != NULL)
    {
      PopQuerySoln (theEnv);
    }

  ProcedureFunctionData (theEnv)->BreakFlag = false;
  CL_rm (theEnv, FactQueryData (theEnv)->QueryCore->solns,
	 (sizeof (Fact *) * rcnt));
  rtn_struct (theEnv, query_core, FactQueryData (theEnv)->QueryCore);
  PopQueryCore (theEnv);
  DeleteQueryTemplates (theEnv, qtemplates);
}

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/*******************************************************
  NAME         : PushQueryCore
  DESCRIPTION  : Pushes the current QueryCore onto stack
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Allocates new stack node and changes
                   QueryCoreStack
  NOTES        : None
 *******************************************************/
static void
PushQueryCore (Environment * theEnv)
{
  QUERY_STACK *qptr;

  qptr = get_struct (theEnv, query_stack);
  qptr->core = FactQueryData (theEnv)->QueryCore;
  qptr->nxt = FactQueryData (theEnv)->QueryCoreStack;
  FactQueryData (theEnv)->QueryCoreStack = qptr;
}

/******************************************************
  NAME         : PopQueryCore
  DESCRIPTION  : Pops top of QueryCore stack and
                   restores QueryCore to this core
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Stack node deallocated, QueryCoreStack
                   changed and QueryCore reset
  NOTES        : Assumes stack is not empty
 ******************************************************/
static void
PopQueryCore (Environment * theEnv)
{
  QUERY_STACK *qptr;

  FactQueryData (theEnv)->QueryCore =
    FactQueryData (theEnv)->QueryCoreStack->core;
  qptr = FactQueryData (theEnv)->QueryCoreStack;
  FactQueryData (theEnv)->QueryCoreStack =
    FactQueryData (theEnv)->QueryCoreStack->nxt;
  rtn_struct (theEnv, query_stack, qptr);
}

/***************************************************
  NAME         : FindQueryCore
  DESCRIPTION  : Looks up a QueryCore Stack Frame
                    Depth 0 is current frame
                    1 is next deepest, etc.
  INPUTS       : Depth
  RETURNS      : Address of query core stack frame
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
static QUERY_CORE *
FindQueryCore (Environment * theEnv, long long depth)
{
  QUERY_STACK *qptr;

  if (depth == 0)
    return FactQueryData (theEnv)->QueryCore;

  qptr = FactQueryData (theEnv)->QueryCoreStack;
  while (depth > 1)
    {
      qptr = qptr->nxt;
      depth--;
    }

  return qptr->core;
}

/**********************************************************
  NAME         : Dete_rmineQueryTemplates
  DESCRIPTION  : CL_Builds a list of templates to be used in
                   fact queries - uses parse fo_rm.
  INPUTS       : 1) The parse template expression chain
                 2) The name of the function being executed
                 3) Caller's buffer for restriction count
                    (# of separate lists)
  RETURNS      : The query list, or NULL on errors
  SIDE EFFECTS : Memory allocated for list
                 Busy count incremented for all templates
  NOTES        : Each restriction is linked by nxt pointer,
                   multiple templates in a restriction are
                   linked by the chain pointer.
                 Rcnt caller's buffer is set to reflect the
                   total number of chains
                 Assumes templateExp is not NULL and that each
                   restriction chain is te_rminated with
                   the QUERY_DELIMITER_SYMBOL "(QDS)"
 **********************************************************/
static QUERY_TEMPLATE *
Dete_rmineQueryTemplates (Environment * theEnv,
			  Expression * templateExp,
			  const char *func, unsigned *rcnt)
{
  QUERY_TEMPLATE *clist = NULL, *cnxt = NULL, *cchain = NULL, *tmp;
  bool new_list = false;
  UDFValue temp;
  Deftemplate *theDeftemplate;

  *rcnt = 0;
  while (templateExp != NULL)
    {
      theDeftemplate = NULL;

      if (templateExp->type == DEFTEMPLATE_PTR)
	{
	  theDeftemplate = (Deftemplate *) templateExp->value;
	}
      else if (CL_EvaluateExpression (theEnv, templateExp, &temp))
	{
	  DeleteQueryTemplates (theEnv, clist);
	  return NULL;
	}

      if ((theDeftemplate == NULL) &&
	  (temp.value ==
	   (void *) FactQueryData (theEnv)->QUERY_DELIMITER_SYMBOL))
	{
	  new_list = true;
	  (*rcnt)++;
	}
      else if ((tmp = Fo_rmChain (theEnv, func, theDeftemplate, &temp)) !=
	       NULL)
	{
	  if (clist == NULL)
	    {
	      clist = cnxt = cchain = tmp;
	    }
	  else if (new_list == true)
	    {
	      new_list = false;
	      cnxt->nxt = tmp;
	      cnxt = cchain = tmp;
	    }
	  else
	    {
	      cchain->chain = tmp;
	    }

	  while (cchain->chain != NULL)
	    {
	      cchain = cchain->chain;
	    }
	}
      else
	{
	  CL_SyntaxErrorMessage (theEnv, "fact-set query class restrictions");
	  DeleteQueryTemplates (theEnv, clist);
	  Set_EvaluationError (theEnv, true);
	  return NULL;
	}

      templateExp = templateExp->nextArg;
    }

  return clist;
}

/*************************************************************
  NAME         : Fo_rmChain
  DESCRIPTION  : CL_Builds a list of deftemplates to be used in
                   fact queries - uses parse fo_rm.
  INPUTS       : 1) Name of calling function for error msgs
                 2) Data object - must be a symbol or a
                      multifield value containing all symbols
                 The symbols must be names of existing templates
  RETURNS      : The query chain, or NULL on errors
  SIDE EFFECTS : Memory allocated for chain
                 Busy count incremented for all templates
  NOTES        : None
 *************************************************************/
static QUERY_TEMPLATE *
Fo_rmChain (Environment * theEnv,
	    const char *func, Deftemplate * theDeftemplate, UDFValue * val)
{
  Deftemplate *templatePtr;
  QUERY_TEMPLATE *head, *bot, *tmp;
  size_t i;			/* 6.04 Bug Fix */
  const char *templateName;
  unsigned int count;

  if (theDeftemplate != NULL)
    {
      CL_IncrementDeftemplateBusyCount (theEnv, theDeftemplate);
      head = get_struct (theEnv, query_template);
      head->templatePtr = theDeftemplate;

      head->chain = NULL;
      head->nxt = NULL;
      return head;
    }

  if (val->header->type == SYMBOL_TYPE)
    {
      /*============================================*/
      /* Allow instance-set query restrictions to   */
      /* have module specifier as part of the class */
      /* name, but search imported defclasses too   */
      /* if a module specifier is not given.        */
      /*============================================*/

      templatePtr = (Deftemplate *)
	CL_FindImportedConstruct (theEnv, "deftemplate", NULL,
				  val->lexemeValue->contents, &count, true,
				  NULL);

      if (templatePtr == NULL)
	{
	  CL_CantFindItemInFunctionErrorMessage (theEnv, "deftemplate",
						 val->lexemeValue->contents,
						 func, true);
	  return NULL;
	}

      CL_IncrementDeftemplateBusyCount (theEnv, templatePtr);
      head = get_struct (theEnv, query_template);
      head->templatePtr = templatePtr;

      head->chain = NULL;
      head->nxt = NULL;
      return head;
    }

  if (val->header->type == MULTIFIELD_TYPE)
    {
      head = bot = NULL;

      for (i = val->begin; i < (val->begin + val->range); i++)
	{
	  if (val->multifieldValue->contents[i].header->type == SYMBOL_TYPE)
	    {
	      templateName =
		val->multifieldValue->contents[i].lexemeValue->contents;

	      templatePtr = (Deftemplate *)
		CL_FindImportedConstruct (theEnv, "deftemplate", NULL,
					  templateName, &count, true, NULL);

	      if (templatePtr == NULL)
		{
		  CL_CantFindItemInFunctionErrorMessage (theEnv,
							 "deftemplate",
							 templateName, func,
							 true);
		  DeleteQueryTemplates (theEnv, head);
		  return NULL;
		}
	    }
	  else
	    {
	      DeleteQueryTemplates (theEnv, head);
	      return NULL;
	    }

	  CL_IncrementDeftemplateBusyCount (theEnv, templatePtr);
	  tmp = get_struct (theEnv, query_template);
	  tmp->templatePtr = templatePtr;

	  tmp->chain = NULL;
	  tmp->nxt = NULL;

	  if (head == NULL)
	    {
	      head = tmp;
	    }
	  else
	    {
	      bot->chain = tmp;
	    }

	  bot = tmp;
	}

      return head;
    }
  return NULL;
}

/******************************************************
  NAME         : DeleteQueryTemplates
  DESCRIPTION  : Deletes a query class-list
  INPUTS       : The query list address
  RETURNS      : Nothing useful
  SIDE EFFECTS : Nodes deallocated
                 Busy count decremented for all templates
  NOTES        : None
 ******************************************************/
static void
DeleteQueryTemplates (Environment * theEnv, QUERY_TEMPLATE * qlist)
{
  QUERY_TEMPLATE *tmp;

  while (qlist != NULL)
    {
      while (qlist->chain != NULL)
	{
	  tmp = qlist->chain;
	  qlist->chain = qlist->chain->chain;
	  CL_DecrementDeftemplateBusyCount (theEnv, tmp->templatePtr);
	  rtn_struct (theEnv, query_template, tmp);
	}
      tmp = qlist;
      qlist = qlist->nxt;
      CL_DecrementDeftemplateBusyCount (theEnv, tmp->templatePtr);
      rtn_struct (theEnv, query_template, tmp);
    }
}

/************************************************************
  NAME         : TestForFirstInChain
  DESCRIPTION  : Processes all templates in a restriction chain
                   until success or done
  INPUTS       : 1) The current chain
                 2) The index of the chain restriction
                     (e.g. the 4th query-variable)
  RETURNS      : True if query succeeds, false otherwise
  SIDE EFFECTS : Sets current restriction class
                 Fact variable values set
  NOTES        : None
 ************************************************************/
static bool
TestForFirstInChain (Environment * theEnv,
		     QUERY_TEMPLATE * qchain, unsigned indx)
{
  QUERY_TEMPLATE *qptr;

  FactQueryData (theEnv)->AbortQuery = true;
  for (qptr = qchain; qptr != NULL; qptr = qptr->chain)
    {
      FactQueryData (theEnv)->AbortQuery = false;

      if (TestForFirstFactInTemplate
	  (theEnv, qptr->templatePtr, qchain, indx))
	{
	  return true;
	}

      if ((CL_EvaluationData (theEnv)->CL_HaltExecution == true)
	  || (FactQueryData (theEnv)->AbortQuery == true))
	return false;
    }
  return false;
}

/*****************************************************************
  NAME         : TestForFirstFactInTemplate
  DESCRIPTION  : Processes all facts in a template
  INPUTS       : 1) Visitation traversal id
                 2) The template
                 3) The current template restriction chain
                 4) The index of the current restriction
  RETURNS      : True if query succeeds, false otherwise
  SIDE EFFECTS : Fact variable values set
  NOTES        : None
 *****************************************************************/
static bool
TestForFirstFactInTemplate (Environment * theEnv,
			    Deftemplate * templatePtr,
			    QUERY_TEMPLATE * qchain, unsigned indx)
{
  Fact *theFact;
  UDFValue temp;
  GCBlock gcb;
  unsigned j;

  CL_GCBlockStart (theEnv, &gcb);

  theFact = templatePtr->factList;
  while (theFact != NULL)
    {
      FactQueryData (theEnv)->QueryCore->solns[indx] = theFact;
      if (qchain->nxt != NULL)
	{
	  theFact->patternHeader.busyCount++;
	  if (TestForFirstInChain (theEnv, qchain->nxt, indx + 1) == true)
	    {
	      theFact->patternHeader.busyCount--;
	      break;
	    }
	  theFact->patternHeader.busyCount--;
	  if ((CL_EvaluationData (theEnv)->CL_HaltExecution == true)
	      || (FactQueryData (theEnv)->AbortQuery == true))
	    break;
	}
      else
	{
	  for (j = 0; j < indx; j++)
	    {
	      if (FactQueryData (theEnv)->QueryCore->solns[j]->garbage)
		{
		  theFact = NULL;
		  goto endTest;
		}
	    }

	  theFact->patternHeader.busyCount++;

	  CL_EvaluateExpression (theEnv,
				 FactQueryData (theEnv)->QueryCore->query,
				 &temp);

	  CL_CleanCurrentGarbageFrame (theEnv, NULL);
	  CL_CallPeriodicTasks (theEnv);

	  theFact->patternHeader.busyCount--;

	  if (CL_EvaluationData (theEnv)->CL_HaltExecution == true)
	    {
	      break;
	    }

	  if (temp.value != FalseSymbol (theEnv))
	    {
	      break;
	    }
	}

      /*================================================*/
      /* Get the next fact that has not been retracted. */
      /*================================================*/

      theFact = theFact->nextTemplateFact;
      while ((theFact != NULL) ? (theFact->garbage == 1) : false)
	{
	  theFact = theFact->nextTemplateFact;
	}
    }

endTest:

  CL_GCBlockEnd (theEnv, &gcb);
  CL_CallPeriodicTasks (theEnv);

  if (theFact != NULL)
    return (((CL_EvaluationData (theEnv)->CL_HaltExecution == true)
	     || (FactQueryData (theEnv)->AbortQuery == true)) ? false : true);

  return false;
}

/************************************************************
  NAME         : TestEntireChain
  DESCRIPTION  : Processes all templates in a restriction chain
                   until done
  INPUTS       : 1) The current chain
                 2) The index of the chain restriction
                     (i.e. the 4th query-variable)
  RETURNS      : Nothing useful
  SIDE EFFECTS : Sets current restriction template
                 Query fact variables set
                 Solution sets stored in global list
  NOTES        : None
 ************************************************************/
static void
TestEntireChain (Environment * theEnv, QUERY_TEMPLATE * qchain, unsigned indx)
{
  QUERY_TEMPLATE *qptr;

  FactQueryData (theEnv)->AbortQuery = true;
  for (qptr = qchain; qptr != NULL; qptr = qptr->chain)
    {
      FactQueryData (theEnv)->AbortQuery = false;

      TestEntireTemplate (theEnv, qptr->templatePtr, qchain, indx);

      if ((CL_EvaluationData (theEnv)->CL_HaltExecution == true)
	  || (FactQueryData (theEnv)->AbortQuery == true))
	return;
    }
}

/*****************************************************************
  NAME         : TestEntireTemplate
  DESCRIPTION  : Processes all facts in a template
  INPUTS       : 1) The module for which templates tested must be
                    in scope
                 3) The template
                 4) The current template restriction chain
                 5) The index of the current restriction
  RETURNS      : Nothing useful
  SIDE EFFECTS : Instance variable values set
                 Solution sets stored in global list
  NOTES        : None
 *****************************************************************/
static void
TestEntireTemplate (Environment * theEnv,
		    Deftemplate * templatePtr,
		    QUERY_TEMPLATE * qchain, unsigned indx)
{
  Fact *theFact;
  UDFValue temp;
  GCBlock gcb;
  unsigned j;

  CL_GCBlockStart (theEnv, &gcb);

  theFact = templatePtr->factList;
  while (theFact != NULL)
    {
      FactQueryData (theEnv)->QueryCore->solns[indx] = theFact;
      if (qchain->nxt != NULL)
	{
	  theFact->patternHeader.busyCount++;
	  TestEntireChain (theEnv, qchain->nxt, indx + 1);
	  theFact->patternHeader.busyCount--;
	  if ((CL_EvaluationData (theEnv)->CL_HaltExecution == true)
	      || (FactQueryData (theEnv)->AbortQuery == true))
	    break;
	}
      else
	{
	  for (j = 0; j < indx; j++)
	    {
	      if (FactQueryData (theEnv)->QueryCore->solns[j]->garbage)
		{
		  goto endTest;
		}
	    }

	  theFact->patternHeader.busyCount++;

	  CL_EvaluateExpression (theEnv,
				 FactQueryData (theEnv)->QueryCore->query,
				 &temp);

	  theFact->patternHeader.busyCount--;
	  if (CL_EvaluationData (theEnv)->CL_HaltExecution == true)
	    break;
	  if (temp.value != FalseSymbol (theEnv))
	    {
	      if (FactQueryData (theEnv)->QueryCore->action != NULL)
		{
		  theFact->patternHeader.busyCount++;
		  CL_ReleaseUDFV (theEnv,
				  FactQueryData (theEnv)->QueryCore->result);
		  CL_EvaluateExpression (theEnv,
					 FactQueryData (theEnv)->QueryCore->
					 action,
					 FactQueryData (theEnv)->QueryCore->
					 result);
		  CL_RetainUDFV (theEnv,
				 FactQueryData (theEnv)->QueryCore->result);

		  theFact->patternHeader.busyCount--;
		  if (ProcedureFunctionData (theEnv)->BreakFlag
		      || ProcedureFunctionData (theEnv)->ReturnFlag)
		    {
		      FactQueryData (theEnv)->AbortQuery = true;
		      break;
		    }
		  if (CL_EvaluationData (theEnv)->CL_HaltExecution == true)
		    break;
		}
	      else
		AddSolution (theEnv);
	    }
	}

      theFact = theFact->nextTemplateFact;
      while ((theFact != NULL) ? (theFact->garbage == 1) : false)
	{
	  theFact = theFact->nextTemplateFact;
	}

      CL_CleanCurrentGarbageFrame (theEnv, NULL);
      CL_CallPeriodicTasks (theEnv);
    }

endTest:

  CL_GCBlockEnd (theEnv, &gcb);
  CL_CallPeriodicTasks (theEnv);
}

/***************************************************************************
  NAME         : AddSolution
  DESCRIPTION  : Adds the current fact set to a global list of
                   solutions
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Global list and count updated
  NOTES        : Solutions are stored as sequential arrays of Fact *
 ***************************************************************************/
static void
AddSolution (Environment * theEnv)
{
  QUERY_SOLN *new_soln;
  unsigned i;

  new_soln = (QUERY_SOLN *) CL_gm2 (theEnv, sizeof (QUERY_SOLN));
  new_soln->soln = (Fact **)
    CL_gm2 (theEnv,
	    (sizeof (Fact *) *
	     (FactQueryData (theEnv)->QueryCore->soln_size)));
  for (i = 0; i < FactQueryData (theEnv)->QueryCore->soln_size; i++)
    new_soln->soln[i] = FactQueryData (theEnv)->QueryCore->solns[i];
  new_soln->nxt = NULL;
  if (FactQueryData (theEnv)->QueryCore->soln_set == NULL)
    FactQueryData (theEnv)->QueryCore->soln_set = new_soln;
  else
    FactQueryData (theEnv)->QueryCore->soln_bottom->nxt = new_soln;
  FactQueryData (theEnv)->QueryCore->soln_bottom = new_soln;
  FactQueryData (theEnv)->QueryCore->soln_cnt++;
}

/***************************************************
  NAME         : PopQuerySoln
  DESCRIPTION  : Deallocates the topmost solution
                   set for an fact-set query
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Solution set deallocated
  NOTES        : Assumes QueryCore->soln_set != 0
 ***************************************************/
static void
PopQuerySoln (Environment * theEnv)
{
  FactQueryData (theEnv)->QueryCore->soln_bottom =
    FactQueryData (theEnv)->QueryCore->soln_set;
  FactQueryData (theEnv)->QueryCore->soln_set =
    FactQueryData (theEnv)->QueryCore->soln_set->nxt;
  CL_rm (theEnv, FactQueryData (theEnv)->QueryCore->soln_bottom->soln,
	 (sizeof (Fact *) * FactQueryData (theEnv)->QueryCore->soln_size));
  CL_rm (theEnv, FactQueryData (theEnv)->QueryCore->soln_bottom,
	 sizeof (QUERY_SOLN));
}

#endif
