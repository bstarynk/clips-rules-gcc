   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*              DEFRULE BSAVE/BLOAD MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the binary save/load feature for the  */
/*    defrule construct.                                     */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*      Barry Cameron                                        */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed CONFLICT_RESOLUTION_STRATEGIES,        */
/*            DYNAMIC_SALIENCE, and LOGICAL_DEPENDENCIES     */
/*            compilation flags.                             */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Added support for alpha memories.              */
/*                                                           */
/*            Added salience groups to improve perfoCL_rmance   */
/*            with large numbers of activations of different */
/*            saliences.                                     */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFRULE_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)

#include <stdio.h>
#include <string.h>

#include "agenda.h"
#include "bload.h"
#include "bsave.h"
#include "engine.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "moduldef.h"
#include "pattern.h"
#include "reteutil.h"
#include "retract.h"
#include "rulebsc.h"

#include "rulebin.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if BLOAD_AND_BSAVE
   static void                    CL_BsaveFind(Environment *);
   static void                    CL_BsaveExpressions(Environment *,FILE *);
   static void                    CL_BsaveStorage(Environment *,FILE *);
   static void                    CL_BsaveBinaryItem(Environment *,FILE *);
   static void                    CL_BsaveJoins(Environment *,FILE *);
   static void                    CL_BsaveJoin(Environment *,FILE *,struct joinNode *);
   static void                    CL_BsaveDisjuncts(Environment *,FILE *,Defrule *);
   static void                    CL_BsaveTraverseJoins(Environment *,FILE *,struct joinNode *);
   static void                    CL_BsaveLinks(Environment *,FILE *);
   static void                    CL_BsaveTraverseLinks(Environment *,FILE *,struct joinNode *);
   static void                    CL_BsaveLink(FILE *,struct joinLink *);
#endif
   static void                    CL_BloadStorage(Environment *);
   static void                    CL_BloadBinaryItem(Environment *);
   static void                    UpdateCL_DefruleModule(Environment *,void *,unsigned long);
   static void                    UpdateDefrule(Environment *,void *,unsigned long);
   static void                    UpdateJoin(Environment *,void *,unsigned long);
   static void                    UpdateLink(Environment *,void *,unsigned long);
   static void                    CL_ClearCL_Bload(Environment *);
   static void                    DeallocateDefruleCL_BloadData(Environment *);

/*****************************************************/
/* CL_DefruleBinarySetup: Installs the binary save/load */
/*   feature for the defrule construct.              */
/*****************************************************/
void CL_DefruleBinarySetup(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,RULEBIN_DATA,sizeof(struct defruleBinaryData),DeallocateDefruleCL_BloadData);

#if BLOAD_AND_BSAVE
   CL_AddBinaryItem(theEnv,"defrule",20,CL_BsaveFind,CL_BsaveExpressions,
                             CL_BsaveStorage,CL_BsaveBinaryItem,
                             CL_BloadStorage,CL_BloadBinaryItem,
                             CL_ClearCL_Bload);
#endif
#if BLOAD || BLOAD_ONLY
   CL_AddBinaryItem(theEnv,"defrule",20,NULL,NULL,NULL,NULL,
                             CL_BloadStorage,CL_BloadBinaryItem,
                             CL_ClearCL_Bload);
#endif
  }

/*******************************************************/
/* DeallocateDefruleCL_BloadData: Deallocates environment */
/*    data for the defrule bsave functionality.        */
/*******************************************************/
static void DeallocateDefruleCL_BloadData(
  Environment *theEnv)
  {
#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)
   size_t space;
   unsigned long i;
   struct defruleModule *theModuleItem;
   struct activation *theActivation, *tmpActivation;
   struct salienceGroup *theGroup, *tmpGroup;

   for (i = 0; i < DefruleBinaryData(theEnv)->NumberOfJoins; i++)
     {
      CL_DestroyBetaMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i],LHS);
      CL_DestroyBetaMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i],RHS);
      CL_ReturnLeftMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i]);
      CL_ReturnRightMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i]);
     }

   for (i = 0; i < DefruleBinaryData(theEnv)->NumberOfCL_DefruleModules; i++)
     {
      theModuleItem = &DefruleBinaryData(theEnv)->ModuleArray[i];

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
     }

   space = DefruleBinaryData(theEnv)->NumberOfCL_DefruleModules * sizeof(struct defruleModule);
   if (space != 0) CL_genfree(theEnv,DefruleBinaryData(theEnv)->ModuleArray,space);

   space = DefruleBinaryData(theEnv)->NumberOfDefrules * sizeof(Defrule);
   if (space != 0) CL_genfree(theEnv,DefruleBinaryData(theEnv)->DefruleArray,space);

   space = DefruleBinaryData(theEnv)->NumberOfJoins * sizeof(struct joinNode);
   if (space != 0) CL_genfree(theEnv,DefruleBinaryData(theEnv)->JoinArray,space);

   space = DefruleBinaryData(theEnv)->NumberOfLinks * sizeof(struct joinLink);
   if (space != 0) CL_genfree(theEnv,DefruleBinaryData(theEnv)->LinkArray,space);

   if (CL_Bloaded(theEnv))
     { CL_rm(theEnv,DefruleData(theEnv)->AlphaMemoryTable,sizeof(ALPHA_MEMORY_HASH *) * ALPHA_MEMORY_HASH_SIZE); }
#endif
  }

#if BLOAD_AND_BSAVE

/*************************************************************/
/* CL_BsaveFind: DeteCL_rmines the amount of memory needed to save */
/*   the defrule and joinNode data structures in addition to */
/*   the memory needed for their associated expressions.     */
/*************************************************************/
static void CL_BsaveFind(
  Environment *theEnv)
  {
   Defrule *theDefrule, *theDisjunct;
   Defmodule *theModule;

   /*=======================================================*/
   /* If a binary image is already loaded, then temporarily */
   /* save the count values since these will be overwritten */
   /* in the process of saving the binary image.            */
   /*=======================================================*/

   CL_SaveCL_BloadCount(theEnv,DefruleBinaryData(theEnv)->NumberOfCL_DefruleModules);
   CL_SaveCL_BloadCount(theEnv,DefruleBinaryData(theEnv)->NumberOfDefrules);
   CL_SaveCL_BloadCount(theEnv,DefruleBinaryData(theEnv)->NumberOfJoins);
   CL_SaveCL_BloadCount(theEnv,DefruleBinaryData(theEnv)->NumberOfLinks);

   /*====================================================*/
   /* Set the binary save ID for defrule data structures */
   /* and count the number of each type.                 */
   /*====================================================*/

   CL_TagRuleNetwork(theEnv,&DefruleBinaryData(theEnv)->NumberOfCL_DefruleModules,
                         &DefruleBinaryData(theEnv)->NumberOfDefrules,
                         &DefruleBinaryData(theEnv)->NumberOfJoins,
                         &DefruleBinaryData(theEnv)->NumberOfLinks);

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

      /*==================================================*/
      /* Loop through each defrule in the current module. */
      /*==================================================*/

      for (theDefrule = CL_GetNextDefrule(theEnv,NULL);
           theDefrule != NULL;
           theDefrule = CL_GetNextDefrule(theEnv,theDefrule))
        {
         /*================================================*/
         /* Initialize the construct header for the binary */
         /* save. The binary save ID has already been set. */
         /*================================================*/

         CL_MarkConstructHeaderNeededItems(&theDefrule->header,theDefrule->header.bsaveID);

         /*===========================================*/
         /* Count and mark data structures associated */
         /* with dynamic salience.                    */
         /*===========================================*/

         ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(theDefrule->dynamicSalience);
         CL_MarkNeededItems(theEnv,theDefrule->dynamicSalience);

         /*==========================================*/
         /* Loop through each disjunct of the rule   */
         /* counting and marking the data structures */
         /* associated with RHS actions.             */
         /*==========================================*/

         for (theDisjunct = theDefrule;
              theDisjunct != NULL;
              theDisjunct = theDisjunct->disjunct)
           {
            ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(theDisjunct->actions);
            CL_MarkNeededItems(theEnv,theDisjunct->actions);
           }
        }
     }

   /*===============================*/
   /* CL_Reset the bsave tags assigned */
   /* to defrule data structures.   */
   /*===============================*/

   CL_MarkRuleNetwork(theEnv,1);
  }

/************************************************/
/* CL_BsaveExpressions: CL_Saves the expressions used */
/*   by defrules to the binary save file.       */
/************************************************/
static void CL_BsaveExpressions(
  Environment *theEnv,
  FILE *fp)
  {
   Defrule *theDefrule, *theDisjunct;
   Defmodule *theModule;

   /*===========================*/
   /* Loop through each module. */
   /*===========================*/

   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      /*======================================================*/
      /* Set the current module to the module being examined. */
      /*======================================================*/

      CL_SetCurrentModule(theEnv,theModule);

      /*==================================================*/
      /* Loop through each defrule in the current module. */
      /*==================================================*/

      for (theDefrule = CL_GetNextDefrule(theEnv,NULL);
           theDefrule != NULL;
           theDefrule = CL_GetNextDefrule(theEnv,theDefrule))
        {
         /*===========================================*/
         /* CL_Save the dynamic salience of the defrule. */
         /*===========================================*/

         CL_BsaveExpression(theEnv,theDefrule->dynamicSalience,fp);

         /*===================================*/
         /* Loop through each disjunct of the */
         /* defrule and save its RHS actions. */
         /*===================================*/

         for (theDisjunct = theDefrule;
              theDisjunct != NULL;
              theDisjunct = theDisjunct->disjunct)
           { CL_BsaveExpression(theEnv,theDisjunct->actions,fp); }
        }
     }

   /*==============================*/
   /* Set the marked flag for each */
   /* join in the join network.    */
   /*==============================*/

   CL_MarkRuleNetwork(theEnv,1);
  }

/*****************************************************/
/* CL_BsaveStorage: CL_Writes out storage requirements for */
/*   all defrule structures to the binary file       */
/*****************************************************/
static void CL_BsaveStorage(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;
   unsigned long value;

   space = sizeof(long) * 5;
   CL_GenCL_Write(&space,sizeof(size_t),fp);
   CL_GenCL_Write(&DefruleBinaryData(theEnv)->NumberOfCL_DefruleModules,sizeof(long),fp);
   CL_GenCL_Write(&DefruleBinaryData(theEnv)->NumberOfDefrules,sizeof(long),fp);
   CL_GenCL_Write(&DefruleBinaryData(theEnv)->NumberOfJoins,sizeof(long),fp);
   CL_GenCL_Write(&DefruleBinaryData(theEnv)->NumberOfLinks,sizeof(long),fp);

   if (DefruleData(theEnv)->RightPrimeJoins == NULL)
     { value = ULONG_MAX; }
   else
     { value = DefruleData(theEnv)->RightPrimeJoins->bsaveID; }

   CL_GenCL_Write(&value,sizeof(unsigned long),fp);

   if (DefruleData(theEnv)->LeftPrimeJoins == NULL)
     { value = ULONG_MAX; }
   else
     { value = DefruleData(theEnv)->LeftPrimeJoins->bsaveID; }

   CL_GenCL_Write(&value,sizeof(unsigned long),fp);
  }

/*******************************************/
/* CL_BsaveBinaryItem: CL_Writes out all defrule */
/*   structures to the binary file.        */
/*******************************************/
static void CL_BsaveBinaryItem(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;
   Defrule *theDefrule;
   Defmodule *theModule;
   struct defruleModule *theModuleItem;
   struct bsaveCL_DefruleModule tempCL_DefruleModule;

   /*===============================================*/
   /* CL_Write out the space required by the defrules. */
   /*===============================================*/

   space = (DefruleBinaryData(theEnv)->NumberOfDefrules * sizeof(struct bsaveDefrule)) +
           (DefruleBinaryData(theEnv)->NumberOfJoins * sizeof(struct bsaveJoinNode)) +
           (DefruleBinaryData(theEnv)->NumberOfLinks * sizeof(struct bsaveJoinLink)) +
           (DefruleBinaryData(theEnv)->NumberOfCL_DefruleModules * sizeof(struct bsaveCL_DefruleModule));
   CL_GenCL_Write(&space,sizeof(size_t),fp);

   /*===============================================*/
   /* CL_Write out each defrule module data structure. */
   /*===============================================*/

   DefruleBinaryData(theEnv)->NumberOfDefrules = 0;
   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      CL_SetCurrentModule(theEnv,theModule);

      theModuleItem = (struct defruleModule *)
                      CL_GetModuleItem(theEnv,NULL,CL_FindModuleItem(theEnv,"defrule")->moduleIndex);
      CL_AssignCL_BsaveDefmdlItemHdrVals(&tempCL_DefruleModule.header,
                                           &theModuleItem->header);
      CL_GenCL_Write(&tempCL_DefruleModule,sizeof(struct bsaveCL_DefruleModule),fp);
     }

   /*========================================*/
   /* CL_Write out each defrule data structure. */
   /*========================================*/

   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      CL_SetCurrentModule(theEnv,theModule);

      for (theDefrule = CL_GetNextDefrule(theEnv,NULL);
           theDefrule != NULL;
           theDefrule = CL_GetNextDefrule(theEnv,theDefrule))
        { CL_BsaveDisjuncts(theEnv,fp,theDefrule); }
     }

   /*=============================*/
   /* CL_Write out the Rete Network. */
   /*=============================*/

   CL_MarkRuleNetwork(theEnv,1);
   CL_BsaveJoins(theEnv,fp);

   /*===========================*/
   /* CL_Write out the join links. */
   /*===========================*/

   CL_MarkRuleNetwork(theEnv,1);
   CL_BsaveLinks(theEnv,fp);

   /*=============================================================*/
   /* If a binary image was already loaded when the bsave command */
   /* was issued, then restore the counts indicating the number   */
   /* of defrules, defrule modules, and joins in the binary image */
   /* (these were overwritten by the binary save).                */
   /*=============================================================*/

   RestoreCL_BloadCount(theEnv,&DefruleBinaryData(theEnv)->NumberOfCL_DefruleModules);
   RestoreCL_BloadCount(theEnv,&DefruleBinaryData(theEnv)->NumberOfDefrules);
   RestoreCL_BloadCount(theEnv,&DefruleBinaryData(theEnv)->NumberOfJoins);
   RestoreCL_BloadCount(theEnv,&DefruleBinaryData(theEnv)->NumberOfLinks);
  }

/************************************************************/
/* CL_BsaveDisjuncts: CL_Writes out all the disjunct defrule data */
/*   structures for a specific rule to the binary file.     */
/************************************************************/
static void CL_BsaveDisjuncts(
  Environment *theEnv,
  FILE *fp,
  Defrule *theDefrule)
  {
   Defrule *theDisjunct;
   struct bsaveDefrule tempDefrule;
   unsigned long disjunctExpressionCount = 0;
   bool first;

   /*=========================================*/
   /* Loop through each disjunct of the rule. */
   /*=========================================*/

   for (theDisjunct = theDefrule, first = true;
        theDisjunct != NULL;
        theDisjunct = theDisjunct->disjunct, first = false)
     {
      DefruleBinaryData(theEnv)->NumberOfDefrules++;

      /*======================================*/
      /* Set header and miscellaneous values. */
      /*======================================*/

      CL_AssignCL_BsaveConstructHeaderVals(&tempDefrule.header,
                                     &theDisjunct->header);
      tempDefrule.salience = theDisjunct->salience;
      tempDefrule.localVarCnt = theDisjunct->localVarCnt;
      tempDefrule.complexity = theDisjunct->complexity;
      tempDefrule.autoCL_Focus = theDisjunct->autoCL_Focus;

      /*=======================================*/
      /* Set dynamic salience data structures. */
      /*=======================================*/

      if (theDisjunct->dynamicSalience != NULL)
        {
         if (first)
           {
            tempDefrule.dynamicSalience = ExpressionData(theEnv)->ExpressionCount;
            disjunctExpressionCount = ExpressionData(theEnv)->ExpressionCount;
            ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(theDisjunct->dynamicSalience);
           }
         else
           { tempDefrule.dynamicSalience = disjunctExpressionCount; }
        }
      else
        { tempDefrule.dynamicSalience = ULONG_MAX; }

      /*==============================================*/
      /* Set the index to the disjunct's RHS actions. */
      /*==============================================*/

      if (theDisjunct->actions != NULL)
        {
         tempDefrule.actions = ExpressionData(theEnv)->ExpressionCount;
         ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(theDisjunct->actions);
        }
      else
        { tempDefrule.actions = ULONG_MAX; }

      /*=================================*/
      /* Set the index to the disjunct's */
      /* logical join and last join.     */
      /*=================================*/

      tempDefrule.logicalJoin = CL_BsaveJoinIndex(theDisjunct->logicalJoin);
      tempDefrule.lastJoin = CL_BsaveJoinIndex(theDisjunct->lastJoin);

      /*=====================================*/
      /* Set the index to the next disjunct. */
      /*=====================================*/

      if (theDisjunct->disjunct != NULL)
        { tempDefrule.disjunct = DefruleBinaryData(theEnv)->NumberOfDefrules; }
      else
        { tempDefrule.disjunct = ULONG_MAX; }

      /*=================================*/
      /* CL_Write the disjunct to the file. */
      /*=================================*/

      CL_GenCL_Write(&tempDefrule,sizeof(struct bsaveDefrule),fp);
     }
  }

/********************************************/
/* CL_BsaveJoins: CL_Writes out all the join node */
/*   data structures to the binary file.    */
/********************************************/
static void CL_BsaveJoins(
  Environment *theEnv,
  FILE *fp)
  {
   Defrule *rulePtr, *disjunctPtr;
   Defmodule *theModule;

   /*===========================*/
   /* Loop through each module. */
   /*===========================*/

   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      CL_SetCurrentModule(theEnv,theModule);

      /*===========================================*/
      /* Loop through each rule and its disjuncts. */
      /*===========================================*/

      rulePtr = CL_GetNextDefrule(theEnv,NULL);
      while (rulePtr != NULL)
        {
         /*=========================================*/
         /* Loop through each join of the disjunct. */
         /*=========================================*/

         for (disjunctPtr = rulePtr; disjunctPtr != NULL; disjunctPtr = disjunctPtr->disjunct)
           { CL_BsaveTraverseJoins(theEnv,fp,disjunctPtr->lastJoin); }

         /*===========================*/
         /* Move on to the next rule. */
         /*===========================*/

         rulePtr = CL_GetNextDefrule(theEnv,rulePtr);
        }
     }
  }

/**************************************************************/
/* CL_BsaveTraverseJoins: Traverses the join network for a rule. */
/**************************************************************/
static void CL_BsaveTraverseJoins(
  Environment *theEnv,
  FILE *fp,
  struct joinNode *joinPtr)
  {
   for (;
        joinPtr != NULL;
        joinPtr = joinPtr->lastLevel)
     {
      if (joinPtr->marked) CL_BsaveJoin(theEnv,fp,joinPtr);

      if (joinPtr->joinFromTheRight)
        { CL_BsaveTraverseJoins(theEnv,fp,(struct joinNode *) joinPtr->rightSideEntryStructure); }
     }
  }

/********************************************/
/* CL_BsaveJoin: CL_Writes out a single join node */
/*   data structure to the binary file.     */
/********************************************/
static void CL_BsaveJoin(
  Environment *theEnv,
  FILE *fp,
  struct joinNode *joinPtr)
  {
   struct bsaveJoinNode tempJoin;

   joinPtr->marked = 0;
   tempJoin.depth = joinPtr->depth;
   tempJoin.rhsType = joinPtr->rhsType;
   tempJoin.firstJoin = joinPtr->firstJoin;
   tempJoin.logicalJoin = joinPtr->logicalJoin;
   tempJoin.joinFromTheRight = joinPtr->joinFromTheRight;
   tempJoin.patternIsNegated = joinPtr->patternIsNegated;
   tempJoin.patternIsExists = joinPtr->patternIsExists;

   if (joinPtr->joinFromTheRight)
     { tempJoin.rightSideEntryStructure = CL_BsaveJoinIndex(joinPtr->rightSideEntryStructure); }
   else
     { tempJoin.rightSideEntryStructure = ULONG_MAX; }

   tempJoin.lastLevel =  CL_BsaveJoinIndex(joinPtr->lastLevel);
   tempJoin.nextLinks =  CL_BsaveJoinLinkIndex(joinPtr->nextLinks);
   tempJoin.rightMatchNode =  CL_BsaveJoinIndex(joinPtr->rightMatchNode);
   tempJoin.networkTest = CL_HashedExpressionIndex(theEnv,joinPtr->networkTest);
   tempJoin.secondaryNetworkTest = CL_HashedExpressionIndex(theEnv,joinPtr->secondaryNetworkTest);
   tempJoin.leftHash = CL_HashedExpressionIndex(theEnv,joinPtr->leftHash);
   tempJoin.rightHash = CL_HashedExpressionIndex(theEnv,joinPtr->rightHash);

   if (joinPtr->ruleToActivate != NULL)
     {
      tempJoin.ruleToActivate =
         GetDisjunctIndex(joinPtr->ruleToActivate);
     }
   else
     { tempJoin.ruleToActivate = ULONG_MAX; }

   CL_GenCL_Write(&tempJoin,sizeof(struct bsaveJoinNode),fp);
  }

/********************************************/
/* CL_BsaveLinks: CL_Writes out all the join link */
/*   data structures to the binary file.    */
/********************************************/
static void CL_BsaveLinks(
  Environment *theEnv,
  FILE *fp)
  {
   Defrule *rulePtr, *disjunctPtr;
   Defmodule *theModule;
   struct joinLink *theLink;

   for (theLink = DefruleData(theEnv)->LeftPrimeJoins;
        theLink != NULL;
        theLink = theLink->next)
     { CL_BsaveLink(fp,theLink);  }

   for (theLink = DefruleData(theEnv)->RightPrimeJoins;
        theLink != NULL;
        theLink = theLink->next)
     { CL_BsaveLink(fp,theLink);  }

   /*===========================*/
   /* Loop through each module. */
   /*===========================*/

   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      CL_SetCurrentModule(theEnv,theModule);

      /*===========================================*/
      /* Loop through each rule and its disjuncts. */
      /*===========================================*/

      rulePtr = CL_GetNextDefrule(theEnv,NULL);
      while (rulePtr != NULL)
        {
         /*=========================================*/
         /* Loop through each join of the disjunct. */
         /*=========================================*/

         for (disjunctPtr = rulePtr; disjunctPtr != NULL; disjunctPtr = disjunctPtr->disjunct)
           { CL_BsaveTraverseLinks(theEnv,fp,disjunctPtr->lastJoin); }

         /*=======================================*/
         /* Move on to the next rule or disjunct. */
         /*=======================================*/

         rulePtr = CL_GetNextDefrule(theEnv,rulePtr);
        }
     }
  }

/***************************************************/
/* CL_BsaveTraverseLinks: Traverses the join network */
/*   for a rule saving the join links.            */
/**************************************************/
static void CL_BsaveTraverseLinks(
  Environment *theEnv,
  FILE *fp,
  struct joinNode *joinPtr)
  {
   struct joinLink *theLink;

   for (;
        joinPtr != NULL;
        joinPtr = joinPtr->lastLevel)
     {
      if (joinPtr->marked)
        {
         for (theLink = joinPtr->nextLinks;
              theLink != NULL;
              theLink = theLink->next)
           { CL_BsaveLink(fp,theLink); }

         joinPtr->marked = 0;
        }

      if (joinPtr->joinFromTheRight)
        { CL_BsaveTraverseLinks(theEnv,fp,(struct joinNode *) joinPtr->rightSideEntryStructure); }
     }
  }

/********************************************/
/* CL_BsaveLink: CL_Writes out a single join link */
/*   data structure to the binary file.     */
/********************************************/
static void CL_BsaveLink(
  FILE *fp,
  struct joinLink *linkPtr)
  {
   struct bsaveJoinLink tempLink;

   tempLink.enterDirection = linkPtr->enterDirection;
   tempLink.join =  CL_BsaveJoinIndex(linkPtr->join);
   tempLink.next =  CL_BsaveJoinLinkIndex(linkPtr->next);

   CL_GenCL_Write(&tempLink,sizeof(struct bsaveJoinLink),fp);
  }

/***********************************************************/
/* CL_AssignCL_BsavePatternHeaderValues: Assigns the appropriate */
/*   values to a bsave pattern header record.              */
/***********************************************************/
void CL_AssignCL_BsavePatternHeaderValues(
  Environment *theEnv,
  struct bsavePatternNodeHeader *theCL_BsaveHeader,
  struct patternNodeHeader *theHeader)
  {
   theCL_BsaveHeader->multifieldNode = theHeader->multifieldNode;
   theCL_BsaveHeader->entryJoin = CL_BsaveJoinIndex(theHeader->entryJoin);
   theCL_BsaveHeader->rightHash = CL_HashedExpressionIndex(theEnv,theHeader->rightHash);
   theCL_BsaveHeader->singlefieldNode = theHeader->singlefieldNode;
   theCL_BsaveHeader->stopNode = theHeader->stopNode;
   theCL_BsaveHeader->beginSlot = theHeader->beginSlot;
   theCL_BsaveHeader->endSlot = theHeader->endSlot;
   theCL_BsaveHeader->selector = theHeader->selector;
  }

#endif /* BLOAD_AND_BSAVE */

/************************************************/
/* CL_BloadStorage: CL_Loads storage requirements for */
/*   the defrules used by this binary image.    */
/************************************************/
static void CL_BloadStorage(
  Environment *theEnv)
  {
   size_t space;

   /*=================================================*/
   /* DeteCL_rmine the number of defrule, defruleModule, */
   /* and joinNode data structures to be read.        */
   /*=================================================*/

   CL_GenReadBinary(theEnv,&space,sizeof(size_t));
   CL_GenReadBinary(theEnv,&DefruleBinaryData(theEnv)->NumberOfCL_DefruleModules,sizeof(long));
   CL_GenReadBinary(theEnv,&DefruleBinaryData(theEnv)->NumberOfDefrules,sizeof(long));
   CL_GenReadBinary(theEnv,&DefruleBinaryData(theEnv)->NumberOfJoins,sizeof(long));
   CL_GenReadBinary(theEnv,&DefruleBinaryData(theEnv)->NumberOfLinks,sizeof(long));
   CL_GenReadBinary(theEnv,&DefruleBinaryData(theEnv)->RightPrimeIndex,sizeof(long));
   CL_GenReadBinary(theEnv,&DefruleBinaryData(theEnv)->LeftPrimeIndex,sizeof(long));

   /*===================================*/
   /* Allocate the space needed for the */
   /* defruleModule data structures.    */
   /*===================================*/

   if (DefruleBinaryData(theEnv)->NumberOfCL_DefruleModules == 0)
     {
      DefruleBinaryData(theEnv)->ModuleArray = NULL;
      DefruleBinaryData(theEnv)->DefruleArray = NULL;
      DefruleBinaryData(theEnv)->JoinArray = NULL;
     }

   space = DefruleBinaryData(theEnv)->NumberOfCL_DefruleModules * sizeof(struct defruleModule);
   DefruleBinaryData(theEnv)->ModuleArray = (struct defruleModule *) CL_genalloc(theEnv,space);

   /*===============================*/
   /* Allocate the space needed for */
   /* the defrule data structures.  */
   /*===============================*/

   if (DefruleBinaryData(theEnv)->NumberOfDefrules == 0)
     {
      DefruleBinaryData(theEnv)->DefruleArray = NULL;
      DefruleBinaryData(theEnv)->JoinArray = NULL;
      return;
     }

   space = DefruleBinaryData(theEnv)->NumberOfDefrules * sizeof(Defrule);
   DefruleBinaryData(theEnv)->DefruleArray = (Defrule *) CL_genalloc(theEnv,space);

   /*===============================*/
   /* Allocate the space needed for */
   /* the joinNode data structures. */
   /*===============================*/

   space = DefruleBinaryData(theEnv)->NumberOfJoins * sizeof(struct joinNode);
   DefruleBinaryData(theEnv)->JoinArray = (struct joinNode *) CL_genalloc(theEnv,space);

   /*===============================*/
   /* Allocate the space needed for */
   /* the joinNode data structures. */
   /*===============================*/

   space = DefruleBinaryData(theEnv)->NumberOfLinks * sizeof(struct joinLink);
   DefruleBinaryData(theEnv)->LinkArray = (struct joinLink *) CL_genalloc(theEnv,space);
  }

/****************************************************/
/* CL_BloadBinaryItem: CL_Loads and refreshes the defrule */
/*   constructs used by this binary image.          */
/****************************************************/
static void CL_BloadBinaryItem(
  Environment *theEnv)
  {
   size_t space;

   /*======================================================*/
   /* Read in the amount of space used by the binary image */
   /* (this is used to skip the construct in the event it  */
   /* is not available in the version being run).          */
   /*======================================================*/

   CL_GenReadBinary(theEnv,&space,sizeof(size_t));

   /*===========================================*/
   /* Read in the defruleModule data structures */
   /* and refresh the pointers.                 */
   /*===========================================*/

   CL_BloadandCL_Refresh(theEnv,DefruleBinaryData(theEnv)->NumberOfCL_DefruleModules,
                   sizeof(struct bsaveCL_DefruleModule),UpdateCL_DefruleModule);

   /*=====================================*/
   /* Read in the defrule data structures */
   /* and refresh the pointers.           */
   /*=====================================*/

   CL_BloadandCL_Refresh(theEnv,DefruleBinaryData(theEnv)->NumberOfDefrules,
                   sizeof(struct bsaveDefrule),UpdateDefrule);

   /*======================================*/
   /* Read in the joinNode data structures */
   /* and refresh the pointers.            */
   /*======================================*/

   CL_BloadandCL_Refresh(theEnv,DefruleBinaryData(theEnv)->NumberOfJoins,
                   sizeof(struct bsaveJoinNode),UpdateJoin);

   /*======================================*/
   /* Read in the joinLink data structures */
   /* and refresh the pointers.            */
   /*======================================*/

   CL_BloadandCL_Refresh(theEnv,DefruleBinaryData(theEnv)->NumberOfLinks,
                   sizeof(struct bsaveJoinLink),UpdateLink);

   DefruleData(theEnv)->RightPrimeJoins = CL_BloadJoinLinkPointer(DefruleBinaryData(theEnv)->RightPrimeIndex);
   DefruleData(theEnv)->LeftPrimeJoins = CL_BloadJoinLinkPointer(DefruleBinaryData(theEnv)->LeftPrimeIndex);
  }

/**********************************************/
/* UpdateCL_DefruleModule: CL_Bload refresh routine */
/*   for defrule module data structures.      */
/**********************************************/
static void UpdateCL_DefruleModule(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsaveCL_DefruleModule *bdmPtr;

   bdmPtr = (struct bsaveCL_DefruleModule *) buf;
   CL_UpdateDefmoduleItemHeader(theEnv,&bdmPtr->header,&DefruleBinaryData(theEnv)->ModuleArray[obji].header,
                             sizeof(Defrule),
                             (void *) DefruleBinaryData(theEnv)->DefruleArray);
   DefruleBinaryData(theEnv)->ModuleArray[obji].agenda = NULL;
   DefruleBinaryData(theEnv)->ModuleArray[obji].groupings = NULL;

  }

/****************************************/
/* UpdateDefrule: CL_Bload refresh routine */
/*   for defrule data structures.       */
/****************************************/
static void UpdateDefrule(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsaveDefrule *br;

   br = (struct bsaveDefrule *) buf;
   CL_UpdateConstructHeader(theEnv,&br->header,&DefruleBinaryData(theEnv)->DefruleArray[obji].header,DEFRULE,
                         sizeof(struct defruleModule),(void *) DefruleBinaryData(theEnv)->ModuleArray,
                         sizeof(Defrule),(void *) DefruleBinaryData(theEnv)->DefruleArray);

   DefruleBinaryData(theEnv)->DefruleArray[obji].dynamicSalience = ExpressionPointer(br->dynamicSalience);

   DefruleBinaryData(theEnv)->DefruleArray[obji].actions = ExpressionPointer(br->actions);
   DefruleBinaryData(theEnv)->DefruleArray[obji].logicalJoin = CL_BloadJoinPointer(br->logicalJoin);
   DefruleBinaryData(theEnv)->DefruleArray[obji].lastJoin = CL_BloadJoinPointer(br->lastJoin);
   DefruleBinaryData(theEnv)->DefruleArray[obji].disjunct = CL_BloadDefrulePointer(DefruleBinaryData(theEnv)->DefruleArray,br->disjunct);
   DefruleBinaryData(theEnv)->DefruleArray[obji].salience = br->salience;
   DefruleBinaryData(theEnv)->DefruleArray[obji].localVarCnt = br->localVarCnt;
   DefruleBinaryData(theEnv)->DefruleArray[obji].complexity = br->complexity;
   DefruleBinaryData(theEnv)->DefruleArray[obji].autoCL_Focus = br->autoCL_Focus;
   DefruleBinaryData(theEnv)->DefruleArray[obji].executing = 0;
   DefruleBinaryData(theEnv)->DefruleArray[obji].afterBreakpoint = 0;
#if DEBUGGING_FUNCTIONS
   DefruleBinaryData(theEnv)->DefruleArray[obji].watchActivation = CL_AgendaData(theEnv)->CL_WatchActivations;
   DefruleBinaryData(theEnv)->DefruleArray[obji].watchFiring = DefruleData(theEnv)->CL_WatchRules;
#endif
  }

/*************************************/
/* UpdateJoin: CL_Bload refresh routine */
/*   for joinNode data structures.   */
/*************************************/
static void UpdateJoin(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsaveJoinNode *bj;

   bj = (struct bsaveJoinNode *) buf;
   DefruleBinaryData(theEnv)->JoinArray[obji].firstJoin = bj->firstJoin;
   DefruleBinaryData(theEnv)->JoinArray[obji].logicalJoin = bj->logicalJoin;
   DefruleBinaryData(theEnv)->JoinArray[obji].joinFromTheRight = bj->joinFromTheRight;
   DefruleBinaryData(theEnv)->JoinArray[obji].patternIsNegated = bj->patternIsNegated;
   DefruleBinaryData(theEnv)->JoinArray[obji].patternIsExists = bj->patternIsExists;
   DefruleBinaryData(theEnv)->JoinArray[obji].depth = bj->depth;
   DefruleBinaryData(theEnv)->JoinArray[obji].rhsType = bj->rhsType;
   DefruleBinaryData(theEnv)->JoinArray[obji].networkTest = HashedExpressionPointer(bj->networkTest);
   DefruleBinaryData(theEnv)->JoinArray[obji].secondaryNetworkTest = HashedExpressionPointer(bj->secondaryNetworkTest);
   DefruleBinaryData(theEnv)->JoinArray[obji].leftHash = HashedExpressionPointer(bj->leftHash);
   DefruleBinaryData(theEnv)->JoinArray[obji].rightHash = HashedExpressionPointer(bj->rightHash);
   DefruleBinaryData(theEnv)->JoinArray[obji].nextLinks = CL_BloadJoinLinkPointer(bj->nextLinks);
   DefruleBinaryData(theEnv)->JoinArray[obji].lastLevel = CL_BloadJoinPointer(bj->lastLevel);

   if (bj->joinFromTheRight == true)
     { DefruleBinaryData(theEnv)->JoinArray[obji].rightSideEntryStructure =  (void *) CL_BloadJoinPointer(bj->rightSideEntryStructure); }
   else
     { DefruleBinaryData(theEnv)->JoinArray[obji].rightSideEntryStructure = NULL; }

   DefruleBinaryData(theEnv)->JoinArray[obji].rightMatchNode = CL_BloadJoinPointer(bj->rightMatchNode);
   DefruleBinaryData(theEnv)->JoinArray[obji].ruleToActivate = CL_BloadDefrulePointer(DefruleBinaryData(theEnv)->DefruleArray,bj->ruleToActivate);
   DefruleBinaryData(theEnv)->JoinArray[obji].initialize = 0;
   DefruleBinaryData(theEnv)->JoinArray[obji].marked = 0;
   DefruleBinaryData(theEnv)->JoinArray[obji].bsaveID = 0L;
   DefruleBinaryData(theEnv)->JoinArray[obji].leftMemory = NULL;
   DefruleBinaryData(theEnv)->JoinArray[obji].rightMemory = NULL;

   CL_AddBetaMemoriesToJoin(theEnv,&DefruleBinaryData(theEnv)->JoinArray[obji]);
  }

/*************************************/
/* UpdateLink: CL_Bload refresh routine */
/*   for joinLink data structures.   */
/*************************************/
static void UpdateLink(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   struct bsaveJoinLink *bj;

   bj = (struct bsaveJoinLink *) buf;
   DefruleBinaryData(theEnv)->LinkArray[obji].enterDirection = bj->enterDirection;
   DefruleBinaryData(theEnv)->LinkArray[obji].next = CL_BloadJoinLinkPointer(bj->next);
   DefruleBinaryData(theEnv)->LinkArray[obji].join = CL_BloadJoinPointer(bj->join);
  }

/************************************************************/
/* CL_UpdatePatternNodeHeader: CL_Refreshes the values in pattern */
/*   node headers from the loaded binary image.             */
/************************************************************/
void CL_UpdatePatternNodeHeader(
  Environment *theEnv,
  struct patternNodeHeader *theHeader,
  struct bsavePatternNodeHeader *theCL_BsaveHeader)
  {
   struct joinNode *theJoin;

   theHeader->singlefieldNode = theCL_BsaveHeader->singlefieldNode;
   theHeader->multifieldNode = theCL_BsaveHeader->multifieldNode;
   theHeader->stopNode = theCL_BsaveHeader->stopNode;
   theHeader->beginSlot = theCL_BsaveHeader->beginSlot;
   theHeader->endSlot = theCL_BsaveHeader->endSlot;
   theHeader->selector = theCL_BsaveHeader->selector;
   theHeader->initialize = 0;
   theHeader->marked = 0;
   theHeader->firstHash = NULL;
   theHeader->lastHash = NULL;
   theHeader->rightHash = HashedExpressionPointer(theCL_BsaveHeader->rightHash);

   theJoin = CL_BloadJoinPointer(theCL_BsaveHeader->entryJoin);
   theHeader->entryJoin = theJoin;

   while (theJoin != NULL)
     {
      theJoin->rightSideEntryStructure = (void *) theHeader;
      theJoin = theJoin->rightMatchNode;
     }
  }

/**************************************/
/* CL_ClearCL_Bload: Defrule clear routine  */
/*   when a binary load is in effect. */
/**************************************/
static void CL_ClearCL_Bload(
  Environment *theEnv)
  {
   size_t space;
   unsigned long i;
   struct patternParser *theParser = NULL;
   struct patternEntity *theEntity = NULL;
   Defmodule *theModule;

   /*===========================================*/
   /* Delete all known entities before removing */
   /* the defrule data structures.              */
   /*===========================================*/

   CL_GetNextPatternEntity(theEnv,&theParser,&theEntity);
   while (theEntity != NULL)
     {
      (*theEntity->theInfo->base.deleteFunction)(theEntity,theEnv);
      theEntity = NULL;
      CL_GetNextPatternEntity(theEnv,&theParser,&theEntity);
     }

   /*=========================================*/
   /* Remove all activations from the agenda. */
   /*=========================================*/

   CL_SaveCurrentModule(theEnv);
   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      CL_SetCurrentModule(theEnv,theModule);
      CL_RemoveAllActivations(theEnv);
     }
   CL_RestoreCurrentModule(theEnv);
   CL_ClearCL_FocusStack(theEnv);

   /*==========================================================*/
   /* Remove all partial matches from the beta memories in the */
   /* join network. Alpha memories do not need to be examined  */
   /* since all pattern entities have been deleted by now.     */
   /*==========================================================*/

   for (i = 0; i < DefruleBinaryData(theEnv)->NumberOfJoins; i++)
     {
      CL_FlushBetaMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i],LHS);
      CL_ReturnLeftMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i]);
      CL_FlushBetaMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i],RHS);
      CL_ReturnRightMemory(theEnv,&DefruleBinaryData(theEnv)->JoinArray[i]);
     }

   /*================================================*/
   /* Decrement the symbol count for each rule name. */
   /*================================================*/

   for (i = 0; i < DefruleBinaryData(theEnv)->NumberOfDefrules; i++)
     { CL_UnmarkConstructHeader(theEnv,&DefruleBinaryData(theEnv)->DefruleArray[i].header); }

   /*==================================================*/
   /* Return the space allocated for the bload arrays. */
   /*==================================================*/

   space = DefruleBinaryData(theEnv)->NumberOfCL_DefruleModules * sizeof(struct defruleModule);
   if (space != 0) CL_genfree(theEnv,DefruleBinaryData(theEnv)->ModuleArray,space);
   DefruleBinaryData(theEnv)->NumberOfCL_DefruleModules = 0;

   space = DefruleBinaryData(theEnv)->NumberOfDefrules * sizeof(Defrule);
   if (space != 0) CL_genfree(theEnv,DefruleBinaryData(theEnv)->DefruleArray,space);
   DefruleBinaryData(theEnv)->NumberOfDefrules = 0;

   space = DefruleBinaryData(theEnv)->NumberOfJoins * sizeof(struct joinNode);
   if (space != 0) CL_genfree(theEnv,DefruleBinaryData(theEnv)->JoinArray,space);
   DefruleBinaryData(theEnv)->NumberOfJoins = 0;

   space = DefruleBinaryData(theEnv)->NumberOfLinks * sizeof(struct joinLink);
   if (space != 0) CL_genfree(theEnv,DefruleBinaryData(theEnv)->LinkArray,space);
   DefruleBinaryData(theEnv)->NumberOfLinks = 0;

   DefruleData(theEnv)->RightPrimeJoins = NULL;
   DefruleData(theEnv)->LeftPrimeJoins = NULL;
  }

/*******************************************************/
/* CL_BloadCL_DefruleModuleReference: Returns the defrule    */
/*   module pointer for using with the bload function. */
/*******************************************************/
void *CL_BloadCL_DefruleModuleReference(
  Environment *theEnv,
  unsigned long theIndex)
  {
   return ((void *) &DefruleBinaryData(theEnv)->ModuleArray[theIndex]);
  }

#endif /* DEFRULE_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME) */


