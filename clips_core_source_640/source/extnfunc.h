   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/24/17            */
   /*                                                     */
   /*            EXTERNAL FUNCTIONS HEADER FILE           */
   /*******************************************************/

/*************************************************************/
/* Purpose: Routines for adding new user or system defined   */
/*   functions.                                              */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Added support for passing context info_rmation  */
/*            to user defined functions.                     */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Replaced ALLOW_ENVIRONMENT_GLOBALS macros      */
/*            with functions.                                */
/*                                                           */
/*      6.40: Changed restrictions from char * to            */
/*            CLIPSLexeme * to support strings               */
/*            originating from sources that are not          */
/*            statically allocated.                          */
/*                                                           */
/*            Removed LOCALE definition.                     */
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
/*            Callbacks must be environment aware.           */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#ifndef _H_extnfunc

#pragma once

#define _H_extnfunc

#include "entities.h"

#include "evaluatn.h"
#include "expressn.h"
#include "symbol.h"
#include "userdata.h"

typedef void UserDefinedFunction (Environment *, UDFContext *, UDFValue *);

struct functionDefinition
{
  CLIPSLexeme *callFunctionName;
  const char *actualFunctionName;
  unsigned unknownReturnValueType;
  void (*functionPointer) (Environment *, UDFContext *, UDFValue *);
  struct expr *(*parser) (Environment *, struct expr *, const char *);
  CLIPSLexeme *restrictions;
  unsigned short minArgs;
  unsigned short maxArgs;
  bool overloadable;
  bool sequenceuseok;
  bool neededFunction;
  unsigned long bsaveIndex;
  struct functionDefinition *next;
  struct userData *usrData;
  void *context;
};

#define UnknownFunctionType(target) (((struct functionDefinition *) target)->unknownReturnValueType)
#define ExpressionFunctionPointer(target) ((target)->functionValue->functionPointer)
#define ExpressionFunctionCallName(target) ((target)->functionValue->callFunctionName)
#define ExpressionFunctionRealName(target) ((target)->functionValue->actualFunctionName)
#define ExpressionUnknownFunctionType(target) ((target)->functionValue->unknownReturnValueType)

/*==================*/
/* ENVIRONMENT DATA */
/*==================*/

#define EXTERNAL_FUNCTION_DATA 50

struct externalFunctionData
{
  struct functionDefinition *ListOfFunctions;
  struct FunctionHash **FunctionHashtable;
};

#define ExternalFunctionData(theEnv) ((struct externalFunctionData *) GetEnvironmentData(theEnv,EXTERNAL_FUNCTION_DATA))

typedef enum
{
  AUE_NO_ERROR = 0,
  AUE_MIN_EXCEEDS_MAX_ERROR,
  AUE_FUNCTION_NAME_IN_USE_ERROR,
  AUE_INVALID_ARGUMENT_TYPE_ERROR,
  AUE_INVALID_RETURN_TYPE_ERROR
} CL_AddUDFError;

struct FunctionHash
{
  struct functionDefinition *fdPtr;
  struct FunctionHash *next;
};

#define SIZE_FUNCTION_HASH 517

void CL_InitializeExternalFunctionData (Environment *);
CL_AddUDFError CL_AddUDF (Environment *, const char *, const char *,
			  unsigned short, unsigned short, const char *,
			  UserDefinedFunction *, const char *, void *);
bool CL_AddFunctionParser (Environment *, const char *,
			   struct expr *(*)(Environment *, struct expr *,
					    const char *));
bool CL_RemoveFunctionParser (Environment *, const char *);
bool CL_FuncSeqOvlFlags (Environment *, const char *, bool, bool);
struct functionDefinition *CL_GetFunctionList (Environment *);
void CL_InstallFunctionList (Environment *, struct functionDefinition *);
struct functionDefinition *CL_FindFunction (Environment *, const char *);
unsigned CL_GetNthRestriction (Environment *, struct functionDefinition *,
			       unsigned int);
bool CL_RemoveUDF (Environment *, const char *);
int CL_GetMinimumArgs (struct functionDefinition *);
int CL_GetMaximumArgs (struct functionDefinition *);
unsigned int CL_UDFArgumentCount (UDFContext *);
bool CL_UDFNthArgument (UDFContext *, unsigned int, unsigned, UDFValue *);
void CL_UDFInvalidArgumentMessage (UDFContext *, const char *);
const char *CL_UDFContextFunctionName (UDFContext *);
void CL_PrintTypesString (Environment *, const char *, unsigned, bool);
bool CL_UDFFirstArgument (UDFContext *, unsigned, UDFValue *);
bool CL_UDFNextArgument (UDFContext *, unsigned, UDFValue *);
void CL_UDFThrowError (UDFContext *);
void *CL_GetUDFContext (Environment *, const char *);

#define UDFHasNextArgument(context) (context->lastArg != NULL)

#endif /* _H_extnfunc */
