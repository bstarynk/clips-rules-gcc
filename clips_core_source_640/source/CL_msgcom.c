   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/02/18             */
   /*                                                     */
   /*                OBJECT MESSAGE COMMANDS              */
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
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            Pretty print functions accept optional logical */
/*            name argument.                                 */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if OBJECT_SYSTEM

#include <string.h>

#include "argacces.h"
#if BLOAD || BLOAD_AND_BSAVE
#include "bload.h"
#endif
#include "classcom.h"
#include "classfun.h"
#include "classinf.h"
#if (! BLOAD_ONLY) && (! RUN_TIME)
#include "constrct.h"
#include "msgpsr.h"
#endif
#include "envrnmnt.h"
#if ! RUN_TIME
#include "extnfunc.h"
#endif
#include "insfun.h"
#include "insmoddp.h"
#include "msgfun.h"
#include "msgpass.h"
#include "memalloc.h"
#include "prccode.h"
#include "prntutil.h"
#include "router.h"
#if DEBUGGING_FUNCTIONS
#include "watch.h"
#endif

#include "msgcom.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if ! RUN_TIME
   static void                    CreateSystemHandlers(Environment *,void *);
#endif

#if (! BLOAD_ONLY) && (! RUN_TIME)
   static bool                    Wild_DeleteHandler(Environment *,Defclass *,CLIPSLexeme *,const char *);
#endif

#if DEBUGGING_FUNCTIONS
   static bool                    DefmessageHandler_WatchAccess(Environment *,int,bool,Expression *);
   static bool                    DefmessageHandler_WatchPrint(Environment *,const char *,int,Expression *);
   static bool                    DefmessageHandler_WatchSupport(Environment *,const char *,const char *,bool,
                                                                void (*)(Environment *,const char *,Defclass *,unsigned),
                                                                void (*)(Defclass *,unsigned,bool),
                                                                Expression *);
   static bool                    CL_WatchClassHandlers(Environment *,Defclass *,const char *,int,const char *,bool,bool,
                                                     void (*)(Environment *,const char *,Defclass *,unsigned),
                                                     void (*)(Defclass *,unsigned,bool));
   static void                    CL_PrintHandler_WatchFlag(Environment *,const char *,Defclass *,unsigned);
#endif

   static void                    DeallocateMessageHandlerData(Environment *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************
  NAME         : CL_SetupMessageHandlers
  DESCRIPTION  : Sets up internal symbols and
                 fucntion definitions pertaining to
                 message-handlers.  Also creates
                 system handlers
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Functions and data structures
                 initialized
  NOTES        : Should be called before
                 CL_SetupInstanceModDupCommands() in
                 INSMODDP.C
 ***************************************************/
void CL_SetupMessageHandlers(
  Environment *theEnv)
  {
   EntityRecord handlerGetInfo = { "HANDLER_GET", HANDLER_GET,0,1,1,
                                        Print_HandlerSlotGetFunction,
                                        Print_HandlerSlotGetFunction,NULL,
                                        CL_HandlerSlotGetFunction,
                                        NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL },

                 handlerPutInfo = { "HANDLER_PUT", HANDLER_PUT,0,1,1,
                                        Print_HandlerSlotPutFunction,
                                        Print_HandlerSlotPutFunction,NULL,
                                        CL_HandlerSlotPutFunction,
                                        NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

   CL_AllocateEnvironmentData(theEnv,MESSAGE_HANDLER_DATA,sizeof(struct messageHandlerData),DeallocateMessageHandlerData);
   memcpy(&MessageHandlerData(theEnv)->HandlerGetInfo,&handlerGetInfo,sizeof(struct entityRecord));
   memcpy(&MessageHandlerData(theEnv)->HandlerPutInfo,&handlerPutInfo,sizeof(struct entityRecord));

   MessageHandlerData(theEnv)->hndquals[0] = "around";
   MessageHandlerData(theEnv)->hndquals[1] = "before";
   MessageHandlerData(theEnv)->hndquals[2] = "primary";
   MessageHandlerData(theEnv)->hndquals[3] = "after";

   CL_InstallPrimitive(theEnv,&MessageHandlerData(theEnv)->HandlerGetInfo,HANDLER_GET);
   CL_InstallPrimitive(theEnv,&MessageHandlerData(theEnv)->HandlerPutInfo,HANDLER_PUT);

#if ! RUN_TIME
   MessageHandlerData(theEnv)->INIT_SYMBOL = CL_CreateSymbol(theEnv,INIT_STRING);
   IncrementLexemeCount(MessageHandlerData(theEnv)->INIT_SYMBOL);

   MessageHandlerData(theEnv)->DELETE_SYMBOL = CL_CreateSymbol(theEnv,DELETE_STRING);
   IncrementLexemeCount(MessageHandlerData(theEnv)->DELETE_SYMBOL);

   MessageHandlerData(theEnv)->CREATE_SYMBOL = CL_CreateSymbol(theEnv,CREATE_STRING);
   IncrementLexemeCount(MessageHandlerData(theEnv)->CREATE_SYMBOL);

   CL_Add_ClearFunction(theEnv,"defclass",CreateSystemHandlers,-100,NULL);

#if ! BLOAD_ONLY
   MessageHandlerData(theEnv)->SELF_SYMBOL = CL_CreateSymbol(theEnv,SELF_STRING);
   IncrementLexemeCount(MessageHandlerData(theEnv)->SELF_SYMBOL);

   CL_AddConstruct(theEnv,"defmessage-handler","defmessage-handlers",
                CL_ParseDefmessageHandler,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
   CL_AddUDF(theEnv,"undefmessage-handler","v",2,3,"y",CL_UndefmessageHandlerCommand,"CL_UndefmessageHandlerCommand",NULL);

#endif

   CL_AddUDF(theEnv,"send","*",2,UNBOUNDED,"*;*;y",CL_SendCommand,"CL_SendCommand",NULL);

#if DEBUGGING_FUNCTIONS
   CL_AddUDF(theEnv,"preview-send","v",2,2,"y",CL_Preview_SendCommand,"CL_Preview_SendCommand",NULL);

   CL_AddUDF(theEnv,"ppdefmessage-handler","v",2,4,"y",CL_PPDefmessageHandlerCommand,"CL_PPDefmessageHandlerCommand",NULL);
   CL_AddUDF(theEnv,"list-defmessage-handlers","v",0,2,"y",CL_ListDefmessageHandlersCommand,"CL_ListDefmessageHandlersCommand",NULL);
#endif

   CL_AddUDF(theEnv,"next-handlerp","b",0,0,NULL,CL_NextHandlerAvailableFunction,"CL_NextHandlerAvailableFunction",NULL);
   CL_FuncSeqOvlFlags(theEnv,"next-handlerp",true,false);
   CL_AddUDF(theEnv,"call-next-handler","*",0,0,NULL,CL_CallNextHandler,"CL_CallNextHandler",NULL);
   CL_FuncSeqOvlFlags(theEnv,"call-next-handler",true,false);
   CL_AddUDF(theEnv,"override-next-handler","*",0,UNBOUNDED,NULL,CL_CallNextHandler,"CL_CallNextHandler",NULL);
   CL_FuncSeqOvlFlags(theEnv,"override-next-handler",true,false);

   CL_AddUDF(theEnv,"dynamic-get","*",1,1,"y",CL_DynamicHandlerGetSlot,"CL_DynamicHandlerGetSlot",NULL);
   CL_AddUDF(theEnv,"dynamic-put","*",1,UNBOUNDED,"*;y",CL_DynamicHandlerPutSlot,"CL_DynamicHandlerPutSlot",NULL);
   CL_AddUDF(theEnv,"get","*",1,1,"y",CL_DynamicHandlerGetSlot,"CL_DynamicHandlerGetSlot",NULL);
   CL_AddUDF(theEnv,"put","*",1,UNBOUNDED,"*;y",CL_DynamicHandlerPutSlot,"CL_DynamicHandlerPutSlot",NULL);
#endif

#if DEBUGGING_FUNCTIONS
   CL_Add_WatchItem(theEnv,"messages",0,&MessageHandlerData(theEnv)->CL_WatchMessages,36,NULL,NULL);
   CL_Add_WatchItem(theEnv,"message-handlers",0,&MessageHandlerData(theEnv)->CL_WatchHandlers,35,
                DefmessageHandler_WatchAccess,DefmessageHandler_WatchPrint);
#endif
  }

/*******************************************************/
/* DeallocateMessageHandlerData: Deallocates environment */
/*    data for the message handler functionality.        */
/******************************************************/
static void DeallocateMessageHandlerData(
  Environment *theEnv)
  {
   HANDLER_LINK *tmp, *mhead, *chead;

   mhead = MessageHandlerData(theEnv)->TopOfCore;
   while (mhead != NULL)
     {
      tmp = mhead;
      mhead = mhead->nxt;
      rtn_struct(theEnv,messageHandlerLink,tmp);
     }

   chead = MessageHandlerData(theEnv)->OldCore;
   while (chead != NULL)
     {
      mhead = chead;
      chead = chead->nxtInStack;

      while (mhead != NULL)
        {
         tmp = mhead;
         mhead = mhead->nxt;
         rtn_struct(theEnv,messageHandlerLink,tmp);
        }
     }
  }

/*****************************************************
  NAME         : CL_DefmessageHandlerName
  DESCRIPTION  : Gets the name of a message-handler
  INPUTS       : 1) Pointer to a class
                 2) Array index of handler in class's
                    message-handler array (+1)
  RETURNS      : Name-string of message-handler
  SIDE EFFECTS : None
  NOTES        : None
 *****************************************************/
const char *CL_DefmessageHandlerName(
  Defclass *theDefclass,
  unsigned theIndex)
  {
   return theDefclass->handlers[theIndex-1].header.name->contents;
  }

/*****************************************************
  NAME         : CL_Defmessage_HandlerType
  DESCRIPTION  : Gets the type of a message-handler
  INPUTS       : 1) Pointer to a class
                 2) Array index of handler in class's
                    message-handler array (+1)
  RETURNS      : Type-string of message-handler
  SIDE EFFECTS : None
  NOTES        : None
 *****************************************************/
const char *CL_Defmessage_HandlerType(
  Defclass *theDefclass,
  unsigned theIndex)
  {
   Environment *theEnv = theDefclass->header.env;
   
   return MessageHandlerData(theEnv)->hndquals[theDefclass->handlers[theIndex-1].type];
  }

/**************************************************************
  NAME         : CL_GetNextDefmessageHandler
  DESCRIPTION  : Finds first or next handler for a class
  INPUTS       : 1) The address of the handler's class
                 2) The array index of the current handler (+1)
  RETURNS      : The array index (+1) of the next handler, or 0
                   if there is none
  SIDE EFFECTS : None
  NOTES        : If index == 0, the first handler array index
                 (i.e. 1) returned
 **************************************************************/
unsigned CL_GetNextDefmessageHandler(
  Defclass *theDefclass,
  unsigned theIndex)
  {
   if (theIndex == 0)
     { return (theDefclass->handlers != NULL) ? 1 : 0; }

   if (theIndex == theDefclass->handlerCount)
     { return 0; }

   return theIndex+1;
  }

/*****************************************************
  NAME         : CL_GetDefmessageHandlerPointer
  DESCRIPTION  : Returns a pointer to a handler
  INPUTS       : 1) Pointer to a class
                 2) Array index of handler in class's
                    message-handler array (+1)
  RETURNS      : Pointer to the handler.
  SIDE EFFECTS : None
  NOTES        : None
 *****************************************************/
DefmessageHandler *CL_GetDefmessageHandlerPointer(
  Defclass *theDefclass,
  unsigned int theIndex)
  {
   return &theDefclass->handlers[theIndex-1];
  }

#if DEBUGGING_FUNCTIONS

/*********************************************************
  NAME         : CL_DefmessageHandlerGet_Watch
  DESCRIPTION  : Dete_rmines if trace messages for calls
                 to this handler will be generated or not
  INPUTS       : 1) A pointer to the class
                 2) The index of the handler
  RETURNS      : True if a trace is active,
                 false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 *********************************************************/
bool CL_DefmessageHandlerGet_Watch(
  Defclass *theDefclass,
  unsigned theIndex)
  {
   return theDefclass->handlers[theIndex-1].trace;
  }

/*********************************************************
  NAME         : CL_DefmessageHandlerSet_Watch
  DESCRIPTION  : Sets the trace to ON/OFF for the
                 calling of the handler
  INPUTS       : 1) True to set the trace on,
                    false to set it off
                 2) A pointer to the class
                 3) The index of the handler
  RETURNS      : Nothing useful
  SIDE EFFECTS : CL_Watch flag for the handler set
  NOTES        : None
 *********************************************************/
void CL_DefmessageHandlerSet_Watch(
  Defclass *theClass,
  unsigned theIndex,
  bool newState)
  {
   theClass->handlers[theIndex-1].trace = newState;
  }

#endif

/***************************************************
  NAME         : CL_FindDefmessageHandler
  DESCRIPTION  : Dete_rmines the index of a specfied
                  message-handler
  INPUTS       : 1) A pointer to the class
                 2) Name-string of the handler
                 3) Handler-type: "around","before",
                    "primary", or "after"
  RETURNS      : The index of the handler
                   (0 if not found)
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
unsigned CL_FindDefmessageHandler(
  Defclass *theDefclass,
  const char *hname,
  const char *htypestr)
  {
   unsigned htype;
   CLIPSLexeme *hsym;
   int theIndex;
   Environment *theEnv = theDefclass->header.env;

   htype = CL_HandlerType(theEnv,"handler-lookup",false,htypestr);
   if (htype == MERROR)
     { return 0; }

   hsym = CL_FindSymbolHN(theEnv,hname,SYMBOL_BIT);
   if (hsym == NULL)
     { return 0; }

   theIndex = CL_FindHandlerByIndex(theDefclass,hsym,htype);
   return (unsigned) (theIndex+1);
  }

/***************************************************
  NAME         : CL_DefmessageHandlerIsDeletable
  DESCRIPTION  : Dete_rmines if a message-handler
                   can be deleted
  INPUTS       : 1) Address of the handler's class
                 2) Index of the handler
  RETURNS      : True if deletable, false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
bool CL_DefmessageHandlerIsDeletable(
  Defclass *theDefclass,
  unsigned theIndex)
  {
   Environment *theEnv = theDefclass->header.env;

   if (! CL_ConstructsDeletable(theEnv))
     { return false; }

   if (theDefclass->handlers[theIndex-1].system == 1)
     { return false; }

#if (! BLOAD_ONLY) && (! RUN_TIME)
   return (CL_HandlersExecuting(theDefclass) == false) ? true : false;
#else
   return false;
#endif
  }

/******************************************************************************
  NAME         : CL_UndefmessageHandlerCommand
  DESCRIPTION  : Deletes a handler from a class
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Handler deleted if possible
  NOTES        : H/L Syntax: (undefmessage-handler <class> <handler> [<type>])
 ******************************************************************************/
void CL_UndefmessageHandlerCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
#if RUN_TIME || BLOAD_ONLY
   CL_PrintErrorID(theEnv,"MSGCOM",3,false);
   CL_WriteString(theEnv,STDERR,"Unable to delete message-handlers.\n");
#else
   CLIPSLexeme *mname;
   const char *tname;
   UDFValue theArg;
   Defclass *cls;

#if BLOAD || BLOAD_AND_BSAVE
   if (CL_Bloaded(theEnv))
     {
      CL_PrintErrorID(theEnv,"MSGCOM",3,false);
      CL_WriteString(theEnv,STDERR,"Unable to delete message-handlers.\n");
      return;
     }
#endif
   if (! CL_UDFFirstArgument(context,SYMBOL_BIT,&theArg)) return;

   cls = CL_LookupDefclassByMdlOrScope(theEnv,theArg.lexemeValue->contents);
   if ((cls == NULL) ? (strcmp(theArg.lexemeValue->contents,"*") != 0) : false)
     {
      CL_ClassExistError(theEnv,"undefmessage-handler",theArg.lexemeValue->contents);
      return;
     }
   if (! CL_UDFNextArgument(context,SYMBOL_BIT,&theArg)) return;

   mname = theArg.lexemeValue;
   if (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,SYMBOL_BIT,&theArg)) return;

      tname = theArg.lexemeValue->contents;
      if (strcmp(tname,"*") == 0)
        tname = NULL;
     }
   else
     tname = MessageHandlerData(theEnv)->hndquals[MPRIMARY];
   Wild_DeleteHandler(theEnv,cls,mname,tname);
#endif
  }

/***********************************************************
  NAME         : CL_UndefmessageHandler
  DESCRIPTION  : Deletes a handler from a class
  INPUTS       : 1) Class address    (Can be NULL)
                 2) Handler index (can be 0)
  RETURNS      : 1 if successful, 0 otherwise
  SIDE EFFECTS : Handler deleted if possible
  NOTES        : None
 ***********************************************************/
bool CL_UndefmessageHandler(
  Defclass *theDefclass,
  unsigned mhi,
  Environment *allEnv)
  {
   Environment *theEnv;
#if (! RUN_TIME) &&  (! BLOAD_ONLY)
   bool success;
   GCBlock gcb;
#endif

   if (theDefclass == NULL)
     { theEnv = allEnv; }
   else
     { theEnv = theDefclass->header.env; }

#if RUN_TIME || BLOAD_ONLY
   CL_PrintErrorID(theEnv,"MSGCOM",3,false);
   CL_WriteString(theEnv,STDERR,"Unable to delete message-handlers.\n");
   return false;
#else
     
#if BLOAD || BLOAD_AND_BSAVE
   if (CL_Bloaded(theEnv))
     {
      CL_PrintErrorID(theEnv,"MSGCOM",3,false);
      CL_WriteString(theEnv,STDERR,"Unable to delete message-handlers.\n");
      return false;
     }
#endif
   CL_GCBlockStart(theEnv,&gcb);
   if (theDefclass == NULL)
     {
      if (mhi != 0)
        {
         CL_PrintErrorID(theEnv,"MSGCOM",1,false);
         CL_WriteString(theEnv,STDERR,"Incomplete message-handler specification for deletion.\n");
         CL_GCBlockEnd(theEnv,&gcb);
         return false;
        }
      success = Wild_DeleteHandler(theEnv,NULL,NULL,NULL);
      CL_GCBlockEnd(theEnv,&gcb);
      return success;
     }

   if (mhi == 0)
     {
      success = Wild_DeleteHandler(theEnv,theDefclass,NULL,NULL);
      CL_GCBlockEnd(theEnv,&gcb);
      return success;
     }

   if (CL_HandlersExecuting(theDefclass))
     {
      CL_HandlerDeleteError(theEnv,CL_DefclassName(theDefclass));
      CL_GCBlockEnd(theEnv,&gcb);
      return false;
     }

   theDefclass->handlers[mhi-1].mark = 1;
   CL_DeallocateMarkedHandlers(theEnv,theDefclass);
   CL_GCBlockEnd(theEnv,&gcb);
   return true;
#endif
  }

#if DEBUGGING_FUNCTIONS

/*******************************************************************************
  NAME         : CL_PPDefmessageHandlerCommand
  DESCRIPTION  : Displays the pretty-print fo_rm (if any) for a handler
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : H/L Syntax: (ppdefmessage-handler <class> <message> [<type>])
 *******************************************************************************/
void CL_PPDefmessageHandlerCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;
   CLIPSLexeme *csym, *msym;
   const char *tname;
   const char *logicalName;
   Defclass *cls = NULL;
   unsigned mtype;
   DefmessageHandler *hnd;

   if (! CL_UDFFirstArgument(context,SYMBOL_BIT,&theArg))
     { return; }

   csym = CL_FindSymbolHN(theEnv,theArg.lexemeValue->contents,SYMBOL_BIT);

   if (! CL_UDFNextArgument(context,SYMBOL_BIT,&theArg))
     { return; }

   msym = CL_FindSymbolHN(theEnv,theArg.lexemeValue->contents,SYMBOL_BIT);

   if (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,SYMBOL_BIT,&theArg))
        { return; }
      tname = theArg.lexemeValue->contents;
     }
   else
     tname = MessageHandlerData(theEnv)->hndquals[MPRIMARY];
     
   mtype = CL_HandlerType(theEnv,"ppdefmessage-handler",true,tname);
   if (mtype == MERROR)
     {
      Set_EvaluationError(theEnv,true);
      return;
     }
     
   if (UDFHasNextArgument(context))
     {
      logicalName = CL_GetLogicalName(context,STDOUT);
      if (logicalName == NULL)
        {
         CL_IllegalLogicalNameMessage(theEnv,"ppdefmessage-handler");
         Set_HaltExecution(theEnv,true);
         Set_EvaluationError(theEnv,true);
         return;
        }
     }
   else
     { logicalName = STDOUT; }

   if (csym != NULL)
     cls = CL_LookupDefclassByMdlOrScope(theEnv,csym->contents);
   if (((cls == NULL) || (msym == NULL)) ? true :
       ((hnd = CL_FindHandlerByAddress(cls,msym,mtype)) == NULL))
     {
      CL_PrintErrorID(theEnv,"MSGCOM",2,false);
      CL_WriteString(theEnv,STDERR,"Unable to find message-handler '");
      CL_WriteString(theEnv,STDERR,msym->contents);
      CL_WriteString(theEnv,STDERR,"' ");
      CL_WriteString(theEnv,STDERR,tname);
      CL_WriteString(theEnv,STDERR," for class '");
      CL_WriteString(theEnv,STDERR,csym->contents);
      CL_WriteString(theEnv,STDERR,"' in function 'ppdefmessage-handler'.\n");
      Set_EvaluationError(theEnv,true);
      return;
     }
     
   if (strcmp(logicalName,"nil") == 0)
     {
      if (hnd->header.ppFo_rm != NULL)
        { returnValue->lexemeValue = CL_CreateString(theEnv,hnd->header.ppFo_rm); }
      else
        { returnValue->lexemeValue = CL_CreateString(theEnv,""); }
     }
   else
     {
      if (hnd->header.ppFo_rm != NULL)
        CL_WriteString(theEnv,logicalName,hnd->header.ppFo_rm);
     }
  }

/*****************************************************************************
  NAME         : CL_ListDefmessageHandlersCommand
  DESCRIPTION  : Depending on arguments, does lists handlers which
                   match restrictions
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : H/L Syntax: (list-defmessage-handlers [<class> [inherit]]))
 *****************************************************************************/
void CL_ListDefmessageHandlersCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   bool inhp;
   Defclass *clsptr;

   if (CL_UDFArgumentCount(context) == 0)
     CL_ListDefmessageHandlers(theEnv,NULL,STDOUT,false);
   else
     {
      clsptr = CL_ClassInfoFnxArgs(context,"list-defmessage-handlers",&inhp);
      if (clsptr == NULL)
        return;
      CL_ListDefmessageHandlers(theEnv,clsptr,STDOUT,inhp);
     }
  }

/********************************************************************
  NAME         : CL_Preview_SendCommand
  DESCRIPTION  : Displays a list of the core for a message describing
                   shadows,etc.
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Temporary core created and destroyed
  NOTES        : H/L Syntax: (preview-send <class> <msg>)
 ********************************************************************/
void CL_Preview_SendCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Defclass *cls;
   UDFValue theArg;

   /* =============================
      Get the class for the message
      ============================= */

   if (! CL_UDFFirstArgument(context,SYMBOL_BIT,&theArg))
     { return; }

   cls = CL_LookupDefclassByMdlOrScope(theEnv,theArg.lexemeValue->contents);

   if (cls == NULL)
     {
      CL_ClassExistError(theEnv,"preview-send",theArg.lexemeValue->contents);
      return;
     }

   if (! CL_UDFNextArgument(context,SYMBOL_BIT,&theArg))
     { return; }

   CL_Preview_Send(cls,STDOUT,theArg.lexemeValue->contents);
  }

/********************************************************
  NAME         : CL_DefmessageHandlerPPFo_rm
  DESCRIPTION  : Gets a message-handler pretty print fo_rm
  INPUTS       : 1) Address of the handler's class
                 2) Index of the handler
  RETURNS      : True if printable, false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 ********************************************************/
const char *CL_DefmessageHandlerPPFo_rm(
  Defclass *theDefclass,
  unsigned theIndex)
  {
   return theDefclass->handlers[theIndex-1].header.ppFo_rm;
  }

/*******************************************************************
  NAME         : CL_ListDefmessageHandlers
  DESCRIPTION  : Lists message-handlers for a class
  INPUTS       : 1) The logical name of the output
                 2) Class name (NULL to display all handlers)
                 3) A flag indicating whether to list inherited
                    handlers or not
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 *******************************************************************/
void CL_ListDefmessageHandlers(
  Environment *theEnv,
  Defclass *theDefclass,
  const char *logName,
  bool inhp)
  {
   unsigned long cnt;
   PACKED_CLASS_LINKS plinks;

   if (theDefclass != NULL)
     {
      if (inhp)
        { cnt = CL_DisplayHandlersInLinks(theEnv,logName,&theDefclass->allSuperclasses,0); }
      else
        {
         plinks.classCount = 1;
         plinks.classArray = &theDefclass;
         cnt = CL_DisplayHandlersInLinks(theEnv,logName,&plinks,0);
        }
     }
   else
     {
      plinks.classCount = 1;
      cnt = 0L;
      for (theDefclass = CL_GetNextDefclass(theEnv,NULL) ;
           theDefclass != NULL ;
           theDefclass = CL_GetNextDefclass(theEnv,theDefclass))
        {
         plinks.classArray = &theDefclass;
         cnt += CL_DisplayHandlersInLinks(theEnv,logName,&plinks,0);
        }
     }
   CL_PrintTally(theEnv,logName,cnt,"message-handler","message-handlers");
  }

/********************************************************************
  NAME         : CL_Preview_Send
  DESCRIPTION  : Displays a list of the core for a message describing
                   shadows,etc.
  INPUTS       : 1) Logical name of output
                 2) Class pointer
                 3) Message name-string
  RETURNS      : Nothing useful
  SIDE EFFECTS : Temporary core created and destroyed
  NOTES        : None
 ********************************************************************/
void CL_Preview_Send(
  Defclass *theDefclass,
  const char *logicalName,
  const char *msgname)
  {
   HANDLER_LINK *core;
   CLIPSLexeme *msym;
   Environment *theEnv = theDefclass->header.env;

   msym = CL_FindSymbolHN(theEnv,msgname,SYMBOL_BIT);
   if (msym == NULL)
     { return; }

   core = CL_FindPreviewApplicableHandlers(theEnv,theDefclass,msym);
   if (core != NULL)
     {
      CL_DisplayCore(theEnv,logicalName,core,0);
      CL_DestroyHandlerLinks(theEnv,core);
     }
  }

/****************************************************
  NAME         : CL_DisplayHandlersInLinks
  DESCRIPTION  : Recursively displays all handlers
                  for an array of classes
  INPUTS       : 1) The logical name of the output
                 2) The packed class links
                 3) The index to print from the links
  RETURNS      : The number of handlers printed
  SIDE EFFECTS : None
  NOTES        : Used by CL_DescribeClass()
 ****************************************************/
unsigned long CL_DisplayHandlersInLinks(
  Environment *theEnv,
  const char *logName,
  PACKED_CLASS_LINKS *plinks,
  unsigned int theIndex)
  {
   unsigned long i;
   unsigned long cnt;

   cnt = plinks->classArray[theIndex]->handlerCount;
   if ((theIndex + 1) < plinks->classCount)
     cnt += CL_DisplayHandlersInLinks(theEnv,logName,plinks,theIndex + 1);
   for (i = 0 ; i < plinks->classArray[theIndex]->handlerCount ; i++)
     CL_PrintHandler(theEnv,logName,&plinks->classArray[theIndex]->handlers[i],false,true);
   return cnt;
  }

#endif

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if ! RUN_TIME

/**********************************************************
  NAME         : CreateSystemHandlers
  DESCRIPTION  : Attachess the system message-handlers
                 after a (clear)
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : System handlers created
  NOTES        : Must be called after CL_CreateSystemClasses()
 **********************************************************/
static void CreateSystemHandlers(
  Environment *theEnv,
  void *context)
  {
   CL_NewSystemHandler(theEnv,USER_TYPE_NAME,INIT_STRING,"init-slots",0);
   CL_NewSystemHandler(theEnv,USER_TYPE_NAME,DELETE_STRING,"delete-instance",0);
   CL_NewSystemHandler(theEnv,USER_TYPE_NAME,CREATE_STRING,"(create-instance)",0);

#if DEBUGGING_FUNCTIONS
   CL_NewSystemHandler(theEnv,USER_TYPE_NAME,PRINT_STRING,"ppinstance",0);
#endif

   CL_NewSystemHandler(theEnv,USER_TYPE_NAME,DIRECT_MODIFY_STRING,"(direct-modify)",1);
   CL_NewSystemHandler(theEnv,USER_TYPE_NAME,MSG_MODIFY_STRING,"(message-modify)",1);
   CL_NewSystemHandler(theEnv,USER_TYPE_NAME,DIRECT_DUPLICATE_STRING,"(direct-duplicate)",2);
   CL_NewSystemHandler(theEnv,USER_TYPE_NAME,MSG_DUPLICATE_STRING,"(message-duplicate)",2);
  }

#endif

#if (! BLOAD_ONLY) && (! RUN_TIME)

/************************************************************
  NAME         : Wild_DeleteHandler
  DESCRIPTION  : Deletes a handler from a class
  INPUTS       : 1) Class address (Can be NULL)
                 2) Message Handler Name (Can be NULL)
                 3) Type name ("primary", etc.)
  RETURNS      : 1 if successful, 0 otherwise
  SIDE EFFECTS : Handler deleted if possible
  NOTES        : None
 ************************************************************/
static bool Wild_DeleteHandler(
  Environment *theEnv,
  Defclass *cls,
  CLIPSLexeme *msym,
  const char *tname)
  {
   int mtype;

   if (msym == NULL)
     msym = CL_CreateSymbol(theEnv,"*");
   if (tname != NULL)
     {
      mtype = (int) CL_HandlerType(theEnv,"undefmessage-handler",true,tname);
      if (mtype == MERROR)
        return false;
     }
   else
     mtype = -1;
   if (cls == NULL)
     {
      bool success = true;

      for (cls = CL_GetNextDefclass(theEnv,NULL) ;
           cls != NULL ;
           cls = CL_GetNextDefclass(theEnv,cls))
        if (CL_DeleteHandler(theEnv,cls,msym,mtype,false) == false)
          success = false;
      return(success);
     }
   return(CL_DeleteHandler(theEnv,cls,msym,mtype,true));
  }

#endif

#if DEBUGGING_FUNCTIONS

/******************************************************************
  NAME         : DefmessageHandler_WatchAccess
  DESCRIPTION  : Parses a list of class names passed by
                 CL_Add_WatchItem() and sets the traces accordingly
  INPUTS       : 1) A code indicating which trace flag is to be set
                    0 - CL_Watch instance creation/deletion
                    1 - CL_Watch slot changes to instances
                 2) The value to which to set the trace flags
                 3) A list of expressions containing the names
                    of the classes for which to set traces
  RETURNS      : True if all OK, false otherwise
  SIDE EFFECTS : CL_Watch flags set in specified classes
  NOTES        : Accessory function for CL_Add_WatchItem()
 ******************************************************************/
static bool DefmessageHandler_WatchAccess(
  Environment *theEnv,
  int code,
  bool newState,
  Expression *argExprs)
  {
#if MAC_XCD
#pragma unused(code)
#endif
   if (newState)
     return(DefmessageHandler_WatchSupport(theEnv,"watch",NULL,newState,
                                        NULL,CL_DefmessageHandlerSet_Watch,argExprs));
   else
     return(DefmessageHandler_WatchSupport(theEnv,"unwatch",NULL,newState,
                                        NULL,CL_DefmessageHandlerSet_Watch,argExprs));
  }

/***********************************************************************
  NAME         : DefmessageHandler_WatchPrint
  DESCRIPTION  : Parses a list of class names passed by
                 CL_Add_WatchItem() and displays the traces accordingly
  INPUTS       : 1) The logical name of the output
                 2) A code indicating which trace flag is to be examined
                    0 - CL_Watch instance creation/deletion
                    1 - CL_Watch slot changes to instances
                 3) A list of expressions containing the names
                    of the classes for which to examine traces
  RETURNS      : True if all OK, false otherwise
  SIDE EFFECTS : CL_Watch flags displayed for specified classes
  NOTES        : Accessory function for CL_Add_WatchItem()
 ***********************************************************************/
static bool DefmessageHandler_WatchPrint(
  Environment *theEnv,
  const char *logName,
  int code,
  Expression *argExprs)
  {
#if MAC_XCD
#pragma unused(code)
#endif
   return DefmessageHandler_WatchSupport(theEnv,"list-watch-items",logName,false,
                                        CL_PrintHandler_WatchFlag,NULL,argExprs);
  }

/*******************************************************
  NAME         : DefmessageHandler_WatchSupport
  DESCRIPTION  : Sets or displays handlers specified
  INPUTS       : 1) The calling function name
                 2) The logical output name for displays
                    (can be NULL)
                 4) The new set state (can be -1)
                 5) The print function (can be NULL)
                 6) The trace function (can be NULL)
                 7) The handlers expression list
  RETURNS      : True if all OK,
                 false otherwise
  SIDE EFFECTS : Handler trace flags set or displayed
  NOTES        : None
 *******************************************************/
static bool DefmessageHandler_WatchSupport(
  Environment *theEnv,
  const char *funcName,
  const char *logName,
  bool newState,
  void (*printFunc)(Environment *,const char *,Defclass *,unsigned),
  void (*traceFunc)(Defclass *,unsigned,bool),
  Expression *argExprs)
  {
   Defmodule *theModule;
   Defclass *theClass;
   const char *theHandlerStr;
   int theType;
   unsigned int argIndex = 2;
   UDFValue tmpData;

   /* ===============================
      If no handlers are specified,
      show the trace for all handlers
      in all handlers
      =============================== */
   if (argExprs == NULL)
     {
      CL_SaveCurrentModule(theEnv);
      theModule = CL_GetNextDefmodule(theEnv,NULL);
      while (theModule != NULL)
        {
         CL_SetCurrentModule(theEnv,theModule);
         if (traceFunc == NULL)
           {
            CL_WriteString(theEnv,logName,CL_DefmoduleName(theModule));
            CL_WriteString(theEnv,logName,":\n");
           }
         theClass = CL_GetNextDefclass(theEnv,NULL);
         while (theClass != NULL)
            {
             if (CL_WatchClassHandlers(theEnv,theClass,NULL,-1,logName,newState,
                                    true,printFunc,traceFunc) == false)
                 return false;
             theClass = CL_GetNextDefclass(theEnv,theClass);
            }
          theModule = CL_GetNextDefmodule(theEnv,theModule);
         }
      CL_RestoreCurrentModule(theEnv);
      return true;
     }

   /* ================================================
      Set or show the traces for the specified handler
      ================================================ */
   while (argExprs != NULL)
     {
      if (CL_EvaluateExpression(theEnv,argExprs,&tmpData))
        return false;
      if (tmpData.header->type != SYMBOL_TYPE)
        {
         CL_ExpectedTypeError1(theEnv,funcName,argIndex,"'class name'");
         return false;
        }
      theClass = CL_LookupDefclassByMdlOrScope(theEnv,tmpData.lexemeValue->contents);
      if (theClass == NULL)
        {
         CL_ExpectedTypeError1(theEnv,funcName,argIndex,"'class name'");
         return false;
        }
      if (GetNextArgument(argExprs) != NULL)
        {
         argExprs = GetNextArgument(argExprs);
         argIndex++;
         if (CL_EvaluateExpression(theEnv,argExprs,&tmpData))
           return false;
         if (tmpData.header->type != SYMBOL_TYPE)
           {
            CL_ExpectedTypeError1(theEnv,funcName,argIndex,"'handler name'");
            return false;
           }
         theHandlerStr = tmpData.lexemeValue->contents;
         if (GetNextArgument(argExprs) != NULL)
           {
            argExprs = GetNextArgument(argExprs);
            argIndex++;
            if (CL_EvaluateExpression(theEnv,argExprs,&tmpData))
              return false;
            if (tmpData.header->type != SYMBOL_TYPE)
              {
               CL_ExpectedTypeError1(theEnv,funcName,argIndex,"'handler type'");
               return false;
              }
            if ((theType = (int) CL_HandlerType(theEnv,funcName,true,tmpData.lexemeValue->contents)) == MERROR)
              return false;
           }
         else
           theType = -1;
        }
      else
        {
         theHandlerStr = NULL;
         theType = -1;
        }
      if (CL_WatchClassHandlers(theEnv,theClass,theHandlerStr,theType,logName,
                             newState,false,printFunc,traceFunc) == false)
        {
         CL_ExpectedTypeError1(theEnv,funcName,argIndex,"handler");
         return false;
        }
      argIndex++;
      argExprs = GetNextArgument(argExprs);
     }
   return true;
  }

/*******************************************************
  NAME         : CL_WatchClassHandlers
  DESCRIPTION  : Sets or displays handlers specified
  INPUTS       : 1) The class
                 2) The handler name (or NULL wildcard)
                 3) The handler type (or -1 wildcard)
                 4) The logical output name for displays
                    (can be NULL)
                 5) The new set state (can be -1)
                 6) The print function (can be NULL)
                 7) The trace function (can be NULL)
  RETURNS      : True if all OK,
                 false otherwise
  SIDE EFFECTS : Handler trace flags set or displayed
  NOTES        : None
 *******************************************************/
static bool CL_WatchClassHandlers(
  Environment *theEnv,
  Defclass *theClass,
  const char *theHandlerStr,
  int theType,
  const char *logName,
  bool newState,
  bool indentp,
  void (*printFunc)(Environment *,const char *,Defclass *,unsigned),
  void (*traceFunc)(Defclass *,unsigned,bool))
  {
   unsigned theHandler;
   bool found = false;

   theHandler = CL_GetNextDefmessageHandler(theClass,0);
   while (theHandler != 0)
     {
      if ((theType == -1) ? true :
          (theType == (int) theClass->handlers[theHandler-1].type))
        {
         if ((theHandlerStr == NULL) ? true :
             (strcmp(theHandlerStr,CL_DefmessageHandlerName(theClass,theHandler)) == 0))
            {
             if (traceFunc != NULL)
               (*traceFunc)(theClass,theHandler,newState);
             else
               {
                if (indentp)
                  CL_WriteString(theEnv,logName,"   ");
                (*printFunc)(theEnv,logName,theClass,theHandler);
               }
             found = true;
            }
        }
      theHandler = CL_GetNextDefmessageHandler(theClass,theHandler);
     }
   if ((theHandlerStr != NULL) && (theType != -1) && (found == false))
     return false;
   return true;
  }

/***************************************************
  NAME         : CL_PrintHandler_WatchFlag
  DESCRIPTION  : Displays trace value for handler
  INPUTS       : 1) The logical name of the output
                 2) The class
                 3) The handler index
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
static void CL_PrintHandler_WatchFlag(
  Environment *theEnv,
  const char *logName,
  Defclass *theClass,
  unsigned theHandler)
  {
   CL_WriteString(theEnv,logName,CL_DefclassName(theClass));
   CL_WriteString(theEnv,logName," ");
   CL_WriteString(theEnv,logName,CL_DefmessageHandlerName(theClass,theHandler));
   CL_WriteString(theEnv,logName," ");
   CL_WriteString(theEnv,logName,CL_Defmessage_HandlerType(theClass,theHandler));

   if (CL_DefmessageHandlerGet_Watch(theClass,theHandler))
     CL_WriteString(theEnv,logName," = on\n");
   else
     CL_WriteString(theEnv,logName," = off\n");
  }

#endif /* DEBUGGING_FUNCTIONS */

#endif /* OBJECT_SYSTEM */

