   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/18/16            */
   /*                                                     */
   /*           INSTANCE MODIFY AND DUPLICATE MODULE      */
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
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*            Changed name of variable exp to theExp         */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Converted INSTANCE_PATTERN_MATCHING to         */
/*            DEFRULE_CONSTRUCT.                             */
/*                                                           */
/*      6.30: Added DATA_OBJECT_ARRAY primitive type.        */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            The return value of CL_DirectMessage indicates    */
/*            whether an execution error has occurred.       */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            Removed DATA_OBJECT_ARRAY primitive type.      */
/*                                                           */
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*************************************************************/

#ifndef _H_insmoddp

#pragma once

#define _H_insmoddp

#define DIRECT_MODIFY_STRING    "direct-modify"
#define MSG_MODIFY_STRING       "message-modify"
#define DIRECT_DUPLICATE_STRING "direct-duplicate"
#define MSG_DUPLICATE_STRING    "message-duplicate"

#ifndef _H_evaluatn
#include "evaluatn.h"
#endif

   void                           CL_SetupInstanceModDupCommands(Environment *);

   void                           CL_ModifyInstance(Environment *,UDFContext *,UDFValue *);
   void                           MsgCL_ModifyInstance(Environment *,UDFContext *,UDFValue *);
   void                           CL_DuplicateInstance(Environment *,UDFContext *,UDFValue *);
   void                           MsgCL_DuplicateInstance(Environment *,UDFContext *,UDFValue *);

#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM
   void                           CL_InactiveCL_ModifyInstance(Environment *,UDFContext *,UDFValue *);
   void                           CL_InactiveMsgCL_ModifyInstance(Environment *,UDFContext *,UDFValue *);
   void                           InactiveCL_DuplicateInstance(Environment *,UDFContext *,UDFValue *);
   void                           InactiveMsgCL_DuplicateInstance(Environment *,UDFContext *,UDFValue *);
#endif

   void                           CL_DirectModifyMsgHandler(Environment *,UDFContext *,UDFValue *);
   void                           CL_MsgModifyMsgHandler(Environment *,UDFContext *,UDFValue *);
   void                           CL_DirectDuplicateMsgHandler(Environment *,UDFContext *,UDFValue *);
   void                           CL_MsgDuplicateMsgHandler(Environment *,UDFContext *,UDFValue *);

#endif /* _H_insmoddp */







