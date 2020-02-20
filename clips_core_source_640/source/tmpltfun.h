   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/01/16            */
   /*                                                     */
   /*          DEFTEMPLATE FUNCTION HEADER FILE           */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Added deftemplate-slot-names,                  */
/*            deftemplate-slot-default-value,                */
/*            deftemplate-slot-cardinality,                  */
/*            deftemplate-slot-allowed-values,               */
/*            deftemplate-slot-range,                        */
/*            deftemplate-slot-types,                        */
/*            deftemplate-slot-multip,                       */
/*            deftemplate-slot-singlep,                      */
/*            deftemplate-slot-existp, and                   */
/*            deftemplate-slot-defaultp functions.           */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for deftemplate slot facets.           */
/*                                                           */
/*            Added deftemplate-slot-facet-existp and        */
/*            deftemplate-slot-facet-value functions.        */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Used CL_gensprintf instead of sprintf.            */
/*                                                           */
/*            Support for modify callback function.          */
/*                                                           */
/*            Added additional argument to function          */
/*            CheckDeftemplateAndSlotArguments to specify    */
/*            the expected number of arguments.              */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Added code to prevent a clear command from     */
/*            being executed during fact assertions via      */
/*            Increment/DecrementCL_ClearReadyLocks API.        */
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
/*            Modify command preserves fact id and address.  */
/*                                                           */
/*************************************************************/

#ifndef _H_tmpltfun

#pragma once

#define _H_tmpltfun

#include "expressn.h"
#include "factmngr.h"
#include "symbol.h"
#include "tmpltdef.h"

   bool                           CL_UpdateModifyDuplicate(Environment *,struct expr *,const char *,void *);
   struct expr                   *CL_ModifyParse(Environment *,struct expr *,const char *);
   struct expr                   *CL_DuplicateParse(Environment *,struct expr *,const char *);
   void                           CL_DeftemplateFunctions(Environment *);
   void                           CL_ModifyCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_DuplicateCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_DeftemplateSlotNamesFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_DeftemplateSlotNames(Deftemplate *,CLIPSValue *);
   void                           CL_DeftemplateCL_SlotDefaultValueFunction(Environment *,UDFContext *,UDFValue *);
   bool                           CL_DeftemplateCL_SlotDefaultValue(Deftemplate *,const char *,CLIPSValue *);
   void                           CL_DeftemplateCL_SlotCardinalityFunction(Environment *,UDFContext *,UDFValue *);
   bool                           CL_DeftemplateCL_SlotCardinality(Deftemplate *,const char *,CLIPSValue *);
   void                           CL_DeftemplateCL_SlotAllowedValuesFunction(Environment *,UDFContext *,UDFValue *);
   bool                           CL_DeftemplateCL_SlotAllowedValues(Deftemplate *,const char *,CLIPSValue *);
   void                           CL_DeftemplateCL_SlotRangeFunction(Environment *,UDFContext *,UDFValue *);
   bool                           CL_DeftemplateCL_SlotRange(Deftemplate *,const char *,CLIPSValue *);
   void                           CL_DeftemplateCL_SlotTypesFunction(Environment *,UDFContext *,UDFValue *);
   bool                           CL_DeftemplateCL_SlotTypes(Deftemplate *,const char *,CLIPSValue *);
   void                           CL_DeftemplateSlotMultiPFunction(Environment *,UDFContext *,UDFValue *);
   bool                           CL_DeftemplateSlotMultiP(Deftemplate *,const char *);
   void                           CL_DeftemplateSlotSinglePFunction(Environment *,UDFContext *,UDFValue *);
   bool                           CL_DeftemplateSlotSingleP(Deftemplate *,const char *);
   void                           CL_DeftemplateCL_SlotExistPFunction(Environment *,UDFContext *,UDFValue *);
   bool                           CL_DeftemplateCL_SlotExistP(Deftemplate *,const char *);
   void                           CL_DeftemplateCL_SlotDefaultPFunction(Environment *,UDFContext *,UDFValue *);
   DefaultType                    CL_DeftemplateCL_SlotDefaultP(Deftemplate *,const char *);
   void                           CL_DeftemplateSlotFacetExistPFunction(Environment *,UDFContext *,UDFValue *);
   bool                           CL_DeftemplateSlotFacetExistP(Environment *,Deftemplate *,const char *,const char *);
   void                           CL_DeftemplateSlotFacetValueFunction(Environment *,UDFContext *,UDFValue *);
   bool                           CL_DeftemplateSlotFacetValue(Environment *,Deftemplate *,const char *,const char *,UDFValue *);
   Fact                          *CL_ReplaceFact(Environment *,Fact *,CLIPSValue *,char *);

#endif /* _H_tmpltfun */




