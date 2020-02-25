   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/06/16            */
   /*                                                     */
   /*                 DEFRULE HEADER FILE                 */
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
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
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
/*************************************************************/

#ifndef _H_ruledef

#pragma once

#define _H_ruledef

#define GetDisjunctIndex(r) (r->header.bsaveID)

typedef struct defrule Defrule;
struct defruleModule;

#include "constrct.h"
#include "expressn.h"
#include "network.h"
#include "ruledef.h"

struct defrule
  {
   ConstructHeader header;
   int salience;
   unsigned short localVarCnt;
   unsigned int complexity      : 11;
   unsigned int afterBreakpoint :  1;
   unsigned int watchActivation :  1;
   unsigned int watchFiring     :  1;
   unsigned int auto_Focus       :  1;
   unsigned int executing       :  1;
   struct expr *dynamicSalience;
   struct expr *actions;
   struct joinNode *logicalJoin;
   struct joinNode *lastJoin;
   Defrule *disjunct;
  };

#include "agenda.h"
#include "conscomp.h"
#include "constrnt.h"
#include "cstrccom.h"
#include "evaluatn.h"
#include "moduldef.h"
#include "symbol.h"

struct defruleModule
  {
   struct defmoduleItemHeader header;
   struct salienceGroup *groupings;
   struct activation *agenda;
  };

#ifndef ALPHA_MEMORY_HASH_SIZE
#define ALPHA_MEMORY_HASH_SIZE       63559L
#endif

#define DEFRULE_DATA 16

struct defruleData
  {
   Construct *DefruleConstruct;
   unsigned CL_DefruleModuleIndex;
   unsigned long long CurrentEntityTimeTag;
   struct alphaMemoryHash **AlphaMemoryTable;
   bool BetaMemoryResizingFlag;
   struct joinLink *RightPrimeJoins;
   struct joinLink *LeftPrimeJoins;

#if DEBUGGING_FUNCTIONS
    bool CL_WatchRules;
    int DeletedRuleDebugFlags;
#endif
#if DEVELOPER && (! RUN_TIME) && (! BLOAD_ONLY)
    bool CL_WatchRuleAnalysis;
#endif
#if CONSTRUCT_COMPILER && (! RUN_TIME)
   struct CodeGeneratorItem *DefruleCodeItem;
#endif
  };

#define DefruleData(theEnv) ((struct defruleData *) GetEnvironmentData(theEnv,DEFRULE_DATA))

#define GetPreviousJoin(theJoin) \
   (((theJoin)->joinFromTheRight) ? \
    ((struct joinNode *) (theJoin)->rightSideEntryStructure) : \
    ((theJoin)->lastLevel))
#define GetPatternForJoin(theJoin) \
   (((theJoin)->joinFromTheRight) ? \
    NULL : \
    ((theJoin)->rightSideEntryStructure))

   void                           CL_InitializeDefrules(Environment *);
   Defrule                       *CL_FindDefrule(Environment *,const char *);
   Defrule                       *CL_FindDefruleInModule(Environment *,const char *);
   Defrule                       *CL_GetNextDefrule(Environment *,Defrule *);
   struct defruleModule          *Get_DefruleModuleItem(Environment *,Defmodule *);
   bool                           CL_DefruleIsDeletable(Defrule *);
#if RUN_TIME
   void                           Defrule_RunTimeInitialize(Environment *,struct joinLink *,struct joinLink *);
#endif
#if RUN_TIME || BLOAD_ONLY || BLOAD || BLOAD_AND_BSAVE
   void                           CL_AddBetaMemoriesToJoin(Environment *,struct joinNode *);
#endif
   long                           CL_GetDisjunctCount(Environment *,Defrule *);
   Defrule                       *CL_GetNthDisjunct(Environment *,Defrule *,long);
   const char                    *CL_DefruleModule(Defrule *);
   const char                    *CL_DefruleName(Defrule *);
   const char                    *CL_DefrulePPFo_rm(Defrule *);

#endif /* _H_ruledef */


