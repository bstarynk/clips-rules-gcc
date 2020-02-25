   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Generic Functions Internal Routines              */
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
/*      6.24: Removed IMPERATIVE_METHODS compilation flag.   */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Fixed linkage issue when DEBUGGING_FUNCTIONS   */
/*            is set to 0 and PROFILING_FUNCTIONS is set to  */
/*            1.                                             */
/*                                                           */
/*            Fixed typing issue when OBJECT_SYSTEM          */
/*            compiler flag is set to 0.                     */
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

#if DEFGENERIC_CONSTRUCT

#if BLOAD || BLOAD_AND_BSAVE
#include "bload.h"
#endif

#if OBJECT_SYSTEM
#include "classcom.h"
#include "classfun.h"
#endif

#include "argacces.h"
#include "constrct.h"
#include "cstrccom.h"
#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "genrccom.h"
#include "genrcexe.h"
#include "genrcfun.h"
#include "memalloc.h"
#include "modulutl.h"
#include "prccode.h"
#include "prntutil.h"
#include "router.h"
#include "sysdep.h"

#include "genrcfun.h"

/* =========================================
   *****************************************
      INTERNALLY VISIBLE FUNCTION HEADERS
   =========================================
   ***************************************** */

#if DEBUGGING_FUNCTIONS
static void DisplayGenericCore (Environment *, Defgeneric *);
#endif

#if RUN_TIME
static void CL_RuntimeDefgenericAction (Environment *, ConstructHeader *,
					void *);
#endif

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if ! RUN_TIME

/***************************************************
  NAME         : CL_ClearDefgenericsReady
  DESCRIPTION  : Dete_rmines if it is safe to
                 remove all defgenerics
                 Assumes *all* constructs will be
                 deleted - only checks to see if
                 any methods are currently
                 executing
  INPUTS       : None
  RETURNS      : True if no methods are
                 executing, false otherwise
  SIDE EFFECTS : None
  NOTES        : Used by (clear) and (bload)
 ***************************************************/
bool
CL_ClearDefgenericsReady (Environment * theEnv, void *context)
{
  return ((DefgenericData (theEnv)->CurrentGeneric != NULL) ? false : true);
}

/*****************************************************
  NAME         : CL_Allocate_DefgenericModule
  DESCRIPTION  : Creates and initializes a
                 list of defgenerics for a new module
  INPUTS       : None
  RETURNS      : The new deffunction module
  SIDE EFFECTS : Deffunction module created
  NOTES        : None
 *****************************************************/
void *
CL_Allocate_DefgenericModule (Environment * theEnv)
{
  return (void *) get_struct (theEnv, defgenericModule);
}

/***************************************************
  NAME         : Free_DefgenericModule
  DESCRIPTION  : Removes a deffunction module and
                 all associated deffunctions
  INPUTS       : The deffunction module
  RETURNS      : Nothing useful
  SIDE EFFECTS : Module and deffunctions deleted
  NOTES        : None
 ***************************************************/
void
Free_DefgenericModule (Environment * theEnv, void *theItem)
{
#if (! BLOAD_ONLY)
  CL_FreeConstructHeaderModule (theEnv,
				(struct defmoduleItemHeader *) theItem,
				DefgenericData (theEnv)->DefgenericConstruct);
#endif
  rtn_struct (theEnv, defgenericModule, theItem);
}

#endif

#if RUN_TIME

/*************************************************/
/* CL_RuntimeDefgenericAction: Action to be applied */
/*   to each deffacts construct when a runtime   */
/*   initialization occurs.                      */
/*************************************************/
static void
CL_RuntimeDefgenericAction (Environment * theEnv,
			    ConstructHeader * theConstruct, void *buffer)
{
#if MAC_XCD
#pragma unused(buffer)
#endif
  Defgeneric *theDefgeneric = (Defgeneric *) theConstruct;
  long gi;

  theDefgeneric->header.env = theEnv;

  for (gi = 0; gi < theDefgeneric->mcnt; gi++)
    {
      theDefgeneric->methods[gi].header.env = theEnv;
    }
}

/********************************/
/* Defgeneric_RunTimeInitialize: */
/********************************/
void
Defgeneric_RunTimeInitialize (Environment * theEnv)
{
  CL_DoForAllConstructs (theEnv, CL_RuntimeDefgenericAction,
			 DefgenericData (theEnv)->CL_DefgenericModuleIndex,
			 true, NULL);
}

#endif

#if (! BLOAD_ONLY) && (! RUN_TIME)

/************************************************************
  NAME         : CL_ClearDefmethods
  DESCRIPTION  : Deletes all defmethods - generic headers
                   are left intact
  INPUTS       : None
  RETURNS      : True if all methods deleted, false otherwise
  SIDE EFFECTS : Defmethods deleted
  NOTES        : CL_Clearing generic functions is done in
                   two stages

                 1) Delete all methods (to clear any
                    references to other constructs)
                 2) Delete all generic headers

                 This allows other constructs which
                   mutually refer to generic functions
                   to be cleared
 ************************************************************/
bool
CL_ClearDefmethods (Environment * theEnv)
{
  Defgeneric *gfunc;
  bool success = true;

#if BLOAD || BLOAD_AND_BSAVE
  if (CL_Bloaded (theEnv) == true)
    return false;
#endif

  gfunc = CL_GetNextDefgeneric (theEnv, NULL);
  while (gfunc != NULL)
    {
      if (CL_RemoveAllExplicitMethods (theEnv, gfunc) == false)
	success = false;
      gfunc = CL_GetNextDefgeneric (theEnv, gfunc);
    }
  return success;
}

/*****************************************************************
  NAME         : CL_RemoveAllExplicitMethods
  DESCRIPTION  : Deletes all explicit defmethods - generic headers
                   are left intact (as well as a method for an
                   overloaded system function)
  INPUTS       : None
  RETURNS      : True if all methods deleted, false otherwise
  SIDE EFFECTS : Explicit defmethods deleted
  NOTES        : None
 *****************************************************************/
bool
CL_RemoveAllExplicitMethods (Environment * theEnv, Defgeneric * gfunc)
{
  unsigned short i, j;
  unsigned short systemMethodCount = 0;
  Defmethod *narr;

  if (CL_MethodsExecuting (gfunc) == false)
    {
      for (i = 0; i < gfunc->mcnt; i++)
	{
	  if (gfunc->methods[i].system)
	    systemMethodCount++;
	  else
	    CL_DeleteMethodInfo (theEnv, gfunc, &gfunc->methods[i]);
	}
      if (systemMethodCount != 0)
	{
	  narr =
	    (Defmethod *) CL_gm2 (theEnv,
				  (systemMethodCount * sizeof (Defmethod)));
	  i = 0;
	  j = 0;
	  while (i < gfunc->mcnt)
	    {
	      if (gfunc->methods[i].system)
		GenCopyMemory (Defmethod, 1, &narr[j++], &gfunc->methods[i]);
	      i++;
	    }
	  CL_rm (theEnv, gfunc->methods, (sizeof (Defmethod) * gfunc->mcnt));
	  gfunc->mcnt = systemMethodCount;
	  gfunc->methods = narr;
	}
      else
	{
	  if (gfunc->mcnt != 0)
	    CL_rm (theEnv, gfunc->methods,
		   (sizeof (Defmethod) * gfunc->mcnt));
	  gfunc->mcnt = 0;
	  gfunc->methods = NULL;
	}
      return true;
    }
  return false;
}

/**************************************************
  NAME         : CL_RemoveDefgeneric
  DESCRIPTION  : Removes a generic function node
                   from the generic list along with
                   all its methods
  INPUTS       : The generic function
  RETURNS      : Nothing useful
  SIDE EFFECTS : List adjusted
                 Nodes deallocated
  NOTES        : Assumes generic is not in use!!!
 **************************************************/
void
CL_RemoveDefgeneric (Environment * theEnv, Defgeneric * theDefgeneric)
{
  long i;

  for (i = 0; i < theDefgeneric->mcnt; i++)
    CL_DeleteMethodInfo (theEnv, theDefgeneric, &theDefgeneric->methods[i]);

  if (theDefgeneric->mcnt != 0)
    {
      CL_rm (theEnv, theDefgeneric->methods,
	     (sizeof (Defmethod) * theDefgeneric->mcnt));
    }
  CL_ReleaseLexeme (theEnv, Get_DefgenericNamePointer (theDefgeneric));
  SetCL_DefgenericPPFo_rm (theEnv, theDefgeneric, NULL);
  CL_ClearUserDataList (theEnv, theDefgeneric->header.usrData);
  rtn_struct (theEnv, defgeneric, theDefgeneric);
}

/****************************************************************
  NAME         : CL_ClearDefgenerics
  DESCRIPTION  : Deletes all generic headers
  INPUTS       : None
  RETURNS      : True if all methods deleted, false otherwise
  SIDE EFFECTS : Generic headers deleted (and any implicit system
                  function methods)
  NOTES        : None
 ****************************************************************/
bool
CL_ClearDefgenerics (Environment * theEnv)
{
  Defgeneric *gfunc, *gtmp;
  bool success = true;

#if BLOAD || BLOAD_AND_BSAVE
  if (CL_Bloaded (theEnv) == true)
    return false;
#endif

  gfunc = CL_GetNextDefgeneric (theEnv, NULL);
  while (gfunc != NULL)
    {
      gtmp = gfunc;
      gfunc = CL_GetNextDefgeneric (theEnv, gfunc);
      if (CL_RemoveAllExplicitMethods (theEnv, gtmp) == false)
	{
	  CL_CantDeleteItemErrorMessage (theEnv, "generic function",
					 CL_DefgenericName (gtmp));
	  success = false;
	}
      else
	{
	  CL_RemoveConstructFromModule (theEnv, &gtmp->header);
	  CL_RemoveDefgeneric (theEnv, gtmp);
	}
    }
  return (success);
}

/********************************************************
  NAME         : CL_MethodAlterError
  DESCRIPTION  : Prints out an error message reflecting
                   that a generic function's methods
                   cannot be altered while any of them
                   are executing
  INPUTS       : The generic function
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ********************************************************/
void
CL_MethodAlterError (Environment * theEnv, Defgeneric * gfunc)
{
  CL_PrintErrorID (theEnv, "GENRCFUN", 1, false);
  CL_WriteString (theEnv, STDERR, "Defgeneric '");
  CL_WriteString (theEnv, STDERR, CL_DefgenericName (gfunc));
  CL_WriteString (theEnv, STDERR,
		  "' cannot be modified while one of its methods is executing.\n");
}

/***************************************************
  NAME         : CL_DeleteMethodInfo
  DESCRIPTION  : Deallocates all the data associated
                  w/ a method but does not release
                  the method structure itself
  INPUTS       : 1) The generic function address
                 2) The method address
  RETURNS      : Nothing useful
  SIDE EFFECTS : Nodes deallocated
  NOTES        : None
 ***************************************************/
void
CL_DeleteMethodInfo (Environment * theEnv,
		     Defgeneric * gfunc, Defmethod * meth)
{
  short j, k;
  RESTRICTION *rptr;

  CL_SaveBusyCount (gfunc);
  CL_ExpressionDeinstall (theEnv, meth->actions);
  CL_ReturnPackedExpression (theEnv, meth->actions);
  CL_ClearUserDataList (theEnv, meth->header.usrData);
  if (meth->header.ppFo_rm != NULL)
    CL_rm (theEnv, (void *) meth->header.ppFo_rm,
	   (sizeof (char) * (strlen (meth->header.ppFo_rm) + 1)));
  for (j = 0; j < meth->restrictionCount; j++)
    {
      rptr = &meth->restrictions[j];

      for (k = 0; k < rptr->tcnt; k++)
#if OBJECT_SYSTEM
	CL_DecrementDefclassBusyCount (theEnv, (Defclass *) rptr->types[k]);
#else
	DecrementIntegerCount (theEnv, (CLIPSInteger *) rptr->types[k]);
#endif

      if (rptr->types != NULL)
	CL_rm (theEnv, rptr->types, (sizeof (void *) * rptr->tcnt));
      CL_ExpressionDeinstall (theEnv, rptr->query);
      CL_ReturnPackedExpression (theEnv, rptr->query);
    }
  if (meth->restrictions != NULL)
    CL_rm (theEnv, meth->restrictions,
	   (sizeof (RESTRICTION) * meth->restrictionCount));
  RestoreBusyCount (gfunc);
}

/***************************************************
  NAME         : CL_DestroyMethodInfo
  DESCRIPTION  : Deallocates all the data associated
                  w/ a method but does not release
                  the method structure itself
  INPUTS       : 1) The generic function address
                 2) The method address
  RETURNS      : Nothing useful
  SIDE EFFECTS : Nodes deallocated
  NOTES        : None
 ***************************************************/
void
CL_DestroyMethodInfo (Environment * theEnv,
		      Defgeneric * gfunc, Defmethod * meth)
{
  int j;
  RESTRICTION *rptr;
#if MAC_XCD
#pragma unused(gfunc)
#endif

  CL_ReturnPackedExpression (theEnv, meth->actions);

  CL_ClearUserDataList (theEnv, meth->header.usrData);
  if (meth->header.ppFo_rm != NULL)
    CL_rm (theEnv, (void *) meth->header.ppFo_rm,
	   (sizeof (char) * (strlen (meth->header.ppFo_rm) + 1)));
  for (j = 0; j < meth->restrictionCount; j++)
    {
      rptr = &meth->restrictions[j];

      if (rptr->types != NULL)
	CL_rm (theEnv, rptr->types, (sizeof (void *) * rptr->tcnt));
      CL_ReturnPackedExpression (theEnv, rptr->query);
    }

  if (meth->restrictions != NULL)
    CL_rm (theEnv, meth->restrictions,
	   (sizeof (RESTRICTION) * meth->restrictionCount));
}

/***************************************************
  NAME         : CL_MethodsExecuting
  DESCRIPTION  : Dete_rmines if any of the methods of
                   a generic function are currently
                   executing
  INPUTS       : The generic function address
  RETURNS      : True if any methods are executing,
                   false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
bool
CL_MethodsExecuting (Defgeneric * gfunc)
{
  long i;

  for (i = 0; i < gfunc->mcnt; i++)
    if (gfunc->methods[i].busy > 0)
      return true;
  return false;
}

#endif

#if ! OBJECT_SYSTEM

/**************************************************************
  NAME         : SubsumeType
  DESCRIPTION  : Dete_rmines if the second type subsumes
                 the first type
                 (e.g. INTEGER_TYPE is subsumed by NUMBER_TYPE_CODE)
  INPUTS       : Two type codes
  RETURNS      : True if type 2 subsumes type 1, false
                 otherwise
  SIDE EFFECTS : None
  NOTES        : Used only when COOL is not present
 **************************************************************/
bool
SubsumeType (int t1, int t2)
{
  if ((t2 == OBJECT_TYPE_CODE) || (t2 == PRIMITIVE_TYPE_CODE))
    return true;
  if ((t2 == NUMBER_TYPE_CODE)
      && ((t1 == INTEGER_TYPE) || (t1 == FLOAT_TYPE)))
    return true;
  if ((t2 == LEXEME_TYPE_CODE)
      && ((t1 == STRING_TYPE) || (t1 == SYMBOL_TYPE)))
    return true;
  if ((t2 == ADDRESS_TYPE_CODE) && ((t1 == EXTERNAL_ADDRESS_TYPE) ||
				    (t1 == FACT_ADDRESS_TYPE)
				    || (t1 == INSTANCE_ADDRESS_TYPE)))
    return true;
  if ((t2 == LEXEME_TYPE_CODE) &&
      ((t1 == INSTANCE_NAME_TYPE) || (t1 == INSTANCE_ADDRESS_TYPE)))
    return true;
  return false;
}

#endif

/*****************************************************
  NAME         : CL_FindMethodByIndex
  DESCRIPTION  : Finds a generic function method of
                   specified index
  INPUTS       : 1) The generic function
                 2) The index
  RETURNS      : The position of the method in the
                   generic function's method array,
                   -1 if not found
  SIDE EFFECTS : None
  NOTES        : None
 *****************************************************/
unsigned short
CL_FindMethodByIndex (Defgeneric * gfunc, unsigned short theIndex)
{
  unsigned short i;

  for (i = 0; i < gfunc->mcnt; i++)
    {
      if (gfunc->methods[i].index == theIndex)
	{
	  return i;
	}
    }

  return METHOD_NOT_FOUND;
}

#if DEBUGGING_FUNCTIONS || PROFILING_FUNCTIONS

/******************************************************************
  NAME         : CL_PrintMethod
  DESCRIPTION  : Lists a brief description of methods for a method
  INPUTS       : 1) Buffer for method info
                 2) Size of buffer (not including space for '\0')
                 3) The method address
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : A te_rminating newline is NOT included
 ******************************************************************/
void
CL_PrintMethod (Environment * theEnv,
		Defmethod * meth, String_Builder * theSB)
{
#if MAC_XCD
#pragma unused(theEnv)
#endif
  unsigned short j, k;
  RESTRICTION *rptr;
  char numbuf[15];

  SB_Reset (theSB);
  if (meth->system)
    CL_SBAppend (theSB, "SYS");
  CL_gensprintf (numbuf, "%-2hu ", meth->index);
  CL_SBAppend (theSB, numbuf);
  for (j = 0; j < meth->restrictionCount; j++)
    {
      rptr = &meth->restrictions[j];
      if (((j + 1) == meth->restrictionCount)
	  && (meth->maxRestrictions == RESTRICTIONS_UNBOUNDED))
	{
	  if ((rptr->tcnt == 0) && (rptr->query == NULL))
	    {
	      CL_SBAppend (theSB, "$?");
	      break;
	    }
	  CL_SBAppend (theSB, "($? ");
	}
      else
	CL_SBAppend (theSB, "(");
      for (k = 0; k < rptr->tcnt; k++)
	{
#if OBJECT_SYSTEM
	  CL_SBAppend (theSB, CL_DefclassName ((Defclass *) rptr->types[k]));
#else
	  CL_SBAppend (theSB,
		       TypeName (theEnv,
				 ((CLIPSInteger *) rptr->types[k])->
				 contents));
#endif
	  if ((k + 1) < rptr->tcnt)
	    CL_SBAppend (theSB, " ");
	}
      if (rptr->query != NULL)
	{
	  if (rptr->tcnt != 0)
	    CL_SBAppend (theSB, " ");
	  CL_SBAppend (theSB, "<qry>");
	}
      CL_SBAppend (theSB, ")");
      if ((j + 1) != meth->restrictionCount)
	CL_SBAppend (theSB, " ");
    }
}

#endif /* DEBUGGING_FUNCTIONS || PROFILING_FUNCTIONS */

#if DEBUGGING_FUNCTIONS

/*************************************************************
  NAME         : CL_PreviewGeneric
  DESCRIPTION  : Allows the user to see a printout of all the
                   applicable methods for a particular generic
                   function call
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Any side-effects of evaluating the generic
                   function arguments
                 and evaluating query-functions to dete_rmine
                   the set of applicable methods
  NOTES        : H/L Syntax: (preview-generic <func> <args>)
 *************************************************************/
void
CL_PreviewGeneric (Environment * theEnv,
		   UDFContext * context, UDFValue * returnValue)
{
  Defgeneric *gfunc;
  Defgeneric *previousGeneric;
  bool oldce;
  UDFValue theArg;

  CL_EvaluationData (theEnv)->CL_EvaluationError = false;
  if (!CL_UDFFirstArgument (context, SYMBOL_BIT, &theArg))
    return;

  gfunc =
    CL_LookupDefgenericByMdlOrScope (theEnv, theArg.lexemeValue->contents);
  if (gfunc == NULL)
    {
      CL_PrintErrorID (theEnv, "GENRCFUN", 3, false);
      CL_WriteString (theEnv, STDERR, "Unable to find generic function '");
      CL_WriteString (theEnv, STDERR, theArg.lexemeValue->contents);
      CL_WriteString (theEnv, STDERR, "' in function preview-generic.\n");
      return;
    }
  oldce = CL_ExecutingConstruct (theEnv);
  Set_ExecutingConstruct (theEnv, true);
  previousGeneric = DefgenericData (theEnv)->CurrentGeneric;
  DefgenericData (theEnv)->CurrentGeneric = gfunc;
  CL_EvaluationData (theEnv)->Current_EvaluationDepth++;
  CL_PushProcParameters (theEnv, GetFirstArgument ()->nextArg,
			 CL_CountArguments (GetFirstArgument ()->nextArg),
			 CL_DefgenericName (gfunc), "generic function",
			 CL_UnboundMethodErr);
  if (CL_EvaluationData (theEnv)->CL_EvaluationError)
    {
      CL_PopProcParameters (theEnv);
      DefgenericData (theEnv)->CurrentGeneric = previousGeneric;
      CL_EvaluationData (theEnv)->Current_EvaluationDepth--;
      Set_ExecutingConstruct (theEnv, oldce);
      return;
    }
  gfunc->busy++;
  DisplayGenericCore (theEnv, gfunc);
  gfunc->busy--;
  CL_PopProcParameters (theEnv);
  DefgenericData (theEnv)->CurrentGeneric = previousGeneric;
  CL_EvaluationData (theEnv)->Current_EvaluationDepth--;
  Set_ExecutingConstruct (theEnv, oldce);
}

#endif /* DEBUGGING_FUNCTIONS */

/***************************************************
  NAME         : CL_CheckGenericExists
  DESCRIPTION  : Finds the address of named
                  generic function and prints out
                  error message if not found
  INPUTS       : 1) Calling function
                 2) Name of generic function
  RETURNS      : Generic function address (NULL if
                   not found)
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
Defgeneric *
CL_CheckGenericExists (Environment * theEnv,
		       const char *fname, const char *gname)
{
  Defgeneric *gfunc;

  gfunc = CL_LookupDefgenericByMdlOrScope (theEnv, gname);
  if (gfunc == NULL)
    {
      CL_PrintErrorID (theEnv, "GENRCFUN", 3, false);
      CL_WriteString (theEnv, STDERR, "Unable to find generic function '");
      CL_WriteString (theEnv, STDERR, gname);
      CL_WriteString (theEnv, STDERR, "' in function '");
      CL_WriteString (theEnv, STDERR, fname);
      CL_WriteString (theEnv, STDERR, "'.\n");
      Set_EvaluationError (theEnv, true);
    }
  return (gfunc);
}

/***************************************************
  NAME         : CL_CheckMethodExists
  DESCRIPTION  : Finds the array index of the
                  specified method and prints out
                  error message if not found
  INPUTS       : 1) Calling function
                 2) Generic function address
                 3) Index of method
  RETURNS      : Method array index (METHOD_NOT_FOUND if not found)
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
unsigned short
CL_CheckMethodExists (Environment * theEnv,
		      const char *fname,
		      Defgeneric * gfunc, unsigned short mi)
{
  unsigned short fi;

  fi = CL_FindMethodByIndex (gfunc, mi);
  if (fi == METHOD_NOT_FOUND)
    {
      CL_PrintErrorID (theEnv, "GENRCFUN", 2, false);
      CL_WriteString (theEnv, STDERR, "Unable to find method '");
      CL_WriteString (theEnv, STDERR, CL_DefgenericName (gfunc));
      CL_WriteString (theEnv, STDERR, "' #");
      CL_PrintUnsignedInteger (theEnv, STDERR, mi);
      CL_WriteString (theEnv, STDERR, " in function '");
      CL_WriteString (theEnv, STDERR, fname);
      CL_WriteString (theEnv, STDERR, "'.\n");
      Set_EvaluationError (theEnv, true);
    }
  return fi;
}

#if ! OBJECT_SYSTEM

/*******************************************************
  NAME         : TypeName
  DESCRIPTION  : Given an integer type code, this
                 function returns the string name of
                 the type
  INPUTS       : The type code
  RETURNS      : The name-string of the type, or
                 "<???UNKNOWN-TYPE???>" for unrecognized
                 types
  SIDE EFFECTS : CL_EvaluationError set and error message
                 printed for unrecognized types
  NOTES        : Used only when COOL is not present
 *******************************************************/
const char *
TypeName (Environment * theEnv, int tcode)
{
  switch (tcode)
    {
    case INTEGER_TYPE:
      return (INTEGER_TYPE_NAME);
    case FLOAT_TYPE:
      return (FLOAT_TYPE_NAME);
    case SYMBOL_TYPE:
      return (SYMBOL_TYPE_NAME);
    case STRING_TYPE:
      return (STRING_TYPE_NAME);
    case MULTIFIELD_TYPE:
      return (MULTIFIELD_TYPE_NAME);
    case EXTERNAL_ADDRESS_TYPE:
      return (EXTERNAL_ADDRESS_TYPE_NAME);
    case FACT_ADDRESS_TYPE:
      return (FACT_ADDRESS_TYPE_NAME);
    case INSTANCE_ADDRESS_TYPE:
      return (INSTANCE_ADDRESS_TYPE_NAME);
    case INSTANCE_NAME_TYPE:
      return (INSTANCE_NAME_TYPE_NAME);
    case OBJECT_TYPE_CODE:
      return (OBJECT_TYPE_NAME);
    case PRIMITIVE_TYPE_CODE:
      return (PRIMITIVE_TYPE_NAME);
    case NUMBER_TYPE_CODE:
      return (NUMBER_TYPE_NAME);
    case LEXEME_TYPE_CODE:
      return (LEXEME_TYPE_NAME);
    case ADDRESS_TYPE_CODE:
      return (ADDRESS_TYPE_NAME);
    case INSTANCE_TYPE_CODE:
      return (INSTANCE_TYPE_NAME);
    default:
      CL_PrintErrorID (theEnv, "INSCOM", 1, false);
      CL_WriteString (theEnv, STDERR, "Undefined type in function 'type'.\n");
      Set_EvaluationError (theEnv, true);
      return ("<UNKNOWN-TYPE>");
    }
}

#endif

/******************************************************
  NAME         : CL_PrintGenericName
  DESCRIPTION  : Prints the name of a gneric function
                 (including the module name if the
                  generic is not in the current module)
  INPUTS       : 1) The logical name of the output
                 2) The generic functions
  RETURNS      : Nothing useful
  SIDE EFFECTS : Generic name printed
  NOTES        : None
 ******************************************************/
void
CL_PrintGenericName (Environment * theEnv,
		     const char *logName, Defgeneric * gfunc)
{
  if (gfunc->header.whichModule->theModule != CL_GetCurrentModule (theEnv))
    {
      CL_WriteString (theEnv, logName, CL_DefgenericModule (gfunc));
      CL_WriteString (theEnv, logName, "::");
    }
  CL_WriteString (theEnv, logName, gfunc->header.name->contents);
}

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if DEBUGGING_FUNCTIONS

/*********************************************************
  NAME         : DisplayGenericCore
  DESCRIPTION  : Prints out a description of a core
                   frame of applicable methods for
                   a particular call of a generic function
  INPUTS       : The generic function
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 *********************************************************/
static void
DisplayGenericCore (Environment * theEnv, Defgeneric * gfunc)
{
  long i;
  bool rtn = false;
  String_Builder *theSB;

  theSB = CL_CreateString_Builder (theEnv, 256);

  for (i = 0; i < gfunc->mcnt; i++)
    {
      gfunc->methods[i].busy++;
      if (CL_IsMethodApplicable (theEnv, &gfunc->methods[i]))
	{
	  rtn = true;
	  CL_WriteString (theEnv, STDOUT, CL_DefgenericName (gfunc));
	  CL_WriteString (theEnv, STDOUT, " #");
	  CL_PrintMethod (theEnv, &gfunc->methods[i], theSB);
	  CL_WriteString (theEnv, STDOUT, theSB->contents);
	  CL_WriteString (theEnv, STDOUT, "\n");
	}
      gfunc->methods[i].busy--;
    }
  if (rtn == false)
    {
      CL_WriteString (theEnv, STDOUT, "No applicable methods for ");
      CL_WriteString (theEnv, STDOUT, CL_DefgenericName (gfunc));
      CL_WriteString (theEnv, STDOUT, ".\n");
    }

  CL_SBDispose (theSB);
}

#endif

#endif
