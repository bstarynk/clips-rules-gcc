   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/18/16             */
   /*                                                     */
   /*             PROCEDURAL FUNCTIONS MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for several procedural         */
/*   functions including if, while, loop-for-count, bind,    */
/*   progn, return, break, and switch                        */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*            Changed name of variable exp to theExp         */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Local variables set with the bind function     */
/*            persist until a reset/clear command is issued. */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Support for long long integers.                */
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
/*            UDF redesign.                                  */
/*                                                           */
/*            Added CL_GCBlockStart and CL_GCBlockEnd functions    */
/*            for garbage collection blocks.                 */
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
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "multifld.h"
#include "prcdrpsr.h"
#include "router.h"
#include "scanner.h"
#include "utility.h"

#include "prcdrfun.h"

#if DEFGLOBAL_CONSTRUCT
#include "globldef.h"
#endif

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void DeallocateProceduralFunctionData (Environment *);

/**********************************************/
/* CL_ProceduralFunctionDefinitions: Initializes */
/*   the procedural functions.                */
/**********************************************/
void
CL_ProceduralFunctionDefinitions (Environment * theEnv)
{
  CL_AllocateEnvironmentData (theEnv, PRCDRFUN_DATA,
			      sizeof (struct procedureFunctionData),
			      DeallocateProceduralFunctionData);

#if ! RUN_TIME
  CL_AddUDF (theEnv, "if", "*", 0, UNBOUNDED, NULL, CL_IfFunction,
	     "CL_IfFunction", NULL);
  CL_AddUDF (theEnv, "while", "*", 0, UNBOUNDED, NULL, CL_WhileFunction,
	     "CL_WhileFunction", NULL);
  CL_AddUDF (theEnv, "loop-for-count", "*", 0, UNBOUNDED, NULL,
	     CL_LoopForCountFunction, "CL_LoopForCountFunction", NULL);
  CL_AddUDF (theEnv, "(get-loop-count)", "l", 1, 1, NULL, CL_GetLoopCount,
	     "CL_GetLoopCount", NULL);
  CL_AddUDF (theEnv, "bind", "*", 0, UNBOUNDED, NULL, CL_BindFunction,
	     "CL_BindFunction", NULL);
  CL_AddUDF (theEnv, "progn", "*", 0, UNBOUNDED, NULL, CL_PrognFunction,
	     "CL_PrognFunction", NULL);
  CL_AddUDF (theEnv, "return", "*", 0, UNBOUNDED, NULL, CL_ReturnFunction,
	     "CL_ReturnFunction", NULL);
  CL_AddUDF (theEnv, "break", "v", 0, 0, NULL, CL_BreakFunction,
	     "CL_BreakFunction", NULL);
  CL_AddUDF (theEnv, "switch", "*", 0, UNBOUNDED, NULL, CL_SwitchFunction,
	     "CL_SwitchFunction", NULL);
#endif

  CL_ProceduralFunctionParsers (theEnv);

#if ! RUN_TIME
  CL_FuncSeqOvlFlags (theEnv, "progn", false, false);
  CL_FuncSeqOvlFlags (theEnv, "if", false, false);
  CL_FuncSeqOvlFlags (theEnv, "while", false, false);
  CL_FuncSeqOvlFlags (theEnv, "loop-for-count", false, false);
  CL_FuncSeqOvlFlags (theEnv, "return", false, false);
  CL_FuncSeqOvlFlags (theEnv, "switch", false, false);
#endif

  CL_Add_ResetFunction (theEnv, "bind", CL_FlushBindList, 0, NULL);
  CL_Add_ClearFunction (theEnv, "bind", CL_FlushBindList, 0, NULL);
}

/*************************************************************/
/* DeallocateProceduralFunctionData: Deallocates environment */
/*    data for procedural functions.                         */
/*************************************************************/
static void
DeallocateProceduralFunctionData (Environment * theEnv)
{
  UDFValue *nextPtr, *garbagePtr;

  garbagePtr = ProcedureFunctionData (theEnv)->BindList;

  while (garbagePtr != NULL)
    {
      nextPtr = garbagePtr->next;
      rtn_struct (theEnv, udfValue, garbagePtr);
      garbagePtr = nextPtr;
    }
}

/***************************************/
/* CL_WhileFunction: H/L access routine   */
/*   for the while function.           */
/***************************************/
void
CL_WhileFunction (Environment * theEnv,
		  UDFContext * context, UDFValue * returnValue)
{
  UDFValue theResult;
  GCBlock gcb;

   /*====================================================*/
  /* CL_Evaluate the body of the while loop as long as the */
  /* while condition evaluates to a non-FALSE value.    */
   /*====================================================*/

  CL_GCBlockStart (theEnv, &gcb);

  CL_UDFNthArgument (context, 1, ANY_TYPE_BITS, &theResult);
  while ((theResult.value != FalseSymbol (theEnv)) &&
	 (CL_EvaluationData (theEnv)->CL_HaltExecution != true))
    {
      if ((ProcedureFunctionData (theEnv)->BreakFlag == true)
	  || (ProcedureFunctionData (theEnv)->ReturnFlag == true))
	break;

      CL_UDFNthArgument (context, 2, ANY_TYPE_BITS, &theResult);

      if ((ProcedureFunctionData (theEnv)->BreakFlag == true)
	  || (ProcedureFunctionData (theEnv)->ReturnFlag == true))
	break;

      CL_CleanCurrentGarbageFrame (theEnv, NULL);
      CL_CallPeriodicTasks (theEnv);

      CL_UDFNthArgument (context, 1, ANY_TYPE_BITS, &theResult);
    }

   /*=====================================================*/
  /* CL_Reset the break flag. The return flag is not reset  */
  /* because the while loop is probably contained within */
  /* a deffunction or RHS of a rule which needs to be    */
  /* returned from as well.                              */
   /*=====================================================*/

  ProcedureFunctionData (theEnv)->BreakFlag = false;

   /*====================================================*/
  /* If the return command was issued, then return that */
  /* value, otherwise return the symbol FALSE.          */
   /*====================================================*/

  if (ProcedureFunctionData (theEnv)->ReturnFlag == true)
    {
      returnValue->value = theResult.value;
      returnValue->begin = theResult.begin;
      returnValue->range = theResult.range;
    }
  else
    {
      returnValue->value = FalseSymbol (theEnv);
    }

  CL_GCBlockEndUDF (theEnv, &gcb, returnValue);
  CL_CallPeriodicTasks (theEnv);
}

/********************************************/
/* CL_LoopForCountFunction: H/L access routine */
/*   for the loop-for-count function.       */
/********************************************/
void
CL_LoopForCountFunction (Environment * theEnv,
			 UDFContext * context, UDFValue * loopResult)
{
  UDFValue theArg;
  long long iterationEnd;
  LOOP_COUNTER_STACK *tmpCounter;
  GCBlock gcb;

  tmpCounter = get_struct (theEnv, loopCounterStack);
  tmpCounter->loopCounter = 0L;
  tmpCounter->nxt = ProcedureFunctionData (theEnv)->LoopCounterStack;
  ProcedureFunctionData (theEnv)->LoopCounterStack = tmpCounter;

  if (!CL_UDFNthArgument (context, 1, INTEGER_BIT, &theArg))
    {
      loopResult->value = FalseSymbol (theEnv);
      ProcedureFunctionData (theEnv)->LoopCounterStack = tmpCounter->nxt;
      rtn_struct (theEnv, loopCounterStack, tmpCounter);
      return;
    }
  tmpCounter->loopCounter = theArg.integerValue->contents;
  if (!CL_UDFNthArgument (context, 2, INTEGER_BIT, &theArg))
    {
      loopResult->value = FalseSymbol (theEnv);
      ProcedureFunctionData (theEnv)->LoopCounterStack = tmpCounter->nxt;
      rtn_struct (theEnv, loopCounterStack, tmpCounter);
      return;
    }

  CL_GCBlockStart (theEnv, &gcb);

  iterationEnd = theArg.integerValue->contents;
  while ((tmpCounter->loopCounter <= iterationEnd) &&
	 (CL_EvaluationData (theEnv)->CL_HaltExecution != true))
    {
      if ((ProcedureFunctionData (theEnv)->BreakFlag == true)
	  || (ProcedureFunctionData (theEnv)->ReturnFlag == true))
	break;

      CL_UDFNthArgument (context, 3, ANY_TYPE_BITS, &theArg);

      if ((ProcedureFunctionData (theEnv)->BreakFlag == true)
	  || (ProcedureFunctionData (theEnv)->ReturnFlag == true))
	break;

      CL_CleanCurrentGarbageFrame (theEnv, NULL);
      CL_CallPeriodicTasks (theEnv);

      tmpCounter->loopCounter++;
    }

  ProcedureFunctionData (theEnv)->BreakFlag = false;
  if (ProcedureFunctionData (theEnv)->ReturnFlag == true)
    {
      loopResult->value = theArg.value;
      loopResult->begin = theArg.begin;
      loopResult->range = theArg.range;
    }
  else
    {
      loopResult->value = FalseSymbol (theEnv);
    }
  ProcedureFunctionData (theEnv)->LoopCounterStack = tmpCounter->nxt;
  rtn_struct (theEnv, loopCounterStack, tmpCounter);

  CL_GCBlockEndUDF (theEnv, &gcb, loopResult);
  CL_CallPeriodicTasks (theEnv);
}

/*****************/
/* CL_GetLoopCount: */
/*****************/
void
CL_GetLoopCount (Environment * theEnv,
		 UDFContext * context, UDFValue * returnValue)
{
  long long depth;
  UDFValue theArg;
  LOOP_COUNTER_STACK *tmpCounter;

  if (!CL_UDFFirstArgument (context, INTEGER_BIT, &theArg))
    {
      return;
    }
  depth = theArg.integerValue->contents;
  tmpCounter = ProcedureFunctionData (theEnv)->LoopCounterStack;
  while (depth > 0)
    {
      tmpCounter = tmpCounter->nxt;
      depth--;
    }

  returnValue->integerValue =
    CL_CreateInteger (theEnv, tmpCounter->loopCounter);
}

/************************************/
/* CL_IfFunction: H/L access routine   */
/*   for the if function.           */
/************************************/
void
CL_IfFunction (Environment * theEnv,
	       UDFContext * context, UDFValue * returnValue)
{
  unsigned int numArgs;

   /*=========================*/
  /* CL_Evaluate the condition. */
   /*=========================*/

  if (!CL_UDFNthArgument (context, 1, ANY_TYPE_BITS, returnValue))
    {
      returnValue->value = FalseSymbol (theEnv);
      return;
    }

  if ((ProcedureFunctionData (theEnv)->BreakFlag == true) ||
      (ProcedureFunctionData (theEnv)->ReturnFlag == true))
    {
      returnValue->value = FalseSymbol (theEnv);
      return;
    }

   /*=========================================*/
  /* If the condition evaluated to FALSE and */
  /* an "else" portion exists, evaluate it   */
  /* and return the value.                   */
   /*=========================================*/

  numArgs = CL_UDFArgumentCount (context);
  if ((returnValue->value == FalseSymbol (theEnv)) && (numArgs == 3))
    {
      CL_UDFNthArgument (context, 3, ANY_TYPE_BITS, returnValue);
      return;
    }

   /*===================================================*/
  /* Otherwise if the symbol evaluated to a non-FALSE  */
  /* value, evaluate the "then" portion and return it. */
   /*===================================================*/

  else if (returnValue->value != FalseSymbol (theEnv))
    {
      CL_UDFNthArgument (context, 2, ANY_TYPE_BITS, returnValue);
      return;
    }

   /*=========================================*/
  /* Return FALSE if the condition evaluated */
  /* to FALSE and there is no "else" portion */
  /* of the if statement.                    */
   /*=========================================*/

  returnValue->value = FalseSymbol (theEnv);
}

/**************************************/
/* CL_BindFunction: H/L access routine   */
/*   for the bind function.           */
/**************************************/
void
CL_BindFunction (Environment * theEnv,
		 UDFContext * context, UDFValue * returnValue)
{
  UDFValue *theBind, *lastBind;
  bool found = false, unbindVar = false;
  CLIPSLexeme *variableName = NULL;
#if DEFGLOBAL_CONSTRUCT
  Defglobal *theGlobal = NULL;
#endif

   /*===============================================*/
  /* Dete_rmine the name of the variable to be set. */
   /*===============================================*/

#if DEFGLOBAL_CONSTRUCT
  if (GetFirstArgument ()->type == DEFGLOBAL_PTR)
    {
      theGlobal = (Defglobal *) GetFirstArgument ()->value;
    }
  else
#endif
    {
      CL_EvaluateExpression (theEnv, GetFirstArgument (), returnValue);
      variableName = returnValue->lexemeValue;
    }

   /*===========================================*/
  /* Dete_rmine the new value for the variable. */
   /*===========================================*/

  if (GetFirstArgument ()->nextArg == NULL)
    {
      unbindVar = true;
    }
  else if (GetFirstArgument ()->nextArg->nextArg == NULL)
    {
      CL_EvaluateExpression (theEnv, GetFirstArgument ()->nextArg,
			     returnValue);
    }
  else
    {
      CL_StoreInMultifield (theEnv, returnValue, GetFirstArgument ()->nextArg,
			    true);
    }

   /*==================================*/
  /* Bind a defglobal if appropriate. */
   /*==================================*/

#if DEFGLOBAL_CONSTRUCT
  if (theGlobal != NULL)
    {
      CL_QSetDefglobalValue (theEnv, theGlobal, returnValue, unbindVar);
      return;
    }
#endif

   /*===============================================*/
  /* Search for the variable in the list of binds. */
   /*===============================================*/

  theBind = ProcedureFunctionData (theEnv)->BindList;
  lastBind = NULL;

  while ((theBind != NULL) && (found == false))
    {
      if (theBind->supplementalInfo == (void *) variableName)
	{
	  found = true;
	}
      else
	{
	  lastBind = theBind;
	  theBind = theBind->next;
	}
    }

   /*========================================================*/
  /* If variable was not in the list of binds, then add it. */
  /* Make sure that this operation preserves the bind list  */
  /* as a stack.                                            */
   /*========================================================*/

  if (found == false)
    {
      if (unbindVar == false)
	{
	  theBind = get_struct (theEnv, udfValue);
	  theBind->supplementalInfo = (void *) variableName;
	  IncrementLexemeCount (variableName);
	  theBind->next = NULL;
	  if (lastBind == NULL)
	    {
	      ProcedureFunctionData (theEnv)->BindList = theBind;
	    }
	  else
	    {
	      lastBind->next = theBind;
	    }
	}
      else
	{
	  returnValue->value = FalseSymbol (theEnv);
	  return;
	}
    }
  else
    {
      CL_ReleaseUDFV (theEnv, theBind);
    }

   /*================================*/
  /* Set the value of the variable. */
   /*================================*/

  if (unbindVar == false)
    {
      theBind->value = returnValue->value;
      theBind->begin = returnValue->begin;
      theBind->range = returnValue->range;
      CL_RetainUDFV (theEnv, returnValue);
    }
  else
    {
      if (lastBind == NULL)
	ProcedureFunctionData (theEnv)->BindList = theBind->next;
      else
	lastBind->next = theBind->next;
      CL_ReleaseLexeme (theEnv, (CLIPSLexeme *) theBind->supplementalInfo);
      rtn_struct (theEnv, udfValue, theBind);
      returnValue->value = FalseSymbol (theEnv);
    }
}

/*******************************************/
/* CL_GetBoundVariable: Searches the BindList */
/*   for a specified variable.             */
/*******************************************/
bool
CL_GetBoundVariable (Environment * theEnv,
		     UDFValue * vPtr, CLIPSLexeme * varName)
{
  UDFValue *bindPtr;

  for (bindPtr = ProcedureFunctionData (theEnv)->BindList; bindPtr != NULL;
       bindPtr = bindPtr->next)
    {
      if (bindPtr->supplementalInfo == (void *) varName)
	{
	  vPtr->value = bindPtr->value;
	  vPtr->begin = bindPtr->begin;
	  vPtr->range = bindPtr->range;
	  return true;
	}
    }

  return false;
}

/*************************************************/
/* CL_FlushBindList: Removes all variables from the */
/*   list of currently bound local variables.    */
/*************************************************/
void
CL_FlushBindList (Environment * theEnv, void *context)
{
  CL_ReturnValues (theEnv, ProcedureFunctionData (theEnv)->BindList, true);
  ProcedureFunctionData (theEnv)->BindList = NULL;
}

/***************************************/
/* CL_PrognFunction: H/L access routine   */
/*   for the progn function.           */
/***************************************/
void
CL_PrognFunction (Environment * theEnv,
		  UDFContext * context, UDFValue * returnValue)
{
  struct expr *argPtr;

  argPtr = CL_EvaluationData (theEnv)->CurrentExpression->argList;

  if (argPtr == NULL)
    {
      returnValue->value = FalseSymbol (theEnv);
      return;
    }

  while ((argPtr != NULL) && (CL_Get_HaltExecution (theEnv) != true))
    {
      CL_EvaluateExpression (theEnv, argPtr, returnValue);

      if ((ProcedureFunctionData (theEnv)->BreakFlag == true)
	  || (ProcedureFunctionData (theEnv)->ReturnFlag == true))
	break;
      argPtr = argPtr->nextArg;
    }

  if (CL_Get_HaltExecution (theEnv) == true)
    {
      returnValue->value = FalseSymbol (theEnv);
      return;
    }

  return;
}

/***************************************************************/
/* CL_ReturnFunction: H/L access routine for the return function. */
/***************************************************************/
void
CL_ReturnFunction (Environment * theEnv,
		   UDFContext * context, UDFValue * returnValue)
{
  if (!UDFHasNextArgument (context))
    {
      returnValue->voidValue = VoidConstant (theEnv);
    }
  else
    {
      CL_UDFNextArgument (context, ANY_TYPE_BITS, returnValue);
    }
  ProcedureFunctionData (theEnv)->ReturnFlag = true;
}

/***************************************************************/
/* CL_BreakFunction: H/L access routine for the break function.   */
/***************************************************************/
void
CL_BreakFunction (Environment * theEnv,
		  UDFContext * context, UDFValue * returnValue)
{
  ProcedureFunctionData (theEnv)->BreakFlag = true;
}

/*****************************************************************/
/* CL_SwitchFunction: H/L access routine for the switch function.   */
/*****************************************************************/
void
CL_SwitchFunction (Environment * theEnv,
		   UDFContext * context, UDFValue * returnValue)
{
  UDFValue switch_val, case_val;
  Expression *theExp;

  returnValue->lexemeValue = FalseSymbol (theEnv);

  /* ==========================
     Get the value to switch on
     ========================== */
  CL_EvaluateExpression (theEnv, GetFirstArgument (), &switch_val);
  if (CL_EvaluationData (theEnv)->CL_EvaluationError)
    return;
  for (theExp = GetFirstArgument ()->nextArg; theExp != NULL;
       theExp = theExp->nextArg->nextArg)
    {
      /* =================================================
         CL_VOID_TYPE is the default case (if any) for the switch
         ================================================= */
      if (theExp->type == CL_VOID_TYPE)
	{
	  CL_EvaluateExpression (theEnv, theExp->nextArg, returnValue);
	  return;
	}

      /* ====================================================
         If the case matches, evaluate the actions and return
         ==================================================== */
      CL_EvaluateExpression (theEnv, theExp, &case_val);
      if (CL_EvaluationData (theEnv)->CL_EvaluationError)
	return;
      if (switch_val.header->type == case_val.header->type)
	{
	  if ((case_val.header->type ==
	       MULTIFIELD_TYPE) ? Multifield_DOsEqual (&switch_val,
						       &case_val)
	      : (switch_val.value == case_val.value))
	    {
	      CL_EvaluateExpression (theEnv, theExp->nextArg, returnValue);
	      return;
	    }
	}
    }
}
