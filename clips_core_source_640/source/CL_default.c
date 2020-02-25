   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*               DEFAULT ATTRIBUTE MODULE              */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides functions for parsing the default       */
/*   attribute and dete_rmining default values based on       */
/*   slot constraints.                                       */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian Dantes                                         */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Support for deftemplate-slot-default-value     */
/*            function.                                      */
/*                                                           */
/*      6.30: Support for long long integers.                */
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

#include "setup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constant.h"
#include "constrnt.h"
#include "cstrnchk.h"
#include "cstrnutl.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "factmngr.h"
#include "inscom.h"
#include "multifld.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"

#include "default.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void *FindDefaultValue (Environment *, int, CONSTRAINT_RECORD *,
			       void *);

/********************************************************/
/* CL_DeriveDefaultFromConstraints: Returns an appropriate */
/*   default value for the supplied constraints.        */
/********************************************************/
void
CL_DeriveDefaultFromConstraints (Environment * theEnv,
				 CONSTRAINT_RECORD * constraints,
				 UDFValue * theDefault,
				 bool multifield, bool garbageMultifield)
{
  unsigned long minFields;
  void *theValue;

   /*=============================================================*/
  /* If no constraints are specified, then use the symbol nil as */
  /* a default for single field slots and a multifield of length */
  /* 0 as a default for multifield slots.                        */
   /*=============================================================*/

  if (constraints == NULL)
    {
      if (multifield)
	{
	  theDefault->begin = 0;
	  theDefault->range = 0;
	  if (garbageMultifield)
	    theDefault->value = CL_CreateMultifield (theEnv, 0L);
	  else
	    theDefault->value = CL_CreateUnmanagedMultifield (theEnv, 0L);
	}
      else
	{
	  theDefault->value = CL_CreateSymbol (theEnv, "nil");
	}

      return;
    }

   /*=========================================*/
  /* Dete_rmine the default's type and value. */
   /*=========================================*/

  if (constraints->anyAllowed || constraints->symbolsAllowed)
    {
      theValue =
	FindDefaultValue (theEnv, SYMBOL_TYPE, constraints,
			  CL_CreateSymbol (theEnv, "nil"));
    }

  else if (constraints->stringsAllowed)
    {
      theValue =
	FindDefaultValue (theEnv, STRING_TYPE, constraints,
			  CL_CreateString (theEnv, ""));
    }

  else if (constraints->integersAllowed)
    {
      theValue =
	FindDefaultValue (theEnv, INTEGER_TYPE, constraints,
			  CL_CreateInteger (theEnv, 0LL));
    }

  else if (constraints->floatsAllowed)
    {
      theValue =
	FindDefaultValue (theEnv, FLOAT_TYPE, constraints,
			  CL_CreateFloat (theEnv, 0.0));
    }
#if OBJECT_SYSTEM
  else if (constraints->instanceNamesAllowed)
    {
      theValue =
	FindDefaultValue (theEnv, INSTANCE_NAME_TYPE, constraints,
			  CL_Create_InstanceName (theEnv, "nil"));
    }

  else if (constraints->instanceAddressesAllowed)
    {
      theValue = &InstanceData (theEnv)->DummyInstance;
    }
#endif
#if DEFTEMPLATE_CONSTRUCT
  else if (constraints->factAddressesAllowed)
    {
      theValue = &FactData (theEnv)->DummyFact;
    }
#endif
  else if (constraints->externalAddressesAllowed)
    {
      theValue = CL_CreateExternalAddress (theEnv, NULL, 0);
    }

  else
    {
      theValue = CL_CreateSymbol (theEnv, "nil");
    }

   /*=========================================================*/
  /* If the default is for a multifield slot, then create a  */
  /* multifield default value that satisfies the cardinality */
  /* constraints for the slot. The default value for a       */
  /* multifield slot is a multifield of length 0.            */
   /*=========================================================*/

  if (multifield)
    {
      if (constraints->minFields == NULL)
	minFields = 0;
      else if (constraints->minFields->value ==
	       SymbolData (theEnv)->NegativeInfinity)
	minFields = 0;
      else
	minFields =
	  (unsigned long) constraints->minFields->integerValue->contents;

      theDefault->begin = 0;
      theDefault->range = minFields;
      if (garbageMultifield)
	theDefault->value = CL_CreateMultifield (theEnv, minFields);
      else
	theDefault->value = CL_CreateUnmanagedMultifield (theEnv, minFields);

      for (; minFields > 0; minFields--)
	{
	  theDefault->multifieldValue->contents[minFields - 1].value =
	    theValue;
	}
    }
  else
    {
      theDefault->value = theValue;
    }
}

/***********************************************************************/
/* FindDefaultValue: Searches the list of restriction values for a     */
/*   constraint to find a default value of the specified type. For     */
/*   example, if the attribute (allowed-symbols on off) was specified, */
/*   then the symbol "on" would be used as a default value rather than */
/*   the symbol "nil". For integers and floats, the range attribute is */
/*   also used to select a suitable default value. If a minimum value  */
/*   was specified, then this value is used first followed by the      */
/*   maximum value.                                                    */
/************************************************************************/
static void *
FindDefaultValue (Environment * theEnv,
		  int theType,
		  CONSTRAINT_RECORD * theConstraints, void *standardDefault)
{
  struct expr *theList;

   /*=====================================================*/
  /* Look on the the allowed values list to see if there */
  /* is a value of the requested type. Return the first  */
  /* value found of the requested type.                  */
   /*=====================================================*/

  theList = theConstraints->restrictionList;
  while (theList != NULL)
    {
      if (theList->type == theType)
	return (theList->value);
      theList = theList->nextArg;
    }

   /*=============================================================*/
  /* If no specific values were available for the default value, */
  /* and the type requested is a float or integer, then use the  */
  /* range attribute to select a default value.                  */
   /*=============================================================*/

  if (theType == INTEGER_TYPE)
    {
      if (theConstraints->minValue->type == INTEGER_TYPE)
	{
	  return (theConstraints->minValue->value);
	}
      else if (theConstraints->minValue->type == FLOAT_TYPE)
	{
	  return (CL_CreateInteger
		  (theEnv,
		   (long long) theConstraints->minValue->floatValue->
		   contents));
	}
      else if (theConstraints->maxValue->type == INTEGER_TYPE)
	{
	  return (theConstraints->maxValue->value);
	}
      else if (theConstraints->maxValue->type == FLOAT_TYPE)
	{
	  return (CL_CreateInteger
		  (theEnv,
		   (long long) theConstraints->maxValue->floatValue->
		   contents));
	}
    }
  else if (theType == FLOAT_TYPE)
    {
      if (theConstraints->minValue->type == FLOAT_TYPE)
	{
	  return (theConstraints->minValue->value);
	}
      else if (theConstraints->minValue->type == INTEGER_TYPE)
	{
	  return (CL_CreateFloat
		  (theEnv,
		   (double) theConstraints->minValue->integerValue->
		   contents));
	}
      else if (theConstraints->maxValue->type == FLOAT_TYPE)
	{
	  return (theConstraints->maxValue->value);
	}
      else if (theConstraints->maxValue->type == INTEGER_TYPE)
	{
	  return (CL_CreateFloat
		  (theEnv,
		   (double) theConstraints->maxValue->integerValue->
		   contents));
	}
    }

   /*======================================*/
  /* Use the standard default value (such */
  /* as nil if symbols are allowed).      */
   /*======================================*/

  return (standardDefault);
}

#if (! RUN_TIME) && (! BLOAD_ONLY)

/**********************************************/
/* CL_ParseDefault: Parses a default value list. */
/**********************************************/
struct expr *
CL_ParseDefault (Environment * theEnv,
		 const char *readSource,
		 bool multifield,
		 bool dynamic,
		 bool evalStatic,
		 bool *noneSpecified, bool *deriveSpecified, bool *error)
{
  struct expr *defaultList = NULL, *lastDefault = NULL;
  struct expr *newItem, *tmpItem;
  struct token theToken;
  UDFValue theValue;
  CONSTRAINT_RECORD *rv;
  int specialVarCode;

  *noneSpecified = false;
  *deriveSpecified = false;

  CL_SavePPBuffer (theEnv, " ");
  CL_GetToken (theEnv, readSource, &theToken);

   /*===================================================*/
  /* Read the items contained in the default attribute */
  /* until a closing right parenthesis is encountered. */
   /*===================================================*/

  while (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      /*========================================*/
      /* Get the next item in the default list. */
      /*========================================*/

      newItem = CL_ParseAtomOrExpression (theEnv, readSource, &theToken);
      if (newItem == NULL)
	{
	  CL_ReturnExpression (theEnv, defaultList);
	  *error = true;
	  return NULL;
	}

      /*===========================================================*/
      /* Check for invalid variable usage. With the expection of   */
      /* ?NONE for the default attribute, local variables may not  */
      /* be used within the default or default-dynamic attributes. */
      /*===========================================================*/

      if ((newItem->type == SF_VARIABLE) || (newItem->type == MF_VARIABLE))
	{
	  if (strcmp (newItem->lexemeValue->contents, "NONE") == 0)
	    {
	      specialVarCode = 0;
	    }
	  else if (strcmp (newItem->lexemeValue->contents, "DERIVE") == 0)
	    {
	      specialVarCode = 1;
	    }
	  else
	    {
	      specialVarCode = -1;
	    }

	  if ((dynamic) ||
	      (newItem->type == MF_VARIABLE) ||
	      (specialVarCode == -1) ||
	      ((specialVarCode != -1) && (defaultList != NULL)))
	    {
	      if (dynamic)
		CL_SyntaxErrorMessage (theEnv, "default-dynamic attribute");
	      else
		CL_SyntaxErrorMessage (theEnv, "default attribute");
	      CL_ReturnExpression (theEnv, newItem);
	      CL_ReturnExpression (theEnv, defaultList);
	      *error = true;
	      return NULL;
	    }

	  CL_ReturnExpression (theEnv, newItem);

	 /*============================================*/
	  /* Check for the closing right parenthesis of */
	  /* the default or default dynamic attribute.  */
	 /*============================================*/

	  CL_GetToken (theEnv, readSource, &theToken);

	  if (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
	    {
	      if (dynamic)
		CL_SyntaxErrorMessage (theEnv, "default-dynamic attribute");
	      else
		CL_SyntaxErrorMessage (theEnv, "default attribute");
	      CL_PPBackup (theEnv);
	      CL_SavePPBuffer (theEnv, " ");
	      CL_SavePPBuffer (theEnv, theToken.printFo_rm);
	      *error = true;
	    }

	  if (specialVarCode == 0)
	    *noneSpecified = true;
	  else
	    *deriveSpecified = true;
	  return NULL;
	}

      /*====================================================*/
      /* Look to see if any variables have been used within */
      /* expressions contained within the default list.     */
      /*====================================================*/

      if (CL_ExpressionContainsVariables (newItem, false) == true)
	{
	  CL_ReturnExpression (theEnv, defaultList);
	  CL_ReturnExpression (theEnv, newItem);
	  *error = true;
	  if (dynamic)
	    CL_SyntaxErrorMessage (theEnv, "default-dynamic attribute");
	  else
	    CL_SyntaxErrorMessage (theEnv, "default attribute");
	  return NULL;
	}

      /*============================================*/
      /* Add the default value to the default list. */
      /*============================================*/

      if (lastDefault == NULL)
	{
	  defaultList = newItem;
	}
      else
	{
	  lastDefault->nextArg = newItem;
	}
      lastDefault = newItem;

      /*=======================================*/
      /* Begin parsing the next default value. */
      /*=======================================*/

      CL_SavePPBuffer (theEnv, " ");
      CL_GetToken (theEnv, readSource, &theToken);
    }

   /*=====================================*/
  /* Fix up pretty print representation. */
   /*=====================================*/

  CL_PPBackup (theEnv);
  CL_PPBackup (theEnv);
  CL_SavePPBuffer (theEnv, ")");

   /*=========================================*/
  /* A single field slot's default attribute */
  /* must contain a single value.            */
   /*=========================================*/

  if (multifield == false)
    {
      if (defaultList == NULL)
	{
	  *error = true;
	}
      else if (defaultList->nextArg != NULL)
	{
	  *error = true;
	}
      else
	{
	  rv = CL_ExpressionToConstraintRecord (theEnv, defaultList);
	  rv->multifieldsAllowed = false;
	  if (CL_UnmatchableConstraint (rv))
	    *error = true;
	  CL_RemoveConstraint (theEnv, rv);
	}

      if (*error)
	{
	  CL_PrintErrorID (theEnv, "DEFAULT", 1, true);
	  CL_WriteString (theEnv, STDERR,
			  "The default value for a single field slot must be a single field value.\n");
	  CL_ReturnExpression (theEnv, defaultList);
	  return NULL;
	}
    }

   /*=======================================================*/
  /* If the dynamic-default attribute is not being parsed, */
  /* evaluate the expressions to make the default value.   */
   /*=======================================================*/

  if (dynamic || (!evalStatic) || (defaultList == NULL))
    return (defaultList);

  tmpItem = defaultList;
  newItem = defaultList;

  defaultList = NULL;

  while (newItem != NULL)
    {
      Set_EvaluationError (theEnv, false);
      if (CL_EvaluateExpression (theEnv, newItem, &theValue))
	*error = true;

      if ((theValue.header->type == MULTIFIELD_TYPE) &&
	  (multifield == false) && (*error == false))
	{
	  CL_PrintErrorID (theEnv, "DEFAULT", 1, true);
	  CL_WriteString (theEnv, STDERR,
			  "The default value for a single field slot must be a single field value.\n");
	  *error = true;
	}

      if (*error)
	{
	  CL_ReturnExpression (theEnv, tmpItem);
	  CL_ReturnExpression (theEnv, defaultList);
	  *error = true;
	  return NULL;
	}

      lastDefault = CL_ConvertValueToExpression (theEnv, &theValue);

      defaultList = CL_AppendExpressions (defaultList, lastDefault);

      newItem = newItem->nextArg;
    }

  CL_ReturnExpression (theEnv, tmpItem);

   /*==========================*/
  /* Return the default list. */
   /*==========================*/

  return (defaultList);
}

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */
