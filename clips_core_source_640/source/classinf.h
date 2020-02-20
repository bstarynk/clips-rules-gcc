   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*     CLASS INFO PROGRAMMATIC ACCESS HEADER FILE      */
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
/*                                                            */
/*      6.24: Added allowed-classes slot facet.              */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Borland C (IBM_TBC) and Metrowerks CodeWarrior */
/*            (MAC_MCW, IBM_MCW) are no longer supported.    */
/*                                                           */
/*            Changed integer type/precision.                */
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

#ifndef _H_classinf

#pragma once

#define _H_classinf

#include "evaluatn.h"

   void                           CL_ClassAbstractPCommand(Environment *,UDFContext *,UDFValue *);
#if DEFRULE_CONSTRUCT
   void                           CL_ClassReactivePCommand(Environment *,UDFContext *,UDFValue *);
#endif
   Defclass                      *CL_ClassInfoFnxArgs(UDFContext *,const char *,bool *);
   void                           CL_ClassSlotsCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_ClassSuperclassesCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_ClassSubclassesCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_GetDefmessageHandlersListCmd(Environment *,UDFContext *,UDFValue *);
   void                           CL_SlotFacetsCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_SlotSourcesCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_SlotTypesCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_SlotAllowedValuesCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_SlotAllowedClassesCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_SlotRangeCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_SlotCardinalityCommand(Environment *,UDFContext *,UDFValue *);
   bool                           CL_ClassAbstractP(Defclass *);
#if DEFRULE_CONSTRUCT
   bool                           CL_ClassReactiveP(Defclass *);
#endif
   void                           CL_ClassSlots(Defclass *,CLIPSValue *,bool);
   void                           CL_GetDefmessageHandlerList(Environment *,Defclass *,CLIPSValue *,bool);
   void                           CL_ClassSuperclasses(Defclass *,CLIPSValue *,bool);
   void                           CL_ClassSubclasses(Defclass *,CLIPSValue *,bool);
   void                           CL_ClassSubclassAddresses(Environment *,Defclass *,UDFValue *,bool);
   bool                           CL_SlotFacets(Defclass *,const char *,CLIPSValue *);
   bool                           CL_SlotSources(Defclass *,const char *,CLIPSValue *);
   bool                           CL_SlotTypes(Defclass *,const char *,CLIPSValue *);
   bool                           CL_SlotAllowedValues(Defclass *,const char *,CLIPSValue *);
   bool                           CL_SlotAllowedClasses(Defclass *,const char *,CLIPSValue *);
   bool                           CL_SlotRange(Defclass *,const char *,CLIPSValue *);
   bool                           CL_SlotCardinality(Defclass *,const char *,CLIPSValue *);

#endif /* _H_classinf */





