   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/05/18             */
   /*                                                     */
   /*            STRING_TYPE FUNCTIONS MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for several string functions   */
/*   including str-cat, sym-cat, str-length, str-compare,    */
/*   upcase, lowcase, sub-string, str-index, eval, and       */
/*   build.                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Barry Cameron                                        */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Used CL_gensprintf instead of sprintf.            */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Added support for UTF-8 strings to str-length, */
/*            str-index, and sub-string functions.           */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Added code to keep track of pointers to        */
/*            constructs that are contained externally to    */
/*            to constructs, DanglingConstructs.             */
/*                                                           */
/*            Fixed str-cat bug that could be invoked by     */
/*            (funcall str-cat).                             */
/*                                                           */
/*      6.31: Prior error flags are cleared before CL_Eval      */
/*            and CL_Build are processed.                       */
/*                                                           */
/*      6.40: Added Env prefix to Get_EvaluationError and     */
/*            Set_EvaluationError functions.                  */
/*                                                           */
/*            Added Env prefix to CL_Get_HaltExecution and       */
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
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*            The eval function can now access any local     */
/*            variables that have been defined.              */
/*                                                           */
/*            The str-index function now returns 1 if the    */
/*            search string is "".                           */
/*                                                           */
/*            The eval and build functions generate an       */
/*            error if extraneous input is encountered.      */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if STRING_FUNCTIONS

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "argacces.h"
#include "commline.h"
#include "constrct.h"
#include "cstrcpsr.h"
#include "engine.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "extnfunc.h"
#include "memalloc.h"
#include "miscfun.h"
#include "multifld.h"
#include "prcdrpsr.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "strngrtr.h"
#include "scanner.h"
#include "sysdep.h"
#include "utility.h"

#if DEFRULE_CONSTRUCT
#include "drive.h"
#endif

#include "strngfun.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void StrOr_SymCatFunction (UDFContext *, UDFValue *, unsigned short);

/******************************************/
/* CL_StringFunctionDefinitions: Initializes */
/*   the string manipulation functions.   */
/******************************************/
void
CL_StringFunctionDefinitions (Environment * theEnv)
{
#if ! RUN_TIME
  CL_AddUDF (theEnv, "str-cat", "sy", 1, UNBOUNDED, "synld",
	     CL_StrCatFunction, "CL_StrCatFunction", NULL);
  CL_AddUDF (theEnv, "sym-cat", "sy", 1, UNBOUNDED, "synld",
	     CL_SymCatFunction, "CL_SymCatFunction", NULL);
  CL_AddUDF (theEnv, "str-length", "l", 1, 1, "syn", Str_LengthFunction,
	     "Str_LengthFunction", NULL);
  CL_AddUDF (theEnv, "str-compare", "l", 2, 3, "*;syn;syn;l",
	     CL_StrCompareFunction, "CL_StrCompareFunction", NULL);
  CL_AddUDF (theEnv, "upcase", "syn", 1, 1, "syn", CL_UpcaseFunction,
	     "CL_UpcaseFunction", NULL);
  CL_AddUDF (theEnv, "lowcase", "syn", 1, 1, "syn", CL_LowcaseFunction,
	     "CL_LowcaseFunction", NULL);
  CL_AddUDF (theEnv, "sub-string", "s", 3, 3, "*;l;l;syn",
	     CL_SubStringFunction, "CL_SubStringFunction", NULL);
  CL_AddUDF (theEnv, "str-index", "bl", 2, 2, "syn", CL_StrIndexFunction,
	     "CL_StrIndexFunction", NULL);
  CL_AddUDF (theEnv, "eval", "*", 1, 1, "sy", CL_EvalFunction,
	     "CL_EvalFunction", NULL);
  CL_AddUDF (theEnv, "build", "b", 1, 1, "sy", CL_BuildFunction,
	     "CL_BuildFunction", NULL);
  CL_AddUDF (theEnv, "string-to-field", "*", 1, 1, "syn",
	     CL_StringToFieldFunction, "CL_StringToFieldFunction", NULL);
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
}

/****************************************/
/* CL_StrCatFunction: H/L access routine   */
/*   for the str-cat function.          */
/****************************************/
void
CL_StrCatFunction (Environment * theEnv,
		   UDFContext * context, UDFValue * returnValue)
{
  StrOr_SymCatFunction (context, returnValue, STRING_TYPE);
}

/****************************************/
/* CL_SymCatFunction: H/L access routine   */
/*   for the sym-cat function.          */
/****************************************/
void
CL_SymCatFunction (Environment * theEnv,
		   UDFContext * context, UDFValue * returnValue)
{
  StrOr_SymCatFunction (context, returnValue, SYMBOL_TYPE);
}

/********************************************************/
/* StrOr_SymCatFunction: Driver routine for implementing */
/*   the str-cat and sym-cat functions.                 */
/********************************************************/
static void
StrOr_SymCatFunction (UDFContext * context,
		      UDFValue * returnValue, unsigned short returnType)
{
  UDFValue theArg;
  unsigned int numArgs;
  unsigned int i;
  size_t total;
  size_t j;
  char *theString;
  CLIPSLexeme **arrayOfStrings;
  CLIPSLexeme *hashPtr;
  Environment *theEnv = context->environment;

   /*===============================================*/
  /* Dete_rmine the number of arguments as create a */
  /* string array which is large enough to store   */
  /* the string representation of each argument.   */
   /*===============================================*/

  numArgs = CL_UDFArgumentCount (context);
  if (numArgs == 0)
    return;

  arrayOfStrings =
    (CLIPSLexeme **) CL_gm1 (theEnv, sizeof (CLIPSLexeme *) * numArgs);
  for (i = 0; i < numArgs; i++)
    {
      arrayOfStrings[i] = NULL;
    }

   /*=============================================*/
  /* CL_Evaluate each argument and store its string */
  /* representation in the string array.         */
   /*=============================================*/

  total = 1;
  for (i = 1; i <= numArgs; i++)
    {
      CL_UDFNthArgument (context, i, ANY_TYPE_BITS, &theArg);

      switch (theArg.header->type)
	{
	case STRING_TYPE:
#if OBJECT_SYSTEM
	case INSTANCE_NAME_TYPE:
#endif
	case SYMBOL_TYPE:
	  hashPtr = theArg.lexemeValue;
	  arrayOfStrings[i - 1] = hashPtr;
	  IncrementLexemeCount (hashPtr);
	  break;

	case FLOAT_TYPE:
	  hashPtr =
	    CL_CreateString (theEnv,
			     CL_FloatToString (theEnv,
					       theArg.floatValue->contents));
	  arrayOfStrings[i - 1] = hashPtr;
	  IncrementLexemeCount (hashPtr);
	  break;

	case INTEGER_TYPE:
	  hashPtr =
	    CL_CreateString (theEnv,
			     CL_LongIntegerToString (theEnv,
						     theArg.
						     integerValue->contents));
	  arrayOfStrings[i - 1] = hashPtr;
	  IncrementLexemeCount (hashPtr);
	  break;

	default:
	  CL_UDFInvalidArgumentMessage (context,
					"string, instance name, symbol, float, or integer");
	  Set_EvaluationError (theEnv, true);
	  break;
	}

      if (CL_EvaluationData (theEnv)->CL_EvaluationError)
	{
	  for (i = 0; i < numArgs; i++)
	    {
	      if (arrayOfStrings[i] != NULL)
		{
		  CL_ReleaseLexeme (theEnv, arrayOfStrings[i]);
		}
	    }

	  CL_rm (theEnv, arrayOfStrings, sizeof (CLIPSLexeme *) * numArgs);

	  if (returnType == STRING_TYPE)
	    {
	      returnValue->value = CL_CreateString (theEnv, "");
	    }
	  else
	    {
	      returnValue->value = CL_CreateSymbol (theEnv, "nil");
	    }
	  return;
	}

      total += strlen (arrayOfStrings[i - 1]->contents);
    }

   /*=========================================================*/
  /* Allocate the memory to store the concatenated string or */
  /* symbol, then copy the values in the string array to the */
  /* memory just allocated.                                  */
   /*=========================================================*/

  theString = (char *) CL_gm2 (theEnv, (sizeof (char) * total));

  j = 0;
  for (i = 0; i < numArgs; i++)
    {
      CL_gensprintf (&theString[j], "%s", arrayOfStrings[i]->contents);
      j += strlen (arrayOfStrings[i]->contents);
    }

   /*=========================================*/
  /* Return the concatenated value and clean */
  /* up the temporary memory used.           */
   /*=========================================*/

  if (returnType == STRING_TYPE)
    {
      returnValue->value = CL_CreateString (theEnv, theString);
    }
  else
    {
      returnValue->value = CL_CreateSymbol (theEnv, theString);
    }
  CL_rm (theEnv, theString, sizeof (char) * total);

  for (i = 0; i < numArgs; i++)
    {
      if (arrayOfStrings[i] != NULL)
	{
	  CL_ReleaseLexeme (theEnv, arrayOfStrings[i]);
	}
    }

  CL_rm (theEnv, arrayOfStrings, sizeof (CLIPSLexeme *) * numArgs);
}

/*******************************************/
/* Str_LengthFunction: H/L access routine   */
/*   for the str-length function.          */
/*******************************************/
void
Str_LengthFunction (Environment * theEnv,
		    UDFContext * context, UDFValue * returnValue)
{
  UDFValue theArg;

   /*==================================================================*/
  /* The argument should be of type symbol, string, or instance name. */
   /*==================================================================*/

  if (!CL_UDFFirstArgument
      (context, LEXEME_BITS | INSTANCE_NAME_BIT, &theArg))
    {
      return;
    }

   /*============================================*/
  /* Return the length of the string or symbol. */
   /*============================================*/

  returnValue->integerValue =
    CL_CreateInteger (theEnv,
		      (long long) CL_UTF8Length (theArg.
						 lexemeValue->contents));
}

/****************************************/
/* CL_UpcaseFunction: H/L access routine   */
/*   for the upcase function.           */
/****************************************/
void
CL_UpcaseFunction (Environment * theEnv,
		   UDFContext * context, UDFValue * returnValue)
{
  UDFValue theArg;
  unsigned i;
  size_t slen;
  const char *osptr;
  char *nsptr;

   /*==================================================*/
  /* The argument should be of type symbol or string. */
   /*==================================================*/

  if (!CL_UDFFirstArgument
      (context, LEXEME_BITS | INSTANCE_NAME_BIT, &theArg))
    {
      return;
    }

   /*======================================================*/
  /* Allocate temporary memory and then copy the original */
  /* string or symbol to that memory, while uppercasing   */
  /* lower case alphabetic characters.                    */
   /*======================================================*/

  osptr = theArg.lexemeValue->contents;
  slen = strlen (osptr) + 1;
  nsptr = (char *) CL_gm2 (theEnv, slen);

  for (i = 0; i < slen; i++)
    {
      if (islower (osptr[i]))
	{
	  nsptr[i] = (char) toupper (osptr[i]);
	}
      else
	{
	  nsptr[i] = osptr[i];
	}
    }

   /*========================================*/
  /* Return the uppercased string and clean */
  /* up the temporary memory used.          */
   /*========================================*/

  if (CVIsType (&theArg, SYMBOL_BIT))
    {
      returnValue->value = CL_CreateSymbol (theEnv, nsptr);
    }
  else if (CVIsType (&theArg, INSTANCE_NAME_BIT))
    {
      returnValue->value = CL_Create_InstanceName (theEnv, nsptr);
    }
  else
    {
      returnValue->value = CL_CreateString (theEnv, nsptr);
    }
  CL_rm (theEnv, nsptr, slen);
}

/*****************************************/
/* CL_LowcaseFunction: H/L access routine   */
/*   for the lowcase function.           */
/*****************************************/
void
CL_LowcaseFunction (Environment * theEnv,
		    UDFContext * context, UDFValue * returnValue)
{
  UDFValue theArg;
  unsigned i;
  size_t slen;
  const char *osptr;
  char *nsptr;

   /*==================================================*/
  /* The argument should be of type symbol or string. */
   /*==================================================*/

  if (!CL_UDFFirstArgument
      (context, LEXEME_BITS | INSTANCE_NAME_BIT, &theArg))
    {
      return;
    }

   /*======================================================*/
  /* Allocate temporary memory and then copy the original */
  /* string or symbol to that memory, while lowercasing   */
  /* upper case alphabetic characters.                    */
   /*======================================================*/

  osptr = theArg.lexemeValue->contents;
  slen = strlen (osptr) + 1;
  nsptr = (char *) CL_gm2 (theEnv, slen);

  for (i = 0; i < slen; i++)
    {
      if (isupper (osptr[i]))
	{
	  nsptr[i] = (char) tolower (osptr[i]);
	}
      else
	{
	  nsptr[i] = osptr[i];
	}
    }

   /*========================================*/
  /* Return the lowercased string and clean */
  /* up the temporary memory used.          */
   /*========================================*/

  if (CVIsType (&theArg, SYMBOL_BIT))
    {
      returnValue->value = CL_CreateSymbol (theEnv, nsptr);
    }
  else if (CVIsType (&theArg, INSTANCE_NAME_BIT))
    {
      returnValue->value = CL_Create_InstanceName (theEnv, nsptr);
    }
  else
    {
      returnValue->value = CL_CreateString (theEnv, nsptr);
    }
  CL_rm (theEnv, nsptr, slen);
}

/********************************************/
/* CL_StrCompareFunction: H/L access routine   */
/*   for the str-compare function.          */
/********************************************/
void
CL_StrCompareFunction (Environment * theEnv,
		       UDFContext * context, UDFValue * returnValue)
{
  UDFValue arg1, arg2, arg3;
  int compareResult;

   /*=============================================================*/
  /* The first two arguments should be of type symbol or string. */
   /*=============================================================*/

  if (!CL_UDFFirstArgument (context, LEXEME_BITS | INSTANCE_NAME_BIT, &arg1))
    {
      return;
    }

  if (!CL_UDFNextArgument (context, LEXEME_BITS | INSTANCE_NAME_BIT, &arg2))
    {
      return;
    }

   /*===================================================*/
  /* Compare the strings. Use the 3rd argument for the */
  /* maximum length of comparison, if it is provided.  */
   /*===================================================*/

  if (UDFHasNextArgument (context))
    {
      if (!CL_UDFNextArgument (context, INTEGER_BIT, &arg3))
	{
	  return;
	}

      compareResult =
	strncmp (arg1.lexemeValue->contents, arg2.lexemeValue->contents,
		 (STD_SIZE) arg3.integerValue->contents);
    }
  else
    {
      compareResult =
	strcmp (arg1.lexemeValue->contents, arg2.lexemeValue->contents);
    }

   /*========================================================*/
  /* Return Values are as follows:                          */
  /* -1 is returned if <string-1> is less than <string-2>.  */
  /*  1 is return if <string-1> is greater than <string-2>. */
  /*  0 is returned if <string-1> is equal to <string-2>.   */
   /*========================================================*/

  if (compareResult < 0)
    {
      returnValue->integerValue = CL_CreateInteger (theEnv, -1L);
    }
  else if (compareResult > 0)
    {
      returnValue->integerValue = CL_CreateInteger (theEnv, 1L);
    }
  else
    {
      returnValue->integerValue = CL_CreateInteger (theEnv, 0L);
    }
}

/*******************************************/
/* CL_SubStringFunction: H/L access routine   */
/*   for the sub-string function.          */
/*******************************************/
void
CL_SubStringFunction (Environment * theEnv,
		      UDFContext * context, UDFValue * returnValue)
{
  UDFValue theArg;
  const char *tempString;
  char *returnString;
  size_t start, end, i, j, length;

   /*===================================*/
  /* Check and retrieve the arguments. */
   /*===================================*/

  if (!CL_UDFFirstArgument (context, INTEGER_BIT, &theArg))
    {
      return;
    }

  if (theArg.integerValue->contents < 1)
    {
      start = 0;
    }
  else
    {
      start = (size_t) theArg.integerValue->contents - 1;
    }

  if (!CL_UDFNextArgument (context, INTEGER_BIT, &theArg))
    {
      return;
    }

  if (theArg.integerValue->contents < 1)
    {
      returnValue->lexemeValue = CL_CreateString (theEnv, "");
      return;
    }
  else
    {
      end = (size_t) theArg.integerValue->contents - 1;
    }

  if (!CL_UDFNextArgument (context, LEXEME_BITS | INSTANCE_NAME_BIT, &theArg))
    {
      return;
    }

  tempString = theArg.lexemeValue->contents;

   /*================================================*/
  /* If parameters are out of range return an error */
   /*================================================*/

  length = CL_UTF8Length (tempString);

  if (end > length)
    {
      end = length;
    }

   /*==================================*/
  /* If the start is greater than the */
  /* end, return a null string.       */
   /*==================================*/

  if ((start > end) || (length == 0))
    {
      returnValue->lexemeValue = CL_CreateString (theEnv, "");
      return;
    }

   /*=============================================*/
  /* Otherwise, allocate the string and copy the */
  /* designated portion of the old string to the */
  /* new string.                                 */
   /*=============================================*/

  else
    {
      start = CL_UTF8Offset (tempString, start);
      end = CL_UTF8Offset (tempString, end + 1) - 1;

      returnString = (char *) CL_gm2 (theEnv, (end - start + 2));	/* (end - start) inclusive + EOS */
      for (j = 0, i = start; i <= end; i++, j++)
	{
	  *(returnString + j) = *(tempString + i);
	}
      *(returnString + j) = '\0';
    }

   /*========================*/
  /* Return the new string. */
   /*========================*/

  returnValue->lexemeValue = CL_CreateString (theEnv, returnString);
  CL_rm (theEnv, returnString, (end - start + 2));
}

/******************************************/
/* CL_StrIndexFunction: H/L access routine   */
/*   for the sub-index function.          */
/******************************************/
void
CL_StrIndexFunction (Environment * theEnv,
		     UDFContext * context, UDFValue * returnValue)
{
  UDFValue theArg1, theArg2;
  const char *strg1, *strg2, *strg3;
  size_t i;

  returnValue->lexemeValue = FalseSymbol (theEnv);

   /*===================================*/
  /* Check and retrieve the arguments. */
   /*===================================*/

  if (!CL_UDFFirstArgument
      (context, LEXEME_BITS | INSTANCE_NAME_BIT, &theArg1))
    {
      return;
    }

  if (!CL_UDFNextArgument
      (context, LEXEME_BITS | INSTANCE_NAME_BIT, &theArg2))
    {
      return;
    }

  strg1 = theArg1.lexemeValue->contents;
  strg2 = theArg2.lexemeValue->contents;

   /*=================================*/
  /* Find the position in string2 of */
  /* string1 (counting from 1).      */
   /*=================================*/

  if (strlen (strg1) == 0)
    {
      returnValue->integerValue = CL_CreateInteger (theEnv, 1LL);
      return;
    }

  strg3 = strstr (strg2, strg1);

  if (strg3 != NULL)
    {
      i = (size_t) (strg3 - strg2) + 1;
      returnValue->integerValue =
	CL_CreateInteger (theEnv, (long long) CL_UTF8CharNum (strg2, i));
    }
}

/********************************************/
/* CL_StringToFieldFunction: H/L access routine */
/*   for the string-to-field function.       */
/********************************************/
void
CL_StringToFieldFunction (Environment * theEnv,
			  UDFContext * context, UDFValue * returnValue)
{
  UDFValue theArg;

  CL_ClearErrorValue (theEnv);

   /*==================================================*/
  /* The argument should be of type symbol or string. */
   /*==================================================*/

  if (!CL_UDFFirstArgument
      (context, LEXEME_BITS | INSTANCE_NAME_BIT, &theArg))
    {
      CL_SetErrorValue (theEnv,
			&CL_CreateSymbol (theEnv,
					  "INVALID_ARGUMENT")->header);
      returnValue->lexemeValue = FalseSymbol (theEnv);
      return;
    }

   /*================================*/
  /* Convert the string to an atom. */
   /*================================*/

  CL_StringToField (theEnv, theArg.lexemeValue->contents, returnValue);
}

/*************************************************************/
/* CL_StringToField: Converts a string to an atomic data value. */
/*************************************************************/
void
CL_StringToField (Environment * theEnv,
		  const char *theString, UDFValue * returnValue)
{
  struct token theToken;

   /*====================================*/
  /* Open the string as an input source */
  /* and retrieve the first value.      */
   /*====================================*/

  CL_OpenStringSource (theEnv, "string-to-field-str", theString, 0);
  CL_GetToken (theEnv, "string-to-field-str", &theToken);
  CL_CloseStringSource (theEnv, "string-to-field-str");

   /*====================================================*/
  /* Copy the token to the return value data structure. */
   /*====================================================*/

  if ((theToken.tknType == FLOAT_TOKEN) || (theToken.tknType == STRING_TOKEN)
      ||
#if OBJECT_SYSTEM
      (theToken.tknType == INSTANCE_NAME_TOKEN) ||
#endif
      (theToken.tknType == SYMBOL_TOKEN)
      || (theToken.tknType == INTEGER_TOKEN))
    {
      returnValue->value = theToken.value;
    }
  else if (theToken.tknType == STOP_TOKEN)
    {
      CL_SetErrorValue (theEnv, &CL_CreateSymbol (theEnv, "EOF")->header);
      returnValue->value = CL_CreateSymbol (theEnv, "EOF");
    }
  else if (theToken.tknType == UNKNOWN_VALUE_TOKEN)
    {
      CL_SetErrorValue (theEnv,
			&CL_CreateSymbol (theEnv,
					  "INVALID_ARGUMENT")->header);
      returnValue->lexemeValue = FalseSymbol (theEnv);
    }
  else
    {
      returnValue->value = CL_CreateSymbol (theEnv, theToken.printFo_rm);
    }
}

/**************************************/
/* CL_EvalFunction: H/L access routine   */
/*   for the eval function.           */
/**************************************/
void
CL_EvalFunction (Environment * theEnv,
		 UDFContext * context, UDFValue * returnValue)
{
  UDFValue theArg;
  CLIPSValue cv;

   /*==================================================*/
  /* The argument should be of type SYMBOL or STRING. */
   /*==================================================*/

  if (!CL_UDFFirstArgument (context, LEXEME_BITS, &theArg))
    {
      return;
    }

   /*======================*/
  /* CL_Evaluate the string. */
   /*======================*/

  CL_Eval (theEnv, theArg.lexemeValue->contents, &cv);
  CL_CLIPSToUDFValue (&cv, returnValue);
}

/****************************/
/* CL_Eval: C access routine   */
/*   for the eval function. */
/****************************/
CL_EvalError
CL_Eval (Environment * theEnv,
	 const char *theString, CLIPSValue * returnValue)
{
  struct expr *top;
  bool ov;
  static int depth = 0;
  char logicalNameBuffer[20];
  struct BindInfo *oldBinds;
  int danglingConstructs;
  UDFValue evalResult;
  GCBlock gcb;
  struct token theToken;

   /*========================================*/
  /* Set up the frame for tracking garbage. */
   /*========================================*/

  CL_GCBlockStart (theEnv, &gcb);

   /*=====================================*/
  /* If embedded, clear the error flags. */
   /*=====================================*/

  if (CL_EvaluationData (theEnv)->CurrentExpression == NULL)
    {
      CL_ResetErrorFlags (theEnv);
    }

   /*======================================================*/
  /* CL_Evaluate the string. Create a different logical name */
  /* for use each time the eval function is called.       */
   /*======================================================*/

  depth++;
  CL_gensprintf (logicalNameBuffer, "CL_Eval-%d", depth);
  if (CL_OpenStringSource (theEnv, logicalNameBuffer, theString, 0) == 0)
    {
      CL_SystemError (theEnv, "STRNGFUN", 1);
      CL_ExitRouter (theEnv, EXIT_FAILURE);
    }

   /*================================================*/
  /* CL_Save the current parsing state before routines */
  /* are called to parse the eval string.           */
   /*================================================*/

  ov = CL_GetPPBufferStatus (theEnv);
  CL_SetPPBufferStatus (theEnv, false);
  oldBinds = CL_GetParsedBindNames (theEnv);
  CL_SetParsedBindNames (theEnv, NULL);
  danglingConstructs = ConstructData (theEnv)->DanglingConstructs;

   /*========================================================*/
  /* Parse the string argument passed to the eval function. */
   /*========================================================*/

  top = CL_ParseAtomOrExpression (theEnv, logicalNameBuffer, NULL);

   /*============================*/
  /* Restore the parsing state. */
   /*============================*/

  CL_SetPPBufferStatus (theEnv, ov);
  CL_ClearParsedBindNames (theEnv);
  CL_SetParsedBindNames (theEnv, oldBinds);

   /*===========================================*/
  /* Return if an error occured while parsing. */
   /*===========================================*/

  if (top == NULL)
    {
      Set_EvaluationError (theEnv, true);
      CL_CloseStringSource (theEnv, logicalNameBuffer);
      CL_GCBlockEnd (theEnv, &gcb);
      if (returnValue != NULL)
	{
	  returnValue->lexemeValue = FalseSymbol (theEnv);
	}
      depth--;
      ConstructData (theEnv)->DanglingConstructs = danglingConstructs;
      return EE_PARSING_ERROR;
    }

   /*======================================*/
  /* Return if there is extraneous input. */
   /*======================================*/

  CL_GetToken (theEnv, logicalNameBuffer, &theToken);
  if (theToken.tknType != STOP_TOKEN)
    {
      CL_PrintErrorID (theEnv, "STRNGFUN", 2, false);
      CL_WriteString (theEnv, STDERR,
		      "Function 'eval' encountered extraneous input.\n");
      Set_EvaluationError (theEnv, true);
      CL_ReturnExpression (theEnv, top);
      CL_CloseStringSource (theEnv, logicalNameBuffer);
      CL_GCBlockEnd (theEnv, &gcb);
      if (returnValue != NULL)
	{
	  returnValue->lexemeValue = FalseSymbol (theEnv);
	}
      depth--;
      ConstructData (theEnv)->DanglingConstructs = danglingConstructs;
      return EE_PARSING_ERROR;
    }

   /*====================================*/
  /* CL_Evaluate the expression and return */
  /* the memory used to parse it.       */
   /*====================================*/

  CL_ExpressionInstall (theEnv, top);
  CL_EvaluateExpression (theEnv, top, &evalResult);
  CL_ExpressionDeinstall (theEnv, top);

  depth--;
  CL_ReturnExpression (theEnv, top);
  CL_CloseStringSource (theEnv, logicalNameBuffer);

   /*====================================================*/
  /* Convert a partial multifield to a full multifield. */
   /*====================================================*/

  CL_No_rmalizeMultifield (theEnv, &evalResult);

   /*==============================================*/
  /* If embedded, reset dangling construct count. */
   /*==============================================*/

  if (CL_EvaluationData (theEnv)->CurrentExpression == NULL)
    {
      ConstructData (theEnv)->DanglingConstructs = danglingConstructs;
    }

   /*================================*/
  /* Restore the old garbage frame. */
   /*================================*/

  if (returnValue != NULL)
    {
      CL_GCBlockEndUDF (theEnv, &gcb, &evalResult);
    }
  else
    {
      CL_GCBlockEnd (theEnv, &gcb);
    }

   /*==========================================*/
  /* Perfo_rm periodic cleanup if the eval was */
  /* issued from an embedded controller.      */
   /*==========================================*/

  if (CL_EvaluationData (theEnv)->CurrentExpression == NULL)
    {
      if (returnValue != NULL)
	{
	  CL_CleanCurrentGarbageFrame (theEnv, &evalResult);
	}
      else
	{
	  CL_CleanCurrentGarbageFrame (theEnv, NULL);
	}
      CL_CallPeriodicTasks (theEnv);
    }

  if (returnValue != NULL)
    {
      returnValue->value = evalResult.value;
    }

  if (Get_EvaluationError (theEnv))
    return EE_PROCESSING_ERROR;

  return EE_NO_ERROR;
}

#if (! RUN_TIME) && (! BLOAD_ONLY)
/***************************************/
/* CL_BuildFunction: H/L access routine   */
/*   for the build function.           */
/***************************************/
void
CL_BuildFunction (Environment * theEnv,
		  UDFContext * context, UDFValue * returnValue)
{
  UDFValue theArg;
  CL_BuildError rv;

   /*==================================================*/
  /* The argument should be of type SYMBOL or STRING. */
   /*==================================================*/

  if (!CL_UDFFirstArgument (context, LEXEME_BITS, &theArg))
    {
      return;
    }

   /*======================*/
  /* CL_Build the construct. */
   /*======================*/

  rv = CL_Build (theEnv, theArg.lexemeValue->contents);
  returnValue->lexemeValue = CL_CreateBoolean (theEnv, (rv == BE_NO_ERROR));
}

/*****************************/
/* CL_Build: C access routine   */
/*   for the build function. */
/*****************************/
CL_BuildError
CL_Build (Environment * theEnv, const char *theString)
{
  const char *constructType;
  struct token theToken;
  CL_BuildError errorFlag;
  GCBlock gcb;

   /*=====================================*/
  /* If embedded, clear the error flags. */
   /*=====================================*/

  if (CL_EvaluationData (theEnv)->CurrentExpression == NULL)
    {
      CL_ResetErrorFlags (theEnv);
    }

   /*====================================================*/
  /* No additions during defrule join network activity. */
   /*====================================================*/

#if DEFRULE_CONSTRUCT
  if (EngineData (theEnv)->JoinOperationInProgress)
    return BE_COULD_NOT_BUILD_ERROR;
#endif

   /*===========================================*/
  /* Create a string source router so that the */
  /* string can be used as an input source.    */
   /*===========================================*/

  if (CL_OpenStringSource (theEnv, "build", theString, 0) == 0)
    {
      return BE_COULD_NOT_BUILD_ERROR;
    }

   /*===================================*/
  /* Start a garbage collection block. */
   /*===================================*/

  CL_GCBlockStart (theEnv, &gcb);

   /*================================*/
  /* The first token of a construct */
  /* must be a left parenthesis.    */
   /*================================*/

  CL_GetToken (theEnv, "build", &theToken);

  if (theToken.tknType != LEFT_PARENTHESIS_TOKEN)
    {
      CL_CloseStringSource (theEnv, "build");
      CL_GCBlockEnd (theEnv, &gcb);
      return BE_PARSING_ERROR;
    }

   /*==============================================*/
  /* The next token should be the construct type. */
   /*==============================================*/

  CL_GetToken (theEnv, "build", &theToken);
  if (theToken.tknType != SYMBOL_TOKEN)
    {
      CL_CloseStringSource (theEnv, "build");
      CL_GCBlockEnd (theEnv, &gcb);
      return BE_PARSING_ERROR;
    }

  constructType = theToken.lexemeValue->contents;

   /*======================*/
  /* Parse the construct. */
   /*======================*/

  errorFlag = CL_ParseConstruct (theEnv, constructType, "build");

   /*=============================*/
  /* Grab any extraneous token. */
   /*============================*/

  CL_GetToken (theEnv, "build", &theToken);

   /*=================================*/
  /* Close the string source router. */
   /*=================================*/

  CL_CloseStringSource (theEnv, "build");

   /*=========================================*/
  /* If an error occured while parsing the   */
  /* construct, then print an error message. */
   /*=========================================*/

  if (errorFlag == BE_PARSING_ERROR)
    {
      CL_WriteString (theEnv, STDERR, "\nERROR:\n");
      CL_WriteString (theEnv, STDERR, CL_GetPPBuffer (theEnv));
      CL_WriteString (theEnv, STDERR, "\n");
    }

  CL_DestroyPPBuffer (theEnv);

   /*===================================*/
  /* End the garbage collection block. */
   /*===================================*/

  CL_GCBlockEnd (theEnv, &gcb);

   /*===================================*/
  /* Throw error for extraneous input. */
   /*===================================*/

  if ((errorFlag == BE_NO_ERROR) && (theToken.tknType != STOP_TOKEN))
    {
      CL_PrintErrorID (theEnv, "STRNGFUN", 2, false);
      CL_WriteString (theEnv, STDERR,
		      "Function 'build' encountered extraneous input.\n");
      Set_EvaluationError (theEnv, true);
      errorFlag = BE_PARSING_ERROR;
    }

   /*===================================================*/
  /* Return the error code from parsing the construct. */
   /*===================================================*/

  return errorFlag;
}
#else
/**************************************************/
/* CL_BuildFunction: This is the non-functional stub */
/*   provided for use with a run-time version.    */
/**************************************************/
void
CL_BuildFunction (Environment * theEnv,
		  UDFContext * context, UDFValue * returnValue)
{
  CL_PrintErrorID (theEnv, "STRNGFUN", 1, false);
  CL_WriteString (theEnv, STDERR,
		  "Function 'build' does not work in run time modules.\n");
  returnValue->lexemeValue = FalseSymbol (theEnv);
}

/***************************************************/
/* CL_Build: This is the non-functional stub provided */
/*   for use with a run-time version.              */
/***************************************************/
CL_BuildError
CL_Build (Environment * theEnv, const char *theString)
{
  CL_PrintErrorID (theEnv, "STRNGFUN", 1, false);
  CL_WriteString (theEnv, STDERR,
		  "Function 'build' does not work in run time modules.\n");
  return BE_COULD_NOT_BUILD_ERROR;
}
#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */

#endif /* STRING_FUNCTIONS */
