   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/01/16            */
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
/*      6.23: Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Removed IMPERATIVE_MESSAGE_HANDLERS            */
/*                    compilation flag.                      */
/*                                                           */
/*            Corrected code to remove run-time program      */
/*            compiler warnings.                             */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added DeallocateMessageHandlerData to          */
/*            deallocate message handler environment data.   */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
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
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#ifndef _H_msgcom

#pragma once

#define _H_msgcom

#include "msgpass.h"
#include "object.h"

#define MESSAGE_HANDLER_DATA 32

struct messageHandlerData
  {
   EntityRecord HandlerGetInfo;
   EntityRecord HandlerPutInfo;
   CLIPSLexeme *INIT_SYMBOL;
   CLIPSLexeme *DELETE_SYMBOL;
   CLIPSLexeme *CREATE_SYMBOL;
#if DEBUGGING_FUNCTIONS
   bool CL_WatchHandlers;
   bool CL_WatchMessages;
#endif
   const char *hndquals[4];
   CLIPSLexeme *SELF_SYMBOL;
   CLIPSLexeme *CurrentMessageName;
   HANDLER_LINK *CurrentCore;
   HANDLER_LINK *TopOfCore;
   HANDLER_LINK *NextInCore;
   HANDLER_LINK *OldCore;
  };

#define MessageHandlerData(theEnv) ((struct messageHandlerData *) GetEnvironmentData(theEnv,MESSAGE_HANDLER_DATA))

#define INIT_STRING   "init"
#define DELETE_STRING "delete"
#define PRINT_STRING  "print"
#define CREATE_STRING "create"

   void             CL_SetupMessageHandlers(Environment *);
   const char      *CL_DefmessageHandlerName(Defclass *,unsigned);
   const char      *CL_Defmessage_HandlerType(Defclass *,unsigned);
   unsigned         CL_GetNextDefmessageHandler(Defclass *,unsigned);
   DefmessageHandler
                   *CL_GetDefmessageHandlerPointer(Defclass *,unsigned int);
#if DEBUGGING_FUNCTIONS
   bool             CL_DefmessageHandlerGet_Watch(Defclass *,unsigned);
   void             CL_DefmessageHandlerSet_Watch(Defclass *,unsigned,bool);
#endif
   unsigned         CL_FindDefmessageHandler(Defclass *,const char *,const char *);
   bool             CL_DefmessageHandlerIsDeletable(Defclass *,unsigned);
   void             CL_UndefmessageHandlerCommand(Environment *,UDFContext *,UDFValue *);
   bool             CL_UndefmessageHandler(Defclass *,unsigned,Environment *);
#if DEBUGGING_FUNCTIONS
   void             CL_PPDefmessageHandlerCommand(Environment *,UDFContext *,UDFValue *);
   void             CL_ListDefmessageHandlersCommand(Environment *,UDFContext *,UDFValue *);
   void             CL_Preview_SendCommand(Environment *,UDFContext *,UDFValue *);
   const char      *CL_DefmessageHandlerPPFo_rm(Defclass *,unsigned);
   void             CL_ListDefmessageHandlers(Environment *,Defclass *,const char *,bool);
   void             CL_Preview_Send(Defclass *,const char *,const char *);
   unsigned long    CL_DisplayHandlersInLinks(Environment *,const char *,PACKED_CLASS_LINKS *,unsigned int);
#endif

#endif /* _H_msgcom */





