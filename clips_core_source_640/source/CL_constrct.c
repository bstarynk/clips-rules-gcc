   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  11/10/17             */
   /*                                                     */
   /*                  CONSTRUCT MODULE                   */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides basic functionality for creating new    */
/*   types of constructs, saving constructs to a file, and   */
/*   adding new functionality to the clear and reset         */
/*   commands.                                               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added environment parameter to CL_GenClose.       */
/*            Added environment parameter to CL_GenOpen.        */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Changed garbage collection algorithm.          */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added code for capturing errors/warnings       */
/*            (EnvCL_SetParserErrorCallback).                   */
/*                                                           */
/*            Fixed issue with save function when multiple   */
/*            defmodules exist.                              */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Fixed linkage issue when BLOAD_ONLY compiler   */
/*            flag is set to 1.                              */
/*                                                           */
/*            Added code to prevent a clear command from     */
/*            being executed during fact assertions via      */
/*            Increment/DecrementCL_ClearReadyLocks API.        */
/*                                                           */
/*            Added code to keep track of pointers to        */
/*            constructs that are contained externally to    */
/*            to constructs, DanglingConstructs.             */
/*                                                           */
/*      6.31: Error flags reset before CL_Clear processed when  */
/*            called from embedded controller.               */
/*                                                           */
/*      6.40: Added Env prefix to CL_GetCL_HaltExecution and       */
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
/*            Modified EnvCL_Clear to return completion status. */
/*                                                           */
/*            Compilation watch flag defaults to off.        */
/*                                                           */
/*            File name/line count displayed for errors      */
/*            and warnings during load command.              */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <string.h>

#include "setup.h"

#include "argacces.h"
#include "commline.h"
#include "constant.h"
#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "miscfun.h"
#include "moduldef.h"
#include "modulutl.h"
#include "multifld.h"
#include "prcdrfun.h"
#include "prcdrpsr.h"
#include "prntutil.h"
#include "router.h"
#include "ruledef.h"
#include "scanner.h"
#include "sysdep.h"
#include "utility.h"
#include "watch.h"

#include "constrct.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                        DeallocateConstructData(Environment *);

/**************************************************/
/* CL_InitializeConstructData: Allocates environment */
/*    data for constructs.                        */
/**************************************************/
void CL_InitializeConstructData(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,CONSTRUCT_DATA,sizeof(struct constructData),DeallocateConstructData);
  }

/****************************************************/
/* DeallocateConstructData: Deallocates environment */
/*    data for constructs.                          */
/****************************************************/
static void DeallocateConstructData(
  Environment *theEnv)
  {
   Construct *tmpPtr, *nextPtr;

#if (! RUN_TIME) && (! BLOAD_ONLY)
   CL_DeallocateCL_SaveCallList(theEnv,ConstructData(theEnv)->ListOfCL_SaveFunctions);
#endif
   CL_DeallocateVoidCallList(theEnv,ConstructData(theEnv)->ListOfCL_ResetFunctions);
   CL_DeallocateVoidCallList(theEnv,ConstructData(theEnv)->ListOfCL_ClearFunctions);
   CL_DeallocateBoolCallList(theEnv,ConstructData(theEnv)->ListOfCL_ClearReadyFunctions);

#if (! RUN_TIME) && (! BLOAD_ONLY)
   if (ConstructData(theEnv)->ErrorString != NULL)
     { CL_genfree(theEnv,ConstructData(theEnv)->ErrorString,sizeof(ConstructData(theEnv)->ErrorString) + 1); }

   if (ConstructData(theEnv)->WarningString != NULL)
     { CL_genfree(theEnv,ConstructData(theEnv)->WarningString,sizeof(ConstructData(theEnv)->WarningString) + 1); }

   ConstructData(theEnv)->ErrorString = NULL;
   ConstructData(theEnv)->WarningString = NULL;

   CL_SetParsingFileName(theEnv,NULL);
   CL_SetWarningFileName(theEnv,NULL);
   CL_SetErrorFileName(theEnv,NULL);
#endif

   tmpPtr = ConstructData(theEnv)->ListOfConstructs;
   while (tmpPtr != NULL)
     {
      nextPtr = tmpPtr->next;
      rtn_struct(theEnv,construct,tmpPtr);
      tmpPtr = nextPtr;
     }
  }

#if (! RUN_TIME) && (! BLOAD_ONLY)

/***********************************************/
/* CL_SetParserErrorCallback: Allows the function */
/*   which is called when a construct parsing  */
/*    error occurs to be changed.              */
/***********************************************/
ParserErrorFunction *CL_SetParserErrorCallback(
   Environment *theEnv,
   ParserErrorFunction *functionPtr,
   void *context)
  {
   ParserErrorFunction *tmpPtr;

   tmpPtr = ConstructData(theEnv)->ParserErrorCallback;
   ConstructData(theEnv)->ParserErrorCallback = functionPtr;
   ConstructData(theEnv)->ParserErrorContext = context;
   return tmpPtr;
  }

/*************************************************/
/* CL_FindConstruct: DeteCL_rmines whether a construct */
/*   type is in the ListOfConstructs.            */
/*************************************************/
Construct *CL_FindConstruct(
  Environment *theEnv,
  const char *name)
  {
   Construct *currentPtr;

   for (currentPtr = ConstructData(theEnv)->ListOfConstructs;
        currentPtr != NULL;
        currentPtr = currentPtr->next)
     {
      if (strcmp(name,currentPtr->constructName) == 0)
        { return currentPtr; }
     }

   return NULL;
  }

/***********************************************************/
/* CL_RemoveConstruct: Removes a construct and its associated */
/*   parsing function from the ListOfConstructs. Returns   */
/*   true if the construct type was removed, otherwise     */
/*   false.                                                */
/***********************************************************/
bool CL_RemoveConstruct(
  Environment *theEnv,
  const char *name)
  {
   Construct *currentPtr, *lastPtr = NULL;

   for (currentPtr = ConstructData(theEnv)->ListOfConstructs;
        currentPtr != NULL;
        currentPtr = currentPtr->next)
     {
      if (strcmp(name,currentPtr->constructName) == 0)
        {
         if (lastPtr == NULL)
           { ConstructData(theEnv)->ListOfConstructs = currentPtr->next; }
         else
           { lastPtr->next = currentPtr->next; }
         rtn_struct(theEnv,construct,currentPtr);
         return true;
        }

      lastPtr = currentPtr;
     }

   return false;
  }

/************************************************/
/* CL_Save: C access routine for the save command. */
/************************************************/
bool CL_Save(
  Environment *theEnv,
  const char *fileName)
  {
   struct saveCL_CallFunctionItem *saveFunction;
   FILE *filePtr;
   Defmodule *defmodulePtr;
   bool updated = false;
   bool unvisited = true;

   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/
   
   if (CL_EvaluationData(theEnv)->CurrentExpression == NULL)
     { CL_ResetErrorFlags(theEnv); }

   /*=====================*/
   /* Open the save file. */
   /*=====================*/

   if ((filePtr = CL_GenOpen(theEnv,fileName,"w")) == NULL)
     { return false; }

   /*===========================*/
   /* Bypass the router system. */
   /*===========================*/

   SetFastCL_Save(theEnv,filePtr);

   /*================================*/
   /* Mark all modules as unvisited. */
   /*================================*/

   CL_MarkModulesAsUnvisited(theEnv);

   /*===============================================*/
   /* CL_Save the constructs. Repeatedly loop over the */
   /* modules until each module has been save.      */
   /*===============================================*/

   while (unvisited)
     {
      unvisited = false;
      updated = false;

      for (defmodulePtr = CL_GetNextDefmodule(theEnv,NULL);
           defmodulePtr != NULL;
           defmodulePtr = CL_GetNextDefmodule(theEnv,defmodulePtr))
        {
         /*=================================================================*/
         /* We only want to save a module if all of the modules it imports  */
         /* from have already been saved. Since there can't be circular     */
         /* dependencies in imported modules, this should save the modules  */
         /* that don't import anything first and then work back from those. */
         /*=================================================================*/

         if (defmodulePtr->visitedFlag)
           { /* Module has already been saved. */ }
         else if (CL_AllImportedModulesVisited(theEnv,defmodulePtr))
           {
            for (saveFunction = ConstructData(theEnv)->ListOfCL_SaveFunctions;
                 saveFunction != NULL;
                 saveFunction = saveFunction->next)
              { (*saveFunction->func)(theEnv,defmodulePtr,(char *) filePtr,saveFunction->context); }

            updated = true;
            defmodulePtr->visitedFlag = true;
           }
         else
           { unvisited = true; }
        }

      /*=====================================================================*/
      /* At least one module should be saved in every pass. If all have been */
      /* visited/saved, then both flags will be false. If all reCL_maining      */
      /* unvisited/unsaved modules were visited/saved, then unvisited will   */
      /* be false and updated will be true. If some, but not all, reCL_maining  */
      /* unvisited/unsaved modules are visited/saved, then  unvisited will   */
      /* be true and updated will be true. This leaves the case where there  */
      /* are reCL_maining unvisited/unsaved modules, but none were              */
      /* visited/saved: unvisited is true and updated is false.              */
      /*=====================================================================*/

      if (unvisited && (! updated))
        {
         CL_SystemError(theEnv,"CONSTRCT",2);
         break;
        }
     }

   /*======================*/
   /* Close the save file. */
   /*======================*/

   CL_GenClose(theEnv,filePtr);

   /*===========================*/
   /* Remove the router bypass. */
   /*===========================*/

   SetFastCL_Save(theEnv,NULL);

   /*=========================*/
   /* Return true to indicate */
   /* successful completion.  */
   /*=========================*/

   return true;
  }

/*******************************************************/
/* CL_RemoveCL_SaveFunction: Removes a function from the     */
/*   ListOfCL_SaveFunctions. Returns true if the function */
/*   was successfully removed, otherwise false.        */
/*******************************************************/
bool CL_RemoveCL_SaveFunction(
  Environment *theEnv,
  const char *name)
  {
   bool found;

   ConstructData(theEnv)->ListOfCL_SaveFunctions =
     CL_RemoveCL_SaveFunctionFromCallList(theEnv,name,ConstructData(theEnv)->ListOfCL_SaveFunctions,&found);

   if (found) return true;

   return false;
  }

/**********************************/
/* CL_SetCompilationsCL_Watch: Sets the */
/*   value of CL_WatchCompilations.  */
/**********************************/
void CL_SetCompilationsCL_Watch(
  Environment *theEnv,
  bool value)
  {
   ConstructData(theEnv)->CL_WatchCompilations = value;
  }

/*************************************/
/* CL_GetCompilationsCL_Watch: Returns the */
/*   value of CL_WatchCompilations.     */
/*************************************/
bool CL_GetCompilationsCL_Watch(
  Environment *theEnv)
  {
   return ConstructData(theEnv)->CL_WatchCompilations;
  }

/**********************************/
/* SetPrintWhileCL_Loading: Sets the */
/*   value of PrintWhileCL_Loading.  */
/**********************************/
void SetPrintWhileCL_Loading(
  Environment *theEnv,
  bool value)
  {
   ConstructData(theEnv)->PrintWhileCL_Loading = value;
  }

/*************************************/
/* CL_GetPrintWhileCL_Loading: Returns the */
/*   value of PrintWhileCL_Loading.     */
/*************************************/
bool CL_GetPrintWhileCL_Loading(
  Environment *theEnv)
  {
   return(ConstructData(theEnv)->PrintWhileCL_Loading);
  }

/*******************************/
/* SetCL_LoadInProgress: Sets the */
/*   value of CL_LoadInProgress.  */
/*******************************/
void SetCL_LoadInProgress(
  Environment *theEnv,
  bool value)
  {
   ConstructData(theEnv)->CL_LoadInProgress = value;
  }

/**********************************/
/* CL_GetCL_LoadInProgress: Returns the */
/*   value of CL_LoadInProgress.     */
/**********************************/
bool CL_GetCL_LoadInProgress(
  Environment *theEnv)
  {
   return(ConstructData(theEnv)->CL_LoadInProgress);
  }

#endif

/*************************************/
/* CL_InitializeConstructs: Initializes */
/*   the Construct Manager.          */
/*************************************/
void CL_InitializeConstructs(
  Environment *theEnv)
  {
#if (! RUN_TIME)
   CL_AddUDF(theEnv,"clear","v",0,0,NULL,CL_ClearCommand,"CL_ClearCommand",NULL);
   CL_AddUDF(theEnv,"reset","v",0,0,NULL,CL_ResetCommand,"CL_ResetCommand",NULL);

#if DEBUGGING_FUNCTIONS && (! BLOAD_ONLY)
   CL_AddCL_WatchItem(theEnv,"compilations",0,&ConstructData(theEnv)->CL_WatchCompilations,30,NULL,NULL);
#endif
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
  }

/**************************************/
/* CL_ClearCommand: H/L access routine   */
/*   for the clear command.           */
/**************************************/
void CL_ClearCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   CL_Clear(theEnv);
  }

/**************************************/
/* CL_ResetCommand: H/L access routine   */
/*   for the reset command.           */
/**************************************/
void CL_ResetCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   CL_Reset(theEnv);
  }

/****************************/
/* CL_Reset: C access routine  */
/*   for the reset command. */
/****************************/
void CL_Reset(
  Environment *theEnv)
  {
   struct voidCL_CallFunctionItem *resetPtr;
   GCBlock gcb;

   /*=====================================*/
   /* The reset command can't be executed */
   /* while a reset is in progress.       */
   /*=====================================*/

   if (ConstructData(theEnv)->CL_ResetInProgress) return;

   ConstructData(theEnv)->CL_ResetInProgress = true;
   ConstructData(theEnv)->CL_ResetReadyInProgress = true;
   
   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/
   
   if (CL_EvaluationData(theEnv)->CurrentExpression == NULL)
     { CL_ResetErrorFlags(theEnv); }
   CL_SetErrorValue(theEnv,NULL);
    
   /*========================================*/
   /* Set up the frame for tracking garbage. */
   /*========================================*/

   CL_GCBlockStart(theEnv,&gcb);

   /*=======================================================*/
   /* Call the before reset function to deteCL_rmine if the    */
   /* reset should continue. [Used by the some of the       */
   /* windowed interfaces to query the user whether a       */
   /* reset should proceed with activations on the agenda.] */
   /*=======================================================*/

   if ((ConstructData(theEnv)->BeforeCL_ResetCallback != NULL) ?
       ((*ConstructData(theEnv)->BeforeCL_ResetCallback)(theEnv) == false) : false)
     {
      ConstructData(theEnv)->CL_ResetReadyInProgress = false;
      ConstructData(theEnv)->CL_ResetInProgress = false;
      return;
     }
   ConstructData(theEnv)->CL_ResetReadyInProgress = false;

   /*===========================*/
   /* Call each reset function. */
   /*===========================*/

   for (resetPtr = ConstructData(theEnv)->ListOfCL_ResetFunctions;
        (resetPtr != NULL) && (CL_GetCL_HaltExecution(theEnv) == false);
        resetPtr = resetPtr->next)
     { (*resetPtr->func)(theEnv,resetPtr->context); }

   /*============================================*/
   /* Set the current module to the MAIN module. */
   /*============================================*/

   CL_SetCurrentModule(theEnv,CL_FindDefmodule(theEnv,"MAIN"));

   /*===========================================*/
   /* PerfoCL_rm periodic cleanup if the reset was */
   /* issued from an embedded controller.       */
   /*===========================================*/

   CL_GCBlockEnd(theEnv,&gcb);
   CL_CallPeriodicTasks(theEnv);

   /*===================================*/
   /* A reset is no longer in progress. */
   /*===================================*/

   ConstructData(theEnv)->CL_ResetInProgress = false;
  }

/************************************/
/* SetBeforeCL_ResetFunction: Sets the */
/*  value of BeforeCL_ResetFunction.   */
/************************************/
BeforeCL_ResetFunction *SetBeforeCL_ResetFunction(
  Environment *theEnv,
  BeforeCL_ResetFunction *theFunction)
  {
   BeforeCL_ResetFunction *tempFunction;

   tempFunction = ConstructData(theEnv)->BeforeCL_ResetCallback;
   ConstructData(theEnv)->BeforeCL_ResetCallback = theFunction;
   return tempFunction;
  }

/*************************************/
/* CL_AddCL_ResetFunction: Adds a function */
/*   to ListOfCL_ResetFunctions.        */
/*************************************/
bool CL_AddCL_ResetFunction(
  Environment *theEnv,
  const char *name,
  VoidCL_CallFunction *functionPtr,
  int priority,
  void *context)
  {
   ConstructData(theEnv)->ListOfCL_ResetFunctions =
      CL_AddCL_VoidFunctionToCallList(theEnv,name,priority,functionPtr,
                                ConstructData(theEnv)->ListOfCL_ResetFunctions,context);
   return true;
  }

/*******************************************/
/* CL_RemoveCL_ResetFunction: Removes a function */
/*   from the ListOfCL_ResetFunctions.        */
/*******************************************/
bool CL_RemoveCL_ResetFunction(
  Environment *theEnv,
  const char *name)
  {
   bool found;

   ConstructData(theEnv)->ListOfCL_ResetFunctions =
      CL_RemoveCL_VoidFunctionFromCallList(theEnv,name,ConstructData(theEnv)->ListOfCL_ResetFunctions,&found);

   return found;
  }

/****************************************/
/* IncrementCL_ClearReadyLocks: Increments */
/*   the number of clear ready locks.   */
/****************************************/
void IncrementCL_ClearReadyLocks(
  Environment *theEnv)
  {
   ConstructData(theEnv)->CL_ClearReadyLocks++;
  }

/*******************************************/
/* DecrementCL_ClearReadyLocks: Decrements    */
/*   the number of clear locks.            */
/*******************************************/
void DecrementCL_ClearReadyLocks(
  Environment *theEnv)
  {
   if (ConstructData(theEnv)->CL_ClearReadyLocks > 0)
     { ConstructData(theEnv)->CL_ClearReadyLocks--; }
  }

/**************************************************/
/* CL_Clear: C access routine for the clear command. */
/**************************************************/
bool CL_Clear(
  Environment *theEnv)
  {
   struct voidCL_CallFunctionItem *theFunction;
   GCBlock gcb;

   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/

   if (CL_EvaluationData(theEnv)->CurrentExpression == NULL)
     { CL_ResetErrorFlags(theEnv); }
   CL_SetErrorValue(theEnv,NULL);
   
   /*===================================*/
   /* DeteCL_rmine if a clear is possible. */
   /*===================================*/

   ConstructData(theEnv)->CL_ClearReadyInProgress = true;
   if ((ConstructData(theEnv)->CL_ClearReadyLocks > 0) ||
       (ConstructData(theEnv)->DanglingConstructs > 0) ||
       (CL_ClearReady(theEnv) == false))
     {
      CL_PrintErrorID(theEnv,"CONSTRCT",1,false);
      CL_WriteString(theEnv,STDERR,"Some constructs are still in use. CL_Clear cannot continue.\n");
      ConstructData(theEnv)->CL_ClearReadyInProgress = false;
      return false;
     }
   ConstructData(theEnv)->CL_ClearReadyInProgress = false;

   /*========================================*/
   /* Set up the frame for tracking garbage. */
   /*========================================*/
   
   CL_GCBlockStart(theEnv,&gcb);

   /*===========================*/
   /* Call all clear functions. */
   /*===========================*/

   ConstructData(theEnv)->CL_ClearInProgress = true;

   for (theFunction = ConstructData(theEnv)->ListOfCL_ClearFunctions;
        theFunction != NULL;
        theFunction = theFunction->next)
     { (*theFunction->func)(theEnv,theFunction->context); }

   /*================================*/
   /* Restore the old garbage frame. */
   /*================================*/
   
   CL_GCBlockEnd(theEnv,&gcb);
   CL_CallPeriodicTasks(theEnv);

   /*===========================*/
   /* CL_Clear has been completed. */
   /*===========================*/

   ConstructData(theEnv)->CL_ClearInProgress = false;

#if DEFRULE_CONSTRUCT
   if ((DefruleData(theEnv)->RightPrimeJoins != NULL) ||
       (DefruleData(theEnv)->LeftPrimeJoins != NULL))
     { CL_SystemError(theEnv,"CONSTRCT",1); }
#endif

   /*============================*/
   /* PerfoCL_rm reset after clear. */
   /*============================*/

   CL_Reset(theEnv);

   return true;
  }

/*********************************************************/
/* CL_ClearReady: Returns true if a clear can be perfoCL_rmed, */
/*   otherwise false. Note that this is destructively    */
/*   deteCL_rmined (e.g. facts will be deleted as part of   */
/*   the deteCL_rmination).                                 */
/*********************************************************/
bool CL_ClearReady(
  Environment *theEnv)
  {
   struct boolCL_CallFunctionItem *theFunction;

   for (theFunction = ConstructData(theEnv)->ListOfCL_ClearReadyFunctions;
        theFunction != NULL;
        theFunction = theFunction->next)
     {
      if ((*theFunction->func)(theEnv,theFunction->context) == false)
        { return false; }
     }

   return true;
  }

/******************************************/
/* CL_AddCL_ClearReadyFunction: Adds a function */
/*   to ListOfCL_ClearReadyFunctions.        */
/******************************************/
bool CL_AddCL_ClearReadyFunction(
  Environment *theEnv,
  const char *name,
  BoolCL_CallFunction *functionPtr,
  int priority,
  void *context)
  {
   ConstructData(theEnv)->ListOfCL_ClearReadyFunctions =
     CL_AddBoolFunctionToCallList(theEnv,name,priority,functionPtr,
                               ConstructData(theEnv)->ListOfCL_ClearReadyFunctions,context);
   return true;
  }

/************************************************/
/* RemoveCL_ClearReadyFunction: Removes a function */
/*   from the ListOfCL_ClearReadyFunctions.        */
/************************************************/
bool RemoveCL_ClearReadyFunction(
  Environment *theEnv,
  const char *name)
  {
   bool found;

   ConstructData(theEnv)->ListOfCL_ClearReadyFunctions =
      CL_RemoveBoolFunctionFromCallList(theEnv,name,ConstructData(theEnv)->ListOfCL_ClearReadyFunctions,&found);

   if (found) return true;

   return false;
  }

/*************************************/
/* CL_AddCL_ClearFunction: Adds a function */
/*   to ListOfCL_ClearFunctions.        */
/*************************************/
bool CL_AddCL_ClearFunction(
  Environment *theEnv,
  const char *name,
  VoidCL_CallFunction *functionPtr,
  int priority,
  void *context)
  {
   ConstructData(theEnv)->ListOfCL_ClearFunctions =
      CL_AddCL_VoidFunctionToCallList(theEnv,name,priority,functionPtr,
                                ConstructData(theEnv)->ListOfCL_ClearFunctions,context);
   return true;
  }

/*******************************************/
/* RemoveCL_ClearFunction: Removes a function */
/*    from the ListOfCL_ClearFunctions.       */
/*******************************************/
bool RemoveCL_ClearFunction(
  Environment *theEnv,
  const char *name)
  {
   bool found;

   ConstructData(theEnv)->ListOfCL_ClearFunctions =
     CL_RemoveCL_VoidFunctionFromCallList(theEnv,name,ConstructData(theEnv)->ListOfCL_ClearFunctions,&found);

   if (found) return true;

   return false;
  }

/********************************************/
/* CL_ExecutingConstruct: Returns true if a    */
/*   construct is currently being executed, */
/*   otherwise false.                       */
/********************************************/
bool CL_ExecutingConstruct(
  Environment *theEnv)
  {
   return ConstructData(theEnv)->Executing;
  }

/********************************************/
/* SetCL_ExecutingConstruct: Sets the value of */
/*   the executing variable indicating that */
/*   actions such as reset, clear, etc      */
/*   should not be perfoCL_rmed.               */
/********************************************/
void SetCL_ExecutingConstruct(
  Environment *theEnv,
  bool value)
  {
   ConstructData(theEnv)->Executing = value;
  }

/*******************************************************/
/* CL_DeinstallConstructHeader: Decrements the busy count */
/*   of a construct name and frees its pretty print    */
/*   representation string (both of which are stored   */
/*   in the generic construct header).                 */
/*******************************************************/
void CL_DeinstallConstructHeader(
  Environment *theEnv,
  ConstructHeader *theHeader)
  {
   CL_ReleaseLexeme(theEnv,theHeader->name);
   if (theHeader->ppFoCL_rm != NULL)
     {
      CL_rm(theEnv,(void *) theHeader->ppFoCL_rm,
         sizeof(char) * (strlen(theHeader->ppFoCL_rm) + 1));
      theHeader->ppFoCL_rm = NULL;
     }

   if (theHeader->usrData != NULL)
     {
      CL_ClearUserDataList(theEnv,theHeader->usrData);
      theHeader->usrData = NULL;
     }
  }

/**************************************************/
/* CL_DestroyConstructHeader: Frees the pretty print */
/*   representation string and user data (both of */
/*   which are stored in the generic construct    */
/*   header).                                     */
/**************************************************/
void CL_DestroyConstructHeader(
  Environment *theEnv,
  ConstructHeader *theHeader)
  {
   if (theHeader->ppFoCL_rm != NULL)
     {
      CL_rm(theEnv,(void *) theHeader->ppFoCL_rm,
         sizeof(char) * (strlen(theHeader->ppFoCL_rm) + 1));
      theHeader->ppFoCL_rm = NULL;
     }

   if (theHeader->usrData != NULL)
     {
      CL_ClearUserDataList(theEnv,theHeader->usrData);
      theHeader->usrData = NULL;
     }
  }

/*****************************************************/
/* CL_AddConstruct: Adds a construct and its associated */
/*   parsing function to the ListOfConstructs.       */
/*****************************************************/
Construct *CL_AddConstruct(
  Environment *theEnv,
  const char *name,
  const char *pluralName,
  bool (*parseFunction)(Environment *,const char *),
  CL_FindConstructFunction *findFunction,
  CLIPSLexeme *(*getConstructNameFunction)(ConstructHeader *),
  const char *(*getPPFoCL_rmFunction)(ConstructHeader *),
  struct defmoduleItemHeader *(*getModuleItemFunction)(ConstructHeader *),
  GetNextConstructFunction *getNextItemFunction,
  void (*setNextItemFunction)(ConstructHeader *,ConstructHeader *),
  IsConstructDeletableFunction *isConstructDeletableFunction,
  DeleteConstructFunction *deleteFunction,
  FreeConstructFunction *freeFunction)
  {
   Construct *newPtr;

   /*=============================*/
   /* Allocate and initialize the */
   /* construct data structure.   */
   /*=============================*/

   newPtr = get_struct(theEnv,construct);

   newPtr->constructName = name;
   newPtr->pluralName = pluralName;
   newPtr->parseFunction = parseFunction;
   newPtr->findFunction = findFunction;
   newPtr->getConstructNameFunction = getConstructNameFunction;
   newPtr->getPPFoCL_rmFunction = getPPFoCL_rmFunction;
   newPtr->getModuleItemFunction = getModuleItemFunction;
   newPtr->getNextItemFunction = getNextItemFunction;
   newPtr->setNextItemFunction = setNextItemFunction;
   newPtr->isConstructDeletableFunction = isConstructDeletableFunction;
   newPtr->deleteFunction = deleteFunction;
   newPtr->freeFunction = freeFunction;

   /*===============================*/
   /* Add the construct to the list */
   /* of constructs and return it.  */
   /*===============================*/

   newPtr->next = ConstructData(theEnv)->ListOfConstructs;
   ConstructData(theEnv)->ListOfConstructs = newPtr;
   return(newPtr);
  }

/************************************/
/* CL_AddCL_SaveFunction: Adds a function */
/*   to the ListOfCL_SaveFunctions.    */
/************************************/
bool CL_AddCL_SaveFunction(
  Environment *theEnv,
  const char *name,
  CL_SaveCL_CallFunction *functionPtr,
  int priority,
  void *context)
  {
#if (! RUN_TIME) && (! BLOAD_ONLY)
   ConstructData(theEnv)->ListOfCL_SaveFunctions =
     CL_AddCL_SaveFunctionToCallList(theEnv,name,priority,
                           functionPtr,
                           ConstructData(theEnv)->ListOfCL_SaveFunctions,context);
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif

   return true;
  }

/**********************************************************/
/* CL_AddCL_SaveFunctionToCallList: Adds a function to a list   */
/*   of functions which are called to perfoCL_rm certain     */
/*   operations (e.g. clear, reset, and bload functions). */
/**********************************************************/
CL_SaveCL_CallFunctionItem *CL_AddCL_SaveFunctionToCallList(
  Environment *theEnv,
  const char *name,
  int priority,
  CL_SaveCL_CallFunction *func,
  struct saveCL_CallFunctionItem *head,
  void *context)
  {
   struct saveCL_CallFunctionItem *newPtr, *currentPtr, *lastPtr = NULL;
   char  *nameCopy;

   newPtr = get_struct(theEnv,saveCL_CallFunctionItem);

   nameCopy = (char *) CL_genalloc(theEnv,strlen(name) + 1);
   CL_genstrcpy(nameCopy,name);
   newPtr->name = nameCopy;

   newPtr->func = func;
   newPtr->priority = priority;
   newPtr->context = context;

   if (head == NULL)
     {
      newPtr->next = NULL;
      return(newPtr);
     }

   currentPtr = head;
   while ((currentPtr != NULL) ? (priority < currentPtr->priority) : false)
     {
      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
     }

   if (lastPtr == NULL)
     {
      newPtr->next = head;
      head = newPtr;
     }
   else
     {
      newPtr->next = currentPtr;
      lastPtr->next = newPtr;
     }

   return(head);
  }

/******************************************************************/
/* CL_RemoveCL_SaveFunctionFromCallList: Removes a function from a list */
/*   of functions which are called to perfoCL_rm certain operations  */
/*   (e.g. clear, reset, and bload functions).                    */
/******************************************************************/
struct saveCL_CallFunctionItem *CL_RemoveCL_SaveFunctionFromCallList(
  Environment *theEnv,
  const char *name,
  struct saveCL_CallFunctionItem *head,
  bool *found)
  {
   struct saveCL_CallFunctionItem *currentPtr, *lastPtr;

   *found = false;
   lastPtr = NULL;
   currentPtr = head;

   while (currentPtr != NULL)
     {
      if (strcmp(name,currentPtr->name) == 0)
        {
         *found = true;
         if (lastPtr == NULL)
           { head = currentPtr->next; }
         else
           { lastPtr->next = currentPtr->next; }

         CL_genfree(theEnv,(void *) currentPtr->name,strlen(currentPtr->name) + 1);
         rtn_struct(theEnv,saveCL_CallFunctionItem,currentPtr);
         return head;
        }

      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
     }

   return head;
  }

/*************************************************************/
/* CL_DeallocateCL_SaveCallList: Removes all functions from a list */
/*   of functions which are called to perfoCL_rm certain        */
/*   operations (e.g. clear, reset, and bload functions).    */
/*************************************************************/
void CL_DeallocateCL_SaveCallList(
  Environment *theEnv,
  struct saveCL_CallFunctionItem *theList)
  {
   struct saveCL_CallFunctionItem *tmpPtr, *nextPtr;

   tmpPtr = theList;
   while (tmpPtr != NULL)
     {
      nextPtr = tmpPtr->next;
      CL_genfree(theEnv,(void *) tmpPtr->name,strlen(tmpPtr->name) + 1);
      rtn_struct(theEnv,saveCL_CallFunctionItem,tmpPtr);
      tmpPtr = nextPtr;
     }
  }

