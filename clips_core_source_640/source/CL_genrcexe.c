   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/01/16             */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Generic Function Execution Routines              */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Removed IMPERATIVE_METHODS compilation flag.   */
/*                                                           */
/*      6.30: Changed garbage collection algorithm.          */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Added Env prefix to GetCL_EvaluationError and     */
/*            SetCL_EvaluationError functions.                  */
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
/*            Added CL_GCBlockStart and CL_GCBlockEnd functions    */
/*            for garbage collection blocks.                 */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if DEFGENERIC_CONSTRUCT

#include <string.h>

#if OBJECT_SYSTEM
#include "classcom.h"
#include "classfun.h"
#include "insfun.h"
#endif

#include "argacces.h"
#include "constrct.h"
#include "envrnmnt.h"
#include "genrccom.h"
#include "prcdrfun.h"
#include "prccode.h"
#include "prntutil.h"
#include "proflfun.h"
#include "router.h"
#include "utility.h"

#include "genrcexe.h"

/* =========================================
   *****************************************
                   CONSTANTS
   =========================================
   ***************************************** */

#define BEGIN_TRACE     ">>"
#define END_TRACE       "<<"

/* =========================================
   *****************************************
      INTERNALLY VISIBLE FUNCTION HEADERS
   =========================================
   ***************************************** */

   static Defmethod              *FindApplicableMethod(Environment *,Defgeneric *,Defmethod *);

#if DEBUGGING_FUNCTIONS
   static void                    CL_WatchGeneric(Environment *,const char *);
   static void                    CL_WatchMethod(Environment *,const char *);
#endif

#if OBJECT_SYSTEM
   static Defclass               *DeteCL_rmineRestrictionClass(Environment *,UDFValue *);
#endif

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***********************************************************************************
  NAME         : CL_GenericDispatch
  DESCRIPTION  : Executes the most specific applicable method
  INPUTS       : 1) The generic function
                 2) The method to start after in the search for an applicable
                    method (ignored if arg #3 is not NULL).
                 3) A specific method to call (NULL if want highest precedence
                    method to be called)
                 4) The generic function argument expressions
                 5) The caller's result value buffer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Any side-effects of evaluating the generic function arguments
                 Any side-effects of evaluating query functions on method parameter
                   restrictions when deteCL_rmining the core (see warning #1)
                 Any side-effects of actual execution of methods (see warning #2)
                 Caller's buffer set to the result of the generic function call

                 In case of errors, the result is false, otherwise it is the
                   result returned by the most specific method (which can choose
                   to ignore or return the values of more general methods)
  NOTES        : WARNING #1: Query functions on method parameter restrictions
                    should not have side-effects, for they might be evaluated even
                    for methods that aren't applicable to the generic function call.
                 WARNING #2: Side-effects of method execution should not always rely
                    on only being executed once per generic function call.  Every
                    time a method calls (shadow-call) the same next-most-specific
                    method is executed.  Thus, it is possible for a method to be
                    executed multiple times per generic function call.
 ***********************************************************************************/
void CL_GenericDispatch(
  Environment *theEnv,
  Defgeneric *gfunc,
  Defmethod *prevmeth,
  Defmethod *meth,
  Expression *params,
  UDFValue *returnValue)
  {
   Defgeneric *previousGeneric;
   Defmethod *previousMethod;
   bool oldce;
#if PROFILING_FUNCTIONS
   struct profileFrameInfo profileFrame;
#endif
   GCBlock gcb;

   returnValue->value = FalseSymbol(theEnv);
   CL_EvaluationData(theEnv)->CL_EvaluationError = false;
   if (CL_EvaluationData(theEnv)->CL_HaltExecution)
     return;

   CL_GCBlockStart(theEnv,&gcb);

   oldce = CL_ExecutingConstruct(theEnv);
   SetCL_ExecutingConstruct(theEnv,true);
   previousGeneric = DefgenericData(theEnv)->CurrentGeneric;
   previousMethod = DefgenericData(theEnv)->CurrentMethod;
   DefgenericData(theEnv)->CurrentGeneric = gfunc;
   CL_EvaluationData(theEnv)->CurrentCL_EvaluationDepth++;
   gfunc->busy++;
   CL_PushProcParameters(theEnv,params,CL_CountArguments(params),
                      CL_DefgenericName(gfunc),
                      "generic function",CL_UnboundMethodErr);
   if (CL_EvaluationData(theEnv)->CL_EvaluationError)
     {
      gfunc->busy--;
      DefgenericData(theEnv)->CurrentGeneric = previousGeneric;
      DefgenericData(theEnv)->CurrentMethod = previousMethod;
      CL_EvaluationData(theEnv)->CurrentCL_EvaluationDepth--;

      CL_GCBlockEndUDF(theEnv,&gcb,returnValue);
      CL_CallPeriodicTasks(theEnv);

      SetCL_ExecutingConstruct(theEnv,oldce);
      return;
     }
   if (meth != NULL)
     {
      if (CL_IsMethodApplicable(theEnv,meth))
        {
         meth->busy++;
         DefgenericData(theEnv)->CurrentMethod = meth;
        }
      else
        {
         CL_PrintErrorID(theEnv,"GENRCEXE",4,false);
         SetCL_EvaluationError(theEnv,true);
         DefgenericData(theEnv)->CurrentMethod = NULL;
         CL_WriteString(theEnv,STDERR,"Generic function '");
         CL_WriteString(theEnv,STDERR,CL_DefgenericName(gfunc));
         CL_WriteString(theEnv,STDERR,"' method #");
         CL_PrintUnsignedInteger(theEnv,STDERR,meth->index);
         CL_WriteString(theEnv,STDERR," is not applicable to the given arguments.\n");
        }
     }
   else
     DefgenericData(theEnv)->CurrentMethod = FindApplicableMethod(theEnv,gfunc,prevmeth);
   if (DefgenericData(theEnv)->CurrentMethod != NULL)
     {
#if DEBUGGING_FUNCTIONS
      if (DefgenericData(theEnv)->CurrentGeneric->trace)
        CL_WatchGeneric(theEnv,BEGIN_TRACE);
      if (DefgenericData(theEnv)->CurrentMethod->trace)
        CL_WatchMethod(theEnv,BEGIN_TRACE);
#endif
      if (DefgenericData(theEnv)->CurrentMethod->system)
        {
         Expression fcall;

         fcall.type = FCALL;
         fcall.value = DefgenericData(theEnv)->CurrentMethod->actions->value;
         fcall.nextArg = NULL;
         fcall.argList = CL_GetProcParamExpressions(theEnv);
         CL_EvaluateExpression(theEnv,&fcall,returnValue);
        }
      else
        {
#if PROFILING_FUNCTIONS
         StartCL_Profile(theEnv,&profileFrame,
                      &DefgenericData(theEnv)->CurrentMethod->header.usrData,
                      CL_ProfileFunctionData(theEnv)->CL_ProfileConstructs);
#endif

         CL_EvaluateProcActions(theEnv,DefgenericData(theEnv)->CurrentGeneric->header.whichModule->theModule,
                             DefgenericData(theEnv)->CurrentMethod->actions,DefgenericData(theEnv)->CurrentMethod->localVarCount,
                             returnValue,CL_UnboundMethodErr);

#if PROFILING_FUNCTIONS
         CL_EndCL_Profile(theEnv,&profileFrame);
#endif
        }
      DefgenericData(theEnv)->CurrentMethod->busy--;
#if DEBUGGING_FUNCTIONS
      if (DefgenericData(theEnv)->CurrentMethod->trace)
        CL_WatchMethod(theEnv,END_TRACE);
      if (DefgenericData(theEnv)->CurrentGeneric->trace)
        CL_WatchGeneric(theEnv,END_TRACE);
#endif
     }
   else if (! CL_EvaluationData(theEnv)->CL_EvaluationError)
     {
      CL_PrintErrorID(theEnv,"GENRCEXE",1,false);
      CL_WriteString(theEnv,STDERR,"No applicable methods for '");
      CL_WriteString(theEnv,STDERR,CL_DefgenericName(gfunc));
      CL_WriteString(theEnv,STDERR,"'.\n");
      SetCL_EvaluationError(theEnv,true);
     }
   gfunc->busy--;
   ProcedureFunctionData(theEnv)->ReturnFlag = false;
   CL_PopProcParameters(theEnv);
   DefgenericData(theEnv)->CurrentGeneric = previousGeneric;
   DefgenericData(theEnv)->CurrentMethod = previousMethod;
   CL_EvaluationData(theEnv)->CurrentCL_EvaluationDepth--;

   CL_GCBlockEndUDF(theEnv,&gcb,returnValue);
   CL_CallPeriodicTasks(theEnv);

   SetCL_ExecutingConstruct(theEnv,oldce);
  }

/*******************************************************
  NAME         : CL_UnboundMethodErr
  DESCRIPTION  : Print out a synopis of the currently
                   executing method for unbound variable
                   errors
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Error synopsis printed to STDERR
  NOTES        : None
 *******************************************************/
void CL_UnboundMethodErr(
  Environment *theEnv,
  const char *logName)
  {
   CL_WriteString(theEnv,logName,"generic function '");
   CL_WriteString(theEnv,logName,CL_DefgenericName(DefgenericData(theEnv)->CurrentGeneric));
   CL_WriteString(theEnv,logName,"' method #");
   CL_PrintUnsignedInteger(theEnv,logName,DefgenericData(theEnv)->CurrentMethod->index);
   CL_WriteString(theEnv,logName,".\n");
  }

/***********************************************************************
  NAME         : CL_IsMethodApplicable
  DESCRIPTION  : Tests to see if a method satsifies the arguments of a
                   generic function
                 A method is applicable if all its restrictions are
                   satisfied by the corresponding arguments
  INPUTS       : The method address
  RETURNS      : True if method is applicable, false otherwise
  SIDE EFFECTS : Any query functions are evaluated
  NOTES        : Uses globals ProcParamArraySize and ProcParamArray
 ***********************************************************************/
bool CL_IsMethodApplicable(
  Environment *theEnv,
  Defmethod *meth)
  {
   UDFValue temp;
   unsigned int i,j,k;
   RESTRICTION *rp;
#if OBJECT_SYSTEM
   Defclass *type;
#else
   int type;
#endif

   if (((ProceduralPrimitiveData(theEnv)->ProcParamArraySize < meth->minRestrictions) && (meth->minRestrictions != RESTRICTIONS_UNBOUNDED)) ||
       ((ProceduralPrimitiveData(theEnv)->ProcParamArraySize > meth->minRestrictions) && (meth->maxRestrictions != RESTRICTIONS_UNBOUNDED))) // TBD minRestrictions || maxRestrictions
     return false;
   for (i = 0 , k = 0 ; i < ProceduralPrimitiveData(theEnv)->ProcParamArraySize ; i++)
     {
      rp = &meth->restrictions[k];
      if (rp->tcnt != 0)
        {
#if OBJECT_SYSTEM
         type = DeteCL_rmineRestrictionClass(theEnv,&ProceduralPrimitiveData(theEnv)->ProcParamArray[i]);
         if (type == NULL)
           return false;
         for (j = 0 ; j < rp->tcnt ; j++)
           {
            if (type == rp->types[j])
              break;
            if (CL_HasSuperclass(type,(Defclass *) rp->types[j]))
              break;
            if (rp->types[j] == (void *) DefclassData(theEnv)->PrimitiveClassMap[INSTANCE_ADDRESS_TYPE])
              {
               if (ProceduralPrimitiveData(theEnv)->ProcParamArray[i].header->type == INSTANCE_ADDRESS_TYPE)
                 break;
              }
            else if (rp->types[j] == (void *) DefclassData(theEnv)->PrimitiveClassMap[INSTANCE_NAME_TYPE])
              {
               if (ProceduralPrimitiveData(theEnv)->ProcParamArray[i].header->type == INSTANCE_NAME_TYPE)
                 break;
              }
            else if (rp->types[j] ==
                DefclassData(theEnv)->PrimitiveClassMap[INSTANCE_NAME_TYPE]->directSuperclasses.classArray[0])
              {
               if ((ProceduralPrimitiveData(theEnv)->ProcParamArray[i].header->type == INSTANCE_NAME_TYPE) ||
                   (ProceduralPrimitiveData(theEnv)->ProcParamArray[i].header->type == INSTANCE_ADDRESS_TYPE))
                 break;
              }
           }
#else
         type = ProceduralPrimitiveData(theEnv)->ProcParamArray[i].header->type;
         for (j = 0 ; j < rp->tcnt ; j++)
           {
            if (type == ((CLIPSInteger *) (rp->types[j]))->contents)
              break;
            if (SubsumeType(type,((CLIPSInteger *) (rp->types[j]))->contents))
              break;
           }
#endif
         if (j == rp->tcnt)
           return false;
        }
      if (rp->query != NULL)
        {
         DefgenericData(theEnv)->GenericCurrentArgument = &ProceduralPrimitiveData(theEnv)->ProcParamArray[i];
         CL_EvaluateExpression(theEnv,rp->query,&temp);
         if (temp.value == FalseSymbol(theEnv))
           return false;
        }
      if ((k + 1) != meth->restrictionCount)
        k++;
     }
   return true;
  }

/***************************************************
  NAME         : CL_NextMethodP
  DESCRIPTION  : DeteCL_rmines if a shadowed generic
                   function method is available for
                   execution
  INPUTS       : None
  RETURNS      : True if there is a method available,
                   false otherwise
  SIDE EFFECTS : None
  NOTES        : H/L Syntax: (next-methodp)
 ***************************************************/
bool CL_NextMethodP(
  Environment *theEnv)
  {
   Defmethod *meth;

   if (DefgenericData(theEnv)->CurrentMethod == NULL)
     { return false; }

   meth = FindApplicableMethod(theEnv,DefgenericData(theEnv)->CurrentGeneric,DefgenericData(theEnv)->CurrentMethod);
   if (meth != NULL)
     {
      meth->busy--;
      return true;
     }
   else
     { return false; }
  }

void CL_NextMethodPCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_NextMethodP(theEnv));
  }

/****************************************************
  NAME         : CL_CallNextMethod
  DESCRIPTION  : Executes the next available method
                   in the core for a generic function
  INPUTS       : Caller's buffer for the result
  RETURNS      : Nothing useful
  SIDE EFFECTS : Side effects of execution of shadow
                 CL_EvaluationError set if no method
                   is available to execute.
  NOTES        : H/L Syntax: (call-next-method)
 ****************************************************/
void CL_CallNextMethod(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Defmethod *oldMethod;
#if PROFILING_FUNCTIONS
   struct profileFrameInfo profileFrame;
#endif

   returnValue->lexemeValue = FalseSymbol(theEnv);

   if (CL_EvaluationData(theEnv)->CL_HaltExecution)
     return;
   oldMethod = DefgenericData(theEnv)->CurrentMethod;
   if (DefgenericData(theEnv)->CurrentMethod != NULL)
     DefgenericData(theEnv)->CurrentMethod = FindApplicableMethod(theEnv,DefgenericData(theEnv)->CurrentGeneric,DefgenericData(theEnv)->CurrentMethod);
   if (DefgenericData(theEnv)->CurrentMethod == NULL)
     {
      DefgenericData(theEnv)->CurrentMethod = oldMethod;
      CL_PrintErrorID(theEnv,"GENRCEXE",2,false);
      CL_WriteString(theEnv,STDERR,"Shadowed methods not applicable in current context.\n");
      SetCL_EvaluationError(theEnv,true);
      return;
     }

#if DEBUGGING_FUNCTIONS
   if (DefgenericData(theEnv)->CurrentMethod->trace)
     CL_WatchMethod(theEnv,BEGIN_TRACE);
#endif
   if (DefgenericData(theEnv)->CurrentMethod->system)
     {
      Expression fcall;

      fcall.type = FCALL;
      fcall.value = DefgenericData(theEnv)->CurrentMethod->actions->value;
      fcall.nextArg = NULL;
      fcall.argList = CL_GetProcParamExpressions(theEnv);
      CL_EvaluateExpression(theEnv,&fcall,returnValue);
     }
   else
     {
#if PROFILING_FUNCTIONS
      StartCL_Profile(theEnv,&profileFrame,
                   &DefgenericData(theEnv)->CurrentGeneric->header.usrData,
                   CL_ProfileFunctionData(theEnv)->CL_ProfileConstructs);
#endif

      CL_EvaluateProcActions(theEnv,DefgenericData(theEnv)->CurrentGeneric->header.whichModule->theModule,
                         DefgenericData(theEnv)->CurrentMethod->actions,DefgenericData(theEnv)->CurrentMethod->localVarCount,
                         returnValue,CL_UnboundMethodErr);

#if PROFILING_FUNCTIONS
      CL_EndCL_Profile(theEnv,&profileFrame);
#endif
     }

   DefgenericData(theEnv)->CurrentMethod->busy--;
#if DEBUGGING_FUNCTIONS
   if (DefgenericData(theEnv)->CurrentMethod->trace)
     CL_WatchMethod(theEnv,END_TRACE);
#endif
   DefgenericData(theEnv)->CurrentMethod = oldMethod;
   ProcedureFunctionData(theEnv)->ReturnFlag = false;
  }

/**************************************************************************
  NAME         : CL_CallSpecificMethod
  DESCRIPTION  : Allows a specific method to be called without regards to
                   higher precedence methods which might also be applicable
                   However, shadowed methods can still be called.
  INPUTS       : A data object buffer to hold the method evaluation result
  RETURNS      : Nothing useful
  SIDE EFFECTS : Side-effects of method applicability tests and the
                 evaluation of methods
  NOTES        : H/L Syntax: (call-specific-method
                                <generic-function> <method-index> <args>)
 **************************************************************************/
void CL_CallSpecificMethod(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;
   Defgeneric *gfunc;
   int mi;

   returnValue->lexemeValue = FalseSymbol(theEnv);

   if (! CL_UDFFirstArgument(context,SYMBOL_BIT,&theArg)) return;

   gfunc = CL_CheckGenericExists(theEnv,"call-specific-method",theArg.lexemeValue->contents);
   if (gfunc == NULL) return;

   if (! CL_UDFNextArgument(context,INTEGER_BIT,&theArg)) return;

   mi = CL_CheckMethodExists(theEnv,"call-specific-method",gfunc,(unsigned short) theArg.integerValue->contents);
   if (mi == METHOD_NOT_FOUND)
     return;
   gfunc->methods[mi].busy++;
   CL_GenericDispatch(theEnv,gfunc,NULL,&gfunc->methods[mi],
                   GetFirstArgument()->nextArg->nextArg,returnValue);
   gfunc->methods[mi].busy--;
  }

/***********************************************************************
  NAME         : CL_OverrideNextMethod
  DESCRIPTION  : Changes the arguments to shadowed methods, thus the set
                 of applicable methods to this call may change
  INPUTS       : A buffer to hold the result of the call
  RETURNS      : Nothing useful
  SIDE EFFECTS : Any of evaluating method restrictions and bodies
  NOTES        : H/L Syntax: (override-next-method <args>)
 ***********************************************************************/
void CL_OverrideNextMethod(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->lexemeValue = FalseSymbol(theEnv);
   if (CL_EvaluationData(theEnv)->CL_HaltExecution)
     return;
   if (DefgenericData(theEnv)->CurrentMethod == NULL)
     {
      CL_PrintErrorID(theEnv,"GENRCEXE",2,false);
      CL_WriteString(theEnv,STDERR,"Shadowed methods not applicable in current context.\n");
      SetCL_EvaluationError(theEnv,true);
      return;
     }
   CL_GenericDispatch(theEnv,DefgenericData(theEnv)->CurrentGeneric,DefgenericData(theEnv)->CurrentMethod,NULL,
                   GetFirstArgument(),returnValue);
  }

/***********************************************************
  NAME         : CL_GetGenericCurrentArgument
  DESCRIPTION  : Returns the value of the generic function
                   argument being tested in the method
                   applicability deteCL_rmination process
  INPUTS       : A data-object buffer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Data-object set
  NOTES        : Useful for queries in wildcard restrictions
 ***********************************************************/
void CL_GetGenericCurrentArgument(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->value = DefgenericData(theEnv)->GenericCurrentArgument->value;
   returnValue->begin = DefgenericData(theEnv)->GenericCurrentArgument->begin;
   returnValue->range = DefgenericData(theEnv)->GenericCurrentArgument->range;
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/************************************************************
  NAME         : FindApplicableMethod
  DESCRIPTION  : Finds the first/next applicable
                   method for a generic function call
  INPUTS       : 1) The generic function pointer
                 2) The address of the current method
                    (NULL to find the first)
  RETURNS      : The address of the first/next
                   applicable method (NULL on errors)
  SIDE EFFECTS : Any from evaluating query restrictions
                 Methoid busy count incremented if applicable
  NOTES        : None
 ************************************************************/
static Defmethod *FindApplicableMethod(
  Environment *theEnv,
  Defgeneric *gfunc,
  Defmethod *meth)
  {
   if (meth != NULL)
     meth++;
   else
     meth = gfunc->methods;
   for ( ; meth < &gfunc->methods[gfunc->mcnt] ; meth++)
     {
      meth->busy++;
      if (CL_IsMethodApplicable(theEnv,meth))
        return(meth);
      meth->busy--;
     }
   return NULL;
  }

#if DEBUGGING_FUNCTIONS

/**********************************************************************
  NAME         : CL_WatchGeneric
  DESCRIPTION  : Prints out a trace of the beginning or end
                   of the execution of a generic function
  INPUTS       : A string to indicate beginning or end of execution
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : Uses the globals CurrentGeneric, ProcParamArraySize and
                   ProcParamArray for other trace info
 **********************************************************************/
static void CL_WatchGeneric(
  Environment *theEnv,
  const char *tstring)
  {
   if (ConstructData(theEnv)->CL_ClearReadyInProgress ||
       ConstructData(theEnv)->CL_ClearInProgress)
     { return; }

   CL_WriteString(theEnv,STDOUT,"GNC ");
   CL_WriteString(theEnv,STDOUT,tstring);
   CL_WriteString(theEnv,STDOUT," ");
   if (DefgenericData(theEnv)->CurrentGeneric->header.whichModule->theModule != CL_GetCurrentModule(theEnv))
     {
      CL_WriteString(theEnv,STDOUT,CL_DefgenericModule(DefgenericData(theEnv)->CurrentGeneric));
      CL_WriteString(theEnv,STDOUT,"::");
     }
   CL_WriteString(theEnv,STDOUT,DefgenericData(theEnv)->CurrentGeneric->header.name->contents);
   CL_WriteString(theEnv,STDOUT," ");
   CL_WriteString(theEnv,STDOUT," ED:");
   CL_WriteInteger(theEnv,STDOUT,CL_EvaluationData(theEnv)->CurrentCL_EvaluationDepth);
   CL_PrintProcParamArray(theEnv,STDOUT);
  }

/**********************************************************************
  NAME         : CL_WatchMethod
  DESCRIPTION  : Prints out a trace of the beginning or end
                   of the execution of a generic function
                   method
  INPUTS       : A string to indicate beginning or end of execution
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : Uses the globals CurrentGeneric, CurrentMethod,
                   ProcParamArraySize and ProcParamArray for
                   other trace info
 **********************************************************************/
static void CL_WatchMethod(
  Environment *theEnv,
  const char *tstring)
  {
   if (ConstructData(theEnv)->CL_ClearReadyInProgress ||
       ConstructData(theEnv)->CL_ClearInProgress)
     { return; }

   CL_WriteString(theEnv,STDOUT,"MTH ");
   CL_WriteString(theEnv,STDOUT,tstring);
   CL_WriteString(theEnv,STDOUT," ");
   if (DefgenericData(theEnv)->CurrentGeneric->header.whichModule->theModule != CL_GetCurrentModule(theEnv))
     {
      CL_WriteString(theEnv,STDOUT,CL_DefgenericModule(DefgenericData(theEnv)->CurrentGeneric));
      CL_WriteString(theEnv,STDOUT,"::");
     }
   CL_WriteString(theEnv,STDOUT,DefgenericData(theEnv)->CurrentGeneric->header.name->contents);
   CL_WriteString(theEnv,STDOUT,":#");
   if (DefgenericData(theEnv)->CurrentMethod->system)
     CL_WriteString(theEnv,STDOUT,"SYS");
   CL_PrintUnsignedInteger(theEnv,STDOUT,DefgenericData(theEnv)->CurrentMethod->index);
   CL_WriteString(theEnv,STDOUT," ");
   CL_WriteString(theEnv,STDOUT," ED:");
   CL_WriteInteger(theEnv,STDOUT,CL_EvaluationData(theEnv)->CurrentCL_EvaluationDepth);
   CL_PrintProcParamArray(theEnv,STDOUT);
  }

#endif

#if OBJECT_SYSTEM

/***************************************************
  NAME         : DeteCL_rmineRestrictionClass
  DESCRIPTION  : Finds the class of an argument in
                   the ProcParamArray
  INPUTS       : The argument data object
  RETURNS      : The class address, NULL if error
  SIDE EFFECTS : CL_EvaluationError set on errors
  NOTES        : None
 ***************************************************/
static Defclass *DeteCL_rmineRestrictionClass(
  Environment *theEnv,
  UDFValue *dobj)
  {
   Instance *ins;
   Defclass *cls;

   if (dobj->header->type == INSTANCE_NAME_TYPE)
     {
      ins = CL_FindInstanceBySymbol(theEnv,dobj->lexemeValue);
      cls = (ins != NULL) ? ins->cls : NULL;
     }
   else if (dobj->header->type == INSTANCE_ADDRESS_TYPE)
     {
      ins = dobj->instanceValue;
      cls = (ins->garbage == 0) ? ins->cls : NULL;
     }
   else
     return(DefclassData(theEnv)->PrimitiveClassMap[dobj->header->type]);
   if (cls == NULL)
     {
      SetCL_EvaluationError(theEnv,true);
      CL_PrintErrorID(theEnv,"GENRCEXE",3,false);
      CL_WriteString(theEnv,STDERR,"Unable to deteCL_rmine class of ");
      CL_WriteUDFValue(theEnv,STDERR,dobj);
      CL_WriteString(theEnv,STDERR," in generic function '");
      CL_WriteString(theEnv,STDERR,CL_DefgenericName(DefgenericData(theEnv)->CurrentGeneric));
      CL_WriteString(theEnv,STDERR,"'.\n");
     }
   return(cls);
  }

#endif

#endif

