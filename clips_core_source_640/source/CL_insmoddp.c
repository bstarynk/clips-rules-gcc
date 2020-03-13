   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  06/22/18             */
   /*                                                     */
   /*        INSTANCE MODIFY AND DUPLICATE MODULE         */
   /*******************************************************/

/*************************************************************/
/* Purpose:  Instance modify and duplicate support routines  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
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
/*            Removed DATA_OBJECT_ARRAY primitive type.      */
/*                                                           */
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if OBJECT_SYSTEM

#include "argacces.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "inscom.h"
#include "insfun.h"
#include "insmngr.h"
#include "inspsr.h"
#include "memalloc.h"
#include "miscfun.h"
#include "msgcom.h"
#include "msgfun.h"
#include "msgpass.h"
#if DEFRULE_CONSTRUCT
#include "network.h"
#include "objrtmch.h"
#endif
#include "prccode.h"
#include "prntutil.h"
#include "router.h"

#include "insmoddp.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static UDFValue *CL_EvaluateSlotOverrides (Environment *, Expression *,
					   unsigned short *, bool *);
static void DeleteSlotOverride_Evaluations (Environment *, UDFValue *,
					    unsigned short);
static void ModifyMsgHandlerSupport (Environment *, UDFValue *, bool);
static void DuplicateMsgHandlerSupport (Environment *, UDFValue *, bool);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************
  NAME         : CL_SetupInstanceModDupCommands
  DESCRIPTION  : Defines function interfaces for
                 modify- and duplicate- instance
                 functions
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Functions defined to KB
  NOTES        : None
 ***************************************************/
void
CL_SetupInstanceModDupCommands (Environment * theEnv)
{
#if ! RUN_TIME

#if DEFRULE_CONSTRUCT
  CL_AddUDF (theEnv, "modify-instance", "*", 0, UNBOUNDED, NULL,
	     CL_Inactive_ModifyInstance, "CL_Inactive_ModifyInstance", NULL);
  CL_AddUDF (theEnv, "active-modify-instance", "*", 0, UNBOUNDED, NULL,
	     CL_ModifyInstance, "CL_ModifyInstance", NULL);
  CL_AddFunctionParser (theEnv, "active-modify-instance",
			CL_ParseInitializeInstance);
  CL_AddUDF (theEnv, "message-modify-instance", "*", 0, UNBOUNDED, NULL,
	     CL_InactiveMsg_ModifyInstance, "CL_InactiveMsg_ModifyInstance",
	     NULL);
  CL_AddUDF (theEnv, "active-message-modify-instance", "*", 0, UNBOUNDED,
	     NULL, Msg_ModifyInstance, "Msg_ModifyInstance", NULL);
  CL_AddFunctionParser (theEnv, "active-message-modify-instance",
			CL_ParseInitializeInstance);

  CL_AddUDF (theEnv, "duplicate-instance", "*", 0, UNBOUNDED, NULL,
	     Inactive_DuplicateInstance, "Inactive_DuplicateInstance", NULL);
  CL_AddUDF (theEnv, "active-duplicate-instance", "*", 0, UNBOUNDED, NULL,
	     CL_DuplicateInstance, "CL_DuplicateInstance", NULL);
  CL_AddFunctionParser (theEnv, "active-duplicate-instance",
			CL_ParseInitializeInstance);
  CL_AddUDF (theEnv, "message-duplicate-instance", "*", 0, UNBOUNDED, NULL,
	     InactiveMsg_DuplicateInstance, "InactiveMsg_DuplicateInstance",
	     NULL);
  CL_AddUDF (theEnv, "active-message-duplicate-instance", "*", 0, UNBOUNDED,
	     NULL, Msg_DuplicateInstance, "Msg_DuplicateInstance", NULL);
  CL_AddFunctionParser (theEnv, "active-message-duplicate-instance",
			CL_ParseInitializeInstance);
#else
  CL_AddUDF (theEnv, "modify-instance", "*", 0, UNBOUNDED, NULL,
	     CL_ModifyInstance, "CL_ModifyInstance", NULL);
  CL_AddUDF (theEnv, "message-modify-instance", "*", 0, UNBOUNDED, NULL,
	     Msg_ModifyInstance, "Msg_ModifyInstance", NULL);
  CL_AddUDF (theEnv, "duplicate-instance", "*", 0, UNBOUNDED, NULL,
	     CL_DuplicateInstance, "CL_DuplicateInstance", NULL);
  CL_AddUDF (theEnv, "message-duplicate-instance", "*", 0, UNBOUNDED, NULL,
	     Msg_DuplicateInstance, "Msg_DuplicateInstance", NULL);
#endif

  CL_AddUDF (theEnv, "(direct-modify)", "*", 0, UNBOUNDED, NULL,
	     CL_DirectModifyMsgHandler, "CL_DirectModifyMsgHandler", NULL);
  CL_AddUDF (theEnv, "(message-modify)", "*", 0, UNBOUNDED, NULL,
	     CL_MsgModifyMsgHandler, "CL_MsgModifyMsgHandler", NULL);
  CL_AddUDF (theEnv, "(direct-duplicate)", "*", 0, UNBOUNDED, NULL,
	     CL_DirectDuplicateMsgHandler, "CL_DirectDuplicateMsgHandler",
	     NULL);
  CL_AddUDF (theEnv, "(message-duplicate)", "*", 0, UNBOUNDED, NULL,
	     CL_MsgDuplicateMsgHandler, "CL_MsgDuplicateMsgHandler", NULL);

#endif

#if DEFRULE_CONSTRUCT
  CL_AddFunctionParser (theEnv, "active-modify-instance",
			CL_ParseInitializeInstance);
  CL_AddFunctionParser (theEnv, "active-message-modify-instance",
			CL_ParseInitializeInstance);

  CL_AddFunctionParser (theEnv, "active-duplicate-instance",
			CL_ParseInitializeInstance);
  CL_AddFunctionParser (theEnv, "active-message-duplicate-instance",
			CL_ParseInitializeInstance);
#endif

  CL_AddFunctionParser (theEnv, "modify-instance",
			CL_ParseInitializeInstance);
  CL_AddFunctionParser (theEnv, "message-modify-instance",
			CL_ParseInitializeInstance);
  CL_AddFunctionParser (theEnv, "duplicate-instance",
			CL_ParseInitializeInstance);
  CL_AddFunctionParser (theEnv, "message-duplicate-instance",
			CL_ParseInitializeInstance);
}

/*************************************************************
  NAME         : CL_ModifyInstance
  DESCRIPTION  : Modifies slots of an instance via the
                 direct-modify message
  INPUTS       : The address of the result value
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slot updates perfo_rmed directly
  NOTES        : H/L Syntax:
                 (modify-instance <instance> <slot-override>*)
 *************************************************************/
void
CL_ModifyInstance (Environment * theEnv,
		   UDFContext * context, UDFValue * returnValue)
{
  Instance *ins;
  Expression theExp;
  UDFValue *overrides;
  bool oldOMDMV;
  unsigned short overrideCount;
  bool error;

  /* ===========================================
     The slot-overrides need to be evaluated now
     to resolve any variable references before a
     new frame is pushed for message-handler
     execution
     =========================================== */

  overrides = CL_EvaluateSlotOverrides (theEnv, GetFirstArgument ()->nextArg,
					&overrideCount, &error);
  if (error)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
      return;
    }

  /* ==================================
     Find the instance and make sure it
     wasn't deleted by the overrides
     ================================== */
  ins = CL_CheckInstance (context);
  if (ins == NULL)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
      DeleteSlotOverride_Evaluations (theEnv, overrides, overrideCount);
      return;
    }

  /* ======================================
     We are passing the slot override
     expression info_rmation along
     to whatever message-handler implements
     the modify
     ====================================== */

  theExp.type = EXTERNAL_ADDRESS_TYPE;
  theExp.value = CL_CreateExternalAddress (theEnv, overrides, 0);
  theExp.argList = NULL;
  theExp.nextArg = NULL;

  oldOMDMV = InstanceData (theEnv)->ObjectModDupMsgValid;
  InstanceData (theEnv)->ObjectModDupMsgValid = true;
  CL_DirectMessage (theEnv,
		    CL_FindSymbolHN (theEnv, DIRECT_MODIFY_STRING,
				     SYMBOL_BIT), ins, returnValue, &theExp);
  InstanceData (theEnv)->ObjectModDupMsgValid = oldOMDMV;

  DeleteSlotOverride_Evaluations (theEnv, overrides, overrideCount);
}

/*************************************************************
  NAME         : Msg_ModifyInstance
  DESCRIPTION  : Modifies slots of an instance via the
                 direct-modify message
  INPUTS       : The address of the result value
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slot updates perfo_rmed with put- messages
  NOTES        : H/L Syntax:
                 (message-modify-instance <instance>
                    <slot-override>*)
 *************************************************************/
void
Msg_ModifyInstance (Environment * theEnv,
		    UDFContext * context, UDFValue * returnValue)
{
  Instance *ins;
  Expression theExp;
  UDFValue *overrides;
  bool oldOMDMV;
  unsigned short overrideCount;
  bool error;

  /* ===========================================
     The slot-overrides need to be evaluated now
     to resolve any variable references before a
     new frame is pushed for message-handler
     execution
     =========================================== */
  overrides = CL_EvaluateSlotOverrides (theEnv, GetFirstArgument ()->nextArg,
					&overrideCount, &error);
  if (error)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
      return;
    }

  /* ==================================
     Find the instance and make sure it
     wasn't deleted by the overrides
     ================================== */
  ins = CL_CheckInstance (context);
  if (ins == NULL)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
      DeleteSlotOverride_Evaluations (theEnv, overrides, overrideCount);
      return;
    }

  /* ======================================
     We are passing the slot override
     expression info_rmation along
     to whatever message-handler implements
     the modify
     ====================================== */

  theExp.type = EXTERNAL_ADDRESS_TYPE;
  theExp.value = CL_CreateExternalAddress (theEnv, overrides, 0);
  theExp.argList = NULL;
  theExp.nextArg = NULL;

  oldOMDMV = InstanceData (theEnv)->ObjectModDupMsgValid;
  InstanceData (theEnv)->ObjectModDupMsgValid = true;
  CL_DirectMessage (theEnv,
		    CL_FindSymbolHN (theEnv, MSG_MODIFY_STRING, SYMBOL_BIT),
		    ins, returnValue, &theExp);
  InstanceData (theEnv)->ObjectModDupMsgValid = oldOMDMV;

  DeleteSlotOverride_Evaluations (theEnv, overrides, overrideCount);
}

/*************************************************************
  NAME         : CL_DuplicateInstance
  DESCRIPTION  : Duplicates an instance via the
                 direct-duplicate message
  INPUTS       : The address of the result value
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slot updates perfo_rmed directly
  NOTES        : H/L Syntax:
                 (duplicate-instance <instance>
                   [to <instance-name>] <slot-override>*)
 *************************************************************/
void
CL_DuplicateInstance (Environment * theEnv,
		      UDFContext * context, UDFValue * returnValue)
{
  Instance *ins;
  UDFValue newName;
  Expression theExp[2];
  UDFValue *overrides;
  bool oldOMDMV;
  unsigned short overrideCount;
  bool error;

  /* ===========================================
     The slot-overrides need to be evaluated now
     to resolve any variable references before a
     new frame is pushed for message-handler
     execution
     =========================================== */
  overrides =
    CL_EvaluateSlotOverrides (theEnv, GetFirstArgument ()->nextArg->nextArg,
			      &overrideCount, &error);
  if (error)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
      return;
    }

  /* ==================================
     Find the instance and make sure it
     wasn't deleted by the overrides
     ================================== */
  ins = CL_CheckInstance (context);
  if (ins == NULL)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
      DeleteSlotOverride_Evaluations (theEnv, overrides, overrideCount);
      return;
    }

  if (!CL_UDFNextArgument (context, INSTANCE_NAME_BIT | SYMBOL_BIT, &newName))
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
      DeleteSlotOverride_Evaluations (theEnv, overrides, overrideCount);
      return;
    }

  /* ======================================
     We are passing the slot override
     expression info_rmation along
     to whatever message-handler implements
     the duplicate
     ====================================== */
  theExp[0].type = INSTANCE_NAME_TYPE;
  theExp[0].value = newName.value;
  theExp[0].argList = NULL;
  theExp[0].nextArg = &theExp[1];
  theExp[1].type = EXTERNAL_ADDRESS_TYPE;
  theExp[1].value = CL_CreateExternalAddress (theEnv, overrides, 0);
  theExp[1].argList = NULL;
  theExp[1].nextArg = NULL;

  oldOMDMV = InstanceData (theEnv)->ObjectModDupMsgValid;
  InstanceData (theEnv)->ObjectModDupMsgValid = true;
  CL_DirectMessage (theEnv,
		    CL_FindSymbolHN (theEnv, DIRECT_DUPLICATE_STRING,
				     SYMBOL_BIT), ins, returnValue,
		    &theExp[0]);
  InstanceData (theEnv)->ObjectModDupMsgValid = oldOMDMV;

  DeleteSlotOverride_Evaluations (theEnv, overrides, overrideCount);
}

/*************************************************************
  NAME         : Msg_DuplicateInstance
  DESCRIPTION  : Duplicates an instance via the
                 message-duplicate message
  INPUTS       : The address of the result value
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slot updates perfo_rmed w/ int & put- messages
  NOTES        : H/L Syntax:
                 (duplicate-instance <instance>
                   [to <instance-name>] <slot-override>*)
 *************************************************************/
void
Msg_DuplicateInstance (Environment * theEnv,
		       UDFContext * context, UDFValue * returnValue)
{
  Instance *ins;
  UDFValue newName;
  Expression theExp[2];
  UDFValue *overrides;
  bool oldOMDMV;
  unsigned short overrideCount;
  bool error;

  /* ===========================================
     The slot-overrides need to be evaluated now
     to resolve any variable references before a
     new frame is pushed for message-handler
     execution
     =========================================== */
  overrides =
    CL_EvaluateSlotOverrides (theEnv, GetFirstArgument ()->nextArg->nextArg,
			      &overrideCount, &error);
  if (error)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
      return;
    }

  /* ==================================
     Find the instance and make sure it
     wasn't deleted by the overrides
     ================================== */
  ins = CL_CheckInstance (context);
  if (ins == NULL)
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
      DeleteSlotOverride_Evaluations (theEnv, overrides, overrideCount);
      return;
    }
  if (!CL_UDFNextArgument (context, INSTANCE_NAME_BIT | SYMBOL_BIT, &newName))
    {
      returnValue->lexemeValue = FalseSymbol (theEnv);
      DeleteSlotOverride_Evaluations (theEnv, overrides, overrideCount);
      return;
    }

  /* ======================================
     We are passing the slot override
     expression info_rmation along
     to whatever message-handler implements
     the duplicate
     ====================================== */
  theExp[0].type = INSTANCE_NAME_TYPE;
  theExp[0].value = newName.value;
  theExp[0].argList = NULL;
  theExp[0].nextArg = &theExp[1];
  theExp[1].type = EXTERNAL_ADDRESS_TYPE;
  theExp[1].value = CL_CreateExternalAddress (theEnv, overrides, 0);
  theExp[1].argList = NULL;
  theExp[1].nextArg = NULL;

  oldOMDMV = InstanceData (theEnv)->ObjectModDupMsgValid;
  InstanceData (theEnv)->ObjectModDupMsgValid = true;
  CL_DirectMessage (theEnv,
		    CL_FindSymbolHN (theEnv, MSG_DUPLICATE_STRING,
				     SYMBOL_BIT), ins, returnValue,
		    &theExp[0]);
  InstanceData (theEnv)->ObjectModDupMsgValid = oldOMDMV;

  DeleteSlotOverride_Evaluations (theEnv, overrides, overrideCount);
}

#if DEFRULE_CONSTRUCT

/**************************************************************
  NAME         : CL_Inactive_ModifyInstance
  DESCRIPTION  : Modifies slots of an instance of a class
                 Pattern-matching is automatically
                 delayed until the slot updates are done
  INPUTS       : The address of the result value
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slot updates perfo_rmed directly
  NOTES        : H/L Syntax:
                 (modify-instance <instance-name>
                   <slot-override>*)
 **************************************************************/
void
CL_Inactive_ModifyInstance (Environment * theEnv,
			    UDFContext * context, UDFValue * returnValue)
{
  bool ov;

  ov = CL_SetDelayObjectPatternMatching (theEnv, true);
  CL_ModifyInstance (theEnv, context, returnValue);
  CL_SetDelayObjectPatternMatching (theEnv, ov);
}

/**************************************************************
  NAME         : CL_InactiveMsg_ModifyInstance
  DESCRIPTION  : Modifies slots of an instance of a class
                 Pattern-matching is automatically
                 delayed until the slot updates are done
  INPUTS       : The address of the result value
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slot updates perfo_rmed with put- messages
  NOTES        : H/L Syntax:
                 (message-modify-instance <instance-name>
                   <slot-override>*)
 **************************************************************/
void
CL_InactiveMsg_ModifyInstance (Environment * theEnv,
			       UDFContext * context, UDFValue * returnValue)
{
  bool ov;

  ov = CL_SetDelayObjectPatternMatching (theEnv, true);
  Msg_ModifyInstance (theEnv, context, returnValue);
  CL_SetDelayObjectPatternMatching (theEnv, ov);
}

/*******************************************************************
  NAME         : Inactive_DuplicateInstance
  DESCRIPTION  : Duplicates an instance of a class
                 Pattern-matching is automatically
                 delayed until the slot updates are done
  INPUTS       : The address of the result value
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slot updates perfo_rmed directly
  NOTES        : H/L Syntax:
                 (duplicate-instance <instance> [to <instance-name>]
                   <slot-override>*)
 *******************************************************************/
void
Inactive_DuplicateInstance (Environment * theEnv,
			    UDFContext * context, UDFValue * returnValue)
{
  bool ov;

  ov = CL_SetDelayObjectPatternMatching (theEnv, true);
  CL_DuplicateInstance (theEnv, context, returnValue);
  CL_SetDelayObjectPatternMatching (theEnv, ov);
}

/**************************************************************
  NAME         : InactiveMsg_DuplicateInstance
  DESCRIPTION  : Duplicates an instance of a class
                 Pattern-matching is automatically
                 delayed until the slot updates are done
  INPUTS       : The address of the result value
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slot updates perfo_rmed with put- messages
  NOTES        : H/L Syntax:
                 (message-duplicate-instance <instance>
                   [to <instance-name>]
                   <slot-override>*)
 **************************************************************/
void
InactiveMsg_DuplicateInstance (Environment * theEnv,
			       UDFContext * context, UDFValue * returnValue)
{
  bool ov;

  ov = CL_SetDelayObjectPatternMatching (theEnv, true);
  Msg_DuplicateInstance (theEnv, context, returnValue);
  CL_SetDelayObjectPatternMatching (theEnv, ov);
}

#endif

/*****************************************************
  NAME         : CL_DirectDuplicateMsgHandler
  DESCRIPTION  : Implementation for the USER class
                 handler direct-duplicate

                 Implements duplicate-instance message
                 with a series of direct slot
                 placements
  INPUTS       : A data object buffer to hold the
                 result
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slot values updated
  NOTES        : None
 *****************************************************/
void
CL_DirectDuplicateMsgHandler (Environment * theEnv,
			      UDFContext * context, UDFValue * returnValue)
{
  DuplicateMsgHandlerSupport (theEnv, returnValue, false);
}

/*****************************************************
  NAME         : CL_MsgDuplicateMsgHandler
  DESCRIPTION  : Implementation for the USER class
                 handler message-duplicate

                 Implements duplicate-instance message
                 with a series of put- messages
  INPUTS       : A data object buffer to hold the
                 result
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slot values updated
  NOTES        : None
 *****************************************************/
void
CL_MsgDuplicateMsgHandler (Environment * theEnv,
			   UDFContext * context, UDFValue * returnValue)
{
  DuplicateMsgHandlerSupport (theEnv, returnValue, true);
}

/***************************************************
  NAME         : CL_DirectModifyMsgHandler
  DESCRIPTION  : Implementation for the USER class
                 handler direct-modify

                 Implements modify-instance message
                 with a series of direct slot
                 placements
  INPUTS       : A data object buffer to hold the
                 result
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slot values updated
  NOTES        : None
 ***************************************************/
void
CL_DirectModifyMsgHandler (Environment * theEnv,
			   UDFContext * context, UDFValue * returnValue)
{
  ModifyMsgHandlerSupport (theEnv, returnValue, false);
}

/***************************************************
  NAME         : CL_MsgModifyMsgHandler
  DESCRIPTION  : Implementation for the USER class
                 handler message-modify

                 Implements modify-instance message
                 with a series of put- messages
  INPUTS       : A data object buffer to hold the
                 result
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slot values updated
  NOTES        : None
 ***************************************************/
void
CL_MsgModifyMsgHandler (Environment * theEnv,
			UDFContext * context, UDFValue * returnValue)
{
  ModifyMsgHandlerSupport (theEnv, returnValue, true);
}

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***********************************************************
  NAME         : CL_EvaluateSlotOverrides
  DESCRIPTION  : CL_Evaluates the slot-override expressions
                 for modify-instance and duplicate-instance
                 CL_Evaluations are stored in an array of
                 data objects, where the supplementalInfo
                 field points at the name of the slot
                 The data object next fields are used
                 to link the array as well.
  INPUTS       : 1) The slot override expressions
                 2) A buffer to hold the number
                    of slot overrides
                 3) A buffer to hold an error flag
  RETURNS      : The slot override data object array
  SIDE EFFECTS : Data object array allocated and initialized
                 override count and error buffers set
  NOTES        : Slot overrides must be evaluated before
                 calling supporting message-handlers for
                 modify- and duplicate-instance in the
                 event that the overrides contain variable
                 references to an outer frame
 ***********************************************************/
static UDFValue *
CL_EvaluateSlotOverrides (Environment * theEnv,
			  Expression * ovExprs,
			  unsigned short *ovCnt, bool *error)
{
  UDFValue *ovs;
  unsigned ovi;
  void *slotName;

  *error = false;

  /* ==========================================
     There are two expressions chains for every
     slot override: one for the slot name and
     one for the slot value
     ========================================== */
  *ovCnt = CL_CountArguments (ovExprs) / 2;
  if (*ovCnt == 0)
    return NULL;

  /* ===============================================
     CL_Evaluate all the slot override names and values
     and store them in a contiguous array
     =============================================== */
  ovs = (UDFValue *) CL_gm2 (theEnv, (sizeof (UDFValue) * (*ovCnt)));
  ovi = 0;
  while (ovExprs != NULL)
    {
      if (CL_EvaluateExpression (theEnv, ovExprs, &ovs[ovi]))
	goto CL_EvaluateOverridesError;
      if (ovs[ovi].header->type != SYMBOL_TYPE)
	{
	  CL_ExpectedTypeError1 (theEnv,
				 ExpressionFunctionCallName (CL_EvaluationData
							     (theEnv)->
							     CurrentExpression)->
				 contents, ovi + 1, "slot name");
	  Set_EvaluationError (theEnv, true);
	  goto CL_EvaluateOverridesError;
	}
      slotName = ovs[ovi].value;
      if (ovExprs->nextArg->argList)
	{
	  if (CL_EvaluateAndStoreInDataObject
	      (theEnv, false, ovExprs->nextArg->argList, &ovs[ovi],
	       true) == false)
	    goto CL_EvaluateOverridesError;
	}
      else
	{
	  ovs[ovi].begin = 0;
	  ovs[ovi].range = 0;
	  ovs[ovi].value = ProceduralPrimitiveData (theEnv)->NoParamValue;
	}
      ovs[ovi].supplementalInfo = slotName;
      ovExprs = ovExprs->nextArg->nextArg;
      ovs[ovi].next = (ovExprs != NULL) ? &ovs[ovi + 1] : NULL;
      ovi++;
    }
  return (ovs);

CL_EvaluateOverridesError:
  CL_rm (theEnv, ovs, (sizeof (UDFValue) * (*ovCnt)));
  *error = true;
  return NULL;
}

/**********************************************************
  NAME         : DeleteSlotOverride_Evaluations
  DESCRIPTION  : Deallocates slot override evaluation array
  INPUTS       : 1) The data object array
                 2) The number of elements
  RETURNS      : Nothing useful
  SIDE EFFECTS : Deallocates slot override data object
                 array for modify- and duplicate- instance
  NOTES        : None
 **********************************************************/
static void
DeleteSlotOverride_Evaluations (Environment * theEnv,
				UDFValue * ov_Evals, unsigned short ovCnt)
{
  if (ov_Evals != NULL)
    CL_rm (theEnv, ov_Evals, (sizeof (UDFValue) * ovCnt));
}

/**********************************************************
  NAME         : ModifyMsgHandlerSupport
  DESCRIPTION  : Support routine for CL_DirectModifyMsgHandler
                 and CL_MsgModifyMsgHandler

                 Perfo_rms a series of slot updates
                 directly or with messages
  INPUTS       : 1) A data object buffer to hold the result
                 2) A flag indicating whether to use
                    put- messages or direct placement
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slots updated (messages sent)
  NOTES        : None
 **********************************************************/
static void
ModifyMsgHandlerSupport (Environment * theEnv,
			 UDFValue * returnValue, bool msgpass)
{
  UDFValue *slotOverrides, *newval, temp, junk;
  Expression msgExp;
  Instance *ins;
  InstanceSlot *insSlot;

  returnValue->value = FalseSymbol (theEnv);
  if (InstanceData (theEnv)->ObjectModDupMsgValid == false)
    {
      CL_PrintErrorID (theEnv, "INSMODDP", 1, false);
      CL_WriteString (theEnv, STDERR,
		      "Direct/message-modify message valid only in modify-instance.\n");
      Set_EvaluationError (theEnv, true);
      return;
    }
  InstanceData (theEnv)->ObjectModDupMsgValid = false;

  ins = GetActiveInstance (theEnv);
  if (ins->garbage)
    {
      CL_StaleInstanceAddress (theEnv, "modify-instance", 0);
      Set_EvaluationError (theEnv, true);
      return;
    }

  /* =======================================
     Retrieve the slot override data objects
     passed from CL_ModifyInstance - the slot
     name is stored in the supplementalInfo
     field - and the next fields are links
     ======================================= */

  slotOverrides =
    (UDFValue *) ((CLIPSExternalAddress *)
		  CL_GetNthMessageArgument (theEnv, 1)->value)->contents;

  while (slotOverrides != NULL)
    {
      /* ===========================================================
         No evaluation or error checking needs to be done
         since this has already been done by CL_EvaluateSlotOverrides()
         =========================================================== */
      insSlot =
	CL_FindInstanceSlot (theEnv, ins,
			     (CLIPSLexeme *) slotOverrides->supplementalInfo);
      if (insSlot == NULL)
	{
	  CL_SlotExistError (theEnv,
			     ((CLIPSLexeme *)
			      slotOverrides->supplementalInfo)->contents,
			     "modify-instance");
	  Set_EvaluationError (theEnv, true);
	  return;
	}
      if (msgpass)
	{
	  msgExp.type = slotOverrides->header->type;
	  if (msgExp.type != MULTIFIELD_TYPE)
	    msgExp.value = slotOverrides->value;
	  else
	    msgExp.value = slotOverrides;
	  msgExp.argList = NULL;
	  msgExp.nextArg = NULL;
	  if (!CL_DirectMessage
	      (theEnv, insSlot->desc->overrideMessage, ins, &temp, &msgExp))
	    return;
	}
      else
	{
	  if (insSlot->desc->multiple
	      && (slotOverrides->header->type != MULTIFIELD_TYPE))
	    {
	      temp.value = CL_CreateMultifield (theEnv, 1L);
	      temp.begin = 0;
	      temp.range = 1;
	      temp.multifieldValue->contents[0].value = slotOverrides->value;
	      newval = &temp;
	    }
	  else
	    newval = slotOverrides;
	  if (CL_PutSlotValue
	      (theEnv, ins, insSlot, newval, &junk,
	       "modify-instance") != PSE_NO_ERROR)
	    return;
	}

      slotOverrides = slotOverrides->next;
    }
  returnValue->value = TrueSymbol (theEnv);
}

/*************************************************************
  NAME         : DuplicateMsgHandlerSupport
  DESCRIPTION  : Support routine for CL_DirectDuplicateMsgHandler
                 and CL_MsgDuplicateMsgHandler

                 Perfo_rms a series of slot updates
                 directly or with messages
  INPUTS       : 1) A data object buffer to hold the result
                 2) A flag indicating whether to use
                    put- messages or direct placement
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slots updated (messages sent)
  NOTES        : None
 *************************************************************/
static void
DuplicateMsgHandlerSupport (Environment * theEnv,
			    UDFValue * returnValue, bool msgpass)
{
  Instance *srcins, *dstins;
  CLIPSLexeme *newName;
  UDFValue *slotOverrides;
  Expression *valArg, msgExp;
  long i;
  bool oldMkInsMsgPass;
  InstanceSlot *dstInsSlot;
  UDFValue temp, junk, *newval;
  bool success;

  returnValue->value = FalseSymbol (theEnv);
  if (InstanceData (theEnv)->ObjectModDupMsgValid == false)
    {
      CL_PrintErrorID (theEnv, "INSMODDP", 2, false);
      CL_WriteString (theEnv, STDERR,
		      "Direct/message-duplicate message valid only in duplicate-instance.\n");
      Set_EvaluationError (theEnv, true);
      return;
    }
  InstanceData (theEnv)->ObjectModDupMsgValid = false;

  /* ==================================
     Grab the slot override expressions
     and dete_rmine the source instance
     and the name of the new instance
     ================================== */
  srcins = GetActiveInstance (theEnv);
  newName = CL_GetNthMessageArgument (theEnv, 1)->lexemeValue;
  slotOverrides =
    (UDFValue *) ((CLIPSExternalAddress *)
		  CL_GetNthMessageArgument (theEnv, 2)->value)->contents;
  if (srcins->garbage)
    {
      CL_StaleInstanceAddress (theEnv, "duplicate-instance", 0);
      Set_EvaluationError (theEnv, true);
      return;
    }
  if (((newName->header.type == srcins->name->header.type) &&
       (newName == srcins->name)) ||
      (strcmp (newName->contents, srcins->name->contents) == 0))
    {
      CL_PrintErrorID (theEnv, "INSMODDP", 3, false);
      CL_WriteString (theEnv, STDERR,
		      "Instance copy must have a different name in duplicate-instance.\n");
      Set_EvaluationError (theEnv, true);
      return;
    }

  /* ==========================================
     Create an uninitialized new instance of
     the new name (delete old version - if any)
     ========================================== */
  oldMkInsMsgPass = InstanceData (theEnv)->MkInsMsgPass;
  InstanceData (theEnv)->MkInsMsgPass = msgpass;
  dstins = CL_BuildInstance (theEnv, newName, srcins->cls, true);
  InstanceData (theEnv)->MkInsMsgPass = oldMkInsMsgPass;
  if (dstins == NULL)
    return;
  dstins->busy++;

  /* ================================
     Place slot overrides directly or
     with put- messages
     ================================ */
  while (slotOverrides != NULL)
    {
      /* ===========================================================
         No evaluation or error checking needs to be done
         since this has already been done by CL_EvaluateSlotOverrides()
         =========================================================== */
      dstInsSlot =
	CL_FindInstanceSlot (theEnv, dstins,
			     (CLIPSLexeme *) slotOverrides->supplementalInfo);
      if (dstInsSlot == NULL)
	{
	  CL_SlotExistError (theEnv,
			     ((CLIPSLexeme *)
			      slotOverrides->supplementalInfo)->contents,
			     "duplicate-instance");
	  goto DuplicateError;
	}
      if (msgpass)
	{
	  msgExp.type = slotOverrides->header->type;
	  if (msgExp.type != MULTIFIELD_TYPE)
	    msgExp.value = slotOverrides->value;
	  else
	    msgExp.value = slotOverrides;

	  msgExp.argList = NULL;
	  msgExp.nextArg = NULL;
	  if (!CL_DirectMessage
	      (theEnv, dstInsSlot->desc->overrideMessage, dstins, &temp,
	       &msgExp))
	    goto DuplicateError;
	}
      else
	{
	  if (dstInsSlot->desc->multiple
	      && (slotOverrides->header->type != MULTIFIELD_TYPE))
	    {
	      temp.value = CL_CreateMultifield (theEnv, 1L);
	      temp.begin = 0;
	      temp.range = 1;
	      temp.multifieldValue->contents[0].value = slotOverrides->value;
	      newval = &temp;
	    }
	  else
	    newval = slotOverrides;
	  if (CL_PutSlotValue
	      (theEnv, dstins, dstInsSlot, newval, &junk,
	       "duplicate-instance") != PSE_NO_ERROR)
	    goto DuplicateError;
	}
      dstInsSlot->override = true;
      slotOverrides = slotOverrides->next;
    }

  /* =======================================
     Copy values from source instance to new
     directly or with put- messages
     ======================================= */
  for (i = 0; i < dstins->cls->localInstanceSlotCount; i++)
    {
      if (dstins->slots[i].override == false)
	{
	  if (msgpass)
	    {
	      temp.value = srcins->slots[i].value;
	      if (temp.header->type == MULTIFIELD_TYPE)
		{
		  temp.begin = 0;
		  temp.range = temp.multifieldValue->length;
		}
	      valArg = CL_ConvertValueToExpression (theEnv, &temp);
	      success =
		CL_DirectMessage (theEnv,
				  dstins->slots[i].desc->overrideMessage,
				  dstins, &temp, valArg);
	      CL_ReturnExpression (theEnv, valArg);
	      if (!success)
		goto DuplicateError;
	    }
	  else
	    {
	      temp.value = srcins->slots[i].value;
	      if (srcins->slots[i].type == MULTIFIELD_TYPE)
		{
		  temp.begin = 0;
		  temp.range = srcins->slots[i].multifieldValue->length;
		}
	      if (CL_PutSlotValue
		  (theEnv, dstins, &dstins->slots[i], &temp, &junk,
		   "duplicate-instance") != PSE_NO_ERROR)
		goto DuplicateError;
	    }
	}
    }

  /* =======================================
     CL_Send init message for message-duplicate
     ======================================= */
  if (msgpass)
    {
      for (i = 0; i < dstins->cls->instanceSlotCount; i++)
	dstins->slotAddresses[i]->override = true;
      dstins->initializeInProgress = 1;
      CL_DirectMessage (theEnv, MessageHandlerData (theEnv)->INIT_SYMBOL,
			dstins, returnValue, NULL);
    }
  dstins->busy--;
  if (dstins->garbage)
    {
      returnValue->value = FalseSymbol (theEnv);
      Set_EvaluationError (theEnv, true);
    }
  else
    {
      returnValue->value = CL_GetFull_InstanceName (theEnv, dstins);
    }
  return;

DuplicateError:
  dstins->busy--;
  CL_QuashInstance (theEnv, dstins);
  Set_EvaluationError (theEnv, true);
}

#endif
