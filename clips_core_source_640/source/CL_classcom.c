   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/02/18             */
   /*                                                     */
   /*                  CLASS COMMANDS MODULE              */
   /*******************************************************/

/**************************************************************/
/* Purpose: Kernel Interface Commands for Object System       */
/*                                                            */
/* Principal Programmer(s):                                   */
/*      Brian L. Dantes                                       */
/*                                                            */
/* Contributing Programmer(s):                                */
/*                                                            */
/* Revision History:                                          */
/*                                                            */
/*      6.23: Corrected compilation errors for files          */
/*            generated by constructs-to-c. DR0861            */
/*                                                            */
/*            Changed name of variable log to logName         */
/*            because of Unix compiler warnings of shadowed   */
/*            definitions.                                    */
/*                                                            */
/*      6.24: Renamed BOOLEAN macro type to intBool.          */
/*                                                            */
/*            Added pragmas to remove compilation warnings.   */
/*                                                            */
/*      6.30: Borland C (IBM_TBC) and Metrowerks CodeWarrior  */
/*            (MAC_MCW, IBM_MCW) are no longer supported.     */
/*                                                            */
/*            Added const qualifiers to remove C++            */
/*            deprecation warnings.                           */
/*                                                            */
/*            Converted API macros to function calls.         */
/*                                                            */
/*            Changed find construct functionality so that    */
/*            imported modules are search when locating a     */
/*            named construct.                                */
/*                                                            */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            Pretty print functions accept optional logical */
/*            name argument.                                 */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */

#include <string.h>

#include "setup.h"

#if OBJECT_SYSTEM

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "bload.h"
#endif

#include "argacces.h"
#include "classfun.h"
#include "classini.h"
#include "envrnmnt.h"
#include "modulutl.h"
#include "msgcom.h"
#include "prntutil.h"
#include "router.h"

#include "classcom.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if (! BLOAD_ONLY) && (! RUN_TIME) && DEBUGGING_FUNCTIONS
static void CL_SaveDefclass (Environment *, ConstructHeader *, void *);
#endif
static const char *CL_GetClassDefaultsModeName (unsigned short);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/*******************************************************************
  NAME         : CL_FindDefclass
  DESCRIPTION  : Looks up a specified class in the class hash table
                 (Only looks in current or specified module)
  INPUTS       : The name-string of the class (including module)
  RETURNS      : The address of the found class, NULL otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ******************************************************************/
Defclass *
CL_FindDefclass (		// TBD Needs to look in imported
		  Environment * theEnv, const char *classAndModuleName)
{
  CLIPSLexeme *classSymbol = NULL;
  Defclass *cls;
  Defmodule *theModule = NULL;
  const char *className;

  CL_SaveCurrentModule (theEnv);

  className = CL_ExtractModuleAndConstructName (theEnv, classAndModuleName);
  if (className != NULL)
    {
      classSymbol =
	CL_FindSymbolHN (theEnv,
			 CL_ExtractModuleAndConstructName (theEnv,
							   classAndModuleName),
			 SYMBOL_BIT);
      theModule = CL_GetCurrentModule (theEnv);
    }

  CL_RestoreCurrentModule (theEnv);

  if (classSymbol == NULL)
    {
      return NULL;
    }

  cls = DefclassData (theEnv)->ClassTable[CL_HashClass (classSymbol)];
  while (cls != NULL)
    {
      if (cls->header.name == classSymbol)
	{
	  if (cls->system
	      || (cls->header.whichModule->theModule == theModule))
	    {
	      return cls->installed ? cls : NULL;
	    }
	}
      cls = cls->nxtHash;
    }

  return NULL;
}

/*******************************************************************
  NAME         : CL_FindDefclassInModule
  DESCRIPTION  : Looks up a specified class in the class hash table
                 (Only looks in current or specified module)
  INPUTS       : The name-string of the class (including module)
  RETURNS      : The address of the found class, NULL otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ******************************************************************/
Defclass *
CL_FindDefclassInModule (Environment * theEnv, const char *classAndModuleName)
{
  CLIPSLexeme *classSymbol = NULL;
  Defclass *cls;
  Defmodule *theModule = NULL;
  const char *className;

  CL_SaveCurrentModule (theEnv);
  className = CL_ExtractModuleAndConstructName (theEnv, classAndModuleName);
  if (className != NULL)
    {
      classSymbol =
	CL_FindSymbolHN (theEnv,
			 CL_ExtractModuleAndConstructName (theEnv,
							   classAndModuleName),
			 SYMBOL_BIT);
      theModule = CL_GetCurrentModule (theEnv);
    }
  CL_RestoreCurrentModule (theEnv);

  if (classSymbol == NULL)
    {
      return NULL;
    }

  cls = DefclassData (theEnv)->ClassTable[CL_HashClass (classSymbol)];
  while (cls != NULL)
    {
      if (cls->header.name == classSymbol)
	{
	  if (cls->system
	      || (cls->header.whichModule->theModule == theModule))
	    {
	      return cls->installed ? cls : NULL;
	    }
	}
      cls = cls->nxtHash;
    }

  return NULL;
}

/***************************************************
  NAME         : CL_LookupDefclassByMdlOrScope
  DESCRIPTION  : Finds a class anywhere (if module
                 is specified) or in current or
                 imported modules
  INPUTS       : The class name
  RETURNS      : The class (NULL if not found)
  SIDE EFFECTS : Error message printed on
                  ambiguous references
  NOTES        : Assumes no two classes of the same
                 name are ever in the same scope
 ***************************************************/
Defclass *
CL_LookupDefclassByMdlOrScope (Environment * theEnv,
			       const char *classAndModuleName)
{
  Defclass *cls;
  const char *className;
  CLIPSLexeme *classSymbol;
  Defmodule *theModule;

  if (CL_FindModuleSeparator (classAndModuleName) == 0)
    {
      return Lookup_DefclassInScope (theEnv, classAndModuleName);
    }

  CL_SaveCurrentModule (theEnv);
  className = CL_ExtractModuleAndConstructName (theEnv, classAndModuleName);
  theModule = CL_GetCurrentModule (theEnv);
  CL_RestoreCurrentModule (theEnv);

  if (className == NULL)
    {
      return NULL;
    }

  if ((classSymbol = CL_FindSymbolHN (theEnv, className, SYMBOL_BIT)) == NULL)
    {
      return NULL;
    }

  cls = DefclassData (theEnv)->ClassTable[CL_HashClass (classSymbol)];
  while (cls != NULL)
    {
      if ((cls->header.name == classSymbol) &&
	  (cls->header.whichModule->theModule == theModule))
	return (cls->installed ? cls : NULL);
      cls = cls->nxtHash;
    }

  return NULL;
}

/****************************************************
  NAME         : Lookup_DefclassInScope
  DESCRIPTION  : Finds a class in current or imported
                   modules (module specifier
                   is not allowed)
  INPUTS       : The class name
  RETURNS      : The class (NULL if not found)
  SIDE EFFECTS : Error message printed on
                  ambiguous references
  NOTES        : Assumes no two classes of the same
                 name are ever in the same scope
 ****************************************************/
Defclass *
Lookup_DefclassInScope (Environment * theEnv, const char *className)
{
  Defclass *cls;
  CLIPSLexeme *classSymbol;

  if ((classSymbol = CL_FindSymbolHN (theEnv, className, SYMBOL_BIT)) == NULL)
    {
      return NULL;
    }

  cls = DefclassData (theEnv)->ClassTable[CL_HashClass (classSymbol)];
  while (cls != NULL)
    {
      if ((cls->header.name == classSymbol)
	  && CL_DefclassInScope (theEnv, cls, NULL))
	return cls->installed ? cls : NULL;
      cls = cls->nxtHash;
    }

  return NULL;
}

/******************************************************
  NAME         : CL_LookupDefclassAnywhere
  DESCRIPTION  : Finds a class in specified
                 (or any) module
  INPUTS       : 1) The module (NULL if don't care)
                 2) The class name (module specifier
                    in name not allowed)
  RETURNS      : The class (NULL if not found)
  SIDE EFFECTS : None
  NOTES        : Does *not* generate an error if
                 multiple classes of the same name
                 exist as do the other lookup functions
 ******************************************************/
Defclass *
CL_LookupDefclassAnywhere (Environment * theEnv,
			   Defmodule * theModule, const char *className)
{
  Defclass *cls;
  CLIPSLexeme *classSymbol;

  if ((classSymbol = CL_FindSymbolHN (theEnv, className, SYMBOL_BIT)) == NULL)
    {
      return NULL;
    }

  cls = DefclassData (theEnv)->ClassTable[CL_HashClass (classSymbol)];
  while (cls != NULL)
    {
      if ((cls->header.name == classSymbol) &&
	  ((theModule == NULL) ||
	   (cls->header.whichModule->theModule == theModule)))
	{
	  return cls->installed ? cls : NULL;
	}
      cls = cls->nxtHash;
    }

  return NULL;
}

/***************************************************
  NAME         : CL_DefclassInScope
  DESCRIPTION  : Dete_rmines if a defclass is in
                 scope of the given module
  INPUTS       : 1) The defclass
                 2) The module (NULL for current
                    module)
  RETURNS      : True if in scope,
                 false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
bool
CL_DefclassInScope (Environment * theEnv,
		    Defclass * theDefclass, Defmodule * theModule)
{
#if DEFMODULE_CONSTRUCT
  unsigned long moduleID;
  char *scopeMap;

  scopeMap = (char *) theDefclass->scopeMap->contents;
  if (theModule == NULL)
    {
      theModule = CL_GetCurrentModule (theEnv);
    }
  moduleID = theModule->header.bsaveID;

  return TestBitMap (scopeMap, moduleID);
#else
#if MAC_XCD
#pragma unused(theEnv,theDefclass,theModule)
#endif
  return true;
#endif
}

/***********************************************************
  NAME         : CL_GetNextDefclass
  DESCRIPTION  : Finds first or next defclass
  INPUTS       : The address of the current defclass
  RETURNS      : The address of the next defclass
                   (NULL if none)
  SIDE EFFECTS : None
  NOTES        : If ptr == NULL, the first defclass
                    is returned.
 ***********************************************************/
Defclass *
CL_GetNextDefclass (Environment * theEnv, Defclass * theDefclass)
{
  return (Defclass *) CL_GetNextConstructItem (theEnv, &theDefclass->header,
					       DefclassData (theEnv)->
					       CL_DefclassModuleIndex);
}

/***************************************************
  NAME         : CL_DefclassIsDeletable
  DESCRIPTION  : Dete_rmines if a defclass
                   can be deleted
  INPUTS       : Address of the defclass
  RETURNS      : True if deletable,
                 false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
bool
CL_DefclassIsDeletable (Defclass * theDefclass)
{
  Environment *theEnv = theDefclass->header.env;

  if (!CL_ConstructsDeletable (theEnv))
    {
      return false;
    }

  if (theDefclass->system == 1)
    {
      return false;
    }

#if (! BLOAD_ONLY) && (! RUN_TIME)
  return (CL_IsClassBeingUsed (theDefclass) == false) ? true : false;
#else
  return false;
#endif
}

/*************************************************************
  NAME         : CL_UndefclassCommand
  DESCRIPTION  : Deletes a class and its subclasses, as
                 well as their associated instances
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : Syntax : (undefclass <class-name> | *)
 *************************************************************/
void
CL_UndefclassCommand (Environment * theEnv,
		      UDFContext * context, UDFValue * returnValue)
{
  CL_UndefconstructCommand (context, "undefclass",
			    DefclassData (theEnv)->DefclassConstruct);
}

/********************************************************
  NAME         : CL_Undefclass
  DESCRIPTION  : Deletes the named defclass
  INPUTS       : None
  RETURNS      : True if deleted, or false
  SIDE EFFECTS : Defclass and handlers removed
  NOTES        : Interface for CL_AddConstruct()
 ********************************************************/
bool
CL_Undefclass (Defclass * theDefclass, Environment * allEnv)
{
#if RUN_TIME || BLOAD_ONLY
  return false;
#else
  Environment *theEnv;
  bool success;
  GCBlock gcb;

  if (theDefclass == NULL)
    {
      theEnv = allEnv;
    }
  else
    {
      theEnv = theDefclass->header.env;
    }

#if BLOAD || BLOAD_AND_BSAVE
  if (CL_Bloaded (theEnv))
    return false;
#endif

  CL_GCBlockStart (theEnv, &gcb);
  if (theDefclass == NULL)
    {
      success = CL_RemoveAllUserClasses (theEnv);
      CL_GCBlockEnd (theEnv, &gcb);
      return success;
    }

  success = CL_DeleteClassUAG (theEnv, theDefclass);
  CL_GCBlockEnd (theEnv, &gcb);
  return success;
#endif
}


#if DEBUGGING_FUNCTIONS

/*********************************************************
  NAME         : CL_PPDefclassCommand
  DESCRIPTION  : Displays the pretty print fo_rm of
                 a class to stdout.
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : Syntax : (ppdefclass <class-name>)
 *********************************************************/
void
CL_PPDefclassCommand (Environment * theEnv,
		      UDFContext * context, UDFValue * returnValue)
{
  CL_PPConstructCommand (context, "ppdefclass",
			 DefclassData (theEnv)->DefclassConstruct,
			 returnValue);
}

/***************************************************
  NAME         : CL_ListDefclassesCommand
  DESCRIPTION  : Displays all defclass names
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defclass names printed
  NOTES        : H/L Interface
 ***************************************************/
void
CL_ListDefclassesCommand (Environment * theEnv,
			  UDFContext * context, UDFValue * returnValue)
{
  CL_ListConstructCommand (context, DefclassData (theEnv)->DefclassConstruct);
}

/***************************************************
  NAME         : CL_ListDefclasses
  DESCRIPTION  : Displays all defclass names
  INPUTS       : 1) The logical name of the output
                 2) The module
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defclass names printed
  NOTES        : C Interface
 ***************************************************/
void
CL_ListDefclasses (Environment * theEnv,
		   const char *logicalName, Defmodule * theModule)
{
  CL_ListConstruct (theEnv, DefclassData (theEnv)->DefclassConstruct,
		    logicalName, theModule);
}

/*********************************************************
  NAME         : CL_DefclassGet_Watch_Instances
  DESCRIPTION  : Dete_rmines if deletions/creations of
                 instances of this class will generate
                 trace messages or not
  INPUTS       : A pointer to the class
  RETURNS      : True if a trace is active,
                 false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 *********************************************************/
bool
CL_DefclassGet_Watch_Instances (Defclass * theDefclass)
{
  return theDefclass->trace_Instances;
}

/*********************************************************
  NAME         : CL_DefclassSet_Watch_Instances
  DESCRIPTION  : Sets the trace to ON/OFF for the
                 creation/deletion of instances
                 of the class
  INPUTS       : 1) true to set the trace on,
                    false to set it off
                 2) A pointer to the class
  RETURNS      : Nothing useful
  SIDE EFFECTS : CL_Watch flag for the class set
  NOTES        : None
 *********************************************************/
void
CL_DefclassSet_Watch_Instances (Defclass * theDefclass, bool newState)
{
  if (theDefclass->abstract)
    {
      return;
    }

  theDefclass->trace_Instances = newState;
}

/*********************************************************
  NAME         : CL_DefclassGet_WatchSlots
  DESCRIPTION  : Dete_rmines if changes to slots of
                 instances of this class will generate
                 trace messages or not
  INPUTS       : A pointer to the class
  RETURNS      : True if a trace is active,
                 false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 *********************************************************/
bool
CL_DefclassGet_WatchSlots (Defclass * theDefclass)
{
  return theDefclass->traceSlots;
}

/**********************************************************
  NAME         : SetDefclass_WatchSlots
  DESCRIPTION  : Sets the trace to ON/OFF for the
                 changes to slots of instances of the class
  INPUTS       : 1) true to set the trace on,
                    false to set it off
                 2) A pointer to the class
  RETURNS      : Nothing useful
  SIDE EFFECTS : CL_Watch flag for the class set
  NOTES        : None
 **********************************************************/
void
CL_DefclassSet_WatchSlots (Defclass * theDefclass, bool newState)
{
  theDefclass->traceSlots = newState;
}

/******************************************************************
  NAME         : CL_Defclass_WatchAccess
  DESCRIPTION  : Parses a list of class names passed by
                 CL_Add_WatchItem() and sets the traces accordingly
  INPUTS       : 1) A code indicating which trace flag is to be set
                    0 - CL_Watch instance creation/deletion
                    1 - CL_Watch slot changes to instances
                 2) The value to which to set the trace flags
                 3) A list of expressions containing the names
                    of the classes for which to set traces
  RETURNS      : True if all OK, false otherwise
  SIDE EFFECTS : CL_Watch flags set in specified classes
  NOTES        : Accessory function for CL_Add_WatchItem()
 ******************************************************************/
bool
CL_Defclass_WatchAccess (Environment * theEnv,
			 int code, bool newState, Expression * argExprs)
{
  if (code)
    return (CL_ConstructSet_WatchAccess
	    (theEnv, DefclassData (theEnv)->DefclassConstruct, newState,
	     argExprs,
	     (ConstructGet_WatchFunction *) CL_DefclassGet_WatchSlots,
	     (ConstructSet_WatchFunction *) CL_DefclassSet_WatchSlots));
  else
    return (CL_ConstructSet_WatchAccess
	    (theEnv, DefclassData (theEnv)->DefclassConstruct, newState,
	     argExprs,
	     (ConstructGet_WatchFunction *) CL_DefclassGet_Watch_Instances,
	     (ConstructSet_WatchFunction *) CL_DefclassSet_Watch_Instances));
}

/***********************************************************************
  NAME         : CL_Defclass_WatchPrint
  DESCRIPTION  : Parses a list of class names passed by
                 CL_Add_WatchItem() and displays the traces accordingly
  INPUTS       : 1) The logical name of the output
                 2) A code indicating which trace flag is to be examined
                    0 - CL_Watch instance creation/deletion
                    1 - CL_Watch slot changes to instances
                 3) A list of expressions containing the names
                    of the classes for which to examine traces
  RETURNS      : True if all OK, false otherwise
  SIDE EFFECTS : CL_Watch flags displayed for specified classes
  NOTES        : Accessory function for CL_Add_WatchItem()
 ***********************************************************************/
bool
CL_Defclass_WatchPrint (Environment * theEnv,
			const char *logName, int code, Expression * argExprs)
{
  if (code)
    return (CL_ConstructPrint_WatchAccess
	    (theEnv, DefclassData (theEnv)->DefclassConstruct, logName,
	     argExprs,
	     (ConstructGet_WatchFunction *) CL_DefclassGet_WatchSlots,
	     (ConstructSet_WatchFunction *) CL_DefclassSet_WatchSlots));
  else
    return (CL_ConstructPrint_WatchAccess
	    (theEnv, DefclassData (theEnv)->DefclassConstruct, logName,
	     argExprs,
	     (ConstructGet_WatchFunction *) CL_DefclassGet_Watch_Instances,
	     (ConstructSet_WatchFunction *) CL_DefclassSet_Watch_Instances));
}

#endif /* DEBUGGING_FUNCTIONS */

/*********************************************************
  NAME         : CL_GetDefclassListFunction
  DESCRIPTION  : Groups names of all defclasses into
                   a multifield variable
  INPUTS       : A data object buffer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Multifield set to list of classes
  NOTES        : None
 *********************************************************/
void
CL_GetDefclassListFunction (Environment * theEnv,
			    UDFContext * context, UDFValue * returnValue)
{
  CL_GetConstructListFunction (context, returnValue,
			       DefclassData (theEnv)->DefclassConstruct);
}

/***************************************************************
  NAME         : CL_GetDefclassList
  DESCRIPTION  : Groups all defclass names into
                 a multifield list
  INPUTS       : 1) A data object buffer to hold
                    the multifield result
                 2) The module from which to obtain defclasses
  RETURNS      : Nothing useful
  SIDE EFFECTS : Multifield allocated and filled
  NOTES        : External C access
 ***************************************************************/
void
CL_GetDefclassList (Environment * theEnv,
		    CLIPSValue * returnValue, Defmodule * theModule)
{
  UDFValue result;

  CL_GetConstructList (theEnv, &result,
		       DefclassData (theEnv)->DefclassConstruct, theModule);
  CL_No_rmalizeMultifield (theEnv, &result);
  returnValue->value = result.value;
}

/*****************************************************
  NAME         : CL_HasSuperclass
  DESCRIPTION  : Dete_rmines if class-2 is a superclass
                   of class-1
  INPUTS       : 1) Class-1
                 2) Class-2
  RETURNS      : True if class-2 is a superclass of
                   class-1, false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 *****************************************************/
bool
CL_HasSuperclass (Defclass * c1, Defclass * c2)
{
  unsigned long i;

  for (i = 1; i < c1->allSuperclasses.classCount; i++)
    if (c1->allSuperclasses.classArray[i] == c2)
      return true;
  return false;
}

/********************************************************************
  NAME         : CL_CheckClassAndSlot
  DESCRIPTION  : Checks class and slot argument for various functions
  INPUTS       : 1) Name of the calling function
                 2) Buffer for class address
  RETURNS      : Slot symbol, NULL on errors
  SIDE EFFECTS : None
  NOTES        : None
 ********************************************************************/
CLIPSLexeme *
CL_CheckClassAndSlot (UDFContext * context, const char *func, Defclass ** cls)
{
  UDFValue theArg;
  Environment *theEnv = context->environment;

  if (!CL_UDFFirstArgument (context, SYMBOL_BIT, &theArg))
    return NULL;

  *cls = CL_LookupDefclassByMdlOrScope (theEnv, theArg.lexemeValue->contents);
  if (*cls == NULL)
    {
      CL_ClassExistError (theEnv, func, theArg.lexemeValue->contents);
      return NULL;
    }

  if (!CL_UDFNextArgument (context, SYMBOL_BIT, &theArg))
    return NULL;

  return theArg.lexemeValue;
}

#if (! BLOAD_ONLY) && (! RUN_TIME)

/***************************************************
  NAME         : CL_SaveDefclasses
  DESCRIPTION  : Prints pretty print fo_rm of
                   defclasses to specified output
  INPUTS       : The  logical name of the output
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
void
CL_SaveDefclasses (Environment * theEnv,
		   Defmodule * theModule, const char *logName, void *context)
{
#if DEBUGGING_FUNCTIONS
  CL_DoForAllConstructsInModule (theEnv, theModule, CL_SaveDefclass,
				 DefclassData (theEnv)->
				 CL_DefclassModuleIndex, false,
				 (void *) logName);
#else
#if MAC_XCD
#pragma unused(theEnv,theModule,logName)
#endif
#endif
}

#endif

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if (! BLOAD_ONLY) && (! RUN_TIME) && DEBUGGING_FUNCTIONS

/***************************************************
  NAME         : CL_SaveDefclass
  DESCRIPTION  : CL_Writes out the pretty-print fo_rms
                 of a class and all its handlers
  INPUTS       : 1) The class
                 2) The logical name of the output
  RETURNS      : Nothing useful
  SIDE EFFECTS : Class and handlers written
  NOTES        : None
 ***************************************************/
static void
CL_SaveDefclass (Environment * theEnv,
		 ConstructHeader * theConstruct, void *userBuffer)
{
  const char *logName = (const char *) userBuffer;
  Defclass *theDefclass = (Defclass *) theConstruct;
  unsigned hnd;
  const char *ppFo_rm;

  ppFo_rm = CL_DefclassPPFo_rm (theDefclass);
  if (ppFo_rm != NULL)
    {
      CL_WriteString (theEnv, logName, ppFo_rm);
      CL_WriteString (theEnv, logName, "\n");
      hnd = CL_GetNextDefmessageHandler (theDefclass, 0);
      while (hnd != 0)
	{
	  ppFo_rm = CL_DefmessageHandlerPPFo_rm (theDefclass, hnd);
	  if (ppFo_rm != NULL)
	    {
	      CL_WriteString (theEnv, logName, ppFo_rm);
	      CL_WriteString (theEnv, logName, "\n");
	    }
	  hnd = CL_GetNextDefmessageHandler (theDefclass, hnd);
	}
    }
}

#endif

/********************************************/
/* CL_SetClassDefaultsMode: Allows the setting */
/*    of the class defaults mode.           */
/********************************************/
ClassDefaultsMode
CL_SetClassDefaultsMode (Environment * theEnv, ClassDefaultsMode value)
{
  ClassDefaultsMode ov;

  ov = DefclassData (theEnv)->ClassDefaultsModeValue;
  DefclassData (theEnv)->ClassDefaultsModeValue = value;
  return ov;
}

/****************************************/
/* CL_GetClassDefaultsMode: Returns the    */
/*    value of the class defaults mode. */
/****************************************/
ClassDefaultsMode
CL_GetClassDefaultsMode (Environment * theEnv)
{
  return DefclassData (theEnv)->ClassDefaultsModeValue;
}

/***************************************************/
/* CL_GetClassDefaultsModeCommand: H/L access routine */
/*   for the get-class-defaults-mode command.      */
/***************************************************/
void
CL_GetClassDefaultsModeCommand (Environment * theEnv,
				UDFContext * context, UDFValue * returnValue)
{
  returnValue->lexemeValue =
    CL_CreateSymbol (theEnv,
		     CL_GetClassDefaultsModeName (CL_GetClassDefaultsMode
						  (theEnv)));
}

/***************************************************/
/* CL_SetClassDefaultsModeCommand: H/L access routine */
/*   for the set-class-defaults-mode command.      */
/***************************************************/
void
CL_SetClassDefaultsModeCommand (Environment * theEnv,
				UDFContext * context, UDFValue * returnValue)
{
  UDFValue theArg;
  const char *argument;
  unsigned short oldMode;

  oldMode = DefclassData (theEnv)->ClassDefaultsModeValue;

   /*=====================================================*/
  /* Check for the correct number and type of arguments. */
   /*=====================================================*/

  if (!CL_UDFFirstArgument (context, SYMBOL_BIT, &theArg))
    {
      return;
    }

  argument = theArg.lexemeValue->contents;

   /*=============================================*/
  /* Set the strategy to the specified strategy. */
   /*=============================================*/

  if (strcmp (argument, "conservation") == 0)
    {
      CL_SetClassDefaultsMode (theEnv, CONSERVATION_MODE);
    }
  else if (strcmp (argument, "convenience") == 0)
    {
      CL_SetClassDefaultsMode (theEnv, CONVENIENCE_MODE);
    }
  else
    {
      CL_UDFInvalidArgumentMessage (context,
				    "symbol with value conservation or convenience");
      returnValue->lexemeValue =
	CL_CreateSymbol (theEnv,
			 CL_GetClassDefaultsModeName (CL_GetClassDefaultsMode
						      (theEnv)));
      return;
    }

   /*===================================*/
  /* Return the old value of the mode. */
   /*===================================*/

  returnValue->lexemeValue =
    CL_CreateSymbol (theEnv, CL_GetClassDefaultsModeName (oldMode));
}

/*******************************************************************/
/* CL_GetClassDefaultsModeName: Given the integer value corresponding */
/*   to a specified class defaults mode, return a character string */
/*   of the class defaults mode's name.                            */
/*******************************************************************/
static const char *
CL_GetClassDefaultsModeName (unsigned short mode)
{
  const char *sname;

  switch (mode)
    {
    case CONSERVATION_MODE:
      sname = "conservation";
      break;
    case CONVENIENCE_MODE:
      sname = "convenience";
      break;
    default:
      sname = "unknown";
      break;
    }

  return (sname);
}

/*#############################*/
/* Additional Access Functions */
/*#############################*/

CLIPSLexeme *
Get_DefclassNamePointer (Defclass * theClass)
{
  return CL_GetConstructNamePointer (&theClass->header);
}

void
CL_SetNextDefclass (Defclass * theClass, Defclass * targetClass)
{
  CL_SetNextConstruct (&theClass->header, &targetClass->header);
}

/*##################################*/
/* Additional Environment Functions */
/*##################################*/

const char *
CL_DefclassName (Defclass * theClass)
{
  return CL_GetConstructNameString (&theClass->header);
}

const char *
CL_DefclassPPFo_rm (Defclass * theClass)
{
  return CL_GetConstructPPFo_rm (&theClass->header);
}

struct defmoduleItemHeader *
Get_DefclassModule (Environment * theEnv, Defclass * theClass)
{
  return CL_GetConstructModuleItem (&theClass->header);
}

const char *
CL_DefclassModule (Defclass * theClass)
{
  return CL_GetConstructModuleName (&theClass->header);
}

void
SetCL_DefclassPPFo_rm (Environment * theEnv,
		       Defclass * theClass, char *thePPFo_rm)
{
  SetConstructPPFo_rm (theEnv, &theClass->header, thePPFo_rm);
}

#endif /* OBJECT_SYSTEM */
