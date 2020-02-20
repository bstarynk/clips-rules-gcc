   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/30/16            */
   /*                                                     */
   /*                                                     */
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
/*      6.24: Removed IMPERATIVE_MESSAGE_HANDLERS and        */
/*            AUXILIARY_MESSAGE_HANDLERS compilation flags.  */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#ifndef _H_objbin

#pragma once

#define _H_objbin

#include "object.h"

#define OBJECTBIN_DATA 33

struct objectBinaryData
  {
   Defclass *DefclassArray;
   unsigned long ModuleCount;
   unsigned long ClassCount;
   unsigned long LinkCount;
   unsigned long SlotCount;
   unsigned long SlotNameCount;
   unsigned long TemplateSlotCount;
   unsigned long SlotNameMapCount;
   unsigned long HandlerCount;
   DEFCLASS_MODULE *ModuleArray;
   Defclass **LinkArray;
   SlotDescriptor *SlotArray;
   SlotDescriptor **TmpslotArray;
   SLOT_NAME *SlotNameArray;
   unsigned *MapslotArray;
   DefmessageHandler *HandlerArray;
   unsigned *MaphandlerArray;
  };

#define ObjectBinaryData(theEnv) ((struct objectBinaryData *) GetEnvironmentData(theEnv,OBJECTBIN_DATA))

#define DefclassPointer(i) (((i) == ULONG_MAX) ? NULL : &ObjectBinaryData(theEnv)->DefclassArray[i])
#define DefclassIndex(cls) (((cls) == NULL) ? ULONG_MAX : ((ConstructHeader *) cls)->bsaveID)

   void                    SetupObjectsCL_Bload(Environment *);
   void                   *CL_BloadCL_DefclassModuleReference(Environment *,unsigned long);

#endif /* _H_objbin */



