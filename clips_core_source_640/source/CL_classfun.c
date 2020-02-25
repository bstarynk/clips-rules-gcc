   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  02/03/18             */
   /*                                                     */
   /*                CLASS FUNCTIONS MODULE               */
   /*******************************************************/

/*************************************************************/
/* Purpose: Internal class manipulation routines             */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove run-time program      */
/*            compiler warning.                              */
/*                                                           */
/*      6.30: Borland C (IBM_TBC) and Metrowerks CodeWarrior */
/*            (MAC_MCW, IBM_MCW) are no longer supported.    */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Used CL_genstrcpy and CL_genstrcat instead of strcpy */
/*            and strcat.                                    */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Fixed linkage issue when BLOAD_AND_SAVE        */
/*            compiler flag is set to 0.                     */
/*                                                           */
/*      6.31: Optimization of slot ID creation previously    */
/*            provided by NewSlotNameID function.            */
/*                                                           */
/*            Optimization for marking relevant alpha nodes  */
/*            in the object pattern network.                 */
/*                                                           */
/*      6.40: Added Env prefix to Get_EvaluationError and     */
/*            Set_EvaluationError functions.                  */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */

#include <stdlib.h>

#include "setup.h"

#if OBJECT_SYSTEM

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "bload.h"
#endif

#include "classcom.h"
#include "classini.h"
#include "constant.h"
#include "constrct.h"
#include "cstrccom.h"
#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "evaluatn.h"
#include "inscom.h"
#include "insfun.h"
#include "insmngr.h"
#include "memalloc.h"
#include "modulutl.h"
#include "msgfun.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"
#include "sysdep.h"
#include "utility.h"

#include "classfun.h"

/* =========================================
   *****************************************
                   CONSTANTS
   =========================================
   ***************************************** */
#define BIG_PRIME          11329

#define CLASS_ID_MAP_CHUNK 30

#define PUT_PREFIX         "put-"
#define PUT_PREFIX_LENGTH  4

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static unsigned int HashSlotName (CLIPSLexeme *);

#if (! RUN_TIME)
static void DeassignClassID (Environment *, unsigned short);
#endif

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************
  NAME         : CL_IncrementDefclassBusyCount
  DESCRIPTION  : Increments use count of defclass
  INPUTS       : The class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Busy count incremented
  NOTES        : None
 ***************************************************/
void
CL_IncrementDefclassBusyCount (Environment * theEnv, Defclass * theDefclass)
{
#if MAC_XCD
#pragma unused(theEnv)
#endif

  theDefclass->busy++;
}

/***************************************************
  NAME         : CL_DecrementDefclassBusyCount
  DESCRIPTION  : Decrements use count of defclass
  INPUTS       : The class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Busy count decremented
  NOTES        : Since use counts are ignored on
                 a clear and defclasses might be
                 deleted already anyway, this is
                 a no-op on a clear
 ***************************************************/
void
CL_DecrementDefclassBusyCount (Environment * theEnv, Defclass * theDefclass)
{
  if (!ConstructData (theEnv)->CL_ClearInProgress)
    {
      theDefclass->busy--;
    }
}

/****************************************************
  NAME         : CL_InstancesPurge
  DESCRIPTION  : Removes all instances
  INPUTS       : None
  RETURNS      : True if all instances deleted,
                 false otherwise
  SIDE EFFECTS : The instance hash table is cleared
  NOTES        : None
 ****************************************************/
bool
CL_InstancesPurge (Environment * theEnv, void *context)
{
  CL_DestroyAll_Instances (theEnv, NULL);
  CL_Cleanup_Instances (theEnv, NULL);
  return ((InstanceData (theEnv)->InstanceList != NULL) ? false : true);
}

#if ! RUN_TIME

/***************************************************
  NAME         : CL_InitializeClasses
  DESCRIPTION  : Allocates class hash table
                 Initializes class hash table
                   to all NULL addresses
                 Creates system classes
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Hash table initialized
  NOTES        : None
 ***************************************************/
void
CL_InitializeClasses (Environment * theEnv)
{
  int i;

  DefclassData (theEnv)->ClassTable =
    (Defclass **) CL_gm2 (theEnv,
			  (sizeof (Defclass *) * CLASS_TABLE_HASH_SIZE));
  for (i = 0; i < CLASS_TABLE_HASH_SIZE; i++)
    DefclassData (theEnv)->ClassTable[i] = NULL;
  DefclassData (theEnv)->SlotNameTable =
    (SLOT_NAME **) CL_gm2 (theEnv,
			   (sizeof (SLOT_NAME *) *
			    SLOT_NAME_TABLE_HASH_SIZE));
  for (i = 0; i < SLOT_NAME_TABLE_HASH_SIZE; i++)
    DefclassData (theEnv)->SlotNameTable[i] = NULL;
}

#endif

/********************************************************
  NAME         : CL_FindClassSlot
  DESCRIPTION  : Searches for a named slot in a class
  INPUTS       : 1) The class address
                 2) The symbolic slot name
  RETURNS      : Address of slot if found, NULL otherwise
  SIDE EFFECTS : None
  NOTES        : Only looks in class defn, does not
                   examine inheritance paths
 ********************************************************/
SlotDescriptor *
CL_FindClassSlot (Defclass * cls, CLIPSLexeme * sname)
{
  long i;

  for (i = 0; i < cls->slotCount; i++)
    {
      if (cls->slots[i].slotName->name == sname)
	return (&cls->slots[i]);
    }
  return NULL;
}

/***************************************************************
  NAME         : CL_ClassExistError
  DESCRIPTION  : Prints out error message for non-existent class
  INPUTS       : 1) Name of function having the error
                 2) The name of the non-existent class
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************************/
void
CL_ClassExistError (Environment * theEnv, const char *func, const char *cname)
{
  CL_PrintErrorID (theEnv, "CLASSFUN", 1, false);
  CL_WriteString (theEnv, STDERR, "Unable to find class '");
  CL_WriteString (theEnv, STDERR, cname);
  CL_WriteString (theEnv, STDERR, "' in function '");
  CL_WriteString (theEnv, STDERR, func);
  CL_WriteString (theEnv, STDERR, "'.\n");
  Set_EvaluationError (theEnv, true);
}

/*********************************************
  NAME         : CL_DeleteClassLinks
  DESCRIPTION  : Deallocates a class link list
  INPUTS       : The address of the list
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 *********************************************/
void
CL_DeleteClassLinks (Environment * theEnv, CLASS_LINK * clink)
{
  CLASS_LINK *ctmp;

  for (ctmp = clink; ctmp != NULL; ctmp = clink)
    {
      clink = clink->nxt;
      rtn_struct (theEnv, classLink, ctmp);
    }
}

/******************************************************
  NAME         : CL_PrintClassName
  DESCRIPTION  : Displays a class's name
  INPUTS       : 1) Logical name of output
                 2) The class
                 3) Flag indicating whether to
                    print carriage-return at end
  RETURNS      : Nothing useful
  SIDE EFFECTS : Class name printed (and module name
                 too if class is not in current module)
  NOTES        : None
 ******************************************************/
void
CL_PrintClassName (Environment * theEnv,
		   const char *logicalName,
		   Defclass * theDefclass, bool useQuotes, bool linefeedFlag)
{
  if (useQuotes)
    CL_WriteString (theEnv, logicalName, "'");
  if ((theDefclass->header.whichModule->theModule !=
       CL_GetCurrentModule (theEnv)) && (theDefclass->system == 0))
    {
      CL_WriteString (theEnv, logicalName,
		      CL_DefmoduleName (theDefclass->header.whichModule->
					theModule));
      CL_WriteString (theEnv, logicalName, "::");
    }
  CL_WriteString (theEnv, logicalName, theDefclass->header.name->contents);
  if (useQuotes)
    CL_WriteString (theEnv, logicalName, "'");
  if (linefeedFlag)
    CL_WriteString (theEnv, logicalName, "\n");
}

#if DEBUGGING_FUNCTIONS || ((! BLOAD_ONLY) && (! RUN_TIME))

/***************************************************
  NAME         : CL_PrintPackedClassLinks
  DESCRIPTION  : Displays the names of classes in
                 a list with a title
  INPUTS       : 1) The logical name of the output
                 2) Title string
                 3) List of class links
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
void
CL_PrintPackedClassLinks (Environment * theEnv,
			  const char *logicalName,
			  const char *title, PACKED_CLASS_LINKS * plinks)
{
  unsigned long i;

  CL_WriteString (theEnv, logicalName, title);
  for (i = 0; i < plinks->classCount; i++)
    {
      CL_WriteString (theEnv, logicalName, " ");
      CL_PrintClassName (theEnv, logicalName, plinks->classArray[i], false,
			 false);
    }
  CL_WriteString (theEnv, logicalName, "\n");
}

#endif

#if ! RUN_TIME

/*******************************************************
  NAME         : CL_PutClassInTable
  DESCRIPTION  : Inserts a class in the class hash table
  INPUTS       : The class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Class inserted
  NOTES        : None
 *******************************************************/
void
CL_PutClassInTable (Environment * theEnv, Defclass * cls)
{
  cls->hashTableIndex = CL_HashClass (Get_DefclassNamePointer (cls));
  cls->nxtHash = DefclassData (theEnv)->ClassTable[cls->hashTableIndex];
  DefclassData (theEnv)->ClassTable[cls->hashTableIndex] = cls;
}

/*********************************************************
  NAME         : CL_RemoveClassFromTable
  DESCRIPTION  : Removes a class from the class hash table
  INPUTS       : The class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Class removed
  NOTES        : None
 *********************************************************/
void
CL_RemoveClassFromTable (Environment * theEnv, Defclass * cls)
{
  Defclass *prvhsh, *hshptr;

  prvhsh = NULL;
  hshptr = DefclassData (theEnv)->ClassTable[cls->hashTableIndex];
  while (hshptr != cls)
    {
      prvhsh = hshptr;
      hshptr = hshptr->nxtHash;
    }
  if (prvhsh == NULL)
    DefclassData (theEnv)->ClassTable[cls->hashTableIndex] = cls->nxtHash;
  else
    prvhsh->nxtHash = cls->nxtHash;
}

/***************************************************
  NAME         : CL_AddClassLink
  DESCRIPTION  : Adds a class link from one class to
                  another
  INPUTS       : 1) The packed links in which to
                    insert the new class
                 2) The subclass pointer
                 3) A flag indicating if the link
                    should be appended.
                 4) Index of where to place the
                    class if not appended
  RETURNS      : Nothing useful
  SIDE EFFECTS : Link created and attached
  NOTES        : Assumes the pack structure belongs
                 to a class and does not need to
                 be deallocated
 ***************************************************/
void
CL_AddClassLink (Environment * theEnv,
		 PACKED_CLASS_LINKS * src,
		 Defclass * cls, bool append, unsigned int posn)
{
  PACKED_CLASS_LINKS dst;

  dst.classArray =
    (Defclass **) CL_gm2 (theEnv,
			  (sizeof (Defclass *) * (src->classCount + 1)));

  if (append)
    {
      GenCopyMemory (Defclass *, src->classCount, dst.classArray,
		     src->classArray);
      dst.classArray[src->classCount] = cls;
    }
  else
    {
      if (posn != 0)
	GenCopyMemory (Defclass *, posn, dst.classArray, src->classArray);
      GenCopyMemory (Defclass *, src->classCount - posn,
		     dst.classArray + posn + 1, src->classArray + posn);
      dst.classArray[posn] = cls;
    }
  dst.classCount = src->classCount + 1;
  CL_DeletePackedClassLinks (theEnv, src, false);
  src->classCount = dst.classCount;
  src->classArray = dst.classArray;
}

/***************************************************
  NAME         : CL_DeleteSubclassLink
  DESCRIPTION  : Removes a class from another
                 class's subclass list
  INPUTS       : 1) The superclass whose subclass
                    list is to be modified
                 2) The subclass to be removed
  RETURNS      : Nothing useful
  SIDE EFFECTS : The subclass list is changed
  NOTES        : None
 ***************************************************/
void
CL_DeleteSubclassLink (Environment * theEnv,
		       Defclass * sclass, Defclass * cls)
{
  unsigned long deletedIndex;
  PACKED_CLASS_LINKS *src, dst;

  src = &sclass->directSubclasses;

  for (deletedIndex = 0; deletedIndex < src->classCount; deletedIndex++)
    if (src->classArray[deletedIndex] == cls)
      break;
  if (deletedIndex == src->classCount)
    return;
  if (src->classCount > 1)
    {
      dst.classArray =
	(Defclass **) CL_gm2 (theEnv,
			      (sizeof (Defclass *) * (src->classCount - 1)));
      if (deletedIndex != 0)
	GenCopyMemory (Defclass *, deletedIndex, dst.classArray,
		       src->classArray);
      GenCopyMemory (Defclass *, src->classCount - deletedIndex - 1,
		     dst.classArray + deletedIndex,
		     src->classArray + deletedIndex + 1);
    }
  else
    dst.classArray = NULL;

  dst.classCount = src->classCount - 1;
  CL_DeletePackedClassLinks (theEnv, src, false);
  src->classCount = dst.classCount;
  src->classArray = dst.classArray;
}


/***************************************************
  NAME         : CL_DeleteSuperclassLink
  DESCRIPTION  : Removes a class from another
                 class's superclass list
  INPUTS       : 1) The subclass whose superclass
                    list is to be modified
                 2) The superclass to be removed
  RETURNS      : Nothing useful
  SIDE EFFECTS : The subclass list is changed
  NOTES        : None
 ***************************************************/
void
CL_DeleteSuperclassLink (Environment * theEnv,
			 Defclass * sclass, Defclass * cls)
{
  unsigned long deletedIndex;
  PACKED_CLASS_LINKS *src, dst;

  src = &sclass->directSuperclasses;

  for (deletedIndex = 0; deletedIndex < src->classCount; deletedIndex++)
    if (src->classArray[deletedIndex] == cls)
      break;
  if (deletedIndex == src->classCount)
    return;
  if (src->classCount > 1)
    {
      dst.classArray =
	(Defclass **) CL_gm2 (theEnv,
			      (sizeof (Defclass *) * (src->classCount - 1)));
      if (deletedIndex != 0)
	GenCopyMemory (Defclass *, deletedIndex, dst.classArray,
		       src->classArray);
      GenCopyMemory (Defclass *, src->classCount - deletedIndex - 1,
		     dst.classArray + deletedIndex,
		     src->classArray + deletedIndex + 1);
    }
  else
    dst.classArray = NULL;

  dst.classCount = src->classCount - 1;
  CL_DeletePackedClassLinks (theEnv, src, false);
  src->classCount = dst.classCount;
  src->classArray = dst.classArray;
}

/**************************************************************
  NAME         : CL_NewClass
  DESCRIPTION  : Allocates and initalizes a new class structure
  INPUTS       : The symbolic name of the new class
  RETURNS      : The address of the new class
  SIDE EFFECTS : None
  NOTES        : None
 **************************************************************/
Defclass *
CL_NewClass (Environment * theEnv, CLIPSLexeme * className)
{
  Defclass *cls;

  cls = get_struct (theEnv, defclass);
  CL_InitializeConstructHeader (theEnv, "defclass", DEFCLASS, &cls->header,
				className);

  cls->id = 0;
  cls->installed = 0;
  cls->busy = 0;
  cls->system = 0;
  cls->abstract = 0;
  cls->reactive = 1;
#if DEBUGGING_FUNCTIONS
  cls->trace_Instances = DefclassData (theEnv)->CL_Watch_Instances;
  cls->traceSlots = DefclassData (theEnv)->CL_WatchSlots;
#endif
  cls->hashTableIndex = 0;
  cls->directSuperclasses.classCount = 0;
  cls->directSuperclasses.classArray = NULL;
  cls->directSubclasses.classCount = 0;
  cls->directSubclasses.classArray = NULL;
  cls->allSuperclasses.classCount = 0;
  cls->allSuperclasses.classArray = NULL;
  cls->slots = NULL;
  cls->instanceTemplate = NULL;
  cls->slotNameMap = NULL;
  cls->instanceSlotCount = 0;
  cls->localInstanceSlotCount = 0;
  cls->slotCount = 0;
  cls->maxSlotNameID = 0;
  cls->handlers = NULL;
  cls->handlerOrderMap = NULL;
  cls->handlerCount = 0;
  cls->instanceList = NULL;
  cls->instanceListBottom = NULL;
  cls->nxtHash = NULL;
  cls->scopeMap = NULL;
  CL_ClearBitString (cls->traversalRecord, TRAVERSAL_BYTES);
  cls->relevant_te_rminal_alpha_nodes = NULL;
  return (cls);
}

/***************************************************
  NAME         : CL_DeletePackedClassLinks
  DESCRIPTION  : Dealloacates a contiguous array
                 holding class links
  INPUTS       : 1) The class link segment
                 2) A flag indicating whether to
                    delete the top pack structure
  RETURNS      : Nothing useful
  SIDE EFFECTS : Class links deallocated
  NOTES        : None
 ***************************************************/
void
CL_DeletePackedClassLinks (Environment * theEnv,
			   PACKED_CLASS_LINKS * plp, bool deleteTop)
{
  if (plp->classCount > 0)
    {
      CL_rm (theEnv, plp->classArray,
	     (sizeof (Defclass *) * plp->classCount));
      plp->classCount = 0;
      plp->classArray = NULL;
    }
  if (deleteTop)
    rtn_struct (theEnv, packedClassLinks, plp);
}

/***************************************************
  NAME         : CL_AssignClassID
  DESCRIPTION  : Assigns a unique id to a class
                 and puts its address in the
                 id map
  INPUTS       : The class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Class id assigned and map set
  NOTES        : None
 ***************************************************/
void
CL_AssignClassID (Environment * theEnv, Defclass * cls)
{
  unsigned short i;

  if ((DefclassData (theEnv)->MaxClassID % CLASS_ID_MAP_CHUNK) == 0)
    {
      DefclassData (theEnv)->ClassIDMap =
	(Defclass **) CL_genrealloc (theEnv,
				     DefclassData (theEnv)->ClassIDMap,
				     (DefclassData (theEnv)->MaxClassID *
				      sizeof (Defclass *)),
				     ((DefclassData (theEnv)->MaxClassID +
				       CLASS_ID_MAP_CHUNK) *
				      sizeof (Defclass *)));
      DefclassData (theEnv)->AvailClassID += CLASS_ID_MAP_CHUNK;

      for (i = DefclassData (theEnv)->MaxClassID;
	   i < (DefclassData (theEnv)->MaxClassID + CLASS_ID_MAP_CHUNK); i++)
	DefclassData (theEnv)->ClassIDMap[i] = NULL;
    }
  DefclassData (theEnv)->ClassIDMap[DefclassData (theEnv)->MaxClassID] = cls;
  cls->id = DefclassData (theEnv)->MaxClassID++;
}

/*********************************************************
  NAME         : CL_AddSlotName
  DESCRIPTION  : Adds a new slot entry (or increments
                 the use count of an existing node).
  INPUTS       : 1) The slot name
                 2) The new canonical id for the slot name
                 3) A flag indicating whether the
                    given id must be used or not
  RETURNS      : The id of the (old) node
  SIDE EFFECTS : Slot name entry added or use count
                 incremented
  NOTES        : None
 *********************************************************/
SLOT_NAME *
CL_AddSlotName (Environment * theEnv,
		CLIPSLexeme * slotName, unsigned short newid, bool usenewid)
{
  SLOT_NAME *snp;
  unsigned hashTableIndex;
  char *buf;
  size_t bufsz;

  hashTableIndex = HashSlotName (slotName);
  snp = DefclassData (theEnv)->SlotNameTable[hashTableIndex];
  while ((snp != NULL) ? (snp->name != slotName) : false)
    snp = snp->nxt;
  if (snp != NULL)
    {
      if (usenewid && (newid != snp->id))
	{
	  CL_SystemError (theEnv, "CLASSFUN", 1);
	  CL_ExitRouter (theEnv, EXIT_FAILURE);
	}
      snp->use++;
    }
  else
    {
      snp = get_struct (theEnv, slotName);
      snp->name = slotName;
      snp->hashTableIndex = hashTableIndex;
      snp->use = 1;
      snp->id =
	(unsigned short) (usenewid ? newid : DefclassData (theEnv)->
			  newSlotID++);
      snp->nxt = DefclassData (theEnv)->SlotNameTable[hashTableIndex];
      DefclassData (theEnv)->SlotNameTable[hashTableIndex] = snp;
      IncrementLexemeCount (slotName);
      bufsz = (sizeof (char) *
	       (PUT_PREFIX_LENGTH + strlen (slotName->contents) + 1));
      buf = (char *) CL_gm2 (theEnv, bufsz);
      CL_genstrcpy (buf, PUT_PREFIX);
      CL_genstrcat (buf, slotName->contents);
      snp->putHandlerName = CL_CreateSymbol (theEnv, buf);
      IncrementLexemeCount (snp->putHandlerName);
      CL_rm (theEnv, buf, bufsz);
      snp->bsaveIndex = 0L;
    }
  return (snp);
}

/***************************************************
  NAME         : CL_DeleteSlotName
  DESCRIPTION  : Removes a slot name entry from
                 the table of all slot names if
                 no longer in use
  INPUTS       : The slot name hash node
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slot name entry deleted or use
                 count decremented
  NOTES        : None
 ***************************************************/
void
CL_DeleteSlotName (Environment * theEnv, SLOT_NAME * slotName)
{
  SLOT_NAME *snp, *prv;

  if (slotName == NULL)
    return;
  prv = NULL;
  snp = DefclassData (theEnv)->SlotNameTable[slotName->hashTableIndex];
  while (snp != slotName)
    {
      prv = snp;
      snp = snp->nxt;
    }
  snp->use--;
  if (snp->use != 0)
    return;
  if (prv == NULL)
    DefclassData (theEnv)->SlotNameTable[snp->hashTableIndex] = snp->nxt;
  else
    prv->nxt = snp->nxt;
  CL_ReleaseLexeme (theEnv, snp->name);
  CL_ReleaseLexeme (theEnv, snp->putHandlerName);
  rtn_struct (theEnv, slotName, snp);
}

/*******************************************************************
  NAME         : CL_RemoveDefclass
  DESCRIPTION  : Deallocates a class structure and
                 all its fields - returns all symbols
                 in use by the class back to the symbol
                 manager for ephemeral removal
  INPUTS       : The address of the class
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : Assumes class has no subclasses
                 IMPORTANT WARNING!! : Assumes class
                   busy count and all instances' busy
                   counts are 0 and all handlers' busy counts are 0!
 *******************************************************************/
void
CL_RemoveDefclass (Environment * theEnv, Defclass * cls)
{
  DefmessageHandler *hnd;
  unsigned long i;
  CLASS_ALPHA_LINK *currentAlphaLink;
  CLASS_ALPHA_LINK *nextAlphaLink;

  /* ====================================================
     Remove all of this class's superclasses' links to it
     ==================================================== */
  for (i = 0; i < cls->directSuperclasses.classCount; i++)
    CL_DeleteSubclassLink (theEnv, cls->directSuperclasses.classArray[i],
			   cls);

  /* ====================================================
     Remove all of this class's subclasses' links to it
     ==================================================== */
  for (i = 0; i < cls->directSubclasses.classCount; i++)
    CL_DeleteSuperclassLink (theEnv, cls->directSubclasses.classArray[i],
			     cls);

  CL_RemoveClassFromTable (theEnv, cls);

  CL_InstallClass (theEnv, cls, false);

  CL_DeletePackedClassLinks (theEnv, &cls->directSuperclasses, false);
  CL_DeletePackedClassLinks (theEnv, &cls->allSuperclasses, false);
  CL_DeletePackedClassLinks (theEnv, &cls->directSubclasses, false);

  for (i = 0; i < cls->slotCount; i++)
    {
      if (cls->slots[i].defaultValue != NULL)
	{
	  if (cls->slots[i].dynamicDefault)
	    CL_ReturnPackedExpression (theEnv,
				       (Expression *) cls->slots[i].
				       defaultValue);
	  else
	    rtn_struct (theEnv, udfValue, cls->slots[i].defaultValue);
	}
      CL_DeleteSlotName (theEnv, cls->slots[i].slotName);
      CL_RemoveConstraint (theEnv, cls->slots[i].constraint);
    }

  if (cls->instanceSlotCount != 0)
    {
      CL_rm (theEnv, cls->instanceTemplate,
	     (sizeof (SlotDescriptor *) * cls->instanceSlotCount));
      CL_rm (theEnv, cls->slotNameMap,
	     (sizeof (unsigned) * (cls->maxSlotNameID + 1)));
    }
  if (cls->slotCount != 0)
    CL_rm (theEnv, cls->slots, (sizeof (SlotDescriptor) * cls->slotCount));

  for (i = 0; i < cls->handlerCount; i++)
    {
      hnd = &cls->handlers[i];
      if (hnd->actions != NULL)
	CL_ReturnPackedExpression (theEnv, hnd->actions);
      if (hnd->header.ppFo_rm != NULL)
	CL_rm (theEnv, (void *) hnd->header.ppFo_rm,
	       (sizeof (char) * (strlen (hnd->header.ppFo_rm) + 1)));
      if (hnd->header.usrData != NULL)
	{
	  CL_ClearUserDataList (theEnv, hnd->header.usrData);
	}
    }
  if (cls->handlerCount != 0)
    {
      CL_rm (theEnv, cls->handlers,
	     (sizeof (DefmessageHandler) * cls->handlerCount));
      CL_rm (theEnv, cls->handlerOrderMap,
	     (sizeof (unsigned) * cls->handlerCount));
    }

  currentAlphaLink = cls->relevant_te_rminal_alpha_nodes;
  while (currentAlphaLink != NULL)
    {
      nextAlphaLink = currentAlphaLink->next;
      rtn_struct (theEnv, classAlphaLink, currentAlphaLink);
      currentAlphaLink = nextAlphaLink;
    }
  cls->relevant_te_rminal_alpha_nodes = NULL;

  SetCL_DefclassPPFo_rm (theEnv, cls, NULL);
  DeassignClassID (theEnv, cls->id);
  rtn_struct (theEnv, defclass, cls);
}

#endif

/*******************************************************************
  NAME         : CL_DestroyDefclass
  DESCRIPTION  : Deallocates a class structure and
                 all its fields.
  INPUTS       : The address of the class
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        :
 *******************************************************************/
void
CL_DestroyDefclass (Environment * theEnv, Defclass * cls)
{
  long i;
  CLASS_ALPHA_LINK *currentAlphaLink;
  CLASS_ALPHA_LINK *nextAlphaLink;

#if ! RUN_TIME
  DefmessageHandler *hnd;
  CL_DeletePackedClassLinks (theEnv, &cls->directSuperclasses, false);
  CL_DeletePackedClassLinks (theEnv, &cls->allSuperclasses, false);
  CL_DeletePackedClassLinks (theEnv, &cls->directSubclasses, false);
#endif
  for (i = 0; i < cls->slotCount; i++)
    {
      if (cls->slots[i].defaultValue != NULL)
	{
#if ! RUN_TIME
	  if (cls->slots[i].dynamicDefault)
	    CL_ReturnPackedExpression (theEnv,
				       (Expression *) cls->slots[i].
				       defaultValue);
	  else
	    rtn_struct (theEnv, udfValue, cls->slots[i].defaultValue);
#else
	  if (cls->slots[i].dynamicDefault == 0)
	    rtn_struct (theEnv, udfValue, cls->slots[i].defaultValue);
#endif
	}
    }

#if ! RUN_TIME
  if (cls->instanceSlotCount != 0)
    {
      CL_rm (theEnv, cls->instanceTemplate,
	     (sizeof (SlotDescriptor *) * cls->instanceSlotCount));
      CL_rm (theEnv, cls->slotNameMap,
	     (sizeof (unsigned) * (cls->maxSlotNameID + 1)));
    }

  if (cls->slotCount != 0)
    CL_rm (theEnv, cls->slots, (sizeof (SlotDescriptor) * cls->slotCount));

  for (i = 0; i < cls->handlerCount; i++)
    {
      hnd = &cls->handlers[i];
      if (hnd->actions != NULL)
	CL_ReturnPackedExpression (theEnv, hnd->actions);

      if (hnd->header.ppFo_rm != NULL)
	CL_rm (theEnv, (void *) hnd->header.ppFo_rm,
	       (sizeof (char) * (strlen (hnd->header.ppFo_rm) + 1)));

      if (hnd->header.usrData != NULL)
	{
	  CL_ClearUserDataList (theEnv, hnd->header.usrData);
	}
    }

  if (cls->handlerCount != 0)
    {
      CL_rm (theEnv, cls->handlers,
	     (sizeof (DefmessageHandler) * cls->handlerCount));
      CL_rm (theEnv, cls->handlerOrderMap,
	     (sizeof (unsigned) * cls->handlerCount));
    }

  currentAlphaLink = cls->relevant_te_rminal_alpha_nodes;
  while (currentAlphaLink != NULL)
    {
      nextAlphaLink = currentAlphaLink->next;
      rtn_struct (theEnv, classAlphaLink, currentAlphaLink);
      currentAlphaLink = nextAlphaLink;
    }
  cls->relevant_te_rminal_alpha_nodes = NULL;

  CL_DestroyConstructHeader (theEnv, &cls->header);

  rtn_struct (theEnv, defclass, cls);
#else
#if MAC_XCD
#pragma unused(hnd)
#endif
#endif
}

#if ! RUN_TIME

/***************************************************
  NAME         : CL_InstallClass
  DESCRIPTION  : In(De)crements all symbol counts for
                 for symbols in use by class
                 Disallows (allows) symbols to become
                 ephemeral.
  INPUTS       : 1) The class address
                 2) 1 - install, 0 - deinstall
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
void
CL_InstallClass (Environment * theEnv, Defclass * cls, bool set)
{
  SlotDescriptor *slot;
  DefmessageHandler *hnd;
  long i;

  if ((set && cls->installed) || ((set == false) && (cls->installed == 0)))
    return;

  /* ==================================================================
     Handler installation is handled when message-handlers are defined:
     see CL_ParseDefmessageHandler() in MSGCOM.C

     Slot installation is handled by CL_ParseSlot() in CLASSPSR.C
     Scope map installation is handled by CL_CreateClassScopeMap()
     ================================================================== */
  if (set == false)
    {
      cls->installed = 0;

      CL_ReleaseLexeme (theEnv, cls->header.name);

#if DEFMODULE_CONSTRUCT
      CL_DecrementBitMapReferenceCount (theEnv, cls->scopeMap);
#endif
      CL_ClearUserDataList (theEnv, cls->header.usrData);

      for (i = 0; i < cls->slotCount; i++)
	{
	  slot = &cls->slots[i];
	  CL_ReleaseLexeme (theEnv, slot->overrideMessage);
	  if (slot->defaultValue != NULL)
	    {
	      if (slot->dynamicDefault)
		CL_ExpressionDeinstall (theEnv,
					(Expression *) slot->defaultValue);
	      else
		CL_ReleaseUDFV (theEnv, (UDFValue *) slot->defaultValue);
	    }
	}
      for (i = 0; i < cls->handlerCount; i++)
	{
	  hnd = &cls->handlers[i];
	  CL_ReleaseLexeme (theEnv, hnd->header.name);
	  if (hnd->actions != NULL)
	    CL_ExpressionDeinstall (theEnv, hnd->actions);
	}
    }
  else
    {
      cls->installed = 1;
      IncrementLexemeCount (cls->header.name);
    }
}

#endif

#if (! BLOAD_ONLY) && (! RUN_TIME)

/***************************************************
  NAME         : CL_IsClassBeingUsed
  DESCRIPTION  : Checks the busy flag of a class
                   and ALL classes that inherit from
                   it to make sure that it is not
                   in use before deletion
  INPUTS       : The class
  RETURNS      : True if in use, false otherwise
  SIDE EFFECTS : None
  NOTES        : Recursively examines all subclasses
 ***************************************************/
bool
CL_IsClassBeingUsed (Defclass * cls)
{
  unsigned long i;

  if (cls->busy > 0)
    return true;
  for (i = 0; i < cls->directSubclasses.classCount; i++)
    if (CL_IsClassBeingUsed (cls->directSubclasses.classArray[i]))
      return true;
  return false;
}

/***************************************************
  NAME         : CL_RemoveAllUserClasses
  DESCRIPTION  : Removes all classes
  INPUTS       : None
  RETURNS      : True if succesful, false otherwise
  SIDE EFFECTS : The class hash table is cleared
  NOTES        : None
 ***************************************************/
bool
CL_RemoveAllUserClasses (Environment * theEnv)
{
  Defclass *userClasses, *ctmp;
  bool success = true;

#if BLOAD || BLOAD_AND_BSAVE
  if (CL_Bloaded (theEnv))
    return false;
#endif
  /* ====================================================
     Don't delete built-in system classes at head of list
     ==================================================== */
  userClasses = CL_GetNextDefclass (theEnv, NULL);
  while (userClasses != NULL)
    {
      if (userClasses->system == 0)
	break;
      userClasses = CL_GetNextDefclass (theEnv, userClasses);
    }
  while (userClasses != NULL)
    {
      ctmp = userClasses;
      userClasses = CL_GetNextDefclass (theEnv, userClasses);
      if (CL_DefclassIsDeletable (ctmp))
	{
	  CL_RemoveConstructFromModule (theEnv, &ctmp->header);
	  CL_RemoveDefclass (theEnv, ctmp);
	}
      else
	{
	  success = false;
	  CL_CantDeleteItemErrorMessage (theEnv, "defclass",
					 CL_DefclassName (ctmp));
	}
    }
  return (success);
}

/****************************************************
  NAME         : CL_DeleteClassUAG
  DESCRIPTION  : Deallocates a class and all its
                 subclasses
  INPUTS       : The address of the class
  RETURNS      : 1 if successful, 0 otherwise
  SIDE EFFECTS : Removes the class from each of
                 its superclasses' subclass lists
  NOTES        : None
 ****************************************************/
bool
CL_DeleteClassUAG (Environment * theEnv, Defclass * cls)
{
  unsigned long subCount;

  while (cls->directSubclasses.classCount != 0)
    {
      subCount = cls->directSubclasses.classCount;
      CL_DeleteClassUAG (theEnv, cls->directSubclasses.classArray[0]);
      if (cls->directSubclasses.classCount == subCount)
	return false;
    }
  if (CL_DefclassIsDeletable (cls))
    {
      CL_RemoveConstructFromModule (theEnv, &cls->header);
      CL_RemoveDefclass (theEnv, cls);
      return true;
    }
  return false;
}

/*********************************************************
  NAME         : CL_MarkBitMapSubclasses
  DESCRIPTION  : Recursively marks the ids of a class
                 and all its subclasses in a bitmap
  INPUTS       : 1) The bitmap
                 2) The class
                 3) A code indicating whether to set
                    or clear the bits of the map
                    corresponding to the class ids
  RETURNS      : Nothing useful
  SIDE EFFECTS : BitMap marked
  NOTES        : IMPORTANT!!!!  Assumes the bitmap is
                 large enough to hold all ids encountered!
 *********************************************************/
void
CL_MarkBitMapSubclasses (char *map, Defclass * cls, int set)
{
  unsigned long i;

  if (set)
    SetBitMap (map, cls->id);
  else
    CL_ClearBitMap (map, cls->id);
  for (i = 0; i < cls->directSubclasses.classCount; i++)
    CL_MarkBitMapSubclasses (map, cls->directSubclasses.classArray[i], set);
}

#endif

/***************************************************
  NAME         : CL_FindSlotNameID
  DESCRIPTION  : Finds the id of a slot name
  INPUTS       : The slot name
  RETURNS      : The slot name id (-1 if not found)
  SIDE EFFECTS : None
  NOTES        : A slot name always has the same
                 id regardless of what class uses
                 it.  In this way, a slot can
                 be referred to by index independent
                 of class.  Each class stores a
                 map showing which slot name indices
                 go to which slot.  This provides
                 for immediate lookup of slots
                 given the index (object pattern
                 matching uses this).
 ***************************************************/
unsigned short
CL_FindSlotNameID (Environment * theEnv, CLIPSLexeme * slotName)
{
  SLOT_NAME *snp;

  snp = DefclassData (theEnv)->SlotNameTable[HashSlotName (slotName)];

  while ((snp != NULL) ? (snp->name != slotName) : false)
    {
      snp = snp->nxt;
    }

  return (snp != NULL) ? snp->id : SLOT_NAME_NOT_FOUND;
}

/***************************************************
  NAME         : CL_FindIDSlotName
  DESCRIPTION  : Finds the slot anme for an id
  INPUTS       : The id
  RETURNS      : The slot name (NULL if not found)
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
CLIPSLexeme *
CL_FindIDSlotName (Environment * theEnv, unsigned short id)
{
  SLOT_NAME *snp;

  snp = CL_FindIDSlotNameHash (theEnv, id);

  return ((snp != NULL) ? snp->name : NULL);
}

/***************************************************
  NAME         : CL_FindIDSlotNameHash
  DESCRIPTION  : Finds the slot anme for an id
  INPUTS       : The id
  RETURNS      : The slot name (NULL if not found)
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
SLOT_NAME *
CL_FindIDSlotNameHash (Environment * theEnv, unsigned short id)
{
  unsigned short i;
  SLOT_NAME *snp;

  for (i = 0; i < SLOT_NAME_TABLE_HASH_SIZE; i++)
    {
      snp = DefclassData (theEnv)->SlotNameTable[i];
      while (snp != NULL)
	{
	  if (snp->id == id)
	    return snp;
	  snp = snp->nxt;
	}
    }
  return NULL;
}

/***************************************************
  NAME         : CL_GetTraversalID
  DESCRIPTION  : Returns a unique integer ID for a
                  traversal into the class hierarchy
  INPUTS       : None
  RETURNS      : The id, or -1 if none available
  SIDE EFFECTS : CL_EvaluationError set when no ids
                   available
  NOTES        : Used for recursive traversals of
                  class hierarchy to assure that a
                  class is only visited once
 ***************************************************/
int
CL_GetTraversalID (Environment * theEnv)
{
  unsigned i;
  Defclass *cls;

  if (DefclassData (theEnv)->CTID >= MAX_TRAVERSALS)
    {
      CL_PrintErrorID (theEnv, "CLASSFUN", 2, false);
      CL_WriteString (theEnv, STDERR,
		      "Maximum number of simultaneous class hierarchy\n  traversals exceeded ");
      CL_WriteInteger (theEnv, STDERR, MAX_TRAVERSALS);
      CL_WriteString (theEnv, STDERR, ".\n");
      Set_EvaluationError (theEnv, true);
      return (-1);
    }

  for (i = 0; i < CLASS_TABLE_HASH_SIZE; i++)
    for (cls = DefclassData (theEnv)->ClassTable[i]; cls != NULL;
	 cls = cls->nxtHash)
      CL_ClearTraversalID (cls->traversalRecord, DefclassData (theEnv)->CTID);
  return (DefclassData (theEnv)->CTID++);
}

/***************************************************
  NAME         : CL_ReleaseTraversalID
  DESCRIPTION  : CL_Releases an ID for future use
                 Also clears id from all classes
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Old ID released for later reuse
  NOTES        : CL_Releases ID returned by most recent
                   call to CL_GetTraversalID()
 ***************************************************/
void
CL_ReleaseTraversalID (Environment * theEnv)
{
  DefclassData (theEnv)->CTID--;
}

/*******************************************************
  NAME         : CL_HashClass
  DESCRIPTION  : Generates a hash index for a given
                 class name
  INPUTS       : The address of the class name SYMBOL_HN
  RETURNS      : The hash index value
  SIDE EFFECTS : None
  NOTES        : Counts on the fact that the symbol
                 has already been hashed into the
                 symbol table - uses that hash value
                 multiplied by a prime for a new hash
 *******************************************************/
unsigned int
CL_HashClass (CLIPSLexeme * cname)
{
  unsigned int tally;

  tally = cname->bucket * BIG_PRIME;

  return tally % CLASS_TABLE_HASH_SIZE;
}

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/*******************************************************
  NAME         : HashSlotName
  DESCRIPTION  : Generates a hash index for a given
                 slot name
  INPUTS       : The address of the slot name SYMBOL_HN
  RETURNS      : The hash index value
  SIDE EFFECTS : None
  NOTES        : Counts on the fact that the symbol
                 has already been hashed into the
                 symbol table - uses that hash value
                 multiplied by a prime for a new hash
 *******************************************************/
static unsigned int
HashSlotName (CLIPSLexeme * sname)
{
  unsigned int tally;

  tally = sname->bucket * BIG_PRIME;

  return tally % SLOT_NAME_TABLE_HASH_SIZE;
}

#if (! RUN_TIME)

/***************************************************
  NAME         : DeassignClassID
  DESCRIPTION  : Reduces id map and MaxClassID if
                 no ids in use above the one being
                 released.
  INPUTS       : The id
  RETURNS      : Nothing useful
  SIDE EFFECTS : ID map and MaxClassID possibly
                 reduced
  NOTES        : None
 ***************************************************/
static void
DeassignClassID (Environment * theEnv, unsigned short id)
{
  unsigned short i;
  bool reallocReqd;
  unsigned short oldChunk = 0, newChunk = 0;

  DefclassData (theEnv)->ClassIDMap[id] = NULL;
  for (i = id + 1; i < DefclassData (theEnv)->MaxClassID; i++)
    if (DefclassData (theEnv)->ClassIDMap[i] != NULL)
      return;

  reallocReqd = false;
  while (DefclassData (theEnv)->ClassIDMap[id] == NULL)
    {
      DefclassData (theEnv)->MaxClassID = id;
      if ((DefclassData (theEnv)->MaxClassID % CLASS_ID_MAP_CHUNK) == 0)
	{
	  newChunk = DefclassData (theEnv)->MaxClassID;
	  if (reallocReqd == false)
	    {
	      oldChunk =
		(DefclassData (theEnv)->MaxClassID + CLASS_ID_MAP_CHUNK);
	      reallocReqd = true;
	    }
	}
      if (id == 0)
	break;
      id--;
    }
  if (reallocReqd)
    {
      DefclassData (theEnv)->ClassIDMap =
	(Defclass **) CL_genrealloc (theEnv,
				     DefclassData (theEnv)->ClassIDMap,
				     (oldChunk * sizeof (Defclass *)),
				     (newChunk * sizeof (Defclass *)));

      DefclassData (theEnv)->AvailClassID = newChunk;
    }
}

#endif

#endif
