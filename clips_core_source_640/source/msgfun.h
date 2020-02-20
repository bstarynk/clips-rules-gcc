   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/01/16            */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Message-passing support functions                */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Removed IMPERATIVE_MESSAGE_HANDLERS and        */
/*            AUXILIARY_MESSAGE_HANDLERS compilation flags.  */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
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
/*************************************************************/

#ifndef _H_msgfun

#pragma once

#define _H_msgfun

typedef struct handlerSlotReference
  {
   unsigned classID;
   unsigned short slotID;
  } HANDLER_SLOT_REFERENCE;

#include "msgpass.h"
#include "object.h"

#define BEGIN_TRACE ">>"
#define END_TRACE   "<<"

/* =================================================================================
   Message-handler types - don't change these values: a string array depends on them
   ================================================================================= */
#define MAROUND        0
#define MBEFORE        1
#define MPRIMARY       2
#define MAFTER         3
#define MERROR         4

#define LOOKUP_HANDLER_INDEX   0
#define LOOKUP_HANDLER_ADDRESS 1

   void             CL_UnboundHandlerErr(Environment *,const char *);
   void             CL_PrintNoHandlerError(Environment *,const char *);
   bool             CL_CheckHandlerArgCount(Environment *);
   void             CL_SlotAccessViolationError(Environment *,const char *,Instance *,Defclass *);
   void             CL_SlotVisibilityViolationError(Environment *,SlotDescriptor *,Defclass *,bool);

#if ! RUN_TIME
   void             CL_NewSystemHandler(Environment *,const char *,const char *,const char *,unsigned short);
   DefmessageHandler
                   *CL_InsertHandlerHeader(Environment *,Defclass *,CLIPSLexeme *,unsigned);
#endif

#if (! BLOAD_ONLY) && (! RUN_TIME)
   DefmessageHandler
                   *NewHandler(void);
   bool             CL_HandlersExecuting(Defclass *);
   bool             CL_DeleteHandler(Environment *,Defclass *,CLIPSLexeme *,int,bool);
   void             CL_DeallocateMarkedHandlers(Environment *,Defclass *);
#endif
   unsigned short   CL_HandlerType(Environment *,const char *,bool,const char *);
   bool             CL_CheckCurrentMessage(Environment *,const char *,bool);
   void             CL_PrintHandler(Environment *,const char *,DefmessageHandler *,bool,bool);
   DefmessageHandler
                   *CL_FindHandlerByAddress(Defclass *,CLIPSLexeme *,unsigned);
   int              CL_FindHandlerByIndex(Defclass *,CLIPSLexeme *,unsigned);
   int              CL_FindHandlerNameGroup(Defclass *,CLIPSLexeme *);
   void             CL_HandlerDeleteError(Environment *,const char *);

#if DEBUGGING_FUNCTIONS
   void             CL_DisplayCore(Environment *,const char *,HANDLER_LINK *,int);
   HANDLER_LINK    *CL_FindPreviewApplicableHandlers(Environment *,Defclass *,CLIPSLexeme *);
   void             CL_WatchMessage(Environment *,const char *,const char *);
   void             CL_WatchHandler(Environment *,const char *,HANDLER_LINK *,const char *);
#endif

#endif /* _H_msgfun */







