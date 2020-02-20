   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/10/18             */
   /*                                                     */
   /*                RULE COMMANDS MODULE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides the matches command. Also provides the  */
/*   the developer commands show-joins and rule-complexity.  */
/*   Also provides the initialization routine which          */
/*   registers rule commands found in other modules.         */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed CONFLICT_RESOLUTION_STRATEGIES         */
/*            INCREMENTAL_RESET, and LOGICAL_DEPENDENCIES    */
/*            compilation flags.                             */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added support for hashed memories.             */
/*                                                           */
/*            Improvements to matches command.               */
/*                                                           */
/*            Add join-activity and join-activity-reset      */
/*            commands.                                      */
/*                                                           */
/*            Added get-beta-memory-resizing and             */
/*            set-beta-memory-resizing functions.            */
/*                                                           */
/*            Added timetag function.                        */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.31: Fixes for show-joins command.                  */
/*                                                           */
/*            Fixes for matches command where the            */
/*            activations listed were not correct if the     */
/*            current module was different than the module   */
/*            for the specified rule.                        */
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
/*            Incremental reset is always enabled.           */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <string.h>

#include "setup.h"

#if DEFRULE_CONSTRUCT

#include "argacces.h"
#include "constant.h"
#include "constrct.h"
#include "crstrtgy.h"
#include "engine.h"
#include "envrnmnt.h"
#include "evaluatn.h"
#include "extnfunc.h"
#include "incrrset.h"
#include "lgcldpnd.h"
#include "memalloc.h"
#include "multifld.h"
#include "pattern.h"
#include "prntutil.h"
#include "reteutil.h"
#include "router.h"
#include "ruledlt.h"
#include "sysdep.h"
#include "utility.h"
#include "watch.h"

#if BLOAD || BLOAD_AND_BSAVE || BLOAD_ONLY
#include "rulebin.h"
#endif

#include "rulecom.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if DEVELOPER
   static void                    ShowJoins(Environment *,Defrule *);
#endif
#if DEBUGGING_FUNCTIONS
   static long long               ListAlphaCL_Matches(Environment *,struct joinInfoCL_rmation *,int);
   static long long               ListBetaCL_Matches(Environment *,struct joinInfoCL_rmation *,long,unsigned short,int);
   static void                    ListBetaCL_JoinActivity(Environment *,struct joinInfoCL_rmation *,long,long,int,UDFValue *);
   static unsigned short          CL_AlphaJoinCountDriver(Environment *,struct joinNode *);
   static unsigned short          CL_BetaJoinCountDriver(Environment *,struct joinNode *);
   static void                    CL_AlphaJoinsDriver(Environment *,struct joinNode *,unsigned short,struct joinInfoCL_rmation *);
   static void                    CL_BetaJoinsDriver(Environment *,struct joinNode *,unsigned short,struct joinInfoCL_rmation *,struct betaMemory *,struct joinNode *);
   static int                     CountPatterns(Environment *,struct joinNode *,bool);
   static const char             *BetaHeaderString(Environment *,struct joinInfoCL_rmation *,long,long);
   static const char             *ActivityHeaderString(Environment *,struct joinInfoCL_rmation *,long,long);
   static void                    CL_JoinActivityCL_Reset(Environment *,ConstructHeader *,void *);
#endif

/****************************************************************/
/* CL_DefruleCommands: Initializes defrule commands and functions. */
/****************************************************************/
void CL_DefruleCommands(
  Environment *theEnv)
  {
#if ! RUN_TIME
   CL_AddUDF(theEnv,"run","v",0,1,"l",CL_RunCommand,"CL_RunCommand",NULL);
   CL_AddUDF(theEnv,"halt","v",0,0,NULL,CL_HaltCommand,"CL_HaltCommand",NULL);
   CL_AddUDF(theEnv,"focus","b",1,UNBOUNDED,"y",CL_FocusCommand,"CL_FocusCommand",NULL);
   CL_AddUDF(theEnv,"clear-focus-stack","v",0,0,NULL,CL_ClearCL_FocusStackCommand,"CL_ClearCL_FocusStackCommand",NULL);
   CL_AddUDF(theEnv,"get-focus-stack","m",0,0,NULL,GetCL_FocusStackFunction,"GetCL_FocusStackFunction",NULL);
   CL_AddUDF(theEnv,"pop-focus","y",0,0,NULL,PopCL_FocusFunction,"PopCL_FocusFunction",NULL);
   CL_AddUDF(theEnv,"get-focus","y",0,0,NULL,GetCL_FocusFunction,"GetCL_FocusFunction",NULL);
#if DEBUGGING_FUNCTIONS
   CL_AddUDF(theEnv,"set-break","v",1,1,"y",CL_SetBreakCommand,"CL_SetBreakCommand",NULL);
   CL_AddUDF(theEnv,"remove-break","v",0,1,"y",CL_RemoveBreakCommand,"CL_RemoveBreakCommand",NULL);
   CL_AddUDF(theEnv,"show-breaks","v",0,1,"y",CL_ShowBreaksCommand,"CL_ShowBreaksCommand",NULL);
   CL_AddUDF(theEnv,"matches","bm",1,2,"y",CL_MatchesCommand,"CL_MatchesCommand",NULL);
   CL_AddUDF(theEnv,"join-activity","bm",1,2,"y",CL_JoinActivityCommand,"CL_JoinActivityCommand",NULL);
   CL_AddUDF(theEnv,"join-activity-reset","v",0,0,NULL,CL_JoinActivityCL_ResetCommand,"CL_JoinActivityCL_ResetCommand",NULL);
   CL_AddUDF(theEnv,"list-focus-stack","v",0,0,NULL,ListCL_FocusStackCommand,"ListCL_FocusStackCommand",NULL);
   CL_AddUDF(theEnv,"dependencies","v",1,1,"infly",CL_DependenciesCommand,"CL_DependenciesCommand",NULL);
   CL_AddUDF(theEnv,"dependents","v",1,1,"infly",CL_DependentsCommand,"CL_DependentsCommand",NULL);

   CL_AddUDF(theEnv,"timetag","l",1,1,"infly" ,CL_TimetagFunction,"CL_TimetagFunction",NULL);
#endif /* DEBUGGING_FUNCTIONS */

   CL_AddUDF(theEnv,"get-beta-memory-resizing","b",0,0,NULL,CL_GetBetaMemoryResizingCommand,"CL_GetBetaMemoryResizingCommand",NULL);
   CL_AddUDF(theEnv,"set-beta-memory-resizing","b",1,1,NULL,CL_SetBetaMemoryResizingCommand,"CL_SetBetaMemoryResizingCommand",NULL);

   CL_AddUDF(theEnv,"get-strategy","y",0,0,NULL,CL_GetStrategyCommand,"CL_GetStrategyCommand",NULL);
   CL_AddUDF(theEnv,"set-strategy","y",1,1,"y",CL_SetStrategyCommand,"CL_SetStrategyCommand",NULL);

#if DEVELOPER && (! BLOAD_ONLY)
   CL_AddUDF(theEnv,"rule-complexity","l",1,1,"y",RuleComplexityCommand,"RuleComplexityCommand",NULL);
   CL_AddUDF(theEnv,"show-joins","v",1,1,"y",ShowJoinsCommand,"ShowJoinsCommand",NULL);
   CL_AddUDF(theEnv,"show-aht","v",0,0,NULL,ShowAlphaHashTable,"ShowAlphaHashTable",NULL);
#if DEBUGGING_FUNCTIONS
   CL_AddCL_WatchItem(theEnv,"rule-analysis",0,&DefruleData(theEnv)->CL_WatchRuleAnalysis,0,NULL,NULL);
#endif
#endif /* DEVELOPER && (! BLOAD_ONLY) */

#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif /* ! RUN_TIME */
  }

/***********************************************/
/* CL_GetBetaMemoryResizing: C access routine     */
/*   for the get-beta-memory-resizing command. */
/***********************************************/
bool CL_GetBetaMemoryResizing(
  Environment *theEnv)
  {
   return DefruleData(theEnv)->BetaMemoryResizingFlag;
  }

/***********************************************/
/* CL_SetBetaMemoryResizing: C access routine     */
/*   for the set-beta-memory-resizing command. */
/***********************************************/
bool CL_SetBetaMemoryResizing(
  Environment *theEnv,
  bool value)
  {
   bool ov;

   ov = DefruleData(theEnv)->BetaMemoryResizingFlag;

   DefruleData(theEnv)->BetaMemoryResizingFlag = value;

   return(ov);
  }

/****************************************************/
/* CL_SetBetaMemoryResizingCommand: H/L access routine */
/*   for the set-beta-memory-resizing command.      */
/****************************************************/
void CL_SetBetaMemoryResizingCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_GetBetaMemoryResizing(theEnv));

   /*=================================================*/
   /* The symbol FALSE disables beta memory resizing. */
   /* Any other value enables beta memory resizing.   */
   /*=================================================*/

   if (! CL_UDFFirstArgument(context,ANY_TYPE_BITS,&theArg))
     { return; }

   if (theArg.value == FalseSymbol(theEnv))
     { CL_SetBetaMemoryResizing(theEnv,false); }
   else
     { CL_SetBetaMemoryResizing(theEnv,true); }
  }

/****************************************************/
/* CL_GetBetaMemoryResizingCommand: H/L access routine */
/*   for the get-beta-memory-resizing command.      */
/****************************************************/
void CL_GetBetaMemoryResizingCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_GetBetaMemoryResizing(theEnv));
  }

/******************************************/
/* GetCL_FocusFunction: H/L access routine   */
/*   for the get-focus function.          */
/******************************************/
void GetCL_FocusFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Defmodule *rv;

   rv = GetCL_Focus(theEnv);

   if (rv == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   returnValue->value = rv->header.name;
  }

/*********************************/
/* GetCL_Focus: C access routine    */
/*   for the get-focus function. */
/*********************************/
Defmodule *GetCL_Focus(
  Environment *theEnv)
  {
   if (EngineData(theEnv)->CurrentCL_Focus == NULL) return NULL;

   return EngineData(theEnv)->CurrentCL_Focus->theModule;
  }

#if DEBUGGING_FUNCTIONS

/****************************************/
/* CL_MatchesCommand: H/L access routine   */
/*   for the matches command.           */
/****************************************/
void CL_MatchesCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *ruleName, *argument;
   Defrule *rulePtr;
   UDFValue theArg;
   Verbosity output;
   CLIPSValue result;

   if (! CL_UDFFirstArgument(context,SYMBOL_BIT,&theArg))
     { return; }

   ruleName = theArg.lexemeValue->contents;

   rulePtr = CL_FindDefrule(theEnv,ruleName);
   if (rulePtr == NULL)
     {
      CL_CantFindItemErrorMessage(theEnv,"defrule",ruleName,true);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   if (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,SYMBOL_BIT,&theArg))
        { return; }

      argument = theArg.lexemeValue->contents;
      if (strcmp(argument,"verbose") == 0)
        { output = VERBOSE; }
      else if (strcmp(argument,"succinct") == 0)
        { output = SUCCINCT; }
      else if (strcmp(argument,"terse") == 0)
        { output = TERSE; }
      else
        {
         CL_UDFInvalidArgumentMessage(context,"symbol with value verbose, succinct, or terse");
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }
     }
   else
     { output = VERBOSE; }

   CL_Matches(rulePtr,output,&result);
   CL_CLIPSToUDFValue(&result,returnValue);
  }

/******************************/
/* CL_Matches: C access routine  */
/*   for the matches command. */
/******************************/
void CL_Matches(
  Defrule *theDefrule,
  Verbosity output,
  CLIPSValue *returnValue)
  {
   Defrule *rulePtr;
   Defrule *topDisjunct = theDefrule;
   long joinIndex;
   unsigned short arraySize;
   struct joinInfoCL_rmation *theInfo;
   long long alphaMatchCount = 0;
   long long betaMatchCount = 0;
   long long activations = 0;
   Activation *agendaPtr;
   Environment *theEnv = theDefrule->header.env;

   /*==========================*/
   /* Set up the return value. */
   /*==========================*/

   returnValue->value = CL_CreateMultifield(theEnv,3L);

   returnValue->multifieldValue->contents[0].integerValue = SymbolData(theEnv)->Zero;
   returnValue->multifieldValue->contents[1].integerValue = SymbolData(theEnv)->Zero;
   returnValue->multifieldValue->contents[2].integerValue = SymbolData(theEnv)->Zero;

   /*=================================================*/
   /* Loop through each of the disjuncts for the rule */
   /*=================================================*/

   for (rulePtr = topDisjunct; rulePtr != NULL; rulePtr = rulePtr->disjunct)
     {
      /*===============================================*/
      /* Create the array containing the list of alpha */
      /* join nodes (those connected to a pattern CE). */
      /*===============================================*/

      arraySize = CL_AlphaJoinCount(theEnv,rulePtr);

      theInfo = CL_CreateJoinArray(theEnv,arraySize);

      CL_AlphaJoins(theEnv,rulePtr,arraySize,theInfo);

      /*=========================*/
      /* List the alpha matches. */
      /*=========================*/

      for (joinIndex = 0; joinIndex < arraySize; joinIndex++)
        {
         alphaMatchCount += ListAlphaCL_Matches(theEnv,&theInfo[joinIndex],output);
         returnValue->multifieldValue->contents[0].integerValue = CL_CreateInteger(theEnv,alphaMatchCount);
        }

      /*================================*/
      /* Free the array of alpha joins. */
      /*================================*/

      CL_FreeJoinArray(theEnv,theInfo,arraySize);

      /*==============================================*/
      /* Create the array containing the list of beta */
      /* join nodes (joins from the right plus joins  */
      /* connected to a pattern CE).                  */
      /*==============================================*/

      arraySize = CL_BetaJoinCount(theEnv,rulePtr);

      theInfo = CL_CreateJoinArray(theEnv,arraySize);

      CL_BetaJoins(theEnv,rulePtr,arraySize,theInfo);

      /*======================================*/
      /* List the beta matches (for all joins */
      /* except the first pattern CE).        */
      /*======================================*/

      for (joinIndex = 1; joinIndex < arraySize; joinIndex++)
        {
         betaMatchCount += ListBetaCL_Matches(theEnv,theInfo,joinIndex,arraySize,output);
         returnValue->multifieldValue->contents[1].integerValue = CL_CreateInteger(theEnv,betaMatchCount);
        }

      /*================================*/
      /* Free the array of alpha joins. */
      /*================================*/

      CL_FreeJoinArray(theEnv,theInfo,arraySize);
     }

   /*===================*/
   /* List activations. */
   /*===================*/

   if (output == VERBOSE)
     { CL_WriteString(theEnv,STDOUT,"Activations\n"); }

   for (agendaPtr = ((struct defruleModule *) topDisjunct->header.whichModule)->agenda;
        agendaPtr != NULL;
        agendaPtr = (struct activation *) CL_GetNextActivation(theEnv,agendaPtr))
     {
      if (CL_GetCL_HaltExecution(theEnv) == true) return;

      if (((struct activation *) agendaPtr)->theRule->header.name == topDisjunct->header.name)
        {
         activations++;

         if (output == VERBOSE)
           {
            CL_PrintPartialMatch(theEnv,STDOUT,CL_GetActivationBasis(theEnv,agendaPtr));
            CL_WriteString(theEnv,STDOUT,"\n");
           }
        }
     }

   if (output == SUCCINCT)
     {
      CL_WriteString(theEnv,STDOUT,"Activations: ");
      CL_WriteInteger(theEnv,STDOUT,activations);
      CL_WriteString(theEnv,STDOUT,"\n");
     }

   if ((activations == 0) && (output == VERBOSE)) CL_WriteString(theEnv,STDOUT," None\n");

   returnValue->multifieldValue->contents[2].integerValue = CL_CreateInteger(theEnv,activations);
  }

/****************************************************/
/* CL_AlphaJoinCountDriver: Driver routine to iterate  */
/*   over a rule's joins to deteCL_rmine the number of */
/*   alpha joins.                                   */
/****************************************************/
static unsigned short CL_AlphaJoinCountDriver(
  Environment *theEnv,
  struct joinNode *theJoin)
  {
   unsigned short alphaCount = 0;

   if (theJoin == NULL)
     { return alphaCount; }

   if (theJoin->joinFromTheRight)
     { return CL_AlphaJoinCountDriver(theEnv,(struct joinNode *) theJoin->rightSideEntryStructure); }
   else if (theJoin->lastLevel != NULL)
     { alphaCount += CL_AlphaJoinCountDriver(theEnv,theJoin->lastLevel); }

   alphaCount++;

   return alphaCount;
  }

/***********************************************/
/* CL_AlphaJoinCount: Returns the number of alpha */
/*   joins associated with the specified rule. */
/***********************************************/
unsigned short CL_AlphaJoinCount(
  Environment *theEnv,
  Defrule *theDefrule)
  {
   return CL_AlphaJoinCountDriver(theEnv,theDefrule->lastJoin->lastLevel);
  }

/***************************************/
/* CL_AlphaJoinsDriver: Driver routine to */
/*   retrieve a rule's alpha joins.    */
/***************************************/
static void CL_AlphaJoinsDriver(
  Environment *theEnv,
  struct joinNode *theJoin,
  unsigned short alphaIndex,
  struct joinInfoCL_rmation *theInfo)
  {
   if (theJoin == NULL)
     { return; }

   if (theJoin->joinFromTheRight)
     {
      CL_AlphaJoinsDriver(theEnv,(struct joinNode *) theJoin->rightSideEntryStructure,alphaIndex,theInfo);
      return;
     }
   else if (theJoin->lastLevel != NULL)
     { CL_AlphaJoinsDriver(theEnv,theJoin->lastLevel,alphaIndex-1,theInfo); }

   theInfo[alphaIndex-1].whichCE = alphaIndex;
   theInfo[alphaIndex-1].theJoin = theJoin;

   return;
  }

/*****************************************/
/* CL_AlphaJoins: Retrieves the alpha joins */
/*   associated with the specified rule. */
/*****************************************/
void CL_AlphaJoins(
  Environment *theEnv,
  Defrule *theDefrule,
  unsigned short alphaCount,
  struct joinInfoCL_rmation *theInfo)
  {
   CL_AlphaJoinsDriver(theEnv,theDefrule->lastJoin->lastLevel,alphaCount,theInfo);
  }

/****************************************************/
/* CL_BetaJoinCountDriver: Driver routine to iterate  */
/*   over a rule's joins to deteCL_rmine the number of */
/*   beta joins.                                   */
/****************************************************/
static unsigned short CL_BetaJoinCountDriver(
  Environment *theEnv,
  struct joinNode *theJoin)
  {
   unsigned short betaCount = 0;

   if (theJoin == NULL)
     { return betaCount; }

   betaCount++;

   if (theJoin->joinFromTheRight)
     { betaCount += CL_BetaJoinCountDriver(theEnv,(struct joinNode *) theJoin->rightSideEntryStructure); }
   else if (theJoin->lastLevel != NULL)
     { betaCount += CL_BetaJoinCountDriver(theEnv,theJoin->lastLevel); }

   return betaCount;
  }

/***********************************************/
/* CL_BetaJoinCount: Returns the number of beta   */
/*   joins associated with the specified rule. */
/***********************************************/
unsigned short CL_BetaJoinCount(
  Environment *theEnv,
  Defrule *theDefrule)
  {
   return CL_BetaJoinCountDriver(theEnv,theDefrule->lastJoin->lastLevel);
  }

/**************************************/
/* CL_BetaJoinsDriver: Driver routine to */
/*   retrieve a rule's beta joins.    */
/**************************************/
static void CL_BetaJoinsDriver(
  Environment *theEnv,
  struct joinNode *theJoin,
  unsigned short betaIndex,
  struct joinInfoCL_rmation *theJoinInfoArray,
  struct betaMemory *lastMemory,
  struct joinNode *nextJoin)
  {
   unsigned short theCE = 0;
   int theCount;
   struct joinNode *tmpPtr;

   if (theJoin == NULL)
     { return; }

   theJoinInfoArray[betaIndex-1].theJoin = theJoin;
   theJoinInfoArray[betaIndex-1].theMemory = lastMemory;
   theJoinInfoArray[betaIndex-1].nextJoin = nextJoin;

   /*===================================*/
   /* DeteCL_rmine the conditional element */
   /* index for this join.              */
   /*===================================*/

   for (tmpPtr = theJoin; tmpPtr != NULL; tmpPtr = tmpPtr->lastLevel)
      { theCE++; }

   theJoinInfoArray[betaIndex-1].whichCE = theCE;

   /*==============================================*/
   /* The end pattern in the range of patterns for */
   /* this join is always the number of patterns   */
   /* reCL_maining to be encountered.                 */
   /*==============================================*/

   theCount = CountPatterns(theEnv,theJoin,true);
   theJoinInfoArray[betaIndex-1].patternEnd = theCount;

   /*========================================================*/
   /* DeteCL_rmine where the block of patterns for a CE begins. */
   /*========================================================*/


   theCount = CountPatterns(theEnv,theJoin,false);
   theJoinInfoArray[betaIndex-1].patternBegin = theCount;

   /*==========================*/
   /* Find the next beta join. */
   /*==========================*/

   if (theJoin->joinFromTheRight)
     { CL_BetaJoinsDriver(theEnv,(struct joinNode *) theJoin->rightSideEntryStructure,betaIndex-1,theJoinInfoArray,theJoin->rightMemory,theJoin); }
   else if (theJoin->lastLevel != NULL)
     { CL_BetaJoinsDriver(theEnv,theJoin->lastLevel,betaIndex-1,theJoinInfoArray,theJoin->leftMemory,theJoin); }

   return;
  }

/*****************************************/
/* CL_BetaJoins: Retrieves the beta joins   */
/*   associated with the specified rule. */
/*****************************************/
void CL_BetaJoins(
  Environment *theEnv,
  Defrule *theDefrule,
  unsigned short betaArraySize,
  struct joinInfoCL_rmation *theInfo)
  {
   CL_BetaJoinsDriver(theEnv,theDefrule->lastJoin->lastLevel,betaArraySize,theInfo,theDefrule->lastJoin->leftMemory,theDefrule->lastJoin);
  }

/***********************************************/
/* CL_CreateJoinArray: Creates a join infoCL_rmation */
/*    array of the specified size.             */
/***********************************************/
struct joinInfoCL_rmation *CL_CreateJoinArray(
   Environment *theEnv,
   unsigned short size)
   {
    if (size == 0) return NULL;

    return (struct joinInfoCL_rmation *) CL_genalloc(theEnv,sizeof(struct joinInfoCL_rmation) * size);
   }

/*******************************************/
/* CL_FreeJoinArray: Frees a join infoCL_rmation */
/*    array of the specified size.         */
/*******************************************/
void CL_FreeJoinArray(
   Environment *theEnv,
   struct joinInfoCL_rmation *theArray,
   unsigned short size)
   {
    if (size == 0) return;

    CL_genfree(theEnv,theArray,sizeof(struct joinInfoCL_rmation) * size);
   }

/*********************/
/* ListAlphaCL_Matches: */
/*********************/
static long long ListAlphaCL_Matches(
  Environment *theEnv,
  struct joinInfoCL_rmation *theInfo,
  int output)
  {
   struct alphaMemoryHash *listOfHashNodes;
   struct partialMatch *listOfCL_Matches;
   long long count;
   struct joinNode *theJoin;
   long long alphaCount = 0;

   if (CL_GetCL_HaltExecution(theEnv) == true)
     { return(alphaCount); }

   theJoin = theInfo->theJoin;

   if (output == VERBOSE)
     {
      CL_WriteString(theEnv,STDOUT,"CL_Matches for Pattern ");
      CL_WriteInteger(theEnv,STDOUT,theInfo->whichCE);
      CL_WriteString(theEnv,STDOUT,"\n");
     }

   if (theJoin->rightSideEntryStructure == NULL)
     {
      if (theJoin->rightMemory->beta[0]->children != NULL)
        { alphaCount += 1; }

      if (output == VERBOSE)
        {
         if (theJoin->rightMemory->beta[0]->children != NULL)
           { CL_WriteString(theEnv,STDOUT,"*\n"); }
         else
           { CL_WriteString(theEnv,STDOUT," None\n"); }
        }
      else if (output == SUCCINCT)
        {
         CL_WriteString(theEnv,STDOUT,"Pattern ");
         CL_WriteInteger(theEnv,STDOUT,theInfo->whichCE);
         CL_WriteString(theEnv,STDOUT,": ");

         if (theJoin->rightMemory->beta[0]->children != NULL)
           { CL_WriteString(theEnv,STDOUT,"1"); }
         else
           { CL_WriteString(theEnv,STDOUT,"0"); }
         CL_WriteString(theEnv,STDOUT,"\n");
        }

      return(alphaCount);
     }

   listOfHashNodes =  ((struct patternNodeHeader *) theJoin->rightSideEntryStructure)->firstHash;

   for (count = 0;
        listOfHashNodes != NULL;
        listOfHashNodes = listOfHashNodes->nextHash)
     {
      listOfCL_Matches = listOfHashNodes->alphaMemory;

      while (listOfCL_Matches != NULL)
        {
         if (CL_GetCL_HaltExecution(theEnv) == true)
           { return(alphaCount); }

         count++;
         if (output == VERBOSE)
           {
            CL_PrintPartialMatch(theEnv,STDOUT,listOfCL_Matches);
            CL_WriteString(theEnv,STDOUT,"\n");
           }
         listOfCL_Matches = listOfCL_Matches->nextInMemory;
        }
     }

   alphaCount += count;

   if ((count == 0) && (output == VERBOSE)) CL_WriteString(theEnv,STDOUT," None\n");

   if (output == SUCCINCT)
     {
      CL_WriteString(theEnv,STDOUT,"Pattern ");
      CL_WriteInteger(theEnv,STDOUT,theInfo->whichCE);
      CL_WriteString(theEnv,STDOUT,": ");
      CL_WriteInteger(theEnv,STDOUT,count);
      CL_WriteString(theEnv,STDOUT,"\n");
     }

   return(alphaCount);
  }

/********************/
/* BetaHeaderString */
/********************/
static const char *BetaHeaderString(
  Environment *theEnv,
  struct joinInfoCL_rmation *infoArray,
  long joinIndex,
  long arraySize)
  {
   struct joinNode *theJoin;
   struct joinInfoCL_rmation *theInfo;
   long i, j, startPosition, endPosition, positionsToPrint = 0;
   bool nestedCEs = false;
   const char *returnString = "";
   long lastIndex;
   char buffer[32];

   /*=============================================*/
   /* DeteCL_rmine which joins need to be traversed. */
   /*=============================================*/

   for (i = 0; i < arraySize; i++)
     { infoArray[i].marked = false; }

   theInfo = &infoArray[joinIndex];
   theJoin = theInfo->theJoin;
   lastIndex = joinIndex;

   while (theJoin != NULL)
     {
      for (i = lastIndex; i >= 0; i--)
        {
         if (infoArray[i].theJoin == theJoin)
           {
            positionsToPrint++;
            infoArray[i].marked = true;
            if (infoArray[i].patternBegin != infoArray[i].patternEnd)
              { nestedCEs = true; }
            lastIndex = i - 1;
            break;
           }
        }
      theJoin = theJoin->lastLevel;
     }

   for (i = 0; i <= joinIndex; i++)
     {
      if (infoArray[i].marked == false) continue;

      positionsToPrint--;
      startPosition = i;
      endPosition = i;

      if (infoArray[i].patternBegin == infoArray[i].patternEnd)
        {
         for (j = i + 1; j <= joinIndex; j++)
           {
            if (infoArray[j].marked == false) continue;

            if (infoArray[j].patternBegin != infoArray[j].patternEnd) break;

            positionsToPrint--;
            i = j;
            endPosition = j;
           }
        }

      theInfo = &infoArray[startPosition];

      CL_gensprintf(buffer,"%d",theInfo->whichCE);
      returnString = CL_AppendStrings(theEnv,returnString,buffer);

      if (nestedCEs)
        {
         if (theInfo->patternBegin == theInfo->patternEnd)
           {
            returnString = CL_AppendStrings(theEnv,returnString," (P");
            CL_gensprintf(buffer,"%d",theInfo->patternBegin);
            returnString = CL_AppendStrings(theEnv,returnString,buffer);
            returnString = CL_AppendStrings(theEnv,returnString,")");
           }
         else
           {
            returnString = CL_AppendStrings(theEnv,returnString," (P");
            CL_gensprintf(buffer,"%d",theInfo->patternBegin);
            returnString = CL_AppendStrings(theEnv,returnString,buffer);
            returnString = CL_AppendStrings(theEnv,returnString," - P");
            CL_gensprintf(buffer,"%d",theInfo->patternEnd);
            returnString = CL_AppendStrings(theEnv,returnString,buffer);
            returnString = CL_AppendStrings(theEnv,returnString,")");
           }
        }

      if (startPosition != endPosition)
        {
         theInfo = &infoArray[endPosition];

         returnString = CL_AppendStrings(theEnv,returnString," - ");
         CL_gensprintf(buffer,"%d",theInfo->whichCE);
         returnString = CL_AppendStrings(theEnv,returnString,buffer);

         if (nestedCEs)
           {
            if (theInfo->patternBegin == theInfo->patternEnd)
              {
               returnString = CL_AppendStrings(theEnv,returnString," (P");
               CL_gensprintf(buffer,"%d",theInfo->patternBegin);
               returnString = CL_AppendStrings(theEnv,returnString,buffer);
               returnString = CL_AppendStrings(theEnv,returnString,")");
              }
            else
              {
               returnString = CL_AppendStrings(theEnv,returnString," (P");
               CL_gensprintf(buffer,"%d",theInfo->patternBegin);
               returnString = CL_AppendStrings(theEnv,returnString,buffer);
               returnString = CL_AppendStrings(theEnv,returnString," - P");
               CL_gensprintf(buffer,"%d",theInfo->patternEnd);
               returnString = CL_AppendStrings(theEnv,returnString,buffer);
               returnString = CL_AppendStrings(theEnv,returnString,")");
              }
           }
        }

      if (positionsToPrint > 0)
        { returnString = CL_AppendStrings(theEnv,returnString," , "); }
     }

   return returnString;
  }

/********************/
/* ListBetaCL_Matches: */
/********************/
static long long ListBetaCL_Matches(
  Environment *theEnv,
  struct joinInfoCL_rmation *infoArray,
  long joinIndex,
  unsigned short arraySize,
  int output)
  {
   long betaCount = 0;
   struct joinInfoCL_rmation *theInfo;
   unsigned long count;

   if (CL_GetCL_HaltExecution(theEnv) == true)
     { return(betaCount); }

   theInfo = &infoArray[joinIndex];

   if (output == VERBOSE)
     {
      CL_WriteString(theEnv,STDOUT,"Partial matches for CEs ");
      CL_WriteString(theEnv,STDOUT,
                     BetaHeaderString(theEnv,infoArray,joinIndex,arraySize));
      CL_WriteString(theEnv,STDOUT,"\n");
     }

   count = CL_PrintBetaMemory(theEnv,STDOUT,theInfo->theMemory,true,"",output);

   betaCount += count;

   if ((output == VERBOSE) && (count == 0))
     { CL_WriteString(theEnv,STDOUT," None\n"); }
   else if (output == SUCCINCT)
     {
      CL_WriteString(theEnv,STDOUT,"CEs ");
      CL_WriteString(theEnv,STDOUT,
                     BetaHeaderString(theEnv,infoArray,joinIndex,arraySize));
      CL_WriteString(theEnv,STDOUT,": ");
      CL_WriteInteger(theEnv,STDOUT,betaCount);
      CL_WriteString(theEnv,STDOUT,"\n");
     }

   return betaCount;
  }

/******************/
/* CountPatterns: */
/******************/
static int CountPatterns(
  Environment *theEnv,
  struct joinNode *theJoin,
  bool followRight)
  {
   int theCount = 0;

   if (theJoin == NULL) return theCount;

   if (theJoin->joinFromTheRight && (followRight == false))
     { theCount++; }

   while (theJoin != NULL)
     {
      if (theJoin->joinFromTheRight)
        {
         if (followRight)
           { theJoin = (struct joinNode *) theJoin->rightSideEntryStructure; }
         else
           { theJoin = theJoin->lastLevel; }
        }
      else
        {
         theCount++;
         theJoin = theJoin->lastLevel;
        }

      followRight = true;
     }

   return theCount;
  }

/*******************************************/
/* CL_JoinActivityCommand: H/L access routine */
/*   for the join-activity command.        */
/*******************************************/
void CL_JoinActivityCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *ruleName, *argument;
   Defrule *rulePtr;
   UDFValue theArg;
   int output;

   if (! CL_UDFFirstArgument(context,SYMBOL_BIT,&theArg))
     { return; }

   ruleName = theArg.lexemeValue->contents;

   rulePtr = CL_FindDefrule(theEnv,ruleName);
   if (rulePtr == NULL)
     {
      CL_CantFindItemErrorMessage(theEnv,"defrule",ruleName,true);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   if (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,SYMBOL_BIT,&theArg))
        { return; }

      argument = theArg.lexemeValue->contents;
      if (strcmp(argument,"verbose") == 0)
        { output = VERBOSE; }
      else if (strcmp(argument,"succinct") == 0)
        { output = SUCCINCT; }
      else if (strcmp(argument,"terse") == 0)
        { output = TERSE; }
      else
        {
         CL_UDFInvalidArgumentMessage(context,"symbol with value verbose, succinct, or terse");
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }
     }
   else
     { output = VERBOSE; }

   CL_JoinActivity(theEnv,rulePtr,output,returnValue);
  }

/************************************/
/* CL_JoinActivity: C access routine   */
/*   for the join-activity command. */
/************************************/
void CL_JoinActivity(
  Environment *theEnv,
  Defrule *theRule,
  int output,
  UDFValue *returnValue)
  {
   Defrule *rulePtr;
   long disjunctCount, disjunctIndex, joinIndex;
   unsigned short arraySize;
   struct joinInfoCL_rmation *theInfo;

   /*==========================*/
   /* Set up the return value. */
   /*==========================*/

   returnValue->begin = 0;
   returnValue->range = 3;
   returnValue->value = CL_CreateMultifield(theEnv,3L);

   returnValue->multifieldValue->contents[0].integerValue = SymbolData(theEnv)->Zero;
   returnValue->multifieldValue->contents[1].integerValue = SymbolData(theEnv)->Zero;
   returnValue->multifieldValue->contents[2].integerValue = SymbolData(theEnv)->Zero;

   /*=================================================*/
   /* Loop through each of the disjuncts for the rule */
   /*=================================================*/

   disjunctCount = CL_GetDisjunctCount(theEnv,theRule);

   for (disjunctIndex = 1; disjunctIndex <= disjunctCount; disjunctIndex++)
     {
      rulePtr = CL_GetNthDisjunct(theEnv,theRule,disjunctIndex);

      /*==============================================*/
      /* Create the array containing the list of beta */
      /* join nodes (joins from the right plus joins  */
      /* connected to a pattern CE).                  */
      /*==============================================*/

      arraySize = CL_BetaJoinCount(theEnv,rulePtr);

      theInfo = CL_CreateJoinArray(theEnv,arraySize);

      CL_BetaJoins(theEnv,rulePtr,arraySize,theInfo);

      /*======================================*/
      /* List the beta matches (for all joins */
      /* except the first pattern CE).        */
      /*======================================*/

      for (joinIndex = 0; joinIndex < arraySize; joinIndex++)
        { ListBetaCL_JoinActivity(theEnv,theInfo,joinIndex,arraySize,output,returnValue); }

      /*================================*/
      /* Free the array of alpha joins. */
      /*================================*/

      CL_FreeJoinArray(theEnv,theInfo,arraySize);
     }
  }

/************************/
/* ActivityHeaderString */
/************************/
static const char *ActivityHeaderString(
  Environment *theEnv,
  struct joinInfoCL_rmation *infoArray,
  long joinIndex,
  long arraySize)
  {
   struct joinNode *theJoin;
   struct joinInfoCL_rmation *theInfo;
   long i;
   bool nestedCEs = false;
   const char *returnString = "";
   long lastIndex;
   char buffer[32];

   /*=============================================*/
   /* DeteCL_rmine which joins need to be traversed. */
   /*=============================================*/

   for (i = 0; i < arraySize; i++)
     { infoArray[i].marked = false; }

   theInfo = &infoArray[joinIndex];
   theJoin = theInfo->theJoin;
   lastIndex = joinIndex;

   while (theJoin != NULL)
     {
      for (i = lastIndex; i >= 0; i--)
        {
         if (infoArray[i].theJoin == theJoin)
           {
            if (infoArray[i].patternBegin != infoArray[i].patternEnd)
              { nestedCEs = true; }
            lastIndex = i - 1;
            break;
           }
        }
      theJoin = theJoin->lastLevel;
     }

   CL_gensprintf(buffer,"%d",theInfo->whichCE);
   returnString = CL_AppendStrings(theEnv,returnString,buffer);
   if (nestedCEs == false)
     { return returnString; }

   if (theInfo->patternBegin == theInfo->patternEnd)
     {
      returnString = CL_AppendStrings(theEnv,returnString," (P");
      CL_gensprintf(buffer,"%d",theInfo->patternBegin);
      returnString = CL_AppendStrings(theEnv,returnString,buffer);

      returnString = CL_AppendStrings(theEnv,returnString,")");
     }
   else
     {
      returnString = CL_AppendStrings(theEnv,returnString," (P");

      CL_gensprintf(buffer,"%d",theInfo->patternBegin);
      returnString = CL_AppendStrings(theEnv,returnString,buffer);

      returnString = CL_AppendStrings(theEnv,returnString," - P");

      CL_gensprintf(buffer,"%d",theInfo->patternEnd);
      returnString = CL_AppendStrings(theEnv,returnString,buffer);

      returnString = CL_AppendStrings(theEnv,returnString,")");
     }

   return returnString;
  }

/*************************/
/* ListBetaCL_JoinActivity: */
/*************************/
static void ListBetaCL_JoinActivity(
  Environment *theEnv,
  struct joinInfoCL_rmation *infoArray,
  long joinIndex,
  long arraySize,
  int output,
  UDFValue *returnValue)
  {
   long long activity = 0;
   long long compares, adds, deletes;
   struct joinNode *theJoin, *nextJoin;
   struct joinInfoCL_rmation *theInfo;

   if (CL_GetCL_HaltExecution(theEnv) == true)
     { return; }

   theInfo = &infoArray[joinIndex];

   theJoin = theInfo->theJoin;
   nextJoin = theInfo->nextJoin;

   compares = theJoin->memoryCompares;
   if (theInfo->nextJoin->joinFromTheRight)
     {
      adds = nextJoin->memoryRightAdds;
      deletes = nextJoin->memoryRightDeletes;
     }
   else
     {
      adds = nextJoin->memoryLeftAdds;
      deletes = nextJoin->memoryLeftDeletes;
     }

   activity = compares + adds + deletes;

   if (output == VERBOSE)
     {
      char buffer[100];

      CL_WriteString(theEnv,STDOUT,"Activity for CE ");
      CL_WriteString(theEnv,STDOUT,
                     ActivityHeaderString(theEnv,infoArray,joinIndex,arraySize));
      CL_WriteString(theEnv,STDOUT,"\n");

      sprintf(buffer,"   Compares: %10lld\n",compares);
      CL_WriteString(theEnv,STDOUT,buffer);
      sprintf(buffer,"   Adds:     %10lld\n",adds);
      CL_WriteString(theEnv,STDOUT,buffer);
      sprintf(buffer,"   Deletes:  %10lld\n",deletes);
      CL_WriteString(theEnv,STDOUT,buffer);
     }
   else if (output == SUCCINCT)
     {
      CL_WriteString(theEnv,STDOUT,"CE ");
      CL_WriteString(theEnv,STDOUT,
                     ActivityHeaderString(theEnv,infoArray,joinIndex,arraySize));
      CL_WriteString(theEnv,STDOUT,": ");
      CL_WriteInteger(theEnv,STDOUT,activity);
      CL_WriteString(theEnv,STDOUT,"\n");
     }

   compares += returnValue->multifieldValue->contents[0].integerValue->contents;
   adds += returnValue->multifieldValue->contents[1].integerValue->contents;
   deletes += returnValue->multifieldValue->contents[2].integerValue->contents;

   returnValue->multifieldValue->contents[0].integerValue = CL_CreateInteger(theEnv,compares);
   returnValue->multifieldValue->contents[1].integerValue = CL_CreateInteger(theEnv,adds);
   returnValue->multifieldValue->contents[2].integerValue = CL_CreateInteger(theEnv,deletes);
  }

/*********************************************/
/* CL_JoinActivityCL_Reset: Sets the join activity */
/*   counts for each rule back to 0.         */
/*********************************************/
static void CL_JoinActivityCL_Reset(
  Environment *theEnv,
  ConstructHeader *theConstruct,
  void *buffer)
  {
#if MAC_XCD
#pragma unused(buffer)
#endif
   Defrule *theDefrule = (Defrule *) theConstruct;
   struct joinNode *theJoin = theDefrule->lastJoin;

   while (theJoin != NULL)
     {
      theJoin->memoryCompares = 0;
      theJoin->memoryLeftAdds = 0;
      theJoin->memoryRightAdds = 0;
      theJoin->memoryLeftDeletes = 0;
      theJoin->memoryRightDeletes = 0;

      if (theJoin->joinFromTheRight)
        { theJoin = (struct joinNode *) theJoin->rightSideEntryStructure; }
      else
        { theJoin = theJoin->lastLevel; }
     }
  }

/************************************************/
/* CL_JoinActivityCL_ResetCommand: H/L access routine */
/*   for the reset-join-activity command.       */
/************************************************/
void CL_JoinActivityCL_ResetCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   CL_DoForAllConstructs(theEnv,
                      CL_JoinActivityCL_Reset,
                      DefruleData(theEnv)->CL_DefruleModuleIndex,true,NULL);
  }

/***************************************/
/* CL_TimetagFunction: H/L access routine */
/*   for the timetag function.         */
/***************************************/
void CL_TimetagFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;
   void *ptr;

   ptr = CL_GetFactOrInstanceArgument(context,1,&theArg);

   if (ptr == NULL)
     {
      returnValue->integerValue = CL_CreateInteger(theEnv,-1LL);
      return;
     }

   returnValue->integerValue = CL_CreateInteger(theEnv,(long long) ((struct patternEntity *) ptr)->timeTag);
  }

#endif /* DEBUGGING_FUNCTIONS */

#if DEVELOPER
/***********************************************/
/* RuleComplexityCommand: H/L access routine   */
/*   for the rule-complexity function.         */
/***********************************************/
void RuleComplexityCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *ruleName;
   Defrule *rulePtr;

   ruleName = CL_GetConstructName(context,"rule-complexity","rule name");
   if (ruleName == NULL)
     {
      returnValue->integerValue = CL_CreateInteger(theEnv,-1);
      return;
     }

   rulePtr = CL_FindDefrule(theEnv,ruleName);
   if (rulePtr == NULL)
     {
      CL_CantFindItemErrorMessage(theEnv,"defrule",ruleName,true);
      returnValue->integerValue = CL_CreateInteger(theEnv,-1);
      return;
     }

   returnValue->integerValue = CL_CreateInteger(theEnv,rulePtr->complexity);
  }

/******************************************/
/* ShowJoinsCommand: H/L access routine   */
/*   for the show-joins command.          */
/******************************************/
void ShowJoinsCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *ruleName;
   Defrule *rulePtr;

   ruleName = CL_GetConstructName(context,"show-joins","rule name");
   if (ruleName == NULL) return;

   rulePtr = CL_FindDefrule(theEnv,ruleName);
   if (rulePtr == NULL)
     {
      CL_CantFindItemErrorMessage(theEnv,"defrule",ruleName,true);
      return;
     }

   ShowJoins(theEnv,rulePtr);

   return;
  }

/*********************************/
/* ShowJoins: C access routine   */
/*   for the show-joins command. */
/*********************************/
static void ShowJoins(
  Environment *theEnv,
  Defrule *theRule)
  {
   Defrule *rulePtr;
   struct joinNode *theJoin;
   struct joinNode *joinList[MAXIMUM_NUMBER_OF_PATTERNS];
   int numberOfJoins;
   char rhsType;
   unsigned int disjunct = 0;
   unsigned long count = 0;

   rulePtr = theRule;

   if ((rulePtr != NULL) && (rulePtr->disjunct != NULL))
     { disjunct = 1; }

   /*=================================================*/
   /* Loop through each of the disjuncts for the rule */
   /*=================================================*/

   while (rulePtr != NULL)
     {
      if (disjunct > 0)
        {
         CL_WriteString(theEnv,STDOUT,"Disjunct #");
         CL_PrintUnsignedInteger(theEnv,STDOUT,disjunct++);
         CL_WriteString(theEnv,STDOUT,"\n");
        }

      /*=====================================*/
      /* DeteCL_rmine the number of join nodes. */
      /*=====================================*/

      numberOfJoins = -1;
      theJoin = rulePtr->lastJoin;
      while (theJoin != NULL)
        {
         if (theJoin->joinFromTheRight)
           {
            numberOfJoins++;
            joinList[numberOfJoins] = theJoin;
            theJoin = (struct joinNode *) theJoin->rightSideEntryStructure;
           }
         else
           {
            numberOfJoins++;
            joinList[numberOfJoins] = theJoin;
            theJoin = theJoin->lastLevel;
           }
        }

      /*====================*/
      /* Display the joins. */
      /*====================*/

      while (numberOfJoins >= 0)
        {
         char buffer[20];

         if (joinList[numberOfJoins]->patternIsNegated)
           { rhsType = 'n'; }
         else if (joinList[numberOfJoins]->patternIsExists)
           { rhsType = 'x'; }
         else
           { rhsType = ' '; }

         CL_gensprintf(buffer,"%2hu%c%c%c%c : ",joinList[numberOfJoins]->depth,
                                     (joinList[numberOfJoins]->firstJoin) ? 'f' : ' ',
                                     rhsType,
                                     (joinList[numberOfJoins]->joinFromTheRight) ? 'j' : ' ',
                                     (joinList[numberOfJoins]->logicalJoin) ? 'l' : ' ');
         CL_WriteString(theEnv,STDOUT,buffer);
         CL_PrintExpression(theEnv,STDOUT,joinList[numberOfJoins]->networkTest);
         CL_WriteString(theEnv,STDOUT,"\n");

         if (joinList[numberOfJoins]->ruleToActivate != NULL)
           {
            CL_WriteString(theEnv,STDOUT,"    RA : ");
            CL_WriteString(theEnv,STDOUT,CL_DefruleName(joinList[numberOfJoins]->ruleToActivate));
            CL_WriteString(theEnv,STDOUT,"\n");
           }

         if (joinList[numberOfJoins]->secondaryNetworkTest != NULL)
           {
            CL_WriteString(theEnv,STDOUT,"    SNT : ");
            CL_PrintExpression(theEnv,STDOUT,joinList[numberOfJoins]->secondaryNetworkTest);
            CL_WriteString(theEnv,STDOUT,"\n");
           }

         if (joinList[numberOfJoins]->leftHash != NULL)
           {
            CL_WriteString(theEnv,STDOUT,"    LH : ");
            CL_PrintExpression(theEnv,STDOUT,joinList[numberOfJoins]->leftHash);
            CL_WriteString(theEnv,STDOUT,"\n");
           }

         if (joinList[numberOfJoins]->rightHash != NULL)
           {
            CL_WriteString(theEnv,STDOUT,"    RH : ");
            CL_PrintExpression(theEnv,STDOUT,joinList[numberOfJoins]->rightHash);
            CL_WriteString(theEnv,STDOUT,"\n");
           }

         if (! joinList[numberOfJoins]->firstJoin)
           {
            CL_WriteString(theEnv,STDOUT,"    LM : ");
            count = CL_PrintBetaMemory(theEnv,STDOUT,joinList[numberOfJoins]->leftMemory,false,"",SUCCINCT);
            if (count == 0)
              { CL_WriteString(theEnv,STDOUT,"None\n"); }
            else
              {
               sprintf(buffer,"%lu\n",count);
               CL_WriteString(theEnv,STDOUT,buffer);
              }
           }

         if (joinList[numberOfJoins]->joinFromTheRight)
           {
            CL_WriteString(theEnv,STDOUT,"    RM : ");
            count = CL_PrintBetaMemory(theEnv,STDOUT,joinList[numberOfJoins]->rightMemory,false,"",SUCCINCT);
            if (count == 0)
              { CL_WriteString(theEnv,STDOUT,"None\n"); }
            else
              {
               sprintf(buffer,"%lu\n",count);
               CL_WriteString(theEnv,STDOUT,buffer);
              }
           }

         numberOfJoins--;
        };

      /*===============================*/
      /* Proceed to the next disjunct. */
      /*===============================*/

      rulePtr = rulePtr->disjunct;
      if (rulePtr != NULL) CL_WriteString(theEnv,STDOUT,"\n");
     }
  }

/******************************************************/
/* ShowAlphaHashTable: Displays the number of entries */
/*   in each slot of the alpha hash table.            */
/******************************************************/
void ShowAlphaHashTable(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
   {
    int i, count;
    long totalCount = 0;
    struct alphaMemoryHash *theEntry;
    struct partialMatch *theMatch;
    char buffer[40];

    for (i = 0; i < ALPHA_MEMORY_HASH_SIZE; i++)
      {
       for (theEntry =  DefruleData(theEnv)->AlphaMemoryTable[i], count = 0;
            theEntry != NULL;
            theEntry = theEntry->next)
         { count++; }

       if (count != 0)
         {
          totalCount += count;
          CL_gensprintf(buffer,"%4d: %4d ->",i,count);
          CL_WriteString(theEnv,STDOUT,buffer);

          for (theEntry =  DefruleData(theEnv)->AlphaMemoryTable[i], count = 0;
               theEntry != NULL;
               theEntry = theEntry->next)
            {
             for (theMatch = theEntry->alphaMemory;
                  theMatch != NULL;
                  theMatch = theMatch->nextInMemory)
               { count++; }

             CL_gensprintf(buffer," %4d",count);
             CL_WriteString(theEnv,STDOUT,buffer);
             if (theEntry->owner->rightHash == NULL)
               { CL_WriteString(theEnv,STDOUT,"*"); }
            }

          CL_WriteString(theEnv,STDOUT,"\n");
         }
      }
    CL_gensprintf(buffer,"Total Count: %ld\n",totalCount);
    CL_WriteString(theEnv,STDOUT,buffer);
   }

#endif /* DEVELOPER */

#endif /* DEFRULE_CONSTRUCT */
