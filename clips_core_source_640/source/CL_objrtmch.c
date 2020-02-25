   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  02/03/18             */
   /*                                                     */
   /*          OBJECT PATTERN MATCHER MODULE              */
   /*******************************************************/

/**************************************************************/
/* Purpose: RETE Network Interface for Objects                */
/*                                                            */
/* Principal Programmer(s):                                   */
/*      Brian L. Dantes                                       */
/*                                                            */
/* Contributing Programmer(s):                                */
/*                                                            */
/* Revision History:                                          */
/*                                                            */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859   */
/*                                                            */
/*      6.24: Removed INCREMENTAL_RESET and                   */
/*            LOGICAL_DEPENDENCIES compilation flags.         */
/*                                                            */
/*            Converted INSTANCE_PATTERN_MATCHING to          */
/*            DEFRULE_CONSTRUCT.                              */
/*                                                            */
/*            Renamed BOOLEAN macro type to intBool.          */
/*                                                            */
/*      6.30: Modified the QueueObjectMatchAction function    */
/*            so that instance retract actions always occur   */
/*            before instance assert and modify actions.      */
/*            This prevents the pattern matching process      */
/*            from attempting the evaluation of a join        */
/*            expression that accesses the slots of a         */
/*            retracted instance.                             */
/*                                                            */
/*            Added support for hashed alpha memories.        */
/*                                                            */
/*            Support for long long integers.                 */
/*                                                            */
/*            Added support for hashed comparisons to         */
/*            constants.                                      */
/*                                                            */
/*      6.31: Optimization for marking relevant alpha nodes  */
/*            in the object pattern network.                 */
/*                                                           */
/*      6.40: Added Env prefix to Get_EvaluationError and      */
/*            Set_EvaluationError functions.                   */
/*                                                            */
/*            Added Env prefix to CL_Get_HaltExecution and        */
/*            Set_HaltExecution functions.                     */
/*                                                            */
/*            Pragma once and other inclusion changes.        */
/*                                                            */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/**************************************************************/
/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM

#include "classfun.h"
#include "memalloc.h"
#include "drive.h"
#include "engine.h"
#include "envrnmnt.h"
#if (! RUN_TIME) && (! BLOAD_ONLY)
#include "incrrset.h"
#endif
#include "lgcldpnd.h"
#include "multifld.h"
#include "objrtfnx.h"
#include "objrtmch.h"
#include "prntutil.h"
#include "reteutil.h"
#include "ruledlt.h"
#include "reorder.h"
#include "retract.h"
#include "router.h"

#include "insmngr.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void QueueObjectMatchAction (Environment *, int, Instance *, int);
static SLOT_BITMAP *QueueModifySlotMap (Environment *, SLOT_BITMAP *, int);
static void ReturnObjectMatchAction (Environment *, OBJECT_MATCH_ACTION *);
static void ProcessObjectMatchQueue (Environment *);
static void MarkObjectPatternNetwork (Environment *, SLOT_BITMAP *);
static bool CompareSlotBitMaps (SLOT_BITMAP *, SLOT_BITMAP *);
static void ObjectPatternMatch (Environment *, size_t, size_t,
				OBJECT_PATTERN_NODE *,
				struct multifieldMarker *);
static void ProcessPatternNode (Environment *, size_t, size_t,
				OBJECT_PATTERN_NODE *,
				struct multifieldMarker *);
static void CreateObjectAlphaMatch (Environment *, OBJECT_ALPHA_NODE *);
static bool CL_EvaluateObjectPatternTest (Environment *, size_t,
					  struct multifieldMarker *,
					  Expression *,
					  OBJECT_PATTERN_NODE *);
static void Object_AssertAction (Environment *, Instance *);
static void ObjectModifyAction (Environment *, Instance *, SLOT_BITMAP *);
static void Object_RetractAction (Environment *, Instance *, SLOT_BITMAP *);
static void ObjectPatternNetErrorMessage (Environment *,
					  OBJECT_PATTERN_NODE *);
static void TraceErrorToObjectPattern (Environment *, bool,
				       OBJECT_PATTERN_NODE *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************************************
  NAME         : CL_ObjectMatchDelay
  DESCRIPTION  : H/L interface for CL_SetDelayObjectPatternMatching
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : DelayObjectPatternMatching set and Rete network updates
                 delayed until pattern-matching is completed
  NOTES        : H/L Syntax: (object-pattern-match-delay <action>*)
 ***************************************************************************/
void
CL_ObjectMatchDelay (Environment * theEnv,
		     UDFContext * context, UDFValue * returnValue)
{
  bool ov;

  ov = CL_SetDelayObjectPatternMatching (theEnv, true);

  if (!CL_UDFFirstArgument (context, ANY_TYPE_BITS, returnValue))
    {
      return;
    }

  if (CL_EvaluationData (theEnv)->CL_EvaluationError)
    {
      Set_HaltExecution (theEnv, false);
      Set_EvaluationError (theEnv, false);
      CL_SetDelayObjectPatternMatching (theEnv, ov);
      Set_EvaluationError (theEnv, true);
    }
  else
    CL_SetDelayObjectPatternMatching (theEnv, ov);
}

/***************************************************
  NAME         : CL_SetDelayObjectPatternMatching
  DESCRIPTION  : Sets the flag dete_rmining if Rete
                 network activity is to be delayed
                 for objects or not
  INPUTS       : The value of the flag
  RETURNS      : The old value of the flag
  SIDE EFFECTS : DelayObjectPatternMatching set
  NOTES        : When the delay is set to false,
                 all pending Rete network updates
                 are perfo_rmed
 ***************************************************/
bool
CL_SetDelayObjectPatternMatching (Environment * theEnv, bool value)
{
  bool oldval;

  oldval = ObjectReteData (theEnv)->DelayObjectPatternMatching;
  if (value)
    ObjectReteData (theEnv)->DelayObjectPatternMatching = true;
  else
    {
      ObjectReteData (theEnv)->DelayObjectPatternMatching = false;
      CL_ObjectNetworkAction (theEnv, 0, NULL, -1);
    }
  return (oldval);
}

/***************************************************
  NAME         : CL_GetDelayObjectPatternMatching
  DESCRIPTION  : Gets the flag dete_rmining if Rete
                 network activity is to be delayed
                 for objects or not
  INPUTS       : None
  RETURNS      : The flag
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
bool
CL_GetDelayObjectPatternMatching (Environment * theEnv)
{
  return (ObjectReteData (theEnv)->DelayObjectPatternMatching);
}

/********************************************************
  NAME         : CL_ObjectNetworkPointer
  DESCRIPTION  : Returns the first object network
                 pattern node
  INPUTS       : None
  RETURNS      : The top of the object pattern network
  SIDE EFFECTS : None
  NOTES        : None
 ********************************************************/
OBJECT_PATTERN_NODE *
CL_ObjectNetworkPointer (Environment * theEnv)
{
  return (ObjectReteData (theEnv)->ObjectPatternNetworkPointer);
}

/********************************************************
  NAME         : CL_ObjectNetworkTe_rminalPointer
  DESCRIPTION  : Returns the first te_rminal pattern node
  INPUTS       : None
  RETURNS      : The last node of a pattern
  SIDE EFFECTS : None
  NOTES        : None
 ********************************************************/
OBJECT_ALPHA_NODE *
CL_ObjectNetworkTe_rminalPointer (Environment * theEnv)
{
  return (ObjectReteData (theEnv)->ObjectPatternNetworkTe_rminalPointer);
}

/***************************************************
  NAME         : Set_ObjectNetworkPointer
  DESCRIPTION  : Sets the object pattern network to
                  the given network
  INPUTS       : Top of the new pattern network
  RETURNS      : Nothing useful
  SIDE EFFECTS : ObjectPatternNetworkPointer set
  NOTES        : None
 ***************************************************/
void
Set_ObjectNetworkPointer (Environment * theEnv, OBJECT_PATTERN_NODE * value)
{
  ObjectReteData (theEnv)->ObjectPatternNetworkPointer = value;
}

/*******************************************************
  NAME         : SetCL_ObjectNetworkTe_rminalPointer
  DESCRIPTION  : Sets the global list of te_rminal
                 pattern nodes (the ones containing
                 the bitmaps) to the given node
  INPUTS       : The last node of a pattern
  RETURNS      : Nothing useful
  SIDE EFFECTS : ObjectPatternNetworkTe_rminalPointer set
  NOTES        : None
 *******************************************************/
void
SetCL_ObjectNetworkTe_rminalPointer (Environment * theEnv,
				     OBJECT_ALPHA_NODE * value)
{
  ObjectReteData (theEnv)->ObjectPatternNetworkTe_rminalPointer = value;
}

/************************************************************************
  NAME         : CL_ObjectNetworkAction
  DESCRIPTION  : Main driver for pattern-matching on objects
                 If the pattern-matching is current delayed or another
                 object is currently being pattern-matched, the requested
                 match action is queued for later processing.
                 Otherwise, the match action is perfo_rmed and the
                 Rete network is updated.
  INPUTS       : 1) The match action type
                    OBJECT_ASSERT  (1)
                    OBJECT_RETRACT (2)
                    OBJECT_MODIFY  (3)
                 2) The instance to be matched (can be NULL if only
                    want pending actions to be perfo_rmed)
                 3) The name id of the slot being updated (can be -1)
                    If this argument is -1, it is assumed that any
                    pattern which could match this instance must be
                    checked.  Otherwise, only the patterns which
                    explicitly match on the named slot will be checked.
  RETURNS      : Nothing useful
  SIDE EFFECTS : Action queued or Rete network updated
  NOTES        : None
 ************************************************************************/
void
CL_ObjectNetworkAction (Environment * theEnv,
			int type, Instance * ins, int slotNameID)
{
  SLOT_BITMAP *tmpMap;

  if (EngineData (theEnv)->JoinOperationInProgress)
    return;

  EngineData (theEnv)->JoinOperationInProgress = true;


  /* ================================================
     For purposes of conflict resolution, all objects
     which have had pattern-matching delayed will
     have the same relative timestamp, i.e., the
     inference engine thinks they all just appeared
     simultaneously

     When delay is off, however, each object gets the
     new and current timestamp as expected.
     ================================================ */
  ObjectReteData (theEnv)->UseEntityTimeTag =
    DefruleData (theEnv)->CurrentEntityTimeTag++;

  /* ==================================================
     If pattern-matching is delayed (by use of the
     set-object-pattern-match-delay function), then
     the instance should be marked for later processing
     (when the delay is turned off).
     ================================================== */
  if (ins != NULL)
    {
      /* 6.05 Bug Fix */
      ins->reteSynchronized = false;

      if (ObjectReteData (theEnv)->DelayObjectPatternMatching == false)
	switch (type)
	  {
	  case OBJECT_ASSERT:
	    Object_AssertAction (theEnv, ins);
	    break;
	  case OBJECT_RETRACT:
	    Object_RetractAction (theEnv, ins, NULL);
	    break;
	  default:
	    tmpMap = QueueModifySlotMap (theEnv, NULL, slotNameID);
	    ObjectModifyAction (theEnv, ins, tmpMap);
	    CL_rm (theEnv, tmpMap, SlotBitMapSize (tmpMap));
	  }
      else
	QueueObjectMatchAction (theEnv, type, ins, slotNameID);
    }

  /* ========================================
     Process all pending actions in the queue
     All updates will use the same timestamp
     ======================================== */
  ProcessObjectMatchQueue (theEnv);

  EngineData (theEnv)->JoinOperationInProgress = false;

  CL_ForceLogical_Retractions (theEnv);

   /*=========================================*/
  /* Free partial matches that were released */
  /* by the assertion of the fact.           */
   /*=========================================*/

  if (EngineData (theEnv)->ExecutingRule == NULL)
    CL_FlushGarbagePartial_Matches (theEnv);
}

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************
  NAME         : CL_ResetObjectMatchTimeTags
  DESCRIPTION  : If CurrentObjectMatchTimeTag + 1
                 would cause an overflow,
                 CurrentObjectMatchTimeTag
                 is reset to 0L and all time tags
                 in object pattern nodes are reset.
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : CurrentObjectMatchTimeTag reset to
                 0, and all match time tags reset
                 These tags are used to recognize
                 valid pattern nodes on a match
  NOTES        : None
 ***************************************************/
void
CL_ResetObjectMatchTimeTags (Environment * theEnv)
{
  OBJECT_ALPHA_NODE *alphaPtr;
  OBJECT_PATTERN_NODE *lastLevel;

  /* ============================================
     If the current tag incremented by one would
     not cause an overflow, then we can leave
     things alone.
     ============================================ */
  if ((ObjectReteData (theEnv)->CurrentObjectMatchTimeTag + 1L) >
      ObjectReteData (theEnv)->CurrentObjectMatchTimeTag)
    return;
  ObjectReteData (theEnv)->CurrentObjectMatchTimeTag = 0L;
  alphaPtr = CL_ObjectNetworkTe_rminalPointer (theEnv);
  while (alphaPtr != NULL)
    {
      alphaPtr->matchTimeTag = 0L;
      lastLevel = alphaPtr->patternNode;
      while (lastLevel != NULL)
	{
	  if (lastLevel->matchTimeTag == 0L)
	    break;
	  lastLevel->matchTimeTag = 0L;
	  lastLevel = lastLevel->lastLevel;
	}
      alphaPtr = alphaPtr->nxtTe_rminal;
    }
}

/***************************************************
  NAME         : QueueObjectMatchAction
  DESCRIPTION  : Posts a Rete network match event
                 for later processing
  INPUTS       : 1) The match action type
                    OBJECT_ASSERT  (1)
                    OBJECT_RETRACT (2)
                    OBJECT_MODIFY  (3)
                 2) The instance to be matched
                 3) The name id of the slot being
                    updated (can be -1)
  RETURNS      : Nothing useful
  SIDE EFFECTS : Queue updated
  NOTES        : None
 ***************************************************/
static void
QueueObjectMatchAction (Environment * theEnv,
			int type, Instance * ins, int slotNameID)
{
  OBJECT_MATCH_ACTION *prv, *cur, *newMatch;
  OBJECT_MATCH_ACTION *prv_Retract = NULL;	/* DR0873 */

  prv = NULL;
  cur = ObjectReteData (theEnv)->ObjectMatchActionQueue;
  while (cur != NULL)
    {
      /* ===========================================================
         Here are the possibilities for the first Rete event already
         on the queue as compared with the new event for an object:

         CL_Assert/CL_Retract  -->  Delete assert event
         Ignore retract event
         CL_Assert/Modify   -->  Ignore modify event
         Modify/Modify   -->  Merge new modify event
         Modify/CL_Retract  -->  Delete modify event
         Queue the retract event
         =========================================================== */
      if (cur->ins == ins)
	{
	  /* ===================================================
	     An action for initially asserting the newly created
	     object to all applicable patterns
	     =================================================== */
	  if (cur->type == OBJECT_ASSERT)
	    {
	      if (type == OBJECT_RETRACT)
		{
		  /* ===================================================
		     If we are retracting the entire object, then we can
		     remove the assert action (and all modifies as well)
		     and ignore the retract action
		     (basically the object came and went before the Rete
		     network had a chance to see it)
		     =================================================== */
		  if (prv == NULL)
		    ObjectReteData (theEnv)->ObjectMatchActionQueue =
		      cur->nxt;
		  else
		    prv->nxt = cur->nxt;
		  cur->ins->busy--;
		  ReturnObjectMatchAction (theEnv, cur);
		}

	      /* =================================================
	         If this is a modify action, then we can ignore it
	         since the assert action will encompass it
	         ================================================= */
	    }

	  /* ===================================================
	     If the object is being deleted after a slot modify,
	     drop the modify event and replace with the retract
	     =================================================== */
	  else if (type == OBJECT_RETRACT)
	    {
	      cur->type = OBJECT_RETRACT;
	      if (cur->slotNameIDs != NULL)
		{
		  CL_rm (theEnv, cur->slotNameIDs,
			 SlotBitMapSize (cur->slotNameIDs));
		  cur->slotNameIDs = NULL;
		}
	    }

	  /* ====================================================
	     If a modify event for this slot is already on the
	     queue, ignore this one. Otherwise, merge the slot id
	     ==================================================== */
	  else
	    cur->slotNameIDs =
	      QueueModifySlotMap (theEnv, cur->slotNameIDs, slotNameID);

	  return;
	}

      if (cur->type == OBJECT_RETRACT)	/* DR0873 */
	{
	  prv_Retract = cur;
	}			/* DR0873 */
      prv = cur;
      cur = cur->nxt;
    }

  /* ================================================
     If there are no actions for the instance already
     on the queue, the new action is simply appended.
     ================================================ */
  newMatch = get_struct (theEnv, objectMatchAction);
  newMatch->type = type;
  newMatch->nxt = NULL;		/* If we get here, cur should be NULL */
  newMatch->slotNameIDs = (type != OBJECT_MODIFY) ? NULL :
    QueueModifySlotMap (theEnv, NULL, slotNameID);
  newMatch->ins = ins;
  newMatch->ins->busy++;

  /* DR0873 Begin */
  /* CL_Retract operations must be processed before assert and   */
  /* modify actions, otherwise the pattern matching process   */
  /* might attempt to access the slots of a retract instance. */

  if (type == OBJECT_RETRACT)
    {
      if (prv_Retract == NULL)
	{
	  newMatch->nxt = ObjectReteData (theEnv)->ObjectMatchActionQueue;
	  ObjectReteData (theEnv)->ObjectMatchActionQueue = newMatch;
	}
      else
	{
	  newMatch->nxt = prv_Retract->nxt;
	  prv_Retract->nxt = newMatch;
	}
    }
  else
    /* DR0873 End */

  if (prv == NULL)
    ObjectReteData (theEnv)->ObjectMatchActionQueue = newMatch;
  else
    prv->nxt = newMatch;
}

/****************************************************
  NAME         : QueueModifySlotMap
  DESCRIPTION  : Sets the bitmap for a queued
                 object modify Rete network action
  INPUTS       : 1) The old bitmap (can be NULL)
                 2) The canonical slot id to set
  RETURNS      : The (new) bitmap
  SIDE EFFECTS : Bitmap allocated/reallocated if
                 necessary, and slot id bit set
  NOTES        : If the bitmap must be (re)allocated,
                 this routine allocates twice the
                 room necessary for the current id
                 to allow for growth.
 ****************************************************/
static SLOT_BITMAP *
QueueModifySlotMap (Environment * theEnv,
		    SLOT_BITMAP * oldMap, int slotNameID)
{
  SLOT_BITMAP *newMap;
  unsigned short newmaxid;
  unsigned oldsz, newsz;

  if ((oldMap == NULL) ? true : (slotNameID > oldMap->maxid))
    {
      newmaxid = (unsigned short) (slotNameID * 2);
      newsz = sizeof (SLOT_BITMAP) +
	(sizeof (char) * (newmaxid / BITS_PER_BYTE));
      newMap = (SLOT_BITMAP *) CL_gm2 (theEnv, newsz);
      CL_ClearBitString (newMap, newsz);
      if (oldMap != NULL)
	{
	  oldsz = SlotBitMapSize (oldMap);
	  GenCopyMemory (char, oldsz, newMap, oldMap);
	  CL_rm (theEnv, oldMap, oldsz);
	}
      newMap->maxid = newmaxid;
    }
  else
    newMap = oldMap;
  SetBitMap (newMap->map, slotNameID);
  return (newMap);
}

/***************************************************
  NAME         : ReturnObjectMatchAction
  DESCRIPTION  : Deallocates and object match action
                 structure and associated slot
                 bitmap (if any)
  INPUTS       : The queued match action item
  RETURNS      : Nothing useful
  SIDE EFFECTS : Object match action item deleted
  NOTES        : None
 ***************************************************/
static void
ReturnObjectMatchAction (Environment * theEnv, OBJECT_MATCH_ACTION * omaPtr)
{
  if (omaPtr->slotNameIDs != NULL)
    CL_rm (theEnv, omaPtr->slotNameIDs, SlotBitMapSize (omaPtr->slotNameIDs));
  rtn_struct (theEnv, objectMatchAction, omaPtr);
}

/***************************************************
  NAME         : ProcessObjectMatchQueue
  DESCRIPTION  : Processes all outstanding object
                 Rete network update events
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Pattern-matching on objects
  NOTES        : None
 ***************************************************/
static void
ProcessObjectMatchQueue (Environment * theEnv)
{
  OBJECT_MATCH_ACTION *cur;

  while ((ObjectReteData (theEnv)->ObjectMatchActionQueue != NULL) &&
	 (ObjectReteData (theEnv)->DelayObjectPatternMatching == false))
    {
      cur = ObjectReteData (theEnv)->ObjectMatchActionQueue;
      ObjectReteData (theEnv)->ObjectMatchActionQueue = cur->nxt;

      switch (cur->type)
	{
	case OBJECT_ASSERT:
	  Object_AssertAction (theEnv, cur->ins);
	  break;
	case OBJECT_RETRACT:
	  Object_RetractAction (theEnv, cur->ins, cur->slotNameIDs);
	  break;
	default:
	  ObjectModifyAction (theEnv, cur->ins, cur->slotNameIDs);
	}
      cur->ins->busy--;
      ReturnObjectMatchAction (theEnv, cur);
    }
}

/******************************************************
  NAME         : MarkObjectPatternNetwork
  DESCRIPTION  : Iterates through all te_rminal
                 pattern nodes checking class and
                 slot bitmaps.  If a pattern is
                 applicable to the object/slot change,
                 then all the nodes belonging to
                 the pattern are marked as needing
                 to be examined by the pattern matcher.
  INPUTS       : The bitmap of ids of the slots being
                 changed (NULL if this is an assert for the
                  for the entire object)
  RETURNS      : Nothing useful
  SIDE EFFECTS : Applicable pattern nodes marked
  NOTES        : Incremental reset status is also
                 checked here
 ******************************************************/
static void
MarkObjectPatternNetwork (Environment * theEnv, SLOT_BITMAP * slotNameIDs)
{
  OBJECT_ALPHA_NODE *alphaPtr;
  OBJECT_PATTERN_NODE *upper;
  Defclass *cls;
  CLASS_ALPHA_LINK *current_alpha_link;

  cls = ObjectReteData (theEnv)->CurrentPatternObject->cls;
  current_alpha_link = cls->relevant_te_rminal_alpha_nodes;

  CL_ResetObjectMatchTimeTags (theEnv);
  ObjectReteData (theEnv)->CurrentObjectMatchTimeTag++;

  while (current_alpha_link != NULL)
    {
      alphaPtr = current_alpha_link->alphaNode;

      /* =============================================================
         If an incremental reset is in progress, make sure that the
         pattern has been marked for initialization before proceeding.
         ============================================================= */
#if (! RUN_TIME) && (! BLOAD_ONLY)
      if (EngineData (theEnv)->CL_Incremental_ResetInProgress &&
	  (alphaPtr->header.initialize == false))
	{
	  current_alpha_link = current_alpha_link->next;
	  continue;
	}
#endif

      /* ===================================================
         If we are doing an assert, then we need to
         check all patterns which satsify the class bitmap
         (The retraction has already been done in this case)
         =================================================== */
      if (slotNameIDs == NULL)
	{
	  alphaPtr->matchTimeTag =
	    ObjectReteData (theEnv)->CurrentObjectMatchTimeTag;

	  for (upper = alphaPtr->patternNode;
	       upper != NULL; upper = upper->lastLevel)
	    {
	      if (upper->matchTimeTag ==
		  ObjectReteData (theEnv)->CurrentObjectMatchTimeTag)
		{
		  break;
		}
	      else
		{
		  upper->matchTimeTag =
		    ObjectReteData (theEnv)->CurrentObjectMatchTimeTag;
		}
	    }
	}

      /* ===================================================
         If we are doing a slot modify, then we need to
         check only the subset of patterns which satisfy the
         class bitmap AND actually match on the slot in
         question.
         =================================================== */
      else if (alphaPtr->slotbmp != NULL)
	{
	  if (CompareSlotBitMaps (slotNameIDs,
				  (SLOT_BITMAP *) alphaPtr->slotbmp->
				  contents))
	    {
	      alphaPtr->matchTimeTag =
		ObjectReteData (theEnv)->CurrentObjectMatchTimeTag;
	      for (upper = alphaPtr->patternNode; upper != NULL;
		   upper = upper->lastLevel)
		{
		  if (upper->matchTimeTag ==
		      ObjectReteData (theEnv)->CurrentObjectMatchTimeTag)
		    {
		      break;
		    }
		  else
		    {
		      upper->matchTimeTag =
			ObjectReteData (theEnv)->CurrentObjectMatchTimeTag;
		    }
		}
	    }
	}

      current_alpha_link = current_alpha_link->next;
    }
}

/***************************************************
  NAME         : CompareSlotBitMaps
  DESCRIPTION  : Compares two slot bitmaps by
                 bitwising and'ing byte per byte up
                 to the length of the smaller map.
  INPUTS       : The two slot bitmaps
  RETURNS      : True if any common bits
                 are set in both maps, false
                 otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
static bool
CompareSlotBitMaps (SLOT_BITMAP * smap1, SLOT_BITMAP * smap2)
{
  unsigned short i, maxByte;

  maxByte = (unsigned short)
    (((smap1->maxid < smap2->maxid) ?
      smap1->maxid : smap2->maxid) / BITS_PER_BYTE);
  for (i = 0; i <= maxByte; i++)
    if (smap1->map[i] & smap2->map[i])
      return true;
  return false;
}

/**********************************************************************************
  NAME         : ObjectPatternMatch
  DESCRIPTION  : Iterates through all the pattern nodes on one level
                 in the pattern network.  A node is only processed
                 if it can lead to a te_rminating class bitmap node
                 which applies to the object being matched.  This
                 allows for a significant reduction in the number of
                 patterns considered.
  INPUTS       : 1) The offset of the slot position from the pattern index
                 2) The pattern node being examined
                 3) The end of the list of multifield markers for the pattern
  RETURNS      : Nothing useful
  SIDE EFFECTS : The pattern tests are evaluated and the child nodes may
                 be processed (which may cause a whole series of Rete network
                 updates).
  NOTES        : Several globals are used to keep track of the current
                 slot being examined:
                 CurrentPatternMarks - the series of multifield markers
                 CurrentPatternObject - the object being pattern-matched
                 CurrentPatternObjectSlot - the current slot being examined
                 CurrentObjectSlotLength - the cardinality of the slot value

                 An optimization is perfo_rmed when evaluating
                 constant tests on a slot value field.  All
                 pattern nodes on a level which restrict the same slot
                 are grouped together.  Those which are constant
                 tests are placed at the far right.  Thus, as soon
                 as one of these constant tests succeeds, the re_maining
                 nodes for that slot on this level can be skipped
 **********************************************************************************/
static void
ObjectPatternMatch (Environment * theEnv,
		    size_t offset,
		    size_t multifieldsProcessed,
		    OBJECT_PATTERN_NODE * patternTop,
		    struct multifieldMarker *endMark)
{
  size_t saveSlotLength;
  InstanceSlot *saveSlot;
  OBJECT_PATTERN_NODE *blockedNode;

  while (patternTop != NULL)
    {
      /*=============================================================*/
      /* MarkObjectPatternNetwork() has already marked pattern nodes */
      /* which need processing according to the class bitmaps, slot  */
      /* updates and incremental reset status.                       */
      /*=============================================================*/

      if (patternTop->matchTimeTag ==
	  ObjectReteData (theEnv)->CurrentObjectMatchTimeTag)
	{
	 /*===========================================*/
	  /* Make sure we are examining the correct    */
	  /* slot of the object for this pattern node. */
	 /*===========================================*/

	  if ((patternTop->slotNameID == ISA_ID) ||
	      (patternTop->slotNameID == NAME_ID))
	    {
	      ObjectReteData (theEnv)->CurrentPatternObjectSlot = NULL;
	      ObjectReteData (theEnv)->CurrentObjectSlotLength = 1;
	      offset = 0;
	    }
	  else if ((ObjectReteData (theEnv)->CurrentPatternObjectSlot == NULL)
		   ? true : (ObjectReteData (theEnv)->
			     CurrentPatternObjectSlot->desc->slotName->id !=
			     patternTop->slotNameID))
	    {
	    /*=======================================================*/
	      /* Need to reset the indices for the multifield          */
	      /* markers now that we have moved onto a different slot. */
	    /*=======================================================*/

	      ObjectReteData (theEnv)->CurrentPatternObjectSlot =
		ObjectReteData (theEnv)->CurrentPatternObject->
		slotAddresses[ObjectReteData (theEnv)->CurrentPatternObject->
			      cls->slotNameMap[patternTop->slotNameID] - 1];
	      offset = 0;
	      if (ObjectReteData (theEnv)->CurrentPatternObjectSlot->desc->
		  multiple)
		ObjectReteData (theEnv)->CurrentObjectSlotLength =
		  ObjectReteData (theEnv)->CurrentPatternObjectSlot->
		  multifieldValue->length;
	      else
		ObjectReteData (theEnv)->CurrentObjectSlotLength = 1;
	    }

	 /*==========================================================*/
	  /* Process the pattern node.  If it is satisfied by the     */
	  /* the instance, ProcessPatternNode() will recursively pass */
	  /* all of its children nodes through ObjectPatternMatch().  */
	 /*==========================================================*/

	  saveSlotLength = ObjectReteData (theEnv)->CurrentObjectSlotLength;
	  saveSlot = ObjectReteData (theEnv)->CurrentPatternObjectSlot;
	  ProcessPatternNode (theEnv, offset, multifieldsProcessed,
			      patternTop, endMark);
	  ObjectReteData (theEnv)->CurrentObjectSlotLength = saveSlotLength;
	  ObjectReteData (theEnv)->CurrentPatternObjectSlot = saveSlot;
	}

      /*============================================================*/
      /* Move on to the siblings of this node - if the current node */
      /* was a constant test that succeeded, skip further sibling   */
      /* nodes (which test on the same field in the pattern) which  */
      /* match on the same slot since they are all constant tests   */
      /* as well and will, of course fail.                          */
      /*============================================================*/

      if (patternTop->blocked == true)
	{
	  patternTop->blocked = false;
	  blockedNode = patternTop;
	  patternTop = patternTop->rightNode;
	  while (patternTop != NULL)
	    {
	      if ((patternTop->slotNameID != blockedNode->slotNameID) ||
		  (patternTop->whichField != blockedNode->whichField))
		break;
	      patternTop = patternTop->rightNode;
	    }
	}
      else
	patternTop = patternTop->rightNode;
    }
}

/**********************************************************************************
  NAME         : ProcessPatternNode
  DESCRIPTION  : Dete_rmines if a pattern node satsifies the corresponding
                 slot value field(s) in an object.  If it does,
                 ObjectPatternMatch() is recursively called to process
                 the child nodes of this node.  In this mutual recursion
                 between ObjectPatternMatch() and ProcessPatternNode(),
                 the nodes of all applicable patterns are processed
                 to completion.  ObjectPatternMatch() enters an object
                 into a pattern's aplha memory when the traversal reaches
                 a te_rminal class bitmap node.
  INPUTS       : 1) The offset of the slot index from the pattern index
                 2) The pattern node being examined
                 3) The end of the list of multifield markers for the pattern
  RETURNS      : Nothing useful
  SIDE EFFECTS : The pattern tests are evaluated and the child nodes may
                 be processed (which may cause a whole series of Rete network
                 updates).
  NOTES        : Several globals are used to keep track of the current
                 slot being examined:
                 CurrentPatternMarks - the series of multifield markers
                 CurrentPatternObject - the object being pattern-matched
                 CurrentPatternObjectSlot - the current slot being examined
                 CurrentObjectSlotLength - the cardinality of the slot value
 **********************************************************************************/
static void
ProcessPatternNode (Environment * theEnv,
		    size_t offset,
		    size_t multifieldsProcessed,
		    OBJECT_PATTERN_NODE * patternNode,
		    struct multifieldMarker *endMark)
{
  unsigned short patternSlotField;
  size_t objectSlotField;
  size_t objectSlotLength;
  size_t repeatCount;
  InstanceSlot *objectSlot;
  struct multifieldMarker *newMark;
  UDFValue theResult;
  OBJECT_PATTERN_NODE *tempPtr;

  patternSlotField = patternNode->whichField;

  objectSlotField = patternSlotField + offset - multifieldsProcessed;

   /*============================================*/
  /* If this is a test on the class or the name */
  /* of the object, process it separately.      */
   /*============================================*/

  if (ObjectReteData (theEnv)->CurrentPatternObjectSlot == NULL)
    {
      if (patternNode->selector)	/* TBD Necessary? */
	{
	  if (CL_EvaluateObjectPatternTest
	      (theEnv, objectSlotField, NULL,
	       patternNode->networkTest->nextArg, patternNode))
	    {
	      CL_EvaluateExpression (theEnv, patternNode->networkTest,
				     &theResult);

	      tempPtr =
		(OBJECT_PATTERN_NODE *) CL_FindHashedPatternNode (theEnv,
								  patternNode,
								  theResult.
								  header->
								  type,
								  theResult.
								  value);

	      if (tempPtr != NULL)
		{
		  if (tempPtr->alphaNode != NULL)
		    {
		      CreateObjectAlphaMatch (theEnv, tempPtr->alphaNode);
		    }
		  ObjectPatternMatch (theEnv, offset, multifieldsProcessed,
				      tempPtr->nextLevel, endMark);
		}
	    }
	}
      else if ((patternNode->networkTest == NULL) ? true :
	       (CL_EvaluateObjectPatternTest (theEnv, objectSlotField, NULL,
					      (Expression *) patternNode->
					      networkTest, patternNode)))
	{
	  if (patternNode->alphaNode != NULL)
	    CreateObjectAlphaMatch (theEnv, patternNode->alphaNode);
	  ObjectPatternMatch (theEnv, offset, multifieldsProcessed,
			      patternNode->nextLevel, endMark);
	}
      return;
    }

   /*===================================*/
  /* Check a single-field restriction. */
   /*===================================*/

  if (patternNode->multifieldNode == 0)
    {
      if (patternNode->selector)
	{
	  if (CL_EvaluateObjectPatternTest
	      (theEnv, objectSlotField, NULL,
	       patternNode->networkTest->nextArg, patternNode))
	    {
	      CL_EvaluateExpression (theEnv, patternNode->networkTest,
				     &theResult);

	      tempPtr =
		(OBJECT_PATTERN_NODE *) CL_FindHashedPatternNode (theEnv,
								  patternNode,
								  theResult.
								  header->
								  type,
								  theResult.
								  value);

	      if (tempPtr != NULL)
		{
		  if (tempPtr->alphaNode != NULL)
		    {
		      CreateObjectAlphaMatch (theEnv, tempPtr->alphaNode);
		    }
		  ObjectPatternMatch (theEnv, offset, multifieldsProcessed,
				      tempPtr->nextLevel, endMark);
		}
	    }
	}
      else if ((patternNode->networkTest == NULL) ? true :
	       CL_EvaluateObjectPatternTest (theEnv, objectSlotField, NULL,
					     (Expression *) patternNode->
					     networkTest, patternNode))
	{
	  if (patternNode->alphaNode != NULL)
	    CreateObjectAlphaMatch (theEnv, patternNode->alphaNode);
	  ObjectPatternMatch (theEnv, offset, multifieldsProcessed,
			      patternNode->nextLevel, endMark);
	}
      return;
    }

   /*==============================================================*/
  /* Check a multifield restriction.  Add a marker for this field */
  /* which has indices indicating to which values in the object   */
  /* slot the multifield pattern node is bound.                   */
   /*==============================================================*/

  newMark = get_struct (theEnv, multifieldMarker);
  newMark->whichField = patternSlotField;
  newMark->where.whichSlot =
    (void *) ObjectReteData (theEnv)->CurrentPatternObjectSlot->desc->
    slotName->name;
  newMark->startPosition = objectSlotField;

  newMark->next = NULL;
  if (ObjectReteData (theEnv)->CurrentPatternObjectMarks == NULL)
    ObjectReteData (theEnv)->CurrentPatternObjectMarks = newMark;
  else
    endMark->next = newMark;

   /*============================================================*/
  /* If there are further pattern restrictions on this slot,    */
  /* try pattern-matching for all possible bound values of the  */
  /* multifield pattern node: from no values to all values from */
  /* the starting position of the multifield to the end of the  */
  /* object slot.  Otherwise, bind the multifield to all the    */
  /* re_maining fields in the slot value and continue with       */
  /* pattern-matching.                                          */
   /*============================================================*/

  if (patternNode->endSlot == false)
    {
      objectSlotLength = ObjectReteData (theEnv)->CurrentObjectSlotLength;
      objectSlot = ObjectReteData (theEnv)->CurrentPatternObjectSlot;
      newMark->range = 0;
      repeatCount =
	objectSlotLength + 2 - newMark->startPosition -
	patternNode->leaveFields;
      while (repeatCount > 0)
	{
	  if (patternNode->selector)
	    {
	      if (CL_EvaluateObjectPatternTest
		  (theEnv, objectSlotField, newMark,
		   patternNode->networkTest->nextArg, patternNode))
		{
		  CL_EvaluateExpression (theEnv, patternNode->networkTest,
					 &theResult);

		  tempPtr =
		    (OBJECT_PATTERN_NODE *) CL_FindHashedPatternNode (theEnv,
								      patternNode,
								      theResult.
								      header->
								      type,
								      theResult.
								      value);

		  if (tempPtr != NULL)
		    {
		      if (tempPtr->alphaNode != NULL)
			{
			  CreateObjectAlphaMatch (theEnv, tempPtr->alphaNode);
			}
		      ObjectPatternMatch (theEnv,
					  offset + newMark->startPosition +
					  newMark->range - objectSlotField,
					  multifieldsProcessed + 1,
					  tempPtr->nextLevel, newMark);
		      ObjectReteData (theEnv)->CurrentObjectSlotLength =
			objectSlotLength;
		      ObjectReteData (theEnv)->CurrentPatternObjectSlot =
			objectSlot;
		    }
		}
	    }
	  else if ((patternNode->networkTest == NULL) ? true :
		   CL_EvaluateObjectPatternTest (theEnv, objectSlotField,
						 newMark,
						 (Expression *) patternNode->
						 networkTest, patternNode))
	    {
	      if (patternNode->alphaNode != NULL)
		CreateObjectAlphaMatch (theEnv, patternNode->alphaNode);
	      ObjectPatternMatch (theEnv,
				  offset + newMark->startPosition +
				  newMark->range - objectSlotField,
				  multifieldsProcessed + 1,
				  patternNode->nextLevel, newMark);
	      ObjectReteData (theEnv)->CurrentObjectSlotLength =
		objectSlotLength;
	      ObjectReteData (theEnv)->CurrentPatternObjectSlot = objectSlot;
	    }
	  newMark->range++;
	  repeatCount--;
	}
    }
  else
    {
      newMark->range =
	ObjectReteData (theEnv)->CurrentObjectSlotLength + 1 -
	newMark->startPosition - patternNode->leaveFields;
      if (patternNode->selector)
	{
	  if (CL_EvaluateObjectPatternTest
	      (theEnv, objectSlotField, newMark,
	       patternNode->networkTest->nextArg, patternNode))
	    {
	      CL_EvaluateExpression (theEnv, patternNode->networkTest,
				     &theResult);

	      tempPtr =
		(OBJECT_PATTERN_NODE *) CL_FindHashedPatternNode (theEnv,
								  patternNode,
								  theResult.
								  header->
								  type,
								  theResult.
								  value);

	      if (tempPtr != NULL)
		{
		  if (tempPtr->alphaNode != NULL)
		    CreateObjectAlphaMatch (theEnv, tempPtr->alphaNode);
		  ObjectPatternMatch (theEnv, 0, 0, tempPtr->nextLevel,
				      newMark);
		}
	    }
	}
      else if ((patternNode->networkTest == NULL) ? true :
	       CL_EvaluateObjectPatternTest (theEnv, objectSlotField, newMark,
					     (Expression *) patternNode->
					     networkTest, patternNode))
	{
	  if (patternNode->alphaNode != NULL)
	    CreateObjectAlphaMatch (theEnv, patternNode->alphaNode);
	  ObjectPatternMatch (theEnv, 0, 0, patternNode->nextLevel, newMark);
	}
    }

   /*=========================================*/
  /* Delete the temporary multifield marker. */
   /*=========================================*/

  if (ObjectReteData (theEnv)->CurrentPatternObjectMarks == newMark)
    ObjectReteData (theEnv)->CurrentPatternObjectMarks = NULL;
  else
    endMark->next = NULL;
  rtn_struct (theEnv, multifieldMarker, newMark);
}

/***************************************************
  NAME         : CreateObjectAlphaMatch
  DESCRIPTION  : Places an instance in the alpha
                 memory of a pattern and drives the
                 partial match through the join
                 network
  INPUTS       : The alpha memory node
  RETURNS      : Nothing useful
  SIDE EFFECTS : Join network updated
  NOTES        : None
 ***************************************************/
static void
CreateObjectAlphaMatch (Environment * theEnv, OBJECT_ALPHA_NODE * alphaPtr)
{
  struct joinNode *listOfJoins;
  struct partialMatch *theMatch;
  struct patternMatch *newMatch;
  unsigned long hashValue;

  while (alphaPtr != NULL)
    {
      if (alphaPtr->matchTimeTag ==
	  ObjectReteData (theEnv)->CurrentObjectMatchTimeTag)
	{
	  hashValue = CL_ComputeRightHashValue (theEnv, &alphaPtr->header);

	  /* ===================================================
	     If we have reached the class bitmap of the pattern,
	     place the object in the alpha memory of each of
	     the te_rminal nodes underneath and drive
	     the partial matches through the join network.

	     Insert the instance into the alpha memory
	     of this pattern and mark it as busy
	     =================================================== */
	  ObjectReteData (theEnv)->CurrentPatternObject->busy++;
	  theMatch =
	    CL_CreateAlphaMatch (theEnv,
				 ObjectReteData (theEnv)->
				 CurrentPatternObject,
				 ObjectReteData (theEnv)->
				 CurrentPatternObjectMarks,
				 (struct patternNodeHeader *) alphaPtr,
				 hashValue);
	  theMatch->owner = alphaPtr;

	  /* ======================================
	     Attach the partial match to the object
	     to ease later retraction
	     ====================================== */
	  newMatch = get_struct (theEnv, patternMatch);
	  newMatch->next =
	    (struct patternMatch *) ObjectReteData (theEnv)->
	    CurrentPatternObject->partialMatchList;
	  newMatch->matchingPattern = (struct patternNodeHeader *) alphaPtr;
	  newMatch->theMatch = theMatch;
	  ObjectReteData (theEnv)->CurrentPatternObject->partialMatchList =
	    (void *) newMatch;

	  /* ================================================
	     Drive the partial match through the join network
	     ================================================ */
	  listOfJoins = alphaPtr->header.entryJoin;
	  while (listOfJoins != NULL)
	    {
	      Network_Assert (theEnv, theMatch, listOfJoins);
	      listOfJoins = listOfJoins->rightMatchNode;
	    }
	}
      alphaPtr = alphaPtr->nxtInGroup;
    }
}

/******************************************************
  NAME         : CL_EvaluateObjectPatternTest
  DESCRIPTION  : CL_Evaluates the pattern network test
                 expression for a node
  INPUTS       : 1) The actual index of the slot value
                    field currently being examined
                 2) The multifield marker (if any)
                    for the pattern node being ex_mained
                 3) The pattern network test expression
                 4) The pattern node being examined
  RETURNS      : True if the node passes the
                 test, false otherwise
  SIDE EFFECTS : CL_Evaluation of the test
                 CL_EvaluationError and CL_HaltExecution
                 are always set to false
  NOTES        : Assumes networkTest != NULL
 ******************************************************/
static bool
CL_EvaluateObjectPatternTest (Environment * theEnv,
			      size_t objectSlotField,
			      struct multifieldMarker *selfSlotMarker,
			      Expression * networkTest,
			      OBJECT_PATTERN_NODE * patternNode)
{
  UDFValue vresult;
  int rv;

  if (networkTest == NULL)
    return true;

  if (networkTest->type == OBJ_PN_CONSTANT)
    {
      struct expr *oldArgument;

      oldArgument = CL_EvaluationData (theEnv)->CurrentExpression;
      CL_EvaluationData (theEnv)->CurrentExpression = networkTest;
      rv =
	CL_ObjectCmpConstantFunction (theEnv, networkTest->value, &vresult);
      CL_EvaluationData (theEnv)->CurrentExpression = oldArgument;
      if (rv)
	{
	  if (((struct ObjectCmpPNConstant *)
	       networkTest->bitMapValue->contents)->pass)
	    patternNode->blocked = true;
	  return true;
	}
      return false;
    }

  /* =========================================================
     CL_Evaluate or expressions expressed in the fo_rmat:
     (or <expression 1> <expression 2> ... <expression n>)
     Returns true (1.0) if any of the expression are true,
     otherwise returns false (0.0).
     ========================================================= */
  if (networkTest->value == ExpressionData (theEnv)->PTR_OR)
    {
      networkTest = networkTest->argList;
      while (networkTest != NULL)
	{
	  if (CL_EvaluateObjectPatternTest
	      (theEnv, objectSlotField, selfSlotMarker, networkTest,
	       patternNode))
	    {
	      /* ============================================
	         A node can be blocked ONLY if there were one
	         positive constant test on that node
	         ============================================ */
	      patternNode->blocked = false;
	      return true;
	    }
	  patternNode->blocked = false;
	  networkTest = networkTest->nextArg;
	}
      return false;
    }

  /* ==========================================================
     CL_Evaluate and expressions expressed in the fo_rmat:
     (and <expression 1> <expression 2> ... <expression n>)
     Returns false if any of the expression are false,
     otherwise returns true.
     ========================================================== */
  else if (networkTest->value == ExpressionData (theEnv)->PTR_AND)
    {
      networkTest = networkTest->argList;
      while (networkTest != NULL)
	{
	  if (CL_EvaluateObjectPatternTest
	      (theEnv, objectSlotField, selfSlotMarker, networkTest,
	       patternNode) == false)
	    {
	      patternNode->blocked = false;
	      return false;
	    }
	  patternNode->blocked = false;
	  networkTest = networkTest->nextArg;
	}
      return true;
    }

  /* =======================================================
     CL_Evaluate all other expressions using CL_EvaluateExpression
     ======================================================= */
  else
    {
      CL_EvaluationData (theEnv)->CL_HaltExecution = false;
      if (CL_EvaluateExpression (theEnv, networkTest, &vresult))
	{
	  ObjectPatternNetErrorMessage (theEnv, patternNode);
	  CL_EvaluationData (theEnv)->CL_EvaluationError = false;
	  CL_EvaluationData (theEnv)->CL_HaltExecution = false;
	  return false;
	}
      if (vresult.value != FalseSymbol (theEnv))
	return true;
    }
  return false;
}

/***************************************************
  NAME         : Object_AssertAction
  DESCRIPTION  : Filters an instance through the
                 object pattern network
  INPUTS       : The instance
  RETURNS      : Nothing useful
  SIDE EFFECTS : Instance matched
  NOTES        : None
 ***************************************************/
static void
Object_AssertAction (Environment * theEnv, Instance * ins)
{
  ins->patternHeader.timeTag = ObjectReteData (theEnv)->UseEntityTimeTag;
  ObjectReteData (theEnv)->CurrentPatternObject = ins;
  ObjectReteData (theEnv)->CurrentPatternObjectSlot = NULL;
  MarkObjectPatternNetwork (theEnv, NULL);
  ObjectPatternMatch (theEnv, 0, 0, CL_ObjectNetworkPointer (theEnv), NULL);
  ins->reteSynchronized = true;
}

/**********************************************************************
  NAME         : ObjectModifyAction
  DESCRIPTION  : Removes an instance from patterns (and attached joins)
                 applicable to specified slot(s), and then filters
                 same instance through object pattern network
                 (only against patterns which explicitly match on
                 named slot(s))
  INPUTS       : 1) The instance
                 2) The bitmap of slot ids
  RETURNS      : Nothing useful
  SIDE EFFECTS : Instance retracted/asserted
  NOTES        : None
 **********************************************************************/
static void
ObjectModifyAction (Environment * theEnv,
		    Instance * ins, SLOT_BITMAP * slotNameIDs)
{
  ins->patternHeader.timeTag = ObjectReteData (theEnv)->UseEntityTimeTag;
  Object_RetractAction (theEnv, ins, slotNameIDs);
  ObjectReteData (theEnv)->CurrentPatternObject = ins;
  ObjectReteData (theEnv)->CurrentPatternObjectSlot = NULL;
  MarkObjectPatternNetwork (theEnv, slotNameIDs);
  ObjectPatternMatch (theEnv, 0, 0, CL_ObjectNetworkPointer (theEnv), NULL);
  ins->reteSynchronized = true;
}

/****************************************************
  NAME         : Object_RetractAction
  DESCRIPTION  : CL_Retracts the instance from the
                 applicable patterns for the object
                 (if the slotNameID != -1, then the
                  instance is only retracted from
                  the alpha memories of the patterns
                  which actually match on that slot)
  INPUTS       : 1) The instance
                 2) The slot bitmap for a modify
                    (NULL if the instance is actually
                     being removed)
  RETURNS      : Nothing useful
  SIDE EFFECTS : CL_Retractions perfo_rmed
  NOTES        : None
 ****************************************************/
static void
Object_RetractAction (Environment * theEnv,
		      Instance * ins, SLOT_BITMAP * slotNameIDs)
{
  struct patternMatch *prvMatch, *tmpMatch, *deleteMatch, *lastDeleteMatch;
  OBJECT_ALPHA_NODE *alphaPtr;
  void *save_Dependents;

  if (slotNameIDs == NULL)
    {
      if (ins->partialMatchList != NULL)
	{
	  tmpMatch = (struct patternMatch *) ins->partialMatchList;
	  while (tmpMatch != NULL)
	    {
	      ins->busy--;
	      tmpMatch = tmpMatch->next;
	    }
	  CL_Network_Retract (theEnv,
			      (struct patternMatch *) ins->partialMatchList);
	  ins->partialMatchList = NULL;
	}
    }
  else
    {
      deleteMatch = NULL;
      lastDeleteMatch = NULL;
      prvMatch = NULL;
      tmpMatch = (struct patternMatch *) ins->partialMatchList;
      while (tmpMatch != NULL)
	{
	  alphaPtr = (OBJECT_ALPHA_NODE *) tmpMatch->matchingPattern;
	  if (alphaPtr->slotbmp != NULL)
	    {
	      if (CompareSlotBitMaps (slotNameIDs,
				      (SLOT_BITMAP *) alphaPtr->slotbmp->
				      contents))
		{
		  ins->busy--;
		  if (prvMatch == NULL)
		    ins->partialMatchList = (void *) tmpMatch->next;
		  else
		    prvMatch->next = tmpMatch->next;
		  if (!deleteMatch)
		    deleteMatch = tmpMatch;
		  else
		    lastDeleteMatch->next = tmpMatch;
		  lastDeleteMatch = tmpMatch;
		  tmpMatch = tmpMatch->next;
		  lastDeleteMatch->next = NULL;
		}
	      else
		{
		  prvMatch = tmpMatch;
		  tmpMatch = tmpMatch->next;
		}
	    }
	  else
	    {
	      prvMatch = tmpMatch;
	      tmpMatch = tmpMatch->next;
	    }
	}

      /* =============================================
         We need to preserve any logical dependencies
         of this object and reattach them after doing
         the retract.  Otherwise, the Rete network
         will believe the object is gone and remove
         the links from the partial matches upon which
         this object is logically dependent.
         ============================================= */
      if (deleteMatch != NULL)
	{
	  save_Dependents = ins->patternHeader.dependents;
	  ins->patternHeader.dependents = NULL;
	  CL_Network_Retract (theEnv, deleteMatch);
	  ins->patternHeader.dependents = save_Dependents;
	}
    }
  ins->reteSynchronized = true;
}

/*****************************************************
  NAME         : ObjectPatternNetErrorMessage
  DESCRIPTION  : Prints out a locational error message
                 when an evaluation error occurs
                 during object pattern-matching
  INPUTS       : The pattern node
  RETURNS      : Nothing useful
  SIDE EFFECTS : Error message displayed
  NOTES        : None
 *****************************************************/
static void
ObjectPatternNetErrorMessage (Environment * theEnv,
			      OBJECT_PATTERN_NODE * patternPtr)
{
  CL_PrintErrorID (theEnv, "OBJRTMCH", 1, true);
  CL_WriteString (theEnv, STDERR,
		  "This error occurred in the object pattern network\n");
  CL_WriteString (theEnv, STDERR, "   Currently active instance: [");
  CL_WriteString (theEnv, STDERR,
		  ObjectReteData (theEnv)->CurrentPatternObject->name->
		  contents);
  CL_WriteString (theEnv, STDERR, "]\n");
  CL_WriteString (theEnv, STDERR, "   Problem resides in slot '");
  CL_WriteString (theEnv, STDERR,
		  CL_FindIDSlotName (theEnv,
				     patternPtr->slotNameID)->contents);
  CL_WriteString (theEnv, STDERR, "' field #");
  CL_PrintUnsignedInteger (theEnv, STDERR, patternPtr->whichField);
  CL_WriteString (theEnv, STDERR, "\n");
  TraceErrorToObjectPattern (theEnv, true, patternPtr);
  CL_WriteString (theEnv, STDERR, "\n");
}

/*********************************************************
  NAME         : TraceErrorToObjectPattern
  DESCRIPTION  : Used by ObjectPatternNetErrorMessage() to
                 print the rule(s) which contain an object
                 pattern.
  INPUTS       : 1) A flag indicating if this is the
                    node in which the error actually
                    occurred or not
                 2) The pattern node
  RETURNS      : Nothing useful
  SIDE EFFECTS : Error message displayed
  NOTES        : None
 *********************************************************/
static void
TraceErrorToObjectPattern (Environment * theEnv,
			   bool errorNode, OBJECT_PATTERN_NODE * patternPtr)
{
  struct joinNode *joinPtr;

  while (patternPtr != NULL)
    {
      if (patternPtr->alphaNode != NULL)
	{
	  joinPtr = patternPtr->alphaNode->header.entryJoin;
	  while (joinPtr != NULL)
	    {
	      CL_TraceErrorToRule (theEnv, joinPtr, "      ");
	      joinPtr = joinPtr->rightMatchNode;
	    }
	}
      TraceErrorToObjectPattern (theEnv, false, patternPtr->nextLevel);
      if (errorNode)
	break;
      patternPtr = patternPtr->rightNode;
    }

}

#endif
