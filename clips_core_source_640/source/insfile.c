   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  06/22/18             */
   /*                                                     */
   /*         INSTANCE LOAD/SAVE (ASCII/BINARY) MODULE    */
   /*******************************************************/

/*************************************************************/
/* Purpose:  File load/save routines for instances           */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added environment parameter to CL_GenClose.       */
/*            Added environment parameter to CL_GenOpen.        */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove compiler warnings.    */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            For save-instances, bsave-instances, and       */
/*            bload-instances, the class name does not       */
/*            have to be in scope if the module name is      */
/*            specified.                                     */
/*                                                           */
/*      6.31: Prior error flags are cleared before           */
/*            EnvCL_LoadCL_Instances, EnvRestoreCL_Instances,         */
/*            EnvCL_LoadCL_InstancesFromString, and                */
/*            EnvRestoreCL_InstancesFromString are processed.   */
/*                                                           */
/*      6.40: Added Env prefix to GetCL_EvaluationError and     */
/*            SetCL_EvaluationError functions.                  */
/*                                                           */
/*            Refactored code to reduce header dependencies  */
/*            in sysdep.c.                                   */
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

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */

#include <stdlib.h>

#include "setup.h"

#if OBJECT_SYSTEM

#include "argacces.h"
#include "classcom.h"
#include "classfun.h"
#include "memalloc.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#if DEFTEMPLATE_CONSTRUCT && DEFRULE_CONSTRUCT
#include "factmngr.h"
#endif
#include "inscom.h"
#include "insfun.h"
#include "insmngr.h"
#include "inspsr.h"
#include "object.h"
#include "prntutil.h"
#include "router.h"
#include "strngrtr.h"
#include "symblbin.h"
#include "sysdep.h"
#include "utility.h"

#include "insfile.h"

/* =========================================
   *****************************************
                   CONSTANTS
   =========================================
   ***************************************** */
#define MAX_BLOCK_SIZE 10240

/* =========================================
   *****************************************
               MACROS AND TYPES
   =========================================
   ***************************************** */
struct bsaveSlotValue
  {
   unsigned long slotName;
   unsigned long valueCount;
  };

struct bsaveSlotValueAtom
  {
   unsigned short type;
   unsigned long value;
  };

struct classItem
  {
   Defclass *classPtr;
   struct classItem *next;
  };

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static long                    CL_InstancesCL_SaveCommandParser(UDFContext *,
                                                             long (*)(Environment *,const char *,
                                                                      CL_SaveScope,Expression *,bool));
   static struct classItem       *ProcessCL_SaveClassList(Environment *,const char *,Expression *,CL_SaveScope,bool);
   static void                    ReturnCL_SaveClassList(Environment *,struct classItem *);
   static long                    CL_SaveOrMarkCL_Instances(Environment *,FILE *,int,struct classItem *,bool,bool,
                                                      void (*)(Environment *,FILE *,Instance *));
   static long                    CL_SaveOrMarkCL_InstancesOfClass(Environment *,FILE *,Defmodule *,int,Defclass *,
                                                             bool,int,void (*)(Environment *,FILE *,Instance *));
   static void                    CL_SaveSingleInstanceText(Environment *,FILE *,Instance *);
   static void                    ProcessFileErrorMessage(Environment *,const char *,const char *);
#if BSAVE_INSTANCES
   static void                    CL_WriteBinaryHeader(Environment *,FILE *);
   static void                    MarkSingleInstance(Environment *,FILE *,Instance *);
   static void                    MarkNeededAtom(Environment *,unsigned short,void *);
   static void                    CL_SaveSingleInstanceBinary(Environment *,FILE *,Instance *);
   static void                    CL_SaveAtomBinary(Environment *,unsigned short,void *,FILE *);
#endif

   static long                    CL_LoadOrRestoreCL_Instances(Environment *,const char *,bool,bool);

#if BLOAD_INSTANCES
   static bool                    VerifyBinaryHeader(Environment *,const char *);
   static bool                    CL_LoadSingleBinaryInstance(Environment *);
   static void                    BinaryCL_LoadInstanceError(Environment *,CLIPSLexeme *,Defclass *);
   static void                    CreateSlotValue(Environment *,UDFValue *,struct bsaveSlotValueAtom *,unsigned long);
   static void                   *GetBinaryAtomValue(Environment *,struct bsaveSlotValueAtom *);
   static void                    BufferedRead(Environment *,void *,size_t);
   static void                    FreeReadBuffer(Environment *);
#endif

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************
  NAME         : CL_SetupInstanceFileCommands
  DESCRIPTION  : Defines function interfaces for
                 saving instances to files
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Functions defined to KB
  NOTES        : None
 ***************************************************/
void CL_SetupInstanceFileCommands(
  Environment *theEnv)
  {
#if BLOAD_INSTANCES || BSAVE_INSTANCES
   CL_AllocateEnvironmentData(theEnv,INSTANCE_FILE_DATA,sizeof(struct instanceFileData),NULL);

   InstanceFileData(theEnv)->InstanceBinaryPrefixID = "\5\6\7CLIPS";
   InstanceFileData(theEnv)->InstanceBinaryVersionID = "V6.00";
#endif

#if (! RUN_TIME)
   CL_AddUDF(theEnv,"save-instances","l",1,UNBOUNDED,"y;sy",CL_SaveCL_InstancesCommand,"CL_SaveCL_InstancesCommand",NULL);
   CL_AddUDF(theEnv,"load-instances","l",1,1,"sy",CL_LoadCL_InstancesCommand,"CL_LoadCL_InstancesCommand",NULL);
   CL_AddUDF(theEnv,"restore-instances","l",1,1,"sy",RestoreCL_InstancesCommand,"RestoreCL_InstancesCommand",NULL);

#if BSAVE_INSTANCES
   CL_AddUDF(theEnv,"bsave-instances","l",1,UNBOUNDED,"y;sy",CL_BinaryCL_SaveCL_InstancesCommand,"CL_BinaryCL_SaveCL_InstancesCommand",NULL);
#endif
#if BLOAD_INSTANCES
   CL_AddUDF(theEnv,"bload-instances","l",1,1,"sy",CL_BinaryCL_LoadCL_InstancesCommand,"CL_BinaryCL_LoadCL_InstancesCommand",NULL);
#endif

#endif
  }


/****************************************************************************
  NAME         : CL_SaveCL_InstancesCommand
  DESCRIPTION  : H/L interface for saving
                   current instances to a file
  INPUTS       : None
  RETURNS      : The number of instances saved
  SIDE EFFECTS : CL_Instances saved to named file
  NOTES        : H/L Syntax :
                 (save-instances <file> [local|visible [[inherit] <class>+]])
 ****************************************************************************/
void CL_SaveCL_InstancesCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->integerValue = CL_CreateInteger(theEnv,CL_InstancesCL_SaveCommandParser(context,CL_SaveCL_InstancesDriver));
  }

/******************************************************
  NAME         : CL_LoadCL_InstancesCommand
  DESCRIPTION  : H/L interface for loading
                   instances from a file
  INPUTS       : None
  RETURNS      : The number of instances loaded
  SIDE EFFECTS : CL_Instances loaded from named file
  NOTES        : H/L Syntax : (load-instances <file>)
 ******************************************************/
void CL_LoadCL_InstancesCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *fileFound;
   UDFValue theArg;
   long instanceCount;

   if (! CL_UDFFirstArgument(context,LEXEME_BITS,&theArg))
     { return; }

   fileFound = theArg.lexemeValue->contents;

   instanceCount = CL_LoadCL_Instances(theEnv,fileFound);
   if (CL_EvaluationData(theEnv)->CL_EvaluationError)
     { ProcessFileErrorMessage(theEnv,"load-instances",fileFound); }

   returnValue->integerValue = CL_CreateInteger(theEnv,instanceCount);
  }

/***************************************************
  NAME         : CL_LoadCL_Instances
  DESCRIPTION  : CL_Loads instances from named file
  INPUTS       : The name of the input file
  RETURNS      : The number of instances loaded
  SIDE EFFECTS : CL_Instances loaded from file
  NOTES        : None
 ***************************************************/
long CL_LoadCL_Instances(
  Environment *theEnv,
  const char *file)
  {
   return(CL_LoadOrRestoreCL_Instances(theEnv,file,true,true));
  }

/***************************************************
  NAME         : CL_LoadCL_InstancesFromString
  DESCRIPTION  : CL_Loads instances from given string
  INPUTS       : 1) The input string
                 2) Index of char in string after
                    last valid char (-1 for all chars)
  RETURNS      : The number of instances loaded
  SIDE EFFECTS : CL_Instances loaded from string
  NOTES        : Uses string routers
 ***************************************************/
long CL_LoadCL_InstancesFromString(
  Environment *theEnv,
  const char *theString,
  size_t theMax)
  {
   long theCount;
   const char * theStrRouter = "*** load-instances-from-string ***";

   if ((theMax == SIZE_MAX) ? (! CL_OpenStringSource(theEnv,theStrRouter,theString,0)) :
                              (! CL_OpenTextSource(theEnv,theStrRouter,theString,0,theMax)))
     { return -1; }

   theCount = CL_LoadOrRestoreCL_Instances(theEnv,theStrRouter,true,false);

   CL_CloseStringSource(theEnv,theStrRouter);

   return theCount;
  }

/*********************************************************
  NAME         : RestoreCL_InstancesCommand
  DESCRIPTION  : H/L interface for loading
                   instances from a file w/o messages
  INPUTS       : None
  RETURNS      : The number of instances restored
  SIDE EFFECTS : CL_Instances loaded from named file
  NOTES        : H/L Syntax : (restore-instances <file>)
 *********************************************************/
void RestoreCL_InstancesCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *fileFound;
   UDFValue theArg;
   long instanceCount;

   if (! CL_UDFFirstArgument(context,LEXEME_BITS,&theArg))
     { return; }

   fileFound = theArg.lexemeValue->contents;

   instanceCount = RestoreCL_Instances(theEnv,fileFound);
   if (CL_EvaluationData(theEnv)->CL_EvaluationError)
     { ProcessFileErrorMessage(theEnv,"restore-instances",fileFound); }

   returnValue->integerValue = CL_CreateInteger(theEnv,instanceCount);
  }

/***************************************************
  NAME         : RestoreCL_Instances
  DESCRIPTION  : Restores instances from named file
  INPUTS       : The name of the input file
  RETURNS      : The number of instances restored
  SIDE EFFECTS : CL_Instances restored from file
  NOTES        : None
 ***************************************************/
long RestoreCL_Instances(
  Environment *theEnv,
  const char *file)
  {
   return(CL_LoadOrRestoreCL_Instances(theEnv,file,false,true));
  }

/***************************************************
  NAME         : RestoreCL_InstancesFromString
  DESCRIPTION  : Restores instances from given string
  INPUTS       : 1) The input string
                 2) Index of char in string after
                    last valid char (-1 for all chars)
  RETURNS      : The number of instances loaded
  SIDE EFFECTS : CL_Instances loaded from string
  NOTES        : Uses string routers
 ***************************************************/
long RestoreCL_InstancesFromString(
  Environment *theEnv,
  const char *theString,
  size_t theMax)
  {
   long theCount;
   const char *theStrRouter = "*** load-instances-from-string ***";

   if ((theMax == SIZE_MAX) ? (! CL_OpenStringSource(theEnv,theStrRouter,theString,0)) :
                              (! CL_OpenTextSource(theEnv,theStrRouter,theString,0,theMax)))
     { return(-1); }

   theCount = CL_LoadOrRestoreCL_Instances(theEnv,theStrRouter,false,false);

   CL_CloseStringSource(theEnv,theStrRouter);

   return(theCount);
  }

#if BLOAD_INSTANCES

/*******************************************************
  NAME         : CL_BinaryCL_LoadCL_InstancesCommand
  DESCRIPTION  : H/L interface for loading
                   instances from a binary file
  INPUTS       : None
  RETURNS      : The number of instances loaded
  SIDE EFFECTS : CL_Instances loaded from named binary file
  NOTES        : H/L Syntax : (bload-instances <file>)
 *******************************************************/
void CL_BinaryCL_LoadCL_InstancesCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *fileFound;
   UDFValue theArg;
   long instanceCount;

   if (! CL_UDFFirstArgument(context,LEXEME_BITS,&theArg))
     { return; }

   fileFound = theArg.lexemeValue->contents;

   instanceCount = CL_BinaryCL_LoadCL_Instances(theEnv,fileFound);
   if (CL_EvaluationData(theEnv)->CL_EvaluationError)
     { ProcessFileErrorMessage(theEnv,"bload-instances",fileFound); }
   returnValue->integerValue = CL_CreateInteger(theEnv,instanceCount);
  }

/****************************************************
  NAME         : CL_BinaryCL_LoadCL_Instances
  DESCRIPTION  : CL_Loads instances quickly from a
                 binary file
  INPUTS       : The file name
  RETURNS      : The number of instances loaded
  SIDE EFFECTS : CL_Instances loaded w/o message-passing
  NOTES        : None
 ****************************************************/
long CL_BinaryCL_LoadCL_Instances(
  Environment *theEnv,
  const char *theFile)
  {
   long i,instanceCount;
   GCBlock gcb;
   
   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/
   
   if (CL_EvaluationData(theEnv)->CurrentExpression == NULL)
     { CL_ResetErrorFlags(theEnv); }

   if (CL_GenOpenReadBinary(theEnv,"bload-instances",theFile) == 0)
     {
      CL_OpenErrorMessage(theEnv,"bload-instances",theFile);
      SetCL_EvaluationError(theEnv,true);
      return -1L;
     }
   if (VerifyBinaryHeader(theEnv,theFile) == false)
     {
      CL_GenCloseBinary(theEnv);
      SetCL_EvaluationError(theEnv,true);
      return -1L;
     }

   CL_GCBlockStart(theEnv,&gcb);
   CL_ReadNeededAtomicValues(theEnv);

   InstanceFileData(theEnv)->BinaryInstanceFileOffset = 0L;

   CL_GenReadBinary(theEnv,&InstanceFileData(theEnv)->BinaryInstanceFileSize,sizeof(unsigned long));
   CL_GenReadBinary(theEnv,&instanceCount,sizeof(long));

   for (i = 0L ; i < instanceCount ; i++)
     {
      if (CL_LoadSingleBinaryInstance(theEnv) == false)
        {
         FreeReadBuffer(theEnv);
         CL_FreeAtomicValueStorage(theEnv);
         CL_GenCloseBinary(theEnv);
         SetCL_EvaluationError(theEnv,true);
         CL_GCBlockEnd(theEnv,&gcb);
         return i;
        }
     }

   FreeReadBuffer(theEnv);
   CL_FreeAtomicValueStorage(theEnv);
   CL_GenCloseBinary(theEnv);

   CL_GCBlockEnd(theEnv,&gcb);
   return instanceCount;
  }

#endif

/*******************************************************
  NAME         : CL_SaveCL_Instances
  DESCRIPTION  : CL_Saves current instances to named file
  INPUTS       : 1) The name of the output file
                 2) A flag indicating whether to
                    save local (current module only)
                    or visible instances
                    LOCAL_SAVE or VISIBLE_SAVE
                 3) A list of expressions containing
                    the names of classes for which
                    instances are to be saved
                 4) A flag indicating if the subclasses
                    of specified classes shoudl also
                    be processed
  RETURNS      : The number of instances saved
  SIDE EFFECTS : CL_Instances saved to file
  NOTES        : None
 *******************************************************/
long CL_SaveCL_Instances(
  Environment *theEnv,
  const char *file,
  CL_SaveScope saveCode)
  {
   return CL_SaveCL_InstancesDriver(theEnv,file,saveCode,NULL,true);
  }

/*******************************************************
  NAME         : CL_SaveCL_InstancesDriver
  DESCRIPTION  : CL_Saves current instances to named file
  INPUTS       : 1) The name of the output file
                 2) A flag indicating whether to
                    save local (current module only)
                    or visible instances
                    LOCAL_SAVE or VISIBLE_SAVE
                 3) A list of expressions containing
                    the names of classes for which
                    instances are to be saved
                 4) A flag indicating if the subclasses
                    of specified classes shoudl also
                    be processed
  RETURNS      : The number of instances saved
  SIDE EFFECTS : CL_Instances saved to file
  NOTES        : None
 *******************************************************/
long CL_SaveCL_InstancesDriver(
  Environment *theEnv,
  const char *file,
  CL_SaveScope saveCode,
  Expression *classExpressionList,
  bool inheritFlag)
  {
   FILE *sfile = NULL;
   bool oldPEC, oldATS, oldIAN;
   struct classItem *classList;
   long instanceCount;
   
   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/

   if (CL_EvaluationData(theEnv)->CurrentExpression == NULL)
     { CL_ResetErrorFlags(theEnv); }

   classList = ProcessCL_SaveClassList(theEnv,"save-instances",classExpressionList,
                                    saveCode,inheritFlag);
   if ((classList == NULL) && (classExpressionList != NULL))
     return(0L);

   CL_SaveOrMarkCL_Instances(theEnv,sfile,saveCode,classList,
                             inheritFlag,true,NULL);

   if ((sfile = CL_GenOpen(theEnv,file,"w")) == NULL)
     {
      CL_OpenErrorMessage(theEnv,"save-instances",file);
      ReturnCL_SaveClassList(theEnv,classList);
      SetCL_EvaluationError(theEnv,true);
      return(0L);
     }

   oldPEC = PrintUtilityData(theEnv)->PreserveEscapedCharacters;
   PrintUtilityData(theEnv)->PreserveEscapedCharacters = true;
   oldATS = PrintUtilityData(theEnv)->AddressesToStrings;
   PrintUtilityData(theEnv)->AddressesToStrings = true;
   oldIAN = PrintUtilityData(theEnv)->InstanceAddressesToNames;
   PrintUtilityData(theEnv)->InstanceAddressesToNames = true;

   SetFastCL_Save(theEnv,sfile);
   instanceCount = CL_SaveOrMarkCL_Instances(theEnv,sfile,saveCode,classList,
                                       inheritFlag,true,CL_SaveSingleInstanceText);
   CL_GenClose(theEnv,sfile);
   SetFastCL_Save(theEnv,NULL);

   PrintUtilityData(theEnv)->PreserveEscapedCharacters = oldPEC;
   PrintUtilityData(theEnv)->AddressesToStrings = oldATS;
   PrintUtilityData(theEnv)->InstanceAddressesToNames = oldIAN;
   ReturnCL_SaveClassList(theEnv,classList);
   return(instanceCount);
  }

#if BSAVE_INSTANCES

/****************************************************************************
  NAME         : CL_BinaryCL_SaveCL_InstancesCommand
  DESCRIPTION  : H/L interface for saving
                   current instances to a binary file
  INPUTS       : None
  RETURNS      : The number of instances saved
  SIDE EFFECTS : CL_Instances saved (in binary foCL_rmat) to named file
  NOTES        : H/L Syntax :
                 (bsave-instances <file> [local|visible [[inherit] <class>+]])
 *****************************************************************************/
void CL_BinaryCL_SaveCL_InstancesCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->integerValue = CL_CreateInteger(theEnv,CL_InstancesCL_SaveCommandParser(context,CL_BinaryCL_SaveCL_InstancesDriver));
  }

/*******************************************************
  NAME         : CL_BinaryCL_SaveCL_Instances
  DESCRIPTION  : CL_Saves current instances to binary file
  INPUTS       : 1) The name of the output file
                 2) A flag indicating whether to
                    save local (current module only)
                    or visible instances
                    LOCAL_SAVE or VISIBLE_SAVE
  RETURNS      : The number of instances saved
  SIDE EFFECTS : CL_Instances saved to file
  NOTES        : None
 *******************************************************/
long CL_BinaryCL_SaveCL_Instances(
  Environment *theEnv,
  const char *file,
  CL_SaveScope saveCode)
  {
   return CL_BinaryCL_SaveCL_InstancesDriver(theEnv,file,saveCode,NULL,true);
  }

/*******************************************************
  NAME         : CL_BinaryCL_SaveCL_InstancesDriver
  DESCRIPTION  : CL_Saves current instances to binary file
  INPUTS       : 1) The name of the output file
                 2) A flag indicating whether to
                    save local (current module only)
                    or visible instances
                    LOCAL_SAVE or VISIBLE_SAVE
                 3) A list of expressions containing
                    the names of classes for which
                    instances are to be saved
                 4) A flag indicating if the subclasses
                    of specified classes shoudl also
                    be processed
  RETURNS      : The number of instances saved
  SIDE EFFECTS : CL_Instances saved to file
  NOTES        : None
 *******************************************************/
long CL_BinaryCL_SaveCL_InstancesDriver(
  Environment *theEnv,
  const char *file,
  CL_SaveScope saveCode,
  Expression *classExpressionList,
  bool inheritFlag)
  {
   struct classItem *classList;
   FILE *bsaveFP;
   long instanceCount;
   
   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/

   if (CL_EvaluationData(theEnv)->CurrentExpression == NULL)
     { CL_ResetErrorFlags(theEnv); }

   classList = ProcessCL_SaveClassList(theEnv,"bsave-instances",classExpressionList,
                                    saveCode,inheritFlag);
   if ((classList == NULL) && (classExpressionList != NULL))
     return(0L);

   InstanceFileData(theEnv)->BinaryInstanceFileSize = 0L;
   CL_InitAtomicValueNeededFlags(theEnv);
   instanceCount = CL_SaveOrMarkCL_Instances(theEnv,NULL,saveCode,classList,inheritFlag,
                                       false,MarkSingleInstance);

   if ((bsaveFP = CL_GenOpen(theEnv,file,"wb")) == NULL)
     {
      CL_OpenErrorMessage(theEnv,"bsave-instances",file);
      ReturnCL_SaveClassList(theEnv,classList);
      SetCL_EvaluationError(theEnv,true);
      return(0L);
     }
   CL_WriteBinaryHeader(theEnv,bsaveFP);
   CL_WriteNeededAtomicValues(theEnv,bsaveFP);

   fwrite(&InstanceFileData(theEnv)->BinaryInstanceFileSize,sizeof(unsigned long),1,bsaveFP);
   fwrite(&instanceCount,sizeof(long),1,bsaveFP);

   CL_SetAtomicValueIndices(theEnv,false);
   CL_SaveOrMarkCL_Instances(theEnv,bsaveFP,saveCode,classList,
                       inheritFlag,false,CL_SaveSingleInstanceBinary);
   CL_RestoreAtomicValueBuckets(theEnv);
   CL_GenClose(theEnv,bsaveFP);
   ReturnCL_SaveClassList(theEnv,classList);
   return(instanceCount);
  }

#endif

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/******************************************************
  NAME         : CL_InstancesCL_SaveCommandParser
  DESCRIPTION  : Argument parser for save-instances
                 and bsave-instances
  INPUTS       : 1) The name of the calling function
                 2) A pointer to the support
                    function to call for the save/bsave
  RETURNS      : The number of instances saved
  SIDE EFFECTS : CL_Instances saved/bsaved
  NOTES        : None
 ******************************************************/
static long CL_InstancesCL_SaveCommandParser(
  UDFContext *context,
  long (*saveFunction)(Environment *,const char *,CL_SaveScope,Expression *,bool))
  {
   const char *fileFound;
   UDFValue temp;
   unsigned int argCount;
   CL_SaveScope saveCode = LOCAL_SAVE;
   Expression *classList = NULL;
   bool inheritFlag = false;
   Environment *theEnv = context->environment;

   if (! CL_UDFFirstArgument(context,LEXEME_BITS,&temp))
     { return 0L; }
   fileFound = temp.lexemeValue->contents;

   argCount = CL_UDFArgumentCount(context);
   if (argCount > 1)
     {
      if (! CL_UDFNextArgument(context,SYMBOL_BIT,&temp))
        { return 0L; }

      if (strcmp(temp.lexemeValue->contents,"local") == 0)
        saveCode = LOCAL_SAVE;
      else if (strcmp(temp.lexemeValue->contents,"visible") == 0)
        saveCode = VISIBLE_SAVE;
      else
        {
         CL_UDFInvalidArgumentMessage(context,"symbol \"local\" or \"visible\"");
         SetCL_EvaluationError(theEnv,true);
         return(0L);
        }
      classList = GetFirstArgument()->nextArg->nextArg;

      /* ===========================
         Check for "inherit" keyword
         Must be at least one class
         name following
         =========================== */
      if ((classList != NULL) ? (classList->nextArg != NULL) : false)
        {
         if ((classList->type != SYMBOL_TYPE) ? false :
             (strcmp(classList->lexemeValue->contents,"inherit") == 0))
           {
            inheritFlag = true;
            classList = classList->nextArg;
           }
        }
     }

   return((*saveFunction)(theEnv,fileFound,saveCode,classList,inheritFlag));
  }

/****************************************************
  NAME         : ProcessCL_SaveClassList
  DESCRIPTION  : CL_Evaluates a list of class name
                 expressions and stores them in a
                 data object list
  INPUTS       : 1) The name of the calling function
                 2) The class expression list
                 3) A flag indicating if only local
                    or all visible instances are
                    being saved
                 4) A flag indicating if inheritance
                    relationships should be checked
                    between classes
  RETURNS      : The evaluated class pointer data
                 objects - NULL on errors
  SIDE EFFECTS : Data objects allocated and
                 classes validated
  NOTES        : None
 ****************************************************/
static struct classItem *ProcessCL_SaveClassList(
  Environment *theEnv,
  const char *functionName,
  Expression *classExps,
  CL_SaveScope saveCode,
  bool inheritFlag)
  {
   struct classItem *head = NULL, *prv, *newItem;
   UDFValue tmp;
   Defclass *theDefclass;
   Defmodule *currentModule;
   unsigned int argIndex = inheritFlag ? 4 : 3;

   currentModule = CL_GetCurrentModule(theEnv);
   while (classExps != NULL)
     {
      if (CL_EvaluateExpression(theEnv,classExps,&tmp))
        goto ProcessClassListError;

      if (tmp.header->type != SYMBOL_TYPE)
        goto ProcessClassListError;
        
      if (saveCode == LOCAL_SAVE)
        { theDefclass = CL_LookupDefclassAnywhere(theEnv,currentModule,tmp.lexemeValue->contents); }
      else
        { theDefclass = CL_LookupDefclassByMdlOrScope(theEnv,tmp.lexemeValue->contents); }

      if (theDefclass == NULL)
        goto ProcessClassListError;
      else if (theDefclass->abstract && (inheritFlag == false))
        goto ProcessClassListError;
        
      prv = newItem = head;
      while (newItem != NULL)
        {
         if (newItem->classPtr == theDefclass)
           goto ProcessClassListError;
         else if (inheritFlag)
           {
            if (CL_HasSuperclass(newItem->classPtr,theDefclass) ||
                CL_HasSuperclass(theDefclass,newItem->classPtr))
             goto ProcessClassListError;
           }
         prv = newItem;
         newItem = newItem->next;
        }
        
      newItem = get_struct(theEnv,classItem);
      newItem->classPtr = theDefclass;
      newItem->next = NULL;
      
      if (prv == NULL)
        head = newItem;
      else
        prv->next = newItem;
        
      argIndex++;
      classExps = classExps->nextArg;
     }
   return head;

ProcessClassListError:
   if (inheritFlag)
     CL_ExpectedTypeError1(theEnv,functionName,argIndex,"'valid class name'");
   else
     CL_ExpectedTypeError1(theEnv,functionName,argIndex,"'valid concrete class name'");
   
   ReturnCL_SaveClassList(theEnv,head);
   
   SetCL_EvaluationError(theEnv,true);
   return NULL;
  }

/****************************************************
  NAME         : ReturnCL_SaveClassList
  DESCRIPTION  : Deallocates the class data object
                 list created by ProcessCL_SaveClassList
  INPUTS       : The class data object list
  RETURNS      : Nothing useful
  SIDE EFFECTS : Class data object returned
  NOTES        : None
 ****************************************************/
static void ReturnCL_SaveClassList(
  Environment *theEnv,
  struct classItem *classList)
  {
   struct classItem *tmp;

   while (classList != NULL)
     {
      tmp = classList;
      classList = classList->next;
      rtn_struct(theEnv,classItem,tmp);
     }
  }

/***************************************************
  NAME         : CL_SaveOrMarkCL_Instances
  DESCRIPTION  : Iterates through all specified
                 instances either marking needed
                 atoms or writing instances in
                 binary/text foCL_rmat
  INPUTS       : 1) NULL (for marking) or
                    file pointer (for text/binary saves)
                 2) A cope flag indicating LOCAL
                    or VISIBLE saves only
                 3) A list of data objects
                    containing the names of classes
                    of instances to be saved
                 4) A flag indicating whether to
                    include subclasses of arg #3
                 5) A flag indicating if the
                    iteration can be interrupted
                    or not
                 6) The access function to mark
                    or save an instance (can be NULL
                    if only counting instances)
  RETURNS      : The number of instances saved
  SIDE EFFECTS : CL_Instances amrked or saved
  NOTES        : None
 ***************************************************/
static long CL_SaveOrMarkCL_Instances(
  Environment *theEnv,
  FILE *theOutput,
  int saveCode,
  struct classItem *classList,
  bool inheritFlag,
  bool interruptOK,
  void (*saveInstanceFunc)(Environment *,FILE *,Instance *))
  {
   Defmodule *currentModule;
   int traversalID;
   struct classItem *tmp;
   Instance *ins;
   long instanceCount = 0L;

   currentModule = CL_GetCurrentModule(theEnv);
   if (classList != NULL)
     {
      traversalID = CL_GetTraversalID(theEnv);
      if (traversalID != -1)
        {
         for (tmp = classList ;
              (! ((tmp == NULL) || (CL_EvaluationData(theEnv)->CL_HaltExecution && interruptOK))) ;
              tmp = tmp->next)
           instanceCount += CL_SaveOrMarkCL_InstancesOfClass(theEnv,theOutput,currentModule,saveCode,
                                                       tmp->classPtr,inheritFlag,
                                                       traversalID,saveInstanceFunc);
         CL_ReleaseTraversalID(theEnv);
        }
     }
   else
     {
      for (ins = CL_GetNextInstanceInScope(theEnv,NULL) ;
           (ins != NULL) && (CL_EvaluationData(theEnv)->CL_HaltExecution != true) ;
           ins = CL_GetNextInstanceInScope(theEnv,ins))
        {
         if ((saveCode == VISIBLE_SAVE) ? true :
             (ins->cls->header.whichModule->theModule == currentModule))
           {
            if (saveInstanceFunc != NULL)
              (*saveInstanceFunc)(theEnv,theOutput,ins);
            instanceCount++;
           }
        }
     }
   return(instanceCount);
  }

/***************************************************
  NAME         : CL_SaveOrMarkCL_InstancesOfClass
  DESCRIPTION  : CL_Saves off the direct (and indirect)
                 instance of the specified class
  INPUTS       : 1) The logical name of the output
                    (or file pointer for binary
                     output)
                 2) The current module
                 3) A flag indicating local
                    or visible saves
                 4) The defclass
                 5) A flag indicating whether to
                    save subclass instances or not
                 6) A traversal id for marking
                    visited classes
                 7) A pointer to the instance
                    manipulation function to call
                    (can be NULL for only counting
                     instances)
  RETURNS      : The number of instances saved
  SIDE EFFECTS : Appropriate instances saved
  NOTES        : None
 ***************************************************/
static long CL_SaveOrMarkCL_InstancesOfClass(
  Environment *theEnv,
  FILE *theOutput,
  Defmodule *currentModule,
  int saveCode,
  Defclass *theDefclass,
  bool inheritFlag,
  int traversalID,
  void (*saveInstanceFunc)(Environment *,FILE *,Instance *))
  {
   Instance *theInstance;
   Defclass *subclass;
   unsigned long i;
   long instanceCount = 0L;

   if (TestTraversalID(theDefclass->traversalRecord,traversalID))
     return(instanceCount);
   SetTraversalID(theDefclass->traversalRecord,traversalID);
   if (((saveCode == LOCAL_SAVE) &&
        (theDefclass->header.whichModule->theModule == currentModule)) ||
       ((saveCode == VISIBLE_SAVE) &&
        CL_DefclassInScope(theEnv,theDefclass,currentModule)))
     {
      for (theInstance = CL_GetNextInstanceInClass(theDefclass,NULL);
           theInstance != NULL;
           theInstance = CL_GetNextInstanceInClass(theDefclass,theInstance))
        {
         if (saveInstanceFunc != NULL)
           (*saveInstanceFunc)(theEnv,theOutput,theInstance);
         instanceCount++;
        }
     }
   if (inheritFlag)
     {
      for (i = 0 ; i < theDefclass->directSubclasses.classCount ; i++)
        {
         subclass = theDefclass->directSubclasses.classArray[i];
           instanceCount += CL_SaveOrMarkCL_InstancesOfClass(theEnv,theOutput,currentModule,saveCode,
                                                       subclass,true,traversalID,
                                                       saveInstanceFunc);
        }
     }
   return(instanceCount);
  }

/***************************************************
  NAME         : CL_SaveSingleInstanceText
  DESCRIPTION  : CL_Writes given instance to file
  INPUTS       : 1) The logical name of the output
                 2) The instance to save
  RETURNS      : Nothing useful
  SIDE EFFECTS : Instance written
  NOTES        : None
 ***************************************************/
static void CL_SaveSingleInstanceText(
  Environment *theEnv,
  FILE *fastCL_SaveFile,
  Instance *theInstance)
  {
   long i;
   InstanceSlot *sp;
   const char *logicalName = (const char *) fastCL_SaveFile;

   CL_WriteString(theEnv,logicalName,"([");
   CL_WriteString(theEnv,logicalName,theInstance->name->contents);
   CL_WriteString(theEnv,logicalName,"] of ");
   CL_WriteString(theEnv,logicalName,theInstance->cls->header.name->contents);
   for (i = 0 ; i < theInstance->cls->instanceSlotCount ; i++)
     {
      sp = theInstance->slotAddresses[i];
      CL_WriteString(theEnv,logicalName,"\n   (");
      CL_WriteString(theEnv,logicalName,sp->desc->slotName->name->contents);
      if (sp->type != MULTIFIELD_TYPE)
        {
         CL_WriteString(theEnv,logicalName," ");
         CL_PrintAtom(theEnv,logicalName,sp->type,sp->value);
        }
      else if (sp->multifieldValue->length != 0)
        {
         CL_WriteString(theEnv,logicalName," ");
         CL_PrintMultifieldDriver(theEnv,logicalName,sp->multifieldValue,0,
                               sp->multifieldValue->length,false);
        }
      CL_WriteString(theEnv,logicalName,")");
     }
   CL_WriteString(theEnv,logicalName,")\n\n");
  }

#if BSAVE_INSTANCES

/***************************************************
  NAME         : CL_WriteBinaryHeader
  DESCRIPTION  : CL_Writes identifying string to
                 instance binary file to assist in
                 later verification
  INPUTS       : The binary file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary prefix headers written
  NOTES        : None
 ***************************************************/
static void CL_WriteBinaryHeader(
  Environment *theEnv,
  FILE *bsaveFP)
  {
   fwrite(InstanceFileData(theEnv)->InstanceBinaryPrefixID,
          (STD_SIZE) (strlen(InstanceFileData(theEnv)->InstanceBinaryPrefixID) + 1),1,bsaveFP);
   fwrite(InstanceFileData(theEnv)->InstanceBinaryVersionID,
          (STD_SIZE) (strlen(InstanceFileData(theEnv)->InstanceBinaryVersionID) + 1),1,bsaveFP);
  }

/***************************************************
  NAME         : MarkSingleInstance
  DESCRIPTION  : Marks all the atoms needed in
                 the slot values of an instance
  INPUTS       : 1) The output (ignored)
                 2) The instance
  RETURNS      : Nothing useful
  SIDE EFFECTS : Instance slot value atoms marked
  NOTES        : None
 ***************************************************/
static void MarkSingleInstance(
  Environment *theEnv,
  FILE *theOutput,
  Instance *theInstance)
  {
#if MAC_XCD
#pragma unused(theOutput)
#endif
   InstanceSlot *sp;
   unsigned int i;
   size_t j;

   InstanceFileData(theEnv)->BinaryInstanceFileSize += (sizeof(long) * 2);
   theInstance->name->neededSymbol = true;
   theInstance->cls->header.name->neededSymbol = true;
   InstanceFileData(theEnv)->BinaryInstanceFileSize +=
                       ((sizeof(long) * 2) +
                        (sizeof(struct bsaveSlotValue) *
                         theInstance->cls->instanceSlotCount) +
                        sizeof(unsigned long) +
                        sizeof(unsigned));
   for (i = 0 ; i < theInstance->cls->instanceSlotCount ; i++)
     {
      sp = theInstance->slotAddresses[i];
      sp->desc->slotName->name->neededSymbol = true;
      if (sp->desc->multiple)
        {
         for (j = 0 ; j < sp->multifieldValue->length ; j++)
           MarkNeededAtom(theEnv,sp->multifieldValue->contents[j].header->type,
                                 sp->multifieldValue->contents[j].value);
        }
      else
        MarkNeededAtom(theEnv,sp->type,sp->value);
     }
  }

/***************************************************
  NAME         : MarkNeededAtom
  DESCRIPTION  : Marks an integer/float/symbol as
                 being need by a set of instances
  INPUTS       : 1) The type of atom
                 2) The value of the atom
  RETURNS      : Nothing useful
  SIDE EFFECTS : Atom marked for saving
  NOTES        : None
 ***************************************************/
static void MarkNeededAtom(
  Environment *theEnv,
  unsigned short type,
  void *value)
  {
   InstanceFileData(theEnv)->BinaryInstanceFileSize += sizeof(struct bsaveSlotValueAtom);

   /* =====================================
      Assumes slot value atoms  can only be
      floats, integers, symbols, strings,
      instance-names, instance-addresses,
      fact-addresses or external-addresses
      ===================================== */
   switch (type)
     {
      case SYMBOL_TYPE:
      case STRING_TYPE:
      case INSTANCE_NAME_TYPE:
         ((CLIPSLexeme *) value)->neededSymbol = true;
         break;
      case FLOAT_TYPE:
         ((CLIPSFloat *) value)->neededFloat = true;
         break;
      case INTEGER_TYPE:
         ((CLIPSInteger *) value)->neededInteger = true;
         break;
      case INSTANCE_ADDRESS_TYPE:
         CL_GetFullCL_InstanceName(theEnv,(Instance *) value)->neededSymbol = true;
         break;
     }
  }

/****************************************************
  NAME         : CL_SaveSingleInstanceBinary
  DESCRIPTION  : CL_Writes given instance to binary file
  INPUTS       : 1) Binary file pointer
                 2) The instance to save
  RETURNS      : Nothing useful
  SIDE EFFECTS : Instance written
  NOTES        : None
 ****************************************************/
static void CL_SaveSingleInstanceBinary(
  Environment *theEnv,
  FILE *bsaveFP,
  Instance *theInstance)
  {
   unsigned long nameIndex;
   unsigned long i;
   size_t j;
   InstanceSlot *sp;
   struct bsaveSlotValue bs;
   unsigned long totalValueCount = 0;
   size_t slotLen;

   /*==============================*/
   /* CL_Write out the instance name. */
   /*==============================*/
   
   nameIndex = theInstance->name->bucket;
   fwrite(&nameIndex,sizeof(unsigned long),1,bsaveFP);

   /*===========================*/
   /* CL_Write out the class name. */
   /*===========================*/
   
   nameIndex = theInstance->cls->header.name->bucket;
   fwrite(&nameIndex,sizeof(unsigned long),1,bsaveFP);

   /*=========================================*/
   /* CL_Write out the number of slot-overrides. */
   /*=========================================*/
   
   fwrite(&theInstance->cls->instanceSlotCount,
          sizeof(short),1,bsaveFP);

   /*============================================*/
   /* CL_Write out the slot names and value counts. */
   /*============================================*/
   
   for (i = 0 ; i < theInstance->cls->instanceSlotCount ; i++)
     {
      sp = theInstance->slotAddresses[i];

      /* ===============================================
         CL_Write out the number of atoms in the slot value
         =============================================== */
      bs.slotName = sp->desc->slotName->name->bucket;
      bs.valueCount = (unsigned long) (sp->desc->multiple ? sp->multifieldValue->length : 1);
      fwrite(&bs,sizeof(struct bsaveSlotValue),1,bsaveFP);
      totalValueCount += bs.valueCount;
     }

   /* ==================================
      CL_Write out the number of slot value
      atoms for the whole instance
      ================================== */
   if (theInstance->cls->instanceSlotCount != 0) // (totalValueCount != 0L) : Bug fix if any slots, write out count
     fwrite(&totalValueCount,sizeof(unsigned long),1,bsaveFP);

   /* ==============================
      CL_Write out the slot value atoms
      ============================== */
   for (i = 0 ; i < theInstance->cls->instanceSlotCount ; i++)
     {
      sp = theInstance->slotAddresses[i];
      slotLen = sp->desc->multiple ? sp->multifieldValue->length : 1;

      /* =========================================
         CL_Write out the type and index of each atom
         ========================================= */
      if (sp->desc->multiple)
        {
         for (j = 0 ; j < slotLen ; j++)
           CL_SaveAtomBinary(theEnv,sp->multifieldValue->contents[j].header->type,
                                 sp->multifieldValue->contents[j].value,bsaveFP);
        }
      else
        CL_SaveAtomBinary(theEnv,sp->type,sp->value,bsaveFP);
     }
  }

/***************************************************
  NAME         : CL_SaveAtomBinary
  DESCRIPTION  : CL_Writes out an instance slot value
                 atom to the binary file
  INPUTS       : 1) The atom type
                 2) The atom value
                 3) The binary file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : atom written
  NOTES        :
 ***************************************************/
static void CL_SaveAtomBinary(
  Environment *theEnv,
  unsigned short type,
  void *value,
  FILE *bsaveFP)
  {
   struct bsaveSlotValueAtom bsa;

   /* =====================================
      Assumes slot value atoms  can only be
      floats, integers, symbols, strings,
      instance-names, instance-addresses,
      fact-addresses or external-addresses
      ===================================== */
   bsa.type = type;
   switch (type)
     {
      case SYMBOL_TYPE:
      case STRING_TYPE:
      case INSTANCE_NAME_TYPE:
         bsa.value = ((CLIPSLexeme *) value)->bucket;
         break;
      case FLOAT_TYPE:
         bsa.value = ((CLIPSFloat *) value)->bucket;
         break;
      case INTEGER_TYPE:
         bsa.value = ((CLIPSInteger *) value)->bucket;
         break;
      case INSTANCE_ADDRESS_TYPE:
         bsa.type = INSTANCE_NAME_TYPE;
         bsa.value = CL_GetFullCL_InstanceName(theEnv,(Instance *) value)->bucket;
         break;
      default:
         bsa.value = ULONG_MAX;
     }
     
   fwrite(&bsa,sizeof(struct bsaveSlotValueAtom),1,bsaveFP);
  }

#endif

/**********************************************************************
  NAME         : CL_LoadOrRestoreCL_Instances
  DESCRIPTION  : CL_Loads instances from named file
  INPUTS       : 1) The name of the input file
                 2) An integer flag indicating whether or
                    not to use message-passing to create
                    the new instances and delete old versions
                 3) An integer flag indicating if arg #1
                    is a file name or the name of a string router
  RETURNS      : The number of instances loaded/restored
  SIDE EFFECTS : CL_Instances loaded from file
  NOTES        : None
 **********************************************************************/
static long CL_LoadOrRestoreCL_Instances(
  Environment *theEnv,
  const char *file,
  bool usemsgs,
  bool isFileName)
  {
   UDFValue temp;
   FILE *sfile = NULL,*svload = NULL;
   const char *ilog;
   Expression *top;
   bool svoverride;
   long instanceCount = 0L;
   
   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/

   if (CL_EvaluationData(theEnv)->CurrentExpression == NULL)
     { CL_ResetErrorFlags(theEnv); }

   if (isFileName)
     {
      if ((sfile = CL_GenOpen(theEnv,file,"r")) == NULL)
        {
         SetCL_EvaluationError(theEnv,true);
         return -1L;
        }
      svload = CL_GetFastCL_Load(theEnv);
      ilog = (char *) sfile;
      SetFastCL_Load(theEnv,sfile);
     }
   else
     { ilog = file; }
     
   top = CL_GenConstant(theEnv,FCALL,CL_FindFunction(theEnv,"make-instance"));
   CL_GetToken(theEnv,ilog,&DefclassData(theEnv)->ObjectParseToken);
   svoverride = InstanceData(theEnv)->MkInsMsgPass;
   InstanceData(theEnv)->MkInsMsgPass = usemsgs;

   while ((DefclassData(theEnv)->ObjectParseToken.tknType != STOP_TOKEN) && (CL_EvaluationData(theEnv)->CL_HaltExecution != true))
     {
      if (DefclassData(theEnv)->ObjectParseToken.tknType != LEFT_PARENTHESIS_TOKEN)
        {
         CL_SyntaxErrorMessage(theEnv,"instance definition");
         rtn_struct(theEnv,expr,top);
         if (isFileName)
           {
            CL_GenClose(theEnv,sfile);
            SetFastCL_Load(theEnv,svload);
           }
         SetCL_EvaluationError(theEnv,true);
         InstanceData(theEnv)->MkInsMsgPass = svoverride;
         return instanceCount;
        }
        
      if (CL_ParseSimpleInstance(theEnv,top,ilog) == NULL)
        {
         if (isFileName)
           {
            CL_GenClose(theEnv,sfile);
            SetFastCL_Load(theEnv,svload);
           }
         InstanceData(theEnv)->MkInsMsgPass = svoverride;
         SetCL_EvaluationError(theEnv,true);
         return instanceCount;
        }
      CL_ExpressionInstall(theEnv,top);
      CL_EvaluateExpression(theEnv,top,&temp);
      CL_ExpressionDeinstall(theEnv,top);
      if (! CL_EvaluationData(theEnv)->CL_EvaluationError)
        { instanceCount++; }
      CL_ReturnExpression(theEnv,top->argList);
      top->argList = NULL;
      CL_GetToken(theEnv,ilog,&DefclassData(theEnv)->ObjectParseToken);
     }
     
   rtn_struct(theEnv,expr,top);
   if (isFileName)
     {
      CL_GenClose(theEnv,sfile);
      SetFastCL_Load(theEnv,svload);
     }
   InstanceData(theEnv)->MkInsMsgPass = svoverride;
   return instanceCount;
  }

/***************************************************
  NAME         : ProcessFileErrorMessage
  DESCRIPTION  : Prints an error message when a
                 file containing text or binary
                 instances cannot be processed.
  INPUTS       : The name of the input file and the
                 function which opened it.
  RETURNS      : No value
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
static void ProcessFileErrorMessage(
  Environment *theEnv,
  const char *functionName,
  const char *fileName)
  {
   CL_PrintErrorID(theEnv,"INSFILE",1,false);
   CL_WriteString(theEnv,STDERR,"Function '");
   CL_WriteString(theEnv,STDERR,functionName);
   CL_WriteString(theEnv,STDERR,"' could not completely process file '");
   CL_WriteString(theEnv,STDERR,fileName);
   CL_WriteString(theEnv,STDERR,"'.\n");
  }

#if BLOAD_INSTANCES

/*******************************************************
  NAME         : VerifyBinaryHeader
  DESCRIPTION  : Reads the prefix and version headers
                 from a file to verify that the
                 input is a valid binary instances file
  INPUTS       : The name of the file
  RETURNS      : True if OK, false otherwise
  SIDE EFFECTS : Input prefix and version read
  NOTES        : Assumes file already open with
                 CL_GenOpenReadBinary
 *******************************************************/
static bool VerifyBinaryHeader(
  Environment *theEnv,
  const char *theFile)
  {
   char buf[20];

   CL_GenReadBinary(theEnv,buf,(strlen(InstanceFileData(theEnv)->InstanceBinaryPrefixID) + 1));
   if (strcmp(buf,InstanceFileData(theEnv)->InstanceBinaryPrefixID) != 0)
     {
      CL_PrintErrorID(theEnv,"INSFILE",2,false);
      CL_WriteString(theEnv,STDERR,"File '");
      CL_WriteString(theEnv,STDERR,theFile);
      CL_WriteString(theEnv,STDERR,"' is not a binary instances file.\n");
      return false;
     }
   CL_GenReadBinary(theEnv,buf,(strlen(InstanceFileData(theEnv)->InstanceBinaryVersionID) + 1));
   if (strcmp(buf,InstanceFileData(theEnv)->InstanceBinaryVersionID) != 0)
     {
      CL_PrintErrorID(theEnv,"INSFILE",3,false);
      CL_WriteString(theEnv,STDERR,"File '");
      CL_WriteString(theEnv,STDERR,theFile);
      CL_WriteString(theEnv,STDERR,"' is not a compatible binary instances file.\n");
      return false;
     }
   return true;
  }

/***************************************************
  NAME         : CL_LoadSingleBinaryInstance
  DESCRIPTION  : Reads the binary data for a new
                 instance and its slot values and
                 creates/initializes the instance
  INPUTS       : None
  RETURNS      : True if all OK,
                 false otherwise
  SIDE EFFECTS : Binary data read and instance
                 created
  NOTES        : Uses global CL_GenReadBinary(theEnv,)
 ***************************************************/
static bool CL_LoadSingleBinaryInstance(
  Environment *theEnv)
  {
   CLIPSLexeme *instanceName,
             *className;
   unsigned short slotCount;
   Defclass *theDefclass;
   Instance *newInstance;
   struct bsaveSlotValue *bsArray;
   struct bsaveSlotValueAtom *bsaArray = NULL;
   long nameIndex;
   unsigned long totalValueCount;
   long i, j;
   InstanceSlot *sp;
   UDFValue slotValue, junkValue;

   /* =====================
      Get the instance name
      ===================== */
   BufferedRead(theEnv,&nameIndex,sizeof(long));
   instanceName = SymbolPointer(nameIndex);

   /* ==================
      Get the class name
      ================== */
   BufferedRead(theEnv,&nameIndex,sizeof(long));
   className = SymbolPointer(nameIndex);

   /* ==================
      Get the slot count
      ================== */
   BufferedRead(theEnv,&slotCount,sizeof(unsigned short));

   /* =============================
      Make sure the defclass exists
      and check the slot count
      ============================= */
   theDefclass = CL_LookupDefclassByMdlOrScope(theEnv,className->contents);
   if (theDefclass == NULL)
     {
      CL_ClassExistError(theEnv,"bload-instances",className->contents);
      return false;
     }
   if (theDefclass->instanceSlotCount != slotCount)
     {
      BinaryCL_LoadInstanceError(theEnv,instanceName,theDefclass);
      return false;
     }

   /* ===================================
      Create the new unitialized instance
      =================================== */
   newInstance = CL_BuildInstance(theEnv,instanceName,theDefclass,false);
   if (newInstance == NULL)
     {
      BinaryCL_LoadInstanceError(theEnv,instanceName,theDefclass);
      return false;
     }
   if (slotCount == 0)
     return true;

   /* ====================================
      Read all slot override info and slot
      value atoms into big arrays
      ==================================== */
   bsArray = (struct bsaveSlotValue *) CL_gm2(theEnv,(sizeof(struct bsaveSlotValue) * slotCount));
   BufferedRead(theEnv,bsArray,(sizeof(struct bsaveSlotValue) * slotCount));

   BufferedRead(theEnv,&totalValueCount,sizeof(unsigned long));

   if (totalValueCount != 0L)
     {
      bsaArray = (struct bsaveSlotValueAtom *)
                  CL_gm2(theEnv,(totalValueCount * sizeof(struct bsaveSlotValueAtom)));
      BufferedRead(theEnv,bsaArray,(totalValueCount * sizeof(struct bsaveSlotValueAtom)));
     }

   /* =========================
      Insert the values for the
      slot overrides
      ========================= */
   for (i = 0 , j = 0L ; i < slotCount ; i++)
     {
      /* ===========================================================
         Here is another check for the validity of the binary file -
         the order of the slots in the file should match the
         order in the class definition
         =========================================================== */
      sp = newInstance->slotAddresses[i];
      if (sp->desc->slotName->name != SymbolPointer(bsArray[i].slotName))
        goto CL_LoadError;
      CreateSlotValue(theEnv,&slotValue,(struct bsaveSlotValueAtom *) &bsaArray[j],
                      bsArray[i].valueCount);

      if (CL_PutSlotValue(theEnv,newInstance,sp,&slotValue,&junkValue,"bload-instances") != PSE_NO_ERROR)
        goto CL_LoadError;

      j += (unsigned long) bsArray[i].valueCount;
     }

   CL_rm(theEnv,bsArray,(sizeof(struct bsaveSlotValue) * slotCount));

   if (totalValueCount != 0L)
     CL_rm(theEnv,bsaArray,(totalValueCount * sizeof(struct bsaveSlotValueAtom)));

   return true;

CL_LoadError:
   BinaryCL_LoadInstanceError(theEnv,instanceName,theDefclass);
   CL_QuashInstance(theEnv,newInstance);
   CL_rm(theEnv,bsArray,(sizeof(struct bsaveSlotValue) * slotCount));
   CL_rm(theEnv,bsaArray,(totalValueCount * sizeof(struct bsaveSlotValueAtom)));
   return false;
  }

/***************************************************
  NAME         : BinaryCL_LoadInstanceError
  DESCRIPTION  : Prints out an error message when
                 an instance could not be
                 successfully loaded from a
                 binary file
  INPUTS       : 1) The instance name
                 2) The defclass
  RETURNS      : Nothing useful
  SIDE EFFECTS : Error message printed
  NOTES        : None
 ***************************************************/
static void BinaryCL_LoadInstanceError(
  Environment *theEnv,
  CLIPSLexeme *instanceName,
  Defclass *theDefclass)
  {
   CL_PrintErrorID(theEnv,"INSFILE",4,false);
   CL_WriteString(theEnv,STDERR,"Function 'bload-instances' is unable to load instance [");
   CL_WriteString(theEnv,STDERR,instanceName->contents);
   CL_WriteString(theEnv,STDERR,"] of class ");
   CL_PrintClassName(theEnv,STDERR,theDefclass,true,true);
  }

/***************************************************
  NAME         : CreateSlotValue
  DESCRIPTION  : Creates a data object value from
                 the binary slot value atom data
  INPUTS       : 1) A data object buffer
                 2) The slot value atoms array
                 3) The number of values to put
                    in the data object
  RETURNS      : Nothing useful
  SIDE EFFECTS : Data object initialized
                 (if more than one value, a
                 multifield is created)
  NOTES        : None
 ***************************************************/
static void CreateSlotValue(
  Environment *theEnv,
  UDFValue *returnValue,
  struct bsaveSlotValueAtom *bsaValues,
  unsigned long valueCount)
  {
   unsigned i;

   if (valueCount == 0)
     {
      returnValue->value = CL_CreateMultifield(theEnv,0L);
      returnValue->begin = 0;
      returnValue->range = 0;
     }
   else if (valueCount == 1)
     {
      returnValue->value = GetBinaryAtomValue(theEnv,&bsaValues[0]);
     }
   else
     {
      returnValue->value = CL_CreateMultifield(theEnv,valueCount);
      returnValue->begin = 0;
      returnValue->range = valueCount;
      for (i = 0 ; i < valueCount ; i++)
        {
         returnValue->multifieldValue->contents[i].value = GetBinaryAtomValue(theEnv,&bsaValues[i-1]);
        }
     }
  }

/***************************************************
  NAME         : GetBinaryAtomValue
  DESCRIPTION  : Uses the binary index of an atom
                 to find the ephemeris value
  INPUTS       : The binary type and index
  RETURNS      : The symbol/etc. pointer
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
static void *GetBinaryAtomValue(
  Environment *theEnv,
  struct bsaveSlotValueAtom *ba)
  {
   switch (ba->type)
     {
      case SYMBOL_TYPE:
      case STRING_TYPE:
      case INSTANCE_NAME_TYPE:
         return((void *) SymbolPointer(ba->value));
         
      case FLOAT_TYPE:
         return((void *) FloatPointer(ba->value));
         
      case INTEGER_TYPE:
         return((void *) IntegerPointer(ba->value));
         
      case FACT_ADDRESS_TYPE:
#if DEFTEMPLATE_CONSTRUCT && DEFRULE_CONSTRUCT
         return((void *) &FactData(theEnv)->DummyFact);
#else
         return NULL;
#endif

      case EXTERNAL_ADDRESS_TYPE:
        return NULL;

      default:
        {
         CL_SystemError(theEnv,"INSFILE",1);
         CL_ExitRouter(theEnv,EXIT_FAILURE);
        }
     }
   return NULL;
  }

/***************************************************
  NAME         : BufferedRead
  DESCRIPTION  : Reads data from binary file
                 (Larger blocks than requested size
                  may be read and buffered)
  INPUTS       : 1) The buffer
                 2) The buffer size
  RETURNS      : Nothing useful
  SIDE EFFECTS : Data stored in buffer
  NOTES        : None
 ***************************************************/
static void BufferedRead(
  Environment *theEnv,
  void *buf,
  size_t bufsz)
  {
   size_t i, amountLeftToRead;

   if (InstanceFileData(theEnv)->CurrentReadBuffer != NULL)
     {
      amountLeftToRead = InstanceFileData(theEnv)->CurrentReadBufferSize - InstanceFileData(theEnv)->CurrentReadBufferOffset;
      if (bufsz <= amountLeftToRead)
        {
         for (i = 0L ; i < bufsz ; i++)
           ((char *) buf)[i] = InstanceFileData(theEnv)->CurrentReadBuffer[i + InstanceFileData(theEnv)->CurrentReadBufferOffset];
         InstanceFileData(theEnv)->CurrentReadBufferOffset += bufsz;
         if (InstanceFileData(theEnv)->CurrentReadBufferOffset == InstanceFileData(theEnv)->CurrentReadBufferSize)
           FreeReadBuffer(theEnv);
        }
      else
        {
         if (InstanceFileData(theEnv)->CurrentReadBufferOffset < InstanceFileData(theEnv)->CurrentReadBufferSize)
           {
            for (i = 0 ; i < amountLeftToRead ; i++)
              ((char *) buf)[i] = InstanceFileData(theEnv)->CurrentReadBuffer[i + InstanceFileData(theEnv)->CurrentReadBufferOffset];
            bufsz -= amountLeftToRead;
            buf = (void *) (((char *) buf) + amountLeftToRead);
           }
         FreeReadBuffer(theEnv);
         BufferedRead(theEnv,buf,bufsz);
        }
     }
   else
     {
      if (bufsz > MAX_BLOCK_SIZE)
        {
         InstanceFileData(theEnv)->CurrentReadBufferSize = bufsz;
         if (bufsz > (InstanceFileData(theEnv)->BinaryInstanceFileSize - InstanceFileData(theEnv)->BinaryInstanceFileOffset))
           {
            CL_SystemError(theEnv,"INSFILE",2);
            CL_ExitRouter(theEnv,EXIT_FAILURE);
           }
        }
      else if (MAX_BLOCK_SIZE >
              (InstanceFileData(theEnv)->BinaryInstanceFileSize - InstanceFileData(theEnv)->BinaryInstanceFileOffset))
        InstanceFileData(theEnv)->CurrentReadBufferSize = InstanceFileData(theEnv)->BinaryInstanceFileSize - InstanceFileData(theEnv)->BinaryInstanceFileOffset;
      else
        InstanceFileData(theEnv)->CurrentReadBufferSize = (unsigned long) MAX_BLOCK_SIZE;
      InstanceFileData(theEnv)->CurrentReadBuffer = (char *) CL_genalloc(theEnv,InstanceFileData(theEnv)->CurrentReadBufferSize);
      CL_GenReadBinary(theEnv,InstanceFileData(theEnv)->CurrentReadBuffer,InstanceFileData(theEnv)->CurrentReadBufferSize);
      for (i = 0L ; i < bufsz ; i++)
        ((char *) buf)[i] = InstanceFileData(theEnv)->CurrentReadBuffer[i];
      InstanceFileData(theEnv)->CurrentReadBufferOffset = bufsz;
      InstanceFileData(theEnv)->BinaryInstanceFileOffset += InstanceFileData(theEnv)->CurrentReadBufferSize;
     }
  }

/*****************************************************
  NAME         : FreeReadBuffer
  DESCRIPTION  : Deallocates buffer for binary reads
  INPUTS       : None
  RETURNS      : Nothing usefu
  SIDE EFFECTS : Binary global read buffer deallocated
  NOTES        : None
 *****************************************************/
static void FreeReadBuffer(
  Environment *theEnv)
  {
   if (InstanceFileData(theEnv)->CurrentReadBufferSize != 0L)
     {
      CL_genfree(theEnv,InstanceFileData(theEnv)->CurrentReadBuffer,InstanceFileData(theEnv)->CurrentReadBufferSize);
      InstanceFileData(theEnv)->CurrentReadBuffer = NULL;
      InstanceFileData(theEnv)->CurrentReadBufferSize = 0L;
     }
  }

#endif /* BLOAD_INSTANCES */

#endif /* OBJECT_SYSTEM */
