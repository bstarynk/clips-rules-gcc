   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*              DEFGLOBAL COMMANDS MODULE              */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides the show-defglobals, set-reset-globals, */
/*   and get-reset-globals commands.                         */
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
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
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

#include "setup.h"

#if DEFGLOBAL_CONSTRUCT

#include "argacces.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "globldef.h"
#include "prntutil.h"
#include "router.h"

#include "globlcom.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if DEBUGGING_FUNCTIONS
   static void                       PrintCL_DefglobalValueFo_rm(Environment *,const char *,Defglobal *);
#endif

/************************************************************/
/* CL_DefglobalCommandDefinitions: Defines defglobal commands. */
/************************************************************/
void CL_DefglobalCommandDefinitions(
  Environment *theEnv)
  {
#if ! RUN_TIME
   CL_AddUDF(theEnv,"set-reset-globals","b",1,1,NULL,Set_ResetGlobalsCommand,"Set_ResetGlobalsCommand",NULL);
   CL_AddUDF(theEnv,"get-reset-globals","b",0,0,NULL,CL_Get_ResetGlobalsCommand,"CL_Get_ResetGlobalsCommand",NULL);

#if DEBUGGING_FUNCTIONS
   CL_AddUDF(theEnv,"show-defglobals","v",0,1,"y",CL_ShowDefglobalsCommand,"CL_ShowDefglobalsCommand",NULL);
#endif

#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
  }

/************************************************/
/* Set_ResetGlobalsCommand: H/L access routine   */
/*   for the get-reset-globals command.         */
/************************************************/
void Set_ResetGlobalsCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   bool oldValue;
   UDFValue theArg;

   /*===========================================*/
   /* Remember the old value of this attribute. */
   /*===========================================*/

   oldValue = CL_Get_ResetGlobals(theEnv);

   /*===========================================*/
   /* Dete_rmine the new value of the attribute. */
   /*===========================================*/

   if (! CL_UDFFirstArgument(context,ANY_TYPE_BITS,&theArg))
     { return; }

   if (theArg.value == FalseSymbol(theEnv))
     { Set_ResetGlobals(theEnv,false); }
   else
     { Set_ResetGlobals(theEnv,true); }

   /*========================================*/
   /* Return the old value of the attribute. */
   /*========================================*/

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,oldValue);
  }

/****************************************/
/* Set_ResetGlobals: C access routine */
/*   for the set-reset-globals command. */
/****************************************/
bool Set_ResetGlobals(
  Environment *theEnv,
  bool value)
  {
   bool ov;

   ov = DefglobalData(theEnv)->CL_ResetGlobals;
   DefglobalData(theEnv)->CL_ResetGlobals = value;
   return(ov);
  }

/************************************************/
/* CL_Get_ResetGlobalsCommand: H/L access routine   */
/*   for the get-reset-globals command.         */
/************************************************/
void CL_Get_ResetGlobalsCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_Get_ResetGlobals(theEnv));
  }

/****************************************/
/* CL_Get_ResetGlobals: C access routine    */
/*   for the get-reset-globals command. */
/****************************************/
bool CL_Get_ResetGlobals(
  Environment *theEnv)
  {
   return(DefglobalData(theEnv)->CL_ResetGlobals);
  }

#if DEBUGGING_FUNCTIONS

/***********************************************/
/* CL_ShowDefglobalsCommand: H/L access routine   */
/*   for the show-defglobals command.          */
/***********************************************/
void CL_ShowDefglobalsCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Defmodule *theModule;
   unsigned int numArgs;
   bool error;

   numArgs = CL_UDFArgumentCount(context);

   if (numArgs == 1)
     {
      theModule = CL_GetModuleName(context,1,&error);
      if (error) return;
     }
   else
     { theModule = CL_GetCurrentModule(theEnv); }

   CL_ShowDefglobals(theEnv,STDOUT,theModule);
  }

/**************************************/
/* CL_ShowDefglobals: C access routine   */
/*   for the show-defglobals command. */
/**************************************/
void CL_ShowDefglobals(
  Environment *theEnv,
  const char *logicalName,
  Defmodule *theModule)
  {
   ConstructHeader *constructPtr;
   bool allModules = false;
   struct defmoduleItemHeader *theModuleItem;

   /*=======================================*/
   /* If the module specified is NULL, then */
   /* list all constructs in all modules.   */
   /*=======================================*/

   if (theModule == NULL)
     {
      theModule = CL_GetNextDefmodule(theEnv,NULL);
      allModules = true;
     }

   /*======================================================*/
   /* Print out the constructs in the specified module(s). */
   /*======================================================*/

   for (;
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      /*===========================================*/
      /* Print the module name before every group  */
      /* of defglobals listed if we're listing the */
      /* defglobals from every module.             */
      /*===========================================*/

      if (allModules)
        {
         CL_WriteString(theEnv,logicalName,CL_DefmoduleName(theModule));
         CL_WriteString(theEnv,logicalName,":\n");
        }

      /*=====================================*/
      /* Print every defglobal in the module */
      /* currently being examined.           */
      /*=====================================*/

      theModuleItem = (struct defmoduleItemHeader *) CL_GetModuleItem(theEnv,theModule,DefglobalData(theEnv)->CL_DefglobalModuleIndex);

      for (constructPtr = theModuleItem->firstItem;
           constructPtr != NULL;
           constructPtr = constructPtr->next)
        {
         if (CL_EvaluationData(theEnv)->CL_HaltExecution == true) return;

         if (allModules) CL_WriteString(theEnv,logicalName,"   ");
         PrintCL_DefglobalValueFo_rm(theEnv,logicalName,(Defglobal *) constructPtr);
         CL_WriteString(theEnv,logicalName,"\n");
        }

      /*===================================*/
      /* If we're only listing the globals */
      /* for one module, then return.      */
      /*===================================*/

      if (! allModules) return;
     }
  }

/*****************************************************/
/* PrintCL_DefglobalValueFo_rm: Prints the value fo_rm of */
/*   a defglobal (the current value). For example,   */
/*   ?*x* = 3                                        */
/*****************************************************/
static void PrintCL_DefglobalValueFo_rm(
  Environment *theEnv,
  const char *logicalName,
  Defglobal *theGlobal)
  {
   CL_WriteString(theEnv,logicalName,"?*");
   CL_WriteString(theEnv,logicalName,theGlobal->header.name->contents);
   CL_WriteString(theEnv,logicalName,"* = ");
   CL_WriteCLIPSValue(theEnv,logicalName,&theGlobal->current);
  }

#endif /* DEBUGGING_FUNCTIONS */

#endif /* DEFGLOBAL_CONSTRUCT */

