   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/18/17            */
   /*                                                     */
   /*               INSTANCE FUNCTIONS MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*            Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*            Changed name of variable exp to theExp         */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Link error occurs for the CL_SlotExistError       */
/*            function when OBJECT_SYSTEM is set to 0 in     */
/*            setup.h. DR0865                                */
/*                                                           */
/*            Converted INSTANCE_PATTERN_MATCHING to         */
/*            DEFRULE_CONSTRUCT.                             */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Moved CL_EvaluateAndStoreInDataObject to          */
/*            evaluatn.c                                     */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Fixed slot override default ?NONE bug.         */
/*                                                           */
/*      6.31: Fix for compilation with -std=c89.             */
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

#ifndef _H_insfun

#pragma once

#define _H_insfun

#include "entities.h"
#include "object.h"

typedef struct igarbage
{
  Instance *ins;
  struct igarbage *nxt;
} IGARBAGE;

#define INSTANCE_TABLE_HASH_SIZE 8191
#define InstanceSizeHeuristic(ins)      sizeof(Instance)

void CL_RetainInstance (Instance *);
void CL_ReleaseInstance (Instance *);
void CL_IncrementInstanceCallback (Environment *, Instance *);
void CL_DecrementInstanceCallback (Environment *, Instance *);
void CL_InitializeInstanceTable (Environment *);
void CL_Cleanup_Instances (Environment *, void *);
unsigned CL_HashInstance (CLIPSLexeme *);
void CL_DestroyAll_Instances (Environment *, void *);
void CL_RemoveInstanceData (Environment *, Instance *);
Instance *CL_FindInstanceBySymbol (Environment *, CLIPSLexeme *);
Instance *CL_FindInstanceInModule (Environment *, CLIPSLexeme *, Defmodule *,
				   Defmodule *, bool);
InstanceSlot *CL_FindInstanceSlot (Environment *, Instance *, CLIPSLexeme *);
int CL_FindInstanceTemplateSlot (Environment *, Defclass *, CLIPSLexeme *);
PutSlotError CL_PutSlotValue (Environment *, Instance *, InstanceSlot *,
			      UDFValue *, UDFValue *, const char *);
PutSlotError CL_Direct_PutSlotValue (Environment *, Instance *,
				     InstanceSlot *, UDFValue *, UDFValue *);
PutSlotError CL_ValidSlotValue (Environment *, UDFValue *, SlotDescriptor *,
				Instance *, const char *);
Instance *CL_CheckInstance (UDFContext *);
void CL_NoInstanceError (Environment *, const char *, const char *);
void CL_StaleInstanceAddress (Environment *, const char *, int);
bool CL_Get_InstancesChanged (Environment *);
void Set_InstancesChanged (Environment *, bool);
void CL_PrintSlot (Environment *, const char *, SlotDescriptor *, Instance *,
		   const char *);
void Print_InstanceNameAndClass (Environment *, const char *, Instance *,
				 bool);
void Print_InstanceName (Environment *, const char *, Instance *);
void CL_PrintInstanceLongFo_rm (Environment *, const char *, Instance *);
#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM
void CL_DecrementObjectBasisCount (Environment *, Instance *);
void CL_IncrementObjectBasisCount (Environment *, Instance *);
void CL_MatchObjectFunction (Environment *, Instance *);
bool CL_NetworkSynchronized (Environment *, Instance *);
bool CL_InstanceIsDeleted (Environment *, Instance *);
#endif

#endif /* _H_insfun */
