   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  12/07/17             */
   /*                                                     */
   /*                  EVALUATION MODULE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides routines for evaluating expressions.    */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added CL_EvaluateAndStoreInDataObject function.   */
/*                                                           */
/*      6.30: Added support for passing context infoCL_rmation  */
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
/*      6.40: Added Env prefix to GetCL_EvaluationError and     */
/*            SetCL_EvaluationError functions.                  */
/*                                                           */
/*            Added Env prefix to CL_GetCL_HaltExecution and       */
/*            SetCL_HaltExecution functions.                    */
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
/*            Removed DATA_OBJECT_ARRAY primitive type.      */
/*                                                           */
/*            Modified CL_GetFunctionReference to handle module */
/*            specifier for funcall.                         */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "setup.h"

#include "argacces.h"
#include "commline.h"
#include "constant.h"
#include "envrnmnt.h"
#include "factmngr.h"
#include "memalloc.h"
#include "modulutl.h"
#include "router.h"
#include "prcdrfun.h"
#include "multifld.h"
#include "prntutil.h"
#include "exprnpsr.h"
#include "utility.h"
#include "proflfun.h"
#include "sysdep.h"

#if DEFFUNCTION_CONSTRUCT
#include "dffnxfun.h"
#endif

#if DEFGENERIC_CONSTRUCT
#include "genrccom.h"
#endif

#if OBJECT_SYSTEM
#include "object.h"
#include "inscom.h"
#endif

#include "evaluatn.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    DeallocateCL_EvaluationData(Environment *);
   static void                    PrintCAddress(Environment *,const char *,void *);
   static void                    NewCAddress(UDFContext *,UDFValue *);
   /*
   static bool                    DiscardCAddress(void *,void *);
   */

/**************************************************/
/* InitializeCL_EvaluationData: Allocates environment */
/*    data for expression evaluation.             */
/**************************************************/
void InitializeCL_EvaluationData(
  Environment *theEnv)
  {
   struct externalAddressType cPointer = { "C", PrintCAddress, PrintCAddress, NULL, NewCAddress, NULL };

   CL_AllocateEnvironmentData(theEnv,EVALUATION_DATA,sizeof(struct evaluationData),DeallocateCL_EvaluationData);

   CL_InstallExternalAddressType(theEnv,&cPointer);
  }

/*****************************************************/
/* DeallocateCL_EvaluationData: Deallocates environment */
/*    data for evaluation data.                      */
/*****************************************************/
static void DeallocateCL_EvaluationData(
  Environment *theEnv)
  {
   int i;

   for (i = 0; i < CL_EvaluationData(theEnv)->numberOfAddressTypes; i++)
     { rtn_struct(theEnv,externalAddressType,CL_EvaluationData(theEnv)->ExternalAddressTypes[i]); }
  }

/**************************************************************/
/* CL_EvaluateExpression: CL_Evaluates an expression. Returns false */
/*   if no errors occurred during evaluation, otherwise true. */
/**************************************************************/
bool CL_EvaluateExpression(
  Environment *theEnv,
  struct expr *problem,
  UDFValue *returnValue)
  {
   struct expr *oldArgument;
   struct functionDefinition *fptr;
   UDFContext theUDFContext;
#if PROFILING_FUNCTIONS
   struct profileFrameInfo profileFrame;
#endif

   returnValue->voidValue = VoidConstant(theEnv);
   returnValue->begin = 0;
   returnValue->range = SIZE_MAX;

   if (problem == NULL)
     {
      returnValue->value = FalseSymbol(theEnv);
      return(CL_EvaluationData(theEnv)->CL_EvaluationError);
     }

   switch (problem->type)
     {
      case STRING_TYPE:
      case SYMBOL_TYPE:
      case FLOAT_TYPE:
      case INTEGER_TYPE:
#if OBJECT_SYSTEM
      case INSTANCE_NAME_TYPE:
      case INSTANCE_ADDRESS_TYPE:
#endif
      case FACT_ADDRESS_TYPE:
      case EXTERNAL_ADDRESS_TYPE:
        returnValue->value = problem->value;
        break;

      case FCALL:
        {
         fptr = problem->functionValue;

#if PROFILING_FUNCTIONS
         StartCL_Profile(theEnv,&profileFrame,
                      &fptr->usrData,
                      CL_ProfileFunctionData(theEnv)->CL_ProfileCL_UserFunctions);
#endif

         oldArgument = CL_EvaluationData(theEnv)->CurrentExpression;
         CL_EvaluationData(theEnv)->CurrentExpression = problem;

         theUDFContext.environment = theEnv;
         theUDFContext.context = fptr->context;
         theUDFContext.theFunction = fptr;
         theUDFContext.lastArg = problem->argList;
         theUDFContext.lastPosition = 1;
         theUDFContext.returnValue = returnValue;
         fptr->functionPointer(theEnv,&theUDFContext,returnValue);
         if ((returnValue->header->type == MULTIFIELD_TYPE) &&
             (returnValue->range == SIZE_MAX))
           { returnValue->range = returnValue->multifieldValue->length; }

#if PROFILING_FUNCTIONS
        CL_EndCL_Profile(theEnv,&profileFrame);
#endif

        CL_EvaluationData(theEnv)->CurrentExpression = oldArgument;
        break;
        }

     case MULTIFIELD_TYPE:
        returnValue->value = ((UDFValue *) (problem->value))->value;
        returnValue->begin = ((UDFValue *) (problem->value))->begin;
        returnValue->range = ((UDFValue *) (problem->value))->range;
        break;

     case MF_VARIABLE:
     case SF_VARIABLE:
        if (CL_GetBoundVariable(theEnv,returnValue,problem->lexemeValue) == false)
          {
           CL_PrintErrorID(theEnv,"EVALUATN",1,false);
           CL_WriteString(theEnv,STDERR,"Variable ");
           if (problem->type == MF_VARIABLE)
             { CL_WriteString(theEnv,STDERR,"$?"); }
           else
             { CL_WriteString(theEnv,STDERR,"?"); }
           CL_WriteString(theEnv,STDERR,problem->lexemeValue->contents);
           CL_WriteString(theEnv,STDERR," is unbound.\n");
           returnValue->value = FalseSymbol(theEnv);
           SetCL_EvaluationError(theEnv,true);
          }
        break;

      default:
        if (CL_EvaluationData(theEnv)->PrimitivesArray[problem->type] == NULL)
          {
           CL_SystemError(theEnv,"EVALUATN",3);
           CL_ExitRouter(theEnv,EXIT_FAILURE);
          }

        if (CL_EvaluationData(theEnv)->PrimitivesArray[problem->type]->copyToCL_Evaluate)
          {
           returnValue->value = problem->value;
           break;
          }

        if (CL_EvaluationData(theEnv)->PrimitivesArray[problem->type]->evaluateFunction == NULL)
          {
           CL_SystemError(theEnv,"EVALUATN",4);
           CL_ExitRouter(theEnv,EXIT_FAILURE);
          }

        oldArgument = CL_EvaluationData(theEnv)->CurrentExpression;
        CL_EvaluationData(theEnv)->CurrentExpression = problem;

#if PROFILING_FUNCTIONS
        StartCL_Profile(theEnv,&profileFrame,
                     &CL_EvaluationData(theEnv)->PrimitivesArray[problem->type]->usrData,
                     CL_ProfileFunctionData(theEnv)->CL_ProfileCL_UserFunctions);
#endif

        (*CL_EvaluationData(theEnv)->PrimitivesArray[problem->type]->evaluateFunction)(theEnv,problem->value,returnValue);

#if PROFILING_FUNCTIONS
        CL_EndCL_Profile(theEnv,&profileFrame);
#endif

        CL_EvaluationData(theEnv)->CurrentExpression = oldArgument;
        break;
     }
     
   return CL_EvaluationData(theEnv)->CL_EvaluationError;
  }

/******************************************/
/* CL_InstallPrimitive: Installs a primitive */
/*   data type in the primitives array.   */
/******************************************/
void CL_InstallPrimitive(
  Environment *theEnv,
  struct entityRecord *thePrimitive,
  int whichPosition)
  {
   if (CL_EvaluationData(theEnv)->PrimitivesArray[whichPosition] != NULL)
     {
      CL_SystemError(theEnv,"EVALUATN",5);
      CL_ExitRouter(theEnv,EXIT_FAILURE);
     }

   CL_EvaluationData(theEnv)->PrimitivesArray[whichPosition] = thePrimitive;
  }

/******************************************************/
/* CL_InstallExternalAddressType: Installs an external   */
/*   address type in the external address type array. */
/******************************************************/
int CL_InstallExternalAddressType(
  Environment *theEnv,
  struct externalAddressType *theAddressType)
  {
   struct externalAddressType *copyEAT;

   int rv = CL_EvaluationData(theEnv)->numberOfAddressTypes;

   if (CL_EvaluationData(theEnv)->numberOfAddressTypes == MAXIMUM_EXTERNAL_ADDRESS_TYPES)
     {
      CL_SystemError(theEnv,"EVALUATN",6);
      CL_ExitRouter(theEnv,EXIT_FAILURE);
     }

   copyEAT = (struct externalAddressType *) CL_genalloc(theEnv,sizeof(struct externalAddressType));
   memcpy(copyEAT,theAddressType,sizeof(struct externalAddressType));
   CL_EvaluationData(theEnv)->ExternalAddressTypes[CL_EvaluationData(theEnv)->numberOfAddressTypes++] = copyEAT;

   return rv;
  }

/*******************/
/* CL_ResetErrorFlags */
/*******************/
void CL_ResetErrorFlags(
  Environment *theEnv)
  {
   CL_EvaluationData(theEnv)->CL_EvaluationError = false;
   CL_EvaluationData(theEnv)->CL_HaltExecution = false;
  }

/******************************************************/
/* SetCL_EvaluationError: Sets the CL_EvaluationError flag. */
/******************************************************/
void SetCL_EvaluationError(
  Environment *theEnv,
  bool value)
  {
   CL_EvaluationData(theEnv)->CL_EvaluationError = value;
   if (value == true)
     { CL_EvaluationData(theEnv)->CL_HaltExecution = true; }
  }

/*********************************************************/
/* GetCL_EvaluationError: Returns the CL_EvaluationError flag. */
/*********************************************************/
bool GetCL_EvaluationError(
  Environment *theEnv)
  {
   return(CL_EvaluationData(theEnv)->CL_EvaluationError);
  }

/**************************************************/
/* SetCL_HaltExecution: Sets the CL_HaltExecution flag. */
/**************************************************/
void SetCL_HaltExecution(
  Environment *theEnv,
  bool value)
  {
   CL_EvaluationData(theEnv)->CL_HaltExecution = value;
  }

/*****************************************************/
/* CL_GetCL_HaltExecution: Returns the CL_HaltExecution flag. */
/*****************************************************/
bool CL_GetCL_HaltExecution(
  Environment *theEnv)
  {
   return(CL_EvaluationData(theEnv)->CL_HaltExecution);
  }

/*****************************************************/
/* CL_ReturnValues: Returns a linked list of UDFValue */
/*   structures to the pool of free memory.          */
/*****************************************************/
void CL_ReturnValues(
  Environment *theEnv,
  UDFValue *garbagePtr,
  bool decrementSupplementalInfo)
  {
   UDFValue *nextPtr;

   while (garbagePtr != NULL)
     {
      nextPtr = garbagePtr->next;
      CL_ReleaseUDFV(theEnv,garbagePtr);
      if ((garbagePtr->supplementalInfo != NULL) && decrementSupplementalInfo)
        { CL_ReleaseLexeme(theEnv,(CLIPSLexeme *) garbagePtr->supplementalInfo); }
      rtn_struct(theEnv,udfValue,garbagePtr);
      garbagePtr = nextPtr;
     }
  }

/**************************************************/
/* CL_WriteCLIPSValue: Prints a CLIPSValue structure */
/*   to the specified logical name.               */
/**************************************************/
void CL_WriteCLIPSValue(
  Environment *theEnv,
  const char *fileid,
  CLIPSValue *argPtr)
  {
   switch(argPtr->header->type)
     {
      case VOID_TYPE:
      case SYMBOL_TYPE:
      case STRING_TYPE:
      case INTEGER_TYPE:
      case FLOAT_TYPE:
      case EXTERNAL_ADDRESS_TYPE:
      case FACT_ADDRESS_TYPE:
#if OBJECT_SYSTEM
      case INSTANCE_NAME_TYPE:
      case INSTANCE_ADDRESS_TYPE:
#endif
        CL_PrintAtom(theEnv,fileid,argPtr->header->type,argPtr->value);
        break;

      case MULTIFIELD_TYPE:
        CL_PrintMultifieldDriver(theEnv,fileid,argPtr->multifieldValue,
                              0,argPtr->multifieldValue->length,true);
        break;

      default:
        CL_WriteString(theEnv,fileid,"<UnknownPrintType");
        CL_WriteInteger(theEnv,fileid,argPtr->header->type);
        CL_WriteString(theEnv,fileid,">");
        SetCL_HaltExecution(theEnv,true);
        SetCL_EvaluationError(theEnv,true);
        break;
     }
  }

/**********************************************/
/* CL_WriteUDFValue: Prints a UDFValue structure */
/*   to the specified logical name.           */
/**********************************************/
void CL_WriteUDFValue(
  Environment *theEnv,
  const char *fileid,
  UDFValue *argPtr)
  {
   switch(argPtr->header->type)
     {
      case VOID_TYPE:
      case SYMBOL_TYPE:
      case STRING_TYPE:
      case INTEGER_TYPE:
      case FLOAT_TYPE:
      case EXTERNAL_ADDRESS_TYPE:
      case FACT_ADDRESS_TYPE:
#if OBJECT_SYSTEM
      case INSTANCE_NAME_TYPE:
      case INSTANCE_ADDRESS_TYPE:
#endif
        CL_PrintAtom(theEnv,fileid,argPtr->header->type,argPtr->value);
        break;

      case MULTIFIELD_TYPE:
        CL_PrintMultifieldDriver(theEnv,fileid,argPtr->multifieldValue,
                              argPtr->begin,argPtr->range,true);
        break;

      default:
        CL_WriteString(theEnv,fileid,"<UnknownPrintType");
        CL_WriteInteger(theEnv,fileid,argPtr->header->type);
        CL_WriteString(theEnv,fileid,">");
        SetCL_HaltExecution(theEnv,true);
        SetCL_EvaluationError(theEnv,true);
        break;
     }
  }

/*************************************************/
/* CL_SetMultifieldErrorValue: Creates a multifield */
/*   value of length zero for error returns.     */
/*************************************************/
void CL_SetMultifieldErrorValue(
  Environment *theEnv,
  UDFValue *returnValue)
  {
   returnValue->value = CL_CreateMultifield(theEnv,0L);
   returnValue->begin = 0;
   returnValue->range = 0;
  }

/***********************************************/
/* CL_RetainUDFV: Increments the appropriate count */
/*   (in use) values for a UDFValue structure. */
/***********************************************/
void CL_RetainUDFV(
  Environment *theEnv,
  UDFValue *vPtr)
  {
   if (vPtr->header->type == MULTIFIELD_TYPE)
     { CL_IncrementCLIPSValueMultifieldReferenceCount(theEnv,vPtr->multifieldValue); }
   else
     { CL_Retain(theEnv,vPtr->header); }
  }

/***********************************************/
/* CL_RetainUDFV: Decrements the appropriate count */
/*   (in use) values for a UDFValue structure. */
/***********************************************/
void CL_ReleaseUDFV(
  Environment *theEnv,
  UDFValue *vPtr)
  {
   if (vPtr->header->type == MULTIFIELD_TYPE)
     { CL_DecrementCLIPSValueMultifieldReferenceCount(theEnv,vPtr->multifieldValue); }
   else
     { CL_Release(theEnv,vPtr->header); }
  }

/*************************************************/
/* CL_RetainCV: Increments the appropriate count    */
/*   (in use) values for a CLIPSValue structure. */
/*************************************************/
void CL_RetainCV(
  Environment *theEnv,
  CLIPSValue *vPtr)
  {
   if (vPtr->header->type == MULTIFIELD_TYPE)
     { CL_IncrementCLIPSValueMultifieldReferenceCount(theEnv,vPtr->multifieldValue); }
   else
     { CL_Retain(theEnv,vPtr->header); }
  }

/*************************************************/
/* CL_ReleaseCV: Decrements the appropriate count   */
/*   (in use) values for a CLIPSValue structure. */
/*************************************************/
void CL_ReleaseCV(
  Environment *theEnv,
  CLIPSValue *vPtr)
  {
   if (vPtr->header->type == MULTIFIELD_TYPE)
     { CL_DecrementCLIPSValueMultifieldReferenceCount(theEnv,vPtr->multifieldValue); }
   else
     { CL_Release(theEnv,vPtr->header); }
  }

/******************************************/
/* CL_Retain: Increments the reference count */
/*   of an atomic data type.              */
/******************************************/
void CL_Retain(
  Environment *theEnv,
  TypeHeader *th)
  {
   switch (th->type)
     {
      case SYMBOL_TYPE:
      case STRING_TYPE:
#if OBJECT_SYSTEM
      case INSTANCE_NAME_TYPE:
#endif
        IncrementLexemeCount(th);
        break;

      case FLOAT_TYPE:
        IncrementFloatCount(th);
        break;

      case INTEGER_TYPE:
        IncrementIntegerCount(th);
        break;

      case EXTERNAL_ADDRESS_TYPE:
        IncrementExternalAddressCount(th);
        break;

      case MULTIFIELD_TYPE:
        CL_RetainMultifield(theEnv,(Multifield *) th);
        break;
        
#if OBJECT_SYSTEM
      case INSTANCE_ADDRESS_TYPE:
        CL_RetainInstance((Instance *) th);
        break;
#endif

#if DEFTEMPLATE_CONSTRUCT
      case FACT_ADDRESS_TYPE:
        CL_RetainFact((Fact *) th);
        break;
#endif
     
      case VOID_TYPE:
        break;

      default:
        CL_SystemError(theEnv,"EVALUATN",7);
        CL_ExitRouter(theEnv,EXIT_FAILURE);
        break;
     }
  }

/*************************************/
/* CL_Release: Decrements the reference */
/*   count of an atomic data type.   */
/*************************************/
void CL_Release(
  Environment *theEnv,
  TypeHeader *th)
  {
   switch (th->type)
     {
      case SYMBOL_TYPE:
      case STRING_TYPE:
#if OBJECT_SYSTEM
      case INSTANCE_NAME_TYPE:
#endif
        CL_ReleaseLexeme(theEnv,(CLIPSLexeme *) th);
        break;

      case FLOAT_TYPE:
        CL_ReleaseFloat(theEnv,(CLIPSFloat *) th);
        break;

      case INTEGER_TYPE:
        CL_ReleaseInteger(theEnv,(CLIPSInteger *) th);
        break;

      case EXTERNAL_ADDRESS_TYPE:
        CL_ReleaseExternalAddress(theEnv,(CLIPSExternalAddress *) th);
        break;

      case MULTIFIELD_TYPE:
        CL_ReleaseMultifield(theEnv,(Multifield *) th);
        break;
        
#if OBJECT_SYSTEM
      case INSTANCE_ADDRESS_TYPE:
        CL_ReleaseInstance((Instance *) th);
        break;
#endif
     
#if DEFTEMPLATE_CONSTRUCT
      case FACT_ADDRESS_TYPE:
        CL_ReleaseFact((Fact *) th);
        break;
#endif

      case VOID_TYPE:
        break;

      default:
        CL_SystemError(theEnv,"EVALUATN",8);
        CL_ExitRouter(theEnv,EXIT_FAILURE);
        break;
     }
  }

/*****************************************/
/* CL_AtomInstall: Increments the reference */
/*   count of an atomic data type.       */
/*****************************************/
void CL_AtomInstall(
  Environment *theEnv,
  unsigned short type,
  void *vPtr)
  {
   switch (type)
     {
      case SYMBOL_TYPE:
      case STRING_TYPE:
#if DEFGLOBAL_CONSTRUCT
      case GBL_VARIABLE:
#endif
#if OBJECT_SYSTEM
      case INSTANCE_NAME_TYPE:
#endif
        IncrementLexemeCount(vPtr);
        break;

      case FLOAT_TYPE:
        IncrementFloatCount(vPtr);
        break;

      case INTEGER_TYPE:
        IncrementIntegerCount(vPtr);
        break;

      case EXTERNAL_ADDRESS_TYPE:
        IncrementExternalAddressCount(vPtr);
        break;

      case MULTIFIELD_TYPE:
        CL_RetainMultifield(theEnv,(Multifield *) vPtr);
        break;

      case VOID_TYPE:
        break;

      default:
        if (CL_EvaluationData(theEnv)->PrimitivesArray[type] == NULL) break;
        if (CL_EvaluationData(theEnv)->PrimitivesArray[type]->bitMap) IncrementBitMapCount(vPtr);
        else if (CL_EvaluationData(theEnv)->PrimitivesArray[type]->incrementBusyCount)
          { (*CL_EvaluationData(theEnv)->PrimitivesArray[type]->incrementBusyCount)(theEnv,vPtr); }
        break;
     }
  }

/*******************************************/
/* CL_AtomDeinstall: Decrements the reference */
/*   count of an atomic data type.         */
/*******************************************/
void CL_AtomDeinstall(
  Environment *theEnv,
  unsigned short type,
  void *vPtr)
  {
   switch (type)
     {
      case SYMBOL_TYPE:
      case STRING_TYPE:
#if DEFGLOBAL_CONSTRUCT
      case GBL_VARIABLE:
#endif
#if OBJECT_SYSTEM
      case INSTANCE_NAME_TYPE:
#endif
        CL_ReleaseLexeme(theEnv,(CLIPSLexeme *) vPtr);
        break;

      case FLOAT_TYPE:
        CL_ReleaseFloat(theEnv,(CLIPSFloat *) vPtr);
        break;

      case INTEGER_TYPE:
        CL_ReleaseInteger(theEnv,(CLIPSInteger *) vPtr);
        break;

      case EXTERNAL_ADDRESS_TYPE:
        CL_ReleaseExternalAddress(theEnv,(CLIPSExternalAddress *) vPtr);
        break;

      case MULTIFIELD_TYPE:
        CL_ReleaseMultifield(theEnv,(Multifield *) vPtr);
        break;

      case VOID_TYPE:
        break;

      default:
        if (CL_EvaluationData(theEnv)->PrimitivesArray[type] == NULL) break;
        if (CL_EvaluationData(theEnv)->PrimitivesArray[type]->bitMap) CL_DecrementBitMapReferenceCount(theEnv,(CLIPSBitMap *) vPtr);
        else if (CL_EvaluationData(theEnv)->PrimitivesArray[type]->decrementBusyCount)
          { (*CL_EvaluationData(theEnv)->PrimitivesArray[type]->decrementBusyCount)(theEnv,vPtr); }
     }
  }

/***************************************************/
/* CL_CopyDataObject: Copies the values from a source */
/*   UDFValue to a destination UDFValue.           */
/***************************************************/
void CL_CopyDataObject(
  Environment *theEnv,
  UDFValue *dst,
  UDFValue *src,
  int garbageMultifield)
  {
   if (src->header->type != MULTIFIELD_TYPE)
     {
      dst->value = src->value;
     }
   else
     {
      CL_DuplicateMultifield(theEnv,dst,src);
      if (garbageMultifield)
        { CL_AddToMultifieldList(theEnv,dst->multifieldValue); }
     }
  }

/***********************************************/
/* CL_TransferDataObjectValues: Copies the values */
/*   directly from a source UDFValue to a    */
/*   destination UDFValue.                   */
/***********************************************/
void CL_TransferDataObjectValues(
  UDFValue *dst,
  UDFValue *src)
  {
   dst->value = src->value;
   dst->begin = src->begin;
   dst->range = src->range;
   dst->supplementalInfo = src->supplementalInfo;
   dst->next = src->next;
  }

/************************************************************************/
/* CL_ConvertValueToExpression: Converts the value stored in a data object */
/*   into an expression. For multifield values, a chain of expressions  */
/*   is generated and the chain is linked by the nextArg field. For a   */
/*   single field value, a single expression is created.                */
/************************************************************************/
struct expr *CL_ConvertValueToExpression(
  Environment *theEnv,
  UDFValue *theValue)
  {
   size_t i;
   struct expr *head = NULL, *last = NULL, *newItem;

   if (theValue->header->type != MULTIFIELD_TYPE)
     { return(CL_GenConstant(theEnv,theValue->header->type,theValue->value)); }

   for (i = theValue->begin; i < (theValue->begin + theValue->range); i++)
     {
      newItem = CL_GenConstant(theEnv,theValue->multifieldValue->contents[i].header->type,
                                   theValue->multifieldValue->contents[i].value);
      if (last == NULL) head = newItem;
      else last->nextArg = newItem;
      last = newItem;
     }

   if (head == NULL)
     return(CL_GenConstant(theEnv,FCALL,CL_FindFunction(theEnv,"create$")));

   return(head);
  }

/****************************************/
/* CL_GetAtomicHashValue: Returns the hash */
/*   value for an atomic data type.     */
/****************************************/
unsigned long CL_GetAtomicHashValue(
  unsigned short type,
  void *value,
  unsigned short position)
  {
   unsigned long tvalue;
   union
     {
      double fv;
      void *vv;
      unsigned long liv;
     } fis;

   switch (type)
     {
      case FLOAT_TYPE:
        fis.liv = 0;
        fis.fv = ((CLIPSFloat *) value)->contents;
        tvalue = fis.liv;
        break;

      case INTEGER_TYPE:
        tvalue = (unsigned long) ((CLIPSInteger *) value)->contents;
        break;

      case EXTERNAL_ADDRESS_TYPE:
         fis.liv = 0;
         fis.vv = ((CLIPSExternalAddress *) value)->contents;
         tvalue = fis.liv;
         break;

      case FACT_ADDRESS_TYPE:
#if OBJECT_SYSTEM
      case INSTANCE_ADDRESS_TYPE:
#endif
         fis.liv = 0;
         fis.vv = value;
         tvalue = fis.liv;
         break;

      case STRING_TYPE:
#if OBJECT_SYSTEM
      case INSTANCE_NAME_TYPE:
#endif
      case SYMBOL_TYPE:
        tvalue = ((CLIPSLexeme *) value)->bucket;
        break;

      default:
        tvalue = type;
     }

   return tvalue * (position + 29);
  }

/***********************************************************/
/* CL_FunctionReferenceExpression: Returns an expression with */
/*   an appropriate expression reference to the specified  */
/*   name if it is the name of a deffunction, defgeneric,  */
/*   or user/system defined function.                      */
/***********************************************************/
struct expr *CL_FunctionReferenceExpression(
  Environment *theEnv,
  const char *name)
  {
#if DEFGENERIC_CONSTRUCT
   Defgeneric *gfunc;
#endif
#if DEFFUNCTION_CONSTRUCT
   Deffunction *dptr;
#endif
   struct functionDefinition *fptr;

   /*=====================================================*/
   /* Check to see if the function call is a deffunction. */
   /*=====================================================*/

#if DEFFUNCTION_CONSTRUCT
   if ((dptr = CL_LookupDeffunctionInScope(theEnv,name)) != NULL)
     { return(CL_GenConstant(theEnv,PCALL,dptr)); }
#endif

   /*====================================================*/
   /* Check to see if the function call is a defgeneric. */
   /*====================================================*/

#if DEFGENERIC_CONSTRUCT
   if ((gfunc = CL_LookupDefgenericInScope(theEnv,name)) != NULL)
     { return(CL_GenConstant(theEnv,GCALL,gfunc)); }
#endif

   /*======================================*/
   /* Check to see if the function call is */
   /* a system or user defined function.   */
   /*======================================*/

   if ((fptr = CL_FindFunction(theEnv,name)) != NULL)
     { return(CL_GenConstant(theEnv,FCALL,fptr)); }

   /*===================================================*/
   /* The specified function name is not a deffunction, */
   /* defgeneric, or user/system defined function.      */
   /*===================================================*/

   return NULL;
  }

/******************************************************************/
/* CL_GetFunctionReference: Fills an expression with an appropriate  */
/*   expression reference to the specified name if it is the      */
/*   name of a deffunction, defgeneric, or user/system defined    */
/*   function.                                                    */
/******************************************************************/
bool CL_GetFunctionReference(
  Environment *theEnv,
  const char *name,
  Expression *theReference)
  {
#if DEFGENERIC_CONSTRUCT
   Defgeneric *gfunc;
#endif
#if DEFFUNCTION_CONSTRUCT
   Deffunction *dptr;
#endif
   struct functionDefinition *fptr;
   bool moduleSpecified = false;
   unsigned position;
   CLIPSLexeme *moduleName = NULL, *constructName = NULL;

   theReference->nextArg = NULL;
   theReference->argList = NULL;
   theReference->type = VOID_TYPE;
   theReference->value = NULL;
   
   /*==============================*/
   /* Look for a module specifier. */
   /*==============================*/

   if ((position = CL_FindModuleSeparator(name)) != 0)
     {
      moduleName = CL_ExtractModuleName(theEnv,position,name);
      constructName = CL_ExtractConstructName(theEnv,position,name,SYMBOL_TYPE);
      moduleSpecified = true;
     }

   /*====================================================*/
   /* Check to see if the function call is a defgeneric. */
   /*====================================================*/

#if DEFGENERIC_CONSTRUCT
   if (moduleSpecified)
     {
      if (CL_ConstructExported(theEnv,"defgeneric",moduleName,constructName) ||
          CL_GetCurrentModule(theEnv) == CL_FindDefmodule(theEnv,moduleName->contents))
        {
         if ((gfunc = CL_FindDefgenericInModule(theEnv,name)) != NULL)
           {
            theReference->type = GCALL;
            theReference->value = gfunc;
            return true;
           }
        }
     }
   else
     {
      if ((gfunc = CL_LookupDefgenericInScope(theEnv,name)) != NULL)
        {
         theReference->type = GCALL;
         theReference->value = gfunc;
         return true;
        }
     }
#endif

   /*=====================================================*/
   /* Check to see if the function call is a deffunction. */
   /*=====================================================*/

#if DEFFUNCTION_CONSTRUCT
   if (moduleSpecified)
     {
      if (CL_ConstructExported(theEnv,"deffunction",moduleName,constructName) ||
          CL_GetCurrentModule(theEnv) == CL_FindDefmodule(theEnv,moduleName->contents))
        {
         if ((dptr = CL_FindDeffunctionInModule(theEnv,name)) != NULL)
           {
            theReference->type = PCALL;
            theReference->value = dptr;
            return true;
           }
        }
     }
   else
     {
      if ((dptr = CL_LookupDeffunctionInScope(theEnv,name)) != NULL)
        {
         theReference->type = PCALL;
         theReference->value = dptr;
         return true;
        }
     }
#endif

   /*======================================*/
   /* Check to see if the function call is */
   /* a system or user defined function.   */
   /*======================================*/

   if ((fptr = CL_FindFunction(theEnv,name)) != NULL)
     {
      theReference->type = FCALL;
      theReference->value = fptr;
      return true;
     }

   /*===================================================*/
   /* The specified function name is not a deffunction, */
   /* defgeneric, or user/system defined function.      */
   /*===================================================*/

   return false;
  }

/*******************************************************/
/* CL_DOsEqual: DeteCL_rmines if two DATA_OBJECTS are equal. */
/*******************************************************/
bool CL_DOsEqual(
  UDFValue *dobj1,
  UDFValue *dobj2)
  {
   if (dobj1->header->type != dobj2->header->type)
     { return false; }

   if (dobj1->header->type == MULTIFIELD_TYPE)
     {
      if (MultifieldCL_DOsEqual(dobj1,dobj2) == false)
        { return false; }
     }
   else if (dobj1->value != dobj2->value)
     { return false; }

   return true;
  }

/***********************************************************
  NAME         : CL_EvaluateAndStoreInDataObject
  DESCRIPTION  : CL_Evaluates slot-value expressions
                   and stores the result in a
                   Kernel data object
  INPUTS       : 1) Flag indicating if multifields are OK
                 2) The value-expression
                 3) The data object structure
                 4) Flag indicating if a multifield value
                    should be placed on the garbage list.
  RETURNS      : False on errors, true otherwise
  SIDE EFFECTS : Segment allocated for storing
                 multifield values
  NOTES        : None
 ***********************************************************/
bool CL_EvaluateAndStoreInDataObject(
  Environment *theEnv,
  bool mfp,
  Expression *theExp,
  UDFValue *val,
  bool garbageSegment)
  {
   val->begin = 0;
   val->range = 0;

   if (theExp == NULL)
     {
      if (garbageSegment) val->value = CL_CreateMultifield(theEnv,0L);
      else val->value = CL_CreateUnmanagedMultifield(theEnv,0L);

      return true;
     }

   if ((mfp == false) && (theExp->nextArg == NULL))
     CL_EvaluateExpression(theEnv,theExp,val);
   else
     CL_StoreInMultifield(theEnv,val,theExp,garbageSegment);

   return(CL_EvaluationData(theEnv)->CL_EvaluationError ? false : true);
  }

/******************/
/* PrintCAddress: */
/******************/
static void PrintCAddress(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
   char buffer[20];

   CL_WriteString(theEnv,logicalName,"<Pointer-C-");

   CL_gensprintf(buffer,"%p",((CLIPSExternalAddress *) theValue)->contents);
   CL_WriteString(theEnv,logicalName,buffer);
   CL_WriteString(theEnv,logicalName,">");
  }

/****************/
/* NewCAddress: */
/****************/
static void NewCAddress(
  UDFContext *context,
  UDFValue *rv)
  {
   unsigned int numberOfArguments;
   Environment *theEnv = context->environment;

   numberOfArguments = CL_UDFArgumentCount(context);

   if (numberOfArguments != 1)
     {
      CL_PrintErrorID(theEnv,"NEW",1,false);
      CL_WriteString(theEnv,STDERR,"Function new expected no additional arguments for the C external language type.\n");
      SetCL_EvaluationError(theEnv,true);
      return;
     }

   rv->value = CL_CreateExternalAddress(theEnv,NULL,0);
  }

/******************************/
/* CL_CreateFunctionCallCL_Builder: */
/******************************/
FunctionCallCL_Builder *CL_CreateFunctionCallCL_Builder(
  Environment *theEnv,
  size_t theSize)
  {
   FunctionCallCL_Builder *theFC;

   if (theEnv == NULL) return NULL;
   
   theFC = get_struct(theEnv,functionCallCL_Builder);
   
   theFC->fcbEnv = theEnv;
   theFC->bufferCL_Reset = theSize;
   theFC->bufferMaximum = theSize;
   theFC->length = 0;
   
   if (theSize == 0)
     { theFC->contents = NULL; }
   else
     { theFC->contents = (CLIPSValue *) CL_gm2(theEnv,sizeof(CLIPSValue) * theSize); }
     
   return theFC;
  }

/**********************/
/* CL_FCBAppendUDFValue: */
/**********************/
void CL_FCBAppendUDFValue(
  FunctionCallCL_Builder *theFCB,
  UDFValue *theValue)
  {
   Environment *theEnv = theFCB->fcbEnv;
   size_t i, neededSize, newSize;
   CLIPSValue *newArray;

   /*==============================================*/
   /* A void value can't be added to a multifield. */
   /*==============================================*/
   
   if (theValue->header->type == VOID_TYPE)
     { return; }

   /*=======================================*/
   /* DeteCL_rmine the amount of space needed. */
   /*=======================================*/
   
   neededSize = theFCB->length + 1;

   /*============================================*/
   /* Increase the size of the buffer if needed. */
   /*============================================*/
   
   if (neededSize > theFCB->bufferMaximum)
     {
      newSize = neededSize * 2;
      
      newArray = (CLIPSValue *) CL_gm2(theEnv,sizeof(CLIPSValue) * newSize);
      
      for (i = 0; i < theFCB->length; i++)
        { newArray[i] = theFCB->contents[i]; }
        
      if (theFCB->bufferMaximum != 0)
        { CL_rm(theFCB->fcbEnv,theFCB->contents,sizeof(CLIPSValue) * theFCB->bufferMaximum); }
        
      theFCB->bufferMaximum = newSize;
      theFCB->contents = newArray;
     }
     
   /*==================================*/
   /* Copy the new value to the array. */
   /*==================================*/
    
   if (theValue->header->type == MULTIFIELD_TYPE)
     {
      CLIPSValue newValue;
      
      CL_UDFToCLIPSValue(theEnv,theValue,&newValue);
      theFCB->contents[theFCB->length].value = newValue.value;
     }
   else
     { theFCB->contents[theFCB->length].value = theValue->value; }
     
   CL_Retain(theEnv,theFCB->contents[theFCB->length].header);
   theFCB->length++;
  }

/**************/
/* CL_FCBAppend: */
/**************/
void CL_FCBAppend(
  FunctionCallCL_Builder *theFCB,
  CLIPSValue *theValue)
  {
   Environment *theEnv = theFCB->fcbEnv;
   size_t i, neededSize, newSize;
   CLIPSValue *newArray;

   /*==============================================*/
   /* A void value can't be added to a multifield. */
   /*==============================================*/
   
   if (theValue->header->type == VOID_TYPE)
     { return; }

   /*=======================================*/
   /* DeteCL_rmine the amount of space needed. */
   /*=======================================*/
   
   neededSize = theFCB->length + 1;

   /*============================================*/
   /* Increase the size of the buffer if needed. */
   /*============================================*/
   
   if (neededSize > theFCB->bufferMaximum)
     {
      newSize = neededSize * 2;
      
      newArray = (CLIPSValue *) CL_gm2(theEnv,sizeof(CLIPSValue) * newSize);
      
      for (i = 0; i < theFCB->length; i++)
        { newArray[i] = theFCB->contents[i]; }
        
      if (theFCB->bufferMaximum != 0)
        { CL_rm(theFCB->fcbEnv,theFCB->contents,sizeof(CLIPSValue) * theFCB->bufferMaximum); }
        
      theFCB->bufferMaximum = newSize;
      theFCB->contents = newArray;
     }
     
   /*===================================*/
   /* Copy the new values to the array. */
   /*===================================*/

   theFCB->contents[theFCB->length].value = theValue->value;
   CL_Retain(theEnv,theFCB->contents[theFCB->length].header);
   theFCB->length++;
  }

/**************************/
/* CL_FCBAppendCLIPSInteger: */
/**************************/
void CL_FCBAppendCLIPSInteger(
  FunctionCallCL_Builder *theFCB,
  CLIPSInteger *pv)
  {
   CLIPSValue theValue;
   
   theValue.integerValue = pv;
   CL_FCBAppend(theFCB,&theValue);
  }

/*********************/
/* CL_FCBAppendInteger: */
/*********************/
void CL_FCBAppendInteger(
  FunctionCallCL_Builder *theFCB,
  long long intValue)
  {
   CLIPSValue theValue;
   CLIPSInteger *pv = CL_CreateInteger(theFCB->fcbEnv,intValue);
   
   theValue.integerValue = pv;
   CL_FCBAppend(theFCB,&theValue);
  }

/************************/
/* CL_FCBAppendCLIPSFloat: */
/************************/
void CL_FCBAppendCLIPSFloat(
  FunctionCallCL_Builder *theFCB,
  CLIPSFloat *pv)
  {
   CLIPSValue theValue;
   
   theValue.floatValue = pv;
   CL_FCBAppend(theFCB,&theValue);
  }

/*******************/
/* CL_FCBAppendFloat: */
/*******************/
void CL_FCBAppendFloat(
  FunctionCallCL_Builder *theFCB,
  double floatValue)
  {
   CLIPSValue theValue;
   CLIPSFloat *pv = CL_CreateFloat(theFCB->fcbEnv,floatValue);
   
   theValue.floatValue = pv;
   CL_FCBAppend(theFCB,&theValue);
  }

/*************************/
/* CL_FCBAppendCLIPSLexeme: */
/*************************/
void CL_FCBAppendCLIPSLexeme(
  FunctionCallCL_Builder *theFCB,
  CLIPSLexeme *pv)
  {
   CLIPSValue theValue;
   
   theValue.lexemeValue = pv;
   CL_FCBAppend(theFCB,&theValue);
  }

/********************/
/* CL_FCBAppendSymbol: */
/********************/
void CL_FCBAppendSymbol(
  FunctionCallCL_Builder *theFCB,
  const char *strValue)
  {
   CLIPSValue theValue;
   CLIPSLexeme *pv = CL_CreateSymbol(theFCB->fcbEnv,strValue);
   
   theValue.lexemeValue = pv;
   CL_FCBAppend(theFCB,&theValue);
  }

/********************/
/* CL_FCBAppendString: */
/********************/
void CL_FCBAppendString(
  FunctionCallCL_Builder *theFCB,
  const char *strValue)
  {
   CLIPSValue theValue;
   CLIPSLexeme *pv = CL_CreateString(theFCB->fcbEnv,strValue);
   
   theValue.lexemeValue = pv;
   CL_FCBAppend(theFCB,&theValue);
  }

/**************************/
/* CL_FCBAppendCL_InstanceName: */
/**************************/
void CL_FCBAppendCL_InstanceName(
  FunctionCallCL_Builder *theFCB,
  const char *strValue)
  {
   CLIPSValue theValue;
   CLIPSLexeme *pv = CL_CreateCL_InstanceName(theFCB->fcbEnv,strValue);
   
   theValue.lexemeValue = pv;
   CL_FCBAppend(theFCB,&theValue);
  }

/**********************************/
/* CL_FCBAppendCLIPSExternalAddress: */
/**********************************/
void CL_FCBAppendCLIPSExternalAddress(
  FunctionCallCL_Builder *theFCB,
  CLIPSExternalAddress *pv)
  {
   CLIPSValue theValue;
   
   theValue.externalAddressValue = pv;
   CL_FCBAppend(theFCB,&theValue);
  }

/******************/
/* CL_FCBAppendFact: */
/******************/
void CL_FCBAppendFact(
  FunctionCallCL_Builder *theFCB,
  Fact *pv)
  {
   CLIPSValue theValue;
   
   theValue.factValue = pv;
   CL_FCBAppend(theFCB,&theValue);
  }

/**********************/
/* CL_FCBAppendInstance: */
/**********************/
void CL_FCBAppendInstance(
  FunctionCallCL_Builder *theFCB,
  Instance *pv)
  {
   CLIPSValue theValue;
   
   theValue.instanceValue = pv;
   CL_FCBAppend(theFCB,&theValue);
  }

/************************/
/* CL_FCBAppendMultifield: */
/************************/
void CL_FCBAppendMultifield(
  FunctionCallCL_Builder *theFCB,
  Multifield *pv)
  {
   CLIPSValue theValue;
   
   theValue.multifieldValue = pv;
   CL_FCBAppend(theFCB,&theValue);
  }

/***********/
/* CL_FCBCall */
/***********/
FunctionCallCL_BuilderError CL_FCBCall(
  FunctionCallCL_Builder *theFCB,
  const char *functionName,
  CLIPSValue *returnValue)
  {
   Environment *theEnv;
   Expression theReference, *lastAdd = NULL, *nextAdd, *multiAdd;
   struct functionDefinition *theFunction = NULL;
   size_t i, j;
   UDFValue udfReturnValue;
   GCBlock gcb;

   /*==========================*/
   /* Check for NULL pointers. */
   /*==========================*/
   
   if ((theFCB == NULL) || (functionName == NULL))
     { return FCBE_NULL_POINTER_ERROR; }
   
   /*======================================*/
   /* Check to see if the function exists. */
   /*======================================*/
   
   if (! CL_GetFunctionReference(theFCB->fcbEnv,functionName,&theReference))
     { return FCBE_FUNCTION_NOT_FOUND_ERROR; }
     
   /*============================================*/
   /* Functions with specialized parsers  cannot */
   /* be used with a FunctionCallCL_Builder.        */
   /*============================================*/
   
   if (theReference.type == FCALL)
     {
      theFunction = CL_FindFunction(theFCB->fcbEnv,functionName);
      if (theFunction->parser != NULL)
        { return FCBE_INVALID_FUNCTION_ERROR; }
     }
   
   /*=======================================*/
   /* Append the arguments for the function */
   /* call to the expression.               */
   /*=======================================*/
   
   theEnv = theFCB->fcbEnv;
   
   for (i = 0; i < theFCB->length; i++)
     {
      /*====================================================*/
      /* Multifield values have to be dynamically recreated */
      /* through a create$ expression call.                 */
      /*====================================================*/
      
      if (theFCB->contents[i].header->type == MULTIFIELD_TYPE)
        {
         nextAdd = CL_GenConstant(theEnv,FCALL,CL_FindFunction(theEnv,"create$"));
         
         if (lastAdd == NULL)
           { theReference.argList = nextAdd; }
         else
           { lastAdd->nextArg = nextAdd; }
           
         lastAdd = nextAdd;
         
         multiAdd = NULL;
         for (j = 0; j < theFCB->contents[i].multifieldValue->length; j++)
           {
            nextAdd = CL_GenConstant(theEnv,theFCB->contents[i].multifieldValue->contents[j].header->type,
                                         theFCB->contents[i].multifieldValue->contents[j].value);
               
            if (multiAdd == NULL)
              { lastAdd->argList = nextAdd; }
            else
               { multiAdd->nextArg = nextAdd; }
            multiAdd = nextAdd;
           }
        }
        
      /*================================================================*/
      /* Single field values can just be appended to the argument list. */
      /*================================================================*/
      
      else
        {
         nextAdd = CL_GenConstant(theEnv,theFCB->contents[i].header->type,theFCB->contents[i].value);
         
         if (lastAdd == NULL)
           { theReference.argList = nextAdd; }
         else
           { lastAdd->nextArg = nextAdd; }
         lastAdd = nextAdd;
        }
     }
      
   CL_ExpressionInstall(theEnv,&theReference);
   
   /*===========================================================*/
   /* Verify a deffunction has the correct number of arguments. */
   /*===========================================================*/

#if DEFFUNCTION_CONSTRUCT
   if (theReference.type == PCALL)
     {
      if (CL_CheckDeffunctionCall(theEnv,(Deffunction *) theReference.value,CL_CountArguments(theReference.argList)) == false)
        {
         CL_ExpressionDeinstall(theEnv,&theReference);
         CL_ReturnExpression(theEnv,theReference.argList);
         return FCBE_ARGUMENT_COUNT_ERROR;
        }
     }
#endif

   /*=========================================*/
   /* Verify the correct number of arguments. */
   /*=========================================*/

// TBD Support run time check of arguments
#if ! RUN_TIME
   if (theReference.type == FCALL)
     {
      FunctionArgumentsError theError;
      if ((theError = CL_CheckExpressionAgainstRestrictions(theEnv,&theReference,theFunction,functionName)) != FAE_NO_ERROR)
        {
         CL_ExpressionDeinstall(theEnv,&theReference);
         CL_ReturnExpression(theEnv,theReference.argList);
         if (theError == FAE_TYPE_ERROR) return FCBE_ARGUMENT_TYPE_ERROR;
         else if (theError == FAE_COUNT_ERROR) return FCBE_ARGUMENT_COUNT_ERROR;
         else
           {
            CL_SystemError(theEnv,"EVALUATN",9);
            CL_ExitRouter(theEnv,EXIT_FAILURE);
           }
        }
     }
#endif
   /*========================================*/
   /* Set up the frame for tracking garbage. */
   /*========================================*/
   
   CL_GCBlockStart(theEnv,&gcb);

   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/
   
   if (CL_EvaluationData(theEnv)->CurrentExpression == NULL)
     { CL_ResetErrorFlags(theEnv); }

   /*======================*/
   /* Call the expression. */
   /*======================*/

   CL_EvaluateExpression(theEnv,&theReference,&udfReturnValue);

   /*====================================================*/
   /* Convert a partial multifield to a full multifield. */
   /*====================================================*/
   
   CL_NoCL_rmalizeMultifield(theEnv,&udfReturnValue);
   
   /*========================================*/
   /* Return the expression data structures. */
   /*========================================*/

   CL_ExpressionDeinstall(theEnv,&theReference);
   CL_ReturnExpression(theEnv,theReference.argList);

   /*================================*/
   /* Restore the old garbage frame. */
   /*================================*/
   
   if (returnValue != NULL)
     { CL_GCBlockEndUDF(theEnv,&gcb,&udfReturnValue); }
   else
     { CL_GCBlockEnd(theEnv,&gcb); }
     
   /*==========================================*/
   /* PerfoCL_rm periodic cleanup if the eval was */
   /* issued from an embedded controller.      */
   /*==========================================*/

   if (CL_EvaluationData(theEnv)->CurrentExpression == NULL)
     {
      if (returnValue != NULL)
        { CL_CleanCurrentGarbageFrame(theEnv,&udfReturnValue); }
      else
        { CL_CleanCurrentGarbageFrame(theEnv,NULL); }
      CL_CallPeriodicTasks(theEnv);
     }
     
   if (returnValue != NULL)
     { returnValue->value = udfReturnValue.value; }
     
   if (GetCL_EvaluationError(theEnv)) return FCBE_PROCESSING_ERROR;

   return FCBE_NO_ERROR;
  }

/*************/
/* CL_FCBCL_Reset: */
/*************/
void CL_FCBCL_Reset(
  FunctionCallCL_Builder *theFCB)
  {
   size_t i;
   
   for (i = 0; i < theFCB->length; i++)
     { CL_Release(theFCB->fcbEnv,theFCB->contents[i].header); }
     
   if (theFCB->bufferCL_Reset != theFCB->bufferMaximum)
     {
      if (theFCB->bufferMaximum != 0)
        { CL_rm(theFCB->fcbEnv,theFCB->contents,sizeof(CLIPSValue) * theFCB->bufferMaximum); }
      
      if (theFCB->bufferCL_Reset == 0)
        { theFCB->contents = NULL; }
      else
        { theFCB->contents = (CLIPSValue *) CL_gm2(theFCB->fcbEnv,sizeof(CLIPSValue) * theFCB->bufferCL_Reset); }
      
      theFCB->bufferMaximum = theFCB->bufferCL_Reset;
     }
     
   theFCB->length = 0;
  }

/***************/
/* CL_FCBDispose: */
/***************/
void CL_FCBDispose(
  FunctionCallCL_Builder *theFCB)
  {
   Environment *theEnv = theFCB->fcbEnv;
   size_t i;
   
   for (i = 0; i < theFCB->length; i++)
     { CL_Release(theFCB->fcbEnv,theFCB->contents[i].header); }
   
   if (theFCB->bufferMaximum != 0)
     { CL_rm(theFCB->fcbEnv,theFCB->contents,sizeof(CLIPSValue) * theFCB->bufferMaximum); }
     
   rtn_struct(theEnv,multifieldCL_Builder,theFCB);
  }

/*******************************/
/* DiscardCAddress: TBD Remove */
/*******************************/
/*
static bool DiscardCAddress(
  Environment *theEnv,
  void *theValue)
  {
   CL_WriteString(theEnv,STDOUT,"Discarding C Address\n");

   return true;
  }
*/

