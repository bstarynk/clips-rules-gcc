   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/02/18             */
   /*                                                     */
   /*              CONSTRUCT COMMANDS MODULE              */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains generic routines for deleting, pretty   */
/*   printing, finding, obtaining module infoCL_rmation,        */
/*   obtaining lists of constructs, listing constructs, and  */
/*   manipulation routines.                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Modified CL_GetConstructList to remove buffer     */
/*            overflow problem with large construct/module   */
/*            names. DR0858                                  */
/*                                                           */
/*            Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*            Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Corrected an error when compiling as a C++     */
/*            file. DR0868                                   */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added CL_ConstructsDeletable function.            */
/*                                                           */
/*      6.30: Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Change find construct functionality so that    */
/*            imported modules are search when locating a    */
/*            named construct.                               */
/*                                                           */
/*      6.31: Fixed use after free issue for deallocation    */
/*            functions passed to CL_DoForAllConstructs.        */
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
/*            UDF redesign.                                  */
/*                                                           */
/*            Pretty print functions accept optional logical */
/*            name argument.                                 */
/*                                                           */
/*************************************************************/

#include <string.h>

#include "setup.h"

#include "constant.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "memalloc.h"
#include "moduldef.h"
#include "argacces.h"
#include "multifld.h"
#include "modulutl.h"
#include "prntutil.h"
#include "router.h"
#include "utility.h"
#include "commline.h"
#include "sysdep.h"

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "bload.h"
#endif

#if (! BLOAD_ONLY) && (! RUN_TIME)
#include "cstrcpsr.h"
#endif

#include "cstrccom.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if DEBUGGING_FUNCTIONS
   static void                    ConstructPrintCL_Watch(Environment *,const char *,Construct *,
                                                      ConstructHeader *,
                                                      ConstructGetCL_WatchFunction *);
   static bool                    ConstructCL_WatchSupport(Environment *,Construct *,const char *,
                                                        const char *,Expression *,bool,
                                                        bool,ConstructGetCL_WatchFunction *,
                                                        ConstructSetCL_WatchFunction *);
#endif

#if (! RUN_TIME)

/************************************/
/* CL_AddConstructToModule: Adds a     */
/* construct to the current module. */
/************************************/
void CL_AddConstructToModule(
  ConstructHeader *theConstruct)
  {
   if (theConstruct->whichModule->lastItem == NULL)
     { theConstruct->whichModule->firstItem = theConstruct; }
   else
     { theConstruct->whichModule->lastItem->next = theConstruct; }

   theConstruct->whichModule->lastItem = theConstruct;
   theConstruct->next = NULL;
  }

#endif /* (! RUN_TIME) */

/****************************************************/
/* CL_DeleteNamedConstruct: Generic driver routine for */
/*   deleting a specific construct from a module.   */
/****************************************************/
bool CL_DeleteNamedConstruct(
  Environment *theEnv,
  const char *constructName,
  Construct *constructClass)
  {
#if (! BLOAD_ONLY)
   ConstructHeader *constructPtr;

   /*=============================*/
   /* Constructs can't be deleted */
   /* while a bload is in effect. */
   /*=============================*/

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
   if (CL_Bloaded(theEnv) == true) return false;
#endif

   /*===============================*/
   /* Look for the named construct. */
   /*===============================*/

   constructPtr = (*constructClass->findFunction)(theEnv,constructName);

   /*========================================*/
   /* If the construct was found, delete it. */
   /*========================================*/

   if (constructPtr != NULL)
     { return (*constructClass->deleteFunction)(constructPtr,theEnv); }

   /*========================================*/
   /* If the construct wasn't found, but the */
   /* special symbol * was used, then delete */
   /* all constructs of the specified type.  */
   /*========================================*/

   if (strcmp("*",constructName) == 0)
     {
      (*constructClass->deleteFunction)(NULL,theEnv);
      return true;
     }

   /*===============================*/
   /* Otherwise, return false to    */
   /* indicate no deletion occured. */
   /*===============================*/

   return false;
#else
#if MAC_XCD
#pragma unused(theEnv,constructName,constructClass)
#endif
   return false;
#endif
  }

/********************************************************/
/* CL_FindNamedConstructInModuleOrImports: Generic routine */
/*   for searching for a specified construct.           */
/********************************************************/
ConstructHeader *CL_FindNamedConstructInModuleOrImports(
  Environment *theEnv,
  const char *constructName,
  Construct *constructClass)
  {
   ConstructHeader *theConstruct;
   unsigned int count;

   /*================================================*/
   /* First look in the current or specified module. */
   /*================================================*/

   theConstruct = CL_FindNamedConstructInModule(theEnv,constructName,constructClass);
   if (theConstruct != NULL) return theConstruct;

   /*=====================================*/
   /* If there's a module specifier, then */
   /* the construct does not exist.       */
   /*=====================================*/

   if (CL_FindModuleSeparator(constructName))
     { return NULL; }

   /*========================================*/
   /* Otherwise, search in imported modules. */
   /*========================================*/

   theConstruct = CL_FindImportedConstruct(theEnv,constructClass->constructName,NULL,
                                        constructName,&count,true,NULL);

   if (count > 1)
     {
      CL_AmbiguousReferenceErrorMessage(theEnv,constructClass->constructName,constructName);
      return NULL;
     }

   return theConstruct;
  }

/***********************************************/
/* CL_FindNamedConstructInModule: Generic routine */
/*   for searching for a specified construct.  */
/***********************************************/
ConstructHeader *CL_FindNamedConstructInModule(
  Environment *theEnv,
  const char *constructName,
  Construct *constructClass)
  {
   ConstructHeader *theConstruct;
   CLIPSLexeme *findValue;

   /*==========================*/
   /* CL_Save the current module. */
   /*==========================*/

   CL_SaveCurrentModule(theEnv);

   /*=========================================================*/
   /* Extract the construct name. If a module was specified,  */
   /* then CL_ExtractModuleAndConstructName will set the current */
   /* module to the module specified in the name.             */
   /*=========================================================*/

   constructName = CL_ExtractModuleAndConstructName(theEnv,constructName);

   /*=================================================*/
   /* If a valid construct name couldn't be extracted */
   /* or the construct name isn't in the symbol table */
   /* (which means the construct doesn't exist), then */
   /* return NULL to indicate the specified construct */
   /* couldn't be found.                              */
   /*=================================================*/

   if ((constructName == NULL) ?
       true :
       ((findValue = CL_FindSymbolHN(theEnv,constructName,SYMBOL_BIT)) == NULL))
     {
      CL_RestoreCurrentModule(theEnv);
      return NULL;
     }

   /*===============================================*/
   /* If we find the symbol for the construct name, */
   /* but it has a count of 0, then it can't be for */
   /* a construct that's currently defined.         */
   /*===============================================*/

   if (findValue->count == 0)
     {
      CL_RestoreCurrentModule(theEnv);
      return NULL;
     }

   /*===============================================*/
   /* Loop through every construct of the specified */
   /* class in the current module checking to see   */
   /* if the construct's name matches the construct */
   /* being sought. If found, restore the current   */
   /* module and return a pointer to the construct. */
   /*===============================================*/

   for (theConstruct = (*constructClass->getNextItemFunction)(theEnv,NULL);
        theConstruct != NULL;
        theConstruct = (*constructClass->getNextItemFunction)(theEnv,theConstruct))
     {
      if (findValue == (*constructClass->getConstructNameFunction)(theConstruct))
        {
         CL_RestoreCurrentModule(theEnv);
         return theConstruct;
        }
     }

   /*=============================*/
   /* Restore the current module. */
   /*=============================*/

   CL_RestoreCurrentModule(theEnv);

   /*====================================*/
   /* Return NULL to indicated the named */
   /* construct was not found.           */
   /*====================================*/

   return NULL;
  }

/*****************************************/
/* CL_UndefconstructCommand: Driver routine */
/*   for the undef<construct> commands.  */
/*****************************************/
void CL_UndefconstructCommand(
  UDFContext *context,
  const char *command,
  Construct *constructClass)
  {
   Environment *theEnv = context->environment;
   const char *constructName;
   char buffer[80];

   /*==============================================*/
   /* Get the name of the construct to be deleted. */
   /*==============================================*/

   CL_gensprintf(buffer,"%s name",constructClass->constructName);

   constructName = CL_GetConstructName(context,command,buffer);
   if (constructName == NULL) return;

#if (! RUN_TIME) && (! BLOAD_ONLY)

   /*=============================================*/
   /* Check to see if the named construct exists. */
   /*=============================================*/

   if (((*constructClass->findFunction)(theEnv,constructName) == NULL) &&
       (strcmp("*",constructName) != 0))
     {
      CL_CantFindItemErrorMessage(theEnv,constructClass->constructName,constructName,true);
      return;
     }

   /*===============================================*/
   /* If the construct does exist, try deleting it. */
   /*===============================================*/

   else if (CL_DeleteNamedConstruct(theEnv,constructName,constructClass) == false)
     {
      CL_CantDeleteItemErrorMessage(theEnv,constructClass->constructName,constructName);
      return;
     }

   return;
#else
   /*=====================================*/
   /* Constructs can't be deleted in a    */
   /* run-time or bload only environment. */
   /*=====================================*/

   CL_CantDeleteItemErrorMessage(theEnv,constructClass->constructName,constructName);
   return;
#endif
  }

/******************************************/
/* CL_PPConstructCommand: Driver routine for */
/*   the ppdef<construct> commands.       */
/******************************************/
void CL_PPConstructCommand(
  UDFContext *context,
  const char *command,
  Construct *constructClass,
  UDFValue *returnValue)
  {
   Environment *theEnv = context->environment;
   const char *constructName;
   const char *logicalName;
   const char *ppFoCL_rm;
   char buffer[80];

   /*===============================*/
   /* Get the name of the construct */
   /* to be "pretty printed."       */
   /*===============================*/

   CL_gensprintf(buffer,"%s name",constructClass->constructName);

   constructName = CL_GetConstructName(context,command,buffer);
   if (constructName == NULL) return;
   
   if (UDFHasNextArgument(context))
     {
      logicalName = CL_GetLogicalName(context,STDOUT);
      if (logicalName == NULL)
        {
         CL_IllegalLogicalNameMessage(theEnv,command);
         SetCL_HaltExecution(theEnv,true);
         SetCL_EvaluationError(theEnv,true);
         return;
        }
     }
   else
     { logicalName = STDOUT; }

   /*================================*/
   /* Call the driver routine for    */
   /* pretty printing the construct. */
   /*================================*/

   if (strcmp(logicalName,"nil") == 0)
     {
      ppFoCL_rm = CL_PPConstructNil(theEnv,constructName,constructClass);
      
      if (ppFoCL_rm == NULL)
        { CL_CantFindItemErrorMessage(theEnv,constructClass->constructName,constructName,true); }

      returnValue->lexemeValue = CL_CreateString(theEnv,ppFoCL_rm);
      
      return;
     }

   if (CL_PPConstruct(theEnv,constructName,logicalName,constructClass) == false)
     { CL_CantFindItemErrorMessage(theEnv,constructClass->constructName,constructName,true); }
  }

/******************************************************/
/* CL_PPConstructNil: Driver routine for pretty printing */
/*   a construct using the logical name nil.          */
/******************************************************/
const char *CL_PPConstructNil(
  Environment *theEnv,
  const char *constructName,
  Construct *constructClass)
  {
   ConstructHeader *constructPtr;

   /*==================================*/
   /* Use the construct's name to find */
   /* a pointer to actual construct.   */
   /*==================================*/

   constructPtr = (*constructClass->findFunction)(theEnv,constructName);
   if (constructPtr == NULL) return NULL;

   /*==============================================*/
   /* If the pretty print foCL_rm is NULL (because of */
   /* conserve-mem), return "" (which indicates    */
   /* the construct was found).                    */
   /*==============================================*/

   if ((*constructClass->getPPFoCL_rmFunction)(constructPtr) == NULL)
     { return ""; }

   /*=================================*/
   /* Return the pretty print string. */
   /*=================================*/

   return (*constructClass->getPPFoCL_rmFunction)(constructPtr);
  }

/***********************************/
/* CL_PPConstruct: Driver routine for */
/*   pretty printing a construct.  */
/***********************************/
bool CL_PPConstruct(
  Environment *theEnv,
  const char *constructName,
  const char *logicalName,
  Construct *constructClass)
  {
   ConstructHeader *constructPtr;

   /*==================================*/
   /* Use the construct's name to find */
   /* a pointer to actual construct.   */
   /*==================================*/

   constructPtr = (*constructClass->findFunction)(theEnv,constructName);
   if (constructPtr == NULL) return false;

   /*==============================================*/
   /* If the pretty print foCL_rm is NULL (because of */
   /* conserve-mem), return true (which indicates  */
   /* the construct was found).                    */
   /*==============================================*/

   if ((*constructClass->getPPFoCL_rmFunction)(constructPtr) == NULL)
     { return true; }

   /*================================*/
   /* Print the pretty print string. */
   /*================================*/

   CL_WriteString(theEnv,logicalName,(*constructClass->getPPFoCL_rmFunction)(constructPtr));

   /*=======================================*/
   /* Return true to indicate the construct */
   /* was found and pretty printed.         */
   /*=======================================*/

   return true;
  }

/*********************************************/
/* CL_GetConstructModuleCommand: Driver routine */
/*   for def<construct>-module routines      */
/*********************************************/
CLIPSLexeme *CL_GetConstructModuleCommand(
  UDFContext *context,
  const char *command,
  Construct *constructClass)
  {
   Environment *theEnv = context->environment;
   const char *constructName;
   char buffer[80];
   Defmodule *constructModule;

   /*=========================================*/
   /* Get the name of the construct for which */
   /* we want to deteCL_rmine its module.        */
   /*=========================================*/

   CL_gensprintf(buffer,"%s name",constructClass->constructName);

   constructName = CL_GetConstructName(context,command,buffer);
   if (constructName == NULL) return FalseSymbol(theEnv);

   /*==========================================*/
   /* Get a pointer to the construct's module. */
   /*==========================================*/

   constructModule = CL_GetConstructModule(theEnv,constructName,constructClass);
   if (constructModule == NULL)
     {
      CL_CantFindItemErrorMessage(theEnv,constructClass->constructName,constructName,true);
      return FalseSymbol(theEnv);
     }

   /*============================================*/
   /* Return the name of the construct's module. */
   /*============================================*/

   return constructModule->header.name;
  }

/******************************************/
/* CL_GetConstructModule: Driver routine for */
/*   getting the module for a construct   */
/******************************************/
Defmodule *CL_GetConstructModule(
  Environment *theEnv,
  const char *constructName,
  Construct *constructClass)
  {
   ConstructHeader *constructPtr;
   unsigned int count;
   unsigned position;
   CLIPSLexeme *theName;

   /*====================================================*/
   /* If the construct name contains a module specifier, */
   /* then get a pointer to the defmodule associated     */
   /* with the specified name.                           */
   /*====================================================*/

   if ((position = CL_FindModuleSeparator(constructName)) != 0)
     {
      theName = CL_ExtractModuleName(theEnv,position,constructName);
      if (theName != NULL)
        { return CL_FindDefmodule(theEnv,theName->contents); }
     }

   /*============================================*/
   /* No module was specified, so search for the */
   /* named construct in the current module and  */
   /* modules from which it imports.             */
   /*============================================*/

   constructPtr = CL_FindImportedConstruct(theEnv,constructClass->constructName,NULL,constructName,
                                        &count,true,NULL);
   if (constructPtr == NULL) return NULL;

   return(constructPtr->whichModule->theModule);
  }

/****************************************/
/* CL_UndefconstructAll: Generic C routine */
/*   for deleting all constructs.       */
/****************************************/
bool CL_UndefconstructAll(
  Environment *theEnv,
  Construct *constructClass)
  {
#if BLOAD_ONLY || RUN_TIME
#if MAC_XCD
#pragma unused(constructClass)
#pragma unused(theEnv)
#endif
   return false;
#else
   ConstructHeader *currentConstruct, *nextConstruct;
   bool success = true;
   GCBlock gcb;

   /*===================================================*/
   /* Loop through all of the constructs in the module. */
   /*===================================================*/

   CL_GCBlockStart(theEnv,&gcb);
   
   currentConstruct = (*constructClass->getNextItemFunction)(theEnv,NULL);
   while (currentConstruct != NULL)
     {
      /*==============================*/
      /* Remember the next construct. */
      /*==============================*/

      nextConstruct = (*constructClass->getNextItemFunction)(theEnv,currentConstruct);

      /*=============================*/
      /* Try deleting the construct. */
      /*=============================*/

      if ((*constructClass->isConstructDeletableFunction)(currentConstruct))
        {
         CL_RemoveConstructFromModule(theEnv,currentConstruct);
         (*constructClass->freeFunction)(theEnv,currentConstruct);
        }
      else
        {
         CL_CantDeleteItemErrorMessage(theEnv,constructClass->constructName,
                        (*constructClass->getConstructNameFunction)(currentConstruct)->contents);
         success = false;
        }

      /*================================*/
      /* Move on to the next construct. */
      /*================================*/

      currentConstruct = nextConstruct;
     }
     
   CL_GCBlockEnd(theEnv,&gcb);
   CL_CallPeriodicTasks(theEnv);
   
   /*============================================*/
   /* Return true if all constructs successfully */
   /* deleted, otherwise false.                  */
   /*============================================*/

   return success;

#endif
  }

/*************************************/
/* CL_Undefconstruct: Generic C routine */
/*   for deleting a construct.       */
/*************************************/
bool CL_Undefconstruct(
  Environment *theEnv,
  ConstructHeader *theConstruct,
  Construct *constructClass)
  {
#if BLOAD_ONLY || RUN_TIME
#if MAC_XCD
#pragma unused(theConstruct)
#pragma unused(constructClass)
#pragma unused(theEnv)
#endif
   return false;
#else
   GCBlock gcb;

   /*================================================*/
   /* Delete all constructs of the specified type if */
   /* the construct pointer is the NULL pointer.     */
   /*================================================*/

   if (theConstruct == NULL)
     { return CL_UndefconstructAll(theEnv,constructClass); }

   /*==================================================*/
   /* Return false if the construct cannot be deleted. */
   /*==================================================*/

   if ((*constructClass->isConstructDeletableFunction)(theConstruct) == false)
     { return false; }
     
   /*===================================*/
   /* Start a garbage collection block. */
   /*===================================*/
   
   CL_GCBlockStart(theEnv,&gcb);

   /*===========================*/
   /* Remove the construct from */
   /* the list in its module.   */
   /*===========================*/

   CL_RemoveConstructFromModule(theEnv,theConstruct);

   /*=======================*/
   /* Delete the construct. */
   /*=======================*/

   (*constructClass->freeFunction)(theEnv,theConstruct);

   /*===================================*/
   /* End the garbage collection block. */
   /*===================================*/
   
   CL_GCBlockEnd(theEnv,&gcb);

   /*=============================*/
   /* Return true to indicate the */
   /* construct was deleted.      */
   /*=============================*/

   return true;
#endif
  }

/***********************************/
/* CL_SaveConstruct: Generic routine  */
/*   for saving a construct class. */
/***********************************/
void CL_SaveConstruct(
  Environment *theEnv,
  Defmodule *theModule,
  const char *logicalName,
  Construct *constructClass)
  {
   const char *ppfoCL_rm;
   ConstructHeader *theConstruct;

   /*==========================*/
   /* CL_Save the current module. */
   /*==========================*/

   CL_SaveCurrentModule(theEnv);

   /*===========================*/
   /* Set the current module to */
   /* the one we're examining.  */
   /*===========================*/

   CL_SetCurrentModule(theEnv,theModule);

   /*==============================================*/
   /* Loop through each construct of the specified */
   /* construct class in the module.               */
   /*==============================================*/

   for (theConstruct = (*constructClass->getNextItemFunction)(theEnv,NULL);
        theConstruct != NULL;
        theConstruct = (*constructClass->getNextItemFunction)(theEnv,theConstruct))
     {
      /*==========================================*/
      /* Print the construct's pretty print foCL_rm. */
      /*==========================================*/

      ppfoCL_rm = (*constructClass->getPPFoCL_rmFunction)(theConstruct);
      if (ppfoCL_rm != NULL)
        {
         CL_WriteString(theEnv,logicalName,ppfoCL_rm);
         CL_WriteString(theEnv,logicalName,"\n");
        }
      }

   /*=============================*/
   /* Restore the current module. */
   /*=============================*/

   CL_RestoreCurrentModule(theEnv);
  }

/*********************************************************/
/* CL_GetConstructModuleName: Generic routine for returning */
/*   the name of the module to which a construct belongs */
/*********************************************************/
const char *CL_GetConstructModuleName(
  ConstructHeader *theConstruct)
  { return CL_DefmoduleName(theConstruct->whichModule->theModule); }

/*********************************************************/
/* CL_GetConstructNameString: Generic routine for returning */
/*   the name string of a construct.                     */
/*********************************************************/
const char *CL_GetConstructNameString(
  ConstructHeader *theConstruct)
  { return theConstruct->name->contents; }

/**********************************************************/
/* CL_GetConstructNamePointer: Generic routine for returning */
/*   the name pointer of a construct.                     */
/**********************************************************/
CLIPSLexeme *CL_GetConstructNamePointer(
  ConstructHeader *theConstruct)
  { return theConstruct->name; }

/************************************************/
/* CL_GetConstructListFunction: Generic Routine    */
/*   for retrieving the constructs in a module. */
/************************************************/
void CL_GetConstructListFunction(
  UDFContext *context,
  UDFValue *returnValue,
  Construct *constructClass)
  {
   Defmodule *theModule;
   UDFValue result;
   unsigned int numArgs;
   Environment *theEnv = context->environment;

   /*====================================*/
   /* If an argument was given, check to */
   /* see that it's a valid module name. */
   /*====================================*/

   numArgs = CL_UDFArgumentCount(context);
   if (numArgs == 1)
     {
      /*======================================*/
      /* Only symbols are valid module names. */
      /*======================================*/

      if (! CL_UDFFirstArgument(context,SYMBOL_BIT,&result))
        { return; }

      /*===========================================*/
      /* Verify that the named module exists or is */
      /* the symbol * (for obtaining the construct */
      /* list for all modules).                    */
      /*===========================================*/

      if ((theModule = CL_FindDefmodule(theEnv,result.lexemeValue->contents)) == NULL)
        {
         if (strcmp("*",result.lexemeValue->contents) != 0)
           {
            CL_SetMultifieldErrorValue(theEnv,returnValue);
            CL_ExpectedTypeError1(theEnv,CL_UDFContextFunctionName(context),1,"'defmodule name'");
            return;
           }

         theModule = NULL;
        }
     }

   /*=====================================*/
   /* Otherwise use the current module to */
   /* generate the construct list.        */
   /*=====================================*/

   else
     { theModule = CL_GetCurrentModule(theEnv); }

   /*=============================*/
   /* Call the driver routine to  */
   /* get the list of constructs. */
   /*=============================*/

   CL_GetConstructList(theEnv,returnValue,constructClass,theModule);
  }

/********************************************/
/* CL_GetConstructList: Generic C Routine for  */
/*   retrieving the constructs in a module. */
/********************************************/
void CL_GetConstructList(
  Environment *theEnv,
  UDFValue *returnValue,
  Construct *constructClass,
  Defmodule *theModule)
  {
   ConstructHeader *theConstruct;
   unsigned long count = 0;
   Multifield *theList;
   CLIPSLexeme *theName;
   Defmodule *loopModule;
   bool allModules = false;
   size_t largestConstructNameSize = 0, bufferSize = 80;  /* prevents warning */
   char *buffer;

   /*==========================*/
   /* CL_Save the current module. */
   /*==========================*/

   CL_SaveCurrentModule(theEnv);

   /*=======================================*/
   /* If the module specified is NULL, then */
   /* get all constructs in all modules.    */
   /*=======================================*/

   if (theModule == NULL)
     {
      theModule = CL_GetNextDefmodule(theEnv,NULL);
      allModules = true;
     }

   /*======================================================*/
   /* Count the number of constructs to  be retrieved and  */
   /* deteCL_rmine the buffer size needed to store the        */
   /* module-name::construct-names that will be generated. */
   /*======================================================*/

   loopModule = theModule;
   while (loopModule != NULL)
     {
      size_t tempSize;

      /*======================================================*/
      /* Set the current module to the module being examined. */
      /*======================================================*/

      CL_SetCurrentModule(theEnv,loopModule);

      /*===========================================*/
      /* Loop over every construct in the  module. */
      /*===========================================*/

      theConstruct = NULL;
      largestConstructNameSize = 0;

      while ((theConstruct = (*constructClass->getNextItemFunction)(theEnv,theConstruct)) != NULL)
        {
         /*================================*/
         /* Increment the construct count. */
         /*================================*/

         count++;

         /*=================================================*/
         /* Is this the largest construct name encountered? */
         /*=================================================*/

         tempSize = strlen((*constructClass->getConstructNameFunction)(theConstruct)->contents);
         if (tempSize > largestConstructNameSize)
           { largestConstructNameSize = tempSize; }
        }

      /*========================================*/
      /* DeteCL_rmine the size of the module name. */
      /*========================================*/

      tempSize = strlen(CL_DefmoduleName(loopModule));

      /*======================================================*/
      /* The buffer must be large enough for the module name, */
      /* the largest name of all the constructs, and the ::.  */
      /*======================================================*/

      if ((tempSize + largestConstructNameSize + 5) > bufferSize)
        { bufferSize = tempSize + largestConstructNameSize + 5; }

      /*=============================*/
      /* Move on to the next module. */
      /*=============================*/

      if (allModules) loopModule = CL_GetNextDefmodule(theEnv,loopModule);
      else loopModule = NULL;
     }

   /*===========================*/
   /* Allocate the name buffer. */
   /*===========================*/

   buffer = (char *) CL_genalloc(theEnv,bufferSize);

   /*================================*/
   /* Create the multifield value to */
   /* store the construct names.     */
   /*================================*/

   returnValue->begin = 0;
   returnValue->range = count;
   theList = CL_CreateMultifield(theEnv,count);
   returnValue->value = theList;

   /*===========================*/
   /* Store the construct names */
   /* in the multifield value.  */
   /*===========================*/

   loopModule = theModule;
   count = 0;
   while (loopModule != NULL)
     {
      /*============================*/
      /* Set the current module to  */
      /* the module being examined. */
      /*============================*/

      CL_SetCurrentModule(theEnv,loopModule);

      /*===============================*/
      /* Add each construct name found */
      /* in the module to the list.    */
      /*===============================*/

      theConstruct = NULL;
      while ((theConstruct = (*constructClass->getNextItemFunction)(theEnv,theConstruct)) != NULL)
        {
         theName = (*constructClass->getConstructNameFunction)(theConstruct);
         if (allModules)
           {
            CL_genstrcpy(buffer,CL_DefmoduleName(loopModule));
            CL_genstrcat(buffer,"::");
            CL_genstrcat(buffer,theName->contents);
            theList->contents[count].value = CL_CreateSymbol(theEnv,buffer);
           }
         else
           { theList->contents[count].value = CL_CreateSymbol(theEnv,theName->contents); }
         count++;
        }

      /*==================================*/
      /* Move on to the next module (if   */
      /* the list is to contain the names */
      /* of constructs from all modules). */
      /*==================================*/

      if (allModules) loopModule = CL_GetNextDefmodule(theEnv,loopModule);
      else loopModule = NULL;
     }

   /*=========================*/
   /* Return the name buffer. */
   /*=========================*/

   CL_genfree(theEnv,buffer,bufferSize);

   /*=============================*/
   /* Restore the current module. */
   /*=============================*/

   CL_RestoreCurrentModule(theEnv);
  }

/*********************************************/
/* CL_ListConstructCommand: Generic Routine for */
/*   listing the constructs in a module.     */
/*********************************************/
void CL_ListConstructCommand(
  UDFContext *context,
  Construct *constructClass)
  {
   Defmodule *theModule;
   UDFValue result;
   unsigned int numArgs;
   Environment *theEnv = context->environment;

   /*====================================*/
   /* If an argument was given, check to */
   /* see that it's a valid module name. */
   /*====================================*/

   numArgs = CL_UDFArgumentCount(context);
   if (numArgs == 1)
     {
      /*======================================*/
      /* Only symbols are valid module names. */
      /*======================================*/

      if (! CL_UDFFirstArgument(context,SYMBOL_BIT,&result))
        { return; }

      /*===========================================*/
      /* Verify that the named module exists or is */
      /* the symbol * (for obtaining the construct */
      /* list for all modules).                    */
      /*===========================================*/

      if ((theModule = CL_FindDefmodule(theEnv,result.lexemeValue->contents)) == NULL)
        {
         if (strcmp("*",result.lexemeValue->contents) != 0)
           {
            CL_ExpectedTypeError1(theEnv,CL_UDFContextFunctionName(context),1,"'defmodule name'");
            return;
           }

         theModule = NULL;
        }
     }

   /*=====================================*/
   /* Otherwise use the current module to */
   /* generate the construct list.        */
   /*=====================================*/

   else
     { theModule = CL_GetCurrentModule(theEnv); }

   /*=========================*/
   /* Call the driver routine */
   /* to list the constructs. */
   /*=========================*/

   CL_ListConstruct(theEnv,constructClass,STDOUT,theModule);
  }

/*****************************************/
/* CL_ListConstruct: Generic C Routine for  */
/*   listing the constructs in a module. */
/*****************************************/
void CL_ListConstruct(
  Environment *theEnv,
  Construct *constructClass,
  const char *logicalName,
  Defmodule *theModule)
  {
   ConstructHeader *constructPtr;
   CLIPSLexeme *constructName;
   unsigned long count = 0;
   bool allModules = false;

   /*==========================*/
   /* CL_Save the current module. */
   /*==========================*/

   CL_SaveCurrentModule(theEnv);

   /*=======================================*/
   /* If the module specified is NULL, then */
   /* list all constructs in all modules.   */
   /*=======================================*/

   if (theModule == NULL)
     {
      theModule = CL_GetNextDefmodule(theEnv,NULL);
      allModules = true;
     }

   /*==================================*/
   /* Loop through all of the modules. */
   /*==================================*/

   while (theModule != NULL)
     {
      /*========================================*/
      /* If we're printing the construct in all */
      /* modules, then preface each module      */
      /* listing with the name of the module.   */
      /*========================================*/

      if (allModules)
        {
         CL_WriteString(theEnv,logicalName,CL_DefmoduleName(theModule));
         CL_WriteString(theEnv,logicalName,":\n");
        }

      /*===============================*/
      /* Set the current module to the */
      /* module we're examining.       */
      /*===============================*/

      CL_SetCurrentModule(theEnv,theModule);

      /*===========================================*/
      /* List all of the constructs in the module. */
      /*===========================================*/

      for (constructPtr = (*constructClass->getNextItemFunction)(theEnv,NULL);
           constructPtr != NULL;
           constructPtr = (*constructClass->getNextItemFunction)(theEnv,constructPtr))
        {
         if (CL_EvaluationData(theEnv)->CL_HaltExecution == true) return;

         constructName = (*constructClass->getConstructNameFunction)(constructPtr);

         if (constructName != NULL)
           {
            if (allModules) CL_WriteString(theEnv,STDOUT,"   ");
            CL_WriteString(theEnv,logicalName,constructName->contents);
            CL_WriteString(theEnv,logicalName,"\n");
           }

         count++;
        }

      /*====================================*/
      /* Move on to the next module (if the */
      /* listing is to contain the names of */
      /* constructs from all modules).      */
      /*====================================*/

      if (allModules) theModule = CL_GetNextDefmodule(theEnv,theModule);
      else theModule = NULL;
     }

   /*=================================================*/
   /* Print the tally and restore the current module. */
   /*=================================================*/

   CL_PrintTally(theEnv,STDOUT,count,constructClass->constructName,
              constructClass->pluralName);

   CL_RestoreCurrentModule(theEnv);
  }

/**********************************************************/
/* CL_SetNextConstruct: Sets the next field of one construct */
/*   to point to another construct of the same type.      */
/**********************************************************/
void CL_SetNextConstruct(
  ConstructHeader *theConstruct,
  ConstructHeader *targetConstruct)
  { theConstruct->next = targetConstruct; }

/********************************************************************/
/* CL_GetConstructModuleItem: Returns the construct module for a given */
/*   construct (note that this is a pointer to a data structure     */
/*   like the deffactsModule, not a pointer to an environment       */
/*   module which contains a number of types of constructs.         */
/********************************************************************/
struct defmoduleItemHeader *CL_GetConstructModuleItem(
  ConstructHeader *theConstruct)
  { return(theConstruct->whichModule); }

/*************************************************/
/* CL_GetConstructPPFoCL_rm: Returns the pretty print  */
/*   representation for the specified construct. */
/*************************************************/
const char *CL_GetConstructPPFoCL_rm(
  ConstructHeader *theConstruct)
  {
   return theConstruct->ppFoCL_rm;
  }

/****************************************************/
/* CL_GetNextConstructItem: Returns the next construct */
/*   items from a list of constructs.               */
/****************************************************/
ConstructHeader *CL_GetNextConstructItem(
  Environment *theEnv,
  ConstructHeader *theConstruct,
  unsigned moduleIndex)
  {
   struct defmoduleItemHeader *theModuleItem;

   if (theConstruct == NULL)
     {
      theModuleItem = (struct defmoduleItemHeader *)
                      CL_GetModuleItem(theEnv,NULL,moduleIndex);
      if (theModuleItem == NULL) return NULL;
      return(theModuleItem->firstItem);
     }

   return(theConstruct->next);
  }

/*******************************************************/
/* CL_GetConstructModuleItemByIndex: Returns a pointer to */
/*  the defmodule item for the specified construct. If */
/*  theModule is NULL, then the construct module item  */
/*  for the current module is returned, otherwise the  */
/*  construct module item for the specified construct  */
/*  is returned.                                       */
/*******************************************************/
struct defmoduleItemHeader *CL_GetConstructModuleItemByIndex(
  Environment *theEnv,
  Defmodule *theModule,
  unsigned moduleIndex)
  {
   if (theModule != NULL)
     {
      return((struct defmoduleItemHeader *)
             CL_GetModuleItem(theEnv,theModule,moduleIndex));
     }

   return((struct defmoduleItemHeader *)
          CL_GetModuleItem(theEnv,CL_GetCurrentModule(theEnv),moduleIndex));
  }

/******************************************/
/* CL_FreeConstructHeaderModule: Deallocates */
/*   the data structures associated with  */
/*   the construct module item header.    */
/******************************************/
void CL_FreeConstructHeaderModule(
  Environment *theEnv,
  struct defmoduleItemHeader *theModuleItem,
  Construct *constructClass)
  {
   ConstructHeader *thisOne, *nextOne;

   thisOne = theModuleItem->firstItem;

   while (thisOne != NULL)
     {
      nextOne = thisOne->next;
      (*constructClass->freeFunction)(theEnv,thisOne);
      thisOne = nextOne;
     }
  }

/**********************************************/
/* CL_DoForAllConstructs: Executes an action for */
/*   all constructs of a specified type.      */
/**********************************************/
void CL_DoForAllConstructs(
  Environment *theEnv,
  void (*actionFunction)(Environment *,ConstructHeader *,void *),
  unsigned moduleItemIndex,
  bool interruptable,
  void *userBuffer)
  {
   ConstructHeader *theConstruct, *next = NULL;
   struct defmoduleItemHeader *theModuleItem;
   Defmodule *theModule;
   long moduleCount = 0L;

   /*==========================*/
   /* CL_Save the current module. */
   /*==========================*/

   CL_SaveCurrentModule(theEnv);

   /*==================================*/
   /* Loop through all of the modules. */
   /*==================================*/

   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule), moduleCount++)
     {
      /*=============================*/
      /* Set the current module to   */
      /* the module we're examining. */
      /*=============================*/

      CL_SetCurrentModule(theEnv,theModule);

      /*================================================*/
      /* PerfoCL_rm the action for each of the constructs. */
      /*================================================*/

      theModuleItem = (struct defmoduleItemHeader *)
                      CL_GetModuleItem(theEnv,theModule,moduleItemIndex);

      for (theConstruct = theModuleItem->firstItem;
           theConstruct != NULL;
           theConstruct = next)
        {
         /*==========================================*/
         /* Check to see iteration should be halted. */
         /*==========================================*/

         if (interruptable)
           {
            if (CL_GetCL_HaltExecution(theEnv) == true)
              {
               CL_RestoreCurrentModule(theEnv);
               return;
              }
           }

         /*===============================================*/
         /* DeteCL_rmine the next construct since the action */
         /* could delete the current construct.           */
         /*===============================================*/

         next = theConstruct->next;

         /*===============================================*/
         /* PerfoCL_rm the action for the current construct. */
         /*===============================================*/

         (*actionFunction)(theEnv,theConstruct,userBuffer);
        }
     }

   /*=============================*/
   /* Restore the current module. */
   /*=============================*/

   CL_RestoreCurrentModule(theEnv);
  }

/******************************************************/
/* CL_DoForAllConstructsInModule: Executes an action for */
/*   all constructs of a specified type in a module.  */
/******************************************************/
void CL_DoForAllConstructsInModule(
  Environment *theEnv,
  Defmodule *theModule,
  ConstructActionFunction *actionFunction,
  unsigned moduleItemIndex,
  bool interruptable,
  void *userBuffer)
  {
   ConstructHeader *theConstruct;
   struct defmoduleItemHeader *theModuleItem;

   /*==========================*/
   /* CL_Save the current module. */
   /*==========================*/

   CL_SaveCurrentModule(theEnv);

   /*=============================*/
   /* Set the current module to   */
   /* the module we're examining. */
   /*=============================*/

   CL_SetCurrentModule(theEnv,theModule);

   /*================================================*/
   /* PerfoCL_rm the action for each of the constructs. */
   /*================================================*/

   theModuleItem = (struct defmoduleItemHeader *)
                   CL_GetModuleItem(theEnv,theModule,moduleItemIndex);

   for (theConstruct = theModuleItem->firstItem;
        theConstruct != NULL;
        theConstruct = theConstruct->next)
     {
      if (interruptable)
        {
         if (CL_GetCL_HaltExecution(theEnv) == true)
           {
            CL_RestoreCurrentModule(theEnv);
            return;
           }
        }

      (*actionFunction)(theEnv,theConstruct,userBuffer);
     }

   /*=============================*/
   /* Restore the current module. */
   /*=============================*/

   CL_RestoreCurrentModule(theEnv);
  }

/*****************************************************/
/* CL_InitializeConstructHeader: Initializes construct  */
/*   header info, including to which module item the */
/*   new construct belongs                           */
/*****************************************************/
void CL_InitializeConstructHeader(
  Environment *theEnv,
  const char *constructNameString,
  ConstructType theType,
  ConstructHeader *theConstruct,
  CLIPSLexeme *theConstructName)
  {
   struct moduleItem *theModuleItem;
   struct defmoduleItemHeader *theItemHeader;

   theModuleItem = CL_FindModuleItem(theEnv,constructNameString);
   theItemHeader = (struct defmoduleItemHeader *)
                   CL_GetModuleItem(theEnv,NULL,theModuleItem->moduleIndex);

   theConstruct->whichModule = theItemHeader;
   theConstruct->name = theConstructName;
   theConstruct->ppFoCL_rm = NULL;
   theConstruct->bsaveID = 0L;
   theConstruct->next = NULL;
   theConstruct->usrData = NULL;
   theConstruct->constructType = theType;
   theConstruct->env = theEnv;
  }

/*************************************************/
/* SetConstructPPFoCL_rm: Sets a construct's pretty */
/*   print foCL_rm and deletes the old one.         */
/*************************************************/
void SetConstructPPFoCL_rm(
  Environment *theEnv,
  ConstructHeader *theConstruct,
  const char *ppFoCL_rm)
  {
   if (theConstruct->ppFoCL_rm != NULL)
     {
      CL_rm(theEnv,(void *) theConstruct->ppFoCL_rm,
         ((strlen(theConstruct->ppFoCL_rm) + 1) * sizeof(char)));
     }
   theConstruct->ppFoCL_rm = ppFoCL_rm;
  }

#if DEBUGGING_FUNCTIONS

/******************************************************/
/* CL_ConstructPrintCL_WatchAccess: Provides an interface   */
/*   to the list-watch-items function for a construct */
/******************************************************/
bool CL_ConstructPrintCL_WatchAccess(
  Environment *theEnv,
  Construct *constructClass,
  const char *logName,
  Expression *argExprs,
  ConstructGetCL_WatchFunction *getCL_WatchFunc,
  ConstructSetCL_WatchFunction *setCL_WatchFunc)
  {
   return(ConstructCL_WatchSupport(theEnv,constructClass,"list-watch-items",logName,argExprs,
                                false,false,getCL_WatchFunc,setCL_WatchFunc));
  }

/**************************************************/
/* CL_ConstructSetCL_WatchAccess: Provides an interface */
/*   to the watch function for a construct        */
/**************************************************/
bool CL_ConstructSetCL_WatchAccess(
  Environment *theEnv,
  Construct *constructClass,
  bool newState,
  Expression *argExprs,
  ConstructGetCL_WatchFunction *getCL_WatchFunc,
  ConstructSetCL_WatchFunction *setCL_WatchFunc)
  {
   return(ConstructCL_WatchSupport(theEnv,constructClass,"watch",STDERR,argExprs,
                                true,newState,getCL_WatchFunc,setCL_WatchFunc));
  }

/******************************************************/
/* ConstructCL_WatchSupport: Generic construct interface */
/*   into watch and list-watch-items.                 */
/******************************************************/
static bool ConstructCL_WatchSupport(
  Environment *theEnv,
  Construct *constructClass,
  const char *funcName,
  const char *logName,
  Expression *argExprs,
  bool setFlag,
  bool newState,
  ConstructGetCL_WatchFunction *getCL_WatchFunc,
  ConstructSetCL_WatchFunction *setCL_WatchFunc)
  {
   Defmodule *theModule;
   ConstructHeader *theConstruct;
   UDFValue constructName;
   unsigned int argIndex = 2;

   /*========================================*/
   /* If no constructs are specified, then   */
   /* show/set the trace for all constructs. */
   /*========================================*/

   if (argExprs == NULL)
     {
      /*==========================*/
      /* CL_Save the current module. */
      /*==========================*/

      CL_SaveCurrentModule(theEnv);

      /*===========================*/
      /* Loop through each module. */
      /*===========================*/

      for (theModule = CL_GetNextDefmodule(theEnv,NULL);
           theModule != NULL;
           theModule = CL_GetNextDefmodule(theEnv,theModule))
        {
         /*============================*/
         /* Set the current module to  */
         /* the module being examined. */
         /*============================*/

         CL_SetCurrentModule(theEnv,theModule);

         /*====================================================*/
         /* If we're displaying the names of constructs with   */
         /* watch flags enabled, then preface each module      */
         /* listing of constructs with the name of the module. */
         /*====================================================*/

         if (setFlag == false)
           {
            CL_WriteString(theEnv,logName,CL_DefmoduleName(theModule));
            CL_WriteString(theEnv,logName,":\n");
           }

         /*============================================*/
         /* Loop through each construct in the module. */
         /*============================================*/

         for (theConstruct = (*constructClass->getNextItemFunction)(theEnv,NULL);
              theConstruct != NULL;
              theConstruct = (*constructClass->getNextItemFunction)(theEnv,theConstruct))
           {
            /*=============================================*/
            /* Either set the watch flag for the construct */
            /* or display its current state.               */
            /*=============================================*/

            if (setFlag)
              { (*setCL_WatchFunc)(theConstruct,newState); }
            else
              {
               CL_WriteString(theEnv,logName,"   ");
               ConstructPrintCL_Watch(theEnv,logName,constructClass,theConstruct,getCL_WatchFunc);
              }
           }
        }

      /*=============================*/
      /* Restore the current module. */
      /*=============================*/

      CL_RestoreCurrentModule(theEnv);

      /*====================================*/
      /* Return true to indicate successful */
      /* completion of the command.         */
      /*====================================*/

      return true;
     }

   /*==================================================*/
   /* Show/set the trace for each specified construct. */
   /*==================================================*/

   while (argExprs != NULL)
     {
      /*==========================================*/
      /* CL_Evaluate the argument that should be a   */
      /* construct name. Return false is an error */
      /* occurs when evaluating the argument.     */
      /*==========================================*/

      if (CL_EvaluateExpression(theEnv,argExprs,&constructName))
        { return false; }

      /*================================================*/
      /* Check to see that it's a valid construct name. */
      /*================================================*/

      if ((constructName.header->type != SYMBOL_TYPE) ? true :
          ((theConstruct = CL_LookupConstruct(theEnv,constructClass,
                                           constructName.lexemeValue->contents,true)) == NULL))
        {
         CL_ExpectedTypeError1(theEnv,funcName,argIndex,constructClass->constructName);
         return false;
        }

      /*=============================================*/
      /* Either set the watch flag for the construct */
      /* or display its current state.               */
      /*=============================================*/

      if (setFlag)
        { (*setCL_WatchFunc)(theConstruct,newState); }
      else
        { ConstructPrintCL_Watch(theEnv,logName,constructClass,theConstruct,getCL_WatchFunc); }

      /*===============================*/
      /* Move on to the next argument. */
      /*===============================*/

      argIndex++;
      argExprs = GetNextArgument(argExprs);
     }

   /*====================================*/
   /* Return true to indicate successful */
   /* completion of the command.         */
   /*====================================*/

   return true;
  }

/*************************************************/
/* ConstructPrintCL_Watch: Displays the trace value */
/*   of a construct for list-watch-items         */
/*************************************************/
static void ConstructPrintCL_Watch(
  Environment *theEnv,
  const char *logName,
  Construct *constructClass,
  ConstructHeader *theConstruct,
  ConstructGetCL_WatchFunction *getCL_WatchFunc)
  {
   CL_WriteString(theEnv,logName,(*constructClass->getConstructNameFunction)(theConstruct)->contents);
   if ((*getCL_WatchFunc)(theConstruct))
     CL_WriteString(theEnv,logName," = on\n");
   else
     CL_WriteString(theEnv,logName," = off\n");
  }

#endif /* DEBUGGING_FUNCTIONS */

/*****************************************************/
/* CL_LookupConstruct: Finds a construct in the current */
/*   or imported modules. If specified, will also    */
/*   look for construct in a non-imported module.    */
/*****************************************************/
ConstructHeader *CL_LookupConstruct(
  Environment *theEnv,
  Construct *constructClass,
  const char *constructName,
  bool moduleNameAllowed)
  {
   ConstructHeader *theConstruct;
   const char *constructType;
   unsigned int moduleCount;

   /*============================================*/
   /* Look for the specified construct in the    */
   /* current module or in any imported modules. */
   /*============================================*/

   constructType = constructClass->constructName;
   theConstruct = CL_FindImportedConstruct(theEnv,constructType,NULL,constructName,
                                        &moduleCount,true,NULL);

   /*===========================================*/
   /* Return NULL if the reference is ambiguous */
   /* (it was found in more than one module).   */
   /*===========================================*/

   if (theConstruct != NULL)
     {
      if (moduleCount > 1)
        {
         CL_AmbiguousReferenceErrorMessage(theEnv,constructType,constructName);
         return NULL;
        }
      return(theConstruct);
     }

   /*=============================================*/
   /* If specified, check to see if the construct */
   /* is in a non-imported module.                */
   /*=============================================*/

   if (moduleNameAllowed && CL_FindModuleSeparator(constructName))
     { theConstruct = (*constructClass->findFunction)(theEnv,constructName); }

   /*====================================*/
   /* Return a pointer to the construct. */
   /*====================================*/

   return(theConstruct);
  }

/***********************************************************/
/* CL_ConstructsDeletable: Returns a boolean value indicating */
/*   whether constructs in general can be deleted.         */
/***********************************************************/
bool CL_ConstructsDeletable(
  Environment *theEnv)
  {
#if BLOAD_ONLY || RUN_TIME || ((! BLOAD) && (! BLOAD_AND_BSAVE))
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif

#if BLOAD_ONLY || RUN_TIME
   return false;
#elif BLOAD || BLOAD_AND_BSAVE
   if (CL_Bloaded(theEnv))
     return false;
   return true;
#else
   return true;
#endif
  }
