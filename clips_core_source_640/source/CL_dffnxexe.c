   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/01/16             */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Deffunction Execution Routines                   */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.30: Changed garbage collection algorithm.          */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
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
/*            Added CL_GCBlockStart and CL_GCBlockEnd functions    */
/*            for garbage collection blocks.                 */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if DEFFUNCTION_CONSTRUCT

#include <stdio.h>
#include <string.h>

#include "constrct.h"
#include "envrnmnt.h"
#include "prcdrfun.h"
#include "prccode.h"
#include "prntutil.h"
#include "proflfun.h"
#include "router.h"
#include "utility.h"
#include "watch.h"

#include "dffnxexe.h"

/* =========================================
   *****************************************
                   CONSTANTS
   =========================================
   ***************************************** */
#define BEGIN_TRACE ">> "
#define END_TRACE   "<< "

/* =========================================
   *****************************************
      INTERNALLY VISIBLE FUNCTION HEADERS
   =========================================
   ***************************************** */

static void UnboundDeffunctionErr (Environment *, const char *);

#if DEBUGGING_FUNCTIONS
static void CL_WatchDeffunction (Environment *, const char *);
#endif

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/****************************************************
  NAME         : CL_CallDeffunction
  DESCRIPTION  : Executes the body of a deffunction
  INPUTS       : 1) The deffunction
                 2) Argument expressions
                 3) Data object buffer to hold result
  RETURNS      : Nothing useful
  SIDE EFFECTS : Deffunction executed and result
                 stored in data object buffer
  NOTES        : Used in CL_EvaluateExpression(theEnv,)
 ****************************************************/
void
CL_CallDeffunction (Environment * theEnv,
		    Deffunction * dptr,
		    Expression * args, UDFValue * returnValue)
{
  bool oldce;
  Deffunction *previouslyExecutingDeffunction;
  GCBlock gcb;
#if PROFILING_FUNCTIONS
  struct profileFrameInfo profileFrame;
#endif

  returnValue->value = FalseSymbol (theEnv);
  CL_EvaluationData (theEnv)->CL_EvaluationError = false;
  if (CL_EvaluationData (theEnv)->CL_HaltExecution)
    return;

  CL_GCBlockStart (theEnv, &gcb);

  oldce = CL_ExecutingConstruct (theEnv);
  Set_ExecutingConstruct (theEnv, true);
  previouslyExecutingDeffunction =
    DeffunctionData (theEnv)->ExecutingDeffunction;
  DeffunctionData (theEnv)->ExecutingDeffunction = dptr;
  CL_EvaluationData (theEnv)->Current_EvaluationDepth++;
  dptr->executing++;
  CL_PushProcParameters (theEnv, args, CL_CountArguments (args),
			 CL_DeffunctionName (dptr), "deffunction",
			 UnboundDeffunctionErr);
  if (CL_EvaluationData (theEnv)->CL_EvaluationError)
    {
      dptr->executing--;
      DeffunctionData (theEnv)->ExecutingDeffunction =
	previouslyExecutingDeffunction;
      CL_EvaluationData (theEnv)->Current_EvaluationDepth--;

      CL_GCBlockEndUDF (theEnv, &gcb, returnValue);
      CL_CallPeriodicTasks (theEnv);

      Set_ExecutingConstruct (theEnv, oldce);
      return;
    }

#if DEBUGGING_FUNCTIONS
  if (dptr->trace)
    CL_WatchDeffunction (theEnv, BEGIN_TRACE);
#endif

#if PROFILING_FUNCTIONS
  Start_Profile (theEnv, &profileFrame,
		 &dptr->header.usrData,
		 CL_ProfileFunctionData (theEnv)->CL_ProfileConstructs);
#endif

  CL_EvaluateProcActions (theEnv, dptr->header.whichModule->theModule,
			  dptr->code, dptr->numberOfLocalVars,
			  returnValue, UnboundDeffunctionErr);

#if PROFILING_FUNCTIONS
  CL_End_Profile (theEnv, &profileFrame);
#endif

#if DEBUGGING_FUNCTIONS
  if (dptr->trace)
    CL_WatchDeffunction (theEnv, END_TRACE);
#endif
  ProcedureFunctionData (theEnv)->ReturnFlag = false;

  dptr->executing--;
  CL_PopProcParameters (theEnv);
  DeffunctionData (theEnv)->ExecutingDeffunction =
    previouslyExecutingDeffunction;
  CL_EvaluationData (theEnv)->Current_EvaluationDepth--;

  CL_GCBlockEndUDF (theEnv, &gcb, returnValue);
  CL_CallPeriodicTasks (theEnv);

  Set_ExecutingConstruct (theEnv, oldce);
}

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/*******************************************************
  NAME         : UnboundDeffunctionErr
  DESCRIPTION  : Print out a synopis of the currently
                   executing deffunction for unbound
                   variable errors
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Error synopsis printed to STDERR
  NOTES        : None
 *******************************************************/
static void
UnboundDeffunctionErr (Environment * theEnv, const char *logName)
{
  CL_WriteString (theEnv, logName, "deffunction '");
  CL_WriteString (theEnv, logName,
		  CL_DeffunctionName (DeffunctionData
				      (theEnv)->ExecutingDeffunction));
  CL_WriteString (theEnv, logName, "'.\n");
}

#if DEBUGGING_FUNCTIONS

/***************************************************
  NAME         : CL_WatchDeffunction
  DESCRIPTION  : Displays a message indicating when
                 a deffunction began and ended
                 execution
  INPUTS       : The beginning or end trace string
                 to print when deffunction starts
                 or finishes respectively
  RETURNS      : Nothing useful
  SIDE EFFECTS : CL_Watch message printed
  NOTES        : None
 ***************************************************/
static void
CL_WatchDeffunction (Environment * theEnv, const char *tstring)
{
  if (ConstructData (theEnv)->CL_ClearReadyInProgress ||
      ConstructData (theEnv)->CL_ClearInProgress)
    {
      return;
    }

  CL_WriteString (theEnv, STDOUT, "DFN ");
  CL_WriteString (theEnv, STDOUT, tstring);
  if (DeffunctionData (theEnv)->ExecutingDeffunction->header.
      whichModule->theModule != CL_GetCurrentModule (theEnv))
    {
      CL_WriteString (theEnv, STDOUT,
		      CL_DeffunctionModule (DeffunctionData
					    (theEnv)->ExecutingDeffunction));;
      CL_WriteString (theEnv, STDOUT, "::");
    }
  CL_WriteString (theEnv, STDOUT,
		  DeffunctionData (theEnv)->ExecutingDeffunction->
		  header.name->contents);
  CL_WriteString (theEnv, STDOUT, " ED:");
  CL_WriteInteger (theEnv, STDOUT,
		   CL_EvaluationData (theEnv)->Current_EvaluationDepth);
  CL_PrintProcParamArray (theEnv, STDOUT);
}

#endif
#endif
