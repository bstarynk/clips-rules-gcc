   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/06/16             */
   /*                                                     */
   /*                   DEFRULE MODULE                    */
   /*******************************************************/

/*************************************************************/
/* Purpose: Defines basic defrule primitive functions such   */
/*   as allocating and deallocating, traversing, and finding */
/*   defrule data structures.                                */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed DYNAMIC_SALIENCE and                   */
/*            LOGICAL_DEPENDENCIES compilation flags.        */
/*                                                           */
/*            Removed CONFLICT_RESOLUTION_STRATEGIES         */
/*            compilation flag.                              */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove run-time program      */
/*            compiler warnings.                             */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added support for hashed memories.             */
/*                                                           */
/*            Added additional developer statistics to help  */
/*            analyze join network perfo_rmance.              */
/*                                                           */
/*            Added salience groups to improve perfo_rmance   */
/*            with large numbers of activations of different */
/*            saliences.                                     */
/*                                                           */
/*            Added Env_GetDisjunctCount and                  */
/*            Env_GetNthDisjunct functions.                   */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Changed find construct functionality so that    */
/*            imported modules are search when locating a    */
/*            named construct.                               */
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
/*************************************************************/

#include "setup.h"

#if DEFRULE_CONSTRUCT

#include <stdio.h>

#include "agenda.h"
#include "drive.h"
#include "engine.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "pattern.h"
#include "retract.h"
#include "reteutil.h"
#include "rulebsc.h"
#include "rulecom.h"
#include "rulepsr.h"
#include "ruledlt.h"

#if BLOAD || BLOAD_AND_BSAVE || BLOAD_ONLY
#include "bload.h"
#include "rulebin.h"
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
#include "rulecmp.h"
#endif

#include "ruledef.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                   *AllocateModule(Environment *);
   static void                    ReturnModule(Environment *,void *);
   static void                    Initialize_DefruleModules(Environment *);
   static void                    DeallocateDefruleData(Environment *);
   static void                    CL_DestroyDefruleAction(Environment *,ConstructHeader *,void *);
#if RUN_TIME
   static void                    AddBetaMemoriesToRule(Environment *,struct joinNode *);
#endif

/**********************************************************/
/* CL_InitializeDefrules: Initializes the defrule construct. */
/**********************************************************/
void CL_InitializeDefrules(
  Environment *theEnv)
  {
   unsigned long i;
   CL_AllocateEnvironmentData(theEnv,DEFRULE_DATA,sizeof(struct defruleData),DeallocateDefruleData);

   CL_InitializeEngine(theEnv);
   Initialize_Agenda(theEnv);
   CL_InitializePatterns(theEnv);
   Initialize_DefruleModules(theEnv);

   CL_Add_ReservedPatternSymbol(theEnv,"and",NULL);
   CL_Add_ReservedPatternSymbol(theEnv,"not",NULL);
   CL_Add_ReservedPatternSymbol(theEnv,"or",NULL);
   CL_Add_ReservedPatternSymbol(theEnv,"test",NULL);
   CL_Add_ReservedPatternSymbol(theEnv,"logical",NULL);
   CL_Add_ReservedPatternSymbol(theEnv,"exists",NULL);
   CL_Add_ReservedPatternSymbol(theEnv,"forall",NULL);

   CL_DefruleBasicCommands(theEnv);

   CL_DefruleCommands(theEnv);

   DefruleData(theEnv)->DefruleConstruct =
      CL_AddConstruct(theEnv,"defrule","defrules",
                   CL_ParseDefrule,
                   (CL_FindConstructFunction *) CL_FindDefrule,
                   CL_GetConstructNamePointer,CL_GetConstructPPFo_rm,
                   CL_GetConstructModuleItem,
                   (GetNextConstructFunction *) CL_GetNextDefrule,
                   CL_SetNextConstruct,
                   (IsConstructDeletableFunction *) CL_DefruleIsDeletable,
                   (DeleteConstructFunction *) CL_Undefrule,
                   (FreeConstructFunction *) CL_ReturnDefrule);

   DefruleData(theEnv)->AlphaMemoryTable = (ALPHA_MEMORY_HASH **)
                  CL_gm2(theEnv,sizeof (ALPHA_MEMORY_HASH *) * ALPHA_MEMORY_HASH_SIZE);

   for (i = 0; i < ALPHA_MEMORY_HASH_SIZE; i++) DefruleData(theEnv)->AlphaMemoryTable[i] = NULL;

   DefruleData(theEnv)->BetaMemoryResizingFlag = true;

   DefruleData(theEnv)->RightPrimeJoins = NULL;
   DefruleData(theEnv)->LeftPrimeJoins = NULL;
  }

/**************************************************/
/* DeallocateDefruleData: Deallocates environment */
/*    data for the defrule construct.             */
/**************************************************/
static void DeallocateDefruleData(
  Environment *theEnv)
  {
   struct defruleModule *theModuleItem;
   Defmodule *theModule;
   Activation *theActivation, *tmpActivation;
   struct salienceGroup *theGroup, *tmpGroup;

#if BLOAD || BLOAD_AND_BSAVE
   if (CL_Bloaded(theEnv))
     { return; }
#endif

   CL_DoForAllConstructs(theEnv,CL_DestroyDefruleAction,
                      DefruleData(theEnv)->CL_DefruleModuleIndex,false,NULL);

   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      theModuleItem = (struct defruleModule *)
                      CL_GetModuleItem(theEnv,theModule,
                                    DefruleData(theEnv)->CL_DefruleModuleIndex);

      theActivation = theModuleItem->agenda;
      while (theActivation != NULL)
        {
         tmpActivation = theActivation->next;

         rtn_struct(theEnv,activation,theActivation);

         theActivation = tmpActivation;
        }

      theGroup = theModuleItem->groupings;
      while (theGroup != NULL)
        {
         tmpGroup = theGroup->next;

         rtn_struct(theEnv,salienceGroup,theGroup);

         theGroup = tmpGroup;
        }

#if ! RUN_TIME
      rtn_struct(theEnv,defruleModule,theModuleItem);
#endif
     }

   CL_rm(theEnv,DefruleData(theEnv)->AlphaMemoryTable,sizeof (ALPHA_MEMORY_HASH *) * ALPHA_MEMORY_HASH_SIZE);
  }

/********************************************************/
/* CL_DestroyDefruleAction: Action used to remove defrules */
/*   as a result of CL_DestroyEnvironment.                 */
/********************************************************/
static void CL_DestroyDefruleAction(
  Environment *theEnv,
  ConstructHeader *theConstruct,
  void *buffer)
  {
#if MAC_XCD
#pragma unused(buffer)
#endif
   Defrule *theDefrule = (Defrule *) theConstruct;

   CL_DestroyDefrule(theEnv,theDefrule);
  }

/*****************************************************/
/* Initialize_DefruleModules: Initializes the defrule */
/*   construct for use with the defmodule construct. */
/*****************************************************/
static void Initialize_DefruleModules(
  Environment *theEnv)
  {
   DefruleData(theEnv)->CL_DefruleModuleIndex = CL_RegisterModuleItem(theEnv,"defrule",
                                    AllocateModule,
                                    ReturnModule,
#if BLOAD_AND_BSAVE || BLOAD || BLOAD_ONLY
                                    CL_Bload_DefruleModuleReference,
#else
                                    NULL,
#endif
#if CONSTRUCT_COMPILER && (! RUN_TIME)
                                    CL_DefruleCModuleReference,
#else
                                    NULL,
#endif
                                    (CL_FindConstructFunction *) CL_FindDefruleInModule);
  }

/***********************************************/
/* AllocateModule: Allocates a defrule module. */
/***********************************************/
static void *AllocateModule(
  Environment *theEnv)
  {
   struct defruleModule *theItem;

   theItem = get_struct(theEnv,defruleModule);
   theItem->agenda = NULL;
   theItem->groupings = NULL;
   return((void *) theItem);
  }

/***********************************************/
/* ReturnModule: Deallocates a defrule module. */
/***********************************************/
static void ReturnModule(
  Environment *theEnv,
  void *theItem)
  {
   CL_FreeConstructHeaderModule(theEnv,(struct defmoduleItemHeader *) theItem,DefruleData(theEnv)->DefruleConstruct);
   rtn_struct(theEnv,defruleModule,theItem);
  }

/************************************************************/
/* Get_DefruleModuleItem: Returns a pointer to the defmodule */
/*  item for the specified defrule or defmodule.            */
/************************************************************/
struct defruleModule *Get_DefruleModuleItem(
  Environment *theEnv,
  Defmodule *theModule)
  {
   return((struct defruleModule *) CL_GetConstructModuleItemByIndex(theEnv,theModule,DefruleData(theEnv)->CL_DefruleModuleIndex));
  }

/****************************************************************/
/* CL_FindDefrule: Searches for a defrule in the list of defrules. */
/*   Returns a pointer to the defrule if found, otherwise NULL. */
/****************************************************************/
Defrule *CL_FindDefrule(
  Environment *theEnv,
  const char *defruleName)
  {
   return (Defrule *) CL_FindNamedConstructInModuleOrImports(theEnv,defruleName,DefruleData(theEnv)->DefruleConstruct);
  }

/************************************************************************/
/* CL_FindDefruleInModule: Searches for a defrule in the list of defrules. */
/*   Returns a pointer to the defrule if found, otherwise NULL.         */
/************************************************************************/
Defrule *CL_FindDefruleInModule(
  Environment *theEnv,
  const char *defruleName)
  {
   return (Defrule *) CL_FindNamedConstructInModule(theEnv,defruleName,DefruleData(theEnv)->DefruleConstruct);
  }

/************************************************************/
/* CL_GetNextDefrule: If passed a NULL pointer, returns the    */
/*   first defrule in the ListOfDefrules. Otherwise returns */
/*   the next defrule following the defrule passed as an    */
/*   argument.                                              */
/************************************************************/
Defrule *CL_GetNextDefrule(
  Environment *theEnv,
  Defrule *defrulePtr)
  {
   return (Defrule *) CL_GetNextConstructItem(theEnv,&defrulePtr->header,DefruleData(theEnv)->CL_DefruleModuleIndex);
  }

/******************************************************/
/* CL_DefruleIsDeletable: Returns true if a particular   */
/*   defrule can be deleted, otherwise returns false. */
/******************************************************/
bool CL_DefruleIsDeletable(
  Defrule *theDefrule)
  {
   Environment *theEnv = theDefrule->header.env;

   if (! CL_ConstructsDeletable(theEnv))
     { return false; }

   for ( ;
        theDefrule != NULL;
        theDefrule = theDefrule->disjunct)
     { if (theDefrule->executing) return false; }

   if (EngineData(theEnv)->JoinOperationInProgress) return false;

   return true;
  }

/********************************************************/
/* CL_GetDisjunctCount: Returns the number of disjuncts of */
/*   a rule (pe_rmutations caused by the use of or CEs). */
/********************************************************/
long CL_GetDisjunctCount(
  Environment *theEnv,
  Defrule *theDefrule)
  {
   long count = 0;

   for ( ;
        theDefrule != NULL;
        theDefrule = theDefrule->disjunct)
     { count++; }

   return(count);
  }

/*******************************************************/
/* CL_GetNthDisjunct: Returns the nth disjunct of a rule. */
/*   The disjunct indices run from 1 to N rather than  */
/*   0 to N - 1.                                       */
/*******************************************************/
Defrule *CL_GetNthDisjunct(
  Environment *theEnv,
  Defrule *theDefrule,
  long index)
  {
   long count = 0;

   for ( ;
        theDefrule != NULL;
        theDefrule = theDefrule->disjunct)
     {
      count++;
      if (count == index)
        { return theDefrule; }
     }

   return NULL;
  }

#if RUN_TIME

/******************************************/
/* Defrule_RunTimeInitialize:  Initializes */
/*   defrule in a run-time module.        */
/******************************************/
void Defrule_RunTimeInitialize(
  Environment *theEnv,
  struct joinLink *rightPrime,
  struct joinLink *leftPrime)
  {
   Defmodule *theModule;
   Defrule *theRule, *theDisjunct;

   DefruleData(theEnv)->RightPrimeJoins = rightPrime;
   DefruleData(theEnv)->LeftPrimeJoins = leftPrime;

   CL_SaveCurrentModule(theEnv);

   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      CL_SetCurrentModule(theEnv,theModule);
      for (theRule = CL_GetNextDefrule(theEnv,NULL);
           theRule != NULL;
           theRule = CL_GetNextDefrule(theEnv,theRule))
        {
         for (theDisjunct = theRule;
              theDisjunct != NULL;
              theDisjunct = theDisjunct->disjunct)
           {
            theDisjunct->header.env = theEnv;
            AddBetaMemoriesToRule(theEnv,theDisjunct->lastJoin);
           }
        }
     }

   CL_RestoreCurrentModule(theEnv);
  }


/**************************/
/* AddBetaMemoriesToRule: */
/**************************/
static void AddBetaMemoriesToRule(
  Environment *theEnv,
  struct joinNode *theNode)
  {
   CL_AddBetaMemoriesToJoin(theEnv,theNode);

   if (theNode->lastLevel != NULL)
     { AddBetaMemoriesToRule(theEnv,theNode->lastLevel); }

   if (theNode->joinFromTheRight)
     { AddBetaMemoriesToRule(theEnv,(struct joinNode *) theNode->rightSideEntryStructure); }
  }

#endif /* RUN_TIME */

#if RUN_TIME || BLOAD_ONLY || BLOAD || BLOAD_AND_BSAVE

/**************************/
/* CL_AddBetaMemoriesToJoin: */
/**************************/
void CL_AddBetaMemoriesToJoin(
  Environment *theEnv,
  struct joinNode *theNode)
  {
   if ((theNode->leftMemory != NULL) || (theNode->rightMemory != NULL))
     { return; }

   if ((! theNode->firstJoin) || theNode->patternIsExists || theNode-> patternIsNegated || theNode->joinFromTheRight)
     {
      if (theNode->leftHash == NULL)
        {
         theNode->leftMemory = get_struct(theEnv,betaMemory);
         theNode->leftMemory->beta = (struct partialMatch **) CL_genalloc(theEnv,sizeof(struct partialMatch *));
         theNode->leftMemory->beta[0] = NULL;
         theNode->leftMemory->size = 1;
         theNode->leftMemory->count = 0;
         theNode->leftMemory->last = NULL;
        }
      else
        {
         theNode->leftMemory = get_struct(theEnv,betaMemory);
         theNode->leftMemory->beta = (struct partialMatch **) CL_genalloc(theEnv,sizeof(struct partialMatch *) * INITIAL_BETA_HASH_SIZE);
         memset(theNode->leftMemory->beta,0,sizeof(struct partialMatch *) * INITIAL_BETA_HASH_SIZE);
         theNode->leftMemory->size = INITIAL_BETA_HASH_SIZE;
         theNode->leftMemory->count = 0;
         theNode->leftMemory->last = NULL;
        }

      if (theNode->firstJoin && (theNode->patternIsExists || theNode-> patternIsNegated || theNode->joinFromTheRight))
        {
         theNode->leftMemory->beta[0] = CL_CreateEmptyPartialMatch(theEnv);
         theNode->leftMemory->beta[0]->owner = theNode;
        }
     }
   else
     { theNode->leftMemory = NULL; }

   if (theNode->joinFromTheRight)
     {
      if (theNode->leftHash == NULL)
        {
         theNode->rightMemory = get_struct(theEnv,betaMemory);
         theNode->rightMemory->beta = (struct partialMatch **) CL_genalloc(theEnv,sizeof(struct partialMatch *));
         theNode->rightMemory->last = (struct partialMatch **) CL_genalloc(theEnv,sizeof(struct partialMatch *));
         theNode->rightMemory->beta[0] = NULL;
         theNode->rightMemory->last[0] = NULL;
         theNode->rightMemory->size = 1;
         theNode->rightMemory->count = 0;
        }
      else
        {
         theNode->rightMemory = get_struct(theEnv,betaMemory);
         theNode->rightMemory->beta = (struct partialMatch **) CL_genalloc(theEnv,sizeof(struct partialMatch *) * INITIAL_BETA_HASH_SIZE);
         theNode->rightMemory->last = (struct partialMatch **) CL_genalloc(theEnv,sizeof(struct partialMatch *) * INITIAL_BETA_HASH_SIZE);
         memset(theNode->rightMemory->beta,0,sizeof(struct partialMatch **) * INITIAL_BETA_HASH_SIZE);
         memset(theNode->rightMemory->last,0,sizeof(struct partialMatch **) * INITIAL_BETA_HASH_SIZE);
         theNode->rightMemory->size = INITIAL_BETA_HASH_SIZE;
         theNode->rightMemory->count = 0;
        }
     }
   else if (theNode->rightSideEntryStructure == NULL)
     {
      theNode->rightMemory = get_struct(theEnv,betaMemory);
      theNode->rightMemory->beta = (struct partialMatch **) CL_genalloc(theEnv,sizeof(struct partialMatch *));
      theNode->rightMemory->last = (struct partialMatch **) CL_genalloc(theEnv,sizeof(struct partialMatch *));
      theNode->rightMemory->beta[0] = CL_CreateEmptyPartialMatch(theEnv);
      theNode->rightMemory->beta[0]->owner = theNode;
      theNode->rightMemory->beta[0]->rhsMemory = true;
      theNode->rightMemory->last[0] = theNode->rightMemory->beta[0];
      theNode->rightMemory->size = 1;
      theNode->rightMemory->count = 1;
     }
   else
     { theNode->rightMemory = NULL; }
  }

#endif /* RUN_TIME || BLOAD_ONLY || BLOAD || BLOAD_AND_BSAVE */

/*##################################*/
/* Additional Environment Functions */
/*##################################*/

const char *CL_DefruleModule(
  Defrule *theDefrule)
  {
   return CL_GetConstructModuleName(&theDefrule->header);
  }

const char *CL_DefruleName(
  Defrule *theDefrule)
  {
   return CL_GetConstructNameString(&theDefrule->header);
  }

const char *CL_DefrulePPFo_rm(
  Defrule *theDefrule)
  {
   return CL_GetConstructPPFo_rm(&theDefrule->header);
  }

#endif /* DEFRULE_CONSTRUCT */


