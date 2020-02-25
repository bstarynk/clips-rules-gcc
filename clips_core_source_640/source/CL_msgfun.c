   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  06/22/18             */
   /*                                                     */
   /*              OBJECT MESSAGE FUNCTIONS               */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Removed IMPERATIVE_MESSAGE_HANDLERS and        */
/*            AUXILIARY_MESSAGE_HANDLERS compilation flags.  */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
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

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if OBJECT_SYSTEM

#include "classcom.h"
#include "classfun.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "inscom.h"
#include "insfun.h"
#include "memalloc.h"
#include "msgcom.h"
#include "prccode.h"
#include "prntutil.h"
#include "router.h"

#include "msgfun.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if DEBUGGING_FUNCTIONS
static HANDLER_LINK *DisplayPrimaryCore (Environment *, const char *,
					 HANDLER_LINK *, int);
static void PrintPreviewHandler (Environment *, const char *, HANDLER_LINK *,
				 int, const char *);
#endif

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/********************************************************
  NAME         : CL_UnboundHandlerErr
  DESCRIPTION  : Print out a synopis of the currently
                   executing handler for unbound variable
                   errors
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Error synopsis printed to STDERR
  NOTES        : None
 ********************************************************/
void
CL_UnboundHandlerErr (Environment * theEnv, const char *logName)
{
  CL_WriteString (theEnv, logName, "message-handler ");
  CL_PrintHandler (theEnv, logName,
		   MessageHandlerData (theEnv)->CurrentCore->hnd, true, true);
}

/*****************************************************************
  NAME         : CL_PrintNoHandlerError
  DESCRIPTION  : Print "No primaries found" error message for send
  INPUTS       : The name of the message
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 *****************************************************************/
void
CL_PrintNoHandlerError (Environment * theEnv, const char *msg)
{
  CL_PrintErrorID (theEnv, "MSGFUN", 1, false);
  CL_WriteString (theEnv, STDERR,
		  "No applicable primary message-handlers found for '");
  CL_WriteString (theEnv, STDERR, msg);
  CL_WriteString (theEnv, STDERR, "'.\n");
}

/***************************************************************
  NAME         : CL_CheckHandlerArgCount
  DESCRIPTION  : Verifies that the current argument
                   list satisfies the current
                   handler's parameter count restriction
  INPUTS       : None
  RETURNS      : True if all OK, false otherwise
  SIDE EFFECTS : CL_EvaluationError set on errors
  NOTES        : Uses ProcParamArraySize and CurrentCore globals
 ***************************************************************/
bool
CL_CheckHandlerArgCount (Environment * theEnv)
{
  DefmessageHandler *hnd;

  hnd = MessageHandlerData (theEnv)->CurrentCore->hnd;
  if ((hnd->maxParams == PARAMETERS_UNBOUNDED) ? (ProceduralPrimitiveData (theEnv)->ProcParamArraySize < hnd->minParams) :	// TBD
      (ProceduralPrimitiveData (theEnv)->ProcParamArraySize !=
       hnd->minParams))
    {
      Set_EvaluationError (theEnv, true);
      CL_PrintErrorID (theEnv, "MSGFUN", 2, false);
      CL_WriteString (theEnv, STDERR, "Message-handler '");
      CL_WriteString (theEnv, STDERR, hnd->header.name->contents);
      CL_WriteString (theEnv, STDERR, "' ");
      CL_WriteString (theEnv, STDERR,
		      MessageHandlerData (theEnv)->hndquals[hnd->type]);
      CL_WriteString (theEnv, STDERR, " in class '");
      CL_WriteString (theEnv, STDERR, CL_DefclassName (hnd->cls));
      CL_WriteString (theEnv, STDERR, "' expected ");

      if (hnd->maxParams == PARAMETERS_UNBOUNDED)
	CL_WriteString (theEnv, STDERR, "at least ");
      else
	CL_WriteString (theEnv, STDERR, "exactly ");

      CL_PrintUnsignedInteger (theEnv, STDERR, hnd->minParams - 1);
      if (hnd->minParams - 1 == 1)
	{
	  CL_WriteString (theEnv, STDERR, " argument.\n");
	}
      else
	{
	  CL_WriteString (theEnv, STDERR, " arguments.\n");
	}

      return false;
    }
  return true;
}

/***************************************************
  NAME         : CL_SlotAccessViolationError
  DESCRIPTION  : Prints out an error message when
                 attempt is made to set a read-only
                 or initialize-only slot improperly
  INPUTS       : 1) The slot name
                 2) A flag indicating if the source
                    is a class or an instance
                 3) A pointer to the source
                    instance/class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Error message printed
  NOTES        : None
 ***************************************************/
void
CL_SlotAccessViolationError (Environment * theEnv,
			     const char *slotName,
			     Instance * theInstance, Defclass * theDefclass)
{
  CL_PrintErrorID (theEnv, "MSGFUN", 3, false);
  CL_WriteString (theEnv, STDERR, "CL_Write access denied for slot '");
  CL_WriteString (theEnv, STDERR, slotName);
  CL_WriteString (theEnv, STDERR, "' in ");
  if (theInstance != NULL)
    {
      CL_WriteString (theEnv, STDERR, "instance ");
      CL_WriteString (theEnv, STDERR, "[");
      CL_WriteString (theEnv, STDERR, CL_InstanceName (theInstance));
      CL_WriteString (theEnv, STDERR, "] of class ");
      CL_PrintClassName (theEnv, STDERR, theInstance->cls, true, false);
    }
  else
    {
      CL_WriteString (theEnv, STDERR, "class ");
      CL_PrintClassName (theEnv, STDERR, theDefclass, true, false);
    }
  CL_WriteString (theEnv, STDERR, ".\n");
}

/***************************************************
  NAME         : CL_SlotVisibilityViolationError
  DESCRIPTION  : Prints out an error message when
                 attempt is made to access a
                 private slot improperly
  INPUTS       : 1) The slot descriptor
                 2) A pointer to the source class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Error message printed
  NOTES        : None
 ***************************************************/
void
CL_SlotVisibilityViolationError (Environment * theEnv,
				 SlotDescriptor * sd,
				 Defclass * theDefclass, bool printCR)
{
  CL_PrintErrorID (theEnv, "MSGFUN", 6, printCR);
  CL_WriteString (theEnv, STDERR, "Private slot '");
  CL_WriteString (theEnv, STDERR, sd->slotName->name->contents);
  CL_WriteString (theEnv, STDERR, "' of class ");
  CL_PrintClassName (theEnv, STDERR, sd->cls, true, false);
  CL_WriteString (theEnv, STDERR,
		  " cannot be accessed directly by handlers attached to class ");
  CL_PrintClassName (theEnv, STDERR, theDefclass, true, false);
  CL_WriteString (theEnv, STDERR, "\n");
}

#if ! RUN_TIME

/******************************************************************************
  NAME         : CL_NewSystemHandler
  DESCRIPTION  : Adds a new system handler for a system class

                 The handler is assumed to be primary and of
                 the fo_rm:

                 (defmessage-handler <class> <handler> () (<func>))

  INPUTS       : 1) Name-string of the system class
                 2) Name-string of the system handler
                 3) Name-string of the internal H/L function to implement
                      this handler
                 4) The number of extra arguments (past the instance itself)
                    that the handler willl accept
  RETURNS      : Nothing useful
  SIDE EFFECTS : Creates the new handler and inserts it in the system class's
                   handler array
                 On errors, generate a system error and exits.
  NOTES        : Does not check to see if handler already exists
 *******************************************************************************/
void
CL_NewSystemHandler (Environment * theEnv,
		     const char *cname,
		     const char *mname,
		     const char *fname, unsigned short extraargs)
{
  Defclass *cls;
  DefmessageHandler *hnd;

  cls = Lookup_DefclassInScope (theEnv, cname);
  hnd =
    CL_InsertHandlerHeader (theEnv, cls, CL_CreateSymbol (theEnv, mname),
			    MPRIMARY);
  IncrementLexemeCount (hnd->header.name);
  hnd->system = 1;
  hnd->minParams = hnd->maxParams = extraargs + 1;
  hnd->localVarCount = 0;
  hnd->actions = get_struct (theEnv, expr);
  hnd->actions->argList = NULL;
  hnd->actions->type = FCALL;
  hnd->actions->value = CL_FindFunction (theEnv, fname);
  hnd->actions->nextArg = NULL;
}

/***************************************************
  NAME         : CL_InsertHandlerHeader
  DESCRIPTION  : Allocates a new handler header and
                   inserts it in the proper (sorted)
                   position in the class hnd array
  INPUTS       : 1) The class
                 2) The handler name
                 3) The handler type
  RETURNS      : The address of the new handler
                   header, NULL on errors
  SIDE EFFECTS : Class handler array reallocated
                   and resorted
  NOTES        : Assumes handler does not exist
 ***************************************************/
DefmessageHandler *
CL_InsertHandlerHeader (Environment * theEnv,
			Defclass * cls, CLIPSLexeme * mname, unsigned mtype)
{
  DefmessageHandler *nhnd, *hnd;
  unsigned *narr, *arr;
  long i;
  long j, ni = -1;

  hnd = cls->handlers;
  arr = cls->handlerOrderMap;
  nhnd =
    (DefmessageHandler *) CL_gm2 (theEnv,
				  (sizeof (DefmessageHandler) *
				   (cls->handlerCount + 1)));
  narr =
    (unsigned *) CL_gm2 (theEnv,
			 (sizeof (unsigned) * (cls->handlerCount + 1)));
  GenCopyMemory (DefmessageHandler, cls->handlerCount, nhnd, hnd);
  for (i = 0, j = 0; i < cls->handlerCount; i++, j++)
    {
      if (ni == -1)
	{
	  if ((hnd[arr[i]].header.name->bucket > mname->bucket) ? true :
	      (hnd[arr[i]].header.name == mname))
	    {
	      ni = i;
	      j++;
	    }
	}
      narr[j] = arr[i];
    }
  if (ni == -1)
    ni = (int) cls->handlerCount;
  narr[ni] = cls->handlerCount;
  nhnd[cls->handlerCount].system = 0;
  nhnd[cls->handlerCount].type = mtype;
  nhnd[cls->handlerCount].busy = 0;
  nhnd[cls->handlerCount].mark = 0;
#if DEBUGGING_FUNCTIONS
  nhnd[cls->handlerCount].trace =
    MessageHandlerData (theEnv)->CL_WatchHandlers;
#endif
  nhnd[cls->handlerCount].header.name = mname;
  nhnd[cls->handlerCount].header.whichModule = cls->header.whichModule;
  nhnd[cls->handlerCount].header.next = NULL;
  nhnd[cls->handlerCount].cls = cls;
  nhnd[cls->handlerCount].minParams = 0;
  nhnd[cls->handlerCount].maxParams = 0;
  nhnd[cls->handlerCount].localVarCount = 0;
  nhnd[cls->handlerCount].actions = NULL;
  nhnd[cls->handlerCount].header.ppFo_rm = NULL;
  nhnd[cls->handlerCount].header.usrData = NULL;
  nhnd[cls->handlerCount].header.constructType = DEFMESSAGE_HANDLER;
  nhnd[cls->handlerCount].header.env = theEnv;
  if (cls->handlerCount != 0)
    {
      CL_rm (theEnv, hnd, (sizeof (DefmessageHandler) * cls->handlerCount));
      CL_rm (theEnv, arr, (sizeof (unsigned) * cls->handlerCount));
    }
  cls->handlers = nhnd;
  cls->handlerOrderMap = narr;
  cls->handlerCount++;
  return (&nhnd[cls->handlerCount - 1]);
}

#endif

#if (! BLOAD_ONLY) && (! RUN_TIME)

/*****************************************************
  NAME         : CL_HandlersExecuting
  DESCRIPTION  : Dete_rmines if any message-handlers
                   for a class are currently executing
  INPUTS       : The class address
  RETURNS      : True if any handlers are executing,
                   false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 *****************************************************/
bool
CL_HandlersExecuting (Defclass * cls)
{
  long i;

  for (i = 0; i < cls->handlerCount; i++)
    if (cls->handlers[i].busy > 0)
      return true;
  return false;
}

/*********************************************************************
  NAME         : CL_DeleteHandler
  DESCRIPTION  : Deletes one or more message-handlers
                   from a class definition
  INPUTS       : 1) The class address
                 2) The message-handler name
                    (if this is * and there is no handler
                     called *, then the delete operations
                     will be applied to all handlers matching the type
                 3) The message-handler type
                    (if this is -1, then the delete operations will be
                     applied to all handlers matching the name
                 4) A flag saying whether to print error messages when
                     handlers are not found meeting specs
  RETURNS      : 1 if successful, 0 otherwise
  SIDE EFFECTS : Handlers deleted
  NOTES        : If any handlers for the class are
                   currently executing, this routine
                   will fail
 **********************************************************************/
bool
CL_DeleteHandler (Environment * theEnv,
		  Defclass * cls,
		  CLIPSLexeme * mname, int mtype, bool indicate_missing)
{
  long i;
  DefmessageHandler *hnd;
  bool found, success = true;

  if (cls->handlerCount == 0)
    {
      if (indicate_missing)
	{
	  CL_HandlerDeleteError (theEnv, CL_DefclassName (cls));
	  return false;
	}
      return true;
    }
  if (CL_HandlersExecuting (cls))
    {
      CL_HandlerDeleteError (theEnv, CL_DefclassName (cls));
      return false;
    }
  if (mtype == -1)
    {
      found = false;
      for (i = MAROUND; i <= MAFTER; i++)
	{
	  hnd = CL_FindHandlerByAddress (cls, mname, (unsigned) i);
	  if (hnd != NULL)
	    {
	      found = true;
	      if (hnd->system == 0)
		hnd->mark = 1;
	      else
		{
		  CL_PrintErrorID (theEnv, "MSGPSR", 3, false);
		  CL_WriteString (theEnv, STDERR,
				  "System message-handlers may not be modified.\n");
		  success = false;
		}
	    }
	}
      if ((found == false) ? (strcmp (mname->contents, "*") == 0) : false)
	{
	  for (i = 0; i < cls->handlerCount; i++)
	    if (cls->handlers[i].system == 0)
	      cls->handlers[i].mark = 1;
	}
    }
  else
    {
      hnd = CL_FindHandlerByAddress (cls, mname, (unsigned) mtype);
      if (hnd == NULL)
	{
	  if (strcmp (mname->contents, "*") == 0)
	    {
	      for (i = 0; i < cls->handlerCount; i++)
		if ((cls->handlers[i].type == (unsigned) mtype) &&
		    (cls->handlers[i].system == 0))
		  cls->handlers[i].mark = 1;
	    }
	  else
	    {
	      if (indicate_missing)
		CL_HandlerDeleteError (theEnv, CL_DefclassName (cls));
	      success = false;
	    }
	}
      else if (hnd->system == 0)
	hnd->mark = 1;
      else
	{
	  if (indicate_missing)
	    {
	      CL_PrintErrorID (theEnv, "MSGPSR", 3, false);
	      CL_WriteString (theEnv, STDERR,
			      "System message-handlers may not be modified.\n");
	    }
	  success = false;
	}
    }
  CL_DeallocateMarkedHandlers (theEnv, cls);
  return (success);
}

/***************************************************
  NAME         : CL_DeallocateMarkedHandlers
  DESCRIPTION  : Removes any handlers from a class
                   that have been previously marked
                   for deletion.
  INPUTS       : The class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Marked handlers are deleted
  NOTES        : Assumes none of the handlers are
                   currently executing or have a
                   busy count != 0 for any reason
 ***************************************************/
void
CL_DeallocateMarkedHandlers (Environment * theEnv, Defclass * cls)
{
  unsigned short count;
  DefmessageHandler *hnd, *nhnd;
  unsigned *arr, *narr;
  long i, j;

  for (i = 0, count = 0; i < cls->handlerCount; i++)
    {
      hnd = &cls->handlers[i];
      if (hnd->mark == 1)
	{
	  count++;
	  CL_ReleaseLexeme (theEnv, hnd->header.name);
	  CL_ExpressionDeinstall (theEnv, hnd->actions);
	  CL_ReturnPackedExpression (theEnv, hnd->actions);
	  CL_ClearUserDataList (theEnv, hnd->header.usrData);
	  if (hnd->header.ppFo_rm != NULL)
	    CL_rm (theEnv, (void *) hnd->header.ppFo_rm,
		   (sizeof (char) * (strlen (hnd->header.ppFo_rm) + 1)));
	}
      else
	/* ============================================
	   Use the busy field to count how many
	   message-handlers are removed before this one
	   ============================================ */
	hnd->busy = count;
    }
  if (count == 0)
    return;
  if (count == cls->handlerCount)
    {
      CL_rm (theEnv, cls->handlers,
	     (sizeof (DefmessageHandler) * cls->handlerCount));
      CL_rm (theEnv, cls->handlerOrderMap,
	     (sizeof (unsigned) * cls->handlerCount));
      cls->handlers = NULL;
      cls->handlerOrderMap = NULL;
      cls->handlerCount = 0;
    }
  else
    {
      count = (cls->handlerCount - count);
      hnd = cls->handlers;
      arr = cls->handlerOrderMap;
      nhnd =
	(DefmessageHandler *) CL_gm2 (theEnv,
				      (sizeof (DefmessageHandler) * count));
      narr = (unsigned *) CL_gm2 (theEnv, (sizeof (unsigned) * count));
      for (i = 0, j = 0; j < count; i++)
	{
	  if (hnd[arr[i]].mark == 0)
	    {
	      /* ==============================================================
	         The offsets in the map need to be decremented by the number of
	         preceding nodes which were deleted.  Use the value of the busy
	         field set in the first loop.
	         ============================================================== */
	      narr[j] = arr[i] - hnd[arr[i]].busy;
	      j++;
	    }
	}
      for (i = 0, j = 0; j < count; i++)
	{
	  if (hnd[i].mark == 0)
	    {
	      hnd[i].busy = 0;
	      GenCopyMemory (DefmessageHandler, 1, &nhnd[j], &hnd[i]);
	      j++;
	    }
	}
      CL_rm (theEnv, hnd, (sizeof (DefmessageHandler) * cls->handlerCount));
      CL_rm (theEnv, arr, (sizeof (unsigned) * cls->handlerCount));
      cls->handlers = nhnd;
      cls->handlerOrderMap = narr;
      cls->handlerCount = count;
    }
}

#endif

/*****************************************************
  NAME         : CL_HandlerType
  DESCRIPTION  : Dete_rmines type of message-handler
  INPUTS       : 1) Calling function string
                 2) String representing type
  RETURNS      : MAROUND  (0) for "around"
                 MBEFORE  (1) for "before"
                 MPRIMARY (2) for "primary"
                 MAFTER   (3) for "after"
                 MERROR   (4) on errors
  SIDE EFFECTS : None
  NOTES        : None
 *****************************************************/
unsigned short
CL_HandlerType (Environment * theEnv,
		const char *where, bool whereIsFunction, const char *str)
{
  unsigned short i;

  for (i = MAROUND; i <= MAFTER; i++)
    {
      if (strcmp (str, MessageHandlerData (theEnv)->hndquals[i]) == 0)
	{
	  return i;
	}
    }

  CL_PrintErrorID (theEnv, "MSGFUN", 7, false);
  CL_WriteString (theEnv, STDERR, "Unrecognized message-handler type in ");
  if (whereIsFunction)
    {
      CL_WriteString (theEnv, STDERR, "function '");
      CL_WriteString (theEnv, STDERR, where);
      CL_WriteString (theEnv, STDERR, "'.");
    }
  else
    {
      CL_WriteString (theEnv, STDERR, where);
    }

  CL_WriteString (theEnv, STDERR, ".\n");

  return (MERROR);
}

/*****************************************************************
  NAME         : CL_CheckCurrentMessage
  DESCRIPTION  : Makes sure that a message is available
                   and active for an internal message function
  INPUTS       : 1) The name of the function checking the message
                 2) A flag indicating whether the object must be
                      a class instance or not (it could be a
                      primitive type)
  RETURNS      : True if all OK, false otherwise
  SIDE EFFECTS : CL_EvaluationError set on errors
  NOTES        : None
 *****************************************************************/
bool
CL_CheckCurrentMessage (Environment * theEnv, const char *func, bool ins_reqd)
{
  UDFValue *activeMsgArg;

  if (!MessageHandlerData (theEnv)->CurrentCore
      || (MessageHandlerData (theEnv)->CurrentCore->hnd->actions !=
	  ProceduralPrimitiveData (theEnv)->CurrentProcActions))
    {
      CL_PrintErrorID (theEnv, "MSGFUN", 4, false);
      CL_WriteString (theEnv, STDERR, "The function '");
      CL_WriteString (theEnv, STDERR, func);
      CL_WriteString (theEnv, STDERR,
		      "' may only be called from within message-handlers.\n");
      Set_EvaluationError (theEnv, true);
      return false;
    }
  activeMsgArg = CL_GetNthMessageArgument (theEnv, 0);
  if ((ins_reqd == true) ? (activeMsgArg->header->type !=
			    INSTANCE_ADDRESS_TYPE) : false)
    {
      CL_PrintErrorID (theEnv, "MSGFUN", 5, false);
      CL_WriteString (theEnv, STDERR, "The function '");
      CL_WriteString (theEnv, STDERR, func);
      CL_WriteString (theEnv, STDERR, "' operates only on instances.\n");
      Set_EvaluationError (theEnv, true);
      return false;
    }
  if ((activeMsgArg->header->type == INSTANCE_ADDRESS_TYPE) ?
      (activeMsgArg->instanceValue->garbage == 1) : false)
    {
      CL_StaleInstanceAddress (theEnv, func, 0);
      Set_EvaluationError (theEnv, true);
      return false;
    }
  return true;
}

/***************************************************
  NAME         : CL_PrintHandler
  DESCRIPTION  : Displays a handler synopsis
  INPUTS       : 1) Logical name of output
                 2) The handler
                 5) Flag indicating whether to
                    printout a te_rminating newline
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
void
CL_PrintHandler (Environment * theEnv,
		 const char *logName,
		 DefmessageHandler * theHandler, bool useQuotes, bool crtn)
{
  if (useQuotes)
    CL_WriteString (theEnv, logName, "'");
  CL_WriteString (theEnv, logName, theHandler->header.name->contents);
  if (useQuotes)
    CL_WriteString (theEnv, logName, "'");
  CL_WriteString (theEnv, logName, " ");
  CL_WriteString (theEnv, logName,
		  MessageHandlerData (theEnv)->hndquals[theHandler->type]);
  CL_WriteString (theEnv, logName, " in class ");
  CL_PrintClassName (theEnv, logName, theHandler->cls, useQuotes, crtn);
}

/***********************************************************
  NAME         : CL_FindHandlerByAddress
  DESCRIPTION  : Uses a binary search on a class's
                   handler header array
  INPUTS       : 1) The class address
                 2) The handler symbolic name
                 3) The handler type (MPRIMARY,etc.)
  RETURNS      : The address of the found handler,
                   NULL if not found
  SIDE EFFECTS : None
  NOTES        : Assumes array is in ascending order
                   1st key: symbolic name of handler
                   2nd key: type of handler
 ***********************************************************/
DefmessageHandler *
CL_FindHandlerByAddress (Defclass * cls, CLIPSLexeme * name, unsigned type)
{
  int b;
  long i;
  DefmessageHandler *hnd;
  unsigned *arr;

  if ((b = CL_FindHandlerNameGroup (cls, name)) == -1)
    return NULL;
  arr = cls->handlerOrderMap;
  hnd = cls->handlers;
  for (i = (unsigned) b; i < cls->handlerCount; i++)
    {
      if (hnd[arr[i]].header.name != name)
	return NULL;
      if (hnd[arr[i]].type == type)
	return (&hnd[arr[i]]);
    }
  return NULL;
}

/***********************************************************
  NAME         : CL_FindHandlerByIndex
  DESCRIPTION  : Uses a binary search on a class's
                   handler header array
  INPUTS       : 1) The class address
                 2) The handler symbolic name
                 3) The handler type (MPRIMARY,etc.)
  RETURNS      : The index of the found handler,
                   -1 if not found
  SIDE EFFECTS : None
  NOTES        : Assumes array is in ascending order
                   1st key: symbolic name of handler
                   2nd key: type of handler
 ***********************************************************/
int
CL_FindHandlerByIndex (Defclass * cls, CLIPSLexeme * name, unsigned type)
{
  int b;
  long i;
  DefmessageHandler *hnd;
  unsigned *arr;

  if ((b = CL_FindHandlerNameGroup (cls, name)) == -1)
    return -1;
  arr = cls->handlerOrderMap;
  hnd = cls->handlers;
  for (i = (unsigned) b; i < cls->handlerCount; i++)
    {
      if (hnd[arr[i]].header.name != name)
	return -1;
      if (hnd[arr[i]].type == type)
	return (int) arr[i];
    }
  return -1;
}

/*****************************************************
  NAME         : CL_FindHandlerNameGroup
  DESCRIPTION  : Uses a binary search on a class's
                   handler header array
  INPUTS       : 1) The class address
                 2) The handler symbolic name
  RETURNS      : The index of the found handler group
                   -1 if not found
  SIDE EFFECTS : None
  NOTES        : Assumes array is in ascending order
                   1st key: handler name symbol bucket
 *****************************************************/
int
CL_FindHandlerNameGroup (Defclass * cls, CLIPSLexeme * name)
{
  int b, e, i, j;
  DefmessageHandler *hnd;
  unsigned *arr;
  int start;

  if (cls->handlerCount == 0)
    return (-1);
  hnd = cls->handlers;
  arr = cls->handlerOrderMap;
  b = 0;
  e = (int) (cls->handlerCount - 1);
  start = -1;
  do
    {
      i = (b + e) / 2;
      if (name->bucket == hnd[arr[i]].header.name->bucket)
	{
	  for (j = i; j >= b; j--)
	    {
	      if (hnd[arr[j]].header.name == name)
		start = j;
	      if (hnd[arr[j]].header.name->bucket != name->bucket)
		break;
	    }
	  if (start != -1)
	    return (start);
	  for (j = i + 1; j <= e; j++)
	    {
	      if (hnd[arr[j]].header.name == name)
		return (j);
	      if (hnd[arr[j]].header.name->bucket != name->bucket)
		return (-1);
	    }
	  return (-1);
	}
      else if (name->bucket < hnd[arr[i]].header.name->bucket)
	e = i - 1;
      else
	b = i + 1;
    }
  while (b <= e);
  return (-1);
}

/***************************************************
  NAME         : CL_HandlerDeleteError
  DESCRIPTION  : Prints out an error message when
                   handlers cannot be deleted
  INPUTS       : Name-string of the class
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
void
CL_HandlerDeleteError (Environment * theEnv, const char *cname)
{
  CL_PrintErrorID (theEnv, "MSGFUN", 8, false);
  CL_WriteString (theEnv, STDERR,
		  "Unable to delete message-handler(s) from class '");
  CL_WriteString (theEnv, STDERR, cname);
  CL_WriteString (theEnv, STDERR, "'.\n");
}

#if DEBUGGING_FUNCTIONS

/********************************************************************
  NAME         : CL_DisplayCore
  DESCRIPTION  : Gives a schematic "printout" of the
                   core framework for a message showing
                   arounds, primaries, shadows etc.
                 This routine uses recursion to print indentation
                   to indicate shadowing and where handlers begin
                   and end execution wrt one another.
  INPUTS       : 1) Logical name of output
                 2) The re_maining core
                 3) The number of handlers this (partial) core
                    shadows
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : Expects that the core was created in PREVIEW mode,
                   i.e. implicit handlers are SlotDescriptor addresses
                   (in PERFORM mode they are INSTANCE_SLOT addresses)
                 Assumes (partial) core is not empty
 ********************************************************************/
void
CL_DisplayCore (Environment * theEnv,
		const char *logicalName, HANDLER_LINK * core, int sdepth)
{
  if (core->hnd->type == MAROUND)
    {
      PrintPreviewHandler (theEnv, logicalName, core, sdepth, BEGIN_TRACE);
      if (core->nxt != NULL)
	CL_DisplayCore (theEnv, logicalName, core->nxt, sdepth + 1);
      PrintPreviewHandler (theEnv, logicalName, core, sdepth, END_TRACE);
    }
  else
    {
      while ((core != NULL) ? (core->hnd->type == MBEFORE) : false)
	{
	  PrintPreviewHandler (theEnv, logicalName, core, sdepth,
			       BEGIN_TRACE);
	  PrintPreviewHandler (theEnv, logicalName, core, sdepth, END_TRACE);
	  core = core->nxt;
	}
      if ((core != NULL) ? (core->hnd->type == MPRIMARY) : false)

	core = DisplayPrimaryCore (theEnv, logicalName, core, sdepth);

      while ((core != NULL) ? (core->hnd->type == MAFTER) : false)
	{
	  PrintPreviewHandler (theEnv, logicalName, core, sdepth,
			       BEGIN_TRACE);
	  PrintPreviewHandler (theEnv, logicalName, core, sdepth, END_TRACE);
	  core = core->nxt;
	}

    }
}

/*******************************************************************
  NAME         : CL_FindPreviewApplicableHandlers
  DESCRIPTION  : See FindApplicableHandlers
                 However, this function only examines classes rather
                   than instances for implicit slot-accessors
  INPUTS       : 1) The class address
                 2) The message name symbol
  RETURNS      : The links of applicable handlers, NULL on errors
  SIDE EFFECTS : Links are allocated for the list
  NOTES        : None
 ******************************************************************/
HANDLER_LINK *
CL_FindPreviewApplicableHandlers (Environment * theEnv,
				  Defclass * cls, CLIPSLexeme * mname)
{
  unsigned int i;
  HANDLER_LINK *tops[4], *bots[4];

  for (i = MAROUND; i <= MAFTER; i++)
    tops[i] = bots[i] = NULL;

  for (i = 0; i < cls->allSuperclasses.classCount; i++)
    CL_FindApplicableOfName (theEnv, cls->allSuperclasses.classArray[i], tops,
			     bots, mname);

  return CL_JoinHandlerLinks (theEnv, tops, bots, mname);
}

/***********************************************************
  NAME         : CL_WatchMessage
  DESCRIPTION  : Prints a condensed description of a
                   message and its arguments
  INPUTS       : 1) The output logical name
                 2) BEGIN_TRACE or END_TRACE string
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : Uses the global variables ProcParamArray
                   and CurrentMessageName
 ***********************************************************/
void
CL_WatchMessage (Environment * theEnv,
		 const char *logName, const char *tstring)
{
  if (ConstructData (theEnv)->CL_ClearReadyInProgress ||
      ConstructData (theEnv)->CL_ClearInProgress)
    {
      return;
    }

  CL_WriteString (theEnv, logName, "MSG ");
  CL_WriteString (theEnv, logName, tstring);
  CL_WriteString (theEnv, logName, " ");
  CL_WriteString (theEnv, logName,
		  MessageHandlerData (theEnv)->CurrentMessageName->contents);
  CL_WriteString (theEnv, logName, " ED:");
  CL_WriteInteger (theEnv, logName,
		   CL_EvaluationData (theEnv)->Current_EvaluationDepth);
  CL_PrintProcParamArray (theEnv, logName);
}

/***********************************************************
  NAME         : CL_WatchHandler
  DESCRIPTION  : Prints a condensed description of a
                   message-handler and its arguments
  INPUTS       : 1) The output logical name
                 2) The handler address
                 3) BEGIN_TRACE or END_TRACE string
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : Uses the global variables ProcParamArray
                   and CurrentMessageName
 ***********************************************************/
void
CL_WatchHandler (Environment * theEnv,
		 const char *logName,
		 HANDLER_LINK * hndl, const char *tstring)
{
  DefmessageHandler *hnd;

  if (ConstructData (theEnv)->CL_ClearReadyInProgress ||
      ConstructData (theEnv)->CL_ClearInProgress)
    {
      return;
    }

  CL_WriteString (theEnv, logName, "HND ");
  CL_WriteString (theEnv, logName, tstring);
  CL_WriteString (theEnv, logName, " ");
  hnd = hndl->hnd;
  CL_PrintHandler (theEnv, logName, hnd, false, true);
  CL_WriteString (theEnv, logName, "       ED:");
  CL_WriteInteger (theEnv, logName,
		   CL_EvaluationData (theEnv)->Current_EvaluationDepth);
  CL_PrintProcParamArray (theEnv, logName);
}

#endif /* DEBUGGING_FUNCTIONS */

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if DEBUGGING_FUNCTIONS

/********************************************************************
  NAME         : DisplayPrimaryCore
  DESCRIPTION  : Gives a schematic "printout" of the primary
                   message showing other shadowed primaries
                 This routine uses recursion to print indentation
                   to indicate shadowing and where handlers begin
                   and end execution wrt one another.
  INPUTS       : 1) The logical name of the output
                 2) The re_maining core
                 3) The number of handlers this (partial) core
                    shadows
  RETURNS      : The address of the handler following the primary
                   group of handlers in the core
  SIDE EFFECTS : None
  NOTES        : Expects that the core was created in PREVIEW mode,
                   i.e. implicit handlers are SlotDescriptor addresses
                   (in PERFORM mode they are INSTANCE_SLOT addresses)
                 Assumes (partial) core is not empty
 ********************************************************************/
static HANDLER_LINK *
DisplayPrimaryCore (Environment * theEnv,
		    const char *logicalName, HANDLER_LINK * core, int pdepth)
{
  HANDLER_LINK *rtn;

  PrintPreviewHandler (theEnv, logicalName, core, pdepth, BEGIN_TRACE);
  if ((core->nxt != NULL) ? (core->nxt->hnd->type == MPRIMARY) : false)
    rtn = DisplayPrimaryCore (theEnv, logicalName, core->nxt, pdepth + 1);
  else
    rtn = core->nxt;
  PrintPreviewHandler (theEnv, logicalName, core, pdepth, END_TRACE);
  return (rtn);
}

/***************************************************
  NAME         : PrintPreviewHandler
  DESCRIPTION  : Displays a message preview
  INPUTS       : 1) The logical name of the output
                 2) Handler-link
                 3) Number of handlers shadowed
                 4) The trace-string
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
static void
PrintPreviewHandler (Environment * theEnv,
		     const char *logicalName,
		     HANDLER_LINK * cptr, int sdepth, const char *tstr)
{
  int i;

  for (i = 0; i < sdepth; i++)
    CL_WriteString (theEnv, logicalName, "| ");
  CL_WriteString (theEnv, logicalName, tstr);
  CL_WriteString (theEnv, logicalName, " ");
  CL_PrintHandler (theEnv, logicalName, cptr->hnd, false, true);
}

#endif

#endif
