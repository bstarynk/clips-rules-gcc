   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  02/03/18             */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Binary CL_Load/CL_Save Functions Defrule               */
/*          Object Pattern Network                           */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Converted INSTANCE_PATTERN_MATCHING to         */
/*            DEFRULE_CONSTRUCT.                             */
/*                                                           */
/*            CL_ResetObjectMatchTimeTags did not pass in the   */
/*            environment argument when BLOAD_ONLY was set.  */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Added support for hashed comparisons to        */
/*            constants.                                     */
/*                                                           */
/*            Added support for hashed alpha memories.       */
/*                                                           */
/*      6.31: Optimization for marking relevant alpha nodes  */
/*            in the object pattern network.                 */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)

#include "bload.h"
#include "bsave.h"
#include "classfun.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "insfun.h"
#include "objrtmch.h"
#include "pattern.h"
#include "reteutil.h"
#include "rulebin.h"

#include "objrtbin.h"

/* =========================================
   *****************************************
               MACROS AND TYPES
   =========================================
   ***************************************** */
typedef unsigned long UNLN;

typedef struct bsaveObjectPatternNode
{
  unsigned multifieldNode:1;
  unsigned endSlot:1;
  unsigned selector:1;
  unsigned whichField:8;
  unsigned short leaveFields;
  unsigned short slotNameID;
  unsigned long networkTest,
    nextLevel, lastLevel, leftNode, rightNode, alphaNode;
} BSAVE_OBJECT_PATTERN_NODE;

typedef struct bsaveObjectAlphaNode
{
  struct bsavePatternNodeHeader header;
  unsigned long classbmp, slotbmp, patternNode, nxtInGroup, nxtTe_rminal;
} BSAVE_OBJECT_ALPHA_NODE;

typedef struct bsaveClassAlphaLink
{
  unsigned long alphaNode;
  unsigned long next;
} BSAVE_CLASS_ALPHA_LINK;

#define CL_BsaveObjectPatternIndex(op) ((op != NULL) ? op->bsaveID : ULONG_MAX)
#define CL_BsaveObjectAlphaIndex(ap)   ((ap != NULL) ? ap->bsaveID : ULONG_MAX)
#define CL_BsaveClassAlphaIndex(cp)   ((cp != NULL) ? cp->bsaveID : ULONG_MAX)

#define ObjectPatternPointer(i) ((i == ULONG_MAX) ? NULL : (OBJECT_PATTERN_NODE *) &ObjectReteBinaryData(theEnv)->PatternArray[i])
#define ObjectAlphaPointer(i)   ((i == ULONG_MAX) ? NULL : (OBJECT_ALPHA_NODE *) &ObjectReteBinaryData(theEnv)->AlphaArray[i])

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if BLOAD_AND_BSAVE
static void CL_BsaveObjectPatternsFind (Environment *);
static void MarkDefclassItems (Environment *, struct constructHeader *,
			       void *);
static void CL_BsaveStorageObjectPatterns (Environment *, FILE *);
static void CL_BsaveObjectPatterns (Environment *, FILE *);
static void CL_BsaveAlphaLinks (Environment *, struct constructHeader *,
				void *);
#endif
static void CL_BloadStorageObjectPatterns (Environment *);
static void CL_BloadObjectPatterns (Environment *);
static void UpdateAlpha (Environment *, void *, unsigned long);
static void UpdatePattern (Environment *, void *, unsigned long);
static void UpdateLink (Environment *, void *, unsigned long);
static void CL_Clear_BloadObjectPatterns (Environment *);
static void DeallocateObjectReteBinaryData (Environment *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***********************************************************
  NAME         : SetupObjectPatterns_Bload
  DESCRIPTION  : Initializes data structures and
                   routines for binary loads of
                   generic function constructs
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Routines defined and structures initialized
  NOTES        : None
 ***********************************************************/
void
SetupObjectPatterns_Bload (Environment * theEnv)
{
  CL_AllocateEnvironmentData (theEnv, OBJECTRETEBIN_DATA,
			      sizeof (struct objectReteBinaryData),
			      DeallocateObjectReteBinaryData);

#if BLOAD_AND_BSAVE
  CL_AddBinaryItem (theEnv, "object patterns", 0, CL_BsaveObjectPatternsFind,
		    NULL, CL_BsaveStorageObjectPatterns,
		    CL_BsaveObjectPatterns, CL_BloadStorageObjectPatterns,
		    CL_BloadObjectPatterns, CL_Clear_BloadObjectPatterns);
#endif
#if BLOAD || BLOAD_ONLY
  CL_AddBinaryItem (theEnv, "object patterns", 0, NULL, NULL, NULL, NULL,
		    CL_BloadStorageObjectPatterns, CL_BloadObjectPatterns,
		    CL_Clear_BloadObjectPatterns);
#endif
}

/***********************************************************/
/* DeallocateObjectReteBinaryData: Deallocates environment */
/*    data for object rete binary functionality.           */
/***********************************************************/
static void
DeallocateObjectReteBinaryData (Environment * theEnv)
{
#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)
  size_t space;
  unsigned long i;

  for (i = 0; i < ObjectReteBinaryData (theEnv)->AlphaNodeCount; i++)
    {
      CL_DestroyAlphaMemory (theEnv,
			     &ObjectReteBinaryData (theEnv)->AlphaArray[i].
			     header, false);
    }

  space =
    ObjectReteBinaryData (theEnv)->AlphaNodeCount *
    sizeof (struct objectAlphaNode);
  if (space != 0)
    CL_genfree (theEnv, ObjectReteBinaryData (theEnv)->AlphaArray, space);

  space =
    ObjectReteBinaryData (theEnv)->PatternNodeCount *
    sizeof (struct objectPatternNode);
  if (space != 0)
    CL_genfree (theEnv, ObjectReteBinaryData (theEnv)->PatternArray, space);

  space =
    ObjectReteBinaryData (theEnv)->AlphaLinkCount *
    sizeof (struct classAlphaLink);
  if (space != 0)
    CL_genfree (theEnv, ObjectReteBinaryData (theEnv)->AlphaLinkArray, space);
#endif
}

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if BLOAD_AND_BSAVE

/***************************************************
  NAME         : CL_BsaveObjectPatternsFind
  DESCRIPTION  : Sets the CL_Bsave IDs for the object
                 pattern data structures and
                 dete_rmines how much space
                 (including padding) is necessary
                 for the alpha node bitmPS
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Counts written
  NOTES        : None
 ***************************************************/
static void
CL_BsaveObjectPatternsFind (Environment * theEnv)
{
  OBJECT_ALPHA_NODE *alphaPtr;
  OBJECT_PATTERN_NODE *patternPtr;

  CL_Save_BloadCount (theEnv, ObjectReteBinaryData (theEnv)->AlphaNodeCount);
  CL_Save_BloadCount (theEnv,
		      ObjectReteBinaryData (theEnv)->PatternNodeCount);
  CL_Save_BloadCount (theEnv, ObjectReteBinaryData (theEnv)->AlphaLinkCount);

  ObjectReteBinaryData (theEnv)->AlphaLinkCount = 0L;
  CL_DoForAllConstructs (theEnv, MarkDefclassItems,
			 DefclassData (theEnv)->CL_DefclassModuleIndex, false,
			 NULL);

  ObjectReteBinaryData (theEnv)->AlphaNodeCount = 0L;
  alphaPtr = CL_ObjectNetworkTe_rminalPointer (theEnv);
  while (alphaPtr != NULL)
    {
      alphaPtr->classbmp->neededBitMap = true;
      if (alphaPtr->slotbmp != NULL)
	alphaPtr->slotbmp->neededBitMap = true;
      alphaPtr->bsaveID = ObjectReteBinaryData (theEnv)->AlphaNodeCount++;
      alphaPtr = alphaPtr->nxtTe_rminal;
    }

  ObjectReteBinaryData (theEnv)->PatternNodeCount = 0L;
  patternPtr = CL_ObjectNetworkPointer (theEnv);
  while (patternPtr != NULL)
    {
      patternPtr->bsaveID = ObjectReteBinaryData (theEnv)->PatternNodeCount++;
      if (patternPtr->nextLevel == NULL)
	{
	  while (patternPtr->rightNode == NULL)
	    {
	      patternPtr = patternPtr->lastLevel;
	      if (patternPtr == NULL)
		return;
	    }
	  patternPtr = patternPtr->rightNode;
	}
      else
	patternPtr = patternPtr->nextLevel;
    }
}

/***************************************************
  NAME         : MarkDefclassItems
  DESCRIPTION  : Marks needed items for a defclass
                 using rules
  INPUTS       : 1) The defclass
                 2) User buffer (ignored)
  RETURNS      : Nothing useful
  SIDE EFFECTS : CL_Bsave indices set
  NOTES        : None
 ***************************************************/
static void
MarkDefclassItems (Environment * theEnv,
		   struct constructHeader *theDefclass, void *buf)
{
#if MAC_XCD
#pragma unused(buf)
#endif
  Defclass *cls = (Defclass *) theDefclass;
  CLASS_ALPHA_LINK *alphaLink;

  for (alphaLink = cls->relevant_te_rminal_alpha_nodes;
       alphaLink != NULL; alphaLink = alphaLink->next)
    {
      alphaLink->bsaveID = ObjectReteBinaryData (theEnv)->AlphaLinkCount++;
    }
}

/****************************************************
  NAME         : CL_BsaveStorageObjectPatterns
  DESCRIPTION  : CL_Writes out the number of bytes
                 required for object pattern bitmaps,
                 and the number of object pattern
                 alpha an inte_rmediate nodes
  INPUTS       : CL_Bsave file stream pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Counts written
  NOTES        : None
 ****************************************************/
static void
CL_BsaveStorageObjectPatterns (Environment * theEnv, FILE * fp)
{
  size_t space;

  space = sizeof (long) * 3;
  CL_Gen_Write (&space, sizeof (size_t), fp);
  CL_Gen_Write (&ObjectReteBinaryData (theEnv)->AlphaNodeCount, sizeof (long),
		fp);
  CL_Gen_Write (&ObjectReteBinaryData (theEnv)->PatternNodeCount,
		sizeof (long), fp);
  CL_Gen_Write (&ObjectReteBinaryData (theEnv)->AlphaLinkCount, sizeof (long),
		fp);
}

/***************************************************
  NAME         : CL_BsaveObjectPatterns
  DESCRIPTION  : CL_Writes out object pattern data
                 structures to binary save file
  INPUTS       : CL_Bsave file stream pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Data structures written
  NOTES        : Extra padding written with alpha
                 node bitmaps to ensure correct
                 alignment of structues on bload
 ***************************************************/
static void
CL_BsaveObjectPatterns (Environment * theEnv, FILE * fp)
{
  size_t space;
  OBJECT_ALPHA_NODE *alphaPtr;
  OBJECT_PATTERN_NODE *patternPtr;
  BSAVE_OBJECT_ALPHA_NODE dummyAlpha;
  BSAVE_OBJECT_PATTERN_NODE dummyPattern;

  space =
    (sizeof (BSAVE_OBJECT_ALPHA_NODE) *
     ObjectReteBinaryData (theEnv)->AlphaNodeCount) +
    (sizeof (BSAVE_OBJECT_PATTERN_NODE) *
     ObjectReteBinaryData (theEnv)->PatternNodeCount) +
    (sizeof (BSAVE_CLASS_ALPHA_LINK) *
     ObjectReteBinaryData (theEnv)->AlphaLinkCount);
  CL_Gen_Write (&space, sizeof (size_t), fp);

  CL_DoForAllConstructs (theEnv, CL_BsaveAlphaLinks,
			 DefclassData (theEnv)->CL_DefclassModuleIndex, false,
			 fp);

  /* ==========================================
     CL_Write out the alpha te_rminal pattern nodes
     ========================================== */
  alphaPtr = CL_ObjectNetworkTe_rminalPointer (theEnv);
  while (alphaPtr != NULL)
    {
      CL_Assign_BsavePatternHeaderValues (theEnv, &dummyAlpha.header,
					  &alphaPtr->header);
      dummyAlpha.classbmp = alphaPtr->classbmp->bucket;
      if (alphaPtr->slotbmp != NULL)
	dummyAlpha.slotbmp = alphaPtr->slotbmp->bucket;
      else
	dummyAlpha.slotbmp = ULONG_MAX;
      dummyAlpha.patternNode =
	CL_BsaveObjectPatternIndex (alphaPtr->patternNode);
      dummyAlpha.nxtInGroup = CL_BsaveObjectAlphaIndex (alphaPtr->nxtInGroup);
      dummyAlpha.nxtTe_rminal =
	CL_BsaveObjectAlphaIndex (alphaPtr->nxtTe_rminal);
      CL_Gen_Write (&dummyAlpha, sizeof (BSAVE_OBJECT_ALPHA_NODE), fp);
      alphaPtr = alphaPtr->nxtTe_rminal;
    }

  /* ========================================
     CL_Write out the inte_rmediate pattern nodes
     ======================================== */
  patternPtr = CL_ObjectNetworkPointer (theEnv);
  while (patternPtr != NULL)
    {
      dummyPattern.multifieldNode = patternPtr->multifieldNode;
      dummyPattern.whichField = patternPtr->whichField;
      dummyPattern.leaveFields = patternPtr->leaveFields;
      dummyPattern.endSlot = patternPtr->endSlot;
      dummyPattern.selector = patternPtr->selector;
      dummyPattern.slotNameID = patternPtr->slotNameID;
      dummyPattern.networkTest =
	CL_HashedExpressionIndex (theEnv, patternPtr->networkTest);
      dummyPattern.nextLevel =
	CL_BsaveObjectPatternIndex (patternPtr->nextLevel);
      dummyPattern.lastLevel =
	CL_BsaveObjectPatternIndex (patternPtr->lastLevel);
      dummyPattern.leftNode =
	CL_BsaveObjectPatternIndex (patternPtr->leftNode);
      dummyPattern.rightNode =
	CL_BsaveObjectPatternIndex (patternPtr->rightNode);
      dummyPattern.alphaNode =
	CL_BsaveObjectAlphaIndex (patternPtr->alphaNode);
      CL_Gen_Write (&dummyPattern, sizeof (BSAVE_OBJECT_PATTERN_NODE), fp);

      if (patternPtr->nextLevel == NULL)
	{
	  while (patternPtr->rightNode == NULL)
	    {
	      patternPtr = patternPtr->lastLevel;
	      if (patternPtr == NULL)
		{
		  Restore_BloadCount (theEnv,
				      &ObjectReteBinaryData
				      (theEnv)->AlphaNodeCount);
		  Restore_BloadCount (theEnv,
				      &ObjectReteBinaryData
				      (theEnv)->PatternNodeCount);
		  Restore_BloadCount (theEnv,
				      &ObjectReteBinaryData
				      (theEnv)->AlphaLinkCount);
		  return;
		}
	    }
	  patternPtr = patternPtr->rightNode;
	}
      else
	patternPtr = patternPtr->nextLevel;
    }

  Restore_BloadCount (theEnv, &ObjectReteBinaryData (theEnv)->AlphaNodeCount);
  Restore_BloadCount (theEnv,
		      &ObjectReteBinaryData (theEnv)->PatternNodeCount);
  Restore_BloadCount (theEnv, &ObjectReteBinaryData (theEnv)->AlphaLinkCount);
}

/***************************************************
  NAME         : CL_BsaveAlphaLinks
  DESCRIPTION  : CL_Writes class alpha links binary data
  INPUTS       : 1) The defclass
                 2) The binary file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defclass alpha links binary data written
  NOTES        : None
 ***************************************************/
static void
CL_BsaveAlphaLinks (Environment * theEnv,
		    struct constructHeader *theDefclass, void *buf)
{
  Defclass *cls = (Defclass *) theDefclass;
  BSAVE_CLASS_ALPHA_LINK dummy_alpha_link;
  CLASS_ALPHA_LINK *alphaLink;

  for (alphaLink = cls->relevant_te_rminal_alpha_nodes;
       alphaLink != NULL; alphaLink = alphaLink->next)
    {
      dummy_alpha_link.alphaNode = alphaLink->alphaNode->bsaveID;

      if (alphaLink->next != NULL)
	{
	  dummy_alpha_link.next = alphaLink->next->bsaveID;
	}
      else
	{
	  dummy_alpha_link.next = ULONG_MAX;
	}

      CL_Gen_Write ((void *) &dummy_alpha_link,
		    sizeof (BSAVE_CLASS_ALPHA_LINK), (FILE *) buf);
    }
}

#endif

/***************************************************
  NAME         : CL_BloadStorageObjectPatterns
  DESCRIPTION  : Reads in the storage requirements
                 for the object patterns in this
                 bload image
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Counts read and arrays allocated
  NOTES        : None
 ***************************************************/
static void
CL_BloadStorageObjectPatterns (Environment * theEnv)
{
  size_t space;
  unsigned long counts[3];

  CL_GenReadBinary (theEnv, &space, sizeof (size_t));
  CL_GenReadBinary (theEnv, counts, space);
  ObjectReteBinaryData (theEnv)->AlphaNodeCount = counts[0];
  ObjectReteBinaryData (theEnv)->PatternNodeCount = counts[1];
  ObjectReteBinaryData (theEnv)->AlphaLinkCount = counts[2];

  if (ObjectReteBinaryData (theEnv)->AlphaNodeCount == 0L)
    ObjectReteBinaryData (theEnv)->AlphaArray = NULL;
  else
    {
      space =
	(ObjectReteBinaryData (theEnv)->AlphaNodeCount *
	 sizeof (OBJECT_ALPHA_NODE));
      ObjectReteBinaryData (theEnv)->AlphaArray =
	(OBJECT_ALPHA_NODE *) CL_genalloc (theEnv, space);
    }

  if (ObjectReteBinaryData (theEnv)->PatternNodeCount == 0L)
    ObjectReteBinaryData (theEnv)->PatternArray = NULL;
  else
    {
      space =
	(ObjectReteBinaryData (theEnv)->PatternNodeCount *
	 sizeof (OBJECT_PATTERN_NODE));
      ObjectReteBinaryData (theEnv)->PatternArray =
	(OBJECT_PATTERN_NODE *) CL_genalloc (theEnv, space);
    }

  if (ObjectReteBinaryData (theEnv)->AlphaLinkCount == 0L)
    ObjectReteBinaryData (theEnv)->AlphaLinkArray = NULL;
  else
    {
      space =
	(ObjectReteBinaryData (theEnv)->AlphaLinkCount *
	 sizeof (CLASS_ALPHA_LINK));
      ObjectReteBinaryData (theEnv)->AlphaLinkArray =
	(CLASS_ALPHA_LINK *) CL_genalloc (theEnv, space);
    }
}

/****************************************************
  NAME         : CL_BloadObjectPatterns
  DESCRIPTION  : Reads in all object pattern
                 data structures from binary
                 image and updates pointers
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary data structures updated
  NOTES        : Assumes storage allocated previously
 ****************************************************/
static void
CL_BloadObjectPatterns (Environment * theEnv)
{
  size_t space;
  unsigned long i;

  CL_GenReadBinary (theEnv, &space, sizeof (size_t));
  if (space == 0L)
    return;

  /* ================================================
     Read in the alpha and inte_rmediate pattern nodes
     ================================================ */

  CL_Bloadand_Refresh (theEnv, ObjectReteBinaryData (theEnv)->AlphaLinkCount,
		       sizeof (BSAVE_CLASS_ALPHA_LINK), UpdateLink);
  CL_Bloadand_Refresh (theEnv, ObjectReteBinaryData (theEnv)->AlphaNodeCount,
		       sizeof (BSAVE_OBJECT_ALPHA_NODE), UpdateAlpha);
  CL_Bloadand_Refresh (theEnv,
		       ObjectReteBinaryData (theEnv)->PatternNodeCount,
		       sizeof (BSAVE_OBJECT_PATTERN_NODE), UpdatePattern);

  for (i = 0; i < ObjectReteBinaryData (theEnv)->PatternNodeCount; i++)
    {
      if ((ObjectReteBinaryData (theEnv)->PatternArray[i].lastLevel != NULL)
	  && (ObjectReteBinaryData (theEnv)->PatternArray[i].lastLevel->
	      selector))
	{
	  CL_AddHashedPatternNode (theEnv,
				   ObjectReteBinaryData (theEnv)->PatternArray
				   [i].lastLevel,
				   &ObjectReteBinaryData
				   (theEnv)->PatternArray[i],
				   ObjectReteBinaryData (theEnv)->PatternArray
				   [i].networkTest->type,
				   ObjectReteBinaryData (theEnv)->PatternArray
				   [i].networkTest->value);
	}
    }

  /* =======================
     Set the global pointers
     ======================= */
  SetCL_ObjectNetworkTe_rminalPointer (theEnv,
				       (OBJECT_ALPHA_NODE *) &
				       ObjectReteBinaryData
				       (theEnv)->AlphaArray[0]);
  Set_ObjectNetworkPointer (theEnv,
			    (OBJECT_PATTERN_NODE *) &
			    ObjectReteBinaryData (theEnv)->PatternArray[0]);
}

/***************************************************
  NAME         : UpdateAlpha
  DESCRIPTION  : Updates all the pointers for an
                 alpha node based on the binary
                 image indices
  INPUTS       : 1) A pointer to the binary
                    image alpha node buffer
                 2) The index of the actual
                    alpha node in the array
  RETURNS      : Nothing useful
  SIDE EFFECTS : Alpha node updated
  NOTES        : None
 ***************************************************/
static void
UpdateAlpha (Environment * theEnv, void *buf, unsigned long obji)
{
  BSAVE_OBJECT_ALPHA_NODE *bap;
  OBJECT_ALPHA_NODE *ap;

  bap = (BSAVE_OBJECT_ALPHA_NODE *) buf;
  ap =
    (OBJECT_ALPHA_NODE *) & ObjectReteBinaryData (theEnv)->AlphaArray[obji];

  CL_UpdatePatternNodeHeader (theEnv, &ap->header, &bap->header);
  ap->matchTimeTag = 0L;
  ap->classbmp = BitMapPointer (bap->classbmp);
  if (bap->slotbmp != ULONG_MAX)
    {
      ap->slotbmp = BitMapPointer (bap->slotbmp);
      IncrementBitMapCount (ap->slotbmp);
    }
  else
    ap->slotbmp = NULL;
  IncrementBitMapCount (ap->classbmp);
  ap->patternNode = ObjectPatternPointer (bap->patternNode);
  ap->nxtInGroup = ObjectAlphaPointer (bap->nxtInGroup);
  ap->nxtTe_rminal = ObjectAlphaPointer (bap->nxtTe_rminal);
  ap->bsaveID = 0L;
}

/***************************************************
  NAME         : UpdatePattern
  DESCRIPTION  : Updates all the pointers for a
                 pattern node based on the binary
                 image indices
  INPUTS       : 1) A pointer to the binary
                    image pattern node buffer
                 2) The index of the actual
                    pattern node in the array
  RETURNS      : Nothing useful
  SIDE EFFECTS : Pattern node updated
  NOTES        : None
 ***************************************************/
static void
UpdatePattern (Environment * theEnv, void *buf, unsigned long obji)
{
  BSAVE_OBJECT_PATTERN_NODE *bop;
  OBJECT_PATTERN_NODE *op;

  bop = (BSAVE_OBJECT_PATTERN_NODE *) buf;
  op =
    (OBJECT_PATTERN_NODE *) &
    ObjectReteBinaryData (theEnv)->PatternArray[obji];

  op->blocked = false;
  op->multifieldNode = bop->multifieldNode;
  op->whichField = bop->whichField;
  op->leaveFields = bop->leaveFields;
  op->endSlot = bop->endSlot;
  op->selector = bop->selector;
  op->matchTimeTag = 0L;
  op->slotNameID = bop->slotNameID;
  op->networkTest = HashedExpressionPointer (bop->networkTest);
  op->nextLevel = ObjectPatternPointer (bop->nextLevel);
  op->lastLevel = ObjectPatternPointer (bop->lastLevel);
  op->leftNode = ObjectPatternPointer (bop->leftNode);
  op->rightNode = ObjectPatternPointer (bop->rightNode);
  op->alphaNode = ObjectAlphaPointer (bop->alphaNode);
  op->bsaveID = 0L;
}

/***************************************************
  NAME         : UpdateLink
  DESCRIPTION  : Updates all the pointers for a
                 pattern node based on the binary
                 image indices
  INPUTS       : 1) A pointer to the binary
                    image pattern node buffer
                 2) The index of the actual
                    pattern node in the array
  RETURNS      : Nothing useful
  SIDE EFFECTS : Pattern node updated
  NOTES        : None
 ***************************************************/
static void
UpdateLink (Environment * theEnv, void *buf, unsigned long obji)
{
  BSAVE_CLASS_ALPHA_LINK *bal;
  CLASS_ALPHA_LINK *al;

  bal = (BSAVE_CLASS_ALPHA_LINK *) buf;
  al =
    (CLASS_ALPHA_LINK *) &
    ObjectReteBinaryData (theEnv)->AlphaLinkArray[obji];

  al->alphaNode = ObjectAlphaPointer (bal->alphaNode);
  al->next = ClassAlphaPointer (bal->next);
  al->bsaveID = 0L;
}

/***************************************************
  NAME         : CL_Clear_BloadObjectPatterns
  DESCRIPTION  : CL_Releases all emmory associated
                 with binary image object patterns
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Memory released and global
                 network pointers set to NULL
  NOTES        : None
 ***************************************************/
static void
CL_Clear_BloadObjectPatterns (Environment * theEnv)
{
  size_t space;
  unsigned long i;

  for (i = 0; i < ObjectReteBinaryData (theEnv)->PatternNodeCount; i++)
    {
      if ((ObjectReteBinaryData (theEnv)->PatternArray[i].lastLevel != NULL)
	  && (ObjectReteBinaryData (theEnv)->PatternArray[i].lastLevel->
	      selector))
	{
	  CL_RemoveHashedPatternNode (theEnv,
				      ObjectReteBinaryData
				      (theEnv)->PatternArray[i].lastLevel,
				      &ObjectReteBinaryData
				      (theEnv)->PatternArray[i],
				      ObjectReteBinaryData
				      (theEnv)->PatternArray[i].
				      networkTest->type,
				      ObjectReteBinaryData (theEnv)->
				      PatternArray[i].networkTest->value);
	}
    }

  /* ================================================
     All instances have been deleted by this point
     so we don't need to worry about clearing partial
     matches
     ================================================ */
  for (i = 0L; i < ObjectReteBinaryData (theEnv)->AlphaNodeCount; i++)
    {
      CL_DecrementBitMapReferenceCount (theEnv,
					ObjectReteBinaryData
					(theEnv)->AlphaArray[i].classbmp);
      if (ObjectReteBinaryData (theEnv)->AlphaArray[i].slotbmp != NULL)
	CL_DecrementBitMapReferenceCount (theEnv,
					  ObjectReteBinaryData
					  (theEnv)->AlphaArray[i].slotbmp);
    }

  if (ObjectReteBinaryData (theEnv)->AlphaNodeCount != 0L)
    {
      space =
	(ObjectReteBinaryData (theEnv)->AlphaNodeCount *
	 sizeof (OBJECT_ALPHA_NODE));
      CL_genfree (theEnv, ObjectReteBinaryData (theEnv)->AlphaArray, space);
      ObjectReteBinaryData (theEnv)->AlphaArray = NULL;
      ObjectReteBinaryData (theEnv)->AlphaNodeCount = 0;
      space =
	(ObjectReteBinaryData (theEnv)->PatternNodeCount *
	 sizeof (OBJECT_PATTERN_NODE));
      CL_genfree (theEnv, ObjectReteBinaryData (theEnv)->PatternArray, space);
      ObjectReteBinaryData (theEnv)->PatternArray = NULL;
      ObjectReteBinaryData (theEnv)->PatternNodeCount = 0;
      space =
	(ObjectReteBinaryData (theEnv)->AlphaLinkCount *
	 sizeof (CLASS_ALPHA_LINK));
      CL_genfree (theEnv,
		  (void *) ObjectReteBinaryData (theEnv)->AlphaLinkArray,
		  space);
      ObjectReteBinaryData (theEnv)->AlphaLinkArray = NULL;
      ObjectReteBinaryData (theEnv)->AlphaLinkCount = 0;
    }

  SetCL_ObjectNetworkTe_rminalPointer (theEnv, NULL);
  Set_ObjectNetworkPointer (theEnv, NULL);
#if BLOAD_ONLY
  CL_ResetObjectMatchTimeTags (theEnv);
#endif
}

#endif
