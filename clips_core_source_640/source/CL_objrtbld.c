   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  02/09/18             */
   /*                                                     */
   /*          OBJECT PATTERN MATCHER MODULE              */
   /*******************************************************/

/*************************************************************/
/* Purpose: RETE Network Parsing Interface for Objects       */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed INCREMENTAL_RESET compilation flag.    */
/*                                                           */
/*            Converted INSTANCE_PATTERN_MATCHING to         */
/*            DEFRULE_CONSTRUCT.                             */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Added support for hashed comparisons to        */
/*            constants.                                     */
/*                                                           */
/*            Added support for hashed alpha memories.       */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.31: Added class name to OBJRTBLD5 error message.   */
/*                                                           */
/*            Optimization for marking relevant alpha nodes  */
/*            in the object pattern network.                 */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Static constraint checking is always enabled.  */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            Removed initial-object support.                */
/*                                                           */
/*************************************************************/
/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM

#if (! BLOAD_ONLY) && (! RUN_TIME)

#include <string.h>
#include <stdlib.h>

#include "classcom.h"
#include "classfun.h"
#include "cstrnutl.h"
#include "constrnt.h"
#include "cstrnchk.h"
#include "cstrnops.h"
#include "drive.h"
#include "envrnmnt.h"
#include "inscom.h"
#include "insfun.h"
#include "insmngr.h"
#include "memalloc.h"
#include "network.h"
#include "object.h"
#include "pattern.h"
#include "prntutil.h"
#include "reteutil.h"
#include "ruledef.h"
#include "rulepsr.h"
#include "scanner.h"
#include "symbol.h"
#include "utility.h"

#endif

#include "constrct.h"
#include "exprnpsr.h"
#include "objrtmch.h"
#include "objrtgen.h"
#include "objrtfnx.h"
#include "pprint.h"
#include "reorder.h"
#include "router.h"

#if BLOAD_AND_BSAVE || BLOAD || BLOAD_ONLY
#include "objrtbin.h"
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
#include "objrtcmp.h"
#endif

#include "objrtbld.h"

#if ! DEFINSTANCES_CONSTRUCT
#include "classfun.h"
#include "classcom.h"
#include "extnfunc.h"
#endif

#if (! BLOAD_ONLY) && (! RUN_TIME)

/* =========================================
   *****************************************
                   CONSTANTS
   =========================================
   ***************************************** */
#define OBJECT_PATTERN_INDICATOR "object"

/* =========================================
   *****************************************
      INTERNALLY VISIBLE FUNCTION HEADERS
   =========================================
   ***************************************** */

static bool PatternParserFind (CLIPSLexeme *);
static struct lhsParseNode *ObjectLHSParse (Environment *, const char *,
					    struct token *);
static bool ReorderAndAnalyzeObjectPattern (Environment *,
					    struct lhsParseNode *);
static struct patternNodeHeader *PlaceObjectPattern (Environment *,
						     struct lhsParseNode *);
static OBJECT_PATTERN_NODE *FindObjectPatternNode (OBJECT_PATTERN_NODE *,
						   struct lhsParseNode *,
						   OBJECT_PATTERN_NODE **,
						   bool, bool);
static OBJECT_PATTERN_NODE *CreateNewObjectPatternNode (Environment *,
							struct lhsParseNode *,
							OBJECT_PATTERN_NODE *,
							OBJECT_PATTERN_NODE *,
							bool, bool);
static void DetachObjectPattern (Environment *, struct patternNodeHeader *);
static void CL_ClearObjectPattern_Matches (Environment *,
					   OBJECT_ALPHA_NODE *);
static void RemoveObjectPartial_Matches (Environment *, Instance *,
					 struct patternNodeHeader *);
static bool CheckDuplicateSlots (Environment *, struct lhsParseNode *,
				 CLIPSLexeme *);
static struct lhsParseNode *ParseClassRestriction (Environment *,
						   const char *,
						   struct token *);
static struct lhsParseNode *ParseNameRestriction (Environment *, const char *,
						  struct token *);
static struct lhsParseNode *CL_ParseSlotRestriction (Environment *,
						     const char *,
						     struct token *,
						     CONSTRAINT_RECORD *,
						     bool);
static CLASS_BITMAP *CL_NewClassBitMap (Environment *, unsigned short, bool);
static void InitializeClassBitMap (Environment *, CLASS_BITMAP *, bool);
static void DeleteInte_rmediateClassBitMap (Environment *, CLASS_BITMAP *);
static void *CopyClassBitMap (Environment *, void *);
static void DeleteClassBitMap (Environment *, void *);
static void MarkBitMapClassesBusy (Environment *, CLIPSBitMap *, int);
static bool EmptyClassBitMap (CLASS_BITMAP *);
static bool IdenticalClassBitMap (CLASS_BITMAP *, CLASS_BITMAP *);
static bool ProcessClassRestriction (Environment *, CLASS_BITMAP *,
				     struct lhsParseNode **, bool);
static CONSTRAINT_RECORD *ProcessSlotRestriction (Environment *,
						  CLASS_BITMAP *,
						  CLIPSLexeme *, bool *);
static void IntersectClassBitMaps (CLASS_BITMAP *, CLASS_BITMAP *);
static void UnionClassBitMaps (CLASS_BITMAP *, CLASS_BITMAP *);
static CLASS_BITMAP *PackClassBitMap (Environment *, CLASS_BITMAP *);
static struct lhsParseNode *FilterObjectPattern (Environment *,
						 struct patternParser *,
						 struct lhsParseNode *,
						 struct lhsParseNode **,
						 struct lhsParseNode **,
						 struct lhsParseNode **);
static CLIPSBitMap *Fo_rmSlotBitMap (Environment *, struct lhsParseNode *);
static struct lhsParseNode *RemoveSlotExistenceTests (Environment *,
						      struct lhsParseNode *,
						      CLIPSBitMap **);
static void MarkObjectPtnCL_Incremental_Reset (Environment *,
					       struct patternNodeHeader *,
					       bool);
static void ObjectCL_Incremental_Reset (Environment *);

#endif

static Expression *CL_ObjectMatchDelayParse (Environment *, Expression *,
					     const char *);

#if ! DEFINSTANCES_CONSTRUCT
static void CL_ResetInitialObject (Environment *);
#endif

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/********************************************************
  NAME         : CL_SetupObjectPatternStuff
  DESCRIPTION  : Installs the parsers and other items
                 necessary for recognizing and processing
                 object patterns in defrules
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Rete network interfaces for objects
                 initialized
  NOTES        : None
 ********************************************************/
void
CL_SetupObjectPatternStuff (Environment * theEnv)
{
#if (! BLOAD_ONLY) && (! RUN_TIME)
  struct patternParser *newPtr;

  if (CL_ReservedPatternSymbol (theEnv, "object", NULL) == true)
    {
      CL_SystemError (theEnv, "OBJRTBLD", 1);
      CL_ExitRouter (theEnv, EXIT_FAILURE);
    }
  CL_Add_ReservedPatternSymbol (theEnv, "object", NULL);

  /* ===========================================================================
     The object pattern parser needs to have a higher priority than deftemplates
     or regular facts so that the "object" keyword is always recognized first
     =========================================================================== */

  newPtr = get_struct (theEnv, patternParser);

  newPtr->name = "objects";
  newPtr->priority = 20;
  newPtr->entityType = &InstanceData (theEnv)->InstanceInfo;

  newPtr->recognizeFunction = PatternParserFind;
  newPtr->parseFunction = ObjectLHSParse;
  newPtr->postAnalysisFunction = ReorderAndAnalyzeObjectPattern;
  newPtr->addPatternFunction = PlaceObjectPattern;
  newPtr->removePatternFunction = DetachObjectPattern;
  newPtr->genJNConstantFunction = NULL;
  newPtr->replaceGetJNValueFunction = CL_ReplaceGetJNObjectValue;
  newPtr->genGetJNValueFunction = CL_GenGetJNObjectValue;
  newPtr->genCompareJNValuesFunction = CL_ObjectJNVariableComparison;
  newPtr->genPNConstantFunction = CL_GenObjectPNConstantCompare;
  newPtr->replaceGetPNValueFunction = CL_ReplaceGetPNObjectValue;
  newPtr->genGetPNValueFunction = CL_GenGetPNObjectValue;
  newPtr->genComparePNValuesFunction = CL_ObjectPNVariableComparison;
  newPtr->returnUserDataFunction = DeleteClassBitMap;
  newPtr->copyUserDataFunction = CopyClassBitMap;

  newPtr->markIRPatternFunction = MarkObjectPtnCL_Incremental_Reset;
  newPtr->incremental_ResetFunction = ObjectCL_Incremental_Reset;

#if CONSTRUCT_COMPILER && (! RUN_TIME)
  newPtr->codeReferenceFunction = CL_ObjectPatternNodeReference;
#else
  newPtr->codeReferenceFunction = NULL;
#endif

  CL_AddPatternParser (theEnv, newPtr);

#endif

#if (! RUN_TIME)
  CL_AddUDF (theEnv, "object-pattern-match-delay", "*", 0, UNBOUNDED, NULL,
	     CL_ObjectMatchDelay, "CL_ObjectMatchDelay", NULL);
  CL_FuncSeqOvlFlags (theEnv, "object-pattern-match-delay", false, false);
#endif

  CL_AddFunctionParser (theEnv, "object-pattern-match-delay",
			CL_ObjectMatchDelayParse);

  CL_InstallObjectPrimitives (theEnv);

#if CONSTRUCT_COMPILER && (! RUN_TIME)
  CL_ObjectPatternsCompilerSetup (theEnv);
#endif

#if BLOAD_AND_BSAVE || BLOAD || BLOAD_ONLY
  SetupObjectPatterns_Bload (theEnv);
#endif
}

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if (! BLOAD_ONLY) && (! RUN_TIME)

/*****************************************************
  NAME         : PatternParserFind
  DESCRIPTION  : Dete_rmines if a pattern CE is an
                 object pattern (i.e. the first field
                 is the constant symbol "object")
  INPUTS       : 1) The type of the first field
                 2) The value of the first field
  RETURNS      : True if it is an object pattern,
                 false otherwise
  SIDE EFFECTS : None
  NOTES        : Used by CL_AddPatternParser()
 *****************************************************/
static bool
PatternParserFind (CLIPSLexeme * value)
{
  if (strcmp (value->contents, OBJECT_PATTERN_INDICATOR) == 0)
    return true;

  return false;
}

/************************************************************************************
  NAME         : ObjectLHSParse
  DESCRIPTION  : Scans and parses an object pattern for a rule
  INPUTS       : 1) The logical name of the input source
                 2) A buffer holding the last token read
  RETURNS      : The address of struct lhsParseNodes, NULL on errors
  SIDE EFFECTS : A series of struct lhsParseNodes are created to represent
                 the inte_rmediate parse of the pattern
                 Pretty-print fo_rm for the pattern is saved
  NOTES        : Object Pattern Syntax:
                 (object [<class-constraint>] [<name-constraint>] <slot-constraint>*)
                 <class-constraint> ::= (is-a <constraint>)
                 <name-constraint> ::= (name <constraint>)
                 <slot-constraint> ::= (<slot-name> <constraint>*)
 ************************************************************************************/
static struct lhsParseNode *
ObjectLHSParse (Environment * theEnv,
		const char *readSource, struct token *lastToken)
{
#if MAC_XCD
#pragma unused(lastToken)
#endif
  struct token theToken;
  struct lhsParseNode *firstNode = NULL, *lastNode = NULL, *tmpNode;
  CLASS_BITMAP *clsset, *tmpset;
  CONSTRAINT_RECORD *slotConstraints;
  bool ppbackupReqd = false, multip;

  /* ========================================================
     Get a bitmap big enough to mark the ids of all currently
     existing classes - and set all bits, since the initial
     set of applicable classes is everything.
     ======================================================== */
  clsset =
    CL_NewClassBitMap (theEnv, DefclassData (theEnv)->MaxClassID - 1, true);
  if (EmptyClassBitMap (clsset))
    {
      CL_PrintErrorID (theEnv, "OBJRTBLD", 1, false);
      CL_WriteString (theEnv, STDERR,
		      "No objects of existing classes can satisfy pattern.\n");
      DeleteInte_rmediateClassBitMap (theEnv, clsset);
      return NULL;
    }
  tmpset =
    CL_NewClassBitMap (theEnv, DefclassData (theEnv)->MaxClassID - 1, true);

  CL_IncrementIndentDepth (theEnv, 7);

  /* ===========================================
     Parse the class, name and slot restrictions
     =========================================== */
  CL_GetToken (theEnv, readSource, &theToken);
  while (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      ppbackupReqd = true;
      CL_PPBackup (theEnv);
      CL_SavePPBuffer (theEnv, " ");
      CL_SavePPBuffer (theEnv, theToken.printFo_rm);
      if (theToken.tknType != LEFT_PARENTHESIS_TOKEN)
	{
	  CL_SyntaxErrorMessage (theEnv, "object pattern");
	  goto ObjectLHSParseERROR;
	}
      CL_GetToken (theEnv, readSource, &theToken);
      if (theToken.tknType != SYMBOL_TOKEN)
	{
	  CL_SyntaxErrorMessage (theEnv, "object pattern");
	  goto ObjectLHSParseERROR;
	}
      if (CheckDuplicateSlots (theEnv, firstNode, theToken.lexemeValue))
	goto ObjectLHSParseERROR;
      if (theToken.value == (void *) DefclassData (theEnv)->ISA_SYMBOL)
	{
	  tmpNode = ParseClassRestriction (theEnv, readSource, &theToken);
	  if (tmpNode == NULL)
	    goto ObjectLHSParseERROR;
	  InitializeClassBitMap (theEnv, tmpset, false);
	  if (ProcessClassRestriction (theEnv, tmpset, &tmpNode->bottom, true)
	      == false)
	    {
	      CL_ReturnLHSParseNodes (theEnv, tmpNode);
	      goto ObjectLHSParseERROR;
	    }
	  IntersectClassBitMaps (clsset, tmpset);
	}
      else if (theToken.value == (void *) DefclassData (theEnv)->NAME_SYMBOL)
	{
	  tmpNode = ParseNameRestriction (theEnv, readSource, &theToken);
	  if (tmpNode == NULL)
	    goto ObjectLHSParseERROR;
	  InitializeClassBitMap (theEnv, tmpset, true);
	}
      else
	{
	  slotConstraints =
	    ProcessSlotRestriction (theEnv, clsset, theToken.lexemeValue,
				    &multip);
	  if (slotConstraints != NULL)
	    {
	      InitializeClassBitMap (theEnv, tmpset, true);
	      tmpNode =
		CL_ParseSlotRestriction (theEnv, readSource, &theToken,
					 slotConstraints, multip);
	      if (tmpNode == NULL)
		goto ObjectLHSParseERROR;
	    }
	  else
	    {
	      InitializeClassBitMap (theEnv, tmpset, false);
	      tmpNode = CL_GetLHSParseNode (theEnv);
	      tmpNode->slot = theToken.lexemeValue;
	    }
	}
      if (EmptyClassBitMap (tmpset))
	{
	  CL_PrintErrorID (theEnv, "OBJRTBLD", 2, false);
	  CL_WriteString (theEnv, STDERR,
			  "No objects of existing classes can satisfy '");
	  CL_WriteString (theEnv, STDERR, tmpNode->slot->contents);
	  CL_WriteString (theEnv, STDERR,
			  "' restriction in object pattern.\n");
	  CL_ReturnLHSParseNodes (theEnv, tmpNode);
	  goto ObjectLHSParseERROR;
	}
      if (EmptyClassBitMap (clsset))
	{
	  CL_PrintErrorID (theEnv, "OBJRTBLD", 1, false);
	  CL_WriteString (theEnv, STDERR,
			  "No objects of existing classes can satisfy pattern.\n");
	  CL_ReturnLHSParseNodes (theEnv, tmpNode);
	  goto ObjectLHSParseERROR;
	}
      if (tmpNode != NULL)
	{
	  if (firstNode == NULL)
	    firstNode = tmpNode;
	  else
	    lastNode->right = tmpNode;
	  lastNode = tmpNode;
	}
      CL_PPCRAndIndent (theEnv);
      CL_GetToken (theEnv, readSource, &theToken);
    }
  if (firstNode == NULL)
    {
      if (EmptyClassBitMap (clsset))
	{
	  CL_PrintErrorID (theEnv, "OBJRTBLD", 1, false);
	  CL_WriteString (theEnv, STDERR,
			  "No objects of existing classes can satisfy pattern.\n");
	  goto ObjectLHSParseERROR;
	}
      firstNode = CL_GetLHSParseNode (theEnv);
      firstNode->pnType = SF_WILDCARD_NODE;
      firstNode->slot = DefclassData (theEnv)->ISA_SYMBOL;
      firstNode->slotNumber = ISA_ID;
      firstNode->index = 1;
    }
  if (ppbackupReqd)
    {
      CL_PPBackup (theEnv);
      CL_PPBackup (theEnv);
      CL_SavePPBuffer (theEnv, theToken.printFo_rm);
    }
  DeleteInte_rmediateClassBitMap (theEnv, tmpset);
  clsset = PackClassBitMap (theEnv, clsset);
  firstNode->userData =
    CL_AddBitMap (theEnv, clsset, ClassBitMapSize (clsset));
  IncrementBitMapCount (firstNode->userData);
  DeleteInte_rmediateClassBitMap (theEnv, clsset);
  CL_DecrementIndentDepth (theEnv, 7);
  return (firstNode);

ObjectLHSParseERROR:
  DeleteInte_rmediateClassBitMap (theEnv, clsset);
  DeleteInte_rmediateClassBitMap (theEnv, tmpset);
  CL_ReturnLHSParseNodes (theEnv, firstNode);
  CL_DecrementIndentDepth (theEnv, 7);
  return NULL;
}

/**************************************************************
  NAME         : ReorderAndAnalyzeObjectPattern
  DESCRIPTION  : This function reexamines the object pattern
                 after constraint and variable analysis info
                 has been propagated from other patterns.
                   Any slots which are no longer applicable
                 to the pattern are eliminated from the
                 class set.
                   Also, the slot names are ordered according
                 to lexical value to aid in detete_rming
                 sharing between object patterns.  (The is-a
                 and name restrictions are always placed
                 first regardless of symbolic hash value.)
  INPUTS       : The pattern CE lhsParseNode
  RETURNS      : False if all OK, otherwise true
                 (e.g. all classes are eliminated as potential
                  matching candidates for the pattern)
  SIDE EFFECTS : Slot restrictions are reordered (if necessary)
  NOTES        : Adds a default is-a slot if one does not
                 already exist
 **************************************************************/
static bool
ReorderAndAnalyzeObjectPattern (Environment * theEnv,
				struct lhsParseNode *topNode)
{
  CLASS_BITMAP *clsset, *tmpset;
  Expression *rexp, *tmpmin, *tmpmax;
  Defclass *cls;
  struct lhsParseNode *tmpNode, *subNode, *bitmap_node, *isa_node, *name_node;
  unsigned short i;
  SlotDescriptor *sd;
  CONSTRAINT_RECORD *crossConstraints, *theConstraint;
  int incompatibleConstraint;
  bool clssetChanged = false;

  /* ==========================================================
     Make sure that the bitmap marking which classes of object
     can match the pattern is attached to the class restriction
     (which will always be present and the last restriction
     after the sort)
     ========================================================== */
  topNode->right =
    FilterObjectPattern (theEnv, topNode->patternType, topNode->right,
			 &bitmap_node, &isa_node, &name_node);

  /* ============================================
     Allocate a temporary set for marking classes
     ============================================ */
  clsset = (CLASS_BITMAP *) ((CLIPSBitMap *) bitmap_node->userData)->contents;
  tmpset = CL_NewClassBitMap (theEnv, clsset->maxid, false);

  /* ==========================================================
     Check the allowed-values for the constraint on the is-a
     slot.  If there are any, make sure that only the classes
     with those values as names are marked in the bitmap.

     There will only be symbols in the list because the
     original constraint on the is-a slot allowed only symbols.
     ========================================================== */
  if ((isa_node == NULL) ? false :
      ((isa_node->constraints == NULL) ? false :
       (isa_node->constraints->restrictionList != NULL)))
    {
      rexp = isa_node->constraints->restrictionList;
      while (rexp != NULL)
	{
	  cls = Lookup_DefclassInScope (theEnv, rexp->lexemeValue->contents);
	  if (cls != NULL)
	    {
	      if ((cls->id <= clsset->maxid) ? TestBitMap (clsset->map,
							   cls->id) : false)
		SetBitMap (tmpset->map, cls->id);
	    }
	  rexp = rexp->nextArg;
	}
      clssetChanged = IdenticalClassBitMap (tmpset, clsset) ? false : true;
    }
  else
    GenCopyMemory (char, tmpset->maxid / BITS_PER_BYTE + 1, tmpset->map,
		   clsset->map);

  /* ================================================================
     For each of the slots (excluding name and is-a), check the total
     constraints for the slot against the individual constraints
     for each occurrence of the slot in the classes marked in
     the bitmap.  For any slot which is not compatible with
     the overall constraint, clear its class's bit in the bitmap.
     ================================================================ */
  tmpNode = topNode->right;
  while (tmpNode != bitmap_node)
    {
      if ((tmpNode == isa_node) || (tmpNode == name_node))
	{
	  tmpNode = tmpNode->right;
	  continue;
	}
      for (i = 0; i <= tmpset->maxid; i++)
	if (TestBitMap (tmpset->map, i))
	  {
	    cls = DefclassData (theEnv)->ClassIDMap[i];
	    sd =
	      cls->instanceTemplate[CL_FindInstanceTemplateSlot
				    (theEnv, cls, tmpNode->slot)];

	    /* =========================================
	       Check the top-level lhsParseNode for type
	       and cardinality compatibility
	       ========================================= */
	    crossConstraints =
	      CL_IntersectConstraints (theEnv, tmpNode->constraints,
				       sd->constraint);
	    incompatibleConstraint =
	      CL_UnmatchableConstraint (crossConstraints);
	    CL_RemoveConstraint (theEnv, crossConstraints);
	    if (incompatibleConstraint)
	      {
		CL_ClearBitMap (tmpset->map, i);
		clssetChanged = true;
	      }
	    else if (tmpNode->pnType == MF_WILDCARD_NODE)
	      {
		/* ==========================================
		   Check the sub-nodes for type compatibility
		   ========================================== */
		for (subNode = tmpNode->bottom; subNode != NULL;
		     subNode = subNode->right)
		  {
		    /* ========================================================
		       Temporarily reset cardinality of variables to
		       match slot so that no cardinality errors will be flagged
		       ======================================================== */
		    if ((subNode->pnType == MF_WILDCARD_NODE)
			|| (subNode->pnType == MF_VARIABLE_NODE))
		      {
			theConstraint = subNode->constraints->multifield;
		      }
		    else
		      {
			theConstraint = subNode->constraints;
		      }

		    tmpmin = theConstraint->minFields;
		    theConstraint->minFields = sd->constraint->minFields;
		    tmpmax = theConstraint->maxFields;
		    theConstraint->maxFields = sd->constraint->maxFields;
		    crossConstraints =
		      CL_IntersectConstraints (theEnv, theConstraint,
					       sd->constraint);
		    theConstraint->minFields = tmpmin;
		    theConstraint->maxFields = tmpmax;

		    incompatibleConstraint =
		      CL_UnmatchableConstraint (crossConstraints);
		    CL_RemoveConstraint (theEnv, crossConstraints);
		    if (incompatibleConstraint)
		      {
			CL_ClearBitMap (tmpset->map, i);
			clssetChanged = true;
			break;
		      }
		  }
	      }
	  }
      tmpNode = tmpNode->right;
    }

  if (clssetChanged)
    {
      /* =======================================================
         Make sure that there are still classes of objects which
         can satisfy this pattern.  Otherwise, signal an error.
         ======================================================= */
      if (EmptyClassBitMap (tmpset))
	{
	  CL_PrintErrorID (theEnv, "OBJRTBLD", 3, true);
	  DeleteInte_rmediateClassBitMap (theEnv, tmpset);
	  CL_WriteString (theEnv, STDERR,
			  "No objects of existing classes can satisfy pattern #");
	  CL_WriteInteger (theEnv, STDERR, topNode->pattern);
	  CL_WriteString (theEnv, STDERR, ".\n");
	  return true;
	}
      clsset = PackClassBitMap (theEnv, tmpset);
      DeleteClassBitMap (theEnv, bitmap_node->userData);
      bitmap_node->userData =
	CL_AddBitMap (theEnv, clsset, ClassBitMapSize (clsset));
      IncrementBitMapCount (bitmap_node->userData);
      DeleteInte_rmediateClassBitMap (theEnv, clsset);
    }
  else
    DeleteInte_rmediateClassBitMap (theEnv, tmpset);
  return false;
}

/*****************************************************
  NAME         : PlaceObjectPattern
  DESCRIPTION  : Integrates an object pattern into the
                 object pattern network
  INPUTS       : The inte_rmediate parse representation
                 of the pattern
  RETURNS      : The address of the new pattern
  SIDE EFFECTS : Object pattern network updated
  NOTES        : None
 *****************************************************/
static struct patternNodeHeader *
PlaceObjectPattern (Environment * theEnv, struct lhsParseNode *thePattern)
{
  OBJECT_PATTERN_NODE *currentLevel, *lastLevel;
  struct lhsParseNode *tempPattern = NULL;
  OBJECT_PATTERN_NODE *nodeSlotGroup, *newNode;
  OBJECT_ALPHA_NODE *newAlphaNode;
  bool endSlot;
  CLIPSBitMap *newClassBitMap, *newSlotBitMap;
  struct expr *rightHash;
  unsigned int i;
  CLASS_BITMAP *cbmp;
  Defclass *relevantDefclass;
  CLASS_ALPHA_LINK *newAlphaLink;

   /*========================================================*/
  /* Get the top of the object pattern network and prepare  */
  /* for the traversal to look for shareable pattern nodes. */
   /*========================================================*/

  currentLevel = CL_ObjectNetworkPointer (theEnv);
  lastLevel = NULL;

   /*====================================================*/
  /* Remove slot existence tests from the pattern since */
  /* these are accounted for by the class bitmap and    */
  /* find the class and slot bitmaps.                   */
   /*====================================================*/

  rightHash = thePattern->rightHash;

  newSlotBitMap = Fo_rmSlotBitMap (theEnv, thePattern->right);
  thePattern->right =
    RemoveSlotExistenceTests (theEnv, thePattern->right, &newClassBitMap);
  thePattern = thePattern->right;

   /*=========================================================*/
  /* Loop until all fields in the pattern have been added to */
  /* the pattern network. Process the bitmap node ONLY if it */
  /* is the only node in the pattern.                        */
   /*=========================================================*/

  do
    {
      if (thePattern->multifieldSlot)
	{
	  tempPattern = thePattern;
	  thePattern = thePattern->bottom;
	}

      /*============================================*/
      /* Dete_rmine if the last pattern field within */
      /* a multifield slot is being processed.      */
      /*============================================*/

      if (((thePattern->pnType == MF_WILDCARD_NODE) ||
	   (thePattern->pnType == MF_VARIABLE_NODE)) &&
	  (thePattern->right == NULL) && (tempPattern != NULL))
	{
	  endSlot = true;
	}
      else
	{
	  endSlot = false;
	}

      /*========================================*/
      /* Is there a node in the pattern network */
      /* that can be reused (shared)?           */
      /*========================================*/

      newNode =
	FindObjectPatternNode (currentLevel, thePattern, &nodeSlotGroup,
			       endSlot, false);

      /*================================================*/
      /* If the pattern node cannot be shared, then add */
      /* a new pattern node to the pattern network.     */
      /*================================================*/

      if (newNode == NULL)
	{
	  newNode =
	    CreateNewObjectPatternNode (theEnv, thePattern, nodeSlotGroup,
					lastLevel, endSlot, false);
	}

      if (thePattern->constantSelector != NULL)
	{
	  currentLevel = newNode->nextLevel;
	  lastLevel = newNode;
	  newNode =
	    FindObjectPatternNode (currentLevel, thePattern, &nodeSlotGroup,
				   endSlot, true);

	  if (newNode == NULL)
	    {
	      newNode =
		CreateNewObjectPatternNode (theEnv, thePattern, nodeSlotGroup,
					    lastLevel, endSlot, true);
	    }
	}

      /*=======================================================*/
      /* Move on to the next field in the pattern to be added. */
      /*=======================================================*/

      if ((thePattern->right == NULL) && (tempPattern != NULL))
	{
	  thePattern = tempPattern;
	  tempPattern = NULL;
	}

      lastLevel = newNode;
      currentLevel = newNode->nextLevel;
      thePattern = thePattern->right;
    }
  while ((thePattern != NULL) ? (thePattern->userData == NULL) : false);

   /*==================================================*/
  /* Return the leaf node of the newly added pattern. */
   /*==================================================*/

  newAlphaNode = lastLevel->alphaNode;
  while (newAlphaNode != NULL)
    {
      if ((newClassBitMap == newAlphaNode->classbmp) &&
	  (newSlotBitMap == newAlphaNode->slotbmp) &&
	  CL_IdenticalExpression (newAlphaNode->header.rightHash, rightHash))
	return ((struct patternNodeHeader *) newAlphaNode);
      newAlphaNode = newAlphaNode->nxtInGroup;
    }

  newAlphaNode = get_struct (theEnv, objectAlphaNode);
  CL_InitializePatternHeader (theEnv, &newAlphaNode->header);
  newAlphaNode->header.rightHash = CL_AddHashedExpression (theEnv, rightHash);
  newAlphaNode->matchTimeTag = 0L;
  newAlphaNode->patternNode = lastLevel;
  newAlphaNode->classbmp = newClassBitMap;
  IncrementBitMapCount (newClassBitMap);
  MarkBitMapClassesBusy (theEnv, newClassBitMap, 1);
  newAlphaNode->slotbmp = newSlotBitMap;
  if (newSlotBitMap != NULL)
    IncrementBitMapCount (newSlotBitMap);
  newAlphaNode->bsaveID = 0L;
  newAlphaNode->nxtInGroup = lastLevel->alphaNode;
  lastLevel->alphaNode = newAlphaNode;
  newAlphaNode->nxtTe_rminal = CL_ObjectNetworkTe_rminalPointer (theEnv);
  SetCL_ObjectNetworkTe_rminalPointer (theEnv, newAlphaNode);

  /*
   * A new te_rminal alpha node has just been created. For each defclass
   * relevant to this alpha node, add it to that defclass'
   * relevant_te_rminal_alpha_nodes list
   */
  cbmp = (CLASS_BITMAP *) newClassBitMap->contents;
  for (i = 0; i <= cbmp->maxid; i++)
    {
      if (TestBitMap (cbmp->map, i))
	{
	  relevantDefclass = DefclassData (theEnv)->ClassIDMap[i];
	  newAlphaLink = get_struct (theEnv, classAlphaLink);
	  newAlphaLink->alphaNode = newAlphaNode;
	  newAlphaLink->next =
	    relevantDefclass->relevant_te_rminal_alpha_nodes;
	  relevantDefclass->relevant_te_rminal_alpha_nodes = newAlphaLink;
	}
    }

  return ((struct patternNodeHeader *) newAlphaNode);
}

/************************************************************************
  NAME         : FindObjectPatternNode
  DESCRIPTION  : Looks for a pattern node at a specified
                 level in the pattern network that can be reused (shared)
                 with a pattern field being added to the pattern network.
  INPUTS       : 1) The current layer of nodes being examined in the
                    object pattern network
                 2) The inte_rmediate parse representation of the pattern
                    being added
                 3) A buffer for holding the first node of a group
                    of slots with the same name as the new node
                 4) An integer code indicating if this is the last
                    fiedl in a slot pattern or not
  RETURNS      : The old pattern network node matching the new node, or
                 NULL if there is none (nodeSlotGroup will hold the
                 place where to attach a new node)
  SIDE EFFECTS : nodeSlotGroup set
  NOTES        : None
 ************************************************************************/
static OBJECT_PATTERN_NODE *
FindObjectPatternNode (OBJECT_PATTERN_NODE * listOfNodes,
		       struct lhsParseNode *thePattern,
		       OBJECT_PATTERN_NODE ** nodeSlotGroup,
		       bool endSlot, bool constantSelector)
{
  struct expr *compareTest;
  *nodeSlotGroup = NULL;

  if (constantSelector)
    {
      compareTest = thePattern->constantValue;
    }
  else if (thePattern->constantSelector != NULL)
    {
      compareTest = thePattern->constantSelector;
    }
  else
    {
      compareTest = thePattern->networkTest;
    }

   /*==========================================================*/
  /* Loop through the nodes at the given level in the pattern */
  /* network looking for a node that can be reused (shared).  */
   /*==========================================================*/

  while (listOfNodes != NULL)
    {
      /*=========================================================*/
      /* A object pattern node can be shared if the slot name is */
      /* the same, the test is on the same field in the pattern, */
      /* and the network test expressions are the same.          */
      /*=========================================================*/

      if (((thePattern->pnType == MF_WILDCARD_NODE)
	   || (thePattern->pnType ==
	       MF_VARIABLE_NODE)) ? listOfNodes->multifieldNode
	  : (listOfNodes->multifieldNode == 0))
	{
	  if ((thePattern->slotNumber == listOfNodes->slotNameID) &&
	      (thePattern->index == listOfNodes->whichField) &&
	      (thePattern->singleFieldsAfter == listOfNodes->leaveFields) &&
	      (endSlot == listOfNodes->endSlot) &&
	      CL_IdenticalExpression (listOfNodes->networkTest, compareTest))
	    return (listOfNodes);
	}

      /*===============================================*/
      /* Find the beginning of a group of nodes with   */
      /* the same slot name testing on the same field. */
      /*===============================================*/

      if ((*nodeSlotGroup == NULL) &&
	  (thePattern->index == listOfNodes->whichField) &&
	  (thePattern->slotNumber == listOfNodes->slotNameID))
	*nodeSlotGroup = listOfNodes;
      listOfNodes = listOfNodes->rightNode;
    }

   /*==============================================*/
  /* A shareable pattern node could not be found. */
   /*==============================================*/

  return NULL;
}

/*****************************************************************
  NAME         : CreateNewObjectPatternNode
  DESCRIPTION  : Creates a new pattern node and initializes
                   all of its values.
  INPUTS       : 1) The inte_rmediate parse representation
                    of the new pattern node
                 2) A pointer to the network node after
                    which to add the new node
                 3) A pointer to the parent node on the
                    level above to link the new node
                 4) An integer code indicating if this is the last
                    fiedl in a slot pattern or not
  RETURNS      : A pointer to the new pattern node
  SIDE EFFECTS : Pattern node allocated, initialized and
                   attached
  NOTES        : None
 *****************************************************************/
static OBJECT_PATTERN_NODE *
CreateNewObjectPatternNode (Environment * theEnv,
			    struct lhsParseNode *thePattern,
			    OBJECT_PATTERN_NODE * nodeSlotGroup,
			    OBJECT_PATTERN_NODE * upperLevel,
			    bool endSlot, bool constantSelector)
{
  OBJECT_PATTERN_NODE *newNode, *prvNode, *curNode;

  newNode = get_struct (theEnv, objectPatternNode);
  newNode->blocked = false;
  newNode->multifieldNode = false;
  newNode->alphaNode = NULL;
  newNode->matchTimeTag = 0L;
  newNode->nextLevel = NULL;
  newNode->rightNode = NULL;
  newNode->leftNode = NULL;
  newNode->bsaveID = 0L;

  if ((thePattern->constantSelector != NULL) && (!constantSelector))
    {
      newNode->selector = true;
    }
  else
    {
      newNode->selector = false;
    }

   /*===========================================================*/
  /* Install the expression associated with this pattern node. */
   /*===========================================================*/

  if (constantSelector)
    {
      newNode->networkTest =
	CL_AddHashedExpression (theEnv, thePattern->constantValue);
    }
  else if (thePattern->constantSelector != NULL)
    {
      newNode->networkTest =
	CL_AddHashedExpression (theEnv, thePattern->constantSelector);
    }
  else
    {
      newNode->networkTest =
	CL_AddHashedExpression (theEnv, thePattern->networkTest);
    }

  newNode->whichField = thePattern->index;
  newNode->leaveFields = thePattern->singleFieldsAfter;

   /*=========================================*/
  /* Install the slot name for the new node. */
   /*=========================================*/

  newNode->slotNameID = thePattern->slotNumber;
  if ((thePattern->pnType == MF_WILDCARD_NODE)
      || (thePattern->pnType == MF_VARIABLE_NODE))
    newNode->multifieldNode = true;
  newNode->endSlot = endSlot;

   /*===============================================*/
  /* Set the upper level pointer for the new node. */
   /*===============================================*/

  newNode->lastLevel = upperLevel;

  if ((upperLevel != NULL) && (upperLevel->selector))
    {
      CL_AddHashedPatternNode (theEnv, upperLevel, newNode,
			       newNode->networkTest->type,
			       newNode->networkTest->value);
    }

   /*==============================================*/
  /* If there are no nodes with this slot name on */
  /* this level, simply prepend it to the front.  */
   /*==============================================*/

  if (nodeSlotGroup == NULL)
    {
      if (upperLevel == NULL)
	{
	  newNode->rightNode = CL_ObjectNetworkPointer (theEnv);
	  Set_ObjectNetworkPointer (theEnv, newNode);
	}
      else
	{
	  newNode->rightNode = upperLevel->nextLevel;
	  upperLevel->nextLevel = newNode;
	}
      if (newNode->rightNode != NULL)
	newNode->rightNode->leftNode = newNode;
      return (newNode);
    }

  /* ===========================================================
     Group this node with other nodes of the same name
     testing on the same field in the pattern
     on this level.  This allows us to do some optimization
     with constant tests on a particular slots.  If we put
     all constant tests for a particular slot/field group at the
     end of that group, then when one of those test succeeds
     during pattern-matching, we don't have to test any
     more of the nodes with that slot/field name to the right.
     =========================================================== */
  prvNode = NULL;
  curNode = nodeSlotGroup;
  while ((curNode == NULL) ? false :
	 (curNode->slotNameID == nodeSlotGroup->slotNameID) &&
	 (curNode->whichField == nodeSlotGroup->whichField))
    {
      if ((curNode->networkTest == NULL) ? false :
	  ((curNode->networkTest->type != OBJ_PN_CONSTANT) ? false :
	   ((struct ObjectCmpPNConstant *) curNode->networkTest->
	    bitMapValue->contents)->pass))
	break;
      prvNode = curNode;
      curNode = curNode->rightNode;
    }
  if (curNode != NULL)
    {
      newNode->leftNode = curNode->leftNode;
      newNode->rightNode = curNode;
      if (curNode->leftNode != NULL)
	curNode->leftNode->rightNode = newNode;
      else if (curNode->lastLevel != NULL)
	curNode->lastLevel->nextLevel = newNode;
      else
	Set_ObjectNetworkPointer (theEnv, newNode);
      curNode->leftNode = newNode;
    }
  else
    {
      newNode->leftNode = prvNode;
      prvNode->rightNode = newNode;
    }

  return (newNode);
}

/********************************************************
  NAME         : DetachObjectPattern
  DESCRIPTION  : Removes a pattern node and all of its
   parent nodes from the pattern network. Nodes are only
   removed if they are no longer shared (i.e. a pattern
   node that has more than one child node is shared). A
   pattern from a rule is typically removed by removing
   the bottom most pattern node used by the pattern and
   then removing any parent nodes which are not shared by
   other patterns.

   Example:
     Patterns (a b c d) and (a b e f) would be represented
     by the pattern net shown on the left.  If (a b c d)
     was detached, the resultant pattern net would be the
     one shown on the right. The '=' represents an
     end-of-pattern node.

           a                  a
           |                  |
           b                  b
           |                  |
           c--e               e
           |  |               |
           d  f               f
           |  |               |
           =  =               =
  INPUTS       : The pattern to be removed
  RETURNS      : Nothing useful
  SIDE EFFECTS : All non-shared nodes associated with the
                 pattern are removed
  NOTES        : None
 ********************************************************/
static void
DetachObjectPattern (Environment * theEnv,
		     struct patternNodeHeader *thePattern)
{
  OBJECT_ALPHA_NODE *alphaPtr, *prv, *te_rminalPtr;
  OBJECT_PATTERN_NODE *patternPtr, *upperLevel;

   /*====================================================*/
  /* Get rid of any matches stored in the alpha memory. */
   /*====================================================*/

  alphaPtr = (OBJECT_ALPHA_NODE *) thePattern;
  CL_ClearObjectPattern_Matches (theEnv, alphaPtr);

   /*==================================*/
  /* Remove alpha links from classes. */
   /*==================================*/

  if (!ConstructData (theEnv)->CL_ClearInProgress)
    {
      CLASS_BITMAP *cbmp;
      unsigned int i;
      Defclass *relevantDefclass;
      CLASS_ALPHA_LINK *alphaLink, *lastAlpha;

      cbmp = (CLASS_BITMAP *) alphaPtr->classbmp->contents;
      for (i = 0; i <= cbmp->maxid; i++)
	{
	  if (TestBitMap (cbmp->map, i))
	    {
	      relevantDefclass = DefclassData (theEnv)->ClassIDMap[i];

	      for (lastAlpha = NULL, alphaLink =
		   relevantDefclass->relevant_te_rminal_alpha_nodes;
		   alphaLink != NULL;
		   lastAlpha = alphaLink, alphaLink = alphaLink->next)
		{
		  if (alphaLink->alphaNode == alphaPtr)
		    {
		      if (lastAlpha == NULL)
			{
			  relevantDefclass->relevant_te_rminal_alpha_nodes =
			    alphaLink->next;
			}
		      else
			{
			  lastAlpha->next = alphaLink->next;
			}
		      rtn_struct (theEnv, classAlphaLink, alphaLink);
		      break;
		    }
		}
	    }
	}
    }

   /*========================================================*/
  /* Unmark the classes to which the pattern is applicable  */
  /* and unmark the class and slot id maps so that they can */
  /* become ephemeral.                                      */
   /*========================================================*/

  MarkBitMapClassesBusy (theEnv, alphaPtr->classbmp, -1);
  DeleteClassBitMap (theEnv, alphaPtr->classbmp);
  if (alphaPtr->slotbmp != NULL)
    {
      CL_DecrementBitMapReferenceCount (theEnv, alphaPtr->slotbmp);
    }

   /*=========================================*/
  /* Only continue deleting this pattern if  */
  /* this is the last alpha memory attached. */
   /*=========================================*/

  prv = NULL;
  te_rminalPtr = CL_ObjectNetworkTe_rminalPointer (theEnv);
  while (te_rminalPtr != alphaPtr)
    {
      prv = te_rminalPtr;
      te_rminalPtr = te_rminalPtr->nxtTe_rminal;
    }

  if (prv == NULL)
    {
      SetCL_ObjectNetworkTe_rminalPointer (theEnv,
					   te_rminalPtr->nxtTe_rminal);
    }
  else
    {
      prv->nxtTe_rminal = te_rminalPtr->nxtTe_rminal;
    }

  prv = NULL;
  te_rminalPtr = alphaPtr->patternNode->alphaNode;
  while (te_rminalPtr != alphaPtr)
    {
      prv = te_rminalPtr;
      te_rminalPtr = te_rminalPtr->nxtInGroup;
    }

  if (prv == NULL)
    {
      if (alphaPtr->nxtInGroup != NULL)
	{
	  alphaPtr->patternNode->alphaNode = alphaPtr->nxtInGroup;
	  CL_RemoveHashedExpression (theEnv, alphaPtr->header.rightHash);
	  rtn_struct (theEnv, objectAlphaNode, alphaPtr);
	  return;
	}
    }
  else
    {
      prv->nxtInGroup = alphaPtr->nxtInGroup;
      CL_RemoveHashedExpression (theEnv, alphaPtr->header.rightHash);
      rtn_struct (theEnv, objectAlphaNode, alphaPtr);
      return;
    }
  alphaPtr->patternNode->alphaNode = NULL;
  CL_RemoveHashedExpression (theEnv, alphaPtr->header.rightHash);
  upperLevel = alphaPtr->patternNode;
  rtn_struct (theEnv, objectAlphaNode, alphaPtr);

  if (upperLevel->nextLevel != NULL)
    return;

   /*==============================================================*/
  /* Loop until all appropriate pattern nodes have been detached. */
   /*==============================================================*/

  while (upperLevel != NULL)
    {
      if ((upperLevel->leftNode == NULL) && (upperLevel->rightNode == NULL))
	{
	 /*===============================================*/
	  /* Pattern node is the only node on this level.  */
	  /* Remove it and continue detaching other nodes  */
	  /* above this one, because no other patterns are */
	  /* dependent upon this node.                     */
	 /*===============================================*/

	  patternPtr = upperLevel;
	  upperLevel = patternPtr->lastLevel;

	  if (upperLevel == NULL)
	    Set_ObjectNetworkPointer (theEnv, NULL);
	  else
	    {
	      if (upperLevel->selector)
		{
		  CL_RemoveHashedPatternNode (theEnv, upperLevel, patternPtr,
					      patternPtr->networkTest->type,
					      patternPtr->networkTest->value);
		}

	      upperLevel->nextLevel = NULL;
	      if (upperLevel->alphaNode != NULL)
		upperLevel = NULL;
	    }

	  CL_RemoveHashedExpression (theEnv,
				     (Expression *) patternPtr->networkTest);
	  rtn_struct (theEnv, objectPatternNode, patternPtr);
	}
      else if (upperLevel->leftNode != NULL)
	{
	 /*====================================================*/
	  /* Pattern node has another pattern node which must   */
	  /* be checked preceding it.  Remove the pattern node, */
	  /* but do not detach any nodes above this one.        */
	 /*====================================================*/

	  patternPtr = upperLevel;

	  if ((patternPtr->lastLevel != NULL) &&
	      (patternPtr->lastLevel->selector))
	    {
	      CL_RemoveHashedPatternNode (theEnv, patternPtr->lastLevel,
					  patternPtr,
					  patternPtr->networkTest->type,
					  patternPtr->networkTest->value);
	    }

	  upperLevel->leftNode->rightNode = upperLevel->rightNode;
	  if (upperLevel->rightNode != NULL)
	    {
	      upperLevel->rightNode->leftNode = upperLevel->leftNode;
	    }

	  CL_RemoveHashedExpression (theEnv,
				     (Expression *) patternPtr->networkTest);
	  rtn_struct (theEnv, objectPatternNode, patternPtr);
	  upperLevel = NULL;
	}
      else
	{
	 /*====================================================*/
	  /* Pattern node has no pattern node preceding it, but */
	  /* does have one succeeding it. Remove the pattern    */
	  /* node, but do not detach any nodes above this one.  */
	 /*====================================================*/

	  patternPtr = upperLevel;
	  upperLevel = upperLevel->lastLevel;
	  if (upperLevel == NULL)
	    {
	      Set_ObjectNetworkPointer (theEnv, patternPtr->rightNode);
	    }
	  else
	    {
	      if (upperLevel->selector)
		{
		  CL_RemoveHashedPatternNode (theEnv, upperLevel, patternPtr,
					      patternPtr->networkTest->type,
					      patternPtr->networkTest->value);
		}

	      upperLevel->nextLevel = patternPtr->rightNode;
	    }
	  patternPtr->rightNode->leftNode = NULL;

	  CL_RemoveHashedExpression (theEnv,
				     (Expression *) patternPtr->networkTest);
	  rtn_struct (theEnv, objectPatternNode, patternPtr);
	  upperLevel = NULL;
	}
    }
}

/***************************************************
  NAME         : CL_ClearObjectPattern_Matches
  DESCRIPTION  : Removes a pattern node alpha memory
                 from the list of partial matches
                 on all instances (active or
                 garbage collected)
  INPUTS       : The pattern node to remove
  RETURNS      : Nothing useful
  SIDE EFFECTS : Pattern alpha memory removed
                 from all object partial match lists
  NOTES        : Used when a pattern is removed
 ***************************************************/
static void
CL_ClearObjectPattern_Matches (Environment * theEnv,
			       OBJECT_ALPHA_NODE * alphaPtr)
{
  Instance *ins;
  IGARBAGE *igrb;

  /* =============================================
     Loop through every active and queued instance
     ============================================= */
  ins = InstanceData (theEnv)->InstanceList;
  while (ins != NULL)
    {
      RemoveObjectPartial_Matches (theEnv, ins,
				   (struct patternNodeHeader *) alphaPtr);
      ins = ins->nxtList;
    }

  /* ============================
     Check for garbaged instances
     ============================ */
  igrb = InstanceData (theEnv)->InstanceGarbageList;
  while (igrb != NULL)
    {
      RemoveObjectPartial_Matches (theEnv, igrb->ins,
				   (struct patternNodeHeader *) alphaPtr);
      igrb = igrb->nxt;
    }
}

/***************************************************
  NAME         : RemoveObjectPartial_Matches
  DESCRIPTION  : Removes a partial match from a
                 list of partial matches for
                 an instance
  INPUTS       : 1) The instance
                 2) The pattern node header
                    corresponding to the match
  RETURNS      : Nothing useful
  SIDE EFFECTS : Match removed
  NOTES        : None
 ***************************************************/
static void
RemoveObjectPartial_Matches (Environment * theEnv,
			     Instance * ins, struct patternNodeHeader *phead)
{
  struct patternMatch *match_before, *match_ptr;

  match_before = NULL;
  match_ptr = (struct patternMatch *) ins->partialMatchList;

  /* =======================================
     Loop through every match for the object
     ======================================= */
  while (match_ptr != NULL)
    {
      if (match_ptr->matchingPattern == phead)
	{
	  ins->busy--;
	  if (match_before == NULL)
	    {
	      ins->partialMatchList = (void *) match_ptr->next;
	      rtn_struct (theEnv, patternMatch, match_ptr);
	      match_ptr = (struct patternMatch *) ins->partialMatchList;
	    }
	  else
	    {
	      match_before->next = match_ptr->next;
	      rtn_struct (theEnv, patternMatch, match_ptr);
	      match_ptr = match_before->next;
	    }
	}
      else
	{
	  match_before = match_ptr;
	  match_ptr = match_ptr->next;
	}
    }
}

/******************************************************
  NAME         : CheckDuplicateSlots
  DESCRIPTION  : Dete_rmines if a restriction has
                 already been defined in a pattern
  INPUTS       : The list of already built restrictions
  RETURNS      : True if a definition already
                 exists, false otherwise
  SIDE EFFECTS : An error message is printed if a
                 duplicate is found
  NOTES        : None
 ******************************************************/
static bool
CheckDuplicateSlots (Environment * theEnv,
		     struct lhsParseNode *nodeList, CLIPSLexeme * slotName)
{
  while (nodeList != NULL)
    {
      if (nodeList->slot == slotName)
	{
	  CL_PrintErrorID (theEnv, "OBJRTBLD", 4, true);
	  CL_WriteString (theEnv, STDERR,
			  "Multiple restrictions on attribute '");
	  CL_WriteString (theEnv, STDERR, slotName->contents);
	  CL_WriteString (theEnv, STDERR, "' not allowed.\n");
	  return true;
	}
      nodeList = nodeList->right;
    }
  return false;
}

/**********************************************************
  NAME         : ParseClassRestriction
  DESCRIPTION  : Parses the single-field constraint
                  on the class an object pattern
  INPUTS       : 1) The logical input source
                 2) A buffer for tokens
  RETURNS      : The inte_rmediate pattern nodes
                 representing the class constraint
                 (NULL on errors)
  SIDE EFFECTS : Inte_rmediate pattern nodes allocated
  NOTES        : None
 **********************************************************/
static struct lhsParseNode *
ParseClassRestriction (Environment * theEnv,
		       const char *readSource, struct token *theToken)
{
  struct lhsParseNode *tmpNode;
  CLIPSLexeme *rln;
  CONSTRAINT_RECORD *rv;

  rv = CL_GetConstraintRecord (theEnv);
  rv->anyAllowed = 0;
  rv->symbolsAllowed = 1;
  rln = theToken->lexemeValue;
  CL_SavePPBuffer (theEnv, " ");
  CL_GetToken (theEnv, readSource, theToken);
  tmpNode =
    CL_RestrictionParse (theEnv, readSource, theToken, false, rln, ISA_ID, rv,
			 0);
  if (tmpNode == NULL)
    {
      CL_RemoveConstraint (theEnv, rv);
      return NULL;
    }
  if ((theToken->tknType != RIGHT_PARENTHESIS_TOKEN) ||
      (tmpNode->pnType == MF_WILDCARD_NODE) ||
      (tmpNode->pnType == MF_VARIABLE_NODE))
    {
      CL_PPBackup (theEnv);
      if (theToken->tknType != RIGHT_PARENTHESIS_TOKEN)
	{
	  CL_SavePPBuffer (theEnv, " ");
	  CL_SavePPBuffer (theEnv, theToken->printFo_rm);
	}
      CL_SyntaxErrorMessage (theEnv, "class restriction in object pattern");
      CL_ReturnLHSParseNodes (theEnv, tmpNode);
      CL_RemoveConstraint (theEnv, rv);
      return NULL;
    }
  tmpNode->derivedConstraints = 1;
  return (tmpNode);
}

/**********************************************************
  NAME         : ParseNameRestriction
  DESCRIPTION  : Parses the single-field constraint
                  on the name of an object pattern
  INPUTS       : 1) The logical input source
                 2) A buffer for tokens
  RETURNS      : The inte_rmediate pattern nodes
                 representing the name constraint
                 (NULL on errors)
  SIDE EFFECTS : Inte_rmediate pattern nodes allocated
  NOTES        : None
 **********************************************************/
static struct lhsParseNode *
ParseNameRestriction (Environment * theEnv,
		      const char *readSource, struct token *theToken)
{
  struct lhsParseNode *tmpNode;
  CLIPSLexeme *rln;
  CONSTRAINT_RECORD *rv;

  rv = CL_GetConstraintRecord (theEnv);
  rv->anyAllowed = 0;
  rv->instanceNamesAllowed = 1;
  rln = theToken->lexemeValue;
  CL_SavePPBuffer (theEnv, " ");
  CL_GetToken (theEnv, readSource, theToken);
  tmpNode =
    CL_RestrictionParse (theEnv, readSource, theToken, false, rln, NAME_ID,
			 rv, 0);
  if (tmpNode == NULL)
    {
      CL_RemoveConstraint (theEnv, rv);
      return NULL;
    }
  if ((theToken->tknType != RIGHT_PARENTHESIS_TOKEN) ||
      (tmpNode->pnType == MF_WILDCARD_NODE) ||
      (tmpNode->pnType == MF_VARIABLE_NODE))
    {
      CL_PPBackup (theEnv);
      if (theToken->tknType != RIGHT_PARENTHESIS_TOKEN)
	{
	  CL_SavePPBuffer (theEnv, " ");
	  CL_SavePPBuffer (theEnv, theToken->printFo_rm);
	}
      CL_SyntaxErrorMessage (theEnv, "name restriction in object pattern");
      CL_ReturnLHSParseNodes (theEnv, tmpNode);
      CL_RemoveConstraint (theEnv, rv);
      return NULL;
    }

  tmpNode->derivedConstraints = 1;
  return (tmpNode);
}

/***************************************************
  NAME         : CL_ParseSlotRestriction
  DESCRIPTION  : Parses the field constraint(s)
                  on a slot of an object pattern
  INPUTS       : 1) The logical input source
                 2) A buffer for tokens
                 3) Constraint record holding the
                    unioned constraints of all the
                    slots which could match the
                    slot pattern
                 4) A flag indicating if any
                    multifield slots match the name
  RETURNS      : The inte_rmediate pattern nodes
                 representing the slot constraint(s)
                 (NULL on errors)
  SIDE EFFECTS : Inte_rmediate pattern nodes
                 allocated
  NOTES        : None
 ***************************************************/
static struct lhsParseNode *
CL_ParseSlotRestriction (Environment * theEnv,
			 const char *readSource,
			 struct token *theToken,
			 CONSTRAINT_RECORD * slotConstraints, bool multip)
{
  struct lhsParseNode *tmpNode;
  CLIPSLexeme *slotName;

  slotName = theToken->lexemeValue;
  CL_SavePPBuffer (theEnv, " ");
  CL_GetToken (theEnv, readSource, theToken);
  tmpNode =
    CL_RestrictionParse (theEnv, readSource, theToken, multip, slotName,
			 CL_FindSlotNameID (theEnv, slotName),
			 slotConstraints, 1);
  if (tmpNode == NULL)
    {
      CL_RemoveConstraint (theEnv, slotConstraints);
      return NULL;
    }
  if (theToken->tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      CL_PPBackup (theEnv);
      CL_SavePPBuffer (theEnv, " ");
      CL_SavePPBuffer (theEnv, theToken->printFo_rm);
      CL_SyntaxErrorMessage (theEnv, "object slot pattern");
      CL_ReturnLHSParseNodes (theEnv, tmpNode);
      CL_RemoveConstraint (theEnv, slotConstraints);
      return NULL;
    }
  if ((tmpNode->bottom == NULL) && (tmpNode->multifieldSlot))
    {
      CL_PPBackup (theEnv);
      CL_PPBackup (theEnv);
      CL_SavePPBuffer (theEnv, ")");
    }
  tmpNode->derivedConstraints = 1;
  return (tmpNode);
}

/********************************************************
  NAME         : CL_NewClassBitMap
  DESCRIPTION  : Creates a new bitmap large enough
                 to hold all ids of classes in the
                 system and initializes all the bits
                 to zero or one.
  INPUTS       : 1) The maximum id that will be set
                    in the bitmap
                 2) An integer code indicating if all
                    the bits are to be set to zero or one
  RETURNS      : The new bitmap
  SIDE EFFECTS : BitMap allocated and initialized
  NOTES        : None
 ********************************************************/
static CLASS_BITMAP *
CL_NewClassBitMap (Environment * theEnv, unsigned short maxid, bool set)
{
  CLASS_BITMAP *bmp;
  size_t size;

  size = sizeof (CLASS_BITMAP) + (sizeof (char) * (maxid / BITS_PER_BYTE));
  bmp = (CLASS_BITMAP *) CL_gm2 (theEnv, size);
  CL_ClearBitString (bmp, size);
  bmp->maxid = (unsigned short) maxid;
  InitializeClassBitMap (theEnv, bmp, set);

  return bmp;
}

/***********************************************************
  NAME         : InitializeClassBitMap
  DESCRIPTION  : Initializes a bitmap to all zeroes or ones.
  INPUTS       : 1) The bitmap
                 2) An integer code indicating if all
                    the bits are to be set to zero or one
  RETURNS      : Nothing useful
  SIDE EFFECTS : The bitmap is initialized
  NOTES        : None
 ***********************************************************/
static void
InitializeClassBitMap (Environment * theEnv, CLASS_BITMAP * bmp, bool set)
{
  unsigned short i, bytes;
  Defclass *cls;
  Defmodule *currentModule;

  bytes = bmp->maxid / BITS_PER_BYTE + 1;
  while (bytes > 0)
    {
      bmp->map[bytes - 1] = (char) 0;
      bytes--;
    }

  if (set)
    {
      currentModule = CL_GetCurrentModule (theEnv);
      for (i = 0; i <= bmp->maxid; i++)
	{
	  cls = DefclassData (theEnv)->ClassIDMap[i];
	  if ((cls != NULL) ? CL_DefclassInScope (theEnv, cls,
						  currentModule) : false)
	    {
	      if (cls->reactive && (cls->abstract == 0))
		SetBitMap (bmp->map, i);
	    }
	}
    }
}

/********************************************
  NAME         : DeleteInte_rmediateClassBitMap
  DESCRIPTION  : Deallocates a bitmap
  INPUTS       : The class set
  RETURNS      : Nothing useful
  SIDE EFFECTS : Class set deallocated
  NOTES        : None
 ********************************************/
static void
DeleteInte_rmediateClassBitMap (Environment * theEnv, CLASS_BITMAP * bmp)
{
  CL_rm (theEnv, bmp, ClassBitMapSize (bmp));
}

/******************************************************
  NAME         : CopyClassBitMap
  DESCRIPTION  : Increments the in use count of a
                 bitmap and returns the same pointer
  INPUTS       : The bitmap
  RETURNS      : The bitmap
  SIDE EFFECTS : Increments the in use count
  NOTES        : Class sets are shared by multiple
                 copies of an object pattern within an
                 OR CE.  The use count prevents having
                 to make duplicate copies of the bitmap
 ******************************************************/
static void *
CopyClassBitMap (Environment * theEnv, void *gset)
{
#if MAC_XCD
#pragma unused(theEnv)
#endif

  if (gset != NULL)
    IncrementBitMapCount (gset);
  return (gset);
}

/**********************************************************
  NAME         : DeleteClassBitMap
  DESCRIPTION  : Deallocates a bitmap,
                 and decrements the busy flags of the
                 classes marked in the bitmap
  INPUTS       : The bitmap
  RETURNS      : Nothing useful
  SIDE EFFECTS : Class set deallocated and classes unmarked
  NOTES        : None
 **********************************************************/
static void
DeleteClassBitMap (Environment * theEnv, void *gset)
{
  if (gset == NULL)
    return;
  CL_DecrementBitMapReferenceCount (theEnv, (CLIPSBitMap *) gset);
}

/***************************************************
  NAME         : MarkBitMapClassesBusy
  DESCRIPTION  : Increments/Decrements busy counts
                 of all classes marked in a bitmap
  INPUTS       : 1) The bitmap hash node
                 2) 1 or -1 (to increment or
                    decrement class busy counts)
  RETURNS      : Nothing useful
  SIDE EFFECTS : Bitmap class busy counts updated
  NOTES        : None
 ***************************************************/
static void
MarkBitMapClassesBusy (Environment * theEnv, CLIPSBitMap * bmphn, int offset)
{
  CLASS_BITMAP *bmp;
  unsigned short i;
  Defclass *cls;

  /* ====================================
     If a clear is in progress, we do not
     have to worry about busy counts
     ==================================== */
  if (ConstructData (theEnv)->CL_ClearInProgress)
    return;
  bmp = (CLASS_BITMAP *) bmphn->contents;
  for (i = 0; i <= bmp->maxid; i++)
    if (TestBitMap (bmp->map, i))
      {
	cls = DefclassData (theEnv)->ClassIDMap[i];
	cls->busy += (unsigned int) offset;
      }
}

/****************************************************
  NAME         : EmptyClassBitMap
  DESCRIPTION  : Dete_rmines if one or more bits
                 are marked in a bitmap
  INPUTS       : The bitmap
  RETURNS      : True if the set has no bits
                 marked, false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ****************************************************/
static bool
EmptyClassBitMap (CLASS_BITMAP * bmp)
{
  unsigned short bytes;

  bytes = (unsigned short) (bmp->maxid / BITS_PER_BYTE + 1);
  while (bytes > 0)
    {
      if (bmp->map[bytes - 1] != (char) 0)
	return false;
      bytes--;
    }
  return true;
}

/***************************************************
  NAME         : IdenticalClassBitMap
  DESCRIPTION  : Dete_rmines if two bitmaps
                 are identical
  INPUTS       : 1) First bitmap
                 2) Second bitmap
  RETURNS      : True if bitmaps are the same,
                 false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
static bool
IdenticalClassBitMap (CLASS_BITMAP * cs1, CLASS_BITMAP * cs2)
{
  unsigned short i;

  if (cs1->maxid != cs2->maxid)
    return false;
  for (i = 0; i < (cs1->maxid / BITS_PER_BYTE + 1); i++)
    if (cs1->map[i] != cs2->map[i])
      return false;
  return true;
}

/*****************************************************************
  NAME         : ProcessClassRestriction
  DESCRIPTION  : Examines a class restriction and fo_rms a bitmap
                 corresponding to the maximal set of classes which
                 can satisfy a static analysis of the restriction
  INPUTS       : 1) The bitmap to mark classes in
                 2) The lhsParseNodes of the restriction
                 3) A flag indicating if this is the first
                    non-recursive call or not
  RETURNS      : True if all OK, false otherwise
  SIDE EFFECTS : Class bitmap set and lhsParseNodes corressponding
                 to constant restrictions are removed
  NOTES        : None
 *****************************************************************/
static bool
ProcessClassRestriction (Environment * theEnv,
			 CLASS_BITMAP * clsset,
			 struct lhsParseNode **classRestrictions,
			 bool recursiveCall)
{
  struct lhsParseNode *chk, **oraddr;
  CLASS_BITMAP *tmpset1, *tmpset2;
  bool constant_restriction = true;

  if (*classRestrictions == NULL)
    {
      if (recursiveCall)
	InitializeClassBitMap (theEnv, clsset, true);
      return true;
    }

  /* ===============================================
     Dete_rmine the corresponding class set and union
     it with the current total class set.  If an AND
     restriction is comprised entirely of symbols,
     it can be removed
     =============================================== */
  tmpset1 =
    CL_NewClassBitMap (theEnv, DefclassData (theEnv)->MaxClassID - 1, 1);
  tmpset2 =
    CL_NewClassBitMap (theEnv, DefclassData (theEnv)->MaxClassID - 1, 0);
  for (chk = *classRestrictions; chk != NULL; chk = chk->right)
    {
      if (chk->pnType == SYMBOL_NODE)
	{
	  const char *className = chk->lexemeValue->contents;

	  chk->value = CL_LookupDefclassByMdlOrScope (theEnv, className);
	  if (chk->value == NULL)
	    {
	      CL_PrintErrorID (theEnv, "OBJRTBLD", 5, false);
	      CL_WriteString (theEnv, STDERR, "Undefined class '");
	      CL_WriteString (theEnv, STDERR, className);
	      CL_WriteString (theEnv, STDERR, "' in object pattern.\n");
	      DeleteInte_rmediateClassBitMap (theEnv, tmpset1);
	      DeleteInte_rmediateClassBitMap (theEnv, tmpset2);
	      return false;
	    }
	  if (chk->negated)
	    {
	      InitializeClassBitMap (theEnv, tmpset2, true);
	      CL_MarkBitMapSubclasses (tmpset2->map, (Defclass *) chk->value,
				       0);
	    }
	  else
	    {
	      InitializeClassBitMap (theEnv, tmpset2, false);
	      CL_MarkBitMapSubclasses (tmpset2->map, (Defclass *) chk->value,
				       1);
	    }
	  IntersectClassBitMaps (tmpset1, tmpset2);
	}
      else
	constant_restriction = false;
    }
  if (EmptyClassBitMap (tmpset1))
    {
      CL_PrintErrorID (theEnv, "OBJRTBLD", 2, false);
      CL_WriteString (theEnv, STDERR,
		      "No objects of existing classes can satisfy ");
      CL_WriteString (theEnv, STDERR,
		      "'is-a' restriction in object pattern.\n");
      DeleteInte_rmediateClassBitMap (theEnv, tmpset1);
      DeleteInte_rmediateClassBitMap (theEnv, tmpset2);
      return false;
    }
  if (constant_restriction)
    {
      chk = *classRestrictions;
      *classRestrictions = chk->bottom;
      chk->bottom = NULL;
      CL_ReturnLHSParseNodes (theEnv, chk);
      oraddr = classRestrictions;
    }
  else
    oraddr = &(*classRestrictions)->bottom;
  UnionClassBitMaps (clsset, tmpset1);
  DeleteInte_rmediateClassBitMap (theEnv, tmpset1);
  DeleteInte_rmediateClassBitMap (theEnv, tmpset2);

  /* =====================================
     Process the next OR class restriction
     ===================================== */
  return (ProcessClassRestriction (theEnv, clsset, oraddr, false));
}

/****************************************************************
  NAME         : ProcessSlotRestriction
  DESCRIPTION  : Dete_rmines which slots could match the slot
                 pattern and dete_rmines the union of all
                 constraints for the pattern
  INPUTS       : 1) The class set
                 2) The slot name
                 3) A buffer to hold a flag indicating if
                    any multifield slots are found w/ this name
  RETURNS      : A union of the constraints on all the slots
                 which could match the slots (NULL if no
                 slots found)
  SIDE EFFECTS : The class bitmap set is marked/cleared
  NOTES        : None
 ****************************************************************/
static CONSTRAINT_RECORD *
ProcessSlotRestriction (Environment * theEnv,
			CLASS_BITMAP * clsset,
			CLIPSLexeme * slotName, bool *multip)
{
  Defclass *cls;
  int si;
  CONSTRAINT_RECORD *totalConstraints = NULL, *tmpConstraints;
  unsigned i;

  *multip = false;
  for (i = 0; i < CLASS_TABLE_HASH_SIZE; i++)
    for (cls = DefclassData (theEnv)->ClassTable[i]; cls != NULL;
	 cls = cls->nxtHash)
      {
	if (TestBitMap (clsset->map, cls->id))
	  {
	    si = CL_FindInstanceTemplateSlot (theEnv, cls, slotName);
	    if ((si != -1) ? cls->instanceTemplate[si]->reactive : false)
	      {
		if (cls->instanceTemplate[si]->multiple)
		  *multip = true;
		tmpConstraints =
		  CL_UnionConstraints (theEnv,
				       cls->instanceTemplate[si]->constraint,
				       totalConstraints);
		CL_RemoveConstraint (theEnv, totalConstraints);
		totalConstraints = tmpConstraints;
	      }
	    else
	      CL_ClearBitMap (clsset->map, cls->id);
	  }
      }
  return (totalConstraints);
}

/****************************************************
  NAME         : IntersectClassBitMaps
  DESCRIPTION  : Bitwise-ands two bitmaps and stores
                 the result in the first
  INPUTS       : The two bitmaps
  RETURNS      : Nothing useful
  SIDE EFFECTS : ClassBitMaps anded
  NOTES        : Assumes the first bitmap is at least
                 as large as the second
 ****************************************************/
static void
IntersectClassBitMaps (CLASS_BITMAP * cs1, CLASS_BITMAP * cs2)
{
  unsigned short bytes;

  bytes = (unsigned short) (cs2->maxid / BITS_PER_BYTE + 1);
  while (bytes > 0)
    {
      cs1->map[bytes - 1] &= cs2->map[bytes - 1];
      bytes--;
    }
}

/****************************************************
  NAME         : UnionClassBitMaps
  DESCRIPTION  : Bitwise-ors two bitmaps and stores
                 the result in the first
  INPUTS       : The two bitmaps
  RETURNS      : Nothing useful
  SIDE EFFECTS : ClassBitMaps ored
  NOTES        : Assumes the first bitmap is at least
                 as large as the second
 ****************************************************/
static void
UnionClassBitMaps (CLASS_BITMAP * cs1, CLASS_BITMAP * cs2)
{
  unsigned short bytes;

  bytes = (unsigned short) (cs2->maxid / BITS_PER_BYTE + 1);
  while (bytes > 0)
    {
      cs1->map[bytes - 1] |= cs2->map[bytes - 1];
      bytes--;
    }
}

/*****************************************************
  NAME         : PackClassBitMap
  DESCRIPTION  : This routine packs a bitmap
                 bitmap such that at least one of
                 the bits in the rightmost byte is
                 set (i.e. the bitmap takes up
                 the smallest space possible).
  INPUTS       : The bitmap
  RETURNS      : The new (packed) bitmap
  SIDE EFFECTS : The oldset is deallocated
  NOTES        : None
 *****************************************************/
static CLASS_BITMAP *
PackClassBitMap (Environment * theEnv, CLASS_BITMAP * oldset)
{
  unsigned short newmaxid;
  CLASS_BITMAP *newset;

  for (newmaxid = oldset->maxid; newmaxid > 0; newmaxid--)
    if (TestBitMap (oldset->map, newmaxid))
      break;
  if (newmaxid != oldset->maxid)
    {
      newset = CL_NewClassBitMap (theEnv, newmaxid, 0);
      GenCopyMemory (char, newmaxid / BITS_PER_BYTE + 1, newset->map,
		     oldset->map);
      DeleteInte_rmediateClassBitMap (theEnv, oldset);
    }
  else
    newset = oldset;
  return (newset);
}

/*****************************************************************
  NAME         : FilterObjectPattern
  DESCRIPTION  : Appends an extra node to hold the bitmap,
                 and finds is-a and name nodes
  INPUTS       : 1) The object pattern parser address
                    to give to a default is-a slot
                 2) The unfiltered slot list
                 3) A buffer to hold the address of
                    the class bitmap restriction node
                 4) A buffer to hold the address of
                    the is-a restriction node
                 4) A buffer to hold the address of
                    the name restriction node
  RETURNS      : The filtered slot list
  SIDE EFFECTS : clsset is attached to extra slot pattern
                 Pointers to the is-a and name slots are also
                 stored (if they exist) for easy reference
  NOTES        : None
 *****************************************************************/
static struct lhsParseNode *
FilterObjectPattern (Environment * theEnv,
		     struct patternParser *selfPatternType,
		     struct lhsParseNode *unfilteredSlots,
		     struct lhsParseNode **bitmap_slot,
		     struct lhsParseNode **isa_slot,
		     struct lhsParseNode **name_slot)
{
  struct lhsParseNode *prv, *cur;

  *isa_slot = NULL;
  *name_slot = NULL;

  /* ============================================
     Create a dummy node to attach to the end
     of the pattern which holds the class bitmap.
     ============================================ */
  *bitmap_slot = CL_GetLHSParseNode (theEnv);
  (*bitmap_slot)->pnType = SF_WILDCARD_NODE;
  (*bitmap_slot)->slot = DefclassData (theEnv)->ISA_SYMBOL;
  (*bitmap_slot)->slotNumber = ISA_ID;
  (*bitmap_slot)->index = 1;
  (*bitmap_slot)->patternType = selfPatternType;
  (*bitmap_slot)->userData = unfilteredSlots->userData;
  unfilteredSlots->userData = NULL;

  /* ========================
     Find is-a and name nodes
     ======================== */
  prv = NULL;
  cur = unfilteredSlots;
  while (cur != NULL)
    {
      if (cur->slot == DefclassData (theEnv)->ISA_SYMBOL)
	*isa_slot = cur;
      else if (cur->slot == DefclassData (theEnv)->NAME_SYMBOL)
	*name_slot = cur;
      prv = cur;
      cur = cur->right;
    }

  /* ================================
     Add the class bitmap conditional
     element to end of pattern
     ================================ */
  if (prv == NULL)
    unfilteredSlots = *bitmap_slot;
  else
    prv->right = *bitmap_slot;
  return (unfilteredSlots);
}

/***************************************************
  NAME         : Fo_rmSlotBitMap
  DESCRIPTION  : Examines an object pattern and
                 fo_rms a minimal bitmap marking
                 the ids of the slots used in
                 the pattern
  INPUTS       : The inte_rmediate parsed pattern
  RETURNS      : The new slot bitmap (can be NULL)
  SIDE EFFECTS : Bitmap created and added to hash
                 table - corresponding bits set
                 for ids of slots used in pattern
  NOTES        : None
 ***************************************************/
static CLIPSBitMap *
Fo_rmSlotBitMap (Environment * theEnv, struct lhsParseNode *thePattern)
{
  struct lhsParseNode *node;
  unsigned short maxSlotID = USHRT_MAX;
  unsigned size;
  SLOT_BITMAP *bmp;
  CLIPSBitMap *hshBmp;

   /*==========================================*/
  /* Find the largest slot id in the pattern. */
   /*==========================================*/

  for (node = thePattern; node != NULL; node = node->right)
    {
      if (((node->slotNumber > maxSlotID) ||
	   (maxSlotID == USHRT_MAX)) &&
	  (node->slotNumber != UNSPECIFIED_SLOT))
	{
	  maxSlotID = node->slotNumber;
	}
    }

   /*==================================================*/
  /* If the pattern contains no slot tests or only    */
  /* tests on the class or name (which do not change) */
  /* do not store a slot bitmap.                      */
   /*==================================================*/

  if ((maxSlotID == ISA_ID) ||
      (maxSlotID == NAME_ID) || (maxSlotID == USHRT_MAX))
    {
      return NULL;
    }

   /*======================================*/
  /* Initialize the bitmap to all zeroes. */
   /*======================================*/

  size =
    (sizeof (SLOT_BITMAP) + (sizeof (char) * (maxSlotID / BITS_PER_BYTE)));
  bmp = (SLOT_BITMAP *) CL_gm2 (theEnv, size);
  CL_ClearBitString (bmp, size);
  bmp->maxid = (unsigned short) maxSlotID;

   /*===============================================*/
  /* Add (retrieve) a bitmap to (from) the bitmap  */
  /* hash table which has a corresponding bit set  */
  /* for the id of every slot used in the pattern. */
   /*===============================================*/

  for (node = thePattern; node != NULL; node = node->right)
    {
      SetBitMap (bmp->map, node->slotNumber);
    }

  hshBmp = (CLIPSBitMap *) CL_AddBitMap (theEnv, bmp, SlotBitMapSize (bmp));
  CL_rm (theEnv, bmp, size);

  return hshBmp;
}

/****************************************************
  NAME         : RemoveSlotExistenceTests
  DESCRIPTION  : Removes slot existence test since
                 these are accounted for by class
                 bitmap or name slot.
  INPUTS       : 1) The inte_rmediate pattern nodes
                 2) A buffer to hold the class bitmap
  RETURNS      : The filtered list
  SIDE EFFECTS : Slot existence tests removed
  NOTES        : None
 ****************************************************/
static struct lhsParseNode *
RemoveSlotExistenceTests (Environment * theEnv,
			  struct lhsParseNode *thePattern, CLIPSBitMap ** bmp)
{
  struct lhsParseNode *tempPattern = thePattern;
  struct lhsParseNode *lastPattern = NULL, *head = thePattern;

  while (tempPattern != NULL)
    {
      /* ==========================================
         Remember the class bitmap for this pattern
         ========================================== */
      if (tempPattern->userData != NULL)
	{
	  *bmp = (CLIPSBitMap *) tempPattern->userData;
	  lastPattern = tempPattern;
	  tempPattern = tempPattern->right;
	}

      /* ===========================================================
         A single field slot that has no pattern network expression
         associated with it can be removed (i.e. any value contained
         in this slot will satisfy the pattern being matched).
         =========================================================== */
      else if (((tempPattern->pnType == SF_WILDCARD_NODE) ||
		(tempPattern->pnType == SF_VARIABLE_NODE)) &&
	       (tempPattern->networkTest == NULL))
	{
	  if (lastPattern != NULL)
	    lastPattern->right = tempPattern->right;
	  else
	    head = tempPattern->right;

	  tempPattern->right = NULL;
	  CL_ReturnLHSParseNodes (theEnv, tempPattern);

	  if (lastPattern != NULL)
	    tempPattern = lastPattern->right;
	  else
	    tempPattern = head;
	}

      /* =====================================================
         A multifield variable or wildcard within a multifield
         slot can be removed if there are no other multifield
         variables or wildcards contained in the same slot
         (and the multifield has no expressions which must be
         evaluated in the fact pattern network).
         ===================================================== */
      else
	if (((tempPattern->pnType == MF_WILDCARD_NODE)
	     || (tempPattern->pnType == MF_VARIABLE_NODE))
	    && (tempPattern->multifieldSlot == false)
	    && (tempPattern->networkTest == NULL)
	    && (tempPattern->multiFieldsBefore == 0)
	    && (tempPattern->multiFieldsAfter == 0))
	{
	  if (lastPattern != NULL)
	    lastPattern->right = tempPattern->right;
	  else
	    head = tempPattern->right;

	  tempPattern->right = NULL;
	  CL_ReturnLHSParseNodes (theEnv, tempPattern);

	  if (lastPattern != NULL)
	    tempPattern = lastPattern->right;
	  else
	    tempPattern = head;
	}

      /* ================================================================
         A multifield wildcard or variable contained in a multifield slot
         that contains no other multifield wildcards or variables, but
         does have an expression that must be evaluated, can be changed
         to a single field pattern node with the same expression.
         ================================================================ */
      else
	if (((tempPattern->pnType == MF_WILDCARD_NODE)
	     || (tempPattern->pnType == MF_VARIABLE_NODE))
	    && (tempPattern->multifieldSlot == false)
	    && (tempPattern->networkTest != NULL)
	    && (tempPattern->multiFieldsBefore == 0)
	    && (tempPattern->multiFieldsAfter == 0))
	{
	  tempPattern->pnType = SF_WILDCARD_NODE;
	  lastPattern = tempPattern;
	  tempPattern = tempPattern->right;
	}

      /* =======================================================
         If we're dealing with a multifield slot with no slot
         restrictions, then treat the multfield slot as a single
         field slot, but attach a test which verifies that the
         slot contains a zero length multifield value.
         ======================================================= */
      else if ((tempPattern->pnType == MF_WILDCARD_NODE) &&
	       (tempPattern->multifieldSlot == true) &&
	       (tempPattern->bottom == NULL))
	{
	  tempPattern->pnType = SF_WILDCARD_NODE;
	  CL_GenObjectZeroLengthTest (theEnv, tempPattern);
	  tempPattern->multifieldSlot = false;
	  lastPattern = tempPattern;
	  tempPattern = tempPattern->right;
	}

      /* ======================================================
         Recursively call RemoveSlotExistenceTests for the slot
         restrictions contained within a multifield slot.
         ====================================================== */
      else if ((tempPattern->pnType == MF_WILDCARD_NODE) &&
	       (tempPattern->multifieldSlot == true))
	{
	  /* =====================================================
	     Add an expression to the first pattern restriction in
	     the multifield slot that dete_rmines whether or not
	     the fact's slot value contains the minimum number of
	     required fields to satisfy the pattern restrictions
	     for this slot. The length check is place before any
	     other tests, so that preceeding checks do not have to
	     dete_rmine if there are enough fields in the slot to
	     safely retrieve a value.
	     ===================================================== */
	  CL_GenObjectLengthTest (theEnv, tempPattern->bottom);

	  /* =======================================================
	     Remove any unneeded pattern restrictions from the slot.
	     ======================================================= */
	  tempPattern->bottom =
	    RemoveSlotExistenceTests (theEnv, tempPattern->bottom, bmp);

	  /* =========================================================
	     If the slot no longer contains any restrictions, then the
	     multifield slot can be completely removed. In any case,
	     move on to the next slot to be examined for removal.
	     ========================================================= */
	  if (tempPattern->bottom == NULL)
	    {
	      if (lastPattern != NULL)
		lastPattern->right = tempPattern->right;
	      else
		head = tempPattern->right;

	      tempPattern->right = NULL;
	      CL_ReturnLHSParseNodes (theEnv, tempPattern);

	      if (lastPattern != NULL)
		tempPattern = lastPattern->right;
	      else
		tempPattern = head;
	    }
	  else
	    {
	      lastPattern = tempPattern;
	      tempPattern = tempPattern->right;
	    }
	}

      /* =====================================================
         If none of the other tests for removing slots or slot
         restrictions apply, then move on to the next slot or
         slot restriction to be tested.
         ===================================================== */
      else
	{
	  lastPattern = tempPattern;
	  tempPattern = tempPattern->right;
	}
    }

  /* ====================================
     Return the pattern with unused slots
     and slot restrictions removed.
     ==================================== */
  return (head);
}

#endif

/**************************************************************
  NAME         : CL_ObjectMatchDelayParse
  DESCRIPTION  : Parses the object-pattern-match-delay function
  INPUTS       : 1) The function call expression
                 2) The logical name of the input source
  RETURNS      : The top expression with the other
                 action expressions attached
  SIDE EFFECTS : Parses the function call and attaches
                 the appropriate arguments to the
                 top node
  NOTES        : None
 **************************************************************/
static Expression *
CL_ObjectMatchDelayParse (Environment * theEnv,
			  struct expr *top, const char *infile)
{
  struct token tkn;

  CL_IncrementIndentDepth (theEnv, 3);
  CL_PPCRAndIndent (theEnv);
  top->argList = CL_GroupActions (theEnv, infile, &tkn, true, NULL, false);
  CL_PPBackup (theEnv);
  CL_PPBackup (theEnv);
  CL_SavePPBuffer (theEnv, tkn.printFo_rm);
  CL_DecrementIndentDepth (theEnv, 3);
  if (top->argList == NULL)
    {
      CL_ReturnExpression (theEnv, top);
      return NULL;
    }
  return (top);
}

#if (! BLOAD_ONLY) && (! RUN_TIME)

/***************************************************
  NAME         : MarkObjectPtnCL_Incremental_Reset
  DESCRIPTION  : Marks/unmarks an object pattern for
                 incremental reset
  INPUTS       : 1) The object pattern alpha node
                 2) The value to which to set the
                    incremental reset flag
  RETURNS      : Nothing useful
  SIDE EFFECTS : The pattern node is set/unset
  NOTES        : The pattern node can only be
                 set if it is a new node and
                 thus marked for initialization
                 by PlaceObjectPattern
 ***************************************************/
static void
MarkObjectPtnCL_Incremental_Reset (Environment * theEnv,
				   struct patternNodeHeader *thePattern,
				   bool value)
{
#if MAC_XCD
#pragma unused(theEnv)
#endif

  if (thePattern->initialize == false)
    return;
  thePattern->initialize = value;
}

/***********************************************************
  NAME         : ObjectCL_Incremental_Reset
  DESCRIPTION  : Perfo_rms an assert for all instances in the
                 system.  All new patterns in the pattern
                 network from the new rule have been marked
                 as needing processing.  Old patterns will
                 be ignored.
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : All objects driven through new patterns
  NOTES        : None
 ***********************************************************/
static void
ObjectCL_Incremental_Reset (Environment * theEnv)
{
  Instance *ins;

  for (ins = InstanceData (theEnv)->InstanceList; ins != NULL;
       ins = ins->nxtList)
    CL_ObjectNetworkAction (theEnv, OBJECT_ASSERT, ins, -1);
}

#endif

#endif
