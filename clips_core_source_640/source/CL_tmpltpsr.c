   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/11/16             */
   /*                                                     */
   /*              DEFTEMPLATE PARSER MODULE              */
   /*******************************************************/

/*************************************************************/
/* Purpose: Parses the deftemplate construct.                */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Added support for templates CL_maintaining their  */
/*            own list of facts.                             */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            CL_GetConstructNameAndComment API change.         */
/*                                                           */
/*            Support for deftemplate slot facets.           */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Static constraint checking is always enabled.  */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT

#include <stdio.h>
#include <string.h>

#if BLOAD || BLOAD_AND_BSAVE
#include "bload.h"
#endif
#include "constant.h"
#include "constrct.h"
#include "cstrcpsr.h"
#include "cstrnchk.h"
#include "cstrnpsr.h"
#include "cstrnutl.h"
#include "default.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "factmngr.h"
#include "memalloc.h"
#include "modulutl.h"
#include "pattern.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"
#include "symbol.h"
#include "tmpltbsc.h"
#include "tmpltdef.h"
#include "watch.h"

#include "tmpltpsr.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if (! RUN_TIME) && (! BLOAD_ONLY)
static struct templateSlot *SlotDeclarations (Environment *, const char *,
					      struct token *);
static struct templateSlot *CL_ParseSlot (Environment *, const char *,
					  struct token *,
					  struct templateSlot *);
static struct templateSlot *DefinedSlots (Environment *, const char *,
					  CLIPSLexeme *, bool,
					  struct token *);
static bool ParseFacetAttribute (Environment *, const char *,
				 struct templateSlot *, bool);
#endif

/*******************************************************/
/* CL_ParseDeftemplate: Parses the deftemplate construct. */
/*******************************************************/
bool
CL_ParseDeftemplate (Environment * theEnv, const char *readSource)
{
#if (! RUN_TIME) && (! BLOAD_ONLY)
  CLIPSLexeme *deftemplateName;
  Deftemplate *newDeftemplate;
  struct templateSlot *slots;
  struct token inputToken;

   /*================================================*/
  /* Initialize pretty print and error info_rmation. */
   /*================================================*/

  DeftemplateData (theEnv)->DeftemplateError = false;
  CL_SetPPBufferStatus (theEnv, true);
  CL_FlushPPBuffer (theEnv);
  CL_SavePPBuffer (theEnv, "(deftemplate ");

   /*==============================================================*/
  /* Deftemplates can not be added when a binary image is loaded. */
   /*==============================================================*/

#if BLOAD || BLOAD_AND_BSAVE
  if ((CL_Bloaded (theEnv) == true)
      && (!ConstructData (theEnv)->CL_CheckSyntaxMode))
    {
      Cannot_LoadWith_BloadMessage (theEnv, "deftemplate");
      return true;
    }
#endif

   /*=======================================================*/
  /* Parse the name and comment fields of the deftemplate. */
   /*=======================================================*/

#if DEBUGGING_FUNCTIONS
  DeftemplateData (theEnv)->DeletedTemplateDebugFlags = 0;
#endif

  deftemplateName =
    CL_GetConstructNameAndComment (theEnv, readSource, &inputToken,
				   "deftemplate",
				   (CL_FindConstructFunction *)
				   CL_FindDeftemplateInModule,
				   (DeleteConstructFunction *)
				   CL_Undeftemplate, "%", true, true, true,
				   false);

  if (deftemplateName == NULL)
    return true;

  if (CL_ReservedPatternSymbol
      (theEnv, deftemplateName->contents, "deftemplate"))
    {
      CL_ReservedPatternSymbolErrorMsg (theEnv, deftemplateName->contents,
					"a deftemplate name");
      return true;
    }

   /*===========================================*/
  /* Parse the slot fields of the deftemplate. */
   /*===========================================*/

  slots = SlotDeclarations (theEnv, readSource, &inputToken);
  if (DeftemplateData (theEnv)->DeftemplateError == true)
    return true;

   /*==============================================*/
  /* If we're only checking syntax, don't add the */
  /* successfully parsed deftemplate to the KB.   */
   /*==============================================*/

  if (ConstructData (theEnv)->CL_CheckSyntaxMode)
    {
      CL_ReturnSlots (theEnv, slots);
      return false;
    }

   /*=====================================*/
  /* Create a new deftemplate structure. */
   /*=====================================*/

  newDeftemplate = get_struct (theEnv, deftemplate);
  newDeftemplate->header.name = deftemplateName;
  newDeftemplate->header.next = NULL;
  newDeftemplate->header.usrData = NULL;
  newDeftemplate->header.constructType = DEFTEMPLATE;
  newDeftemplate->header.env = theEnv;
  newDeftemplate->slotList = slots;
  newDeftemplate->implied = false;
  newDeftemplate->numberOfSlots = 0;
  newDeftemplate->busyCount = 0;
  newDeftemplate->watch = 0;
  newDeftemplate->inScope = true;
  newDeftemplate->patternNetwork = NULL;
  newDeftemplate->factList = NULL;
  newDeftemplate->lastFact = NULL;
  newDeftemplate->header.whichModule = (struct defmoduleItemHeader *)
    CL_GetModuleItem (theEnv, NULL,
		      DeftemplateData (theEnv)->CL_DeftemplateModuleIndex);

   /*================================*/
  /* Dete_rmine the number of slots. */
   /*================================*/

  while (slots != NULL)
    {
      newDeftemplate->numberOfSlots++;
      slots = slots->next;
    }

   /*====================================*/
  /* Store pretty print representation. */
   /*====================================*/

  if (CL_GetConserveMemory (theEnv) == true)
    {
      newDeftemplate->header.ppFo_rm = NULL;
    }
  else
    {
      newDeftemplate->header.ppFo_rm = CL_CopyPPBuffer (theEnv);
    }

   /*=======================================================================*/
  /* If a template is redefined, then we want to restore its watch status. */
   /*=======================================================================*/

#if DEBUGGING_FUNCTIONS
  if ((BitwiseTest (DeftemplateData (theEnv)->DeletedTemplateDebugFlags, 0))
      || (CL_Get_WatchItem (theEnv, "facts") == 1))
    {
      CL_DeftemplateSet_Watch (newDeftemplate, true);
    }
#endif

   /*==============================================*/
  /* Add deftemplate to the list of deftemplates. */
   /*==============================================*/

  CL_AddConstructToModule (&newDeftemplate->header);

  CL_InstallDeftemplate (theEnv, newDeftemplate);

#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif

  return false;
}

#if (! RUN_TIME) && (! BLOAD_ONLY)

/**************************************************************/
/* CL_InstallDeftemplate: Increments all occurrences in the hash */
/*   table of symbols found in an deftemplate and adds it to  */
/*   the hash table.                                          */
/**************************************************************/
void
CL_InstallDeftemplate (Environment * theEnv, Deftemplate * theDeftemplate)
{
  struct templateSlot *slotPtr;
  struct expr *tempExpr;

  IncrementLexemeCount (theDeftemplate->header.name);

  for (slotPtr = theDeftemplate->slotList;
       slotPtr != NULL; slotPtr = slotPtr->next)
    {
      IncrementLexemeCount (slotPtr->slotName);
      tempExpr = CL_AddHashedExpression (theEnv, slotPtr->defaultList);
      CL_ReturnExpression (theEnv, slotPtr->defaultList);
      slotPtr->defaultList = tempExpr;
      tempExpr = CL_AddHashedExpression (theEnv, slotPtr->facetList);
      CL_ReturnExpression (theEnv, slotPtr->facetList);
      slotPtr->facetList = tempExpr;
      slotPtr->constraints = CL_AddConstraint (theEnv, slotPtr->constraints);
    }
}

/********************************************************************/
/* SlotDeclarations: Parses the slot declarations of a deftemplate. */
/********************************************************************/
static struct templateSlot *
SlotDeclarations (Environment * theEnv,
		  const char *readSource, struct token *inputToken)
{
  struct templateSlot *newSlot, *slotList = NULL, *lastSlot = NULL;
  struct templateSlot *multiSlot = NULL;

  while (inputToken->tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      /*====================================================*/
      /* Slots begin with a '(' followed by a slot keyword. */
      /*====================================================*/

      if (inputToken->tknType != LEFT_PARENTHESIS_TOKEN)
	{
	  CL_SyntaxErrorMessage (theEnv, "deftemplate");
	  CL_ReturnSlots (theEnv, slotList);
	  CL_ReturnSlots (theEnv, multiSlot);
	  DeftemplateData (theEnv)->DeftemplateError = true;
	  return NULL;
	}

      CL_GetToken (theEnv, readSource, inputToken);
      if (inputToken->tknType != SYMBOL_TOKEN)
	{
	  CL_SyntaxErrorMessage (theEnv, "deftemplate");
	  CL_ReturnSlots (theEnv, slotList);
	  CL_ReturnSlots (theEnv, multiSlot);
	  DeftemplateData (theEnv)->DeftemplateError = true;
	  return NULL;
	}

      /*=================*/
      /* Parse the slot. */
      /*=================*/

      newSlot = CL_ParseSlot (theEnv, readSource, inputToken, slotList);
      if (DeftemplateData (theEnv)->DeftemplateError == true)
	{
	  CL_ReturnSlots (theEnv, newSlot);
	  CL_ReturnSlots (theEnv, slotList);
	  CL_ReturnSlots (theEnv, multiSlot);
	  return NULL;
	}

      /*===========================================*/
      /* Attach the new slot to the list of slots. */
      /*===========================================*/

      if (newSlot != NULL)
	{
	  if (lastSlot == NULL)
	    {
	      slotList = newSlot;
	    }
	  else
	    {
	      lastSlot->next = newSlot;
	    }
	  lastSlot = newSlot;
	}

      /*================================*/
      /* Check for closing parenthesis. */
      /*================================*/

      CL_GetToken (theEnv, readSource, inputToken);
      if (inputToken->tknType != RIGHT_PARENTHESIS_TOKEN)
	{
	  CL_PPBackup (theEnv);
	  CL_SavePPBuffer (theEnv, "\n   ");
	  CL_SavePPBuffer (theEnv, inputToken->printFo_rm);
	}
    }

  CL_SavePPBuffer (theEnv, "\n");

  /*=======================*/
  /* Return the slot list. */
  /*=======================*/

  return (slotList);
}

/*****************************************************/
/* CL_ParseSlot: Parses a single slot of a deftemplate. */
/*****************************************************/
static struct templateSlot *
CL_ParseSlot (Environment * theEnv,
	      const char *readSource,
	      struct token *inputToken, struct templateSlot *slotList)
{
  bool parsingMultislot;
  CLIPSLexeme *slotName;
  struct templateSlot *newSlot;
  ConstraintViolationType rv;

   /*=====================================================*/
  /* Slots must  begin with keyword field or multifield. */
   /*=====================================================*/

  if ((strcmp (inputToken->lexemeValue->contents, "field") != 0) &&
      (strcmp (inputToken->lexemeValue->contents, "multifield") != 0) &&
      (strcmp (inputToken->lexemeValue->contents, "slot") != 0) &&
      (strcmp (inputToken->lexemeValue->contents, "multislot") != 0))
    {
      CL_SyntaxErrorMessage (theEnv, "deftemplate");
      DeftemplateData (theEnv)->DeftemplateError = true;
      return NULL;
    }

   /*===============================================*/
  /* Dete_rmine if multifield slot is being parsed. */
   /*===============================================*/

  if ((strcmp (inputToken->lexemeValue->contents, "multifield") == 0) ||
      (strcmp (inputToken->lexemeValue->contents, "multislot") == 0))
    {
      parsingMultislot = true;
    }
  else
    {
      parsingMultislot = false;
    }

   /*========================================*/
  /* The name of the slot must be a symbol. */
   /*========================================*/

  CL_SavePPBuffer (theEnv, " ");
  CL_GetToken (theEnv, readSource, inputToken);
  if (inputToken->tknType != SYMBOL_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "deftemplate");
      DeftemplateData (theEnv)->DeftemplateError = true;
      return NULL;
    }

  slotName = inputToken->lexemeValue;

   /*================================================*/
  /* Dete_rmine if the slot has already been parsed. */
   /*================================================*/

  while (slotList != NULL)
    {
      if (slotList->slotName == slotName)
	{
	  CL_AlreadyParsedErrorMessage (theEnv, "slot ",
					slotList->slotName->contents);
	  DeftemplateData (theEnv)->DeftemplateError = true;
	  return NULL;
	}

      slotList = slotList->next;
    }

   /*===================================*/
  /* Parse the attributes of the slot. */
   /*===================================*/

  newSlot =
    DefinedSlots (theEnv, readSource, slotName, parsingMultislot, inputToken);
  if (newSlot == NULL)
    {
      DeftemplateData (theEnv)->DeftemplateError = true;
      return NULL;
    }

   /*=================================*/
  /* Check for slot conflict errors. */
   /*=================================*/

  if (CL_CheckConstraintParseConflicts (theEnv, newSlot->constraints) ==
      false)
    {
      CL_ReturnSlots (theEnv, newSlot);
      DeftemplateData (theEnv)->DeftemplateError = true;
      return NULL;
    }

  if ((newSlot->defaultPresent) || (newSlot->defaultDynamic))
    {
      rv =
	CL_ConstraintCheckExpressionChain (theEnv, newSlot->defaultList,
					   newSlot->constraints);
    }
  else
    {
      rv = NO_VIOLATION;
    }

  if (rv != NO_VIOLATION)
    {
      const char *temp;
      if (newSlot->defaultDynamic)
	temp = "the default-dynamic attribute";
      else
	temp = "the default attribute";
      CL_ConstraintViolationErrorMessage (theEnv, "An expression", temp,
					  false, 0, newSlot->slotName, 0, rv,
					  newSlot->constraints, true);
      CL_ReturnSlots (theEnv, newSlot);
      DeftemplateData (theEnv)->DeftemplateError = true;
      return NULL;
    }

   /*==================*/
  /* Return the slot. */
   /*==================*/

  return (newSlot);
}

/**************************************************************/
/* DefinedSlots: Parses a field or multifield slot attribute. */
/**************************************************************/
static struct templateSlot *
DefinedSlots (Environment * theEnv,
	      const char *readSource,
	      CLIPSLexeme * slotName,
	      bool multifieldSlot, struct token *inputToken)
{
  struct templateSlot *newSlot;
  struct expr *defaultList;
  bool defaultFound = false;
  bool noneSpecified, deriveSpecified;
  CONSTRAINT_PARSE_RECORD parsedConstraints;

   /*===========================*/
  /* CL_Build the slot container. */
   /*===========================*/

  newSlot = get_struct (theEnv, templateSlot);
  newSlot->slotName = slotName;
  newSlot->defaultList = NULL;
  newSlot->facetList = NULL;
  newSlot->constraints = CL_GetConstraintRecord (theEnv);
  if (multifieldSlot)
    {
      newSlot->constraints->multifieldsAllowed = true;
    }
  newSlot->multislot = multifieldSlot;
  newSlot->noDefault = false;
  newSlot->defaultPresent = false;
  newSlot->defaultDynamic = false;
  newSlot->next = NULL;

   /*========================================*/
  /* Parse the primitive slot if it exists. */
   /*========================================*/

  CL_InitializeConstraintParseRecord (&parsedConstraints);
  CL_GetToken (theEnv, readSource, inputToken);

  while (inputToken->tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      CL_PPBackup (theEnv);
      CL_SavePPBuffer (theEnv, " ");
      CL_SavePPBuffer (theEnv, inputToken->printFo_rm);

      /*================================================*/
      /* Slot attributes begin with a left parenthesis. */
      /*================================================*/

      if (inputToken->tknType != LEFT_PARENTHESIS_TOKEN)
	{
	  CL_SyntaxErrorMessage (theEnv, "deftemplate");
	  CL_ReturnSlots (theEnv, newSlot);
	  DeftemplateData (theEnv)->DeftemplateError = true;
	  return NULL;
	}

      /*=============================================*/
      /* The name of the attribute must be a symbol. */
      /*=============================================*/

      CL_GetToken (theEnv, readSource, inputToken);
      if (inputToken->tknType != SYMBOL_TOKEN)
	{
	  CL_SyntaxErrorMessage (theEnv, "deftemplate");
	  CL_ReturnSlots (theEnv, newSlot);
	  DeftemplateData (theEnv)->DeftemplateError = true;
	  return NULL;
	}

      /*================================================================*/
      /* Dete_rmine if the attribute is one of the standard constraints. */
      /*================================================================*/

      if (CL_StandardConstraint (inputToken->lexemeValue->contents))
	{
	  if (CL_Parse_StandardConstraint
	      (theEnv, readSource, (inputToken->lexemeValue->contents),
	       newSlot->constraints, &parsedConstraints,
	       multifieldSlot) == false)
	    {
	      DeftemplateData (theEnv)->DeftemplateError = true;
	      CL_ReturnSlots (theEnv, newSlot);
	      return NULL;
	    }
	}

      /*=================================================*/
      /* else if the attribute is the default attribute, */
      /* then get the default list for this slot.        */
      /*=================================================*/

      else if ((strcmp (inputToken->lexemeValue->contents, "default") == 0) ||
	       (strcmp (inputToken->lexemeValue->contents, "default-dynamic")
		== 0))
	{
	 /*======================================================*/
	  /* Check to see if the default has already been parsed. */
	 /*======================================================*/

	  if (defaultFound)
	    {
	      CL_AlreadyParsedErrorMessage (theEnv, "default attribute",
					    NULL);
	      DeftemplateData (theEnv)->DeftemplateError = true;
	      CL_ReturnSlots (theEnv, newSlot);
	      return NULL;
	    }

	  newSlot->noDefault = false;

	 /*=====================================================*/
	  /* Dete_rmine whether the default is dynamic or static. */
	 /*=====================================================*/

	  if (strcmp (inputToken->lexemeValue->contents, "default") == 0)
	    {
	      newSlot->defaultPresent = true;
	      newSlot->defaultDynamic = false;
	    }
	  else
	    {
	      newSlot->defaultPresent = false;
	      newSlot->defaultDynamic = true;
	    }

	 /*===================================*/
	  /* Parse the list of default values. */
	 /*===================================*/

	  defaultList =
	    CL_ParseDefault (theEnv, readSource, multifieldSlot,
			     newSlot->defaultDynamic, true, &noneSpecified,
			     &deriveSpecified,
			     &DeftemplateData (theEnv)->DeftemplateError);
	  if (DeftemplateData (theEnv)->DeftemplateError == true)
	    {
	      CL_ReturnSlots (theEnv, newSlot);
	      return NULL;
	    }

	 /*==================================*/
	  /* Store the default with the slot. */
	 /*==================================*/

	  defaultFound = true;
	  if (deriveSpecified)
	    newSlot->defaultPresent = false;
	  else if (noneSpecified)
	    {
	      newSlot->noDefault = true;
	      newSlot->defaultPresent = false;
	    }
	  newSlot->defaultList = defaultList;
	}

      /*===============================================*/
      /* else if the attribute is the facet attribute. */
      /*===============================================*/

      else if (strcmp (inputToken->lexemeValue->contents, "facet") == 0)
	{
	  if (!ParseFacetAttribute (theEnv, readSource, newSlot, false))
	    {
	      CL_ReturnSlots (theEnv, newSlot);
	      DeftemplateData (theEnv)->DeftemplateError = true;
	      return NULL;
	    }
	}

      else if (strcmp (inputToken->lexemeValue->contents, "multifacet") == 0)
	{
	  if (!ParseFacetAttribute (theEnv, readSource, newSlot, true))
	    {
	      CL_ReturnSlots (theEnv, newSlot);
	      DeftemplateData (theEnv)->DeftemplateError = true;
	      return NULL;
	    }
	}

      /*============================================*/
      /* Otherwise the attribute is an invalid one. */
      /*============================================*/

      else
	{
	  CL_SyntaxErrorMessage (theEnv, "slot attributes");
	  CL_ReturnSlots (theEnv, newSlot);
	  DeftemplateData (theEnv)->DeftemplateError = true;
	  return NULL;
	}

      /*===================================*/
      /* Begin parsing the next attribute. */
      /*===================================*/

      CL_GetToken (theEnv, readSource, inputToken);
    }

   /*============================*/
  /* Return the attribute list. */
   /*============================*/

  return (newSlot);
}

/***************************************************/
/* ParseFacetAttribute: Parses the type attribute. */
/***************************************************/
static bool
ParseFacetAttribute (Environment * theEnv,
		     const char *readSource,
		     struct templateSlot *theSlot, bool multifacet)
{
  struct token inputToken;
  CLIPSLexeme *facetName;
  struct expr *facetPair, *tempFacet, *facetValue = NULL, *lastValue = NULL;

   /*==============================*/
  /* Parse the name of the facet. */
   /*==============================*/

  CL_SavePPBuffer (theEnv, " ");
  CL_GetToken (theEnv, readSource, &inputToken);

   /*==================================*/
  /* The facet name must be a symbol. */
   /*==================================*/

  if (inputToken.tknType != SYMBOL_TOKEN)
    {
      if (multifacet)
	CL_SyntaxErrorMessage (theEnv, "multifacet attribute");
      else
	CL_SyntaxErrorMessage (theEnv, "facet attribute");
      return false;
    }

  facetName = inputToken.lexemeValue;

   /*===================================*/
  /* Don't allow facets with the same  */
  /* name as a predefined CLIPS facet. */
   /*===================================*/

   /*====================================*/
  /* Has the facet already been parsed? */
   /*====================================*/

  for (tempFacet = theSlot->facetList;
       tempFacet != NULL; tempFacet = tempFacet->nextArg)
    {
      if (tempFacet->value == facetName)
	{
	  if (multifacet)
	    CL_AlreadyParsedErrorMessage (theEnv, "multifacet ",
					  facetName->contents);
	  else
	    CL_AlreadyParsedErrorMessage (theEnv, "facet ",
					  facetName->contents);
	  return false;
	}
    }

   /*===============================*/
  /* Parse the value of the facet. */
   /*===============================*/

  CL_SavePPBuffer (theEnv, " ");
  CL_GetToken (theEnv, readSource, &inputToken);

  while (inputToken.tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      /*=====================================*/
      /* The facet value must be a constant. */
      /*=====================================*/

      if (!CL_ConstantType (CL_TokenTypeToType (inputToken.tknType)))
	{
	  if (multifacet)
	    CL_SyntaxErrorMessage (theEnv, "multifacet attribute");
	  else
	    CL_SyntaxErrorMessage (theEnv, "facet attribute");
	  CL_ReturnExpression (theEnv, facetValue);
	  return false;
	}

      /*======================================*/
      /* Add the value to the list of values. */
      /*======================================*/

      if (lastValue == NULL)
	{
	  facetValue =
	    CL_GenConstant (theEnv, CL_TokenTypeToType (inputToken.tknType),
			    inputToken.value);
	  lastValue = facetValue;
	}
      else
	{
	  lastValue->nextArg =
	    CL_GenConstant (theEnv, CL_TokenTypeToType (inputToken.tknType),
			    inputToken.value);
	  lastValue = lastValue->nextArg;
	}

      /*=====================*/
      /* Get the next token. */
      /*=====================*/

      CL_SavePPBuffer (theEnv, " ");
      CL_GetToken (theEnv, readSource, &inputToken);

      /*===============================================*/
      /* A facet can't contain more than one constant. */
      /*===============================================*/

      if ((!multifacet) && (inputToken.tknType != RIGHT_PARENTHESIS_TOKEN))
	{
	  CL_SyntaxErrorMessage (theEnv, "facet attribute");
	  CL_ReturnExpression (theEnv, facetValue);
	  return false;
	}
    }

   /*========================================================*/
  /* Remove the space before the closing right parenthesis. */
   /*========================================================*/

  CL_PPBackup (theEnv);
  CL_PPBackup (theEnv);
  CL_SavePPBuffer (theEnv, ")");

   /*====================================*/
  /* A facet must contain one constant. */
   /*====================================*/

  if ((!multifacet) && (facetValue == NULL))
    {
      CL_SyntaxErrorMessage (theEnv, "facet attribute");
      return false;
    }

   /*=================================================*/
  /* Add the facet to the list of the slot's facets. */
   /*=================================================*/

  facetPair = CL_GenConstant (theEnv, SYMBOL_TYPE, facetName);

  if (multifacet)
    {
      facetPair->argList =
	CL_GenConstant (theEnv, FCALL, CL_FindFunction (theEnv, "create$"));
      facetPair->argList->argList = facetValue;
    }
  else
    {
      facetPair->argList = facetValue;
    }

  facetPair->nextArg = theSlot->facetList;
  theSlot->facetList = facetPair;

   /*===============================================*/
  /* The facet/multifacet was successfully parsed. */
   /*===============================================*/

  return true;
}

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */

#endif /* DEFTEMPLATE_CONSTRUCT */
