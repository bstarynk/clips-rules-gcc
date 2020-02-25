   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*                 AGENDA HEADER FILE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*   Provides functionality for examining, manipulating,     */
/*   adding, and removing activations from the agenda.       */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*      6.23: Corrected compilation errors for files         */
/*            generated by constructs-to-c. DR0861           */
/*                                                           */
/*      6.24: Removed CONFLICT_RESOLUTION_STRATEGIES and     */
/*            DYNAMIC_SALIENCE compilation flags.            */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added Env_GetActivationBasisPPFo_rm function.    */
/*                                                           */
/*      6.30: Added salience groups to improve perfo_rmance   */
/*            with large numbers of activations of different */
/*            saliences.                                     */
/*                                                           */
/*            Borland C (IBM_TBC) and Metrowerks CodeWarrior */
/*            (MAC_MCW, IBM_MCW) are no longer supported.    */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
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
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#ifndef _H_agenda

#pragma once

#define _H_agenda

typedef struct activation Activation;

#include "ruledef.h"
#include "symbol.h"
#include "match.h"

typedef enum
  {
   WHEN_DEFINED,
   WHEN_ACTIVATED,
   EVERY_CYCLE
  } Salience_EvaluationType;

#define MAX_DEFRULE_SALIENCE  10000
#define MIN_DEFRULE_SALIENCE -10000

/*******************/
/* DATA STRUCTURES */
/*******************/

struct activation
  {
   Defrule *theRule;
   struct partialMatch *basis;
   int salience;
   unsigned long long timetag;
   int randomID;
   struct activation *prev;
   struct activation *next;
  };

struct salienceGroup
  {
   int salience;
   struct activation *first;
   struct activation *last;
   struct salienceGroup *next;
   struct salienceGroup *prev;
  };

#include "crstrtgy.h"

#define AGENDA_DATA 17

struct agendaData
  {
#if DEBUGGING_FUNCTIONS
   bool CL_WatchActivations;
#endif
   unsigned long NumberOfActivations;
   unsigned long long CurrentTimetag;
   bool CL_AgendaChanged;
   Salience_EvaluationType Salience_Evaluation;
   StrategyType Strategy;
  };

#define CL_AgendaData(theEnv) ((struct agendaData *) GetEnvironmentData(theEnv,AGENDA_DATA))

/****************************************/
/* GLOBAL EXTERNAL FUNCTION DEFINITIONS */
/****************************************/

   void                    CL_AddActivation(Environment *,Defrule *,PartialMatch *);
   void                    CL_ClearRuleFrom_Agenda(Environment *,Defrule *);
   Activation             *CL_GetNextActivation(Environment *,Activation *);
   struct partialMatch    *CL_GetActivationBasis(Environment *,Activation *);
   const char             *CL_ActivationRuleName(Activation *);
   Defrule                *CL_GetActivationRule(Environment *,Activation *);
   int                     CL_ActivationGetSalience(Activation *);
   int                     CL_ActivationSetSalience(Activation *,int);
   void                    CL_ActivationPPFo_rm(Activation *,String_Builder *);
   void                    CL_GetActivationBasisPPFo_rm(Environment *,char *,size_t,Activation *);
   bool                    CL_MoveActivationToTop(Environment *,Activation *);
   void                    CL_DeleteActivation(Activation *);
   bool                    CL_DetachActivation(Environment *,Activation *);
   void                    CL_DeleteAllActivations(Defmodule *);
   void                    CL_Agenda(Environment *,const char *,Defmodule *);
   void                    CL_RemoveActivation(Environment *,Activation *,bool,bool);
   void                    CL_RemoveAllActivations(Environment *);
   bool                    Get_AgendaChanged(Environment *);
   void                    Set_AgendaChanged(Environment *,bool);
   unsigned long           CL_GetNumberOfActivations(Environment *);
   Salience_EvaluationType  GetSalience_Evaluation(Environment *);
   Salience_EvaluationType  SetSalience_Evaluation(Environment *,Salience_EvaluationType);
   void                    CL_Refresh_Agenda(Defmodule *);
   void                    CL_RefreshAll_Agendas(Environment *);
   void                    Reorder_Agenda(Defmodule *);
   void                    ReorderAll_Agendas(Environment *);
   void                    Initialize_Agenda(Environment *);
   void                    SetSalience_EvaluationCommand(Environment *,UDFContext *,UDFValue *);
   void                    GetSalience_EvaluationCommand(Environment *,UDFContext *,UDFValue *);
   void                    CL_Refresh_AgendaCommand(Environment *,UDFContext *,UDFValue *);
   void                    CL_RefreshCommand(Environment *,UDFContext *,UDFValue *);
   void                    CL_Refresh(Defrule *);
#if DEBUGGING_FUNCTIONS
   void                    CL_AgendaCommand(Environment *,UDFContext *,UDFValue *);
#endif

#endif






