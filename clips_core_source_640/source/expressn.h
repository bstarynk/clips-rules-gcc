   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/01/16            */
   /*                                                     */
   /*               EXPRESSION HEADER FILE                */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains routines for creating, deleting,        */
/*   compacting, installing, and hashing expressions.        */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Changed expression hashing value.              */
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
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*************************************************************/

#ifndef _H_expressn

#pragma once

#define _H_expressn

struct exprHashNode;
typedef struct savedContexts CL_SavedContexts;

#include "entities.h"
#include "exprnops.h"
#include "constrct.h"

/******************************/
/* Expression Data Structures */
/******************************/

struct expr
{
  unsigned short type;
  union
  {
    void *value;
    CLIPSLexeme *lexemeValue;
    CLIPSFloat *floatValue;
    CLIPSInteger *integerValue;
    CLIPSBitMap *bitMapValue;
    ConstructHeader *constructValue;
    FunctionDefinition *functionValue;
  };
  Expression *argList;
  Expression *nextArg;
};

#define arg_list argList
#define next_arg nextArg

typedef struct exprHashNode
{
  unsigned hashval;
  unsigned count;
  Expression *exp;
  struct exprHashNode *next;
  unsigned long bsaveID;
} EXPRESSION_HN;

struct savedContexts
{
  bool rtn;
  bool brk;
  struct savedContexts *nxt;
};

#define EXPRESSION_HASH_SIZE 503

/********************/
/* ENVIRONMENT DATA */
/********************/

#define EXPRESSION_DATA 45

struct expressionData
{
  FunctionDefinition *PTR_AND;
  FunctionDefinition *PTR_OR;
  FunctionDefinition *PTR_EQ;
  FunctionDefinition *PTR_NEQ;
  FunctionDefinition *PTR_NOT;
  EXPRESSION_HN **ExpressionHashTable;
#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)
  unsigned long NumberOfExpressions;
  Expression *ExpressionArray;
  unsigned long ExpressionCount;
#endif
  CL_SavedContexts *svContexts;
  bool ReturnContext;
  bool BreakContext;
  bool SequenceOpMode;
};

#define ExpressionData(theEnv) ((struct expressionData *) GetEnvironmentData(theEnv,EXPRESSION_DATA))

/********************/
/* Global Functions */
/********************/

void CL_ReturnExpression (Environment *, Expression *);
void CL_ExpressionInstall (Environment *, Expression *);
void CL_ExpressionDeinstall (Environment *, Expression *);
Expression *CL_PackExpression (Environment *, Expression *);
void CL_ReturnPackedExpression (Environment *, Expression *);
void CL_InitExpressionData (Environment *);
void CL_InitExpressionPointers (Environment *);
bool CL_SetSequenceOperatorRecognition (Environment *, bool);
bool CL_GetSequenceOperatorRecognition (Environment *);
#if (! BLOAD_ONLY) && (! RUN_TIME)
Expression *CL_AddHashedExpression (Environment *, Expression *);
#endif
void CL_RemoveHashedExpression (Environment *, Expression *);
#if BLOAD_AND_BSAVE || BLOAD_ONLY || BLOAD || CONSTRUCT_COMPILER
unsigned long CL_HashedExpressionIndex (Environment *, Expression *);
#endif

#endif
