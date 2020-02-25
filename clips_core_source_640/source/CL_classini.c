   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/02/18             */
   /*                                                     */
   /*               CLASS INITIALIZATION MODULE           */
   /*******************************************************/

/**************************************************************/
/* Purpose: Defclass Initialization Routines                  */
/*                                                            */
/* Principal Programmer(s):                                   */
/*      Brian L. Dantes                                       */
/*                                                            */
/* Contributing Programmer(s):                                */
/*                                                            */
/* Revision History:                                          */
/*                                                            */
/*      6.23: Corrected compilation errors for files          */
/*            generated by constructs-to-c. DR0861            */
/*                                                            */
/*      6.24: Added allowed-classes slot facet.               */
/*                                                            */
/*            Converted INSTANCE_PATTERN_MATCHING to          */
/*            DEFRULE_CONSTRUCT.                              */
/*                                                            */
/*            Corrected code to remove run-time program       */
/*            compiler warning.                               */
/*                                                            */
/*      6.30: Borland C (IBM_TBC) and Metrowerks CodeWarrior  */
/*            (MAC_MCW, IBM_MCW) are no longer supported.     */
/*                                                            */
/*            Changed integer type/precision.                 */
/*                                                            */
/*            Support for hashed alpha memories.              */
/*                                                            */
/*            Added const qualifiers to remove C++            */
/*            deprecation warnings.                           */
/*                                                            */
/*            Changed find construct functionality so that    */
/*            imported modules are search when locating a     */
/*            named construct.                                */
/*                                                            */
/*      6.31: Optimization of slot ID creation previously    */
/*            provided by NewSlotNameID function.            */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.        */
/*                                                            */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            Removed initial-object support.                 */
/*                                                            */
/*            Pretty print functions accept optional logical */
/*            name argument.                                 */
/*                                                           */
/**************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if OBJECT_SYSTEM

#include <stdio.h>

#include "classcom.h"
#include "classexm.h"
#include "classfun.h"
#include "classinf.h"
#include "classpsr.h"
#include "cstrccom.h"
#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "inscom.h"
#include "memalloc.h"
#include "modulpsr.h"
#include "modulutl.h"
#include "msgcom.h"
#include "watch.h"

#if DEFINSTANCES_CONSTRUCT
#include "defins.h"
#endif

#if INSTANCE_SET_QUERIES
#include "insquery.h"
#endif

#if BLOAD_AND_BSAVE || BLOAD || BLOAD_ONLY
#include "bload.h"
#include "objbin.h"
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
#include "objcmp.h"
#endif

#if DEFRULE_CONSTRUCT
#include "objrtbld.h"
#include "objrtfnx.h"
#include "objrtmch.h"
#endif

#if RUN_TIME
#include "insfun.h"
#include "msgfun.h"
#include "pattern.h"
#endif

#include "classini.h"

/* =========================================
   *****************************************
                   CONSTANTS
   =========================================
   ***************************************** */
#define SUPERCLASS_RLN       "is-a"
#define NAME_RLN             "name"

/* =========================================
   *****************************************
      INTERNALLY VISIBLE FUNCTION HEADERS
   =========================================
   ***************************************** */

static void SetupDefclasses (Environment *);
static void DeallocateDefclassData (Environment *);

#if (! RUN_TIME)
static void CL_DestroyDefclassAction (Environment *, ConstructHeader *,
				      void *);
static Defclass *AddSystemClass (Environment *, const char *, Defclass *);
static void *AllocateModule (Environment *);
static void ReturnModule (Environment *, void *);
#else
static void SearchForHashedPatternNodes (Environment *,
					 OBJECT_PATTERN_NODE *);
#endif

#if (! BLOAD_ONLY) && (! RUN_TIME) && DEFMODULE_CONSTRUCT
static void UpdateDefclassesScope (Environment *, void *);
#endif

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/**********************************************************
  NAME         : CL_SetupObjectSystem
  DESCRIPTION  : Initializes all COOL constructs, functions,
                   and data structures
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : COOL initialized
  NOTES        : Order of setup calls is important
 **********************************************************/
void
CL_SetupObjectSystem (Environment * theEnv)
{
  EntityRecord defclassEntityRecord = { "DEFCLASS_PTR", DEFCLASS_PTR, 1, 0, 0,
    NULL, NULL, NULL, NULL, NULL,
    (EntityBusyCountFunction *) CL_DecrementDefclassBusyCount,
    (EntityBusyCountFunction *) CL_IncrementDefclassBusyCount,
    NULL, NULL, NULL, NULL, NULL
  };

  CL_AllocateEnvironmentData (theEnv, DEFCLASS_DATA,
			      sizeof (struct defclassData), NULL);
  CL_AddEnvironmentCleanupFunction (theEnv, "defclasses",
				    DeallocateDefclassData, -500);

  memcpy (&DefclassData (theEnv)->DefclassEntityRecord, &defclassEntityRecord,
	  sizeof (struct entityRecord));

  DefclassData (theEnv)->newSlotID = 2;	// IS_A and NAME assigned 0 and 1

#if ! RUN_TIME
  DefclassData (theEnv)->ClassDefaultsModeValue = CONVENIENCE_MODE;
  DefclassData (theEnv)->ISA_SYMBOL =
    CL_CreateSymbol (theEnv, SUPERCLASS_RLN);
  IncrementLexemeCount (DefclassData (theEnv)->ISA_SYMBOL);
  DefclassData (theEnv)->NAME_SYMBOL = CL_CreateSymbol (theEnv, NAME_RLN);
  IncrementLexemeCount (DefclassData (theEnv)->NAME_SYMBOL);
#endif

  SetupDefclasses (theEnv);
  Setup_Instances (theEnv);
  CL_SetupMessageHandlers (theEnv);

#if DEFINSTANCES_CONSTRUCT
  CL_SetupDefinstances (theEnv);
#endif

#if INSTANCE_SET_QUERIES
  CL_SetupQuery (theEnv);
#endif

#if BLOAD_AND_BSAVE || BLOAD || BLOAD_ONLY
  SetupObjects_Bload (theEnv);
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
  CL_SetupObjectsCompiler (theEnv);
#endif

#if DEFRULE_CONSTRUCT
  CL_SetupObjectPatternStuff (theEnv);
#endif
}

/***************************************************/
/* DeallocateDefclassData: Deallocates environment */
/*    data for the defclass construct.             */
/***************************************************/
static void
DeallocateDefclassData (Environment * theEnv)
{
#if ! RUN_TIME
  SLOT_NAME *tmpSNPPtr, *nextSNPPtr;
  int i;
  struct defclassModule *theModuleItem;
  Defmodule *theModule;
  bool bloaded = false;

#if BLOAD || BLOAD_AND_BSAVE
  if (CL_Bloaded (theEnv))
    bloaded = true;
#endif

   /*=============================*/
  /* Destroy all the defclasses. */
   /*=============================*/

  if (!bloaded)
    {
      CL_DoForAllConstructs (theEnv, CL_DestroyDefclassAction,
			     DefclassData (theEnv)->CL_DefclassModuleIndex,
			     false, NULL);

      for (theModule = CL_GetNextDefmodule (theEnv, NULL);
	   theModule != NULL;
	   theModule = CL_GetNextDefmodule (theEnv, theModule))
	{
	  theModuleItem = (struct defclassModule *)
	    CL_GetModuleItem (theEnv, theModule,
			      DefclassData (theEnv)->CL_DefclassModuleIndex);
	  rtn_struct (theEnv, defclassModule, theModuleItem);
	}
    }

   /*==========================*/
  /* Remove the class tables. */
   /*==========================*/

  if (!bloaded)
    {
      if (DefclassData (theEnv)->ClassIDMap != NULL)
	{
	  CL_genfree (theEnv, DefclassData (theEnv)->ClassIDMap,
		      DefclassData (theEnv)->AvailClassID *
		      sizeof (Defclass *));
	}
    }

  if (DefclassData (theEnv)->ClassTable != NULL)
    {
      CL_genfree (theEnv, DefclassData (theEnv)->ClassTable,
		  sizeof (Defclass *) * CLASS_TABLE_HASH_SIZE);
    }

   /*==============================*/
  /* Free up the slot name table. */
   /*==============================*/

  if (!bloaded)
    {
      for (i = 0; i < SLOT_NAME_TABLE_HASH_SIZE; i++)
	{
	  tmpSNPPtr = DefclassData (theEnv)->SlotNameTable[i];

	  while (tmpSNPPtr != NULL)
	    {
	      nextSNPPtr = tmpSNPPtr->nxt;
	      rtn_struct (theEnv, slotName, tmpSNPPtr);
	      tmpSNPPtr = nextSNPPtr;
	    }
	}
    }

  if (DefclassData (theEnv)->SlotNameTable != NULL)
    {
      CL_genfree (theEnv, DefclassData (theEnv)->SlotNameTable,
		  sizeof (SLOT_NAME *) * SLOT_NAME_TABLE_HASH_SIZE);
    }
#else
  Defclass *cls;
  void *tmpexp;
  unsigned int i;
  int j;

  if (DefclassData (theEnv)->ClassTable != NULL)
    {
      for (j = 0; j < CLASS_TABLE_HASH_SIZE; j++)
	for (cls = DefclassData (theEnv)->ClassTable[j]; cls != NULL;
	     cls = cls->nxtHash)
	  {
	    for (i = 0; i < cls->slotCount; i++)
	      {
		if ((cls->slots[i].defaultValue != NULL)
		    && (cls->slots[i].dynamicDefault == 0))
		  {
		    tmpexp =
		      ((UDFValue *) cls->slots[i].defaultValue)->
		      supplementalInfo;
		    rtn_struct (theEnv, udfValue, cls->slots[i].defaultValue);
		    cls->slots[i].defaultValue = tmpexp;
		  }
	      }
	  }
    }
#endif
}

#if ! RUN_TIME
/*********************************************************/
/* CL_DestroyDefclassAction: Action used to remove defclass */
/*   as a result of CL_DestroyEnvironment.                  */
/*********************************************************/
static void
CL_DestroyDefclassAction (Environment * theEnv,
			  ConstructHeader * theConstruct, void *buffer)
{
#if MAC_XCD
#pragma unused(buffer)
#endif
  Defclass *theDefclass = (Defclass *) theConstruct;

  if (theDefclass == NULL)
    return;

#if (! BLOAD_ONLY)
  CL_DestroyDefclass (theEnv, theDefclass);
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
}
#endif

#if RUN_TIME

/***************************************************
  NAME         : Objects_RunTimeInitialize
  DESCRIPTION  : Initializes objects system lists
                   in a run-time module
  INPUTS       : 1) Pointer to new class hash table
                 2) Pointer to new slot name table
  RETURNS      : Nothing useful
  SIDE EFFECTS : Global pointers set
  NOTES        : None
 ***************************************************/
void
Objects_RunTimeInitialize (Environment * theEnv,
			   Defclass * ctable[],
			   SLOT_NAME * sntable[],
			   Defclass ** cidmap, unsigned short mid)
{
  Defclass *cls;
  void *tmpexp;
  unsigned int i, j;

  if (DefclassData (theEnv)->ClassTable != NULL)
    {
      for (j = 0; j < CLASS_TABLE_HASH_SIZE; j++)
	for (cls = DefclassData (theEnv)->ClassTable[j]; cls != NULL;
	     cls = cls->nxtHash)
	  {
	    for (i = 0; i < cls->slotCount; i++)
	      {
		/* =====================================================================
		   For static default values, the data object value needs to deinstalled
		   and deallocated, and the expression needs to be restored (which was
		   temporarily stored in the supplementalInfo field of the data object)
		   ===================================================================== */
		if ((cls->slots[i].defaultValue != NULL)
		    && (cls->slots[i].dynamicDefault == 0))
		  {
		    tmpexp =
		      ((UDFValue *) cls->slots[i].defaultValue)->
		      supplementalInfo;
		    CL_ReleaseUDFV (theEnv,
				    (UDFValue *) cls->slots[i].defaultValue);
		    rtn_struct (theEnv, udfValue, cls->slots[i].defaultValue);
		    cls->slots[i].defaultValue = tmpexp;
		  }
	      }
	  }
    }

  InstanceQueryData (theEnv)->QUERY_DELIMITER_SYMBOL =
    CL_FindSymbolHN (theEnv, QUERY_DELIMITER_STRING, SYMBOL_BIT);
  MessageHandlerData (theEnv)->INIT_SYMBOL =
    CL_FindSymbolHN (theEnv, INIT_STRING, SYMBOL_BIT);
  MessageHandlerData (theEnv)->DELETE_SYMBOL =
    CL_FindSymbolHN (theEnv, DELETE_STRING, SYMBOL_BIT);
  MessageHandlerData (theEnv)->CREATE_SYMBOL =
    CL_FindSymbolHN (theEnv, CREATE_STRING, SYMBOL_BIT);
  DefclassData (theEnv)->ISA_SYMBOL =
    CL_FindSymbolHN (theEnv, SUPERCLASS_RLN, SYMBOL_BIT);
  DefclassData (theEnv)->NAME_SYMBOL =
    CL_FindSymbolHN (theEnv, NAME_RLN, SYMBOL_BIT);

  DefclassData (theEnv)->ClassTable = (Defclass **) ctable;
  DefclassData (theEnv)->SlotNameTable = (SLOT_NAME **) sntable;
  DefclassData (theEnv)->ClassIDMap = (Defclass **) cidmap;
  DefclassData (theEnv)->MaxClassID = mid;
  DefclassData (theEnv)->PrimitiveClassMap[FLOAT_TYPE] =
    CL_LookupDefclassByMdlOrScope (theEnv, FLOAT_TYPE_NAME);
  DefclassData (theEnv)->PrimitiveClassMap[INTEGER_TYPE] =
    CL_LookupDefclassByMdlOrScope (theEnv, INTEGER_TYPE_NAME);
  DefclassData (theEnv)->PrimitiveClassMap[STRING_TYPE] =
    CL_LookupDefclassByMdlOrScope (theEnv, STRING_TYPE_NAME);
  DefclassData (theEnv)->PrimitiveClassMap[SYMBOL_TYPE] =
    CL_LookupDefclassByMdlOrScope (theEnv, SYMBOL_TYPE_NAME);
  DefclassData (theEnv)->PrimitiveClassMap[MULTIFIELD_TYPE] =
    CL_LookupDefclassByMdlOrScope (theEnv, MULTIFIELD_TYPE_NAME);
  DefclassData (theEnv)->PrimitiveClassMap[EXTERNAL_ADDRESS_TYPE] =
    CL_LookupDefclassByMdlOrScope (theEnv, EXTERNAL_ADDRESS_TYPE_NAME);
  DefclassData (theEnv)->PrimitiveClassMap[FACT_ADDRESS_TYPE] =
    CL_LookupDefclassByMdlOrScope (theEnv, FACT_ADDRESS_TYPE_NAME);
  DefclassData (theEnv)->PrimitiveClassMap[INSTANCE_NAME_TYPE] =
    CL_LookupDefclassByMdlOrScope (theEnv, INSTANCE_NAME_TYPE_NAME);
  DefclassData (theEnv)->PrimitiveClassMap[INSTANCE_ADDRESS_TYPE] =
    CL_LookupDefclassByMdlOrScope (theEnv, INSTANCE_ADDRESS_TYPE_NAME);

  for (j = 0; j < CLASS_TABLE_HASH_SIZE; j++)
    for (cls = DefclassData (theEnv)->ClassTable[j]; cls != NULL;
	 cls = cls->nxtHash)
      {
	cls->header.env = theEnv;

	for (i = 0; i < cls->handlerCount; i++)
	  {
	    cls->handlers[i].header.env = theEnv;
	  }

	for (i = 0; i < cls->slotCount; i++)
	  {
	    if ((cls->slots[i].defaultValue != NULL)
		&& (cls->slots[i].dynamicDefault == 0))
	      {
		tmpexp = cls->slots[i].defaultValue;
		cls->slots[i].defaultValue = get_struct (theEnv, udfValue);
		CL_EvaluateAndStoreInDataObject (theEnv,
						 cls->slots[i].multiple,
						 (Expression *) tmpexp,
						 (UDFValue *) cls->slots[i].
						 defaultValue, true);
		CL_RetainUDFV (theEnv,
			       (UDFValue *) cls->slots[i].defaultValue);
		((UDFValue *) cls->slots[i].defaultValue)->supplementalInfo =
		  tmpexp;
	      }
	  }
      }

  SearchForHashedPatternNodes (theEnv,
			       ObjectReteData (theEnv)->
			       ObjectPatternNetworkPointer);
}

/********************************/
/* SearchForHashedPatternNodes: */
/********************************/
static void
SearchForHashedPatternNodes (Environment * theEnv,
			     OBJECT_PATTERN_NODE * theNode)
{
  while (theNode != NULL)
    {
      if ((theNode->lastLevel != NULL) && (theNode->lastLevel->selector))
	{
	  CL_AddHashedPatternNode (theEnv, theNode->lastLevel, theNode,
				   theNode->networkTest->type,
				   theNode->networkTest->value);
	}

      SearchForHashedPatternNodes (theEnv, theNode->nextLevel);

      theNode = theNode->rightNode;
    }
}

#else

/***************************************************************
  NAME         : CL_CreateSystemClasses
  DESCRIPTION  : Creates the built-in system classes
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : System classes inserted in the
                   class hash table
  NOTES        : The binary/load save indices for the primitive
                   types (integer, float, symbol and string,
                   multifield, external-address and fact-address)
                   are very important.  Need to be able to refer
                   to types with the same index regardless of
                   whether the object system is installed or
                   not.  Thus, the bsave/blaod indices of these
                   classes match their integer codes.
                WARNING!!: Assumes no classes exist yet!
 ***************************************************************/
void
CL_CreateSystemClasses (Environment * theEnv, void *context)
{
  Defclass *user, *any, *primitive, *number, *lexeme, *address, *instance;

  /* ===================================
     Add canonical slot name entries for
     the is-a and name fields - used for
     object patterns
     =================================== */
  CL_AddSlotName (theEnv, DefclassData (theEnv)->ISA_SYMBOL, ISA_ID, true);
  CL_AddSlotName (theEnv, DefclassData (theEnv)->NAME_SYMBOL, NAME_ID, true);

  DefclassData (theEnv)->newSlotID = 2;	// IS_A and NAME assigned 0 and 1

  /* =========================================================
     CL_Bsave Indices for non-primitive classes start at 9
     Object is 9, Primitive is 10, Number is 11,
     Lexeme is 12, Address is 13, and Instance is 14.
     because: float = 0, integer = 1, symbol = 2, string = 3,
     multifield = 4, and external-address = 5 and
     fact-address = 6, instance-adress = 7 and
     instance-name = 8.
     ========================================================= */
  any = AddSystemClass (theEnv, OBJECT_TYPE_NAME, NULL);
  primitive = AddSystemClass (theEnv, PRIMITIVE_TYPE_NAME, any);
  user = AddSystemClass (theEnv, USER_TYPE_NAME, any);

  number = AddSystemClass (theEnv, NUMBER_TYPE_NAME, primitive);
  DefclassData (theEnv)->PrimitiveClassMap[INTEGER_TYPE] =
    AddSystemClass (theEnv, INTEGER_TYPE_NAME, number);
  DefclassData (theEnv)->PrimitiveClassMap[FLOAT_TYPE] =
    AddSystemClass (theEnv, FLOAT_TYPE_NAME, number);
  lexeme = AddSystemClass (theEnv, LEXEME_TYPE_NAME, primitive);
  DefclassData (theEnv)->PrimitiveClassMap[SYMBOL_TYPE] =
    AddSystemClass (theEnv, SYMBOL_TYPE_NAME, lexeme);
  DefclassData (theEnv)->PrimitiveClassMap[STRING_TYPE] =
    AddSystemClass (theEnv, STRING_TYPE_NAME, lexeme);
  DefclassData (theEnv)->PrimitiveClassMap[MULTIFIELD_TYPE] =
    AddSystemClass (theEnv, MULTIFIELD_TYPE_NAME, primitive);
  address = AddSystemClass (theEnv, ADDRESS_TYPE_NAME, primitive);
  DefclassData (theEnv)->PrimitiveClassMap[EXTERNAL_ADDRESS_TYPE] =
    AddSystemClass (theEnv, EXTERNAL_ADDRESS_TYPE_NAME, address);
  DefclassData (theEnv)->PrimitiveClassMap[FACT_ADDRESS_TYPE] =
    AddSystemClass (theEnv, FACT_ADDRESS_TYPE_NAME, address);
  instance = AddSystemClass (theEnv, INSTANCE_TYPE_NAME, primitive);
  DefclassData (theEnv)->PrimitiveClassMap[INSTANCE_ADDRESS_TYPE] =
    AddSystemClass (theEnv, INSTANCE_ADDRESS_TYPE_NAME, instance);
  DefclassData (theEnv)->PrimitiveClassMap[INSTANCE_NAME_TYPE] =
    AddSystemClass (theEnv, INSTANCE_NAME_TYPE_NAME, instance);

  /* ================================================================================
     INSTANCE-ADDRESS is-a INSTANCE and ADDRESS.  The links between INSTANCE-ADDRESS
     and ADDRESS still need to be made.
     =============================================================================== */
  CL_AddClassLink (theEnv,
		   &DefclassData (theEnv)->
		   PrimitiveClassMap[INSTANCE_ADDRESS_TYPE]->
		   directSuperclasses, address, true, 0);
  CL_AddClassLink (theEnv,
		   &DefclassData (theEnv)->
		   PrimitiveClassMap[INSTANCE_ADDRESS_TYPE]->allSuperclasses,
		   address, false, 2);
  CL_AddClassLink (theEnv, &address->directSubclasses,
		   DefclassData (theEnv)->
		   PrimitiveClassMap[INSTANCE_ADDRESS_TYPE], true, 0);

  /* =======================================================================
     The order of the class in the list MUST correspond to their type codes!
     See CONSTANT.H
     ======================================================================= */
  CL_AddConstructToModule (&DefclassData (theEnv)->
			   PrimitiveClassMap[FLOAT_TYPE]->header);
  CL_AddConstructToModule (&DefclassData (theEnv)->
			   PrimitiveClassMap[INTEGER_TYPE]->header);
  CL_AddConstructToModule (&DefclassData (theEnv)->
			   PrimitiveClassMap[SYMBOL_TYPE]->header);
  CL_AddConstructToModule (&DefclassData (theEnv)->
			   PrimitiveClassMap[STRING_TYPE]->header);
  CL_AddConstructToModule (&DefclassData (theEnv)->
			   PrimitiveClassMap[MULTIFIELD_TYPE]->header);
  CL_AddConstructToModule (&DefclassData (theEnv)->
			   PrimitiveClassMap[EXTERNAL_ADDRESS_TYPE]->header);
  CL_AddConstructToModule (&DefclassData (theEnv)->
			   PrimitiveClassMap[FACT_ADDRESS_TYPE]->header);
  CL_AddConstructToModule (&DefclassData (theEnv)->
			   PrimitiveClassMap[INSTANCE_ADDRESS_TYPE]->header);
  CL_AddConstructToModule (&DefclassData (theEnv)->
			   PrimitiveClassMap[INSTANCE_NAME_TYPE]->header);
  CL_AddConstructToModule (&any->header);
  CL_AddConstructToModule (&primitive->header);
  CL_AddConstructToModule (&number->header);
  CL_AddConstructToModule (&lexeme->header);
  CL_AddConstructToModule (&address->header);
  CL_AddConstructToModule (&instance->header);
  CL_AddConstructToModule (&user->header);

  for (any = CL_GetNextDefclass (theEnv, NULL);
       any != NULL; any = CL_GetNextDefclass (theEnv, any))
    CL_AssignClassID (theEnv, any);
}

#endif

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/*********************************************************
  NAME         : SetupDefclasses
  DESCRIPTION  : Initializes Class Hash Table,
                   Function Parsers, and Data Structures
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS :
  NOTES        : None
 *********************************************************/
static void
SetupDefclasses (Environment * theEnv)
{
  CL_InstallPrimitive (theEnv, &DefclassData (theEnv)->DefclassEntityRecord,
		       DEFCLASS_PTR);

  DefclassData (theEnv)->CL_DefclassModuleIndex =
    CL_RegisterModuleItem (theEnv, "defclass",
#if (! RUN_TIME)
			   AllocateModule, ReturnModule,
#else
			   NULL, NULL,
#endif
#if BLOAD_AND_BSAVE || BLOAD || BLOAD_ONLY
			   CL_Bload_DefclassModuleReference,
#else
			   NULL,
#endif
#if CONSTRUCT_COMPILER && (! RUN_TIME)
			   CL_DefclassCModuleReference,
#else
			   NULL,
#endif
			   (CL_FindConstructFunction *)
			   CL_FindDefclassInModule);

  DefclassData (theEnv)->DefclassConstruct =
    CL_AddConstruct (theEnv, "defclass", "defclasses",
#if (! BLOAD_ONLY) && (! RUN_TIME)
		     CL_ParseDefclass,
#else
		     NULL,
#endif
		     (CL_FindConstructFunction *) CL_FindDefclass,
		     CL_GetConstructNamePointer, CL_GetConstructPPFo_rm,
		     CL_GetConstructModuleItem,
		     (GetNextConstructFunction *) CL_GetNextDefclass,
		     CL_SetNextConstruct,
		     (IsConstructDeletableFunction *) CL_DefclassIsDeletable,
		     (DeleteConstructFunction *) CL_Undefclass,
#if (! RUN_TIME)
		     (FreeConstructFunction *) CL_RemoveDefclass
#else
		     NULL
#endif
    );

  CL_Add_ClearReadyFunction (theEnv, "defclass", CL_InstancesPurge, 0, NULL);

#if ! RUN_TIME
  CL_Add_ClearFunction (theEnv, "defclass", CL_CreateSystemClasses, 0, NULL);
  CL_InitializeClasses (theEnv);

#if ! BLOAD_ONLY
#if DEFMODULE_CONSTRUCT
  CL_AddPortConstructItem (theEnv, "defclass", SYMBOL_TOKEN);
  CL_AddAfterModuleDefinedFunction (theEnv, "defclass", UpdateDefclassesScope,
				    0, NULL);
#endif
  CL_AddUDF (theEnv, "undefclass", "v", 1, 1, "y", CL_UndefclassCommand,
	     "CL_UndefclassCommand", NULL);

  CL_Add_SaveFunction (theEnv, "defclass", CL_SaveDefclasses, 10, NULL);
#endif

#if DEBUGGING_FUNCTIONS
  CL_AddUDF (theEnv, "list-defclasses", "v", 0, 1, "y",
	     CL_ListDefclassesCommand, "CL_ListDefclassesCommand", NULL);
  CL_AddUDF (theEnv, "ppdefclass", "vs", 1, 2, ";y;ldsyn",
	     CL_PPDefclassCommand, "CL_PPDefclassCommand", NULL);
  CL_AddUDF (theEnv, "describe-class", "v", 1, 1, "y", Describe_ClassCommand,
	     "Describe_ClassCommand", NULL);
  CL_AddUDF (theEnv, "browse-classes", "v", 0, 1, "y",
	     CL_BrowseClassesCommand, "CL_BrowseClassesCommand", NULL);
#endif

  CL_AddUDF (theEnv, "get-defclass-list", "m", 0, 1, "y",
	     CL_GetDefclassListFunction, "CL_GetDefclassListFunction", NULL);
  CL_AddUDF (theEnv, "superclassp", "b", 2, 2, "y", CL_SuperclassPCommand,
	     "CL_SuperclassPCommand", NULL);
  CL_AddUDF (theEnv, "subclassp", "b", 2, 2, "y", CL_SubclassPCommand,
	     "CL_SubclassPCommand", NULL);
  CL_AddUDF (theEnv, "class-existp", "b", 1, 1, "y", CL_ClassExistPCommand,
	     "CL_ClassExistPCommand", NULL);
  CL_AddUDF (theEnv, "message-handler-existp", "b", 2, 3, "y",
	     CL_MessageHandlerExistPCommand, "CL_MessageHandlerExistPCommand",
	     NULL);
  CL_AddUDF (theEnv, "class-abstractp", "b", 1, 1, "y",
	     CL_ClassAbstractPCommand, "CL_ClassAbstractPCommand", NULL);
#if DEFRULE_CONSTRUCT
  CL_AddUDF (theEnv, "class-reactivep", "b", 1, 1, "y",
	     CL_ClassReactivePCommand, "CL_ClassReactivePCommand", NULL);
#endif
  CL_AddUDF (theEnv, "class-slots", "m", 1, 2, "y", CL_ClassSlotsCommand,
	     "CL_ClassSlotsCommand", NULL);
  CL_AddUDF (theEnv, "class-superclasses", "m", 1, 2, "y",
	     CL_ClassSuperclassesCommand, "CL_ClassSuperclassesCommand",
	     NULL);
  CL_AddUDF (theEnv, "class-subclasses", "m", 1, 2, "y",
	     CL_ClassSubclassesCommand, "CL_ClassSubclassesCommand", NULL);
  CL_AddUDF (theEnv, "get-defmessage-handler-list", "m", 0, 2, "y",
	     CL_GetDefmessageHandlersListCmd,
	     "CL_GetDefmessageHandlersListCmd", NULL);
  CL_AddUDF (theEnv, "slot-existp", "b", 2, 3, "y", CL_SlotExistPCommand,
	     "CL_SlotExistPCommand", NULL);
  CL_AddUDF (theEnv, "slot-facets", "m", 2, 2, "y", CL_SlotFacetsCommand,
	     "CL_SlotFacetsCommand", NULL);
  CL_AddUDF (theEnv, "slot-sources", "m", 2, 2, "y", CL_SlotSourcesCommand,
	     "CL_SlotSourcesCommand", NULL);
  CL_AddUDF (theEnv, "slot-types", "m", 2, 2, "y", CL_SlotTypesCommand,
	     "CL_SlotTypesCommand", NULL);
  CL_AddUDF (theEnv, "slot-allowed-values", "m", 2, 2, "y",
	     CL_SlotAllowedValuesCommand, "CL_SlotAllowedValuesCommand",
	     NULL);
  CL_AddUDF (theEnv, "slot-allowed-classes", "m", 2, 2, "y",
	     CL_SlotAllowedClassesCommand, "CL_SlotAllowedClassesCommand",
	     NULL);
  CL_AddUDF (theEnv, "slot-range", "m", 2, 2, "y", CL_SlotRangeCommand,
	     "CL_SlotRangeCommand", NULL);
  CL_AddUDF (theEnv, "slot-cardinality", "m", 2, 2, "y",
	     CL_SlotCardinalityCommand, "CL_SlotCardinalityCommand", NULL);
  CL_AddUDF (theEnv, "slot-writablep", "b", 2, 2, "y",
	     CL_SlotWritablePCommand, "CL_SlotWritablePCommand", NULL);
  CL_AddUDF (theEnv, "slot-initablep", "b", 2, 2, "y",
	     CL_SlotInitablePCommand, "CL_SlotInitablePCommand", NULL);
  CL_AddUDF (theEnv, "slot-publicp", "b", 2, 2, "y", CL_SlotPublicPCommand,
	     "CL_SlotPublicPCommand", NULL);
  CL_AddUDF (theEnv, "slot-direct-accessp", "b", 2, 2, "y",
	     CL_SlotDirectAccessPCommand, "CL_SlotDirectAccessPCommand",
	     NULL);
  CL_AddUDF (theEnv, "slot-default-value", "*", 2, 2, "y",
	     CL_SlotDefaultValueCommand, "CL_SlotDefaultValueCommand", NULL);
  CL_AddUDF (theEnv, "defclass-module", "y", 1, 1, "y",
	     Get_DefclassModuleCommand, "Get_DefclassModuleCommand", NULL);
  CL_AddUDF (theEnv, "get-class-defaults-mode", "y", 0, 0, NULL,
	     CL_GetClassDefaultsModeCommand, "CL_GetClassDefaultsModeCommand",
	     NULL);
  CL_AddUDF (theEnv, "set-class-defaults-mode", "y", 1, 1, "y",
	     CL_SetClassDefaultsModeCommand, "CL_SetClassDefaultsModeCommand",
	     NULL);
#endif

#if DEBUGGING_FUNCTIONS
  CL_Add_WatchItem (theEnv, "instances", 0,
		    &DefclassData (theEnv)->CL_Watch_Instances, 75,
		    CL_Defclass_WatchAccess, CL_Defclass_WatchPrint);
  CL_Add_WatchItem (theEnv, "slots", 1, &DefclassData (theEnv)->CL_WatchSlots,
		    74, CL_Defclass_WatchAccess, CL_Defclass_WatchPrint);
#endif
}

#if (! RUN_TIME)

/*********************************************************
  NAME         : AddSystemClass
  DESCRIPTION  : Perfo_rms all necessary allocations
                   for adding a system class
  INPUTS       : 1) The name-string of the system class
                 2) The address of the parent class
                    (NULL if none)
  RETURNS      : The address of the new system class
  SIDE EFFECTS : Allocations perfo_rmed
  NOTES        : Assumes system-class name is unique
                 Also assumes SINGLE INHERITANCE for
                   system classes to simplify precedence
                   list dete_rmination
                 Adds classes to has table but NOT to
                  class list (this is responsibility
                  of caller)
 *********************************************************/
static Defclass *
AddSystemClass (Environment * theEnv, const char *name, Defclass * parent)
{
  Defclass *sys;
  unsigned long i;
  char defaultScopeMap[1];

  sys = CL_NewClass (theEnv, CL_CreateSymbol (theEnv, name));
  sys->abstract = 1;
#if DEFRULE_CONSTRUCT
  sys->reactive = 0;
#endif
  IncrementLexemeCount (sys->header.name);
  sys->installed = 1;
  sys->system = 1;
  sys->hashTableIndex = CL_HashClass (sys->header.name);

  CL_AddClassLink (theEnv, &sys->allSuperclasses, sys, true, 0);
  if (parent != NULL)
    {
      CL_AddClassLink (theEnv, &sys->directSuperclasses, parent, true, 0);
      CL_AddClassLink (theEnv, &parent->directSubclasses, sys, true, 0);
      CL_AddClassLink (theEnv, &sys->allSuperclasses, parent, true, 0);
      for (i = 1; i < parent->allSuperclasses.classCount; i++)
	CL_AddClassLink (theEnv, &sys->allSuperclasses,
			 parent->allSuperclasses.classArray[i], true, 0);
    }
  sys->nxtHash = DefclassData (theEnv)->ClassTable[sys->hashTableIndex];
  DefclassData (theEnv)->ClassTable[sys->hashTableIndex] = sys;

  /* =========================================
     Add default scope maps for a system class
     There is only one module (MAIN) so far -
     which has an id of 0
     ========================================= */
  CL_ClearBitString (defaultScopeMap, sizeof (char));
  SetBitMap (defaultScopeMap, 0);
#if DEFMODULE_CONSTRUCT
  sys->scopeMap =
    (CLIPSBitMap *) CL_AddBitMap (theEnv, defaultScopeMap, sizeof (char));
  IncrementBitMapCount (sys->scopeMap);
#endif
  return (sys);
}

/*****************************************************
  NAME         : AllocateModule
  DESCRIPTION  : Creates and initializes a
                 list of defclasses for a new module
  INPUTS       : None
  RETURNS      : The new defclass module
  SIDE EFFECTS : Defclass module created
  NOTES        : None
 *****************************************************/
static void *
AllocateModule (Environment * theEnv)
{
  return (void *) get_struct (theEnv, defclassModule);
}

/***************************************************
  NAME         : ReturnModule
  DESCRIPTION  : Removes a defclass module and
                 all associated defclasses
  INPUTS       : The defclass module
  RETURNS      : Nothing useful
  SIDE EFFECTS : Module and defclasses deleted
  NOTES        : None
 ***************************************************/
static void
ReturnModule (Environment * theEnv, void *theItem)
{
  CL_FreeConstructHeaderModule (theEnv,
				(struct defmoduleItemHeader *) theItem,
				DefclassData (theEnv)->DefclassConstruct);
  CL_DeleteSlotName (theEnv, CL_FindIDSlotNameHash (theEnv, ISA_ID));
  CL_DeleteSlotName (theEnv, CL_FindIDSlotNameHash (theEnv, NAME_ID));
  rtn_struct (theEnv, defclassModule, theItem);
}

#endif

#if (! BLOAD_ONLY) && (! RUN_TIME) && DEFMODULE_CONSTRUCT

/***************************************************
  NAME         : UpdateDefclassesScope
  DESCRIPTION  : This function updates the scope
                 bitmaps for existing classes when
                 a new module is defined
  INPUTS       : None
  RETURNS      : Nothing
  SIDE EFFECTS : Class scope bitmaps are updated
  NOTES        : None
 ***************************************************/
static void
UpdateDefclassesScope (Environment * theEnv, void *context)
{
  unsigned i;
  Defclass *theDefclass;
  unsigned long newModuleID;
  unsigned int count;
  char *newScopeMap;
  unsigned short newScopeMapSize;
  const char *className;
  Defmodule *matchModule;

  newModuleID = CL_GetCurrentModule (theEnv)->header.bsaveID;
  newScopeMapSize =
    (sizeof (char) *
     ((CL_GetNumberOfDefmodules (theEnv) / BITS_PER_BYTE) + 1));
  newScopeMap = (char *) CL_gm2 (theEnv, newScopeMapSize);
  for (i = 0; i < CLASS_TABLE_HASH_SIZE; i++)
    for (theDefclass = DefclassData (theEnv)->ClassTable[i];
	 theDefclass != NULL; theDefclass = theDefclass->nxtHash)
      {
	matchModule = theDefclass->header.whichModule->theModule;
	className = theDefclass->header.name->contents;
	CL_ClearBitString (newScopeMap, newScopeMapSize);
	GenCopyMemory (char, theDefclass->scopeMap->size,
		       newScopeMap, theDefclass->scopeMap->contents);
	CL_DecrementBitMapReferenceCount (theEnv, theDefclass->scopeMap);
	if (theDefclass->system)
	  SetBitMap (newScopeMap, newModuleID);
	else if (CL_FindImportedConstruct (theEnv, "defclass", matchModule,
					   className, &count, true,
					   NULL) != NULL)
	  SetBitMap (newScopeMap, newModuleID);
	theDefclass->scopeMap =
	  (CLIPSBitMap *) CL_AddBitMap (theEnv, newScopeMap, newScopeMapSize);
	IncrementBitMapCount (theDefclass->scopeMap);
      }
  CL_rm (theEnv, newScopeMap, newScopeMapSize);
}

#endif

#endif
