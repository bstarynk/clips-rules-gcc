   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/13/17            */
   /*                                                     */
   /*          INSTANCE PRIMITIVE SUPPORT MODULE          */
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
/*      6.24: Removed LOGICAL_DEPENDENCIES compilation flag. */
/*                                                           */
/*            Converted INSTANCE_PATTERN_MATCHING to         */
/*            DEFRULE_CONSTRUCT.                             */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Used CL_gensprintf instead of sprintf.            */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#ifndef _H_insmngr

#pragma once

#define _H_insmngr

#include "object.h"
#include "inscom.h"

   void                           CL_InitializeInstanceCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_MakeInstanceCommand(Environment *,UDFContext *,UDFValue *);
   CLIPSLexeme                   *CL_GetFullCL_InstanceName(Environment *,Instance *);
   Instance                      *CL_BuildInstance(Environment *,CLIPSLexeme *,Defclass *,bool);
   void                           CL_InitSlotsCommand(Environment *,UDFContext *,UDFValue *);
   CL_UnmakeInstanceError            CL_QuashInstance(Environment *,Instance *);

#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM
   void                           CL_InactiveInitializeInstance(Environment *,UDFContext *,UDFValue *);
   void                           CL_InactiveCL_MakeInstance(Environment *,UDFContext *,UDFValue *);
#endif

   InstanceCL_Builder               *CreateInstanceCL_Builder(Environment *,const char *);
   PutSlotError                   CL_IBPutSlot(InstanceCL_Builder *,const char *,CLIPSValue *);

   Instance                      *CL_IBMake(InstanceCL_Builder *,const char *);

   void                           CL_IBDispose(InstanceCL_Builder *);
   void                           CL_IBAbort(InstanceCL_Builder *);
   InstanceCL_BuilderError           CL_IBSetDefclass(InstanceCL_Builder *,const char *);

   PutSlotError                   CL_IBPutSlotCLIPSInteger(InstanceCL_Builder *,const char *,CLIPSInteger *);
   PutSlotError                   CL_IBPutSlotInteger(InstanceCL_Builder *,const char *,long long);

   PutSlotError                   CL_IBPutSlotCLIPSFloat(InstanceCL_Builder *,const char *,CLIPSFloat *);
   PutSlotError                   CL_IBPutSlotFloat(InstanceCL_Builder *,const char *,double);

   PutSlotError                   CL_IBPutSlotCLIPSLexeme(InstanceCL_Builder *,const char *,CLIPSLexeme *);
   PutSlotError                   CL_IBPutSlotSymbol(InstanceCL_Builder *,const char *,const char *);
   PutSlotError                   CL_IBPutSlotString(InstanceCL_Builder *,const char *,const char *);
   PutSlotError                   CL_IBPutSlotCL_InstanceName(InstanceCL_Builder *,const char *,const char *);

   PutSlotError                   CL_IBPutSlotFact(InstanceCL_Builder *,const char *,Fact *);
   PutSlotError                   CL_IBPutSlotInstance(InstanceCL_Builder *,const char *,Instance *);
   PutSlotError                   CL_IBPutSlotExternalAddress(InstanceCL_Builder *,const char *,CLIPSExternalAddress *);
   PutSlotError                   CL_IBPutSlotMultifield(InstanceCL_Builder *,const char *,Multifield *);
   InstanceCL_BuilderError           CL_IBError(Environment *);

   InstanceModifier              *CL_CreateInstanceModifier(Environment *,Instance *);
   PutSlotError                   CL_IMPutSlot(InstanceModifier *,const char *,CLIPSValue *);
   void                           CL_IMDispose(InstanceModifier *);
   void                           CL_IMAbort(InstanceModifier *);
   InstanceModifierError          CL_IMSetInstance(InstanceModifier *,Instance *);
   Instance                      *CL_IMModify(InstanceModifier *);

   PutSlotError                   CL_IMPutSlotCLIPSInteger(InstanceModifier *,const char *,CLIPSInteger *);
   PutSlotError                   CL_IMPutSlotInteger(InstanceModifier *,const char *,long long);

   PutSlotError                   CL_IMPutSlotCLIPSFloat(InstanceModifier *,const char *,CLIPSFloat *);
   PutSlotError                   CL_IMPutSlotFloat(InstanceModifier *,const char *,double);

   PutSlotError                   CL_IMPutSlotCLIPSLexeme(InstanceModifier *,const char *,CLIPSLexeme *);
   PutSlotError                   CL_IMPutSlotSymbol(InstanceModifier *,const char *,const char *);
   PutSlotError                   CL_IMPutSlotString(InstanceModifier *,const char *,const char *);
   PutSlotError                   CL_IMPutSlotCL_InstanceName(InstanceModifier *,const char *,const char *);

   PutSlotError                   CL_IMPutSlotFact(InstanceModifier *,const char *,Fact *);
   PutSlotError                   CL_IMPutSlotInstance(InstanceModifier *,const char *,Instance *);
   PutSlotError                   CL_IMPutSlotExternalAddress(InstanceModifier *,const char *,CLIPSExternalAddress *);
   PutSlotError                   CL_IMPutSlotMultifield(InstanceModifier *,const char *,Multifield *);
   InstanceModifierError          CL_IMError(Environment *);

#endif /* _H_insmngr */







