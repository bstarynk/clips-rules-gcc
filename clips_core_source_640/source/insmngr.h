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

void CL_InitializeInstanceCommand (Environment *, UDFContext *, UDFValue *);
void CL_MakeInstanceCommand (Environment *, UDFContext *, UDFValue *);
CLIPSLexeme *CL_GetFull_InstanceName (Environment *, Instance *);
Instance *CL_BuildInstance (Environment *, CLIPSLexeme *, Defclass *, bool);
void CL_InitSlotsCommand (Environment *, UDFContext *, UDFValue *);
CL_UnmakeInstanceError CL_QuashInstance (Environment *, Instance *);

#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM
void CL_InactiveInitializeInstance (Environment *, UDFContext *, UDFValue *);
void CL_Inactive_MakeInstance (Environment *, UDFContext *, UDFValue *);
#endif

Instance_Builder *CreateInstance_Builder (Environment *, const char *);
PutSlotError CL_IBPutSlot (Instance_Builder *, const char *, CLIPSValue *);

Instance *CL_IBMake (Instance_Builder *, const char *);

void CL_IBDispose (Instance_Builder *);
void CL_IBAbort (Instance_Builder *);
Instance_BuilderError CL_IBSetDefclass (Instance_Builder *, const char *);

PutSlotError CL_IBPutSlotCLIPSInteger (Instance_Builder *, const char *,
				       CLIPSInteger *);
PutSlotError CL_IBPutSlotInteger (Instance_Builder *, const char *,
				  long long);

PutSlotError CL_IBPutSlotCLIPSFloat (Instance_Builder *, const char *,
				     CLIPSFloat *);
PutSlotError CL_IBPutSlotFloat (Instance_Builder *, const char *, double);

PutSlotError CL_IBPutSlotCLIPSLexeme (Instance_Builder *, const char *,
				      CLIPSLexeme *);
PutSlotError CL_IBPutSlotSymbol (Instance_Builder *, const char *,
				 const char *);
PutSlotError CL_IBPutSlotString (Instance_Builder *, const char *,
				 const char *);
PutSlotError CL_IBPutSlot_InstanceName (Instance_Builder *, const char *,
					const char *);

PutSlotError CL_IBPutSlotFact (Instance_Builder *, const char *, Fact *);
PutSlotError CL_IBPutSlotInstance (Instance_Builder *, const char *,
				   Instance *);
PutSlotError CL_IBPutSlotExternalAddress (Instance_Builder *, const char *,
					  CLIPSExternalAddress *);
PutSlotError CL_IBPutSlotMultifield (Instance_Builder *, const char *,
				     Multifield *);
Instance_BuilderError CL_IBError (Environment *);

InstanceModifier *CL_CreateInstanceModifier (Environment *, Instance *);
PutSlotError CL_IMPutSlot (InstanceModifier *, const char *, CLIPSValue *);
void CL_IMDispose (InstanceModifier *);
void CL_IMAbort (InstanceModifier *);
InstanceModifierError CL_IMSetInstance (InstanceModifier *, Instance *);
Instance *CL_IMModify (InstanceModifier *);

PutSlotError CL_IMPutSlotCLIPSInteger (InstanceModifier *, const char *,
				       CLIPSInteger *);
PutSlotError CL_IMPutSlotInteger (InstanceModifier *, const char *,
				  long long);

PutSlotError CL_IMPutSlotCLIPSFloat (InstanceModifier *, const char *,
				     CLIPSFloat *);
PutSlotError CL_IMPutSlotFloat (InstanceModifier *, const char *, double);

PutSlotError CL_IMPutSlotCLIPSLexeme (InstanceModifier *, const char *,
				      CLIPSLexeme *);
PutSlotError CL_IMPutSlotSymbol (InstanceModifier *, const char *,
				 const char *);
PutSlotError CL_IMPutSlotString (InstanceModifier *, const char *,
				 const char *);
PutSlotError CL_IMPutSlot_InstanceName (InstanceModifier *, const char *,
					const char *);

PutSlotError CL_IMPutSlotFact (InstanceModifier *, const char *, Fact *);
PutSlotError CL_IMPutSlotInstance (InstanceModifier *, const char *,
				   Instance *);
PutSlotError CL_IMPutSlotExternalAddress (InstanceModifier *, const char *,
					  CLIPSExternalAddress *);
PutSlotError CL_IMPutSlotMultifield (InstanceModifier *, const char *,
				     Multifield *);
InstanceModifierError CL_IMError (Environment *);

#endif /* _H_insmngr */
