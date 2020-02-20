   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/18/16             */
   /*                                                     */
   /*             ENVIRONMENT BUILD MODULE                */
   /*******************************************************/

/*************************************************************/
/* Purpose: Routines for supporting multiple environments.   */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.40: Added to separate environment creation and     */
/*            deletion code.                                 */
/*                                                           */
/*************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "setup.h"

#include "bmathfun.h"
#include "commline.h"
#include "emathfun.h"
#include "envrnmnt.h"
#include "engine.h"
#include "filecom.h"
#include "iofun.h"
#include "memalloc.h"
#include "miscfun.h"
#include "multifun.h"
#include "parsefun.h"
#include "pprint.h"
#include "prccode.h"
#include "prcdrfun.h"
#include "prdctfun.h"
#include "prntutil.h"
#include "proflfun.h"
#include "router.h"
#include "sortfun.h"
#include "strngfun.h"
#include "sysdep.h"
#include "textpro.h"
#include "utility.h"
#include "watch.h"

#if DEFFACTS_CONSTRUCT
#include "dffctdef.h"
#endif

#if DEFRULE_CONSTRUCT
#include "ruledef.h"
#endif

#if DEFGENERIC_CONSTRUCT
#include "genrccom.h"
#endif

#if DEFFUNCTION_CONSTRUCT
#include "dffnxfun.h"
#endif

#if DEFGLOBAL_CONSTRUCT
#include "globldef.h"
#endif

#if DEFTEMPLATE_CONSTRUCT
#include "tmpltdef.h"
#endif

#if OBJECT_SYSTEM
#include "classini.h"
#endif

#if DEVELOPER
#include "developr.h"
#endif

#include "envrnbld.h"

/****************************************/
/* GLOBAL EXTERNAL FUNCTION DEFINITIONS */
/****************************************/

   extern void                    CL_UserFunctions(Environment *);

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    RemoveEnvironmentCleanupFunctions(struct environmentData *);
   static Environment            *CL_CreateEnvironmentDriver(CLIPSLexeme **,CLIPSFloat **,
                                                          CLIPSInteger **,CLIPSBitMap **,
                                                          CLIPSExternalAddress **,
                                                          struct functionDefinition *);
   static void                    SystemFunctionDefinitions(Environment *);
   static void                    InitializeKeywords(Environment *);
   static void                    InitializeEnvironment(Environment *,CLIPSLexeme **,CLIPSFloat **,
					   								       CLIPSInteger **,CLIPSBitMap **,
														   CLIPSExternalAddress **,
                                                           struct functionDefinition *);

/************************************************************/
/* CL_CreateEnvironment: Creates an environment data structure */
/*   and initializes its content to zero/null.              */
/************************************************************/
Environment *CL_CreateEnvironment()
  {
   return CL_CreateEnvironmentDriver(NULL,NULL,NULL,NULL,NULL,NULL);
  }

/**********************************************************/
/* CL_CreateCL_RuntimeEnvironment: Creates an environment data  */
/*   structure and initializes its content to zero/null.  */
/**********************************************************/
Environment *CL_CreateCL_RuntimeEnvironment(
  CLIPSLexeme **symbolTable,
  CLIPSFloat **floatTable,
  CLIPSInteger **integerTable,
  CLIPSBitMap **bitmapTable,
  struct functionDefinition *functions)
  {
   return CL_CreateEnvironmentDriver(symbolTable,floatTable,integerTable,bitmapTable,NULL,functions);
  }

/*********************************************************/
/* CL_CreateEnvironmentDriver: Creates an environment data  */
/*   structure and initializes its content to zero/null. */
/*********************************************************/
Environment *CL_CreateEnvironmentDriver(
  CLIPSLexeme **symbolTable,
  CLIPSFloat **floatTable,
  CLIPSInteger **integerTable,
  CLIPSBitMap **bitmapTable,
  CLIPSExternalAddress **externalAddressTable,
  struct functionDefinition *functions)
  {
   struct environmentData *theEnvironment;
   void *theData;

   theEnvironment = (struct environmentData *) malloc(sizeof(struct environmentData));

   if (theEnvironment == NULL)
     {
      printf("\n[ENVRNMNT5] Unable to create new environment.\n");
      return NULL;
     }

   theData = malloc(sizeof(void *) * MAXIMUM_ENVIRONMENT_POSITIONS);

   if (theData == NULL)
     {
      free(theEnvironment);
      printf("\n[ENVRNMNT6] Unable to create environment data.\n");
      return NULL;
     }

   memset(theData,0,sizeof(void *) * MAXIMUM_ENVIRONMENT_POSITIONS);

   theEnvironment->initialized = false;
   theEnvironment->theData = (void **) theData;
   theEnvironment->next = NULL;
   theEnvironment->listOfCleanupEnvironmentFunctions = NULL;
   theEnvironment->context = NULL;

   /*=============================================*/
   /* Allocate storage for the cleanup functions. */
   /*=============================================*/

   theData = malloc(sizeof(void (*)(struct environmentData *)) * MAXIMUM_ENVIRONMENT_POSITIONS);

   if (theData == NULL)
     {
      free(theEnvironment->theData);
      free(theEnvironment);
      printf("\n[ENVRNMNT7] Unable to create environment data.\n");
      return NULL;
     }

   memset(theData,0,sizeof(void (*)(struct environmentData *)) * MAXIMUM_ENVIRONMENT_POSITIONS);
   theEnvironment->cleanupFunctions = (void (**)(Environment *))theData;

   InitializeEnvironment(theEnvironment,symbolTable,floatTable,integerTable,
                         bitmapTable,externalAddressTable,functions);
      
   CL_CleanCurrentGarbageFrame(theEnvironment,NULL);

   return theEnvironment;
  }

/**********************************************/
/* CL_DestroyEnvironment: Destroys the specified */
/*   environment returning all of its memory. */
/**********************************************/
bool CL_DestroyEnvironment(
  Environment *theEnvironment)
  {
   struct environmentCleanupFunction *cleanupPtr;
   int i;
   struct memoryData *theMemData;
   bool rv = true;
/*
   if (CL_EvaluationData(theEnvironment)->CurrentExpression != NULL)
     { return false; }

#if DEFRULE_CONSTRUCT
   if (EngineData(theEnvironment)->ExecutingRule != NULL)
     { return false; }
#endif
*/
   theMemData = MemoryData(theEnvironment);

   CL_ReleaseMem(theEnvironment,-1);

   for (i = 0; i < MAXIMUM_ENVIRONMENT_POSITIONS; i++)
     {
      if (theEnvironment->cleanupFunctions[i] != NULL)
        { (*theEnvironment->cleanupFunctions[i])(theEnvironment); }
     }

   free(theEnvironment->cleanupFunctions);

   for (cleanupPtr = theEnvironment->listOfCleanupEnvironmentFunctions;
        cleanupPtr != NULL;
        cleanupPtr = cleanupPtr->next)
     { (*cleanupPtr->func)(theEnvironment); }

   RemoveEnvironmentCleanupFunctions(theEnvironment);

   CL_ReleaseMem(theEnvironment,-1);

   if ((theMemData->MemoryAmount != 0) || (theMemData->MemoryCalls != 0))
     {
      printf("\n[ENVRNMNT8] Environment data not fully deallocated.\n");
      printf("\n[ENVRNMNT8] MemoryAmount = %lld.\n",theMemData->MemoryAmount);
      printf("\n[ENVRNMNT8] MemoryCalls = %lld.\n",theMemData->MemoryCalls);
      rv = false;
     }

#if (MEM_TABLE_SIZE > 0)
   free(theMemData->MemoryTable);
#endif

   for (i = 0; i < MAXIMUM_ENVIRONMENT_POSITIONS; i++)
     {
      if (theEnvironment->theData[i] != NULL)
        {
         free(theEnvironment->theData[i]);
         theEnvironment->theData[i] = NULL;
        }
     }

   free(theEnvironment->theData);

   free(theEnvironment);

   return rv;
  }

/**************************************************/
/* RemoveEnvironmentCleanupFunctions: Removes the */
/*   list of environment cleanup functions.       */
/**************************************************/
static void RemoveEnvironmentCleanupFunctions(
  struct environmentData *theEnv)
  {
   struct environmentCleanupFunction *nextPtr;

   while (theEnv->listOfCleanupEnvironmentFunctions != NULL)
     {
      nextPtr = theEnv->listOfCleanupEnvironmentFunctions->next;
      free(theEnv->listOfCleanupEnvironmentFunctions);
      theEnv->listOfCleanupEnvironmentFunctions = nextPtr;
     }
  }

/**************************************************/
/* InitializeEnvironment: PerfoCL_rms initialization */
/*   of the KB environment.                       */
/**************************************************/
static void InitializeEnvironment(
  Environment *theEnvironment,
  CLIPSLexeme **symbolTable,
  CLIPSFloat **floatTable,
  CLIPSInteger **integerTable,
  CLIPSBitMap **bitmapTable,
  CLIPSExternalAddress **externalAddressTable,
  struct functionDefinition *functions)
  {
   /*================================================*/
   /* Don't allow the initialization to occur twice. */
   /*================================================*/

   if (theEnvironment->initialized) return;

   /*================================*/
   /* Initialize the memory manager. */
   /*================================*/

   CL_InitializeMemory(theEnvironment);

   /*===================================================*/
   /* Initialize environment data for various features. */
   /*===================================================*/

   CL_InitializeCommandLineData(theEnvironment);
#if CONSTRUCT_COMPILER && (! RUN_TIME)
   CL_InitializeConstructCompilerData(theEnvironment);
#endif
   CL_InitializeConstructData(theEnvironment);
   InitializeCL_EvaluationData(theEnvironment);
   CL_InitializeExternalFunctionData(theEnvironment);
   CL_InitializePrettyPrintData(theEnvironment);
   CL_InitializePrintUtilityData(theEnvironment);
   CL_InitializeScannerData(theEnvironment);
   CL_InitializeSystemDependentData(theEnvironment);
   CL_InitializeUserDataData(theEnvironment);
   CL_InitializeUtilityData(theEnvironment);
#if DEBUGGING_FUNCTIONS
   CL_InitializeCL_WatchData(theEnvironment);
#endif

   /*===============================================*/
   /* Initialize the hash tables for atomic values. */
   /*===============================================*/

   CL_InitializeAtomTables(theEnvironment,symbolTable,floatTable,integerTable,bitmapTable,externalAddressTable);

   /*=========================================*/
   /* Initialize file and string I/O routers. */
   /*=========================================*/

   CL_InitializeDefaultRouters(theEnvironment);

   /*=========================================================*/
   /* Initialize some system dependent features such as time. */
   /*=========================================================*/

   CL_InitializeNonportableFeatures(theEnvironment);

   /*=============================================*/
   /* Register system and user defined functions. */
   /*=============================================*/

   if (functions != NULL)
     { CL_InstallFunctionList(theEnvironment,functions); }

   SystemFunctionDefinitions(theEnvironment);
   CL_UserFunctions(theEnvironment);

   /*====================================*/
   /* Initialize the constraint manager. */
   /*====================================*/

   CL_InitializeConstraints(theEnvironment);

   /*==========================================*/
   /* Initialize the expression hash table and */
   /* pointers to specific functions.          */
   /*==========================================*/

   CL_InitExpressionData(theEnvironment);

   /*===================================*/
   /* Initialize the construct manager. */
   /*===================================*/

#if ! RUN_TIME
   CL_InitializeConstructs(theEnvironment);
#endif

   /*=====================================*/
   /* Initialize the defmodule construct. */
   /*=====================================*/

   CL_AllocateDefmoduleGlobals(theEnvironment);

   /*===================================*/
   /* Initialize the defrule construct. */
   /*===================================*/

#if DEFRULE_CONSTRUCT
   CL_InitializeDefrules(theEnvironment);
#endif

   /*====================================*/
   /* Initialize the deffacts construct. */
   /*====================================*/

#if DEFFACTS_CONSTRUCT
   CL_InitializeDeffacts(theEnvironment);
#endif

   /*=====================================================*/
   /* Initialize the defgeneric and defmethod constructs. */
   /*=====================================================*/

#if DEFGENERIC_CONSTRUCT
   CL_SetupGenericFunctions(theEnvironment);
#endif

   /*=======================================*/
   /* Initialize the deffunction construct. */
   /*=======================================*/

#if DEFFUNCTION_CONSTRUCT
   CL_SetupDeffunctions(theEnvironment);
#endif

   /*=====================================*/
   /* Initialize the defglobal construct. */
   /*=====================================*/

#if DEFGLOBAL_CONSTRUCT
   CL_InitializeDefglobals(theEnvironment);
#endif

   /*=======================================*/
   /* Initialize the deftemplate construct. */
   /*=======================================*/

#if DEFTEMPLATE_CONSTRUCT
   CL_InitializeDeftemplates(theEnvironment);
#endif

   /*=============================*/
   /* Initialize COOL constructs. */
   /*=============================*/

#if OBJECT_SYSTEM
   CL_SetupObjectSystem(theEnvironment);
#endif

   /*=====================================*/
   /* Initialize the defmodule construct. */
   /*=====================================*/

   CL_InitializeDefmodules(theEnvironment);

   /*======================================================*/
   /* Register commands and functions for development use. */
   /*======================================================*/

#if DEVELOPER
   DeveloperCommands(theEnvironment);
#endif

   /*=========================================*/
   /* Install the special function primitives */
   /* used by procedural code in constructs.  */
   /*=========================================*/

   CL_InstallProcedurePrimitives(theEnvironment);

   /*==============================================*/
   /* Install keywords in the symbol table so that */
   /* they are available for command completion.   */
   /*==============================================*/

   InitializeKeywords(theEnvironment);

   /*========================*/
   /* Issue a clear command. */
   /*========================*/

   CL_Clear(theEnvironment);

   /*=============================*/
   /* Initialization is complete. */
   /*=============================*/

   theEnvironment->initialized = true;
  }

/**************************************************/
/* SystemFunctionDefinitions: Sets up definitions */
/*   of system defined functions.                 */
/**************************************************/
static void SystemFunctionDefinitions(
  Environment *theEnv)
  {
   CL_ProceduralFunctionDefinitions(theEnv);
   CL_MiscFunctionDefinitions(theEnv);

#if IO_FUNCTIONS
   CL_IOFunctionDefinitions(theEnv);
#endif

   CL_PredicateFunctionDefinitions(theEnv);
   CL_BasicMathFunctionDefinitions(theEnv);
   CL_FileCommandDefinitions(theEnv);
   CL_SortFunctionDefinitions(theEnv);

#if DEBUGGING_FUNCTIONS
   CL_WatchFunctionDefinitions(theEnv);
#endif

#if MULTIFIELD_FUNCTIONS
   CL_MultifieldFunctionDefinitions(theEnv);
#endif

#if STRING_FUNCTIONS
   CL_StringFunctionDefinitions(theEnv);
#endif

#if EXTENDED_MATH_FUNCTIONS
   CL_ExtendedMathFunctionDefinitions(theEnv);
#endif

#if TEXTPRO_FUNCTIONS
   CL_HelpFunctionDefinitions(theEnv);
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
   CL_ConstructsToCCommandDefinition(theEnv);
#endif

#if PROFILING_FUNCTIONS
   CL_ConstructProfilingFunctionDefinitions(theEnv);
#endif

   CL_ParseFunctionDefinitions(theEnv);
  }

/*********************************************/
/* InitializeKeywords: Adds key words to the */
/*   symbol table so that they are available */
/*   for command completion.                 */
/*********************************************/
static void InitializeKeywords(
  Environment *theEnv)
  {
#if (! RUN_TIME) && WINDOW_INTERFACE
   void *ts;

   /*====================*/
   /* construct keywords */
   /*====================*/

   ts = CL_CreateSymbol(theEnv,"defrule");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"defglobal");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"deftemplate");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"deffacts");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"deffunction");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"defmethod");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"defgeneric");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"defclass");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"defmessage-handler");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"definstances");
   IncrementLexemeCount(ts);

   /*=======================*/
   /* set-strategy keywords */
   /*=======================*/

   ts = CL_CreateSymbol(theEnv,"depth");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"breadth");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"lex");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"mea");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"simplicity");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"complexity");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"random");
   IncrementLexemeCount(ts);

   /*==================================*/
   /* set-salience-evaluation keywords */
   /*==================================*/

   ts = CL_CreateSymbol(theEnv,"when-defined");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"when-activated");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"every-cycle");
   IncrementLexemeCount(ts);

   /*======================*/
   /* deftemplate keywords */
   /*======================*/

   ts = CL_CreateSymbol(theEnv,"field");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"multifield");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"default");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"type");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"allowed-symbols");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"allowed-strings");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"allowed-numbers");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"allowed-integers");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"allowed-floats");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"allowed-values");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"min-number-of-elements");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"max-number-of-elements");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"NONE");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"VARIABLE");
   IncrementLexemeCount(ts);

   /*==================*/
   /* defrule keywords */
   /*==================*/

   ts = CL_CreateSymbol(theEnv,"declare");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"salience");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"test");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"or");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"and");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"not");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"logical");
   IncrementLexemeCount(ts);

   /*===============*/
   /* COOL keywords */
   /*===============*/

   ts = CL_CreateSymbol(theEnv,"is-a");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"role");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"abstract");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"concrete");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"pattern-match");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"reactive");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"non-reactive");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"slot");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"field");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"multiple");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"single");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"storage");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"shared");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"local");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"access");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"read");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"write");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"read-only");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"read-write");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"initialize-only");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"propagation");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"inherit");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"no-inherit");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"source");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"composite");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"exclusive");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"allowed-lexemes");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"allowed-instances");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"around");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"before");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"primary");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"after");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"of");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"self");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"visibility");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"override-message");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"private");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"public");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"create-accessor");
   IncrementLexemeCount(ts);

   /*================*/
   /* watch keywords */
   /*================*/

   ts = CL_CreateSymbol(theEnv,"compilations");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"deffunctions");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"globals");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"rules");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"activations");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"statistics");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"facts");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"generic-functions");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"methods");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"instances");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"slots");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"messages");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"message-handlers");
   IncrementLexemeCount(ts);
   ts = CL_CreateSymbol(theEnv,"focus");
   IncrementLexemeCount(ts);
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
  }

