   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  06/22/18             */
   /*                                                     */
   /*                 CLASS EXAMINATION MODULE            */
   /*******************************************************/

/**************************************************************/
/* Purpose: Class browsing and examination commands           */
/*                                                            */
/* Principal Programmer(s):                                   */
/*      Brian L. Dantes                                       */
/*                                                            */
/* Contributing Programmer(s):                                */
/*                                                            */
/* Revision History:                                          */
/*                                                            */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859   */
/*                                                            */
/*            Modified the slot-writablep function to return  */
/*            FALSE for slots having initialize-only access.  */
/*            DR0860                                          */
/*                                                            */
/*      6.24: Added allowed-classes slot facet.               */
/*                                                            */
/*            Converted INSTANCE_PATTERN_MATCHING to          */
/*            DEFRULE_CONSTRUCT.                              */
/*                                                            */
/*            Renamed BOOLEAN macro type to intBool.          */
/*                                                            */
/*            The slot-default-value function crashes when no */
/*            default exists for a slot (the ?NONE value was  */
/*            specified). DR0870                              */
/*                                                            */
/*      6.30: Used %zd for printing size_t arguments.         */
/*                                                            */
/*            Added Env_SlotDefaultP function.                 */
/*                                                            */
/*            Borland C (IBM_TBC) and Metrowerks CodeWarrior  */
/*            (MAC_MCW, IBM_MCW) are no longer supported.     */
/*                                                            */
/*            Used CL_gensprintf and CL_genstrcat instead of        */
/*            sprintf and strcat.                             */
/*                                                            */
/*            Added const qualifiers to remove C++            */
/*            deprecation warnings.                           */
/*                                                            */
/*            Converted API macros to function calls.         */
/*                                                            */
/*      6.40: Added Env prefix to Get_EvaluationError and      */
/*            Set_EvaluationError functions.                   */
/*                                                            */
/*            Pragma once and other inclusion changes.        */
/*                                                            */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/**************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if OBJECT_SYSTEM

#include <string.h>

#include "argacces.h"
#include "classcom.h"
#include "classfun.h"
#include "classini.h"
#include "envrnmnt.h"
#include "insfun.h"
#include "memalloc.h"
#include "msgcom.h"
#include "msgfun.h"
#include "prntutil.h"
#include "router.h"
#include "strngrtr.h"
#include "sysdep.h"

#include "classexm.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static bool CheckTwoClasses (UDFContext *, const char *, Defclass **,
			     Defclass **);
static SlotDescriptor *CheckSlotExists (UDFContext *, const char *,
					Defclass **, bool, bool);
static SlotDescriptor *LookupSlot (Environment *, Defclass *, const char *,
				   bool);

#if DEBUGGING_FUNCTIONS
static Defclass *CheckClass (Environment *, const char *, const char *);
static const char *GetClassNameArgument (UDFContext *);
static void PrintClassBrowse (Environment *, const char *, Defclass *,
			      unsigned long);
static void DisplaySeparator (Environment *, const char *, char *, int, int);
static void DisplaySlotBasicInfo (Environment *, const char *, const char *,
				  const char *, char *, Defclass *);
static bool CL_Print_SlotSources (Environment *, const char *, CLIPSLexeme *,
				  PACKED_CLASS_LINKS *, unsigned long, bool);
static void DisplaySlotConstraintInfo (Environment *, const char *,
				       const char *, char *, unsigned,
				       Defclass *);
static const char *ConstraintCode (CONSTRAINT_RECORD *, unsigned, unsigned);
#endif

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if DEBUGGING_FUNCTIONS

/****************************************************************
  NAME         : CL_BrowseClassesCommand
  DESCRIPTION  : Displays a "graph" of the class hierarchy
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : Syntax : (browse-classes [<class>])
 ****************************************************************/
void
CL_BrowseClassesCommand (Environment * theEnv,
			 UDFContext * context, UDFValue * returnValue)
{
  Defclass *cls;

  if (CL_UDFArgumentCount (context) == 0)
    /* ================================================
       Find the OBJECT root class (has no superclasses)
       ================================================ */
    cls = CL_LookupDefclassByMdlOrScope (theEnv, OBJECT_TYPE_NAME);
  else
    {
      UDFValue theArg;

      if (!CL_UDFFirstArgument (context, SYMBOL_BIT, &theArg))
	return;
      cls =
	CL_LookupDefclassByMdlOrScope (theEnv, theArg.lexemeValue->contents);
      if (cls == NULL)
	{
	  CL_ClassExistError (theEnv, "browse-classes",
			      theArg.lexemeValue->contents);
	  return;
	}
    }
  CL_BrowseClasses (cls, STDOUT);
}

/****************************************************************
  NAME         : CL_BrowseClasses
  DESCRIPTION  : Displays a "graph" of the class hierarchy
  INPUTS       : 1) The logical name of the output
                 2) Class pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ****************************************************************/
void
CL_BrowseClasses (Defclass * theDefclass, const char *logicalName)
{
  Environment *theEnv = theDefclass->header.env;

  PrintClassBrowse (theEnv, logicalName, theDefclass, 0);
}

/****************************************************************
  NAME         : Describe_ClassCommand
  DESCRIPTION  : Displays direct superclasses and
                   subclasses and the entire precedence
                   list for a class
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : Syntax : (describe-class <class-name>)
 ****************************************************************/
void
Describe_ClassCommand (Environment * theEnv,
		       UDFContext * context, UDFValue * returnValue)
{
  const char *className;
  Defclass *theDefclass;

  className = GetClassNameArgument (context);

  if (className == NULL)
    {
      return;
    }

  theDefclass = CheckClass (theEnv, "describe-class", className);

  if (theDefclass == NULL)
    {
      return;
    }

  CL_DescribeClass (theDefclass, STDOUT);
}

/******************************************************
  NAME         : CL_DescribeClass
  DESCRIPTION  : Displays direct superclasses and
                   subclasses and the entire precedence
                   list for a class
  INPUTS       : 1) The logical name of the output
                 2) Class pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ******************************************************/
void
CL_DescribeClass (Defclass * theDefclass, const char *logicalName)
{
  char buf[83], slotNamePrintFo_rmat[12], overrideMessagePrintFo_rmat[12];
  bool messageBanner;
  unsigned long i;
  size_t slotNameLength, maxSlotNameLength;
  size_t overrideMessageLength, maxOverrideMessageLength;
  Environment *theEnv = theDefclass->header.env;

  DisplaySeparator (theEnv, logicalName, buf, 82, '=');
  DisplaySeparator (theEnv, logicalName, buf, 82, '*');
  if (theDefclass->abstract)
    CL_WriteString (theEnv, logicalName,
		    "Abstract: direct instances of this class cannot be created.\n\n");
  else
    {
      CL_WriteString (theEnv, logicalName,
		      "Concrete: direct instances of this class can be created.\n");
#if DEFRULE_CONSTRUCT
      if (theDefclass->reactive)
	CL_WriteString (theEnv, logicalName,
			"Reactive: direct instances of this class can match defrule patterns.\n\n");
      else
	CL_WriteString (theEnv, logicalName,
			"Non-reactive: direct instances of this class cannot match defrule patterns.\n\n");
#else
      CL_WriteString (theEnv, logicalName, "\n");
#endif
    }
  CL_PrintPackedClassLinks (theEnv, logicalName, "Direct Superclasses:",
			    &theDefclass->directSuperclasses);
  CL_PrintPackedClassLinks (theEnv, logicalName, "Inheritance Precedence:",
			    &theDefclass->allSuperclasses);
  CL_PrintPackedClassLinks (theEnv, logicalName, "Direct Subclasses:",
			    &theDefclass->directSubclasses);
  if (theDefclass->instanceTemplate != NULL)
    {
      DisplaySeparator (theEnv, logicalName, buf, 82, '-');
      maxSlotNameLength = 5;
      maxOverrideMessageLength = 8;
      for (i = 0; i < theDefclass->instanceSlotCount; i++)
	{
	  slotNameLength =
	    strlen (theDefclass->instanceTemplate[i]->slotName->
		    name->contents);
	  if (slotNameLength > maxSlotNameLength)
	    maxSlotNameLength = slotNameLength;
	  if (theDefclass->instanceTemplate[i]->no_Write == 0)
	    {
	      overrideMessageLength =
		strlen (theDefclass->instanceTemplate[i]->
			overrideMessage->contents);
	      if (overrideMessageLength > maxOverrideMessageLength)
		maxOverrideMessageLength = overrideMessageLength;
	    }
	}
      if (maxSlotNameLength > 16)
	maxSlotNameLength = 16;
      if (maxOverrideMessageLength > 12)
	maxOverrideMessageLength = 12;
/*        
#if WIN_MVC
      CL_gensprintf(slotNamePrintFo_rmat,"%%-%Id.%Ids : ",maxSlotNameLength,maxSlotNameLength);
      CL_gensprintf(overrideMessagePrintFo_rmat,"%%-%Id.%Ids ",maxOverrideMessageLength,
                                              maxOverrideMessageLength);
#elif WIN_GCC
      CL_gensprintf(slotNamePrintFo_rmat,"%%-%ld.%lds : ",(long) maxSlotNameLength,(long) maxSlotNameLength);
      CL_gensprintf(overrideMessagePrintFo_rmat,"%%-%ld.%lds ",(long) maxOverrideMessageLength,
                                            (long) maxOverrideMessageLength);
#else
*/
      CL_gensprintf (slotNamePrintFo_rmat, "%%-%zd.%zds : ",
		     maxSlotNameLength, maxSlotNameLength);
      CL_gensprintf (overrideMessagePrintFo_rmat, "%%-%zd.%zds ",
		     maxOverrideMessageLength, maxOverrideMessageLength);
/*
#endif
*/
      DisplaySlotBasicInfo (theEnv, logicalName, slotNamePrintFo_rmat,
			    overrideMessagePrintFo_rmat, buf, theDefclass);
      CL_WriteString (theEnv, logicalName,
		      "\nConstraint info_rmation for slots:\n\n");
      DisplaySlotConstraintInfo (theEnv, logicalName, slotNamePrintFo_rmat,
				 buf, 82, theDefclass);
    }
  if (theDefclass->handlerCount > 0)
    messageBanner = true;
  else
    {
      messageBanner = false;
      for (i = 1; i < theDefclass->allSuperclasses.classCount; i++)
	if (theDefclass->allSuperclasses.classArray[i]->handlerCount > 0)
	  {
	    messageBanner = true;
	    break;
	  }
    }
  if (messageBanner)
    {
      DisplaySeparator (theEnv, logicalName, buf, 82, '-');
      CL_WriteString (theEnv, logicalName, "Recognized message-handlers:\n");
      CL_DisplayHandlersInLinks (theEnv, logicalName,
				 &theDefclass->allSuperclasses, 0);
    }
  DisplaySeparator (theEnv, logicalName, buf, 82, '*');
  DisplaySeparator (theEnv, logicalName, buf, 82, '=');
}

#endif /* DEBUGGING_FUNCTIONS */

/**********************************************************
  NAME         : CL_GetCreateAccessorString
  DESCRIPTION  : Gets a string describing which
                 accessors are implicitly created
                 for a slot: R, W, RW or NIL
  INPUTS       : The slot descriptor
  RETURNS      : The string description
  SIDE EFFECTS : None
  NOTES        : Used by (describe-class) and (slot-facets)
 **********************************************************/
const char *
CL_GetCreateAccessorString (SlotDescriptor * sd)
{
  if (sd->createReadAccessor && sd->create_WriteAccessor)
    return ("RW");

  if ((sd->createReadAccessor == 0) && (sd->create_WriteAccessor == 0))
    return ("NIL");
  else
    {
      if (sd->createReadAccessor)
	return "R";
      else
	return "W";
    }
}

/************************************************************
  NAME         : Get_DefclassModuleCommand
  DESCRIPTION  : Dete_rmines to which module a class belongs
  INPUTS       : None
  RETURNS      : The symbolic name of the module
  SIDE EFFECTS : None
  NOTES        : H/L Syntax: (defclass-module <class-name>)
 ************************************************************/
void
Get_DefclassModuleCommand (Environment * theEnv,
			   UDFContext * context, UDFValue * returnValue)
{
  returnValue->value =
    CL_GetConstructModuleCommand (context, "defclass-module",
				  DefclassData (theEnv)->DefclassConstruct);
}

/*********************************************************************
  NAME         : CL_SuperclassPCommand
  DESCRIPTION  : Dete_rmines if a class is a superclass of another
  INPUTS       : None
  RETURNS      : True if class-1 is a superclass of class-2
  SIDE EFFECTS : None
  NOTES        : H/L Syntax : (superclassp <class-1> <class-2>)
 *********************************************************************/
void
CL_SuperclassPCommand (Environment * theEnv,
		       UDFContext * context, UDFValue * returnValue)
{
  Defclass *c1, *c2;

  if (CheckTwoClasses (context, "superclassp", &c1, &c2) == false)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
      return;
    }

  returnValue->lexemeValue =
    CL_CreateBoolean (theEnv, CL_SuperclassP (c1, c2));
}

/***************************************************
  NAME         : CL_SuperclassP
  DESCRIPTION  : Dete_rmines if the first class is
                 a superclass of the other
  INPUTS       : 1) First class
                 2) Second class
  RETURNS      : True if first class is a
                 superclass of the first,
                 false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
bool
CL_SuperclassP (Defclass * firstClass, Defclass * secondClass)
{
  return CL_HasSuperclass (secondClass, firstClass);
}

/*********************************************************************
  NAME         : CL_SubclassPCommand
  DESCRIPTION  : Dete_rmines if a class is a subclass of another
  INPUTS       : None
  RETURNS      : True if class-1 is a subclass of class-2
  SIDE EFFECTS : None
  NOTES        : H/L Syntax : (subclassp <class-1> <class-2>)
 *********************************************************************/
void
CL_SubclassPCommand (Environment * theEnv,
		     UDFContext * context, UDFValue * returnValue)
{
  Defclass *c1, *c2;

  if (CheckTwoClasses (context, "subclassp", &c1, &c2) == false)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
      return;
    }

  returnValue->lexemeValue = CL_CreateBoolean (theEnv, CL_SubclassP (c1, c2));
}

/***************************************************
  NAME         : CL_SubclassP
  DESCRIPTION  : Dete_rmines if the first class is
                 a subclass of the other
  INPUTS       : 1) First class
                 2) Second class
  RETURNS      : True if first class is a
                 subclass of the first,
                 false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
bool
CL_SubclassP (Defclass * firstClass, Defclass * secondClass)
{
  return CL_HasSuperclass (firstClass, secondClass);
}

/*********************************************************************
  NAME         : CL_SlotExistPCommand
  DESCRIPTION  : Dete_rmines if a slot is present in a class
  INPUTS       : None
  RETURNS      : True if the slot exists, false otherwise
  SIDE EFFECTS : None
  NOTES        : H/L Syntax : (slot-existp <class> <slot> [inherit])
 *********************************************************************/
void
CL_SlotExistPCommand (Environment * theEnv,
		      UDFContext * context, UDFValue * returnValue)
{
  Defclass *cls;
  SlotDescriptor *sd;
  bool inheritFlag = false;
  UDFValue theArg;

  sd = CheckSlotExists (context, "slot-existp", &cls, false, true);
  if (sd == NULL)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
      return;
    }

  if (UDFHasNextArgument (context))
    {
      if (!CL_UDFNextArgument (context, SYMBOL_BIT, &theArg))
	{
	  return;
	}

      if (strcmp (theArg.lexemeValue->contents, "inherit") != 0)
	{
	  CL_UDFInvalidArgumentMessage (context, "keyword \"inherit\"");
	  Set_EvaluationError (theEnv, true);
	  returnValue->lexemeValue = FalseSymbol (theEnv);
	  return;
	}
      inheritFlag = true;
    }

  returnValue->lexemeValue =
    CL_CreateBoolean (theEnv, ((sd->cls == cls) ? true : inheritFlag));
}

/***************************************************
  NAME         : CL_SlotExistP
  DESCRIPTION  : Dete_rmines if a slot exists
  INPUTS       : 1) The class
                 2) The slot name
                 3) A flag indicating if the slot
                    can be inherited or not
  RETURNS      : True if slot exists,
                 false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
bool
CL_SlotExistP (Defclass * theDefclass, const char *slotName, bool inheritFlag)
{
  Environment *theEnv = theDefclass->header.env;

  return (LookupSlot (theEnv, theDefclass, slotName, inheritFlag) != NULL)
    ? true : false;
}

/************************************************************************************
  NAME         : CL_MessageHandlerExistPCommand
  DESCRIPTION  : Dete_rmines if a message-handler is present in a class
  INPUTS       : None
  RETURNS      : True if the message header is present, false otherwise
  SIDE EFFECTS : None
  NOTES        : H/L Syntax : (message-handler-existp <class> <hnd> [<type>])
 ************************************************************************************/
void
CL_MessageHandlerExistPCommand (Environment * theEnv,
				UDFContext * context, UDFValue * returnValue)
{
  Defclass *cls;
  CLIPSLexeme *mname;
  UDFValue theArg;
  unsigned mtype = MPRIMARY;

  if (!CL_UDFFirstArgument (context, SYMBOL_BIT, &theArg))
    {
      return;
    }
  cls = CL_LookupDefclassByMdlOrScope (theEnv, theArg.lexemeValue->contents);
  if (cls == NULL)
    {
      CL_ClassExistError (theEnv, "message-handler-existp",
			  theArg.lexemeValue->contents);
      returnValue->lexemeValue = FalseSymbol (theEnv);
      return;
    }

  if (!CL_UDFNextArgument (context, SYMBOL_BIT, &theArg))
    {
      return;
    }

  mname = theArg.lexemeValue;
  if (UDFHasNextArgument (context))
    {
      if (!CL_UDFNextArgument (context, SYMBOL_BIT, &theArg))
	{
	  return;
	}

      mtype =
	CL_HandlerType (theEnv, "message-handler-existp", true,
			theArg.lexemeValue->contents);
      if (mtype == MERROR)
	{
	  Set_EvaluationError (theEnv, true);
	  returnValue->lexemeValue = FalseSymbol (theEnv);
	  return;
	}
    }

  if (CL_FindHandlerByAddress (cls, mname, mtype) != NULL)
    {
      returnValue->lexemeValue = TrueSymbol (theEnv);
    }
  else
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
    }
}

/**********************************************************************
  NAME         : CL_SlotWritablePCommand
  DESCRIPTION  : Dete_rmines if an existing slot can be written to
  INPUTS       : None
  RETURNS      : True if the slot is writable, false otherwise
  SIDE EFFECTS : None
  NOTES        : H/L Syntax : (slot-writablep <class> <slot>)
 **********************************************************************/
void
CL_SlotWritablePCommand (Environment * theEnv,
			 UDFContext * context, UDFValue * returnValue)
{
  Defclass *theDefclass;
  SlotDescriptor *sd;

  sd = CheckSlotExists (context, "slot-writablep", &theDefclass, true, true);
  if (sd == NULL)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
    }
  else
    {
      returnValue->lexemeValue =
	CL_CreateBoolean (theEnv,
			  (sd->no_Write
			   || sd->initializeOnly) ? false : true);
    }
}

/***************************************************
  NAME         : CL_SlotWritableP
  DESCRIPTION  : Dete_rmines if a slot is writable
  INPUTS       : 1) The class
                 2) The slot name
  RETURNS      : True if slot is writable,
                 false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
bool
CL_SlotWritableP (Defclass * theDefclass, const char *slotName)
{
  SlotDescriptor *sd;
  Environment *theEnv = theDefclass->header.env;

  if ((sd = LookupSlot (theEnv, theDefclass, slotName, true)) == NULL)
    return false;
  return ((sd->no_Write || sd->initializeOnly) ? false : true);
}

/**********************************************************************
  NAME         : CL_SlotInitablePCommand
  DESCRIPTION  : Dete_rmines if an existing slot can be initialized
                   via an init message-handler or slot-override
  INPUTS       : None
  RETURNS      : True if the slot is writable, false otherwise
  SIDE EFFECTS : None
  NOTES        : H/L Syntax : (slot-initablep <class> <slot>)
 **********************************************************************/
void
CL_SlotInitablePCommand (Environment * theEnv,
			 UDFContext * context, UDFValue * returnValue)
{
  Defclass *theDefclass;
  SlotDescriptor *sd;

  sd = CheckSlotExists (context, "slot-initablep", &theDefclass, true, true);
  if (sd == NULL)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
    }
  else
    {
      returnValue->lexemeValue =
	CL_CreateBoolean (theEnv,
			  (sd->no_Write
			   && (sd->initializeOnly == 0)) ? false : true);
    }
}

/***************************************************
  NAME         : CL_SlotInitableP
  DESCRIPTION  : Dete_rmines if a slot is initable
  INPUTS       : 1) The class
                 2) The slot name
  RETURNS      : True if slot is initable,
                 false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
bool
CL_SlotInitableP (Defclass * theDefclass, const char *slotName)
{
  SlotDescriptor *sd;
  Environment *theEnv = theDefclass->header.env;

  if ((sd = LookupSlot (theEnv, theDefclass, slotName, true)) == NULL)
    return false;
  return ((sd->no_Write && (sd->initializeOnly == 0)) ? false : true);
}

/**********************************************************************
  NAME         : CL_SlotPublicPCommand
  DESCRIPTION  : Dete_rmines if an existing slot is publicly visible
                   for direct reference by subclasses
  INPUTS       : None
  RETURNS      : True if the slot is public, false otherwise
  SIDE EFFECTS : None
  NOTES        : H/L Syntax : (slot-publicp <class> <slot>)
 **********************************************************************/
void
CL_SlotPublicPCommand (Environment * theEnv,
		       UDFContext * context, UDFValue * returnValue)
{
  Defclass *theDefclass;
  SlotDescriptor *sd;

  sd = CheckSlotExists (context, "slot-publicp", &theDefclass, true, false);
  if (sd == NULL)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
    }
  else
    {
      returnValue->lexemeValue =
	CL_CreateBoolean (theEnv, (sd->publicVisibility ? true : false));
    }
}

/***************************************************
  NAME         : CL_SlotPublicP
  DESCRIPTION  : Dete_rmines if a slot is public
  INPUTS       : 1) The class
                 2) The slot name
  RETURNS      : True if slot is public,
                 false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
bool
CL_SlotPublicP (Defclass * theDefclass, const char *slotName)
{
  SlotDescriptor *sd;
  Environment *theEnv = theDefclass->header.env;

  if ((sd = LookupSlot (theEnv, theDefclass, slotName, false)) == NULL)
    return false;
  return (sd->publicVisibility ? true : false);
}

/***************************************************
  NAME         : CL_SlotDefaultP
  DESCRIPTION  : Dete_rmines if a slot has a default value
  INPUTS       : 1) The class
                 2) The slot name
  RETURNS      : True if slot is public,
                 false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
int
CL_SlotDefaultP (Environment * theEnv,
		 Defclass * theDefclass, const char *slotName)
{
  SlotDescriptor *sd;

  if ((sd = LookupSlot (theEnv, theDefclass, slotName, false)) == NULL)
    return (NO_DEFAULT);

  if (sd->noDefault)
    {
      return (NO_DEFAULT);
    }
  else if (sd->dynamicDefault)
    {
      return (DYNAMIC_DEFAULT);
    }

  return (STATIC_DEFAULT);
}


/**********************************************************************
  NAME         : CL_SlotDirectAccessPCommand
  DESCRIPTION  : Dete_rmines if an existing slot can be directly
                   referenced by the class - i.e., if the slot is
                   private, is the slot defined in the class
  INPUTS       : None
  RETURNS      : True if the slot is private,
                    false otherwise
  SIDE EFFECTS : None
  NOTES        : H/L Syntax : (slot-direct-accessp <class> <slot>)
 **********************************************************************/
void
CL_SlotDirectAccessPCommand (Environment * theEnv,
			     UDFContext * context, UDFValue * returnValue)
{
  Defclass *theDefclass;
  SlotDescriptor *sd;

  sd =
    CheckSlotExists (context, "slot-direct-accessp", &theDefclass, true,
		     true);
  if (sd == NULL)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
    }
  else
    {
      returnValue->lexemeValue =
	CL_CreateBoolean (theEnv,
			  ((sd->publicVisibility
			    || (sd->cls == theDefclass)) ? true : false));
    }
}

/***************************************************
  NAME         : CL_SlotDirectAccessP
  DESCRIPTION  : Dete_rmines if a slot is directly
                 accessible from message-handlers
                 on class
  INPUTS       : 1) The class
                 2) The slot name
  RETURNS      : True if slot is directly
                 accessible, false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
bool
CL_SlotDirectAccessP (Defclass * theDefclass, const char *slotName)
{
  SlotDescriptor *sd;
  Environment *theEnv = theDefclass->header.env;

  if ((sd = LookupSlot (theEnv, theDefclass, slotName, true)) == NULL)
    return false;
  return ((sd->publicVisibility || (sd->cls == theDefclass)) ? true : false);
}

/**********************************************************************
  NAME         : CL_SlotDefaultValueCommand
  DESCRIPTION  : Dete_rmines the default avlue for the specified slot
                 of the specified class
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : H/L Syntax : (slot-default-value <class> <slot>)
 **********************************************************************/
void
CL_SlotDefaultValueCommand (Environment * theEnv,
			    UDFContext * context, UDFValue * returnValue)
{
  Defclass *theDefclass;
  SlotDescriptor *sd;

  returnValue->lexemeValue = FalseSymbol (theEnv);

  sd =
    CheckSlotExists (context, "slot-default-value", &theDefclass, true, true);
  if (sd == NULL)
    return;

  if (sd->noDefault)
    {
      returnValue->lexemeValue = CL_CreateSymbol (theEnv, "?NONE");
      return;
    }

  if (sd->dynamicDefault)
    CL_EvaluateAndStoreInDataObject (theEnv, sd->multiple,
				     (Expression *) sd->defaultValue,
				     returnValue, true);
  else
    GenCopyMemory (UDFValue, 1, returnValue, sd->defaultValue);
}

/*********************************************************
  NAME         : CL_SlotDefaultValue
  DESCRIPTION  : Dete_rmines the default value for
                 the specified slot of the specified class
  INPUTS       : 1) The class
                 2) The slot name
  RETURNS      : True if slot default value is set,
                 false otherwise
  SIDE EFFECTS : Slot default value evaluated - dynamic
                 defaults will cause any side effects
  NOTES        : None
 *********************************************************/
bool
CL_SlotDefaultValue (Defclass * theDefclass,
		     const char *slotName, CLIPSValue * theValue)
{
  SlotDescriptor *sd;
  bool rv;
  UDFValue result;
  UDFValue *tmpPtr;
  Environment *theEnv = theDefclass->header.env;

  theValue->value = FalseSymbol (theEnv);
  if ((sd = LookupSlot (theEnv, theDefclass, slotName, true)) == NULL)
    {
      return false;
    }

  if (sd->noDefault)
    {
      theValue->value = CL_CreateSymbol (theEnv, "?NONE");
      return true;
    }

  if (sd->dynamicDefault)
    {
      rv = CL_EvaluateAndStoreInDataObject (theEnv, sd->multiple,
					    (Expression *) sd->defaultValue,
					    &result, true);
      CL_No_rmalizeMultifield (theEnv, &result);
      theValue->value = result.value;
      return rv;
    }

  tmpPtr = (UDFValue *) sd->defaultValue;
  theValue->value = tmpPtr->value;
  return true;
}

/********************************************************
  NAME         : CL_ClassExistPCommand
  DESCRIPTION  : Dete_rmines if a class exists
  INPUTS       : None
  RETURNS      : True if class exists, false otherwise
  SIDE EFFECTS : None
  NOTES        : H/L Syntax : (class-existp <arg>)
 ********************************************************/
void
CL_ClassExistPCommand (Environment * theEnv,
		       UDFContext * context, UDFValue * returnValue)
{
  UDFValue theArg;

  if (!CL_UDFFirstArgument (context, SYMBOL_BIT, &theArg))
    {
      return;
    }

  returnValue->lexemeValue =
    CL_CreateBoolean (theEnv,
		      ((CL_LookupDefclassByMdlOrScope
			(theEnv,
			 theArg.lexemeValue->contents) !=
			NULL) ? true : false));
}

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/******************************************************
  NAME         : CheckTwoClasses
  DESCRIPTION  : Checks for exactly two class arguments
                    for a H/L function
  INPUTS       : 1) The function name
                 2) Caller's buffer for first class
                 3) Caller's buffer for second class
  RETURNS      : True if both found, false otherwise
  SIDE EFFECTS : Caller's buffers set
  NOTES        : Assumes exactly 2 arguments
 ******************************************************/
static bool
CheckTwoClasses (UDFContext * context,
		 const char *func, Defclass ** c1, Defclass ** c2)
{
  UDFValue theArg;
  Environment *theEnv = context->environment;

  if (!CL_UDFFirstArgument (context, SYMBOL_BIT, &theArg))
    {
      return false;
    }

  *c1 = CL_LookupDefclassByMdlOrScope (theEnv, theArg.lexemeValue->contents);
  if (*c1 == NULL)
    {
      CL_ClassExistError (theEnv, func, theArg.lexemeValue->contents);
      return false;
    }

  if (!CL_UDFNextArgument (context, SYMBOL_BIT, &theArg))
    {
      return false;
    }

  *c2 = CL_LookupDefclassByMdlOrScope (theEnv, theArg.lexemeValue->contents);
  if (*c2 == NULL)
    {
      CL_ClassExistError (theEnv, func, theArg.lexemeValue->contents);
      return false;
    }

  return true;
}

/***************************************************
  NAME         : CheckSlotExists
  DESCRIPTION  : Checks first two arguments of
                 a function for a valid class
                 and (inherited) slot
  INPUTS       : 1) The name of the function
                 2) A buffer to hold the found class
                 3) A flag indicating whether the
                    non-existence of the slot should
                    be an error
                 4) A flag indicating if the slot
                    can be inherited or not
  RETURNS      : NULL if slot not found, slot
                 descriptor otherwise
  SIDE EFFECTS : Class buffer set if no errors,
                 NULL on errors
  NOTES        : None
 ***************************************************/
static SlotDescriptor *
CheckSlotExists (UDFContext * context,
		 const char *func,
		 Defclass ** classBuffer,
		 bool existsErrorFlag, bool inheritFlag)
{
  CLIPSLexeme *ssym;
  int slotIndex;
  SlotDescriptor *sd;
  Environment *theEnv = context->environment;

  ssym = CL_CheckClassAndSlot (context, func, classBuffer);
  if (ssym == NULL)
    return NULL;

  slotIndex = CL_FindInstanceTemplateSlot (theEnv, *classBuffer, ssym);
  if (slotIndex == -1)
    {
      if (existsErrorFlag)
	{
	  CL_SlotExistError (theEnv, ssym->contents, func);
	  Set_EvaluationError (theEnv, true);
	}
      return NULL;
    }

  sd = (*classBuffer)->instanceTemplate[slotIndex];
  if ((sd->cls == *classBuffer) || inheritFlag)
    {
      return sd;
    }

  CL_PrintErrorID (theEnv, "CLASSEXM", 1, false);
  CL_WriteString (theEnv, STDERR, "Inherited slot '");
  CL_WriteString (theEnv, STDERR, ssym->contents);
  CL_WriteString (theEnv, STDERR, "' from class ");
  CL_PrintClassName (theEnv, STDERR, sd->cls, true, false);
  CL_WriteString (theEnv, STDERR, " is not valid for function '");
  CL_WriteString (theEnv, STDERR, func);
  CL_WriteString (theEnv, STDERR, "'.\n");
  Set_EvaluationError (theEnv, true);
  return NULL;
}

/***************************************************
  NAME         : LookupSlot
  DESCRIPTION  : Finds a slot in a class
  INPUTS       : 1) The class
                 2) The slot name
                 3) A flag indicating if inherited
                    slots are OK or not
  RETURNS      : The slot descriptor address, or
                 NULL if not found
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
static SlotDescriptor *
LookupSlot (Environment * theEnv,
	    Defclass * theDefclass, const char *slotName, bool inheritFlag)
{
  CLIPSLexeme *slotSymbol;
  int slotIndex;
  SlotDescriptor *sd;

  slotSymbol = CL_FindSymbolHN (theEnv, slotName, SYMBOL_BIT);
  if (slotSymbol == NULL)
    {
      return NULL;
    }

  slotIndex = CL_FindInstanceTemplateSlot (theEnv, theDefclass, slotSymbol);
  if (slotIndex == -1)
    {
      return NULL;
    }

  sd = theDefclass->instanceTemplate[slotIndex];
  if ((sd->cls != theDefclass) && (inheritFlag == false))
    {
      return NULL;
    }

  return sd;
}

#if DEBUGGING_FUNCTIONS

/*****************************************************
  NAME         : CheckClass
  DESCRIPTION  : Used for to check class name for
                 class accessor functions such
                 as ppdefclass and undefclass
  INPUTS       : 1) The name of the H/L function
                 2) Name of the class
  RETURNS      : The class address,
                   or NULL if ther was an error
  SIDE EFFECTS : None
  NOTES        : None
 ******************************************************/
static Defclass *
CheckClass (Environment * theEnv, const char *func, const char *cname)
{
  Defclass *cls;

  cls = CL_LookupDefclassByMdlOrScope (theEnv, cname);
  if (cls == NULL)
    CL_ClassExistError (theEnv, func, cname);
  return (cls);
}

/*********************************************************
  NAME         : GetClassNameArgument
  DESCRIPTION  : Gets a class name-string
  INPUTS       : Calling function name
  RETURNS      : Class name (NULL on errors)
  SIDE EFFECTS : None
  NOTES        : Assumes only 1 argument
 *********************************************************/
static const char *
GetClassNameArgument (UDFContext * context)
{
  UDFValue theArg;

  if (!CL_UDFFirstArgument (context, SYMBOL_BIT, &theArg))
    {
      return NULL;
    }

  return theArg.lexemeValue->contents;
}

/****************************************************************
  NAME         : PrintClassBrowse
  DESCRIPTION  : Displays a "graph" of class and subclasses
  INPUTS       : 1) The logical name of the output
                 2) The class address
                 3) The depth of the graph
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ****************************************************************/
static void
PrintClassBrowse (Environment * theEnv,
		  const char *logicalName,
		  Defclass * cls, unsigned long depth)
{
  unsigned long i;

  for (i = 0; i < depth; i++)
    CL_WriteString (theEnv, logicalName, "  ");
  CL_WriteString (theEnv, logicalName, CL_DefclassName (cls));
  if (cls->directSuperclasses.classCount > 1)
    CL_WriteString (theEnv, logicalName, " *");
  CL_WriteString (theEnv, logicalName, "\n");
  for (i = 0; i < cls->directSubclasses.classCount; i++)
    PrintClassBrowse (theEnv, logicalName,
		      cls->directSubclasses.classArray[i], depth + 1);
}

/*********************************************************
  NAME         : DisplaySeparator
  DESCRIPTION  : Prints a separator line for CL_DescribeClass
  INPUTS       : 1) The logical name of the output
                 2) The buffer to use for the line
                 3) The buffer size
                 4) The character to use
  RETURNS      : Nothing useful
  SIDE EFFECTS : Buffer overwritten and displayed
  NOTES        : None
 *********************************************************/
static void
DisplaySeparator (Environment * theEnv,
		  const char *logicalName, char *buf, int maxlen, int sepchar)
{
  int i;

  for (i = 0; i < maxlen - 2; i++)
    buf[i] = (char) sepchar;
  buf[i++] = '\n';
  buf[i] = '\0';
  CL_WriteString (theEnv, logicalName, buf);
}

/*************************************************************
  NAME         : DisplaySlotBasicInfo
  DESCRIPTION  : Displays a table summary of basic
                  facets for the slots of a class
                  including:
                  single/multiple
                  default/no-default/default-dynamic
                  inherit/no-inherit
                  read-write/initialize-only/read-only
                  local/shared
                  composite/exclusive
                  reactive/non-reactive
                  public/private
                  create-accessor read/write
                  override-message

                  The function also displays the source
                  class(es) for the facets
  INPUTS       : 1) The logical name of the output
                 2) A fo_rmat string for use in sprintf
                    (for printing slot names)
                 3) A fo_rmat string for use in sprintf
                    (for printing slot override message names)
                 4) A buffer to store the display in
                 5) A pointer to the class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Buffer written to and displayed
  NOTES        : None
 *************************************************************/
static void
DisplaySlotBasicInfo (Environment * theEnv,
		      const char *logicalName,
		      const char *slotNamePrintFo_rmat,
		      const char *overrideMessagePrintFo_rmat,
		      char *buf, Defclass * cls)
{
  long i;
  SlotDescriptor *sp;
  const char *createString;

  CL_gensprintf (buf, slotNamePrintFo_rmat, "SLOTS");
#if DEFRULE_CONSTRUCT
  CL_genstrcat (buf, "FLD DEF PRP ACC STO MCH SRC VIS CRT ");
#else
  CL_genstrcat (buf, "FLD DEF PRP ACC STO SRC VIS CRT ");
#endif
  CL_WriteString (theEnv, logicalName, buf);
  CL_gensprintf (buf, overrideMessagePrintFo_rmat, "OVRD-MSG");
  CL_WriteString (theEnv, logicalName, buf);
  CL_WriteString (theEnv, logicalName, "SOURCE(S)\n");
  for (i = 0; i < cls->instanceSlotCount; i++)
    {
      sp = cls->instanceTemplate[i];
      CL_gensprintf (buf, slotNamePrintFo_rmat, sp->slotName->name->contents);
      CL_genstrcat (buf, sp->multiple ? "MLT " : "SGL ");
      if (sp->noDefault)
	CL_genstrcat (buf, "NIL ");
      else
	CL_genstrcat (buf, sp->dynamicDefault ? "DYN " : "STC ");
      CL_genstrcat (buf, sp->noInherit ? "NIL " : "INH ");
      if (sp->initializeOnly)
	CL_genstrcat (buf, "INT ");
      else if (sp->no_Write)
	CL_genstrcat (buf, " R  ");
      else
	CL_genstrcat (buf, "RW  ");
      CL_genstrcat (buf, sp->shared ? "SHR " : "LCL ");
#if DEFRULE_CONSTRUCT
      CL_genstrcat (buf, sp->reactive ? "RCT " : "NIL ");
#endif
      CL_genstrcat (buf, sp->composite ? "CMP " : "EXC ");
      CL_genstrcat (buf, sp->publicVisibility ? "PUB " : "PRV ");
      createString = CL_GetCreateAccessorString (sp);
      if (createString[1] == '\0')
	CL_genstrcat (buf, " ");
      CL_genstrcat (buf, createString);
      if ((createString[1] == '\0') ? true : (createString[2] == '\0'))
	CL_genstrcat (buf, " ");
      CL_genstrcat (buf, " ");
      CL_WriteString (theEnv, logicalName, buf);
      CL_gensprintf (buf, overrideMessagePrintFo_rmat,
		     sp->no_Write ? "NIL" : sp->overrideMessage->contents);
      CL_WriteString (theEnv, logicalName, buf);
      CL_Print_SlotSources (theEnv, logicalName, sp->slotName->name,
			    &sp->cls->allSuperclasses, 0, true);
      CL_WriteString (theEnv, logicalName, "\n");
    }
}

/***************************************************
  NAME         : CL_Print_SlotSources
  DESCRIPTION  : Displays a list of source classes
                   for a composite class (in order
                   of most general to specific)
  INPUTS       : 1) The logical name of the output
                 2) The name of the slot
                 3) The precedence list of the class
                    of the slot (the source class
                    shold be first in the list)
                 4) The index into the packed
                    links array
                 5) Flag indicating whether to
                    disregard noniherit facet
  RETURNS      : True if a class is printed, false
                 otherwise
  SIDE EFFECTS : Recursively prints out appropriate
                 memebers from list in reverse order
  NOTES        : None
 ***************************************************/
static bool
CL_Print_SlotSources (Environment * theEnv,
		      const char *logicalName,
		      CLIPSLexeme * sname,
		      PACKED_CLASS_LINKS * sprec,
		      unsigned long theIndex, bool inhp)
{
  SlotDescriptor *csp;

  if (theIndex == sprec->classCount)
    return false;
  csp = CL_FindClassSlot (sprec->classArray[theIndex], sname);
  if ((csp != NULL) ? ((csp->noInherit == 0) || inhp) : false)
    {
      if (csp->composite)
	{
	  if (CL_Print_SlotSources
	      (theEnv, logicalName, sname, sprec, theIndex + 1, false))
	    CL_WriteString (theEnv, logicalName, " ");
	}
      CL_PrintClassName (theEnv, logicalName, sprec->classArray[theIndex],
			 false, false);
      return true;
    }
  else
    return (CL_Print_SlotSources
	    (theEnv, logicalName, sname, sprec, theIndex + 1, false));
}

/*********************************************************
  NAME         : DisplaySlotConstraintInfo
  DESCRIPTION  : Displays a table summary of type-checking
                  facets for the slots of a class
                  including:
                  type
                  allowed-symbols
                  allowed-integers
                  allowed-floats
                  allowed-values
                  allowed-instance-names
                  range
                  min-number-of-elements
                  max-number-of-elements

                  The function also displays the source
                  class(es) for the facets
  INPUTS       : 1) A fo_rmat string for use in sprintf
                 2) A buffer to store the display in
                 3) Maximum buffer size
                 4) A pointer to the class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Buffer written to and displayed
  NOTES        : None
 *********************************************************/
static void
DisplaySlotConstraintInfo (Environment * theEnv,
			   const char *logicalName,
			   const char *slotNamePrintFo_rmat,
			   char *buf, unsigned maxlen, Defclass * cls)
{
  long i;
  CONSTRAINT_RECORD *cr;
  const char *strdest = "***describe-class***";

  CL_gensprintf (buf, slotNamePrintFo_rmat, "SLOTS");
  CL_genstrcat (buf, "SYM STR INN INA EXA FTA INT FLT\n");
  CL_WriteString (theEnv, logicalName, buf);
  for (i = 0; i < cls->instanceSlotCount; i++)
    {
      cr = cls->instanceTemplate[i]->constraint;
      CL_gensprintf (buf, slotNamePrintFo_rmat,
		     cls->instanceTemplate[i]->slotName->name->contents);
      if (cr != NULL)
	{
	  CL_genstrcat (buf,
			ConstraintCode (cr, cr->symbolsAllowed,
					cr->symbolRestriction));
	  CL_genstrcat (buf,
			ConstraintCode (cr, cr->stringsAllowed,
					cr->stringRestriction));
	  CL_genstrcat (buf,
			ConstraintCode (cr, cr->instanceNamesAllowed,
					(cr->instanceNameRestriction
					 || cr->classRestriction)));
	  CL_genstrcat (buf,
			ConstraintCode (cr, cr->instanceAddressesAllowed,
					cr->classRestriction));
	  CL_genstrcat (buf,
			ConstraintCode (cr, cr->externalAddressesAllowed, 0));
	  CL_genstrcat (buf,
			ConstraintCode (cr, cr->factAddressesAllowed, 0));
	  CL_genstrcat (buf,
			ConstraintCode (cr, cr->integersAllowed,
					cr->integerRestriction));
	  CL_genstrcat (buf,
			ConstraintCode (cr, cr->floatsAllowed,
					cr->floatRestriction));
	  CL_OpenStringDestination (theEnv, strdest, buf + strlen (buf),
				    (maxlen - strlen (buf) - 1));
	  if (cr->integersAllowed || cr->floatsAllowed || cr->anyAllowed)
	    {
	      CL_WriteString (theEnv, strdest, "RNG:[");
	      CL_PrintExpression (theEnv, strdest, cr->minValue);
	      CL_WriteString (theEnv, strdest, "..");
	      CL_PrintExpression (theEnv, strdest, cr->maxValue);
	      CL_WriteString (theEnv, strdest, "] ");
	    }
	  if (cls->instanceTemplate[i]->multiple)
	    {
	      CL_WriteString (theEnv, strdest, "CRD:[");
	      CL_PrintExpression (theEnv, strdest, cr->minFields);
	      CL_WriteString (theEnv, strdest, "..");
	      CL_PrintExpression (theEnv, strdest, cr->maxFields);
	      CL_WriteString (theEnv, strdest, "]");
	    }
	}
      else
	{
	  CL_OpenStringDestination (theEnv, strdest, buf, maxlen);
	  CL_WriteString (theEnv, strdest,
			  " +   +   +   +   +   +   +   +  RNG:[-oo..+oo]");
	  if (cls->instanceTemplate[i]->multiple)
	    CL_WriteString (theEnv, strdest, " CRD:[0..+oo]");
	}
      CL_WriteString (theEnv, strdest, "\n");
      CL_CloseStringDestination (theEnv, strdest);
      CL_WriteString (theEnv, logicalName, buf);
    }
}

/******************************************************
  NAME         : ConstraintCode
  DESCRIPTION  : Gives a string code representing the
                 type of constraint
  INPUTS       : 1) The constraint record
                 2) Allowed Flag
                 3) Restricted Values flag
  RETURNS      : "    " for type not allowed
                 " +  " for any value of type allowed
                 " #  " for some values of type allowed
  SIDE EFFECTS : None
  NOTES        : Used by DisplaySlotConstraintInfo
 ******************************************************/
static const char *
ConstraintCode (CONSTRAINT_RECORD * cr,
		unsigned allow, unsigned restrictValues)
{
  if (allow || cr->anyAllowed)
    {
      if (restrictValues || cr->anyRestriction)
	return " #  ";
      else
	return " +  ";
    }
  return ("    ");
}

#endif

#endif
