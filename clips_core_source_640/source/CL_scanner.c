   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*                    SCANNER MODULE                   */
   /*******************************************************/

/*************************************************************/
/* Purpose: Routines for scanning lexical tokens from an     */
/*   input source.                                           */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Chris Culbert                                        */
/*      Brian Dantes                                         */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Added CL_SetLineCount function.                   */
/*                                                           */
/*            Added UTF-8 support.                           */
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
/*************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "setup.h"

#include "constant.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "symbol.h"
#include "sysdep.h"
#include "utility.h"

#include "scanner.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static CLIPSLexeme *ScanSymbol (Environment *, const char *, int,
				TokenType *);
static CLIPSLexeme *ScanString (Environment *, const char *);
static void ScanNumber (Environment *, const char *, struct token *);
static void DeallocateScannerData (Environment *);

/************************************************/
/* CL_InitializeScannerData: Allocates environment */
/*    data for scanner routines.                */
/************************************************/
void
CL_InitializeScannerData (Environment * theEnv)
{
  CL_AllocateEnvironmentData (theEnv, SCANNER_DATA,
			      sizeof (struct scannerData),
			      DeallocateScannerData);
}

/**************************************************/
/* DeallocateScannerData: Deallocates environment */
/*    data for scanner routines.                  */
/**************************************************/
static void
DeallocateScannerData (Environment * theEnv)
{
  if (ScannerData (theEnv)->GlobalMax != 0)
    {
      CL_genfree (theEnv, ScannerData (theEnv)->GlobalString,
		  ScannerData (theEnv)->GlobalMax);
    }
}

/***********************************************************************/
/* CL_GetToken: Reads next token from the input stream. The pointer to    */
/*   the token data structure passed as an argument is set to contain  */
/*   the type of token (e.g., symbol, string, integer, etc.), the data */
/*   value for the token (i.e., a symbol table location if it is a     */
/*   symbol or string, an integer table location if it is an integer), */
/*   and the pretty print representation.                              */
/***********************************************************************/
void
CL_GetToken (Environment * theEnv,
	     const char *logicalName, struct token *theToken)
{
  int inchar;
  TokenType type;

   /*=======================================*/
  /* Set Unknown default values for token. */
   /*=======================================*/

  theToken->tknType = UNKNOWN_VALUE_TOKEN;
  theToken->value = NULL;
  theToken->printFo_rm = "unknown";
  ScannerData (theEnv)->GlobalPos = 0;
  ScannerData (theEnv)->GlobalMax = 0;

   /*==============================================*/
  /* Remove all white space before processing the */
  /* CL_GetToken() request.                          */
   /*==============================================*/

  inchar = CL_ReadRouter (theEnv, logicalName);
  while ((inchar == ' ') || (inchar == '\n') || (inchar == '\f') ||
	 (inchar == '\r') || (inchar == ';') || (inchar == '\t'))
    {
      /*=======================*/
      /* Remove comment lines. */
      /*=======================*/

      if (inchar == ';')
	{
	  inchar = CL_ReadRouter (theEnv, logicalName);
	  while ((inchar != '\n') && (inchar != '\r') && (inchar != EOF))
	    {
	      inchar = CL_ReadRouter (theEnv, logicalName);
	    }
	}
      inchar = CL_ReadRouter (theEnv, logicalName);
    }

   /*==========================*/
  /* Process Symbolic Tokens. */
   /*==========================*/

  if (isalpha (inchar) || IsUTF8MultiByteStart (inchar))
    {
      theToken->tknType = SYMBOL_TOKEN;
      CL_UnreadRouter (theEnv, logicalName, inchar);
      theToken->lexemeValue = ScanSymbol (theEnv, logicalName, 0, &type);
      theToken->printFo_rm = theToken->lexemeValue->contents;
    }

   /*===============================================*/
  /* Process Number Tokens beginning with a digit. */
   /*===============================================*/

  else if (isdigit (inchar))
    {
      CL_UnreadRouter (theEnv, logicalName, inchar);
      ScanNumber (theEnv, logicalName, theToken);
    }

  else
    switch (inchar)
      {
      /*========================*/
	/* Process String Tokens. */
      /*========================*/

      case '"':
	theToken->lexemeValue = ScanString (theEnv, logicalName);
	theToken->tknType = STRING_TOKEN;
	theToken->printFo_rm =
	  StringPrintFo_rm (theEnv, theToken->lexemeValue->contents);
	break;

      /*=======================================*/
	/* Process Tokens that might be numbers. */
      /*=======================================*/

      case '-':
      case '.':
      case '+':
	CL_UnreadRouter (theEnv, logicalName, inchar);
	ScanNumber (theEnv, logicalName, theToken);
	break;

      /*===================================*/
	/* Process ? and ?<variable> Tokens. */
      /*===================================*/

      case '?':
	inchar = CL_ReadRouter (theEnv, logicalName);
	if (isalpha (inchar) || IsUTF8MultiByteStart (inchar)
#if DEFGLOBAL_CONSTRUCT
	    || (inchar == '*'))
#else
	  )
#endif
	{
	  CL_UnreadRouter (theEnv, logicalName, inchar);
	  theToken->lexemeValue = ScanSymbol (theEnv, logicalName, 0, &type);
	  theToken->tknType = SF_VARIABLE_TOKEN;
#if DEFGLOBAL_CONSTRUCT
	  if ((theToken->lexemeValue->contents[0] == '*') &&
	      ((strlen (theToken->lexemeValue->contents)) > 1) &&
	      (theToken->lexemeValue->
	       contents[strlen (theToken->lexemeValue->contents) - 1] == '*'))
	    {
	      size_t count;

	      theToken->tknType = GBL_VARIABLE_TOKEN;
	      theToken->printFo_rm =
		CL_AppendStrings (theEnv, "?",
				  theToken->lexemeValue->contents);
	      count = strlen (ScannerData (theEnv)->GlobalString);
	      ScannerData (theEnv)->GlobalString[count - 1] = EOS;
	      theToken->lexemeValue =
		CL_CreateSymbol (theEnv,
				 ScannerData (theEnv)->GlobalString + 1);
	      ScannerData (theEnv)->GlobalString[count - 1] = (char) inchar;

	    }
	  else
#endif
	    theToken->printFo_rm =
	      CL_AppendStrings (theEnv, "?", theToken->lexemeValue->contents);
	}
	else
	{
	  theToken->tknType = SF_WILDCARD_TOKEN;
	  theToken->lexemeValue = CL_CreateSymbol (theEnv, "?");
	  CL_UnreadRouter (theEnv, logicalName, inchar);
	  theToken->printFo_rm = "?";
	}
	break;

      /*=====================================*/
	/* Process $? and $?<variable> Tokens. */
      /*=====================================*/

      case '$':
	if ((inchar = CL_ReadRouter (theEnv, logicalName)) == '?')
	  {
	    inchar = CL_ReadRouter (theEnv, logicalName);
	    if (isalpha (inchar) || IsUTF8MultiByteStart (inchar)
#if DEFGLOBAL_CONSTRUCT
		|| (inchar == '*'))
#else
	      )
#endif
	    {
	      CL_UnreadRouter (theEnv, logicalName, inchar);
	      theToken->lexemeValue =
		ScanSymbol (theEnv, logicalName, 0, &type);
	      theToken->tknType = MF_VARIABLE_TOKEN;
#if DEFGLOBAL_CONSTRUCT
	      if ((theToken->lexemeValue->contents[0] == '*') &&
		  (strlen (theToken->lexemeValue->contents) > 1) &&
		  (theToken->lexemeValue->
		   contents[strlen (theToken->lexemeValue->contents) - 1] ==
		   '*'))
		{
		  size_t count;

		  theToken->tknType = MF_GBL_VARIABLE_TOKEN;
		  theToken->printFo_rm =
		    CL_AppendStrings (theEnv, "$?",
				      theToken->lexemeValue->contents);
		  count = strlen (ScannerData (theEnv)->GlobalString);
		  ScannerData (theEnv)->GlobalString[count - 1] = EOS;
		  theToken->lexemeValue =
		    CL_CreateSymbol (theEnv,
				     ScannerData (theEnv)->GlobalString + 1);
		  ScannerData (theEnv)->GlobalString[count - 1] =
		    (char) inchar;
		}
	      else
#endif
		theToken->printFo_rm =
		  CL_AppendStrings (theEnv, "$?",
				    theToken->lexemeValue->contents);
	    }
	    else
	    {
	      theToken->tknType = MF_WILDCARD_TOKEN;
	      theToken->lexemeValue = CL_CreateSymbol (theEnv, "$?");
	      theToken->printFo_rm = "$?";
	      CL_UnreadRouter (theEnv, logicalName, inchar);
	    }
	  }
	else
	  {
	    theToken->tknType = SYMBOL_TOKEN;
	    ScannerData (theEnv)->GlobalString =
	      CL_ExpandStringWithChar (theEnv, '$',
				       ScannerData (theEnv)->GlobalString,
				       &ScannerData (theEnv)->GlobalPos,
				       &ScannerData (theEnv)->GlobalMax,
				       ScannerData (theEnv)->GlobalMax + 80);
	    CL_UnreadRouter (theEnv, logicalName, inchar);
	    theToken->lexemeValue =
	      ScanSymbol (theEnv, logicalName, 1, &type);
	    theToken->printFo_rm = theToken->lexemeValue->contents;
	  }
	break;

      /*============================*/
	/* Symbols beginning with '<' */
      /*============================*/

      case '<':
	theToken->tknType = SYMBOL_TOKEN;
	ScannerData (theEnv)->GlobalString =
	  CL_ExpandStringWithChar (theEnv, '<',
				   ScannerData (theEnv)->GlobalString,
				   &ScannerData (theEnv)->GlobalPos,
				   &ScannerData (theEnv)->GlobalMax,
				   ScannerData (theEnv)->GlobalMax + 80);
	theToken->lexemeValue = ScanSymbol (theEnv, logicalName, 1, &type);
	theToken->printFo_rm = theToken->lexemeValue->contents;
	break;

      /*=============================================*/
	/* Process "(", ")", "~", "|", and "&" Tokens. */
      /*=============================================*/

      case '(':
	theToken->tknType = LEFT_PARENTHESIS_TOKEN;
	theToken->lexemeValue = CL_CreateString (theEnv, "(");
	theToken->printFo_rm = "(";
	break;

      case ')':
	theToken->tknType = RIGHT_PARENTHESIS_TOKEN;
	theToken->lexemeValue = CL_CreateString (theEnv, ")");
	theToken->printFo_rm = ")";
	break;

      case '~':
	theToken->tknType = NOT_CONSTRAINT_TOKEN;
	theToken->lexemeValue = CL_CreateString (theEnv, "~");
	theToken->printFo_rm = "~";
	break;

      case '|':
	theToken->tknType = OR_CONSTRAINT_TOKEN;
	theToken->lexemeValue = CL_CreateString (theEnv, "|");
	theToken->printFo_rm = "|";
	break;

      case '&':
	theToken->tknType = AND_CONSTRAINT_TOKEN;
	theToken->lexemeValue = CL_CreateString (theEnv, "&");
	theToken->printFo_rm = "&";
	break;

      /*============================*/
	/* Process End-of-File Token. */
      /*============================*/

      case EOF:
      case 0:
      case 3:
	theToken->tknType = STOP_TOKEN;
	theToken->lexemeValue = CL_CreateSymbol (theEnv, "stop");
	theToken->printFo_rm = "";
	break;

      /*=======================*/
	/* Process Other Tokens. */
      /*=======================*/

      default:
	if (isprint (inchar))
	  {
	    CL_UnreadRouter (theEnv, logicalName, inchar);
	    theToken->lexemeValue =
	      ScanSymbol (theEnv, logicalName, 0, &type);
	    theToken->tknType = type;
	    theToken->printFo_rm = theToken->lexemeValue->contents;
	  }
	else
	  {
	    theToken->printFo_rm = "<<<unprintable character>>>";
	  }
	break;
      }

   /*===============================================*/
  /* Put the new token in the pretty print buffer. */
   /*===============================================*/

#if (! RUN_TIME) && (! BLOAD_ONLY)
  if (theToken->tknType == INSTANCE_NAME_TOKEN)
    {
      CL_SavePPBuffer (theEnv, "[");
      CL_SavePPBuffer (theEnv, theToken->printFo_rm);
      CL_SavePPBuffer (theEnv, "]");
    }
  else
    {
      CL_SavePPBuffer (theEnv, theToken->printFo_rm);
    }
#endif

   /*=========================================================*/
  /* Return the temporary memory used in scanning the token. */
   /*=========================================================*/

  if (ScannerData (theEnv)->GlobalString != NULL)
    {
      CL_rm (theEnv, ScannerData (theEnv)->GlobalString,
	     ScannerData (theEnv)->GlobalMax);
      ScannerData (theEnv)->GlobalString = NULL;
      ScannerData (theEnv)->GlobalMax = 0;
      ScannerData (theEnv)->GlobalPos = 0;
    }

  return;
}

/*************************************/
/* ScanSymbol: Scans a symbol token. */
/*************************************/
static CLIPSLexeme *
ScanSymbol (Environment * theEnv,
	    const char *logicalName, int count, TokenType * type)
{
  int inchar;
#if OBJECT_SYSTEM
  CLIPSLexeme *symbol;
#endif

   /*=====================================*/
  /* Scan characters and add them to the */
  /* symbol until a delimiter is found.  */
   /*=====================================*/

  inchar = CL_ReadRouter (theEnv, logicalName);
  while ((inchar != '<') && (inchar != '"') &&
	 (inchar != '(') && (inchar != ')') &&
	 (inchar != '&') && (inchar != '|') && (inchar != '~') &&
	 (inchar != ' ') && (inchar != ';') &&
	 (IsUTF8MultiByteStart (inchar) ||
	  IsUTF8MultiByteContinuation (inchar) || isprint (inchar)))
    {
      ScannerData (theEnv)->GlobalString =
	CL_ExpandStringWithChar (theEnv, inchar,
				 ScannerData (theEnv)->GlobalString,
				 &ScannerData (theEnv)->GlobalPos,
				 &ScannerData (theEnv)->GlobalMax,
				 ScannerData (theEnv)->GlobalMax + 80);

      count++;
      inchar = CL_ReadRouter (theEnv, logicalName);
    }

   /*===================================================*/
  /* Return the last character scanned (the delimiter) */
  /* to the input stream so it will be scanned as part */
  /* of the next token.                                */
   /*===================================================*/

  CL_UnreadRouter (theEnv, logicalName, inchar);

   /*====================================================*/
  /* Add the symbol to the symbol table and return the  */
  /* symbol table address of the symbol. Symbols of the */
  /* fo_rm [<symbol>] are instance names, so the type    */
  /* returned is INSTANCE_NAME_TYPE rather than SYMBOL_TYPE.      */
   /*====================================================*/

#if OBJECT_SYSTEM
  if (count > 2)
    {
      if ((ScannerData (theEnv)->GlobalString[0] ==
	   '[') ? (ScannerData (theEnv)->GlobalString[count - 1] ==
		   ']') : false)
	{
	  *type = INSTANCE_NAME_TOKEN;
	  inchar = ']';
	}
      else
	{
	  *type = SYMBOL_TOKEN;
	  return CL_CreateSymbol (theEnv, ScannerData (theEnv)->GlobalString);
	}
      ScannerData (theEnv)->GlobalString[count - 1] = EOS;
      symbol =
	CL_Create_InstanceName (theEnv,
				ScannerData (theEnv)->GlobalString + 1);
      ScannerData (theEnv)->GlobalString[count - 1] = (char) inchar;
      return symbol;
    }
  else
    {
      *type = SYMBOL_TOKEN;
      return CL_CreateSymbol (theEnv, ScannerData (theEnv)->GlobalString);
    }
#else
  *type = SYMBOL_TOKEN;
  return CL_CreateSymbol (theEnv, ScannerData (theEnv)->GlobalString);
#endif
}

/*************************************/
/* ScanString: Scans a string token. */
/*************************************/
static CLIPSLexeme *
ScanString (Environment * theEnv, const char *logicalName)
{
  int inchar;
  size_t pos = 0;
  size_t max = 0;
  char *theString = NULL;
  CLIPSLexeme *thePtr;

   /*============================================*/
  /* Scan characters and add them to the string */
  /* until the " delimiter is found.            */
   /*============================================*/

  inchar = CL_ReadRouter (theEnv, logicalName);
  while ((inchar != '"') && (inchar != EOF))
    {
      if (inchar == '\\')
	{
	  inchar = CL_ReadRouter (theEnv, logicalName);
	}

      theString =
	CL_ExpandStringWithChar (theEnv, inchar, theString, &pos, &max,
				 max + 80);
      inchar = CL_ReadRouter (theEnv, logicalName);
    }

  if ((inchar == EOF)
      && (ScannerData (theEnv)->IgnoreCompletionErrors == false))
    {
      CL_PrintErrorID (theEnv, "SCANNER", 1, true);
      CL_WriteString (theEnv, STDERR,
		      "Encountered End-Of-File while scanning a string\n");
    }

   /*===============================================*/
  /* Add the string to the symbol table and return */
  /* the symbol table address of the string.       */
   /*===============================================*/

  if (theString == NULL)
    {
      thePtr = CL_CreateString (theEnv, "");
    }
  else
    {
      thePtr = CL_CreateString (theEnv, theString);
      CL_rm (theEnv, theString, max);
    }

  return thePtr;
}

/**************************************/
/* ScanNumber: Scans a numeric token. */
/**************************************/
static void
ScanNumber (Environment * theEnv,
	    const char *logicalName, struct token *theToken)
{
  int count = 0;
  int inchar, phase;
  bool digitFound = false;
  bool processFloat = false;
  double fvalue;
  long long lvalue;
  TokenType type;

  /* Phases:              */
  /*  -1 = sign           */
  /*   0 = integral       */
  /*   1 = decimal        */
  /*   2 = exponent-begin */
  /*   3 = exponent-value */
  /*   5 = done           */
  /*   9 = error          */

  inchar = CL_ReadRouter (theEnv, logicalName);
  phase = -1;

  while ((phase != 5) && (phase != 9))
    {
      if (phase == -1)
	{
	  if (isdigit (inchar))
	    {
	      phase = 0;
	      digitFound = true;
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	    }
	  else if ((inchar == '+') || (inchar == '-'))
	    {
	      phase = 0;
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	    }
	  else if (inchar == '.')
	    {
	      processFloat = true;
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	      phase = 1;
	    }
	  else if ((inchar == 'E') || (inchar == 'e'))
	    {
	      processFloat = true;
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	      phase = 2;
	    }
	  else if ((inchar == '<') || (inchar == '"') ||
		   (inchar == '(') || (inchar == ')') ||
		   (inchar == '&') || (inchar == '|') || (inchar == '~') ||
		   (inchar == ' ') || (inchar == ';') ||
		   (isprint (inchar) == 0))
	    {
	      phase = 5;
	    }
	  else
	    {
	      phase = 9;
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	    }
	}
      else if (phase == 0)
	{
	  if (isdigit (inchar))
	    {
	      digitFound = true;
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	    }
	  else if (inchar == '.')
	    {
	      processFloat = true;
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	      phase = 1;
	    }
	  else if ((inchar == 'E') || (inchar == 'e'))
	    {
	      processFloat = true;
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	      phase = 2;
	    }
	  else if ((inchar == '<') || (inchar == '"') ||
		   (inchar == '(') || (inchar == ')') ||
		   (inchar == '&') || (inchar == '|') || (inchar == '~') ||
		   (inchar == ' ') || (inchar == ';') ||
		   ((!IsUTF8MultiByteStart (inchar)
		     && (isprint (inchar) == 0))))
	    {
	      phase = 5;
	    }
	  else
	    {
	      phase = 9;
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	    }
	}
      else if (phase == 1)
	{
	  if (isdigit (inchar))
	    {
	      digitFound = true;
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	    }
	  else if ((inchar == 'E') || (inchar == 'e'))
	    {
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	      phase = 2;
	    }
	  else if ((inchar == '<') || (inchar == '"') ||
		   (inchar == '(') || (inchar == ')') ||
		   (inchar == '&') || (inchar == '|') || (inchar == '~') ||
		   (inchar == ' ') || (inchar == ';') ||
		   ((!IsUTF8MultiByteStart (inchar))
		    && (isprint (inchar) == 0)))
	    {
	      phase = 5;
	    }
	  else
	    {
	      phase = 9;
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	    }
	}
      else if (phase == 2)
	{
	  if (isdigit (inchar))
	    {
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	      phase = 3;
	    }
	  else if ((inchar == '+') || (inchar == '-'))
	    {
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	      phase = 3;
	    }
	  else if ((inchar == '<') || (inchar == '"') ||
		   (inchar == '(') || (inchar == ')') ||
		   (inchar == '&') || (inchar == '|') || (inchar == '~') ||
		   (inchar == ' ') || (inchar == ';') ||
		   ((!IsUTF8MultiByteStart (inchar))
		    && (isprint (inchar) == 0)))
	    {
	      digitFound = false;
	      phase = 5;
	    }
	  else
	    {
	      phase = 9;
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	    }
	}
      else if (phase == 3)
	{
	  if (isdigit (inchar))
	    {
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	    }
	  else if ((inchar == '<') || (inchar == '"') ||
		   (inchar == '(') || (inchar == ')') ||
		   (inchar == '&') || (inchar == '|') || (inchar == '~') ||
		   (inchar == ' ') || (inchar == ';') ||
		   ((!IsUTF8MultiByteStart (inchar))
		    && (isprint (inchar) == 0)))
	    {
	      if ((ScannerData (theEnv)->GlobalString[count - 1] == '+')
		  || (ScannerData (theEnv)->GlobalString[count - 1] == '-'))
		{
		  digitFound = false;
		}
	      phase = 5;
	    }
	  else
	    {
	      phase = 9;
	      ScannerData (theEnv)->GlobalString =
		CL_ExpandStringWithChar (theEnv, inchar,
					 ScannerData (theEnv)->GlobalString,
					 &ScannerData (theEnv)->GlobalPos,
					 &ScannerData (theEnv)->GlobalMax,
					 ScannerData (theEnv)->GlobalMax +
					 80);
	      count++;
	    }
	}

      if ((phase != 5) && (phase != 9))
	{
	  inchar = CL_ReadRouter (theEnv, logicalName);
	}
    }

  if (phase == 9)
    {
      theToken->lexemeValue = ScanSymbol (theEnv, logicalName, count, &type);
      theToken->tknType = type;
      theToken->printFo_rm = theToken->lexemeValue->contents;
      return;
    }

   /*=======================================*/
  /* Stuff last character back into buffer */
  /* and return the number.                */
   /*=======================================*/

  CL_UnreadRouter (theEnv, logicalName, inchar);

  if (!digitFound)
    {
      theToken->tknType = SYMBOL_TOKEN;
      theToken->lexemeValue =
	CL_CreateSymbol (theEnv, ScannerData (theEnv)->GlobalString);
      theToken->printFo_rm = theToken->lexemeValue->contents;
      return;
    }

  if (processFloat)
    {
      fvalue = atof (ScannerData (theEnv)->GlobalString);
      theToken->tknType = FLOAT_TOKEN;
      theToken->floatValue = CL_CreateFloat (theEnv, fvalue);
      theToken->printFo_rm =
	CL_FloatToString (theEnv, theToken->floatValue->contents);
    }
  else
    {
      errno = 0;
#if WIN_MVC
      lvalue = _strtoi64 (ScannerData (theEnv)->GlobalString, NULL, 10);
#else
      lvalue = strtoll (ScannerData (theEnv)->GlobalString, NULL, 10);
#endif
      if (errno)
	{
	  CL_PrintWarningID (theEnv, "SCANNER", 1, false);
	  CL_WriteString (theEnv, STDWRN,
			  "Over or underflow of long long integer.\n");
	}
      theToken->tknType = INTEGER_TOKEN;
      theToken->integerValue = CL_CreateInteger (theEnv, lvalue);
      theToken->printFo_rm =
	CL_LongIntegerToString (theEnv, theToken->integerValue->contents);
    }

  return;
}

/***********************************************************/
/* CL_CopyToken: Copies values of one token to another token. */
/***********************************************************/
void
CL_CopyToken (struct token *destination, struct token *source)
{
  destination->tknType = source->tknType;
  destination->value = source->value;
  destination->printFo_rm = source->printFo_rm;
}

/****************************************/
/* CL_ResetLineCount: CL_Resets the scanner's */
/*   line count to zero.                */
/****************************************/
void
CL_ResetLineCount (Environment * theEnv)
{
  ScannerData (theEnv)->LineCount = 0;
}

/***************************************************/
/* CL_GetLineCount: Returns the scanner's line count. */
/***************************************************/
long
CL_GetLineCount (Environment * theEnv)
{
  return (ScannerData (theEnv)->LineCount);
}

/***********************************************/
/* CL_SetLineCount: Sets the scanner's line count */
/*   and returns the previous value.           */
/***********************************************/
long
CL_SetLineCount (Environment * theEnv, long value)
{
  long oldValue;

  oldValue = ScannerData (theEnv)->LineCount;

  ScannerData (theEnv)->LineCount = value;

  return (oldValue);
}

/**********************************/
/* CL_IncrementLineCount: Increments */
/*   the scanner's line count.    */
/**********************************/
void
CL_IncrementLineCount (Environment * theEnv)
{
  ScannerData (theEnv)->LineCount++;
}

/**********************************/
/* CL_DecrementLineCount: Decrements */
/*   the scanner's line count.    */
/**********************************/
void
CL_DecrementLineCount (Environment * theEnv)
{
  ScannerData (theEnv)->LineCount--;
}

/********************/
/* CL_TokenTypeToType: */
/********************/
unsigned short
CL_TokenTypeToType (TokenType theType)
{
  switch (theType)
    {
    case FLOAT_TOKEN:
      return FLOAT_TYPE;
    case INTEGER_TOKEN:
      return INTEGER_TYPE;
    case SYMBOL_TOKEN:
      return SYMBOL_TYPE;
    case STRING_TOKEN:
      return STRING_TYPE;
    case INSTANCE_NAME_TOKEN:
      return INSTANCE_NAME_TYPE;
    case SF_VARIABLE_TOKEN:
      return SF_VARIABLE;
    case MF_VARIABLE_TOKEN:
      return MF_VARIABLE;
    case GBL_VARIABLE_TOKEN:
      return GBL_VARIABLE;
    case MF_GBL_VARIABLE_TOKEN:
      return MF_GBL_VARIABLE;
    default:
      return VOID_TYPE;
    }
}
