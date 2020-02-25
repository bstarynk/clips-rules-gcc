   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  02/03/18             */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Binary CL_Load/CL_Save Functions for Classes and their */
/*             message-handlers                              */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed IMPERATIVE_MESSAGE_HANDLERS and        */
/*            AUXILIARY_MESSAGE_HANDLERS compilation flags.  */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
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

#if OBJECT_SYSTEM && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)

#include "bload.h"
#include "bsave.h"
#include "classcom.h"
#include "classfun.h"
#include "classini.h"
#include "cstrcbin.h"
#include "cstrnbin.h"
#include "envrnmnt.h"
#include "insfun.h"
#include "memalloc.h"
#include "modulbin.h"
#include "msgcom.h"
#include "msgfun.h"
#include "prntutil.h"
#include "router.h"

#if DEFRULE_CONSTRUCT
#include "objrtbin.h"
#endif

#include "objbin.h"

/* =========================================
   *****************************************
               MACROS AND TYPES
   =========================================
   ***************************************** */

#define SlotIndex(p)             (((p) != NULL) ? (p)->bsaveIndex : ULONG_MAX)
#define SlotNameIndex(p)         (p)->bsaveIndex

#define LinkPointer(i)           (((i) == ULONG_MAX) ? NULL : (Defclass **) &ObjectBinaryData(theEnv)->LinkArray[i])
#define SlotPointer(i)           (((i) == UINT_MAX) ? NULL : (SlotDescriptor *) &ObjectBinaryData(theEnv)->SlotArray[i])
#define TemplateSlotPointer(i)   (((i) == ULONG_MAX) ? NULL : (SlotDescriptor **) &ObjectBinaryData(theEnv)->TmpslotArray[i])
#define OrderedSlotPointer(i)    (((i) == ULONG_MAX) ? NULL : (unsigned *) &ObjectBinaryData(theEnv)->MapslotArray[i])
#define SlotNamePointer(i)       ((SLOT_NAME *) &ObjectBinaryData(theEnv)->SlotNameArray[i])
#define HandlerPointer(i)        (((i) == ULONG_MAX) ? NULL : &ObjectBinaryData(theEnv)->HandlerArray[i])
#define OrderedHandlerPointer(i) (((i) == ULONG_MAX) ? NULL : (unsigned *) &ObjectBinaryData(theEnv)->MaphandlerArray[i])

typedef struct bsave_DefclassModule
  {
   struct bsaveDefmoduleItemHeader header;
  } BSAVE_DEFCLASS_MODULE;

typedef struct bsavePackedClassLinks
  {
   unsigned long classCount;
   unsigned long classArray;
  } BSAVE_PACKED_CLASS_LINKS;

typedef struct bsaveDefclass
  {
   struct bsaveConstructHeader header;
   unsigned abstract : 1;
   unsigned reactive : 1;
   unsigned system   : 1;
   unsigned short id;
   BSAVE_PACKED_CLASS_LINKS directSuperclasses;
   BSAVE_PACKED_CLASS_LINKS directSubclasses;
   BSAVE_PACKED_CLASS_LINKS allSuperclasses;
   unsigned short slotCount;
   unsigned short localInstanceSlotCount;
   unsigned short instanceSlotCount;
   unsigned short maxSlotNameID;
   unsigned short handlerCount;
   unsigned long slots;
   unsigned long instanceTemplate;
   unsigned long slotNameMap;
   unsigned long handlers;
   unsigned long scopeMap;
#if DEFRULE_CONSTRUCT
   unsigned long relevant_te_rminal_alpha_nodes;
#endif
  } BSAVE_DEFCLASS;

typedef struct bsaveSlotName
  {
   unsigned short id;
   unsigned hashTableIndex;
   unsigned long name;
   unsigned long putHandlerName;
  } BSAVE_SLOT_NAME;

typedef struct bsaveSlotDescriptor
  {
   unsigned shared              : 1;
   unsigned multiple            : 1;
   unsigned composite           : 1;
   unsigned noInherit           : 1;
   unsigned no_Write             : 1;
   unsigned initializeOnly      : 1;
   unsigned dynamicDefault      : 1;
   unsigned noDefault           : 1;
   unsigned reactive            : 1;
   unsigned publicVisibility    : 1;
   unsigned createReadAccessor  : 1;
   unsigned create_WriteAccessor : 1;
   unsigned long cls;
   unsigned long slotName,
        defaultValue,
        constraint,
        overrideMessage;
  } BSAVE_SLOT_DESC;

typedef struct bsaveMessageHandler
  {
   struct bsaveConstructHeader header;
   unsigned system : 1;
   unsigned type   : 2;
   unsigned short minParams;
   unsigned short maxParams;
   unsigned short localVarCount;
   unsigned long cls;
   unsigned long actions;
  } BSAVE_HANDLER;

typedef struct handler_BsaveInfo
  {
   DefmessageHandler *handlers;
   unsigned *handlerOrderMap;
   unsigned handlerCount;
  } HANDLER_BSAVE_INFO;

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if BLOAD_AND_BSAVE

   static void                    CL_BsaveObjectsFind(Environment *);
   static void                    MarkDefclassItems(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveObjectsExpressions(Environment *,FILE *);
   static void                    CL_BsaveDefaultSlotExpressions(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveHandlerActionExpressions(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveStorageObjects(Environment *,FILE *);
   static void                    CL_BsaveObjects(Environment *,FILE *);
   static void                    CL_BsaveDefclass(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveClassLinks(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveSlots(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveTemplateSlots(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveSlotMap(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveHandlers(Environment *,ConstructHeader *,void *);
   static void                    CL_BsaveHandlerMap(Environment *,ConstructHeader *,void *);

#endif

   static void                    CL_BloadStorageObjects(Environment *);
   static void                    CL_BloadObjects(Environment *);
   static void                    UpdatePrimitiveClassesMap(Environment *);
   static void                    Update_DefclassModule(Environment *,void *,unsigned long);
   static void                    UpdateDefclass(Environment *,void *,unsigned long);
   static void                    UpdateLink(Environment *,void *,unsigned long);
   static void                    UpdateSlot(Environment *,void *,unsigned long);
   static void                    UpdateSlotName(Environment *,void *,unsigned long);
   static void                    UpdateTemplateSlot(Environment *,void *,unsigned long);
   static void                    UpdateHandler(Environment *,void *,unsigned long);
   static void                    CL_Clear_BloadObjects(Environment *);
   static void                    DeallocateObjectBinaryData(Environment *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***********************************************************
  NAME         : SetupObjects_Bload
  DESCRIPTION  : Initializes data structures and
                   routines for binary loads of
                   generic function constructs
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Routines defined and structures initialized
  NOTES        : None
 ***********************************************************/
void SetupObjects_Bload(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,OBJECTBIN_DATA,sizeof(struct objectBinaryData),DeallocateObjectBinaryData);

   CL_AddAbort_BloadFunction(theEnv,"defclass",CL_CreateSystemClasses,0,NULL);

#if BLOAD_AND_BSAVE
   CL_AddBinaryItem(theEnv,"defclass",0,CL_BsaveObjectsFind,CL_BsaveObjectsExpressions,
                             CL_BsaveStorageObjects,CL_BsaveObjects,
                             CL_BloadStorageObjects,CL_BloadObjects,
                             CL_Clear_BloadObjects);
#endif
#if BLOAD || BLOAD_ONLY
   CL_AddBinaryItem(theEnv,"defclass",0,NULL,NULL,NULL,NULL,
                             CL_BloadStorageObjects,CL_BloadObjects,
                             CL_Clear_BloadObjects);
#endif

  }

/*******************************************************/
/* DeallocateObjectBinaryData: Deallocates environment */
/*    data for object binary functionality.            */
/*******************************************************/
static void DeallocateObjectBinaryData(
  Environment *theEnv)
  {
   size_t space;
   unsigned long i;

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)

   space = (sizeof(DEFCLASS_MODULE) * ObjectBinaryData(theEnv)->ModuleCount);
   if (space != 0) CL_genfree(theEnv,ObjectBinaryData(theEnv)->ModuleArray,space);

   if (ObjectBinaryData(theEnv)->ClassCount != 0)
     {
      if (DefclassData(theEnv)->ClassIDMap != NULL)
        { CL_rm(theEnv,DefclassData(theEnv)->ClassIDMap,(sizeof(Defclass *) * DefclassData(theEnv)->AvailClassID)); }

      for (i = 0L ; i < ObjectBinaryData(theEnv)->SlotCount ; i++)
        {
         if ((ObjectBinaryData(theEnv)->SlotArray[i].defaultValue != NULL) && (ObjectBinaryData(theEnv)->SlotArray[i].dynamicDefault == 0))
           { rtn_struct(theEnv,udfValue,ObjectBinaryData(theEnv)->SlotArray[i].defaultValue); }
        }

      space = (sizeof(Defclass) * ObjectBinaryData(theEnv)->ClassCount);
      if (space != 0L)
        { CL_genfree(theEnv,ObjectBinaryData(theEnv)->DefclassArray,space); }

      space = (sizeof(Defclass *) * ObjectBinaryData(theEnv)->LinkCount);
      if (space != 0L)
        { CL_genfree(theEnv,ObjectBinaryData(theEnv)->LinkArray,space); }

      space = (sizeof(SlotDescriptor) * ObjectBinaryData(theEnv)->SlotCount);
      if (space != 0L)
        { CL_genfree(theEnv,ObjectBinaryData(theEnv)->SlotArray,space); }

      space = (sizeof(SLOT_NAME) * ObjectBinaryData(theEnv)->SlotNameCount);
      if (space != 0L)
        { CL_genfree(theEnv,ObjectBinaryData(theEnv)->SlotNameArray,space); }

      space = (sizeof(SlotDescriptor *) * ObjectBinaryData(theEnv)->TemplateSlotCount);
      if (space != 0L)
        { CL_genfree(theEnv,ObjectBinaryData(theEnv)->TmpslotArray,space); }

      space = (sizeof(unsigned) * ObjectBinaryData(theEnv)->SlotNameMapCount);
      if (space != 0L)
        { CL_genfree(theEnv,ObjectBinaryData(theEnv)->MapslotArray,space); }
     }

   if (ObjectBinaryData(theEnv)->HandlerCount != 0L)
     {
      space = (sizeof(DefmessageHandler) * ObjectBinaryData(theEnv)->HandlerCount);
      if (space != 0L)
        {
         CL_genfree(theEnv,ObjectBinaryData(theEnv)->HandlerArray,space);
         space = (sizeof(unsigned) * ObjectBinaryData(theEnv)->HandlerCount);
         CL_genfree(theEnv,ObjectBinaryData(theEnv)->MaphandlerArray,space);
        }
     }
#endif
  }

/***************************************************
  NAME         : CL_Bload_DefclassModuleReference
  DESCRIPTION  : Returns a pointer to the
                 appropriate defclass module
  INPUTS       : The index of the module
  RETURNS      : A pointer to the module
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
void *CL_Bload_DefclassModuleReference(
  Environment *theEnv,
  unsigned long theIndex)
  {
   return ((void *) &ObjectBinaryData(theEnv)->ModuleArray[theIndex]);
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if BLOAD_AND_BSAVE

/***************************************************************************
  NAME         : CL_BsaveObjectsFind
  DESCRIPTION  : For all classes and their message-handlers, this routine
                   marks all the needed symbols and system functions.
                 Also, it also counts the number of expression structures
                   needed.
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : ExpressionCount (a global from BSAVE.C) is incremented
                   for every expression needed
                 Symbols are marked in their structures
  NOTES        : Also sets bsaveIndex for each class (assumes classes
                   will be bsaved in order of binary list)
 ***************************************************************************/
static void CL_BsaveObjectsFind(
  Environment *theEnv)
  {
   unsigned i;
   SLOT_NAME *snp;

   /* ========================================================
      The counts need to be saved in case a bload is in effect
      ======================================================== */
      CL_Save_BloadCount(theEnv,ObjectBinaryData(theEnv)->ModuleCount);
      CL_Save_BloadCount(theEnv,ObjectBinaryData(theEnv)->ClassCount);
      CL_Save_BloadCount(theEnv,ObjectBinaryData(theEnv)->LinkCount);
      CL_Save_BloadCount(theEnv,ObjectBinaryData(theEnv)->SlotNameCount);
      CL_Save_BloadCount(theEnv,ObjectBinaryData(theEnv)->SlotCount);
      CL_Save_BloadCount(theEnv,ObjectBinaryData(theEnv)->TemplateSlotCount);
      CL_Save_BloadCount(theEnv,ObjectBinaryData(theEnv)->SlotNameMapCount);
      CL_Save_BloadCount(theEnv,ObjectBinaryData(theEnv)->HandlerCount);

   ObjectBinaryData(theEnv)->ModuleCount= 0L;
   ObjectBinaryData(theEnv)->ClassCount = 0L;
   ObjectBinaryData(theEnv)->SlotCount = 0L;
   ObjectBinaryData(theEnv)->SlotNameCount = 0L;
   ObjectBinaryData(theEnv)->LinkCount = 0L;
   ObjectBinaryData(theEnv)->TemplateSlotCount = 0L;
   ObjectBinaryData(theEnv)->SlotNameMapCount = 0L;
   ObjectBinaryData(theEnv)->HandlerCount = 0L;

   /* ==============================================
      Mark items needed by defclasses in all modules
      ============================================== */
      
   ObjectBinaryData(theEnv)->ModuleCount = CL_GetNumberOfDefmodules(theEnv);
   
   CL_DoForAllConstructs(theEnv,MarkDefclassItems,
                      DefclassData(theEnv)->CL_DefclassModuleIndex,
                      false,NULL);

   /* =============================================
      Mark items needed by canonicalized slot names
      ============================================= */
   for (i = 0 ; i < SLOT_NAME_TABLE_HASH_SIZE ; i++)
     for (snp = DefclassData(theEnv)->SlotNameTable[i] ; snp != NULL ; snp = snp->nxt)
       {
        if ((snp->id != ISA_ID) && (snp->id != NAME_ID))
          {
           snp->bsaveIndex = ObjectBinaryData(theEnv)->SlotNameCount++;
           snp->name->neededSymbol = true;
           snp->putHandlerName->neededSymbol = true;
          }
       }
  }

/***************************************************
  NAME         : MarkDefclassItems
  DESCRIPTION  : Marks needed items for a defclass
  INPUTS       : 1) The defclass
                 2) User buffer (ignored)
  RETURNS      : Nothing useful
  SIDE EFFECTS : CL_Bsave indices set and needed
                 ephemerals marked
  NOTES        : None
 ***************************************************/
static void MarkDefclassItems(
  Environment *theEnv,
  ConstructHeader *theDefclass,
  void *buf)
  {
#if MAC_XCD
#pragma unused(buf)
#endif
   Defclass *cls = (Defclass *) theDefclass;
   long i;
   Expression *tmpexp;

   CL_MarkConstructHeaderNeededItems(&cls->header,ObjectBinaryData(theEnv)->ClassCount++);
   ObjectBinaryData(theEnv)->LinkCount += cls->directSuperclasses.classCount +
                cls->directSubclasses.classCount +
                cls->allSuperclasses.classCount;

#if DEFMODULE_CONSTRUCT
   cls->scopeMap->neededBitMap = true;
#endif

   /* ===================================================
      Mark items needed by slot default value expressions
      =================================================== */
   for (i = 0 ; i < cls->slotCount ; i++)
     {
      cls->slots[i].bsaveIndex = ObjectBinaryData(theEnv)->SlotCount++;
      cls->slots[i].overrideMessage->neededSymbol = true;
      if (cls->slots[i].defaultValue != NULL)
        {
         if (cls->slots[i].dynamicDefault)
           {
            ExpressionData(theEnv)->ExpressionCount +=
              CL_ExpressionSize((Expression *) cls->slots[i].defaultValue);
            CL_MarkNeededItems(theEnv,(Expression *) cls->slots[i].defaultValue);
           }
         else
           {
            /* =================================================
               Static default values are stotred as data objects
               and must be converted into expressions
               ================================================= */
            tmpexp =
              CL_ConvertValueToExpression(theEnv,(UDFValue *) cls->slots[i].defaultValue);
            ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(tmpexp);
            CL_MarkNeededItems(theEnv,tmpexp);
            CL_ReturnExpression(theEnv,tmpexp);
           }
        }
     }

   /* ========================================
      Count canonical slots needed by defclass
      ======================================== */
   ObjectBinaryData(theEnv)->TemplateSlotCount += cls->instanceSlotCount;
   if (cls->instanceSlotCount != 0)
     ObjectBinaryData(theEnv)->SlotNameMapCount += cls->maxSlotNameID + 1;

   /* ===============================================
      Mark items needed by defmessage-handler actions
      =============================================== */
   for (i = 0 ; i < cls->handlerCount ; i++)
     {
      cls->handlers[i].header.name->neededSymbol = true;
      ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(cls->handlers[i].actions);
      CL_MarkNeededItems(theEnv,cls->handlers[i].actions);
     }
   ObjectBinaryData(theEnv)->HandlerCount += cls->handlerCount;
  }

/***************************************************
  NAME         : CL_BsaveObjectsExpressions
  DESCRIPTION  : CL_Writes out all expressions needed
                   by classes and handlers
  INPUTS       : The file pointer of the binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : File updated
  NOTES        : None
 ***************************************************/
static void CL_BsaveObjectsExpressions(
  Environment *theEnv,
  FILE *fp)
  {
   if ((ObjectBinaryData(theEnv)->ClassCount == 0L) && (ObjectBinaryData(theEnv)->HandlerCount == 0L))
     return;

   /* ================================================
      CL_Save the defclass slot default value expressions
      ================================================ */
   CL_DoForAllConstructs(theEnv,CL_BsaveDefaultSlotExpressions,DefclassData(theEnv)->CL_DefclassModuleIndex,
                      false,fp);

   /* ==============================================
      CL_Save the defmessage-handler action expressions
      ============================================== */
   CL_DoForAllConstructs(theEnv,CL_BsaveHandlerActionExpressions,DefclassData(theEnv)->CL_DefclassModuleIndex,
                      false,fp);
  }

/***************************************************
  NAME         : CL_BsaveDefaultSlotExpressions
  DESCRIPTION  : CL_Writes expressions for default
                  slot values to binary file
  INPUTS       : 1) The defclass
                 2) The binary file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slot value expressions written
  NOTES        : None
 ***************************************************/
static void CL_BsaveDefaultSlotExpressions(
  Environment *theEnv,
  ConstructHeader *theDefclass,
  void *buf)
  {
   Defclass *cls = (Defclass *) theDefclass;
   long i;
   Expression *tmpexp;

   for (i = 0 ; i < cls->slotCount ; i++)
     {
      if (cls->slots[i].defaultValue != NULL)
        {
         if (cls->slots[i].dynamicDefault)
           CL_BsaveExpression(theEnv,(Expression *) cls->slots[i].defaultValue,(FILE *) buf);
         else
           {
            /* =================================================
               Static default values are stotred as data objects
               and must be converted into expressions
               ================================================= */
            tmpexp =
              CL_ConvertValueToExpression(theEnv,(UDFValue *) cls->slots[i].defaultValue);
            CL_BsaveExpression(theEnv,tmpexp,(FILE *) buf);
            CL_ReturnExpression(theEnv,tmpexp);
           }
        }
     }
  }

/***************************************************
  NAME         : CL_BsaveHandlerActionExpressions
  DESCRIPTION  : CL_Writes expressions for handler
                  actions to binary file
  INPUTS       : 1) The defclass
                 2) The binary file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Handler actions expressions written
  NOTES        : None
 ***************************************************/
static void CL_BsaveHandlerActionExpressions(
  Environment *theEnv,
  ConstructHeader *theDefclass,
  void *buf)
  {
   Defclass *cls = (Defclass *) theDefclass;
   long i;

   for (i = 0 ; i < cls->handlerCount ; i++)
     CL_BsaveExpression(theEnv,cls->handlers[i].actions,(FILE *) buf);
  }

/*************************************************************************************
  NAME         : CL_BsaveStorageObjects
  DESCRIPTION  : CL_Writes out number of each type of structure required for COOL
                 Space required for counts (unsigned long)
                 Number of class modules (long)
                 Number of classes (long)
                 Number of links to classes (long)
                 Number of slots (long)
                 Number of instance template slots (long)
                 Number of handlers (long)
                 Number of definstances (long)
  INPUTS       : File pointer of binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary file adjusted
  NOTES        : None
 *************************************************************************************/
static void CL_BsaveStorageObjects(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;
   long maxClassID;// TBD unsigned short

   if ((ObjectBinaryData(theEnv)->ClassCount == 0L) && (ObjectBinaryData(theEnv)->HandlerCount == 0L))
     {
      space = 0L;
      CL_Gen_Write(&space,sizeof(size_t),fp);
      return;
     }
   space = sizeof(long) * 9;
   CL_Gen_Write(&space,sizeof(size_t),fp); // 64-bit issue changed long to size_t
   CL_Gen_Write(&ObjectBinaryData(theEnv)->ModuleCount,sizeof(long),fp);
   CL_Gen_Write(&ObjectBinaryData(theEnv)->ClassCount,sizeof(long),fp);
   CL_Gen_Write(&ObjectBinaryData(theEnv)->LinkCount,sizeof(long),fp);
   CL_Gen_Write(&ObjectBinaryData(theEnv)->SlotNameCount,sizeof(long),fp);
   CL_Gen_Write(&ObjectBinaryData(theEnv)->SlotCount,sizeof(long),fp);
   CL_Gen_Write(&ObjectBinaryData(theEnv)->TemplateSlotCount,sizeof(long),fp);
   CL_Gen_Write(&ObjectBinaryData(theEnv)->SlotNameMapCount,sizeof(long),fp);
   CL_Gen_Write(&ObjectBinaryData(theEnv)->HandlerCount,sizeof(long),fp);
   maxClassID = DefclassData(theEnv)->MaxClassID;
   CL_Gen_Write(&maxClassID,sizeof(long),fp);
  }

/*************************************************************************************
  NAME         : CL_BsaveObjects
  DESCRIPTION  : CL_Writes out classes and message-handlers in binary fo_rmat
                 Space required (unsigned long)
                 Followed by the data structures in order
  INPUTS       : File pointer of binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary file adjusted
  NOTES        : None
 *************************************************************************************/
static void CL_BsaveObjects(
  Environment *theEnv,
  FILE *fp)
  {
   size_t space;
   Defmodule *theModule;
   DEFCLASS_MODULE *theModuleItem;
   BSAVE_DEFCLASS_MODULE dummy_mitem;
   BSAVE_SLOT_NAME dummy_slot_name;
   SLOT_NAME *snp;
   unsigned i;

   if ((ObjectBinaryData(theEnv)->ClassCount == 0L) && (ObjectBinaryData(theEnv)->HandlerCount == 0L))
     {
      space = 0L;
      CL_Gen_Write(&space,sizeof(size_t),fp);
      return;
     }
   space = (ObjectBinaryData(theEnv)->ModuleCount * sizeof(BSAVE_DEFCLASS_MODULE)) +
           (ObjectBinaryData(theEnv)->ClassCount * sizeof(BSAVE_DEFCLASS)) +
           (ObjectBinaryData(theEnv)->LinkCount * sizeof(long)) +
           (ObjectBinaryData(theEnv)->SlotCount * sizeof(BSAVE_SLOT_DESC)) +
           (ObjectBinaryData(theEnv)->SlotNameCount * sizeof(BSAVE_SLOT_NAME)) +
           (ObjectBinaryData(theEnv)->TemplateSlotCount * sizeof(long)) +
           (ObjectBinaryData(theEnv)->SlotNameMapCount * sizeof(unsigned)) +
           (ObjectBinaryData(theEnv)->HandlerCount * sizeof(BSAVE_HANDLER)) +
           (ObjectBinaryData(theEnv)->HandlerCount * sizeof(unsigned));
   CL_Gen_Write(&space,sizeof(size_t),fp);

   ObjectBinaryData(theEnv)->ClassCount = 0L;
   ObjectBinaryData(theEnv)->LinkCount = 0L;
   ObjectBinaryData(theEnv)->SlotCount = 0L;
   ObjectBinaryData(theEnv)->SlotNameCount = 0L;
   ObjectBinaryData(theEnv)->TemplateSlotCount = 0L;
   ObjectBinaryData(theEnv)->SlotNameMapCount = 0L;
   ObjectBinaryData(theEnv)->HandlerCount = 0L;

   /* =================================
      CL_Write out each defclass module
      ================================= */
   theModule = CL_GetNextDefmodule(theEnv,NULL);
   while (theModule != NULL)
     {
      theModuleItem = (DEFCLASS_MODULE *)
                      CL_GetModuleItem(theEnv,theModule,CL_FindModuleItem(theEnv,"defclass")->moduleIndex);
      CL_Assign_BsaveDefmdlItemHdrVals(&dummy_mitem.header,&theModuleItem->header);
      CL_Gen_Write(&dummy_mitem,sizeof(BSAVE_DEFCLASS_MODULE),fp);
      theModule = CL_GetNextDefmodule(theEnv,theModule);
     }

   /* =====================
      CL_Write out the classes
      ===================== */
   CL_DoForAllConstructs(theEnv,CL_BsaveDefclass,DefclassData(theEnv)->CL_DefclassModuleIndex,false,fp);

   /* =========================
      CL_Write out the class links
      ========================= */
   ObjectBinaryData(theEnv)->LinkCount = 0L;
   CL_DoForAllConstructs(theEnv,CL_BsaveClassLinks,DefclassData(theEnv)->CL_DefclassModuleIndex,false,fp);

   /* ===============================
      CL_Write out the slot name entries
      =============================== */
   for (i = 0 ; i < SLOT_NAME_TABLE_HASH_SIZE ; i++)
     for (snp = DefclassData(theEnv)->SlotNameTable[i] ; snp != NULL ; snp = snp->nxt)
     {
      if ((snp->id != ISA_ID) && (snp->id != NAME_ID))
        {
         dummy_slot_name.id = snp->id;
         dummy_slot_name.hashTableIndex = snp->hashTableIndex;
         dummy_slot_name.name = snp->name->bucket;
         dummy_slot_name.putHandlerName = snp->putHandlerName->bucket;
         CL_Gen_Write(&dummy_slot_name,sizeof(BSAVE_SLOT_NAME),fp);
        }
     }

   /* ===================
      CL_Write out the slots
      =================== */
   CL_DoForAllConstructs(theEnv,CL_BsaveSlots,DefclassData(theEnv)->CL_DefclassModuleIndex,false,fp);

   /* =====================================
      CL_Write out the template instance slots
      ===================================== */
   CL_DoForAllConstructs(theEnv,CL_BsaveTemplateSlots,DefclassData(theEnv)->CL_DefclassModuleIndex,false,fp);

   /* =============================================
      CL_Write out the ordered instance slot name maps
      ============================================= */
   CL_DoForAllConstructs(theEnv,CL_BsaveSlotMap,DefclassData(theEnv)->CL_DefclassModuleIndex,false,fp);

   /* ==============================
      CL_Write out the message-handlers
      ============================== */
   CL_DoForAllConstructs(theEnv,CL_BsaveHandlers,DefclassData(theEnv)->CL_DefclassModuleIndex,false,fp);

   /* ==========================================
      CL_Write out the ordered message-handler maps
      ========================================== */
   CL_DoForAllConstructs(theEnv,CL_BsaveHandlerMap,DefclassData(theEnv)->CL_DefclassModuleIndex,false,fp);

      Restore_BloadCount(theEnv,&ObjectBinaryData(theEnv)->ModuleCount);
      Restore_BloadCount(theEnv,&ObjectBinaryData(theEnv)->ClassCount);
      Restore_BloadCount(theEnv,&ObjectBinaryData(theEnv)->LinkCount);
      Restore_BloadCount(theEnv,&ObjectBinaryData(theEnv)->SlotCount);
      Restore_BloadCount(theEnv,&ObjectBinaryData(theEnv)->SlotNameCount);
      Restore_BloadCount(theEnv,&ObjectBinaryData(theEnv)->TemplateSlotCount);
      Restore_BloadCount(theEnv,&ObjectBinaryData(theEnv)->SlotNameMapCount);
      Restore_BloadCount(theEnv,&ObjectBinaryData(theEnv)->HandlerCount);
  }

/***************************************************
  NAME         : CL_BsaveDefclass
  DESCRIPTION  : CL_Writes defclass binary data
  INPUTS       : 1) The defclass
                 2) The binary file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defclass binary data written
  NOTES        : None
 ***************************************************/
static void CL_BsaveDefclass(
  Environment *theEnv,
  ConstructHeader *theDefclass,
  void *buf)
  {
   Defclass *cls = (Defclass *) theDefclass;
   BSAVE_DEFCLASS dummy_class;

   CL_Assign_BsaveConstructHeaderVals(&dummy_class.header,&cls->header);
   dummy_class.abstract = cls->abstract;
   dummy_class.reactive = cls->reactive;
   dummy_class.system = cls->system;
   dummy_class.id = cls->id;
   dummy_class.slotCount = cls->slotCount;
   dummy_class.instanceSlotCount = cls->instanceSlotCount;
   dummy_class.localInstanceSlotCount = cls->localInstanceSlotCount;
   dummy_class.maxSlotNameID = cls->maxSlotNameID;
   dummy_class.handlerCount = cls->handlerCount;
   dummy_class.directSuperclasses.classCount = cls->directSuperclasses.classCount;
   dummy_class.directSubclasses.classCount = cls->directSubclasses.classCount;
   dummy_class.allSuperclasses.classCount = cls->allSuperclasses.classCount;
   if (cls->directSuperclasses.classCount != 0)
     {
      dummy_class.directSuperclasses.classArray = ObjectBinaryData(theEnv)->LinkCount;
      ObjectBinaryData(theEnv)->LinkCount += cls->directSuperclasses.classCount;
     }
   else
     dummy_class.directSuperclasses.classArray = ULONG_MAX;
   if (cls->directSubclasses.classCount != 0)
     {
      dummy_class.directSubclasses.classArray = ObjectBinaryData(theEnv)->LinkCount;
      ObjectBinaryData(theEnv)->LinkCount += cls->directSubclasses.classCount;
     }
   else
     dummy_class.directSubclasses.classArray = ULONG_MAX;
   if (cls->allSuperclasses.classCount != 0)
     {
      dummy_class.allSuperclasses.classArray = ObjectBinaryData(theEnv)->LinkCount;
      ObjectBinaryData(theEnv)->LinkCount += cls->allSuperclasses.classCount;
     }
   else
     dummy_class.allSuperclasses.classArray = ULONG_MAX;
   if (cls->slots != NULL)
     {
      dummy_class.slots = ObjectBinaryData(theEnv)->SlotCount;
      ObjectBinaryData(theEnv)->SlotCount += cls->slotCount;
     }
   else
     dummy_class.slots = ULONG_MAX;
   if (cls->instanceTemplate != NULL)
     {
      dummy_class.instanceTemplate = ObjectBinaryData(theEnv)->TemplateSlotCount;
      ObjectBinaryData(theEnv)->TemplateSlotCount += cls->instanceSlotCount;
      dummy_class.slotNameMap = ObjectBinaryData(theEnv)->SlotNameMapCount;
      ObjectBinaryData(theEnv)->SlotNameMapCount += cls->maxSlotNameID + 1;
     }
   else
     {
      dummy_class.instanceTemplate = ULONG_MAX;
      dummy_class.slotNameMap = ULONG_MAX;
     }
   if (cls->handlers != NULL)
     {
      dummy_class.handlers = ObjectBinaryData(theEnv)->HandlerCount;
      ObjectBinaryData(theEnv)->HandlerCount += cls->handlerCount;
     }
   else
     dummy_class.handlers = ULONG_MAX;

#if DEFMODULE_CONSTRUCT
   dummy_class.scopeMap = cls->scopeMap->bucket;
#else
   dummy_class.scopeMap = ULONG_MAX;
#endif

#if DEFRULE_CONSTRUCT
   if (cls->relevant_te_rminal_alpha_nodes != NULL)
     { dummy_class.relevant_te_rminal_alpha_nodes = cls->relevant_te_rminal_alpha_nodes->bsaveID; }
   else
     { dummy_class.relevant_te_rminal_alpha_nodes = ULONG_MAX; }
#endif

   CL_Gen_Write(&dummy_class,sizeof(BSAVE_DEFCLASS),(FILE *) buf);
  }

/***************************************************
  NAME         : CL_BsaveClassLinks
  DESCRIPTION  : CL_Writes class links binary data
  INPUTS       : 1) The defclass
                 2) The binary file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defclass links binary data written
  NOTES        : None
 ***************************************************/
static void CL_BsaveClassLinks(
  Environment *theEnv,
  ConstructHeader *theDefclass,
  void *buf)
  {
   Defclass *cls = (Defclass *) theDefclass;
   unsigned long i;
   unsigned long dummy_class_index;
   for (i = 0 ;  i < cls->directSuperclasses.classCount ; i++)
     {
      dummy_class_index = DefclassIndex(cls->directSuperclasses.classArray[i]);
      CL_Gen_Write(&dummy_class_index,sizeof(long),(FILE *) buf);
     }
   ObjectBinaryData(theEnv)->LinkCount += cls->directSuperclasses.classCount;
   for (i = 0 ;  i < cls->directSubclasses.classCount ; i++)
     {
      dummy_class_index = DefclassIndex(cls->directSubclasses.classArray[i]);
      CL_Gen_Write(&dummy_class_index,sizeof(long),(FILE *) buf);
     }
   ObjectBinaryData(theEnv)->LinkCount += cls->directSubclasses.classCount;
   for (i = 0 ;  i < cls->allSuperclasses.classCount ; i++)
     {
      dummy_class_index = DefclassIndex(cls->allSuperclasses.classArray[i]);
      CL_Gen_Write(&dummy_class_index,sizeof(long),(FILE *) buf);
     }
   ObjectBinaryData(theEnv)->LinkCount += cls->allSuperclasses.classCount;
  }

/***************************************************
  NAME         : CL_BsaveSlots
  DESCRIPTION  : CL_Writes class slots binary data
  INPUTS       : 1) The defclass
                 2) The binary file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defclass slots binary data written
  NOTES        : None
 ***************************************************/
static void CL_BsaveSlots(
  Environment *theEnv,
  ConstructHeader *theDefclass,
  void *buf)
  {
   Defclass *cls = (Defclass *) theDefclass;
   long i;
   BSAVE_SLOT_DESC dummy_slot;
   SlotDescriptor *sp;
   Expression *tmpexp;

   for (i = 0 ; i < cls->slotCount ; i++)
     {
      sp = &cls->slots[i];
      dummy_slot.dynamicDefault = sp->dynamicDefault;
      dummy_slot.noDefault = sp->noDefault;
      dummy_slot.shared = sp->shared;
      dummy_slot.multiple = sp->multiple;
      dummy_slot.composite = sp->composite;
      dummy_slot.noInherit = sp->noInherit;
      dummy_slot.no_Write = sp->no_Write;
      dummy_slot.initializeOnly = sp->initializeOnly;
      dummy_slot.reactive = sp->reactive;
      dummy_slot.publicVisibility = sp->publicVisibility;
      dummy_slot.createReadAccessor = sp->createReadAccessor;
      dummy_slot.create_WriteAccessor = sp->create_WriteAccessor;
      dummy_slot.cls = DefclassIndex(sp->cls);
      dummy_slot.slotName = SlotNameIndex(sp->slotName);
      dummy_slot.overrideMessage = sp->overrideMessage->bucket;
      if (sp->defaultValue != NULL)
        {
         dummy_slot.defaultValue = ExpressionData(theEnv)->ExpressionCount;
         if (sp->dynamicDefault)
           ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize((Expression *) sp->defaultValue);
         else
           {
            tmpexp = CL_ConvertValueToExpression(theEnv,(UDFValue *) sp->defaultValue);
            ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(tmpexp);
            CL_ReturnExpression(theEnv,tmpexp);
           }
        }
      else
        dummy_slot.defaultValue = ULONG_MAX;
      dummy_slot.constraint = ConstraintIndex(sp->constraint);
      CL_Gen_Write(&dummy_slot,sizeof(BSAVE_SLOT_DESC),(FILE *) buf);
     }
  }

/**************************************************************
  NAME         : CL_BsaveTemplateSlots
  DESCRIPTION  : CL_Writes class instance template binary data
  INPUTS       : 1) The defclass
                 2) The binary file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defclass instance template binary data written
  NOTES        : None
 **************************************************************/
static void CL_BsaveTemplateSlots(
  Environment *theEnv,
  ConstructHeader *theDefclass,
  void *buf)
  {
   Defclass *cls = (Defclass *) theDefclass;
   unsigned long i;
   unsigned long tsp;
#if MAC_XCD
#pragma unused(theEnv)
#endif

   for (i = 0 ; i < cls->instanceSlotCount ; i++)
     {
      tsp = SlotIndex(cls->instanceTemplate[i]);
      CL_Gen_Write(&tsp,sizeof(unsigned long),(FILE *) buf);
     }
  }

/***************************************************************
  NAME         : CL_BsaveSlotMap
  DESCRIPTION  : CL_Writes class canonical slot map binary data
  INPUTS       : 1) The defclass
                 2) The binary file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defclass canonical slot map binary data written
  NOTES        : None
 ***************************************************************/
static void CL_BsaveSlotMap(
  Environment *theEnv,
  ConstructHeader *theDefclass,
  void *buf)
  {
   Defclass *cls = (Defclass *) theDefclass;
#if MAC_XCD
#pragma unused(theEnv)
#endif

   if (cls->instanceSlotCount != 0)
     CL_Gen_Write(cls->slotNameMap,
              (sizeof(unsigned) * (cls->maxSlotNameID + 1)),(FILE *) buf);
  }

/************************************************************
  NAME         : CL_BsaveHandlers
  DESCRIPTION  : CL_Writes class message-handlers binary data
  INPUTS       : 1) The defclass
                 2) The binary file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defclass message-handler binary data written
  NOTES        : None
 ************************************************************/
static void CL_BsaveHandlers(
  Environment *theEnv,
  ConstructHeader *theDefclass,
  void *buf)
  {
   Defclass *cls = (Defclass *) theDefclass;
   unsigned long i;
   BSAVE_HANDLER dummy_handler;
   DefmessageHandler *hnd;

   for (i = 0 ; i < cls->handlerCount ; i++)
     {
      hnd = &cls->handlers[i];
      
      CL_Assign_BsaveConstructHeaderVals(&dummy_handler.header,&hnd->header);

      dummy_handler.system = hnd->system;
      dummy_handler.type = hnd->type;
      dummy_handler.minParams = hnd->minParams;
      dummy_handler.maxParams = hnd->maxParams;
      dummy_handler.localVarCount = hnd->localVarCount;
      dummy_handler.cls = DefclassIndex(hnd->cls);
      if (hnd->actions != NULL)
        {
         dummy_handler.actions = ExpressionData(theEnv)->ExpressionCount;
         ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(hnd->actions);
        }
      else
        dummy_handler.actions = ULONG_MAX;
      CL_Gen_Write(&dummy_handler,sizeof(BSAVE_HANDLER),(FILE *) buf);
     }
  }

/****************************************************************
  NAME         : CL_BsaveHandlerMap
  DESCRIPTION  : CL_Writes class message-handler map binary data
  INPUTS       : 1) The defclass
                 2) The binary file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Defclass message-handler map binary data written
  NOTES        : None
 ****************************************************************/
static void CL_BsaveHandlerMap(
  Environment *theEnv,
  ConstructHeader *theDefclass,
  void *buf)
  {
   Defclass *cls = (Defclass *) theDefclass;
#if MAC_XCD
#pragma unused(theEnv)
#endif

   CL_Gen_Write(cls->handlerOrderMap,
            (sizeof(unsigned) * cls->handlerCount),(FILE *) buf);
  }

#endif

/***********************************************************************
  NAME         : CL_BloadStorageObjects
  DESCRIPTION  : This routine reads class and handler info_rmation from
                 a binary file in five chunks:
                 Class count
                 Handler count
                 Class array
                 Handler array
  INPUTS       : Notthing
  RETURNS      : Nothing useful
  SIDE EFFECTS : Arrays allocated and set
  NOTES        : This routine makes no attempt to reset any pointers
                   within the structures
                 CL_Bload fails if there are still classes in the system!!
 ***********************************************************************/
static void CL_BloadStorageObjects(
  Environment *theEnv)
  {
   size_t space;
   unsigned long counts[9];

   if ((DefclassData(theEnv)->ClassIDMap != NULL) || (DefclassData(theEnv)->MaxClassID != 0))
     {
      CL_SystemError(theEnv,"OBJBIN",1);
      CL_ExitRouter(theEnv,EXIT_FAILURE);
     }
   CL_GenReadBinary(theEnv,&space,sizeof(size_t));
   if (space == 0L)
     {
      ObjectBinaryData(theEnv)->ClassCount = ObjectBinaryData(theEnv)->HandlerCount = 0L;
      return;
     }
   CL_GenReadBinary(theEnv,counts,space);
   ObjectBinaryData(theEnv)->ModuleCount = counts[0];
   ObjectBinaryData(theEnv)->ClassCount = counts[1];
   ObjectBinaryData(theEnv)->LinkCount = counts[2];
   ObjectBinaryData(theEnv)->SlotNameCount = counts[3];
   ObjectBinaryData(theEnv)->SlotCount = counts[4];
   ObjectBinaryData(theEnv)->TemplateSlotCount = counts[5];
   ObjectBinaryData(theEnv)->SlotNameMapCount = counts[6];
   ObjectBinaryData(theEnv)->HandlerCount = counts[7];
   DefclassData(theEnv)->MaxClassID = (unsigned short) counts[8];
   DefclassData(theEnv)->AvailClassID = (unsigned short) counts[8];
   if (ObjectBinaryData(theEnv)->ModuleCount != 0L)
     {
      space = (sizeof(DEFCLASS_MODULE) * ObjectBinaryData(theEnv)->ModuleCount);
      ObjectBinaryData(theEnv)->ModuleArray = (DEFCLASS_MODULE *) CL_genalloc(theEnv,space);
     }
   if (ObjectBinaryData(theEnv)->ClassCount != 0L)
     {
      space = (sizeof(Defclass) * ObjectBinaryData(theEnv)->ClassCount);
      ObjectBinaryData(theEnv)->DefclassArray = (Defclass *) CL_genalloc(theEnv,space);
      DefclassData(theEnv)->ClassIDMap = (Defclass **) CL_gm2(theEnv,(sizeof(Defclass *) * DefclassData(theEnv)->MaxClassID));
     }
   if (ObjectBinaryData(theEnv)->LinkCount != 0L)
     {
      space = (sizeof(Defclass *) * ObjectBinaryData(theEnv)->LinkCount);
      ObjectBinaryData(theEnv)->LinkArray = (Defclass **) CL_genalloc(theEnv,space);
     }
   if (ObjectBinaryData(theEnv)->SlotCount != 0L)
     {
      space = (sizeof(SlotDescriptor) * ObjectBinaryData(theEnv)->SlotCount);
      ObjectBinaryData(theEnv)->SlotArray = (SlotDescriptor *) CL_genalloc(theEnv,space);
     }
   if (ObjectBinaryData(theEnv)->SlotNameCount != 0L)
     {
      space = (sizeof(SLOT_NAME) * ObjectBinaryData(theEnv)->SlotNameCount);
      ObjectBinaryData(theEnv)->SlotNameArray = (SLOT_NAME *) CL_genalloc(theEnv,space);
     }
   if (ObjectBinaryData(theEnv)->TemplateSlotCount != 0L)
     {
      space = (sizeof(SlotDescriptor *) * ObjectBinaryData(theEnv)->TemplateSlotCount);
      ObjectBinaryData(theEnv)->TmpslotArray = (SlotDescriptor **) CL_genalloc(theEnv,space);
     }
   if (ObjectBinaryData(theEnv)->SlotNameMapCount != 0L)
     {
      space = (sizeof(unsigned) * ObjectBinaryData(theEnv)->SlotNameMapCount);
      ObjectBinaryData(theEnv)->MapslotArray = (unsigned *) CL_genalloc(theEnv,space);
     }
   if (ObjectBinaryData(theEnv)->HandlerCount != 0L)
     {
      space = (sizeof(DefmessageHandler) * ObjectBinaryData(theEnv)->HandlerCount);
      ObjectBinaryData(theEnv)->HandlerArray = (DefmessageHandler *) CL_genalloc(theEnv,space);
      space = (sizeof(unsigned) * ObjectBinaryData(theEnv)->HandlerCount);
      ObjectBinaryData(theEnv)->MaphandlerArray = (unsigned *) CL_genalloc(theEnv,space);
     }
  }

/***************************************************************
  NAME         : CL_BloadObjects
  DESCRIPTION  : This routine moves through the class and handler
                   binary arrays updating pointers
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Pointers reset from array indices
  NOTES        : Assumes all loading is finished
 **************************************************************/
static void CL_BloadObjects(
  Environment *theEnv)
  {
   size_t space;

   CL_GenReadBinary(theEnv,&space,sizeof(size_t));
   if (space == 0L)
     return;
   if (ObjectBinaryData(theEnv)->ModuleCount != 0L)
     CL_Bloadand_Refresh(theEnv,ObjectBinaryData(theEnv)->ModuleCount,sizeof(BSAVE_DEFCLASS_MODULE),Update_DefclassModule);
   if (ObjectBinaryData(theEnv)->ClassCount != 0L)
     {
      CL_Bloadand_Refresh(theEnv,ObjectBinaryData(theEnv)->ClassCount,sizeof(BSAVE_DEFCLASS),UpdateDefclass);
      CL_Bloadand_Refresh(theEnv,ObjectBinaryData(theEnv)->LinkCount,sizeof(long),UpdateLink); // 64-bit bug fix: Defclass * replaced with long
      CL_Bloadand_Refresh(theEnv,ObjectBinaryData(theEnv)->SlotNameCount,sizeof(BSAVE_SLOT_NAME),UpdateSlotName);
      CL_Bloadand_Refresh(theEnv,ObjectBinaryData(theEnv)->SlotCount,sizeof(BSAVE_SLOT_DESC),UpdateSlot);
      if (ObjectBinaryData(theEnv)->TemplateSlotCount != 0L)
        CL_Bloadand_Refresh(theEnv,ObjectBinaryData(theEnv)->TemplateSlotCount,sizeof(long),UpdateTemplateSlot);
      if (ObjectBinaryData(theEnv)->SlotNameMapCount != 0L)
        {
         space = (sizeof(unsigned) * ObjectBinaryData(theEnv)->SlotNameMapCount);
         CL_GenReadBinary(theEnv,ObjectBinaryData(theEnv)->MapslotArray,space);
        }
      if (ObjectBinaryData(theEnv)->HandlerCount != 0L)
        {
         CL_Bloadand_Refresh(theEnv,ObjectBinaryData(theEnv)->HandlerCount,sizeof(BSAVE_HANDLER),UpdateHandler);
         space = (sizeof(unsigned) * ObjectBinaryData(theEnv)->HandlerCount);
         CL_GenReadBinary(theEnv,ObjectBinaryData(theEnv)->MaphandlerArray,space);
        }
      UpdatePrimitiveClassesMap(theEnv);
     }
  }

/***************************************************
  NAME         : UpdatePrimitiveClassesMap
  DESCRIPTION  : CL_Resets the pointers for the global
                 primitive classes map
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : PrimitiveClassMap pointers set
                 into bload array
  NOTES        : Looks at first nine primitive type
                 codes in the source file CONSTANT.H
 ***************************************************/
static void UpdatePrimitiveClassesMap(
  Environment *theEnv)
  {
   unsigned i;

   for (i = 0 ; i < OBJECT_TYPE_CODE ; i++)
     DefclassData(theEnv)->PrimitiveClassMap[i] = (Defclass *) &ObjectBinaryData(theEnv)->DefclassArray[i];
  }

/*********************************************************
  CL_Refresh update routines for bsaved COOL structures
 *********************************************************/
static void Update_DefclassModule(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   BSAVE_DEFCLASS_MODULE *bdptr;

   bdptr = (BSAVE_DEFCLASS_MODULE *) buf;
   CL_UpdateDefmoduleItemHeader(theEnv,&bdptr->header,&ObjectBinaryData(theEnv)->ModuleArray[obji].header,
                             sizeof(Defclass),ObjectBinaryData(theEnv)->DefclassArray);
  }

static void UpdateDefclass(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   BSAVE_DEFCLASS *bcls;
   Defclass *cls;

   bcls = (BSAVE_DEFCLASS *) buf;
   cls = &ObjectBinaryData(theEnv)->DefclassArray[obji];

   CL_UpdateConstructHeader(theEnv,&bcls->header,&cls->header,DEFCLASS,
                         sizeof(DEFCLASS_MODULE),ObjectBinaryData(theEnv)->ModuleArray,
                         sizeof(Defclass),ObjectBinaryData(theEnv)->DefclassArray);
   cls->abstract = bcls->abstract;
   cls->reactive = bcls->reactive;
   cls->system = bcls->system;
   cls->id = bcls->id;
   DefclassData(theEnv)->ClassIDMap[cls->id] = cls;
#if DEBUGGING_FUNCTIONS
   cls->trace_Instances = DefclassData(theEnv)->CL_Watch_Instances;
   cls->traceSlots = DefclassData(theEnv)->CL_WatchSlots;
#endif
   cls->slotCount = bcls->slotCount;
   cls->instanceSlotCount = bcls->instanceSlotCount;
   cls->localInstanceSlotCount = bcls->localInstanceSlotCount;
   cls->maxSlotNameID = bcls->maxSlotNameID;
   cls->handlerCount = bcls->handlerCount;
   cls->directSuperclasses.classCount = bcls->directSuperclasses.classCount;
   cls->directSuperclasses.classArray = LinkPointer(bcls->directSuperclasses.classArray);
   cls->directSubclasses.classCount = bcls->directSubclasses.classCount;
   cls->directSubclasses.classArray = LinkPointer(bcls->directSubclasses.classArray);
   cls->allSuperclasses.classCount = bcls->allSuperclasses.classCount;
   cls->allSuperclasses.classArray = LinkPointer(bcls->allSuperclasses.classArray);
   cls->slots = SlotPointer(bcls->slots);
   cls->instanceTemplate = TemplateSlotPointer(bcls->instanceTemplate);
   cls->slotNameMap = OrderedSlotPointer(bcls->slotNameMap);
   cls->instanceList = NULL;
   cls->handlers = HandlerPointer(bcls->handlers);
   cls->handlerOrderMap = OrderedHandlerPointer(bcls->handlers);
   cls->installed = 1;
   cls->busy = 0;
   cls->instanceList = NULL;
   cls->instanceListBottom = NULL;
   cls->relevant_te_rminal_alpha_nodes = ClassAlphaPointer(bcls->relevant_te_rminal_alpha_nodes);
#if DEFMODULE_CONSTRUCT
   cls->scopeMap = BitMapPointer(bcls->scopeMap);
   IncrementBitMapCount(cls->scopeMap);
#else
   cls->scopeMap = NULL;
#endif
   CL_PutClassInTable(theEnv,cls);
  }

static void UpdateLink(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   unsigned long *blink;

   blink = (unsigned long *) buf;
   ObjectBinaryData(theEnv)->LinkArray[obji] = DefclassPointer(*blink);
  }

static void UpdateSlot(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   SlotDescriptor *sp;
   BSAVE_SLOT_DESC *bsp;

   sp = (SlotDescriptor *) &ObjectBinaryData(theEnv)->SlotArray[obji];
   bsp = (BSAVE_SLOT_DESC *) buf;
   sp->dynamicDefault = bsp->dynamicDefault;
   sp->noDefault = bsp->noDefault;
   sp->shared = bsp->shared;
   sp->multiple = bsp->multiple;
   sp->composite = bsp->composite;
   sp->noInherit = bsp->noInherit;
   sp->no_Write = bsp->no_Write;
   sp->initializeOnly = bsp->initializeOnly;
   sp->reactive = bsp->reactive;
   sp->publicVisibility = bsp->publicVisibility;
   sp->createReadAccessor = bsp->createReadAccessor;
   sp->create_WriteAccessor = bsp->create_WriteAccessor;
   sp->cls = DefclassPointer(bsp->cls);
   sp->slotName = SlotNamePointer(bsp->slotName);
   sp->overrideMessage = SymbolPointer(bsp->overrideMessage);
   IncrementLexemeCount(sp->overrideMessage);
   if (bsp->defaultValue != ULONG_MAX)
     {
      if (sp->dynamicDefault)
        sp->defaultValue = ExpressionPointer(bsp->defaultValue);
      else
        {
         sp->defaultValue = get_struct(theEnv,udfValue);
         CL_EvaluateAndStoreInDataObject(theEnv,sp->multiple,ExpressionPointer(bsp->defaultValue),
                                      (UDFValue *) sp->defaultValue,true);
         CL_RetainUDFV(theEnv,(UDFValue *) sp->defaultValue);
        }
     }
   else
     sp->defaultValue = NULL;
   sp->constraint = ConstraintPointer(bsp->constraint);
   sp->sharedCount = 0;
   sp->sharedValue.value = NULL;
   sp->bsaveIndex = 0L;
   if (sp->shared)
     {
      sp->sharedValue.desc = sp;
      sp->sharedValue.value = NULL;
     }
  }

static void UpdateSlotName(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   SLOT_NAME *snp;
   BSAVE_SLOT_NAME *bsnp;

   bsnp = (BSAVE_SLOT_NAME *) buf;
   snp = (SLOT_NAME *) &ObjectBinaryData(theEnv)->SlotNameArray[obji];
   snp->id = bsnp->id;
   snp->name = SymbolPointer(bsnp->name);
   IncrementLexemeCount(snp->name);
   snp->putHandlerName = SymbolPointer(bsnp->putHandlerName);
   IncrementLexemeCount(snp->putHandlerName);
   snp->hashTableIndex = bsnp->hashTableIndex;
   snp->nxt = DefclassData(theEnv)->SlotNameTable[snp->hashTableIndex];
   DefclassData(theEnv)->SlotNameTable[snp->hashTableIndex] = snp;
  }

static void UpdateTemplateSlot(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   ObjectBinaryData(theEnv)->TmpslotArray[obji] = SlotPointer(* (long *) buf);
  }

static void UpdateHandler(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   DefmessageHandler *hnd;
   BSAVE_HANDLER *bhnd;

   hnd = &ObjectBinaryData(theEnv)->HandlerArray[obji];
   bhnd = (BSAVE_HANDLER *) buf;
   hnd->system = bhnd->system;
   hnd->type = bhnd->type;

   CL_UpdateConstructHeader(theEnv,&bhnd->header,&hnd->header,DEFMESSAGE_HANDLER,
                         sizeof(DEFCLASS_MODULE),ObjectBinaryData(theEnv)->ModuleArray,
                         sizeof(DefmessageHandler),ObjectBinaryData(theEnv)->HandlerArray);

   hnd->minParams = bhnd->minParams;
   hnd->maxParams = bhnd->maxParams;
   hnd->localVarCount = bhnd->localVarCount;
   hnd->cls = DefclassPointer(bhnd->cls);
   //IncrementLexemeCount(hnd->header.name);
   hnd->actions = ExpressionPointer(bhnd->actions);
   hnd->header.ppFo_rm = NULL;
   hnd->busy = 0;
   hnd->mark = 0;
   hnd->header.usrData = NULL;
#if DEBUGGING_FUNCTIONS
   hnd->trace = MessageHandlerData(theEnv)->CL_WatchHandlers;
#endif
  }

/***************************************************************
  NAME         : CL_Clear_BloadObjects
  DESCRIPTION  : CL_Release all binary-loaded class and handler
                   structure arrays (and others)
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Memory cleared
  NOTES        : None
 ***************************************************************/
static void CL_Clear_BloadObjects(
  Environment *theEnv)
  {
   unsigned long i;
   size_t space;

   space = (sizeof(DEFCLASS_MODULE) * ObjectBinaryData(theEnv)->ModuleCount);
   if (space == 0L)
     return;
   CL_genfree(theEnv,ObjectBinaryData(theEnv)->ModuleArray,space);
   ObjectBinaryData(theEnv)->ModuleArray = NULL;
   ObjectBinaryData(theEnv)->ModuleCount = 0L;

   if (ObjectBinaryData(theEnv)->ClassCount != 0L)
     {
      CL_rm(theEnv,DefclassData(theEnv)->ClassIDMap,(sizeof(Defclass *) * DefclassData(theEnv)->AvailClassID));
      DefclassData(theEnv)->ClassIDMap = NULL;
      DefclassData(theEnv)->MaxClassID = 0;
      DefclassData(theEnv)->AvailClassID = 0;
      for (i = 0 ; i < ObjectBinaryData(theEnv)->ClassCount ; i++)
        {
         CL_UnmarkConstructHeader(theEnv,&ObjectBinaryData(theEnv)->DefclassArray[i].header);
#if DEFMODULE_CONSTRUCT
         CL_DecrementBitMapReferenceCount(theEnv,ObjectBinaryData(theEnv)->DefclassArray[i].scopeMap);
#endif
         CL_RemoveClassFromTable(theEnv,&ObjectBinaryData(theEnv)->DefclassArray[i]);
        }
      for (i = 0 ; i < ObjectBinaryData(theEnv)->SlotCount ; i++)
        {
         CL_ReleaseLexeme(theEnv,ObjectBinaryData(theEnv)->SlotArray[i].overrideMessage);
         if ((ObjectBinaryData(theEnv)->SlotArray[i].defaultValue != NULL) && (ObjectBinaryData(theEnv)->SlotArray[i].dynamicDefault == 0))
           {
            CL_ReleaseUDFV(theEnv,(UDFValue *) ObjectBinaryData(theEnv)->SlotArray[i].defaultValue);
            rtn_struct(theEnv,udfValue,ObjectBinaryData(theEnv)->SlotArray[i].defaultValue);
           }
        }
      for (i = 0 ; i < ObjectBinaryData(theEnv)->SlotNameCount ; i++)
        {
         DefclassData(theEnv)->SlotNameTable[ObjectBinaryData(theEnv)->SlotNameArray[i].hashTableIndex] = NULL;
         CL_ReleaseLexeme(theEnv,ObjectBinaryData(theEnv)->SlotNameArray[i].name);
         CL_ReleaseLexeme(theEnv,ObjectBinaryData(theEnv)->SlotNameArray[i].putHandlerName);
        }

      space = (sizeof(Defclass) * ObjectBinaryData(theEnv)->ClassCount);
      if (space != 0L)
        {
         CL_genfree(theEnv,ObjectBinaryData(theEnv)->DefclassArray,space);
         ObjectBinaryData(theEnv)->DefclassArray = NULL;
         ObjectBinaryData(theEnv)->ClassCount = 0L;
        }

      space = (sizeof(Defclass *) * ObjectBinaryData(theEnv)->LinkCount);
      if (space != 0L)
        {
         CL_genfree(theEnv,ObjectBinaryData(theEnv)->LinkArray,space);
         ObjectBinaryData(theEnv)->LinkArray = NULL;
         ObjectBinaryData(theEnv)->LinkCount = 0L;
        }

      space = (sizeof(SlotDescriptor) * ObjectBinaryData(theEnv)->SlotCount);
      if (space != 0L)
        {
         CL_genfree(theEnv,ObjectBinaryData(theEnv)->SlotArray,space);
         ObjectBinaryData(theEnv)->SlotArray = NULL;
         ObjectBinaryData(theEnv)->SlotCount = 0L;
        }

      space = (sizeof(SLOT_NAME) * ObjectBinaryData(theEnv)->SlotNameCount);
      if (space != 0L)
        {
         CL_genfree(theEnv,ObjectBinaryData(theEnv)->SlotNameArray,space);
         ObjectBinaryData(theEnv)->SlotNameArray = NULL;
         ObjectBinaryData(theEnv)->SlotNameCount = 0L;
        }

      space = (sizeof(SlotDescriptor *) * ObjectBinaryData(theEnv)->TemplateSlotCount);
      if (space != 0L)
        {
         CL_genfree(theEnv,ObjectBinaryData(theEnv)->TmpslotArray,space);
         ObjectBinaryData(theEnv)->TmpslotArray = NULL;
         ObjectBinaryData(theEnv)->TemplateSlotCount = 0L;
        }

      space = (sizeof(unsigned) * ObjectBinaryData(theEnv)->SlotNameMapCount);
      if (space != 0L)
        {
         CL_genfree(theEnv,ObjectBinaryData(theEnv)->MapslotArray,space);
         ObjectBinaryData(theEnv)->MapslotArray = NULL;
         ObjectBinaryData(theEnv)->SlotNameMapCount = 0L;
        }
     }

   if (ObjectBinaryData(theEnv)->HandlerCount != 0L)
     {
      for (i = 0L ; i < ObjectBinaryData(theEnv)->HandlerCount ; i++)
        CL_ReleaseLexeme(theEnv,ObjectBinaryData(theEnv)->HandlerArray[i].header.name);

      space = (sizeof(DefmessageHandler) * ObjectBinaryData(theEnv)->HandlerCount);
      if (space != 0L)
        {
         CL_genfree(theEnv,ObjectBinaryData(theEnv)->HandlerArray,space);
         ObjectBinaryData(theEnv)->HandlerArray = NULL;
         space = (sizeof(unsigned) * ObjectBinaryData(theEnv)->HandlerCount);
         CL_genfree(theEnv,ObjectBinaryData(theEnv)->MaphandlerArray,space);
         ObjectBinaryData(theEnv)->MaphandlerArray = NULL;
         ObjectBinaryData(theEnv)->HandlerCount = 0L;
        }
     }
  }

#endif

