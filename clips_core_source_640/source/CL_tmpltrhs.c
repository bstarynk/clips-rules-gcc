   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*          DEFTEMPLATE RHS PARSING HEADER FILE        */
   /*******************************************************/

/*************************************************************/
/* Purpose: Parses deftemplate fact patterns used with the   */
/*   assert function.                                        */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added additional argument required for         */
/*            CL_DeriveDefaultFromConstraints.                  */
/*                                                           */
/*            Added additional argument required for         */
/*            CL_InvalidDeftemplateSlotMessage.                 */
/*                                                           */
/*      6.30: Added const qualifiers to remove C++           */
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
/*************************************************************/

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT

#include <stdio.h>

#include "default.h"
#include "extnfunc.h"
#include "factrhs.h"
#include "memalloc.h"
#include "modulutl.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "tmpltdef.h"
#include "tmpltfun.h"
#include "tmpltlhs.h"
#include "tmpltutl.h"

#include "tmpltrhs.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static struct expr *Parse_AssertSlotValues (Environment *, const char *,
					    struct token *,
					    struct templateSlot *, bool *,
					    bool);
static struct expr *Reorder_AssertSlotValues (Environment *,
					      struct templateSlot *,
					      struct expr *, bool *);
static struct expr *GetSlot_AssertValues (Environment *,
					  struct templateSlot *,
					  struct expr *, bool *);
static struct expr *Find_AssertSlotItem (struct templateSlot *,
					 struct expr *);
static struct templateSlot *CL_ParseSlotLabel (Environment *, const char *,
					       struct token *, Deftemplate *,
					       bool *, TokenType);

/******************************************************************/
/* Parse_AssertTemplate: Parses and builds the list of values that */
/*   are used for an assert of a fact with a deftemplate.         */
/******************************************************************/
struct expr *
Parse_AssertTemplate (Environment * theEnv,
		      const char *readSource,
		      struct token *theToken,
		      bool *error,
		      TokenType endType,
		      bool constantsOnly, Deftemplate * theDeftemplate)
{
  struct expr *firstSlot, *lastSlot, *nextSlot;
  struct expr *firstArg, *tempSlot;
  struct templateSlot *slotPtr;

  firstSlot = NULL;
  lastSlot = NULL;

   /*==============================================*/
  /* Parse each of the slot fields in the assert. */
   /*==============================================*/

  while ((slotPtr =
	  CL_ParseSlotLabel (theEnv, readSource, theToken, theDeftemplate,
			     error, endType)) != NULL)
    {
      /*========================================================*/
      /* Check to see that the slot hasn't already been parsed. */
      /*========================================================*/

      for (tempSlot = firstSlot;
	   tempSlot != NULL; tempSlot = tempSlot->nextArg)
	{
	  if (tempSlot->value == (void *) slotPtr->slotName)
	    {
	      CL_AlreadyParsedErrorMessage (theEnv, "slot ",
					    slotPtr->slotName->contents);
	      *error = true;
	      CL_ReturnExpression (theEnv, firstSlot);
	      return NULL;
	    }
	}

      /*============================================*/
      /* Parse the values to be stored in the slot. */
      /*============================================*/

      nextSlot = Parse_AssertSlotValues (theEnv, readSource, theToken,
					 slotPtr, error, constantsOnly);

      if (*error)
	{
	  CL_ReturnExpression (theEnv, firstSlot);
	  return NULL;
	}

      /*============================================*/
      /* Check to see if the values to be stored in */
      /* the slot violate the slot's constraints.   */
      /*============================================*/

      if (CL_CheckRHS_SlotTypes (theEnv, nextSlot->argList, slotPtr, "assert")
	  == 0)
	{
	  *error = true;
	  CL_ReturnExpression (theEnv, firstSlot);
	  CL_ReturnExpression (theEnv, nextSlot);
	  return NULL;
	}

      /*===================================================*/
      /* Add the slot to the list of slots already parsed. */
      /*===================================================*/

      if (lastSlot == NULL)
	{
	  firstSlot = nextSlot;
	}
      else
	{
	  lastSlot->nextArg = nextSlot;
	}

      lastSlot = nextSlot;
    }

   /*=================================================*/
  /* Return if an error occured parsing a slot name. */
   /*=================================================*/

  if (*error)
    {
      CL_ReturnExpression (theEnv, firstSlot);
      return NULL;
    }

   /*=============================================================*/
  /* Reorder the arguments to the order used by the deftemplate. */
   /*=============================================================*/

  firstArg =
    Reorder_AssertSlotValues (theEnv, theDeftemplate->slotList, firstSlot,
			      error);
  CL_ReturnExpression (theEnv, firstSlot);

   /*==============================*/
  /* Return the assert arguments. */
   /*==============================*/

  return (firstArg);
}

/****************************************************************/
/* CL_ParseSlotLabel: Parses the beginning of a slot definition.   */
/*   Checks for opening left parenthesis and a valid slot name. */
/****************************************************************/
static struct templateSlot *
CL_ParseSlotLabel (Environment * theEnv,
		   const char *inputSource,
		   struct token *tempToken,
		   Deftemplate * theDeftemplate,
		   bool *error, TokenType endType)
{
  struct templateSlot *slotPtr;

   /*========================*/
  /* Initialize error flag. */
   /*========================*/

  *error = false;

   /*============================================*/
  /* If token is a right parenthesis, then fact */
  /* template definition is complete.           */
   /*============================================*/

  CL_GetToken (theEnv, inputSource, tempToken);
  if (tempToken->tknType == endType)
    {
      return NULL;
    }

   /*=======================================*/
  /* Put a space between the template name */
  /* and the first slot definition.        */
   /*=======================================*/

  CL_PPBackup (theEnv);
  CL_SavePPBuffer (theEnv, " ");
  CL_SavePPBuffer (theEnv, tempToken->printFo_rm);

   /*=======================================================*/
  /* Slot definition begins with opening left parenthesis. */
   /*=======================================================*/

  if (tempToken->tknType != LEFT_PARENTHESIS_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "deftemplate pattern");
      *error = true;
      return NULL;
    }

   /*=============================*/
  /* Slot name must be a symbol. */
   /*=============================*/

  CL_GetToken (theEnv, inputSource, tempToken);
  if (tempToken->tknType != SYMBOL_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "deftemplate pattern");
      *error = true;
      return NULL;
    }

   /*======================================================*/
  /* Check that the slot name is valid for this template. */
   /*======================================================*/

  if ((slotPtr =
       CL_FindSlot (theDeftemplate, tempToken->lexemeValue, NULL)) == NULL)
    {
      CL_InvalidDeftemplateSlotMessage (theEnv,
					tempToken->lexemeValue->contents,
					theDeftemplate->header.name->contents,
					true);
      *error = true;
      return NULL;
    }

   /*====================================*/
  /* Return a pointer to the slot name. */
   /*====================================*/

  return slotPtr;
}

/**************************************************************************/
/* Parse_AssertSlotValues: Gets a single assert slot value for a template. */
/**************************************************************************/
static struct expr *
Parse_AssertSlotValues (Environment * theEnv,
			const char *inputSource,
			struct token *tempToken,
			struct templateSlot *slotPtr,
			bool *error, bool constantsOnly)
{
  struct expr *nextSlot;
  struct expr *newField, *valueList, *lastValue;
  bool printError;

   /*=============================*/
  /* Handle a single field slot. */
   /*=============================*/

  if (slotPtr->multislot == false)
    {
      /*=====================*/
      /* Get the slot value. */
      /*=====================*/

      CL_SavePPBuffer (theEnv, " ");

      newField = Get_AssertArgument (theEnv, inputSource, tempToken, error,
				     RIGHT_PARENTHESIS_TOKEN, constantsOnly,
				     &printError);
      if (*error)
	{
	  if (printError)
	    CL_SyntaxErrorMessage (theEnv, "deftemplate pattern");
	  return NULL;
	}

      /*=================================================*/
      /* A single field slot value must contain a value. */
      /* Only a multifield slot can be empty.            */
      /*=================================================*/

      if (newField == NULL)
	{
	  *error = true;
	  CL_SingleField_SlotCardinalityError (theEnv,
					       slotPtr->slotName->contents);
	  return NULL;
	}

      /*==============================================*/
      /* A function returning a multifield value can  */
      /* not be called to get the value for the slot. */
      /*==============================================*/

      if (newField->type == MF_VARIABLE)
	{
	  *error = true;
	  CL_SingleField_SlotCardinalityError (theEnv,
					       slotPtr->slotName->contents);
	  CL_ReturnExpression (theEnv, newField);
	  return NULL;
	}
      else if (newField->type == FCALL)
	{
	  if ((ExpressionUnknownFunctionType (newField) & SINGLEFIELD_BITS) ==
	      0)
	    {
	      *error = true;
	      CL_SingleField_SlotCardinalityError (theEnv,
						   slotPtr->slotName->
						   contents);
	      CL_ReturnExpression (theEnv, newField);
	      return NULL;
	    }
	}

      /*============================*/
      /* Move on to the next token. */
      /*============================*/

      CL_GetToken (theEnv, inputSource, tempToken);
    }

   /*========================================*/
  /* Handle a multifield slot. CL_Build a list */
  /* of the values stored in the slot.      */
   /*========================================*/

  else
    {
      CL_SavePPBuffer (theEnv, " ");
      valueList = Get_AssertArgument (theEnv, inputSource, tempToken, error,
				      RIGHT_PARENTHESIS_TOKEN, constantsOnly,
				      &printError);
      if (*error)
	{
	  if (printError)
	    CL_SyntaxErrorMessage (theEnv, "deftemplate pattern");
	  return NULL;
	}

      if (valueList == NULL)
	{
	  CL_PPBackup (theEnv);
	  CL_PPBackup (theEnv);
	  CL_SavePPBuffer (theEnv, ")");
	}

      lastValue = valueList;

      while (lastValue != NULL)	/* (tempToken->tknType != RIGHT_PARENTHESIS_TOKEN) */
	{
	  if (tempToken->tknType == RIGHT_PARENTHESIS_TOKEN)
	    {
	      CL_SavePPBuffer (theEnv, " ");
	    }
	  else
	    {
	      /* CL_PPBackup(theEnv); */
	      CL_SavePPBuffer (theEnv, " ");
	      /* CL_SavePPBuffer(theEnv,tempToken->printFo_rm); */
	    }

	  newField =
	    Get_AssertArgument (theEnv, inputSource, tempToken, error,
				RIGHT_PARENTHESIS_TOKEN, constantsOnly,
				&printError);
	  if (*error)
	    {
	      if (printError)
		CL_SyntaxErrorMessage (theEnv, "deftemplate pattern");
	      CL_ReturnExpression (theEnv, valueList);
	      return NULL;
	    }

	  if (newField == NULL)
	    {
	      CL_PPBackup (theEnv);
	      CL_PPBackup (theEnv);
	      CL_SavePPBuffer (theEnv, ")");
	    }

	  lastValue->nextArg = newField;
	  lastValue = newField;
	}

      newField = valueList;
    }

   /*==========================================================*/
  /* Slot definition must be closed with a right parenthesis. */
   /*==========================================================*/

  if (tempToken->tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      CL_SingleField_SlotCardinalityError (theEnv,
					   slotPtr->slotName->contents);
      *error = true;
      CL_ReturnExpression (theEnv, newField);
      return NULL;
    }

   /*=========================================================*/
  /* CL_Build and return a structure describing the slot value. */
   /*=========================================================*/

  nextSlot = CL_GenConstant (theEnv, SYMBOL_TYPE, slotPtr->slotName);
  nextSlot->argList = newField;

  return (nextSlot);
}

/*************************************************************************/
/* Reorder_AssertSlotValues: Rearranges the asserted values to correspond */
/*   to the order of the values described by the deftemplate.            */
/*************************************************************************/
static struct expr *
Reorder_AssertSlotValues (Environment * theEnv,
			  struct templateSlot *slotPtr,
			  struct expr *firstSlot, bool *error)
{
  struct expr *firstArg = NULL;
  struct expr *lastArg = NULL, *newArg;

   /*=============================================*/
  /* Loop through each of the slots in the order */
  /* they're found in the deftemplate.           */
   /*=============================================*/

  for (; slotPtr != NULL; slotPtr = slotPtr->next)
    {
      /*==============================================*/
      /* Get either the value specified in the assert */
      /* command or the default value for the slot.   */
      /*==============================================*/

      newArg = GetSlot_AssertValues (theEnv, slotPtr, firstSlot, error);

      if (*error)
	{
	  CL_ReturnExpression (theEnv, firstArg);
	  return NULL;
	}

      /*=====================================*/
      /* Add the value to the list of values */
      /* for the assert command.             */
      /*=====================================*/

      if (newArg != NULL)
	{
	  if (lastArg == NULL)
	    {
	      firstArg = newArg;
	    }
	  else
	    {
	      lastArg->nextArg = newArg;
	    }

	  lastArg = newArg;
	}
    }

   /*==============================*/
  /* Return the list of arguments */
  /* for the assert command.      */
   /*==============================*/

  return (firstArg);
}

/***************************************************************/
/* GetSlot_AssertValues: Gets the assert value for a given slot */
/*   of a deftemplate. If the value was supplied by the user,  */
/*   it will be used. If not the default value or default      */
/*   default value will be used.                               */
/***************************************************************/
static struct expr *
GetSlot_AssertValues (Environment * theEnv,
		      struct templateSlot *slotPtr,
		      struct expr *firstSlot, bool *error)
{
  struct expr *slotItem;
  struct expr *newArg, *tempArg;
  UDFValue theDefault;
  const char *nullBitMap = "\0";

   /*==================================================*/
  /* Dete_rmine if the slot is assigned in the assert. */
   /*==================================================*/

  slotItem = Find_AssertSlotItem (slotPtr, firstSlot);

   /*==========================================*/
  /* If the slot is assigned, use that value. */
   /*==========================================*/

  if (slotItem != NULL)
    {
      newArg = slotItem->argList;
      slotItem->argList = NULL;
    }

   /*=================================*/
  /* Otherwise, use a default value. */
   /*=================================*/

  else
    {
      /*================================================*/
      /* If the (default ?NONE) attribute was specified */
      /* for the slot, then a value must be supplied.   */
      /*================================================*/

      if (slotPtr->noDefault)
	{
	  CL_PrintErrorID (theEnv, "TMPLTRHS", 1, true);
	  CL_WriteString (theEnv, STDERR, "Slot '");
	  CL_WriteString (theEnv, STDERR, slotPtr->slotName->contents);
	  CL_WriteString (theEnv, STDERR,
			  "' requires a value because of its (default ?NONE) attribute.\n");
	  *error = true;
	  return NULL;
	}

      /*===================================================*/
      /* If the (default ?DERIVE) attribute was specified  */
      /* (the default), then derive the default value from */
      /* the slot's constraints.                           */
      /*===================================================*/

      else if ((slotPtr->defaultPresent == false) &&
	       (slotPtr->defaultDynamic == false))
	{
	  CL_DeriveDefaultFromConstraints (theEnv, slotPtr->constraints,
					   &theDefault, slotPtr->multislot,
					   true);
	  newArg = CL_ConvertValueToExpression (theEnv, &theDefault);
	}

      /*=========================================*/
      /* Otherwise, use the expression contained */
      /* in the default attribute.               */
      /*=========================================*/

      else
	{
	  newArg = CL_CopyExpression (theEnv, slotPtr->defaultList);
	}
    }

   /*=======================================================*/
  /* Since a multifield slot default can contain a list of */
  /* values, the values need to have a store-multifield    */
  /* function called wrapped around it to group all of the */
  /* values into a single multifield value.                */
   /*=======================================================*/

  if (slotPtr->multislot)
    {
      tempArg =
	CL_GenConstant (theEnv, FACT_STORE_MULTIFIELD,
			CL_AddBitMap (theEnv, (void *) nullBitMap, 1));
      tempArg->argList = newArg;
      newArg = tempArg;
    }

   /*==============================================*/
  /* Return the value to be asserted in the slot. */
   /*==============================================*/

  return (newArg);
}

/*******************************************************************/
/* Find_AssertSlotItem: Finds a particular slot in a list of slots. */
/*******************************************************************/
static struct expr *
Find_AssertSlotItem (struct templateSlot *slotPtr, struct expr *listOfSlots)
{
  while (listOfSlots != NULL)
    {
      if (listOfSlots->value == (void *) slotPtr->slotName)
	return (listOfSlots);
      listOfSlots = listOfSlots->nextArg;
    }

  return NULL;
}

#endif /* DEFTEMPLATE_CONSTRUCT */
