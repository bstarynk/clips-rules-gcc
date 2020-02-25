   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Message-passing support functions                */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Removed IMPERATIVE_MESSAGE_HANDLERS and        */
/*            AUXILIARY_MESSAGE_HANDLERS compilation flags.  */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: The return value of CL_DirectMessage indicates    */
/*            whether an execution error has occurred.       */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
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
/*************************************************************/

#ifndef _H_msgpass

#pragma once

#define _H_msgpass

#define GetActiveInstance(theEnv) (CL_GetNthMessageArgument(theEnv,0)->instanceValue)

#include "object.h"

typedef struct messageHandlerLink
{
  DefmessageHandler *hnd;
  struct messageHandlerLink *nxt;
  struct messageHandlerLink *nxtInStack;
} HANDLER_LINK;

bool CL_DirectMessage (Environment *, CLIPSLexeme *, Instance *,
		       UDFValue *, Expression *);
void CL_Send (Environment *, CLIPSValue *, const char *, const char *,
	      CLIPSValue *);
void CL_DestroyHandlerLinks (Environment *, HANDLER_LINK *);
void CL_SendCommand (Environment *, UDFContext *, UDFValue *);
UDFValue *CL_GetNthMessageArgument (Environment *, int);

bool CL_NextHandlerAvailable (Environment *);
void CL_NextHandlerAvailableFunction (Environment *, UDFContext *,
				      UDFValue *);
void CL_CallNextHandler (Environment *, UDFContext *, UDFValue *);
void CL_FindApplicableOfName (Environment *, Defclass *, HANDLER_LINK *[],
			      HANDLER_LINK *[], CLIPSLexeme *);
HANDLER_LINK *CL_JoinHandlerLinks (Environment *, HANDLER_LINK *[],
				   HANDLER_LINK *[], CLIPSLexeme *);

void Print_HandlerSlotGetFunction (Environment *, const char *, void *);
bool CL_HandlerSlotGetFunction (Environment *, void *, UDFValue *);
void Print_HandlerSlotPutFunction (Environment *, const char *, void *);
bool CL_HandlerSlotPutFunction (Environment *, void *, UDFValue *);
void CL_DynamicHandlerGetSlot (Environment *, UDFContext *, UDFValue *);
void CL_DynamicHandlerPutSlot (Environment *, UDFContext *, UDFValue *);

#endif /* _H_object */
