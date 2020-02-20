   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  01/21/18            */
   /*                                                     */
   /*             CLASS FUNCTIONS HEADER FILE             */
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
/*      6.24: Converted INSTANCE_PATTERN_MATCHING to         */
/*            DEFRULE_CONSTRUCT.                             */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
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
/*      6.31: Optimization of slot ID creation previously    */
/*            provided by NewSlotNameID function.            */
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
/*            Removed initial-object support.                */
/*                                                           */
/*************************************************************/

#ifndef _H_classfun

#pragma once

#define _H_classfun

#include "object.h"
#include "scanner.h"

#define TestTraversalID(traversalRecord,id) TestBitMap(traversalRecord,id)
#define SetTraversalID(traversalRecord,id) SetBitMap(traversalRecord,id)
#define CL_ClearTraversalID(traversalRecord,id) CL_ClearBitMap(traversalRecord,id)

#define CLASS_TABLE_HASH_SIZE     167 // TBD Larger?
#define SLOT_NAME_TABLE_HASH_SIZE 167 // TBD Larger?

#define ISA_ID  0
#define NAME_ID 1

#define SLOT_NAME_NOT_FOUND USHRT_MAX

   void                           CL_IncrementDefclassBusyCount(Environment *,Defclass *);
   void                           CL_DecrementDefclassBusyCount(Environment *,Defclass *);
   bool                           CL_InstancesPurge(Environment *,void *);

#if ! RUN_TIME
   void                           CL_InitializeClasses(Environment *);
#endif
   SlotDescriptor                *CL_FindClassSlot(Defclass *,CLIPSLexeme *);
   void                           CL_ClassExistError(Environment *,const char *,const char *);
   void                           CL_DeleteClassLinks(Environment *,CLASS_LINK *);
   void                           CL_PrintClassName(Environment *,const char *,Defclass *,bool,bool);

#if DEBUGGING_FUNCTIONS || ((! BLOAD_ONLY) && (! RUN_TIME))
   void                           CL_PrintPackedClassLinks(Environment *,const char *,const char *,PACKED_CLASS_LINKS *);
#endif

#if ! RUN_TIME
   void                           CL_PutClassInTable(Environment *,Defclass *);
   void                           CL_RemoveClassFromTable(Environment *,Defclass *);
   void                           CL_AddClassLink(Environment *,PACKED_CLASS_LINKS *,Defclass *,bool,unsigned int);
   void                           CL_DeleteSubclassLink(Environment *,Defclass *,Defclass *);
   void                           CL_DeleteSuperclassLink(Environment *,Defclass *,Defclass *);
   Defclass                      *CL_NewClass(Environment *,CLIPSLexeme *);
   void                           CL_DeletePackedClassLinks(Environment *,PACKED_CLASS_LINKS *,bool);
   void                           CL_AssignClassID(Environment *,Defclass *);
   SLOT_NAME                     *CL_AddSlotName(Environment *,CLIPSLexeme *,unsigned short,bool);
   void                           CL_DeleteSlotName(Environment *,SLOT_NAME *);
   void                           CL_RemoveDefclass(Environment *,Defclass *);
   void                           CL_InstallClass(Environment *,Defclass *,bool);
#endif
   void                           CL_DestroyDefclass(Environment *,Defclass *);

#if (! BLOAD_ONLY) && (! RUN_TIME)
   bool                           CL_IsClassBeingUsed(Defclass *);
   bool                           CL_RemoveAllUserClasses(Environment *);
   bool                           CL_DeleteClassUAG(Environment *,Defclass *);
   void                           CL_MarkBitMapSubclasses(char *,Defclass *,int);
#endif

   unsigned short                 CL_FindSlotNameID(Environment *,CLIPSLexeme *);
   CLIPSLexeme                   *CL_FindIDSlotName(Environment *,unsigned short);
   SLOT_NAME                     *CL_FindIDSlotNameHash(Environment *,unsigned short); 
   int                            CL_GetTraversalID(Environment *);
   void                           CL_ReleaseTraversalID(Environment *);
   unsigned int                   CL_HashClass(CLIPSLexeme *);

#define DEFCLASS_DATA 21

#define PRIMITIVE_CLASSES 9

#include "classcom.h"

struct defclassData
  {
   Construct *DefclassConstruct;
   unsigned CL_DefclassModuleIndex;
   EntityRecord DefclassEntityRecord;
   Defclass *PrimitiveClassMap[PRIMITIVE_CLASSES];
   Defclass **ClassIDMap;
   Defclass **ClassTable;
   unsigned short MaxClassID;
   unsigned short AvailClassID;
   SLOT_NAME **SlotNameTable;
   CLIPSLexeme *ISA_SYMBOL;
   CLIPSLexeme *NAME_SYMBOL;
#if DEBUGGING_FUNCTIONS
   bool CL_WatchCL_Instances;
   bool CL_WatchSlots;
#endif
   unsigned short CTID;
   struct token ObjectParseToken;
   ClassDefaultsMode ClassDefaultsModeValue;
   int newSlotID;
  };

#define DefclassData(theEnv) ((struct defclassData *) GetEnvironmentData(theEnv,DEFCLASS_DATA))

#endif









