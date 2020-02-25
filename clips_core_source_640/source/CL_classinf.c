   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*        CLASS INFO PROGRAMMATIC ACCESS MODULE        */
   /*******************************************************/

/**************************************************************/
/* Purpose: Class Info_rmation Interface Support Routines      */
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
/*            Changed name of variable exp to theExp          */
/*            because of Unix compiler warnings of shadowed   */
/*            definitions.                                    */
/*                                                            */
/*      6.24: Added allowed-classes slot facet.               */
/*                                                            */
/*            Converted INSTANCE_PATTERN_MATCHING to          */
/*            DEFRULE_CONSTRUCT.                              */
/*                                                            */
/*            Renamed BOOLEAN macro type to intBool.          */
/*                                                            */
/*      6.30: Borland C (IBM_TBC) and Metrowerks CodeWarrior  */
/*            (MAC_MCW, IBM_MCW) are no longer supported.     */
/*                                                            */
/*            Changed integer type/precision.                 */
/*                                                            */
/*            Added const qualifiers to remove C++            */
/*            deprecation warnings.                           */
/*                                                            */
/*            Converted API macros to function calls.         */
/*                                                            */
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

#include <stdio.h>
#include <string.h>

#include "argacces.h"
#include "classcom.h"
#include "classexm.h"
#include "classfun.h"
#include "classini.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "insfun.h"
#include "msgcom.h"
#include "msgfun.h"
#include "multifld.h"
#include "prntutil.h"

#include "classinf.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void SlotInfoSupportFunction (UDFContext *, UDFValue *, const char *,
				     bool (*)(Defclass *, const char *,
					      CLIPSValue *));
static unsigned CountSubclasses (Defclass *, bool, int);
static unsigned StoreSubclasses (Multifield *, unsigned, Defclass *, int, int,
				 bool);
static SlotDescriptor *SlotInfoSlot (Environment *, UDFValue *, Defclass *,
				     const char *, const char *);

/*********************************************************************
  NAME         : CL_ClassAbstractPCommand
  DESCRIPTION  : Dete_rmines if direct instances of a class can be made
  INPUTS       : None
  RETURNS      : True (1) if class is abstract, false (0) if concrete
  SIDE EFFECTS : None
  NOTES        : Syntax: (class-abstractp <class>)
 *********************************************************************/
void
CL_ClassAbstractPCommand (Environment * theEnv,
			  UDFContext * context, UDFValue * returnValue)
{
  UDFValue theArg;
  Defclass *cls;

  if (!CL_UDFFirstArgument (context, SYMBOL_BIT, &theArg))
    {
      return;
    }

  cls = CL_LookupDefclassByMdlOrScope (theEnv, theArg.lexemeValue->contents);
  if (cls == NULL)
    {
      CL_ClassExistError (theEnv, "class-abstractp",
			  theArg.lexemeValue->contents);
      returnValue->lexemeValue = FalseSymbol (theEnv);
      return;
    }

  returnValue->lexemeValue =
    CL_CreateBoolean (theEnv, (CL_ClassAbstractP (cls)));
}

#if DEFRULE_CONSTRUCT

/*****************************************************************
  NAME         : CL_ClassReactivePCommand
  DESCRIPTION  : Dete_rmines if instances of a class can match rule
                 patterns
  INPUTS       : None
  RETURNS      : True (1) if class is reactive, false (0)
                 if non-reactive
  SIDE EFFECTS : None
  NOTES        : Syntax: (class-reactivep <class>)
 *****************************************************************/
void
CL_ClassReactivePCommand (Environment * theEnv,
			  UDFContext * context, UDFValue * returnValue)
{
  UDFValue theArg;
  Defclass *cls;

  if (!CL_UDFFirstArgument (context, SYMBOL_BIT, &theArg))
    {
      return;
    }

  cls = CL_LookupDefclassByMdlOrScope (theEnv, theArg.lexemeValue->contents);
  if (cls == NULL)
    {
      CL_ClassExistError (theEnv, "class-reactivep",
			  theArg.lexemeValue->contents);
      returnValue->lexemeValue = FalseSymbol (theEnv);
      return;
    }

  returnValue->lexemeValue =
    CL_CreateBoolean (theEnv, CL_ClassReactiveP (cls));
}

#endif

/***********************************************************
  NAME         : CL_ClassInfoFnxArgs
  DESCRIPTION  : Examines arguments for:
                   class-slots, get-defmessage-handler-list,
                   class-superclasses and class-subclasses
  INPUTS       : 1) Name of function
                 2) A buffer to hold a flag indicating if
                    the inherit keyword was specified
  RETURNS      : Pointer to the class on success,
                   NULL on errors
  SIDE EFFECTS : inhp flag set
                 error flag set
  NOTES        : None
 ***********************************************************/
Defclass *
CL_ClassInfoFnxArgs (UDFContext * context, const char *fnx, bool *inhp)
{
  Defclass *clsptr;
  UDFValue theArg;
  Environment *theEnv = context->environment;

  *inhp = false;

  if (!CL_UDFFirstArgument (context, SYMBOL_BIT, &theArg))
    {
      return NULL;
    }

  clsptr =
    CL_LookupDefclassByMdlOrScope (theEnv, theArg.lexemeValue->contents);
  if (clsptr == NULL)
    {
      CL_ClassExistError (theEnv, fnx, theArg.lexemeValue->contents);
      return NULL;
    }

  if (UDFHasNextArgument (context))
    {
      if (!CL_UDFNextArgument (context, SYMBOL_BIT, &theArg))
	{
	  return NULL;
	}

      if (strcmp (theArg.lexemeValue->contents, "inherit") == 0)
	{
	  *inhp = true;
	}
      else
	{
	  CL_SyntaxErrorMessage (theEnv, fnx);
	  Set_EvaluationError (theEnv, true);
	  return NULL;
	}
    }

  return clsptr;
}

/********************************************************************
  NAME         : CL_ClassSlotsCommand
  DESCRIPTION  : Groups slot info for a class into a multifield value
                   for dynamic perusal
  INPUTS       : Data object buffer to hold the slots of the class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Creates a multifield storing the names of
                    the slots of the class
  NOTES        : Syntax: (class-slots <class> [inherit])
 ********************************************************************/
void
CL_ClassSlotsCommand (Environment * theEnv,
		      UDFContext * context, UDFValue * returnValue)
{
  bool inhp;
  Defclass *clsptr;
  CLIPSValue result;

  clsptr = CL_ClassInfoFnxArgs (context, "class-slots", &inhp);
  if (clsptr == NULL)
    {
      CL_SetMultifieldErrorValue (theEnv, returnValue);
      return;
    }
  CL_ClassSlots (clsptr, &result, inhp);
  CL_CLIPSToUDFValue (&result, returnValue);
}

/************************************************************************
  NAME         : CL_ClassSuperclassesCommand
  DESCRIPTION  : Groups superclasses for a class into a multifield value
                   for dynamic perusal
  INPUTS       : Data object buffer to hold the superclasses of the class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Creates a multifield storing the names of
                    the superclasses of the class
  NOTES        : Syntax: (class-superclasses <class> [inherit])
 ************************************************************************/
void
CL_ClassSuperclassesCommand (Environment * theEnv,
			     UDFContext * context, UDFValue * returnValue)
{
  bool inhp;
  Defclass *clsptr;
  CLIPSValue result;

  clsptr = CL_ClassInfoFnxArgs (context, "class-superclasses", &inhp);
  if (clsptr == NULL)
    {
      CL_SetMultifieldErrorValue (theEnv, returnValue);
      return;
    }
  CL_ClassSuperclasses (clsptr, &result, inhp);
  CL_CLIPSToUDFValue (&result, returnValue);
}

/************************************************************************
  NAME         : CL_ClassSubclassesCommand
  DESCRIPTION  : Groups subclasses for a class into a multifield value
                   for dynamic perusal
  INPUTS       : Data object buffer to hold the subclasses of the class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Creates a multifield storing the names of
                    the subclasses of the class
  NOTES        : Syntax: (class-subclasses <class> [inherit])
 ************************************************************************/
void
CL_ClassSubclassesCommand (Environment * theEnv,
			   UDFContext * context, UDFValue * returnValue)
{
  bool inhp;
  Defclass *clsptr;
  CLIPSValue result;

  clsptr = CL_ClassInfoFnxArgs (context, "class-subclasses", &inhp);
  if (clsptr == NULL)
    {
      CL_SetMultifieldErrorValue (theEnv, returnValue);
      return;
    }
  CL_ClassSubclasses (clsptr, &result, inhp);
  CL_CLIPSToUDFValue (&result, returnValue);
}

/***********************************************************************
  NAME         : CL_GetDefmessageHandlersListCmd
  DESCRIPTION  : Groups message-handlers for a class into a multifield
                   value for dynamic perusal
  INPUTS       : Data object buffer to hold the handlers of the class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Creates a multifield storing the names of
                    the message-handlers of the class
  NOTES        : Syntax: (get-defmessage-handler-list <class> [inherit])
 ***********************************************************************/
void
CL_GetDefmessageHandlersListCmd (Environment * theEnv,
				 UDFContext * context, UDFValue * returnValue)
{
  bool inhp;
  Defclass *clsptr;
  CLIPSValue result;

  if (!UDFHasNextArgument (context))
    {
      CL_GetDefmessageHandlerList (theEnv, NULL, &result, false);
      CL_CLIPSToUDFValue (&result, returnValue);
    }
  else
    {
      clsptr =
	CL_ClassInfoFnxArgs (context, "get-defmessage-handler-list", &inhp);
      if (clsptr == NULL)
	{
	  CL_SetMultifieldErrorValue (theEnv, returnValue);
	  return;
	}

      CL_GetDefmessageHandlerList (theEnv, clsptr, &result, inhp);
      CL_CLIPSToUDFValue (&result, returnValue);
    }
}

/*********************************
 Slot Info_rmation Access Functions
 *********************************/
void
CL_SlotFacetsCommand (Environment * theEnv,
		      UDFContext * context, UDFValue * returnValue)
{
  SlotInfoSupportFunction (context, returnValue, "slot-facets",
			   CL_SlotFacets);
}

void
CL_SlotSourcesCommand (Environment * theEnv,
		       UDFContext * context, UDFValue * returnValue)
{
  SlotInfoSupportFunction (context, returnValue, "slot-sources",
			   CL_SlotSources);
}

void
CL_SlotTypesCommand (Environment * theEnv,
		     UDFContext * context, UDFValue * returnValue)
{
  SlotInfoSupportFunction (context, returnValue, "slot-types", CL_SlotTypes);
}

void
CL_SlotAllowedValuesCommand (Environment * theEnv,
			     UDFContext * context, UDFValue * returnValue)
{
  SlotInfoSupportFunction (context, returnValue, "slot-allowed-values",
			   CL_SlotAllowedValues);
}

void
CL_SlotAllowedClassesCommand (Environment * theEnv,
			      UDFContext * context, UDFValue * returnValue)
{
  SlotInfoSupportFunction (context, returnValue, "slot-allowed-classes",
			   CL_SlotAllowedClasses);
}

void
CL_SlotRangeCommand (Environment * theEnv,
		     UDFContext * context, UDFValue * returnValue)
{
  SlotInfoSupportFunction (context, returnValue, "slot-range", CL_SlotRange);
}

void
CL_SlotCardinalityCommand (Environment * theEnv,
			   UDFContext * context, UDFValue * returnValue)
{
  SlotInfoSupportFunction (context, returnValue, "slot-cardinality",
			   CL_SlotCardinality);
}

/********************************************************************
  NAME         : CL_ClassAbstractP
  DESCRIPTION  : Dete_rmines if a class is abstract or not
  INPUTS       : Generic pointer to class
  RETURNS      : 1 if class is abstract, 0 otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ********************************************************************/
bool
CL_ClassAbstractP (Defclass * theDefclass)
{
  return theDefclass->abstract;
}

#if DEFRULE_CONSTRUCT

/********************************************************************
  NAME         : CL_ClassReactiveP
  DESCRIPTION  : Dete_rmines if a class is reactive or not
  INPUTS       : Generic pointer to class
  RETURNS      : 1 if class is reactive, 0 otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ********************************************************************/
bool
CL_ClassReactiveP (Defclass * theDefclass)
{
  return theDefclass->reactive;
}

#endif

/********************************************************************
  NAME         : CL_ClassSlots
  DESCRIPTION  : Groups slot info for a class into a multifield value
                   for dynamic perusal
  INPUTS       : 1) Generic pointer to class
                 2) Data object buffer to hold the slots of the class
                 3) Include (1) or exclude (0) inherited slots
  RETURNS      : Nothing useful
  SIDE EFFECTS : Creates a multifield storing the names of
                    the slots of the class
  NOTES        : None
 ********************************************************************/
void
CL_ClassSlots (Defclass * theDefclass, CLIPSValue * returnValue, bool inhp)
{
  size_t size;
  unsigned i;
  Environment *theEnv = theDefclass->header.env;

  size = inhp ? theDefclass->instanceSlotCount : theDefclass->slotCount;

  returnValue->value = CL_CreateMultifield (theEnv, size);

  if (size == 0)
    {
      return;
    }

  if (inhp)
    {
      for (i = 0; i < theDefclass->instanceSlotCount; i++)
	{
	  returnValue->multifieldValue->contents[i].value =
	    theDefclass->instanceTemplate[i]->slotName->name;
	}
    }
  else
    {
      for (i = 0; i < theDefclass->slotCount; i++)
	{
	  returnValue->multifieldValue->contents[i].value =
	    theDefclass->slots[i].slotName->name;
	}
    }
}

/************************************************************************
  NAME         : CL_GetDefmessageHandlerList
  DESCRIPTION  : Groups handler info for a class into a multifield value
                   for dynamic perusal
  INPUTS       : 1) Generic pointer to class (NULL to get handlers for
                    all classes)
                 2) Data object buffer to hold the handlers of the class
                 3) Include (1) or exclude (0) inherited handlers
  RETURNS      : Nothing useful
  SIDE EFFECTS : Creates a multifield storing the names and types of
                    the message-handlers of the class
  NOTES        : None
 ************************************************************************/
void
CL_GetDefmessageHandlerList (Environment * theEnv,
			     Defclass * theDefclass,
			     CLIPSValue * returnValue, bool inhp)
{
  Defclass *cls, *svcls, *svnxt, *supcls;
  long j;
  unsigned long classi, classiLimit;
  unsigned long i, sublen, len;

  if (theDefclass == NULL)
    {
      inhp = 0;
      cls = CL_GetNextDefclass (theEnv, NULL);
      svnxt = CL_GetNextDefclass (theEnv, cls);
    }
  else
    {
      cls = theDefclass;
      svnxt = CL_GetNextDefclass (theEnv, theDefclass);
      CL_SetNextDefclass (cls, NULL);
    }

  for (svcls = cls, i = 0;
       cls != NULL; cls = CL_GetNextDefclass (theEnv, cls))
    {
      classiLimit = inhp ? cls->allSuperclasses.classCount : 1;
      for (classi = 0; classi < classiLimit; classi++)
	{
	  i += cls->allSuperclasses.classArray[classi]->handlerCount;
	}
    }

  len = i * 3;

  returnValue->value = CL_CreateMultifield (theEnv, len);

  for (cls = svcls, sublen = 0;
       cls != NULL; cls = CL_GetNextDefclass (theEnv, cls))
    {
      classiLimit = inhp ? cls->allSuperclasses.classCount : 1;
      for (classi = 0; classi < classiLimit; classi++)
	{
	  supcls = cls->allSuperclasses.classArray[classi];

	  if (inhp == 0)
	    {
	      i = sublen;
	    }
	  else
	    {
	      i = len - (supcls->handlerCount * 3) - sublen;
	    }

	  for (j = 0; j < supcls->handlerCount; j++)
	    {
	      returnValue->multifieldValue->contents[i++].value =
		Get_DefclassNamePointer (supcls);
	      returnValue->multifieldValue->contents[i++].value =
		supcls->handlers[j].header.name;
	      returnValue->multifieldValue->contents[i++].value =
		CL_CreateSymbol (theEnv,
				 MessageHandlerData (theEnv)->
				 hndquals[supcls->handlers[j].type]);
	    }

	  sublen += supcls->handlerCount * 3;
	}
    }

  if (svcls != NULL)
    {
      CL_SetNextDefclass (svcls, svnxt);
    }
}

/***************************************************************************
  NAME         : CL_ClassSuperclasses
  DESCRIPTION  : Groups the names of superclasses into a multifield
                   value for dynamic perusal
  INPUTS       : 1) Generic pointer to class
                 2) Data object buffer to hold the superclasses of the class
                 3) Include (1) or exclude (0) indirect superclasses
  RETURNS      : Nothing useful
  SIDE EFFECTS : Creates a multifield storing the names of
                    the superclasses of the class
  NOTES        : None
 ***************************************************************************/
void
CL_ClassSuperclasses (Defclass * theDefclass,
		      CLIPSValue * returnValue, bool inhp)
{
  PACKED_CLASS_LINKS *plinks;
  unsigned offset;
  unsigned long i, j;
  Environment *theEnv = theDefclass->header.env;

  if (inhp)
    {
      plinks = &theDefclass->allSuperclasses;
      offset = 1;
    }
  else
    {
      plinks = &theDefclass->directSuperclasses;
      offset = 0;
    }

  returnValue->value =
    CL_CreateMultifield (theEnv, (plinks->classCount - offset));

  if (returnValue->multifieldValue->length == 0)
    {
      return;
    }

  for (i = offset, j = 0; i < plinks->classCount; i++, j++)
    {
      returnValue->multifieldValue->contents[j].value =
	Get_DefclassNamePointer (plinks->classArray[i]);
    }
}

/**************************************************************************
  NAME         : CL_ClassSubclasses
  DESCRIPTION  : Groups the names of subclasses for a class into a
                   multifield value for dynamic perusal
  INPUTS       : 1) Generic pointer to class
                 2) Data object buffer to hold the sublclasses of the class
                 3) Include (1) or exclude (0) indirect subclasses
  RETURNS      : Nothing useful
  SIDE EFFECTS : Creates a multifield storing the names
                    the subclasses of the class
  NOTES        : None
 **************************************************************************/
void
CL_ClassSubclasses (Defclass * theDefclass,
		    CLIPSValue * returnValue, bool inhp)
{
  unsigned i;
  int id;
  Environment *theEnv = theDefclass->header.env;

  if ((id = CL_GetTraversalID (theEnv)) == -1)
    {
      return;
    }

  i = CountSubclasses (theDefclass, inhp, id);

  CL_ReleaseTraversalID (theEnv);

  returnValue->value = CL_CreateMultifield (theEnv, i);

  if (i == 0)
    {
      return;
    }

  if ((id = CL_GetTraversalID (theEnv)) == -1)
    {
      return;
    }

  StoreSubclasses (returnValue->multifieldValue, 0, theDefclass, inhp, id,
		   true);
  CL_ReleaseTraversalID (theEnv);
}

/**************************************************************************
  NAME         : CL_ClassSubclassAddresses
  DESCRIPTION  : Groups the class addresses of subclasses for a class into a
                   multifield value for dynamic perusal
  INPUTS       : 1) Generic pointer to class
                 2) Data object buffer to hold the sublclasses of the class
                 3) Include (1) or exclude (0) indirect subclasses
  RETURNS      : Nothing useful
  SIDE EFFECTS : Creates a multifield storing the subclass
                    addresss of the class
  NOTES        : None
 **************************************************************************/
void
CL_ClassSubclassAddresses (Environment * theEnv,
			   Defclass * theDefclass,
			   UDFValue * returnValue, bool inhp)
{
  unsigned i;
  int id;

  if ((id = CL_GetTraversalID (theEnv)) == -1)
    {
      return;
    }

  i = CountSubclasses (theDefclass, inhp, id);

  CL_ReleaseTraversalID (theEnv);

  returnValue->begin = 0;
  returnValue->range = i;
  returnValue->value = CL_CreateMultifield (theEnv, i);

  if (i == 0)
    {
      return;
    }

  if ((id = CL_GetTraversalID (theEnv)) == -1)
    {
      return;
    }

  StoreSubclasses (returnValue->multifieldValue, 0, theDefclass, inhp, id,
		   false);
  CL_ReleaseTraversalID (theEnv);
}

/**************************************************************************
  NAME         : Slot...  Slot info_rmation access functions
  DESCRIPTION  : Groups the sources/facets/types/allowed-values/range or
                   cardinality  of a slot for a class into a multifield
                   value for dynamic perusal
  INPUTS       : 1) Generic pointer to class
                 2) Name of the slot
                 3) Data object buffer to hold the attributes of the class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Creates a multifield storing the attributes for the slot
                   of the class
  NOTES        : None
 **************************************************************************/

/**************/
/* CL_SlotFacets */
/**************/
bool
CL_SlotFacets (Defclass * theDefclass,
	       const char *sname, CLIPSValue * returnValue)
{
  SlotDescriptor *sp;
  UDFValue result;
  Environment *theEnv = theDefclass->header.env;

  if ((sp =
       SlotInfoSlot (theEnv, &result, theDefclass, sname,
		     "slot-facets")) == NULL)
    {
      CL_No_rmalizeMultifield (theEnv, &result);
      returnValue->value = result.value;
      return false;
    }

#if DEFRULE_CONSTRUCT
  returnValue->value = CL_CreateMultifield (theEnv, 10L);
#else
  returnValue->value = CL_CreateMultifield (theEnv, 9L);
#endif

  if (sp->multiple)
    {
      returnValue->multifieldValue->contents[0].lexemeValue =
	CL_CreateSymbol (theEnv, "MLT");
    }
  else
    {
      returnValue->multifieldValue->contents[0].lexemeValue =
	CL_CreateSymbol (theEnv, "SGL");
    }

  if (sp->noDefault)
    returnValue->multifieldValue->contents[1].lexemeValue =
      CL_CreateSymbol (theEnv, "NIL");
  else
    {
      if (sp->dynamicDefault)
	{
	  returnValue->multifieldValue->contents[1].lexemeValue =
	    CL_CreateSymbol (theEnv, "DYN");
	}
      else
	{
	  returnValue->multifieldValue->contents[1].lexemeValue =
	    CL_CreateSymbol (theEnv, "STC");
	}
    }

  if (sp->noInherit)
    returnValue->multifieldValue->contents[2].lexemeValue =
      CL_CreateSymbol (theEnv, "NIL");
  else
    returnValue->multifieldValue->contents[2].lexemeValue =
      CL_CreateSymbol (theEnv, "INH");

  if (sp->initializeOnly)
    returnValue->multifieldValue->contents[3].lexemeValue =
      CL_CreateSymbol (theEnv, "INT");
  else if (sp->no_Write)
    returnValue->multifieldValue->contents[3].lexemeValue =
      CL_CreateSymbol (theEnv, "R");
  else
    returnValue->multifieldValue->contents[3].lexemeValue =
      CL_CreateSymbol (theEnv, "RW");

  if (sp->shared)
    returnValue->multifieldValue->contents[4].lexemeValue =
      CL_CreateSymbol (theEnv, "SHR");
  else
    returnValue->multifieldValue->contents[4].lexemeValue =
      CL_CreateSymbol (theEnv, "LCL");

#if DEFRULE_CONSTRUCT
  if (sp->reactive)
    returnValue->multifieldValue->contents[5].lexemeValue =
      CL_CreateSymbol (theEnv, "RCT");
  else
    returnValue->multifieldValue->contents[5].lexemeValue =
      CL_CreateSymbol (theEnv, "NIL");

  if (sp->composite)
    returnValue->multifieldValue->contents[6].lexemeValue =
      CL_CreateSymbol (theEnv, "CMP");
  else
    returnValue->multifieldValue->contents[6].lexemeValue =
      CL_CreateSymbol (theEnv, "EXC");

  if (sp->publicVisibility)
    returnValue->multifieldValue->contents[7].lexemeValue =
      CL_CreateSymbol (theEnv, "PUB");
  else
    returnValue->multifieldValue->contents[7].lexemeValue =
      CL_CreateSymbol (theEnv, "PRV");

  returnValue->multifieldValue->contents[8].lexemeValue =
    CL_CreateSymbol (theEnv, CL_GetCreateAccessorString (sp));
  returnValue->multifieldValue->contents[9].lexemeValue =
    (sp->no_Write ? CL_CreateSymbol (theEnv, "NIL") : sp->overrideMessage);
#else
  if (sp->composite)
    returnValue->multifieldValue->contents[5].lexemeValue =
      CL_CreateSymbol (theEnv, "CMP");
  else
    returnValue->multifieldValue->contents[5].lexemeValue =
      CL_CreateSymbol (theEnv, "EXC");

  if (sp->publicVisibility)
    returnValue->multifieldValue->contents[6].lexemeValue =
      CL_CreateSymbol (theEnv, "PUB");
  else
    returnValue->multifieldValue->contents[6].lexemeValue =
      CL_CreateSymbol (theEnv, "PRV");

  returnValue->multifieldValue->contents[7].lexemeValue =
    CL_CreateSymbol (theEnv, CL_GetCreateAccessorString (sp));
  returnValue->multifieldValue->contents[8].lexemeValue =
    (sp->no_Write ? CL_CreateSymbol (theEnv, "NIL") : sp->overrideMessage);
#endif

  return true;
}

/***************/
/* CL_SlotSources */
/***************/
bool
CL_SlotSources (Defclass * theDefclass,
		const char *sname, CLIPSValue * returnValue)
{
  unsigned i;
  unsigned classi;
  SlotDescriptor *sp, *csp;
  CLASS_LINK *ctop, *ctmp;
  Defclass *cls;
  UDFValue result;
  Environment *theEnv = theDefclass->header.env;

  if ((sp =
       SlotInfoSlot (theEnv, &result, theDefclass, sname,
		     "slot-sources")) == NULL)
    {
      CL_No_rmalizeMultifield (theEnv, &result);
      returnValue->value = result.value;
      return false;
    }
  i = 1;
  ctop = get_struct (theEnv, classLink);
  ctop->cls = sp->cls;
  ctop->nxt = NULL;
  if (sp->composite)
    {
      for (classi = 1; classi < sp->cls->allSuperclasses.classCount; classi++)
	{
	  cls = sp->cls->allSuperclasses.classArray[classi];
	  csp = CL_FindClassSlot (cls, sp->slotName->name);
	  if ((csp != NULL) ? (csp->noInherit == 0) : false)
	    {
	      ctmp = get_struct (theEnv, classLink);
	      ctmp->cls = cls;
	      ctmp->nxt = ctop;
	      ctop = ctmp;
	      i++;
	      if (csp->composite == 0)
		break;
	    }
	}
    }

  returnValue->value = CL_CreateMultifield (theEnv, i);
  for (ctmp = ctop, i = 0; ctmp != NULL; ctmp = ctmp->nxt, i++)
    {
      returnValue->multifieldValue->contents[i].value =
	Get_DefclassNamePointer (ctmp->cls);
    }
  CL_DeleteClassLinks (theEnv, ctop);

  return true;
}

/*************/
/* CL_SlotTypes */
/*************/
bool
CL_SlotTypes (Defclass * theDefclass,
	      const char *sname, CLIPSValue * returnValue)
{
  unsigned i, j;
  SlotDescriptor *sp;
  char typemap[2];
  unsigned msize;
  UDFValue result;
  Environment *theEnv = theDefclass->header.env;

  if ((sp =
       SlotInfoSlot (theEnv, &result, theDefclass, sname,
		     "slot-types")) == NULL)
    {
      CL_No_rmalizeMultifield (theEnv, &result);
      returnValue->value = result.value;
      return false;
    }

  if ((sp->constraint != NULL) ? sp->constraint->anyAllowed : true)
    {
      typemap[0] = typemap[1] = (char) 0xFF;
      CL_ClearBitMap (typemap, MULTIFIELD_TYPE);
      msize = 8;
    }
  else
    {
      typemap[0] = typemap[1] = (char) 0x00;
      msize = 0;
      if (sp->constraint->symbolsAllowed)
	{
	  msize++;
	  SetBitMap (typemap, SYMBOL_TYPE);
	}
      if (sp->constraint->stringsAllowed)
	{
	  msize++;
	  SetBitMap (typemap, STRING_TYPE);
	}
      if (sp->constraint->floatsAllowed)
	{
	  msize++;
	  SetBitMap (typemap, FLOAT_TYPE);
	}
      if (sp->constraint->integersAllowed)
	{
	  msize++;
	  SetBitMap (typemap, INTEGER_TYPE);
	}
      if (sp->constraint->instanceNamesAllowed)
	{
	  msize++;
	  SetBitMap (typemap, INSTANCE_NAME_TYPE);
	}
      if (sp->constraint->instanceAddressesAllowed)
	{
	  msize++;
	  SetBitMap (typemap, INSTANCE_ADDRESS_TYPE);
	}
      if (sp->constraint->externalAddressesAllowed)
	{
	  msize++;
	  SetBitMap (typemap, EXTERNAL_ADDRESS_TYPE);
	}
      if (sp->constraint->factAddressesAllowed)
	{
	  msize++;
	  SetBitMap (typemap, FACT_ADDRESS_TYPE);
	}
    }

  returnValue->value = CL_CreateMultifield (theEnv, msize);
  i = 0;
  j = 0;
  while (i < msize)
    {
      if (TestBitMap (typemap, j))
	{
	  returnValue->multifieldValue->contents[i].value =
	    Get_DefclassNamePointer (DefclassData (theEnv)->
				     PrimitiveClassMap[j]);
	  i++;
	}
      j++;
    }

  return true;
}

/*********************/
/* CL_SlotAllowedValues */
/*********************/
bool
CL_SlotAllowedValues (Defclass * theDefclass,
		      const char *sname, CLIPSValue * returnValue)
{
  int i;
  SlotDescriptor *sp;
  Expression *theExp;
  UDFValue result;
  Environment *theEnv = theDefclass->header.env;

  if ((sp =
       SlotInfoSlot (theEnv, &result, theDefclass, sname,
		     "slot-allowed-values")) == NULL)
    {
      CL_No_rmalizeMultifield (theEnv, &result);
      returnValue->value = result.value;
      return false;
    }

  if ((sp->constraint != NULL) ? (sp->constraint->restrictionList ==
				  NULL) : true)
    {
      returnValue->value = FalseSymbol (theEnv);
      return true;
    }

  returnValue->value =
    CL_CreateMultifield (theEnv,
			 CL_ExpressionSize (sp->constraint->restrictionList));
  i = 0;
  theExp = sp->constraint->restrictionList;
  while (theExp != NULL)
    {
      returnValue->multifieldValue->contents[i].value = theExp->value;
      theExp = theExp->nextArg;
      i++;
    }

  return true;
}

/**********************/
/* CL_SlotAllowedClasses */
/**********************/
bool
CL_SlotAllowedClasses (Defclass * theDefclass,
		       const char *sname, CLIPSValue * returnValue)
{
  int i;
  SlotDescriptor *sp;
  Expression *theExp;
  UDFValue result;
  Environment *theEnv = theDefclass->header.env;

  if ((sp =
       SlotInfoSlot (theEnv, &result, theDefclass, sname,
		     "slot-allowed-classes")) == NULL)
    {
      CL_No_rmalizeMultifield (theEnv, &result);
      returnValue->value = result.value;
      return false;
    }
  if ((sp->constraint != NULL) ? (sp->constraint->classList == NULL) : true)
    {
      returnValue->value = FalseSymbol (theEnv);
      return true;
    }
  returnValue->value =
    CL_CreateMultifield (theEnv,
			 CL_ExpressionSize (sp->constraint->classList));
  i = 0;
  theExp = sp->constraint->classList;
  while (theExp != NULL)
    {
      returnValue->multifieldValue->contents[i].value = theExp->value;
      theExp = theExp->nextArg;
      i++;
    }

  return true;
}

/*************/
/* CL_SlotRange */
/*************/
bool
CL_SlotRange (Defclass * theDefclass,
	      const char *sname, CLIPSValue * returnValue)
{
  SlotDescriptor *sp;
  UDFValue result;
  Environment *theEnv = theDefclass->header.env;

  if ((sp =
       SlotInfoSlot (theEnv, &result, theDefclass, sname,
		     "slot-range")) == NULL)
    {
      CL_No_rmalizeMultifield (theEnv, &result);
      returnValue->value = result.value;
      return false;
    }
  if ((sp->constraint == NULL) ? false :
      (sp->constraint->anyAllowed || sp->constraint->floatsAllowed ||
       sp->constraint->integersAllowed))
    {
      returnValue->value = CL_CreateMultifield (theEnv, 2L);
      returnValue->multifieldValue->contents[0].value =
	sp->constraint->minValue->value;
      returnValue->multifieldValue->contents[1].value =
	sp->constraint->maxValue->value;
    }
  else
    {
      returnValue->value = FalseSymbol (theEnv);
    }
  return true;
}

/*******************/
/* CL_SlotCardinality */
/*******************/
bool
CL_SlotCardinality (Defclass * theDefclass,
		    const char *sname, CLIPSValue * returnValue)
{
  SlotDescriptor *sp;
  UDFValue result;
  Environment *theEnv = theDefclass->header.env;

  if ((sp =
       SlotInfoSlot (theEnv, &result, theDefclass, sname,
		     "slot-cardinality")) == NULL)
    {
      CL_No_rmalizeMultifield (theEnv, &result);
      returnValue->value = result.value;
      return false;
    }

  if (sp->multiple == 0)
    {
      returnValue->multifieldValue = CL_CreateMultifield (theEnv, 0L);
      return true;
    }

  returnValue->value = CL_CreateMultifield (theEnv, 2L);
  if (sp->constraint != NULL)
    {
      returnValue->multifieldValue->contents[0].value =
	sp->constraint->minFields->value;
      returnValue->multifieldValue->contents[1].value =
	sp->constraint->maxFields->value;
    }
  else
    {
      returnValue->multifieldValue->contents[0].value =
	SymbolData (theEnv)->Zero;
      returnValue->multifieldValue->contents[1].value =
	SymbolData (theEnv)->PositiveInfinity;
    }

  return true;
}

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/*****************************************************
  NAME         : SlotInfoSupportFunction
  DESCRIPTION  : Support routine for slot-sources,
                   slot-facets, et. al.
  INPUTS       : 1) Data object buffer
                 2) Name of the H/L caller
                 3) Pointer to support function to call
  RETURNS      : Nothing useful
  SIDE EFFECTS : Support function called and data
                  object buffer set
  NOTES        : None
 *****************************************************/
static void
SlotInfoSupportFunction (UDFContext * context,
			 UDFValue * returnValue,
			 const char *fnxname,
			 bool (*fnx) (Defclass *, const char *, CLIPSValue *))
{
  CLIPSLexeme *ssym;
  Defclass *cls;
  CLIPSValue result;

  ssym = CL_CheckClassAndSlot (context, fnxname, &cls);
  if (ssym == NULL)
    {
      CL_SetMultifieldErrorValue (context->environment, returnValue);
      return;
    }
  (*fnx) (cls, ssym->contents, &result);
  CL_CLIPSToUDFValue (&result, returnValue);
}

/*****************************************************************
  NAME         : CountSubclasses
  DESCRIPTION  : Counts the number of direct or indirect
                   subclasses for a class
  INPUTS       : 1) Address of class
                 2) Include (1) or exclude (0) indirect subclasses
                 3) Traversal id
  RETURNS      : The number of subclasses
  SIDE EFFECTS : None
  NOTES        : None
 *****************************************************************/
static unsigned
CountSubclasses (Defclass * cls, bool inhp, int tvid)
{
  unsigned i, cnt;
  Defclass *subcls;

  for (cnt = 0, i = 0; i < cls->directSubclasses.classCount; i++)
    {
      subcls = cls->directSubclasses.classArray[i];
      if (TestTraversalID (subcls->traversalRecord, tvid) == 0)
	{
	  cnt++;
	  SetTraversalID (subcls->traversalRecord, tvid);
	  if (inhp && (subcls->directSubclasses.classCount != 0))
	    cnt += CountSubclasses (subcls, inhp, tvid);
	}
    }
  return cnt;
}

/*********************************************************************
  NAME         : StoreSubclasses
  DESCRIPTION  : Stores the names of direct or indirect
                   subclasses for a class in a mutlifield
  INPUTS       : 1) Caller's multifield buffer
                 2) Starting index
                 3) Address of the class
                 4) Include (1) or exclude (0) indirect subclasses
                 5) Traversal id
  RETURNS      : The number of subclass names stored in the multifield
  SIDE EFFECTS : Multifield set with subclass names
  NOTES        : Assumes multifield is big enough to hold subclasses
 *********************************************************************/
static unsigned
StoreSubclasses (Multifield * mfval,
		 unsigned si,
		 Defclass * cls, int inhp, int tvid, bool storeName)
{
  unsigned i, classi;
  Defclass *subcls;

  for (i = si, classi = 0; classi < cls->directSubclasses.classCount;
       classi++)
    {
      subcls = cls->directSubclasses.classArray[classi];
      if (TestTraversalID (subcls->traversalRecord, tvid) == 0)
	{
	  SetTraversalID (subcls->traversalRecord, tvid);
	  if (storeName)
	    {
	      mfval->contents[i++].value = Get_DefclassNamePointer (subcls);
	    }
	  else
	    {
	      mfval->contents[i++].value = subcls;
	    }

	  if (inhp && (subcls->directSubclasses.classCount != 0))
	    i += StoreSubclasses (mfval, i, subcls, inhp, tvid, storeName);
	}
    }
  return i - si;
}

/*********************************************************
  NAME         : SlotInfoSlot
  DESCRIPTION  : CL_Runtime support routine for slot-sources,
                   slot-facets, et. al. which looks up
                   a slot
  INPUTS       : 1) Data object buffer
                 2) Class pointer
                 3) Name-string of slot to find
                 4) The name of the calling function
  RETURNS      : Nothing useful
  SIDE EFFECTS : Support function called and data object
                  buffer initialized
  NOTES        : None
 *********************************************************/
static SlotDescriptor *
SlotInfoSlot (Environment * theEnv,
	      UDFValue * returnValue,
	      Defclass * cls, const char *sname, const char *fnxname)
{
  CLIPSLexeme *ssym;
  int i;

  if ((ssym = CL_FindSymbolHN (theEnv, sname, SYMBOL_BIT)) == NULL)
    {
      Set_EvaluationError (theEnv, true);
      CL_SetMultifieldErrorValue (theEnv, returnValue);
      return NULL;
    }

  i = CL_FindInstanceTemplateSlot (theEnv, cls, ssym);
  if (i == -1)
    {
      CL_SlotExistError (theEnv, sname, fnxname);
      Set_EvaluationError (theEnv, true);
      CL_SetMultifieldErrorValue (theEnv, returnValue);
      return NULL;
    }

  returnValue->begin = 0;

  return cls->instanceTemplate[i];
}

#endif
