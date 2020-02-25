   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/17/17            */
   /*                                                     */
   /*               EVALUATION HEADER FILE                */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides routines for evaluating expressions.    */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added CL_EvaluateAndStoreInDataObject function.   */
/*                                                           */
/*      6.30: Added support for passing context info_rmation  */
/*            to user defined functions.                     */
/*                                                           */
/*            Added support for external address hash table  */
/*            and subtyping.                                 */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Support for DATA_OBJECT_ARRAY primitive.       */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.40: Added Env prefix to Get_EvaluationError and     */
/*            Set_EvaluationError functions.                  */
/*                                                           */
/*            Added Env prefix to CL_Get_HaltExecution and       */
/*            Set_HaltExecution functions.                    */
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
/*            UDF redesign.                                  */
/*                                                           */
/*            Removed DATA_OBJECT_ARRAY primitive type.      */
/*                                                           */
/*************************************************************/

#ifndef _H_evaluatn

#pragma once

#define _H_evaluatn

#include "constant.h"
#include "entities.h"

typedef struct functionCall_Builder FunctionCall_Builder;

struct functionCall_Builder
  {
   Environment *fcbEnv;
   CLIPSValue *contents;
   size_t buffer_Reset;
   size_t length;
   size_t bufferMaximum;
  };

typedef enum
  {
   FCBE_NO_ERROR = 0,
   FCBE_NULL_POINTER_ERROR,
   FCBE_FUNCTION_NOT_FOUND_ERROR,
   FCBE_INVALID_FUNCTION_ERROR,
   FCBE_ARGUMENT_COUNT_ERROR,
   FCBE_ARGUMENT_TYPE_ERROR,
   FCBE_PROCESSING_ERROR
  } FunctionCall_BuilderError;

#define PARAMETERS_UNBOUNDED USHRT_MAX

#define C_POINTER_EXTERNAL_ADDRESS 0

struct externalAddressType
  {
   const  char *name;
   void (*short_PrintFunction)(Environment *,const char *,void *);
   void (*long_PrintFunction)(Environment *,const char *,void *);
   bool (*discardFunction)(Environment *,void *);
   void (*newFunction)(UDFContext *,UDFValue *);
   bool (*callFunction)(UDFContext *,UDFValue *,UDFValue *);
  };

#define CoerceToLongInteger(t,v) ((t == INTEGER_TYPE) ? ValueToLong(v) : (long) ValueToDouble(v))
#define CoerceToInteger(t,v) ((t == INTEGER_TYPE) ? (int) ValueToLong(v) : (int) ValueToDouble(v))
#define CoerceToDouble(t,v) ((t == INTEGER_TYPE) ? (double) ValueToLong(v) : ValueToDouble(v))

#define GetFirstArgument()           (CL_EvaluationData(theEnv)->CurrentExpression->argList)
#define GetNextArgument(ep)          (ep->nextArg)

#define MAXIMUM_PRIMITIVES 150
#define MAXIMUM_EXTERNAL_ADDRESS_TYPES 10

#define BITS_PER_BYTE    8

#define BitwiseTest(n,b)   (((n) & (char) (1 << (b))) ? true : false)
#define BitwiseSet(n,b)    (n |= (char) (1 << (b)))
#define Bitwise_Clear(n,b)  (n &= (char) ~(1 << (b)))

#define CountToBitMapSize(c) (((c) + (BITS_PER_BYTE - 1)) / BITS_PER_BYTE) 
#define TestBitMap(map,id)  BitwiseTest(map[(id) / BITS_PER_BYTE],(id) % BITS_PER_BYTE)
#define SetBitMap(map,id)   BitwiseSet(map[(id) / BITS_PER_BYTE],(id) % BITS_PER_BYTE)
#define CL_ClearBitMap(map,id) Bitwise_Clear(map[(id) / BITS_PER_BYTE],(id) % BITS_PER_BYTE)

#define EVALUATION_DATA 44

struct evaluationData
  {
   struct expr *CurrentExpression;
   bool CL_EvaluationError;
   bool CL_HaltExecution;
   int Current_EvaluationDepth;
   int numberOfAddressTypes;
   struct entityRecord *PrimitivesArray[MAXIMUM_PRIMITIVES];
   struct externalAddressType *ExternalAddressTypes[MAXIMUM_EXTERNAL_ADDRESS_TYPES];
  };

#define CL_EvaluationData(theEnv) ((struct evaluationData *) GetEnvironmentData(theEnv,EVALUATION_DATA))

   void                           Initialize_EvaluationData(Environment *);
   bool                           CL_EvaluateExpression(Environment *,struct expr *,UDFValue *);
   void                           Set_EvaluationError(Environment *,bool);
   bool                           Get_EvaluationError(Environment *);
   void                           Set_HaltExecution(Environment *,bool);
   bool                           CL_Get_HaltExecution(Environment *);
   void                           CL_ReturnValues(Environment *,UDFValue *,bool);
   void                           CL_WriteUDFValue(Environment *,const char *,UDFValue *);
   void                           CL_WriteCLIPSValue(Environment *,const char *,CLIPSValue *);
   void                           CL_SetMultifieldErrorValue(Environment *,UDFValue *);
   void                           CL_CopyDataObject(Environment *,UDFValue *,UDFValue *,int);
   void                           CL_AtomInstall(Environment *,unsigned short,void *);
   void                           CL_AtomDeinstall(Environment *,unsigned short,void *);
   void                           CL_Retain(Environment *,TypeHeader *);
   void                           CL_Release(Environment *,TypeHeader *);
   void                           CL_RetainCV(Environment *,CLIPSValue *);
   void                           CL_ReleaseCV(Environment *,CLIPSValue *);
   void                           CL_RetainUDFV(Environment *,UDFValue *);
   void                           CL_ReleaseUDFV(Environment *,UDFValue *);
   struct expr                   *CL_ConvertValueToExpression(Environment *,UDFValue *);
   unsigned long                  CL_GetAtomicHashValue(unsigned short,void *,unsigned short);
   void                           CL_InstallPrimitive(Environment *,struct entityRecord *,int);
   int                            CL_InstallExternalAddressType(Environment *,struct externalAddressType *);
   void                           CL_TransferDataObjectValues(UDFValue *,UDFValue *);
   struct expr                   *CL_FunctionReferenceExpression(Environment *,const char *);
   bool                           CL_GetFunctionReference(Environment *,const char *,Expression *);
   bool                           CL_DOsEqual(UDFValue *,UDFValue *);
   bool                           CL_EvaluateAndStoreInDataObject(Environment *,bool,Expression *,UDFValue *,bool);
   void                           CL_ResetErrorFlags(Environment *);
   FunctionCall_Builder           *CL_CreateFunctionCall_Builder(Environment *,size_t);
   void                           CL_FCBAppendUDFValue(FunctionCall_Builder *,UDFValue *);
   void                           CL_FCBAppend(FunctionCall_Builder *,CLIPSValue *);
   void                           CL_FCBAppendCLIPSInteger(FunctionCall_Builder *,CLIPSInteger *);
   void                           CL_FCBAppendInteger(FunctionCall_Builder *,long long);
   void                           CL_FCBAppendCLIPSFloat(FunctionCall_Builder *,CLIPSFloat *);
   void                           CL_FCBAppendFloat(FunctionCall_Builder *,double);
   void                           CL_FCBAppendCLIPSLexeme(FunctionCall_Builder *,CLIPSLexeme *);
   void                           CL_FCBAppendSymbol(FunctionCall_Builder *,const char *);
   void                           CL_FCBAppendString(FunctionCall_Builder *,const char *);
   void                           CL_FCBAppend_InstanceName(FunctionCall_Builder *,const char *);
   void                           CL_FCBAppendCLIPSExternalAddress(FunctionCall_Builder *,CLIPSExternalAddress *);
   void                           CL_FCBAppendFact(FunctionCall_Builder *,Fact *);
   void                           CL_FCBAppendInstance(FunctionCall_Builder *,Instance *);
   void                           CL_FCBAppendMultifield(FunctionCall_Builder *,Multifield *);
   void                           CL_FCBDispose(FunctionCall_Builder *);
   void                           CL_FCB_Reset(FunctionCall_Builder *);
   FunctionCall_BuilderError       CL_FCBCall(FunctionCall_Builder *,const char *,CLIPSValue *);
   
#define CVIsType(cv,cvType) (((1 << (((TypeHeader *) (cv)->value)->type)) & (cvType)) ? true : false)

#define ValueIsType(value,vType) ((1 << (((TypeHeader *) value)->type)) & (vType))

#define CVCoerceToFloat(cv) (((cv)->header->type == FLOAT_TYPE) ? \
                             ((cv)->floatValue->contents) : \
                             ((double) (cv)->integerValue->contents))

#define CVCoerceToInteger(cv) (((cv)->header->type == INTEGER_TYPE) ? \
                               ((cv)->integerValue->contents) : \
                               ((long long) (cv)->floatValue->contents))

#endif /* _H_evaluatn */
