   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  05/16/18             */
   /*                                                     */
   /*                 CLASS PARSER MODULE                 */
   /*******************************************************/

/**************************************************************/
/* Purpose: Parsing Routines for Defclass Construct           */
/*                                                            */
/* Principal Programmer(s):                                   */
/*      Brian L. Dantes                                       */
/*                                                            */
/* Contributing Programmer(s):                                */
/*                                                            */
/* Revision History:                                          */
/*                                                            */
/*      6.24: Converted INSTANCE_PATTERN_MATCHING to          */
/*            DEFRULE_CONSTRUCT.                              */
/*                                                            */
/*            Renamed BOOLEAN macro type to intBool.          */
/*                                                            */
/*      6.30: Changed integer type/precision.                 */
/*                                                            */
/*            Support for long long integers.                 */
/*                                                            */
/*            Added const qualifiers to remove C++            */
/*            deprecation warnings.                           */
/*                                                            */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Static constraint checking is always enabled.   */
/*                                                            */
/*            UDF redesign.                                  */
/*                                                           */
/**************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if OBJECT_SYSTEM && (! BLOAD_ONLY) && (! RUN_TIME)

#include <string.h>

#include "classcom.h"
#include "classfun.h"
#include "cstrnchk.h"
#include "cstrnpsr.h"
#include "cstrnutl.h"
#include "default.h"
#include "envrnmnt.h"
#include "insfun.h"
#include "memalloc.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"

#include "clsltpsr.h"

/* =========================================
   *****************************************
                   CONSTANTS
   =========================================
   ***************************************** */
#define DEFAULT_FACET         "default"
#define DYNAMIC_FACET         "default-dynamic"
#define VARIABLE_VAR          "VARIABLE"

#define STORAGE_FACET         "storage"
#define SLOT_SHARE_RLN        "shared"
#define SLOT_LOCAL_RLN        "local"

#define ACCESS_FACET          "access"
#define SLOT_RDONLY_RLN       "read-only"
#define SLOT_RDWRT_RLN        "read-write"
#define SLOT_INIT_RLN         "initialize-only"

#define PROPAGATION_FACET     "propagation"
#define SLOT_NO_INH_RLN       "no-inherit"
#define SLOT_INH_RLN          "inherit"

#define SOURCE_FACET          "source"
#define SLOT_COMPOSITE_RLN    "composite"
#define SLOT_EXCLUSIVE_RLN    "exclusive"

#define MATCH_FACET           MATCH_RLN
#define SLOT_REACTIVE_RLN     REACTIVE_RLN
#define SLOT_NONREACTIVE_RLN  NONREACTIVE_RLN

#define VISIBILITY_FACET      "visibility"
#define SLOT_PUBLIC_RLN       "public"
#define SLOT_PRIVATE_RLN      "private"

#define CREATE_ACCESSOR_FACET "create-accessor"
#define SLOT_READ_RLN         "read"
#define SLOT_WRITE_RLN        "write"
#define SLOT_NONE_RLN         "NONE"

#define OVERRIDE_MSG_FACET    "override-message"
#define SLOT_DEFAULT_RLN      "DEFAULT"

#define STORAGE_BIT           0
#define FIELD_BIT             1
#define ACCESS_BIT            2
#define PROPAGATION_BIT       3
#define SOURCE_BIT            4
#define MATCH_BIT             5
#define DEFAULT_BIT           6
#define DEFAULT_DYNAMIC_BIT   7
#define VISIBILITY_BIT        8
#define CREATE_ACCESSOR_BIT   9
#define OVERRIDE_MSG_BIT      10

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static SlotDescriptor          *NewSlot(Environment *,CLIPSLexeme *);
   static TEMP_SLOT_LINK          *InsertSlot(Environment *,const char *,TEMP_SLOT_LINK *,SlotDescriptor *);
   static int                      ParseSimpleFacet(Environment *,const char *,SlotDescriptor *,char*,const char *,int,const char *,
                                                    const char *,const char *,const char *,CLIPSLexeme **);
   static bool                    CL_ParseDefaultFacet(Environment *,const char *,char *,SlotDescriptor *);
   static void                    CL_BuildCompositeFacets(Environment *,SlotDescriptor *,PACKED_CLASS_LINKS *,const char *,
                                                       CONSTRAINT_PARSE_RECORD *);
   static bool                    CheckForFacetConflicts(Environment *,SlotDescriptor *,CONSTRAINT_PARSE_RECORD *);
   static bool                    CL_EvaluateCL_SlotDefaultValue(Environment *,SlotDescriptor *,const char *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/************************************************************
  NAME         : CL_ParseSlot
  DESCRIPTION  : Parses slot definitions for a
                   defclass statement
  INPUTS       : 1) The logical name of the input source
                 2) The current slot list
                 3) The class precedence list for the class
                    to which this slot is being attached
                    (used to find facets for composite slots)
                 4) A flag indicating if this is a multifield
                    slot or not
                 5) A flag indicating if the type of slot
                    (single or multi) was explicitly
                    specified or not
  RETURNS      : The address of the list of slots,
                   NULL if there was an error
  SIDE EFFECTS : The slot list is allocated
  NOTES        : Assumes "(slot" has already been parsed.
 ************************************************************/
TEMP_SLOT_LINK *CL_ParseSlot(
  Environment *theEnv,
  const char *readSource,
  const char *className,
  TEMP_SLOT_LINK *slist,
  PACKED_CLASS_LINKS *preclist,
  bool multiSlot)
  {
   SlotDescriptor *slot;
   CONSTRAINT_PARSE_RECORD parsedConstraint;
   char specbits[2];
   int rtnCode;
   CLIPSLexeme *newOverrideMsg;

   /* ===============================================================
      Bits in specbits are when slot qualifiers are specified so that
      duplicate or conflicting qualifiers can be detected.

      Shared/local                          bit-0
      Single/multiple                       bit-1
      Read-only/Read-write/Initialize-Only  bit-2
      Inherit/No-inherit                    bit-3
      Composite/Exclusive                   bit-4
      Reactive/Nonreactive                  bit-5
      Default                               bit-6
      Default-dynamic                       bit-7
      Visibility                            bit-8
      Override-message                      bit-9
      =============================================================== */
   CL_SavePPBuffer(theEnv," ");
   specbits[0] = specbits[1] = '\0';
   CL_GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
   if (DefclassData(theEnv)->ObjectParseToken.tknType != SYMBOL_TOKEN)
     {
      CL_DeleteSlots(theEnv,slist);
      CL_SyntaxErrorMessage(theEnv,"defclass slot");
      return NULL;
     }
   if ((DefclassData(theEnv)->ObjectParseToken.value == (void *) DefclassData(theEnv)->ISA_SYMBOL) ||
       (DefclassData(theEnv)->ObjectParseToken.value == (void *) DefclassData(theEnv)->NAME_SYMBOL))
     {
      CL_DeleteSlots(theEnv,slist);
      CL_SyntaxErrorMessage(theEnv,"defclass slot");
      return NULL;
     }
   slot = NewSlot(theEnv,DefclassData(theEnv)->ObjectParseToken.lexemeValue);
   slist = InsertSlot(theEnv,className,slist,slot);
   if (slist == NULL)
     return NULL;
   if (multiSlot)
     {
      slot->multiple = true;
      SetBitMap(specbits,FIELD_BIT);
     }
      
   CL_GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
   CL_IncrementIndentDepth(theEnv,3);
   CL_InitializeConstraintParseRecord(&parsedConstraint);
   while (DefclassData(theEnv)->ObjectParseToken.tknType == LEFT_PARENTHESIS_TOKEN)
     {
      CL_PPBackup(theEnv);
      CL_PPCRAndIndent(theEnv);
      CL_SavePPBuffer(theEnv,"(");
      CL_GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
      if (DefclassData(theEnv)->ObjectParseToken.tknType != SYMBOL_TOKEN)
        {
         CL_SyntaxErrorMessage(theEnv,"defclass slot");
         goto CL_ParseSlotError;
        }
      else if (strcmp(DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents,DEFAULT_FACET) == 0)
        {
         if (CL_ParseDefaultFacet(theEnv,readSource,specbits,slot) == false)
           goto CL_ParseSlotError;
        }
      else if (strcmp(DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents,DYNAMIC_FACET) == 0)
        {
         SetBitMap(specbits,DEFAULT_DYNAMIC_BIT);
         if (CL_ParseDefaultFacet(theEnv,readSource,specbits,slot) == false)
           goto CL_ParseSlotError;
        }
      else if (strcmp(DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents,ACCESS_FACET) == 0)
        {
         rtnCode = ParseSimpleFacet(theEnv,readSource,slot,specbits,ACCESS_FACET,ACCESS_BIT,
                                    SLOT_RDWRT_RLN,SLOT_RDONLY_RLN,SLOT_INIT_RLN,
                                    NULL,NULL);
         if (rtnCode == -1)
           goto CL_ParseSlotError;
         else if (rtnCode == 1)
           slot->noCL_Write = 1;
         else if (rtnCode == 2)
           slot->initializeOnly = 1;
        }
      else if (strcmp(DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents,STORAGE_FACET) == 0)
        {
         rtnCode = ParseSimpleFacet(theEnv,readSource,slot,specbits,STORAGE_FACET,STORAGE_BIT,
                                    SLOT_LOCAL_RLN,SLOT_SHARE_RLN,NULL,NULL,NULL);
         if (rtnCode == -1)
           goto CL_ParseSlotError;
         slot->shared = (rtnCode == 0) ? false : true;
        }
      else if (strcmp(DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents,PROPAGATION_FACET) == 0)
        {
         rtnCode = ParseSimpleFacet(theEnv,readSource,slot,specbits,PROPAGATION_FACET,PROPAGATION_BIT,
                                    SLOT_INH_RLN,SLOT_NO_INH_RLN,NULL,NULL,NULL);
         if (rtnCode == -1)
           goto CL_ParseSlotError;
         slot->noInherit = (rtnCode == 0) ? false : true;
        }
      else if (strcmp(DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents,SOURCE_FACET) == 0)
        {
         rtnCode = ParseSimpleFacet(theEnv,readSource,slot,specbits,SOURCE_FACET,SOURCE_BIT,
                                    SLOT_EXCLUSIVE_RLN,SLOT_COMPOSITE_RLN,NULL,NULL,NULL);
         if (rtnCode == -1)
           goto CL_ParseSlotError;
         slot->composite = (rtnCode == 0) ? false : true;
        }
#if DEFRULE_CONSTRUCT
      else if (strcmp(DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents,MATCH_FACET) == 0)
        {
         rtnCode = ParseSimpleFacet(theEnv,readSource,slot,specbits,MATCH_FACET,MATCH_BIT,
                                    SLOT_NONREACTIVE_RLN,SLOT_REACTIVE_RLN,NULL,NULL,NULL);
         if (rtnCode == -1)
           goto CL_ParseSlotError;
         slot->reactive = (rtnCode == 0) ? false : true;
        }
#endif
      else if (strcmp(DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents,VISIBILITY_FACET) == 0)
        {
         rtnCode = ParseSimpleFacet(theEnv,readSource,slot,specbits,VISIBILITY_FACET,VISIBILITY_BIT,
                                    SLOT_PRIVATE_RLN,SLOT_PUBLIC_RLN,NULL,NULL,NULL);
         if (rtnCode == -1)
           goto CL_ParseSlotError;
         slot->publicVisibility = (rtnCode == 0) ? false : true;
        }
      else if (strcmp(DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents,CREATE_ACCESSOR_FACET) == 0)
        {
         rtnCode = ParseSimpleFacet(theEnv,readSource,slot,specbits,CREATE_ACCESSOR_FACET,
                                    CREATE_ACCESSOR_BIT,
                                    SLOT_READ_RLN,SLOT_WRITE_RLN,SLOT_RDWRT_RLN,
                                    SLOT_NONE_RLN,NULL);
         if (rtnCode == -1)
           goto CL_ParseSlotError;
         if ((rtnCode == 0) || (rtnCode == 2))
           slot->createReadAccessor = true;
         if ((rtnCode == 1) || (rtnCode == 2))
           slot->createCL_WriteAccessor = true;
        }
      else if (strcmp(DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents,OVERRIDE_MSG_FACET) == 0)
        {
         rtnCode = ParseSimpleFacet(theEnv,readSource,slot,specbits,OVERRIDE_MSG_FACET,OVERRIDE_MSG_BIT,
                                    NULL,NULL,NULL,SLOT_DEFAULT_RLN,&newOverrideMsg);
         if (rtnCode == -1)
           goto CL_ParseSlotError;
         if (rtnCode == 4)
           {
            CL_ReleaseLexeme(theEnv,slot->overrideMessage);
            slot->overrideMessage = newOverrideMsg;
            IncrementLexemeCount(slot->overrideMessage);
           }
         slot->overrideMessageSpecified = true;
        }
      else if (CL_StandardConstraint(DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents))
        {
         if (CL_ParseCL_StandardConstraint(theEnv,readSource,DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents,
                slot->constraint,&parsedConstraint,true) == false)
           goto CL_ParseSlotError;
        }
      else
        {
         CL_SyntaxErrorMessage(theEnv,"defclass slot");
         goto CL_ParseSlotError;
        }
      CL_GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
     }
   if (DefclassData(theEnv)->ObjectParseToken.tknType != RIGHT_PARENTHESIS_TOKEN)
     {
      CL_SyntaxErrorMessage(theEnv,"defclass slot");
      goto CL_ParseSlotError;
     }

   if (DefclassData(theEnv)->ClassDefaultsModeValue == CONVENIENCE_MODE)
     {
      if (! TestBitMap(specbits,CREATE_ACCESSOR_BIT))
        {
         slot->createReadAccessor = true;

         if (! slot->noCL_Write)
           { slot->createCL_WriteAccessor = true; }
        }
     }

   if (slot->composite)
     CL_BuildCompositeFacets(theEnv,slot,preclist,specbits,&parsedConstraint);
   if (CheckForFacetConflicts(theEnv,slot,&parsedConstraint) == false)
     goto CL_ParseSlotError;
   if (CL_CheckConstraintParseConflicts(theEnv,slot->constraint) == false)
     goto CL_ParseSlotError;
   if (CL_EvaluateCL_SlotDefaultValue(theEnv,slot,specbits) == false)
     goto CL_ParseSlotError;
   if ((slot->dynamicDefault == 0) && (slot->noCL_Write == 1) &&
       (slot->initializeOnly == 0))
     slot->shared = 1;
   slot->constraint = CL_AddConstraint(theEnv,slot->constraint);
   CL_DecrementIndentDepth(theEnv,3);
   return(slist);

CL_ParseSlotError:
   CL_DecrementIndentDepth(theEnv,3);
   CL_DeleteSlots(theEnv,slist);
   return NULL;
  }

/***************************************************
  NAME         : CL_DeleteSlots
  DESCRIPTION  : Deallocates a list of slots and
                   their values
  INPUTS       : The address of the slot list
  RETURNS      : Nothing useful
  SIDE EFFECTS : The slot list is destroyed
  NOTES        : None
 ***************************************************/
void CL_DeleteSlots(
  Environment *theEnv,
  TEMP_SLOT_LINK *slots)
  {
   TEMP_SLOT_LINK *stmp;

   while (slots != NULL)
     {
      stmp = slots;
      slots = slots->nxt;
      CL_DeleteSlotName(theEnv,stmp->desc->slotName);
      CL_ReleaseLexeme(theEnv,stmp->desc->overrideMessage);
      CL_RemoveConstraint(theEnv,stmp->desc->constraint);
      if (stmp->desc->dynamicDefault == 1)
        {
         CL_ExpressionDeinstall(theEnv,(Expression *) stmp->desc->defaultValue);
         CL_ReturnPackedExpression(theEnv,(Expression *) stmp->desc->defaultValue);
        }
      else if (stmp->desc->defaultValue != NULL)
        {
         CL_ReleaseUDFV(theEnv,(UDFValue *) stmp->desc->defaultValue);
         rtn_struct(theEnv,udfValue,stmp->desc->defaultValue);
        }
      rtn_struct(theEnv,slotDescriptor,stmp->desc);
      rtn_struct(theEnv,tempSlotLink,stmp);
     }
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/**************************************************************
  NAME         : NewSlot
  DESCRIPTION  : Allocates and initalizes a new slot structure
  INPUTS       : The symbolic name of the new slot
  RETURNS      : The address of the new slot
  SIDE EFFECTS : None
  NOTES        : Also adds symbols of the foCL_rm get-<name> and
                   put-<name> for slot accessors
 **************************************************************/
static SlotDescriptor *NewSlot(
  Environment *theEnv,
  CLIPSLexeme *name)
  {
   SlotDescriptor *slot;

   slot = get_struct(theEnv,slotDescriptor);
   slot->dynamicDefault = 1;
   slot->defaultSpecified = 0;
   slot->noDefault = 0;
#if DEFRULE_CONSTRUCT
   slot->reactive = 1;
#endif
   slot->noInherit = 0;
   slot->noCL_Write = 0;
   slot->initializeOnly = 0;
   slot->shared = 0;
   slot->multiple = 0;
   slot->composite = 0;
   slot->sharedCount = 0;
   slot->publicVisibility = 0;
   slot->createReadAccessor = false;
   slot->createCL_WriteAccessor = false;
   slot->overrideMessageSpecified = 0;
   slot->cls = NULL;
   slot->defaultValue = NULL;
   slot->constraint = CL_GetConstraintRecord(theEnv);
   slot->slotName = CL_AddSlotName(theEnv,name,0,false);
   slot->overrideMessage = slot->slotName->putHandlerName;
   IncrementLexemeCount(slot->overrideMessage);
   return(slot);
  }

/**********************************************************
  NAME         : InsertSlot
  DESCRIPTION  : Inserts a slot into the list of slots
  INPUTS       : 1) The current head of the slot list
                 2) The slot to be inserted
  RETURNS      : The head of the slot list
  SIDE EFFECTS : The slot is inserted if no errors,
                   otherwise the original list and the
                   new slot are destroyed
  NOTES        : None
 **********************************************************/
static TEMP_SLOT_LINK *InsertSlot(
  Environment *theEnv,
  const char *className,
  TEMP_SLOT_LINK *slist,
  SlotDescriptor *slot)
  {
   TEMP_SLOT_LINK *stmp,*sprv,*tmp;

   tmp = get_struct(theEnv,tempSlotLink);
   tmp->desc = slot;
   tmp->nxt = NULL;
   if (slist == NULL)
     slist = tmp;
   else
     {
      stmp = slist;
      sprv = NULL;
      while (stmp != NULL)
        {
         if (stmp->desc->slotName == slot->slotName)
           {
            tmp->nxt = slist;
            CL_DeleteSlots(theEnv,tmp);
            CL_PrintErrorID(theEnv,"CLSLTPSR",1,false);
            CL_WriteString(theEnv,STDERR,"The '");
            CL_WriteString(theEnv,STDERR,slot->slotName->name->contents);
            CL_WriteString(theEnv,STDERR,"' slot for class '");
            CL_WriteString(theEnv,STDERR,className);
            CL_WriteString(theEnv,STDERR,"' is already specified.\n");
            return NULL;
           }
         sprv = stmp;
         stmp = stmp->nxt;
        }
      sprv->nxt = tmp;
     }
   return(slist);
  }

/****************************************************************
  NAME         : ParseSimpleFacet
  DESCRIPTION  : Parses the following facets for a slot:
                   access, source, propagation, storage,
                   pattern-match, visibility and override-message
  INPUTS       : 1) The input logical name
                 2) The bitmap indicating which facets have
                    already been parsed
                 3) The name of the facet
                 4) The bit to test/set in arg #2 for this facet
                 5) The facet value string which indicates the
                    facet should be false
                 6) The facet value string which indicates the
                    facet should be true
                 7) An alternate value string for use when the
                    first two don't match (can be NULL)
                 7) An alternate value string for use when the
                    first three don't match (can be NULL)
                    (will be an SF_VARIABLE type)
                 9) A buffer to hold the facet value symbol
                    (can be NULL - only set if args #5 and #6
                     are both NULL)
  RETURNS      : -1 on errors
                  0 if first value string matched
                  1 if second value string matched
                  2 if alternate value string matched
                  3 if variable value string matched
                  4 if facet value buffer was set
  SIDE EFFECTS : Messages printed on errors
                 Bitmap marked indicating facet was parsed
                 Facet value symbol buffer set, if appropriate
  NOTES        : None
 *****************************************************************/
static int ParseSimpleFacet(
  Environment *theEnv,
  const char *readSource,
  SlotDescriptor *slot,
  char *specbits,
  const char *facetName,
  int testBit,
  const char *clearRelation,
  const char *setRelation,
  const char *alternateRelation,
  const char *varRelation,
  CLIPSLexeme **facetSymbolicValue)
  {
   int rtnCode;

   if (TestBitMap(specbits,testBit))
     {
      CL_PrintErrorID(theEnv,"CLSLTPSR",2,false);
      CL_WriteString(theEnv,STDERR,"The '");
      CL_WriteString(theEnv,STDERR,facetName);
      CL_WriteString(theEnv,STDERR,"' facet for slot '");
      CL_WriteString(theEnv,STDERR,slot->slotName->name->contents);
      CL_WriteString(theEnv,STDERR,"' is already specified.\n");
      return -1;
     }
   SetBitMap(specbits,testBit);
   CL_SavePPBuffer(theEnv," ");
   CL_GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);

   /* ===============================
      Check for the variable relation
      =============================== */
   if (DefclassData(theEnv)->ObjectParseToken.tknType == SF_VARIABLE_TOKEN)
     {
      if ((varRelation == NULL) ? false :
          (strcmp(DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents,varRelation) == 0))
        rtnCode = 3;
      else
        goto ParseSimpleFacetError;
     }
   else
     {
      if (DefclassData(theEnv)->ObjectParseToken.tknType != SYMBOL_TOKEN)
        goto ParseSimpleFacetError;

      /* ===================================================
         If the facet value buffer is non-NULL
         simply get the value and do not check any relations
         =================================================== */
      if (facetSymbolicValue == NULL)
        {
         if (strcmp(DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents,clearRelation) == 0)
           rtnCode = 0;
         else if (strcmp(DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents,setRelation) == 0)
           rtnCode = 1;
         else if ((alternateRelation == NULL) ? false :
                  (strcmp(DefclassData(theEnv)->ObjectParseToken.lexemeValue->contents,alternateRelation) == 0))
           rtnCode = 2;
         else
           goto ParseSimpleFacetError;
        }
      else
        {
         rtnCode = 4;
         *facetSymbolicValue = DefclassData(theEnv)->ObjectParseToken.lexemeValue;
        }
     }
   CL_GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
   if (DefclassData(theEnv)->ObjectParseToken.tknType != RIGHT_PARENTHESIS_TOKEN)
     goto ParseSimpleFacetError;
   return(rtnCode);

ParseSimpleFacetError:
   CL_SyntaxErrorMessage(theEnv,"slot facet");
   return(-1);
  }

/*************************************************************
  NAME         : CL_ParseDefaultFacet
  DESCRIPTION  : Parses the facet for a slot
  INPUTS       : 1) The input logical name
                 2) The bitmap indicating which facets have
                    already been parsed
                 3) The slot descriptor to set
  RETURNS      : True if all OK, false otherwise
  SIDE EFFECTS : Slot  set and parsed facet bitmap set
  NOTES        : Syntax: (default ?NONE|<expression>*)
                         (default-dynamic <expression>*)
 *************************************************************/
static bool CL_ParseDefaultFacet(
  Environment *theEnv,
  const char *readSource,
  char *specbits,
  SlotDescriptor *slot)
  {
   Expression *tmp;
   bool error, noneSpecified, deriveSpecified;

   if (TestBitMap(specbits,DEFAULT_BIT))
     {
      CL_PrintErrorID(theEnv,"CLSLTPSR",2,false);
      CL_WriteString(theEnv,STDERR,"The 'default' facet for slot '");
      CL_WriteString(theEnv,STDERR,slot->slotName->name->contents);
      CL_WriteString(theEnv,STDERR,"' is already specified.\n");
      return false;
     }
   SetBitMap(specbits,DEFAULT_BIT);
   error = false;
   tmp = CL_ParseDefault(theEnv,readSource,true,TestBitMap(specbits,DEFAULT_DYNAMIC_BIT),
                      false,&noneSpecified,&deriveSpecified,&error);
   if (error == true)
     return false;
   if (noneSpecified || deriveSpecified)
     {
     if (noneSpecified)
       {
        slot->noDefault = 1;
        slot->defaultSpecified = 1;
       }
      else
        CL_ClearBitMap(specbits,DEFAULT_BIT);
     }
   else
     {
      slot->defaultValue = CL_PackExpression(theEnv,tmp);
      CL_ReturnExpression(theEnv,tmp);
      CL_ExpressionInstall(theEnv,(Expression *) slot->defaultValue);
      slot->defaultSpecified = 1;
     }
   return true;
  }

/**************************************************************************
  NAME         : CL_BuildCompositeFacets
  DESCRIPTION  : Composite slots are ones that get their facets
                   from more than one class.  By default, the most
                   specific class in object's precedence list specifies
                   the complete set of facets for a slot.  The composite
                   facet in a slot allows facets that are not overridden
                   by the most specific class to be obtained from other
                   classes.

                 Since all superclasses are predeteCL_rmined before creating
                   a new class based on them, this routine need only
                   examine the immediately next most specific class for
                   extra facets.  Even if that slot is also composite, the
                   other facets have already been filtered down.  If the
                   slot is no-inherit, the next most specific class must
                   be examined.
  INPUTS       : 1) The slot descriptor
                 2) The class precedence list
                 3) The bitmap marking which facets were specified in
                    the original slot definition
  RETURNS      : Nothing useful
  SIDE EFFECTS : Composite slot is updated to reflect facets from
                   a less specific class
  NOTES        : Assumes slot is composite
 *************************************************************************/
static void CL_BuildCompositeFacets(
  Environment *theEnv,
  SlotDescriptor *sd,
  PACKED_CLASS_LINKS *preclist,
  const char *specbits,
  CONSTRAINT_PARSE_RECORD *parsedConstraint)
  {
   SlotDescriptor *compslot = NULL;
   unsigned long i;

   for (i = 1 ; i < preclist->classCount ; i++)
     {
      compslot = CL_FindClassSlot(preclist->classArray[i],sd->slotName->name);
      if ((compslot != NULL) ? (compslot->noInherit == 0) : false)
        break;
     }
   if (compslot != NULL)
     {
      if ((sd->defaultSpecified == 0) && (compslot->defaultSpecified == 1))
        {
         sd->dynamicDefault = compslot->dynamicDefault;
         sd->noDefault = compslot->noDefault;
         sd->defaultSpecified = 1;
         if (compslot->defaultValue != NULL)
           {
            if (sd->dynamicDefault)
              {
               sd->defaultValue = CL_PackExpression(theEnv,(Expression *) compslot->defaultValue);
               CL_ExpressionInstall(theEnv,(Expression *) sd->defaultValue);
              }
            else
              {
               sd->defaultValue = get_struct(theEnv,udfValue);
               GenCopyMemory(UDFValue,1,sd->defaultValue,compslot->defaultValue);
               CL_RetainUDFV(theEnv,(UDFValue *) sd->defaultValue);
              }
           }
        }
      if (! TestBitMap(specbits,FIELD_BIT))
        sd->multiple = compslot->multiple;
      if (! TestBitMap(specbits,STORAGE_BIT))
        sd->shared = compslot->shared;
      if (! TestBitMap(specbits,ACCESS_BIT))
        {
         sd->noCL_Write = compslot->noCL_Write;
         sd->initializeOnly = compslot->initializeOnly;
        }
#if DEFRULE_CONSTRUCT
      if (! TestBitMap(specbits,MATCH_BIT))
        sd->reactive = compslot->reactive;
#endif
      if (! TestBitMap(specbits,VISIBILITY_BIT))
        sd->publicVisibility = compslot->publicVisibility;
      if (! TestBitMap(specbits,CREATE_ACCESSOR_BIT))
        {
         sd->createReadAccessor = compslot->createReadAccessor;
         sd->createCL_WriteAccessor = compslot->createCL_WriteAccessor;
        }
      if ((! TestBitMap(specbits,OVERRIDE_MSG_BIT)) &&
          compslot->overrideMessageSpecified)
        {
         CL_ReleaseLexeme(theEnv,sd->overrideMessage);
         sd->overrideMessage = compslot->overrideMessage;
         IncrementLexemeCount(sd->overrideMessage);
         sd->overrideMessageSpecified = true;
        }
      CL_OverlayConstraint(theEnv,parsedConstraint,sd->constraint,compslot->constraint);
     }
  }

/***************************************************
  NAME         : CheckForFacetConflicts
  DESCRIPTION  : DeteCL_rmines if all facets specified
                 (and inherited) for a slot are
                 consistent
  INPUTS       : 1) The slot descriptor
                 2) The parse record for the
                    type constraints on the slot
  RETURNS      : True if all OK,
                 false otherwise
  SIDE EFFECTS : Min and Max fields replaced in
                 constraint for single-field slot
  NOTES        : None
 ***************************************************/
static bool CheckForFacetConflicts(
  Environment *theEnv,
  SlotDescriptor *sd,
  CONSTRAINT_PARSE_RECORD *parsedConstraint)
  {
   if (sd->multiple == 0)
     {
      if (parsedConstraint->cardinality)
        {
         CL_PrintErrorID(theEnv,"CLSLTPSR",3,true);
         CL_WriteString(theEnv,STDERR,"The 'cardinality' facet can only be used with multifield slots.\n");
         return false;
        }
      else
        {
         CL_ReturnExpression(theEnv,sd->constraint->minFields);
         CL_ReturnExpression(theEnv,sd->constraint->maxFields);
         sd->constraint->minFields = CL_GenConstant(theEnv,INTEGER_TYPE,CL_CreateInteger(theEnv,1LL));
         sd->constraint->maxFields = CL_GenConstant(theEnv,INTEGER_TYPE,CL_CreateInteger(theEnv,1LL));
        }
     }
   if (sd->noDefault && sd->noCL_Write)
     {
      CL_PrintErrorID(theEnv,"CLSLTPSR",4,true);
      CL_WriteString(theEnv,STDERR,"Slots with an 'access' facet value of 'read-only' must have a default value.\n");
      return false;
     }
   if (sd->noCL_Write && (sd->createCL_WriteAccessor || sd->overrideMessageSpecified))
     {
      CL_PrintErrorID(theEnv,"CLSLTPSR",5,true);
      CL_WriteString(theEnv,STDERR,"Slots with an 'access' facet value of 'read-only' cannot have a write accessor.\n");
      return false;
     }
   if (sd->noInherit && sd->publicVisibility)
     {
      CL_PrintErrorID(theEnv,"CLSLTPSR",6,true);
      CL_WriteString(theEnv,STDERR,"Slots with a 'propagation' facet value of 'no-inherit' cannot have a 'visibility' facet value of 'public'.\n");
      return false;
     }
   return true;
  }

/********************************************************************
  NAME         : CL_EvaluateCL_SlotDefaultValue
  DESCRIPTION  : Checks the default value against the slot
                 constraints and evaluates static default values
  INPUTS       : 1) The slot descriptor
                 2) The bitmap marking which facets were specified in
                    the original slot definition
  RETURNS      : True if all OK, false otherwise
  SIDE EFFECTS : Static default value expressions deleted and
                 replaced with data object evaluation
  NOTES        : On errors, slot is marked as dynamix so that
                 CL_DeleteSlots() will erase the slot expression
 ********************************************************************/
static bool CL_EvaluateCL_SlotDefaultValue(
  Environment *theEnv,
  SlotDescriptor *sd,
  const char *specbits)
  {
   UDFValue temp;
   bool oldce,olddcc, vPass;
   ConstraintViolationType vCode;

   /* ===================================================================
      Slot default value expression is marked as dynamic until now so
      that CL_DeleteSlots() would erase in the event of an error.  The delay
      was so that the evaluation of a static default value could be
      delayed until all the constraints were parsed.
      =================================================================== */
   if (! TestBitMap(specbits,DEFAULT_DYNAMIC_BIT))
     sd->dynamicDefault = 0;

   if (sd->noDefault)
     return true;

   if (sd->dynamicDefault == 0)
     {
      if (TestBitMap(specbits,DEFAULT_BIT))
        {
         oldce = CL_ExecutingConstruct(theEnv);
         SetCL_ExecutingConstruct(theEnv,true);
         olddcc = CL_SetDynamicConstraintChecking(theEnv,true);
         vPass = CL_EvaluateAndStoreInDataObject(theEnv,sd->multiple,
                  (Expression *) sd->defaultValue,&temp,true);
         if (vPass != false)
           vPass = (CL_ValidSlotValue(theEnv,&temp,sd,NULL,"the 'default' facet") == PSE_NO_ERROR);
         CL_SetDynamicConstraintChecking(theEnv,olddcc);
         SetCL_ExecutingConstruct(theEnv,oldce);
         if (vPass)
           {
            CL_ExpressionDeinstall(theEnv,(Expression *) sd->defaultValue);
            CL_ReturnPackedExpression(theEnv,(Expression *) sd->defaultValue);
            sd->defaultValue = get_struct(theEnv,udfValue);
            GenCopyMemory(UDFValue,1,sd->defaultValue,&temp);
            CL_RetainUDFV(theEnv,(UDFValue *) sd->defaultValue);
           }
         else
           {
            sd->dynamicDefault = 1;
            return false;
           }
        }
      else if (sd->defaultSpecified == 0)
        {
         sd->defaultValue = get_struct(theEnv,udfValue);
         CL_DeriveDefaultFromConstraints(theEnv,sd->constraint,
                                      (UDFValue *) sd->defaultValue,sd->multiple,true);
         CL_RetainUDFV(theEnv,(UDFValue *) sd->defaultValue);
        }
     }
   else
     {
      vCode = CL_ConstraintCheckExpressionChain(theEnv,(Expression *) sd->defaultValue,sd->constraint);
      if (vCode != NO_VIOLATION)
        {
         CL_PrintErrorID(theEnv,"CSTRNCHK",1,false);
         CL_WriteString(theEnv,STDERR,"Expression for ");
         CL_PrintSlot(theEnv,STDERR,sd,NULL,"dynamic default value");
         CL_ConstraintViolationErrorMessage(theEnv,NULL,NULL,0,0,NULL,0,
                                         vCode,sd->constraint,false);
         return false;
        }
     }
   return true;
  }

#endif

