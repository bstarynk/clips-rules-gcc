   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/02/18            */
   /*                                                     */
   /*              FACTS MANAGER HEADER FILE              */
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
/*      6.23: Added support for templates CL_maintaining their  */
/*            own list of facts.                             */
/*                                                           */
/*      6.24: Removed LOGICAL_DEPENDENCIES compilation flag. */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            CL_AssignFactSlotDefaults function does not       */
/*            properly handle defaults for multifield slots. */
/*            DR0869                                         */
/*                                                           */
/*            Support for ppfact command.                    */
/*                                                           */
/*      6.30: Callback function support for assertion,       */
/*            retraction, and modification of facts.         */
/*                                                           */
/*            Updates to fact pattern entity record.         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Removed unused global variables.               */
/*                                                           */
/*            Added code to prevent a clear command from     */
/*            being executed during fact assertions via      */
/*            JoinOperationInProgress mechanism.             */
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
/*            CL_Watch facts for modify command only prints     */
/*            changed slots.                                 */
/*                                                           */
/*            Modify command preserves fact id and address.  */
/*                                                           */
/*            Pretty print functions accept optional logical */
/*            name argument.                                 */
/*                                                           */
/*************************************************************/

#ifndef _H_factmngr

#pragma once

#define _H_factmngr

typedef struct factCL_Builder FactCL_Builder;
typedef struct factModifier FactModifier;

#include "entities.h"
#include "conscomp.h"
#include "tmpltdef.h"

typedef void ModifyCL_CallFunction(Environment *,Fact *,Fact *,void *);
typedef struct modifyCL_CallFunctionItem ModifyCL_CallFunctionItem;

typedef enum
  {
   RE_NO_ERROR = 0,
   RE_NULL_POINTER_ERROR,
   RE_COULD_NOT_RETRACT_ERROR,
   RE_RULE_NETWORK_ERROR
  } CL_RetractError;

typedef enum
  {
   AE_NO_ERROR = 0,
   AE_NULL_POINTER_ERROR,
   AE_RETRACTED_ERROR,
   AE_COULD_NOT_ASSERT_ERROR,
   AE_RULE_NETWORK_ERROR
  } CL_AssertError;

typedef enum
  {
   ASE_NO_ERROR = 0,
   ASE_NULL_POINTER_ERROR,
   ASE_PARSING_ERROR,
   ASE_COULD_NOT_ASSERT_ERROR,
   ASE_RULE_NETWORK_ERROR
  } CL_AssertStringError;

typedef enum
  {
   FBE_NO_ERROR = 0,
   FBE_NULL_POINTER_ERROR,
   FBE_DEFTEMPLATE_NOT_FOUND_ERROR,
   FBE_IMPLIED_DEFTEMPLATE_ERROR,
   FBE_COULD_NOT_ASSERT_ERROR,
   FBE_RULE_NETWORK_ERROR
  } FactCL_BuilderError;

typedef enum
  {
   FME_NO_ERROR = 0,
   FME_NULL_POINTER_ERROR,
   FME_RETRACTED_ERROR,
   FME_IMPLIED_DEFTEMPLATE_ERROR,
   FME_COULD_NOT_MODIFY_ERROR,
   FME_RULE_NETWORK_ERROR
  } FactModifierError;

struct modifyCL_CallFunctionItem
  {
   const char *name;
   ModifyCL_CallFunction *func;
   int priority;
   ModifyCL_CallFunctionItem *next;
   void *context;
  };

struct fact
  {
   union
     {
      struct patternEntity patternHeader;
      TypeHeader header;
     };
   Deftemplate *whichDeftemplate;
   void *list;
   long long factIndex;
   unsigned long hashValue;
   unsigned int garbage : 1;
   Fact *previousFact;
   Fact *nextFact;
   Fact *previousTemplateFact;
   Fact *nextTemplateFact;
   Multifield *basisSlots;
   Multifield theProposition;
  };

struct factCL_Builder
  {
   Environment *fbEnv;
   Deftemplate *fbDeftemplate;
   CLIPSValue *fbValueArray;
  };

struct factModifier
  {
   Environment *fmEnv;
   Fact *fmOldFact;
   CLIPSValue *fmValueArray;
   char *changeMap;
  };

#include "facthsh.h"

#define FACTS_DATA 3

struct factsData
  {
   bool ChangeToFactList;
#if DEBUGGING_FUNCTIONS
   bool CL_WatchCL_Facts;
#endif
   Fact DummyFact;
   Fact *GarbageCL_Facts;
   Fact *LastFact;
   Fact *FactList;
   long long NextCL_FactIndex;
   unsigned long NumberOfCL_Facts;
   struct callFunctionItemWithArg *ListOfCL_AssertFunctions;
   struct callFunctionItemWithArg *ListOfCL_RetractFunctions;
   ModifyCL_CallFunctionItem *ListOfModifyFunctions;
   struct patternEntityRecord  FactInfo;
#if (! RUN_TIME) && (! BLOAD_ONLY)
   Deftemplate *CurrentDeftemplate;
#endif
#if DEFRULE_CONSTRUCT && (! RUN_TIME) && DEFTEMPLATE_CONSTRUCT && CONSTRUCT_COMPILER
   struct CodeGeneratorItem *FactCodeItem;
#endif
   struct factHashEntry **FactHashTable;
   unsigned long FactHashTableSize;
   bool FactDuplication;
#if DEFRULE_CONSTRUCT
   Fact                    *CurrentPatternFact;
   struct multifieldMarker *CurrentPatternMarks;
#endif
   long LastModuleIndex;
   CL_RetractError retractError;
   CL_AssertError assertError;
   CL_AssertStringError assertStringError;
   FactModifierError factModifierError;
   FactCL_BuilderError factCL_BuilderError;
  };

#define FactData(theEnv) ((struct factsData *) GetEnvironmentData(theEnv,FACTS_DATA))

   Fact                          *CL_Assert(Fact *);
   CL_AssertStringError              GetCL_AssertStringError(Environment *);
   Fact                          *CL_AssertDriver(Fact *,long long,Fact *,Fact *,char *);
   Fact                          *CL_AssertString(Environment *,const char *);
   Fact                          *CL_CreateFact(Deftemplate *);
   void                           CL_ReleaseFact(Fact *);
   void                           CL_DecrementFactCallback(Environment *,Fact *);
   long long                      CL_FactIndex(Fact *);
   GetSlotError                   CL_GetFactSlot(Fact *,const char *,CLIPSValue *);
   void                           CL_PrintFactWithIdentifier(Environment *,const char *,Fact *,const char *);
   void                           CL_PrintFact(Environment *,const char *,Fact *,bool,bool,const char *);
   void                           CL_PrintFactIdentifierInLongFoCL_rm(Environment *,const char *,Fact *);
   CL_RetractError                   CL_Retract(Fact *);
   CL_RetractError                   CL_RetractDriver(Environment *,Fact *,bool,char *);
   CL_RetractError                   CL_RetractAllCL_Facts(Environment *);
   Fact                          *CL_CreateFactBySize(Environment *,size_t);
   void                           CL_FactInstall(Environment *,Fact *);
   void                           CL_FactDeinstall(Environment *,Fact *);
   Fact                          *CL_GetNextFact(Environment *,Fact *);
   Fact                          *CL_GetNextFactInScope(Environment *,Fact *);
   void                           CL_FactPPFoCL_rm(Fact *,StringCL_Builder *,bool);
   bool                           CL_GetFactListChanged(Environment *);
   void                           CL_SetFactListChanged(Environment *,bool);
   unsigned long                  GetNumberOfCL_Facts(Environment *);
   void                           InitializeCL_Facts(Environment *);
   Fact                          *CL_FindIndexedFact(Environment *,long long);
   void                           CL_RetainFact(Fact *);
   void                           CL_IncrementFactCallback(Environment *,Fact *);
   void                           CL_PrintFactIdentifier(Environment *,const char *,Fact *);
   void                           CL_DecrementFactBasisCount(Environment *,Fact *);
   void                           CL_IncrementFactBasisCount(Environment *,Fact *);
   bool                           CL_FactIsDeleted(Environment *,Fact *);
   void                           CL_ReturnFact(Environment *,Fact *);
   void                           CL_MatchFactFunction(Environment *,Fact *);
   bool                           CL_PutFactSlot(Fact *,const char *,CLIPSValue *);
   bool                           CL_AssignFactSlotDefaults(Fact *);
   bool                           CL_CopyCL_FactSlotValues(Environment *,Fact *,Fact *);
   bool                           CL_DeftemplateSlotDefault(Environment *,Deftemplate *,
                                                         struct templateSlot *,UDFValue *,bool);
   bool                           CL_AddCL_AssertFunction(Environment *,const char *,
                                                    VoidCL_CallFunctionWithArg *,int,void *);
   bool                           RemoveCL_AssertFunction(Environment *,const char *);
   bool                           CL_AddCL_RetractFunction(Environment *,const char *,
                                                     VoidCL_CallFunctionWithArg *,int,void *);
   bool                           CL_RemoveCL_RetractFunction(Environment *,const char *);
   FactCL_Builder                   *CL_CreateFactCL_Builder(Environment *,const char *);
   PutSlotError                   CL_FBPutSlot(FactCL_Builder *,const char *,CLIPSValue *);
   Fact                          *FBCL_Assert(FactCL_Builder *);
   void                           CL_FBDispose(FactCL_Builder *);
   void                           CL_FBAbort(FactCL_Builder *);
   FactCL_BuilderError               CL_FBSetDeftemplate(FactCL_Builder *,const char *);
   PutSlotError                   CL_FBPutSlotCLIPSInteger(FactCL_Builder *,const char *,CLIPSInteger *);
   PutSlotError                   CL_FBPutSlotInteger(FactCL_Builder *,const char *,long long);
   PutSlotError                   CL_FBPutSlotCLIPSFloat(FactCL_Builder *,const char *,CLIPSFloat *);
   PutSlotError                   CL_FBPutSlotFloat(FactCL_Builder *,const char *,double);
   PutSlotError                   CL_FBPutSlotCLIPSLexeme(FactCL_Builder *,const char *,CLIPSLexeme *);
   PutSlotError                   CL_FBPutSlotSymbol(FactCL_Builder *,const char *,const char *);
   PutSlotError                   CL_FBPutSlotString(FactCL_Builder *,const char *,const char *);
   PutSlotError                   CL_FBPutSlotCL_InstanceName(FactCL_Builder *,const char *,const char *);
   PutSlotError                   CL_FBPutSlotFact(FactCL_Builder *,const char *,Fact *);
   PutSlotError                   CL_FBPutSlotInstance(FactCL_Builder *,const char *,Instance *);
   PutSlotError                   CL_FBPutSlotCLIPSExternalAddress(FactCL_Builder *,const char *,CLIPSExternalAddress *);
   PutSlotError                   CL_FBPutSlotMultifield(FactCL_Builder *,const char *,Multifield *);
   FactCL_BuilderError               CL_FBError(Environment *);
   FactModifier                  *CL_CreateFactModifier(Environment *,Fact *);
   PutSlotError                   CL_FMPutSlot(FactModifier *,const char *,CLIPSValue *);
   Fact                          *CL_FMModify(FactModifier *);
   void                           CL_FMDispose(FactModifier *);
   void                           CL_FMAbort(FactModifier *);
   FactModifierError              CL_FMSetFact(FactModifier *,Fact *);
   PutSlotError                   CL_FMPutSlotCLIPSInteger(FactModifier *,const char *,CLIPSInteger *);
   PutSlotError                   CL_FMPutSlotInteger(FactModifier *,const char *,long long);
   PutSlotError                   CL_FMPutSlotCLIPSFloat(FactModifier *,const char *,CLIPSFloat *);
   PutSlotError                   CL_FMPutSlotFloat(FactModifier *,const char *,double);
   PutSlotError                   CL_FMPutSlotCLIPSLexeme(FactModifier *,const char *,CLIPSLexeme *);
   PutSlotError                   CL_FMPutSlotSymbol(FactModifier *,const char *,const char *);
   PutSlotError                   CL_FMPutSlotString(FactModifier *,const char *,const char *);
   PutSlotError                   CL_FMPutSlotCL_InstanceName(FactModifier *,const char *,const char *);
   PutSlotError                   CL_FMPutSlotFact(FactModifier *,const char *,Fact *);
   PutSlotError                   CL_FMPutSlotInstance(FactModifier *,const char *,Instance *);
   PutSlotError                   CL_FMPutSlotExternalAddress(FactModifier *,const char *,CLIPSExternalAddress *);
   PutSlotError                   CL_FMPutSlotMultifield(FactModifier *,const char *,Multifield *);
   FactModifierError              CL_FMError(Environment *);

   bool                           CL_AddModifyFunction(Environment *,const char *,ModifyCL_CallFunction *,int,void *);
   bool                           CL_RemoveModifyFunction(Environment *,const char *);
   ModifyCL_CallFunctionItem        *CL_AddModifyFunctionToCallList(Environment *,const char *,int,
                                                              ModifyCL_CallFunction *,ModifyCL_CallFunctionItem *,void *);
   ModifyCL_CallFunctionItem        *CL_RemoveModifyFunctionFromCallList(Environment *,const char *,
                                                                   ModifyCL_CallFunctionItem *,bool *);
   void                           CL_DeallocateModifyCallList(Environment *,ModifyCL_CallFunctionItem *);

#endif /* _H_factmngr */





