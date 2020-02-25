   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/05/18             */
   /*                                                     */
   /*                I/O FUNCTIONS MODULE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for several I/O functions      */
/*   including printout, read, open, close, remove, rename,  */
/*   fo_rmat, and readline.                                   */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*      Gary D. Riley                                        */
/*      Bebe Ly                                              */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added the get-char, set-locale, and            */
/*            read-number functions.                         */
/*                                                           */
/*            Modified printing of floats in the fo_rmat      */
/*            function to use the locale from the set-locale */
/*            function.                                      */
/*                                                           */
/*            Moved CL_IllegalLogicalNameMessage function to    */
/*            argacces.c.                                    */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Removed the undocumented use of t in the       */
/*            printout command to perfo_rm the same function  */
/*            as crlf.                                       */
/*                                                           */
/*            Replaced EXT_IO and BASIC_IO compiler flags    */
/*            with IO_FUNCTIONS compiler flag.               */
/*                                                           */
/*            Added rb and ab and removed r+ modes for the   */
/*            open function.                                 */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Used CL_gensprintf instead of sprintf.            */
/*                                                           */
/*            Added put-char function.                       */
/*                                                           */
/*            Added CL_SetFullCRLF which allows option to       */
/*            specify crlf as \n or \r\n.                    */
/*                                                           */
/*            Added AwaitingInput flag.                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Added STDOUT and STDIN logical name            */
/*            definitions.                                   */
/*                                                           */
/*      6.40: Modified ReadTokenFromStdin to capture         */
/*            carriage returns in the input buffer so that   */
/*            input buffer count will accurately reflect     */
/*            the number of characters typed for GUI         */
/*            interfaces that support deleting carriage      */
/*            returns.                                       */
/*                                                           */
/*            Added Env prefix to Get_EvaluationError and     */
/*            Set_EvaluationError functions.                  */
/*                                                           */
/*            Added Env prefix to CL_Get_HaltExecution and       */
/*            Set_HaltExecution functions.                    */
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
/*            Added print and println functions.             */
/*                                                           */
/*            Read function now returns symbols for tokens   */
/*            that are not primitive values.                 */
/*                                                           */
/*            Added unget-char function.                     */
/*                                                           */
/*            Added r+, w+, a+, rb+, wb+, and ab+ file       */
/*            access modes for the open function.            */
/*                                                           */
/*            Added flush, rewind, tell, seek, and chdir     */
/*            functions.                                     */
/*                                                           */
/*            Changed error return value of read, readline,  */
/*            and read-number functions to FALSE and added   */
/*            an error code for read.                        */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if IO_FUNCTIONS
#include <locale.h>
#include <stdlib.h>
#include <ctype.h>
#endif

#include <stdio.h>
#include <string.h>

#include "argacces.h"
#include "commline.h"
#include "constant.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "filertr.h"
#include "memalloc.h"
#include "miscfun.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"
#include "strngrtr.h"
#include "sysdep.h"
#include "utility.h"

#include "iofun.h"

/***************/
/* DEFINITIONS */
/***************/

#define FORMAT_MAX 512
#define FLAG_MAX    80

/********************/
/* ENVIRONMENT DATA */
/********************/

#define IO_FUNCTION_DATA 64

struct IOFunctionData
  {
   CLIPSLexeme *locale;
   bool useFullCRLF;
  };

#define IOFunctionData(theEnv) ((struct IOFunctionData *) GetEnvironmentData(theEnv,IO_FUNCTION_DATA))

/****************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS  */
/****************************************/

#if IO_FUNCTIONS
   static void             ReadTokenFromStdin(Environment *,struct token *);
   static const char      *ControlStringCheck(UDFContext *,unsigned int);
   static char             FindFo_rmatFlag(const char *,size_t *,char *,size_t);
   static const char      *PrintFo_rmatFlag(UDFContext *,const char *,unsigned int,int);
   static char            *FillBuffer(Environment *,const char *,size_t *,size_t *);
   static void             ReadNumber(Environment *,const char *,struct token *,bool);
   static void             PrintDriver(UDFContext *,const char *,bool);
#endif

/**************************************/
/* CL_IOFunctionDefinitions: Initializes */
/*   the I/O functions.               */
/**************************************/
void CL_IOFunctionDefinitions(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,IO_FUNCTION_DATA,sizeof(struct IOFunctionData),NULL);

#if IO_FUNCTIONS
   IOFunctionData(theEnv)->useFullCRLF = false;
   IOFunctionData(theEnv)->locale = CL_CreateSymbol(theEnv,setlocale(LC_ALL,NULL));
   IncrementLexemeCount(IOFunctionData(theEnv)->locale);
#endif

#if ! RUN_TIME
#if IO_FUNCTIONS
   CL_AddUDF(theEnv,"printout","v",1,UNBOUNDED,"*;ldsyn",CL_PrintoutFunction,"CL_PrintoutFunction",NULL);
   CL_AddUDF(theEnv,"print","v",0,UNBOUNDED,NULL,CL_PrintFunction,"CL_PrintFunction",NULL);
   CL_AddUDF(theEnv,"println","v",0,UNBOUNDED,NULL,CL_PrintlnFunction,"CL_PrintlnFunction",NULL);
   CL_AddUDF(theEnv,"read","synldfie",0,1,";ldsyn",CL_ReadFunction,"CL_ReadFunction",NULL);
   CL_AddUDF(theEnv,"open","b",2,3,"*;sy;ldsyn;s",CL_OpenFunction,"CL_OpenFunction",NULL);
   CL_AddUDF(theEnv,"close","b",0,1,"ldsyn",CL_CloseFunction,"CL_CloseFunction",NULL);
   CL_AddUDF(theEnv,"flush","b",0,1,"ldsyn",CL_FlushFunction,"CL_FlushFunction",NULL);
   CL_AddUDF(theEnv,"rewind","b",1,1,";ldsyn",CL_RewindFunction,"CL_RewindFunction",NULL);
   CL_AddUDF(theEnv,"tell","lb",1,1,";ldsyn",CL_TellFunction,"CL_TellFunction",NULL);
   CL_AddUDF(theEnv,"seek","b",3,3,";ldsyn;l;y",CL_SeekFunction,"CL_SeekFunction",NULL);
   CL_AddUDF(theEnv,"get-char","l",0,1,";ldsyn",CL_GetCharFunction,"CL_GetCharFunction",NULL);
   CL_AddUDF(theEnv,"unget-char","l",1,2,";ldsyn;l",CL_UngetCharFunction,"CL_UngetCharFunction",NULL);
   CL_AddUDF(theEnv,"put-char","v",1,2,";ldsyn;l",CL_PutCharFunction,"CL_PutCharFunction",NULL);
   CL_AddUDF(theEnv,"remove","b",1,1,"sy",CL_RemoveFunction,"CL_RemoveFunction",NULL);
   CL_AddUDF(theEnv,"rename","b",2,2,"sy",CL_RenameFunction,"CL_RenameFunction",NULL);
   CL_AddUDF(theEnv,"fo_rmat","s",2,UNBOUNDED,"*;ldsyn;s",CL_Fo_rmatFunction,"CL_Fo_rmatFunction",NULL);
   CL_AddUDF(theEnv,"readline","sy",0,1,";ldsyn",CL_ReadlineFunction,"CL_ReadlineFunction",NULL);
   CL_AddUDF(theEnv,"set-locale","sy",0,1,";s",CL_SetLocaleFunction,"CL_SetLocaleFunction",NULL);
   CL_AddUDF(theEnv,"read-number","syld",0,1,";ldsyn",CL_ReadNumberFunction,"CL_ReadNumberFunction",NULL);
   CL_AddUDF(theEnv,"chdir","b",0,1,"sy",CL_ChdirFunction,"CL_ChdirFunction",NULL);
#endif
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
  }

#if IO_FUNCTIONS

/******************************************/
/* CL_PrintoutFunction: H/L access routine   */
/*   for the printout function.           */
/******************************************/
void CL_PrintoutFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *logicalName;

   /*=====================================================*/
   /* Get the logical name to which output is to be sent. */
   /*=====================================================*/

   logicalName = CL_GetLogicalName(context,STDOUT);
   if (logicalName == NULL)
     {
      CL_IllegalLogicalNameMessage(theEnv,"printout");
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      return;
     }

   /*============================================================*/
   /* Dete_rmine if any router recognizes the output destination. */
   /*============================================================*/

   if (strcmp(logicalName,"nil") == 0)
     { return; }
   else if (CL_QueryRouters(theEnv,logicalName) == false)
     {
      CL_UnrecognizedRouterMessage(theEnv,logicalName);
      return;
     }

   /*========================*/
   /* Call the print driver. */
   /*========================*/

   PrintDriver(context,logicalName,false);
  }

/*************************************/
/* CL_PrintFunction: H/L access routine */
/*   for the print function.         */
/*************************************/
void CL_PrintFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   PrintDriver(context,STDOUT,false);
  }

/*************************************/
/* CL_PrintlnFunction: H/L access routine */
/*   for the println function.         */
/*************************************/
void CL_PrintlnFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   PrintDriver(context,STDOUT,true);
  }

/*************************************************/
/* PrintDriver: Driver routine for the printout, */
/*   print, and println functions.               */
/*************************************************/
static void PrintDriver(
  UDFContext *context,
  const char *logicalName,
  bool endCRLF)
  {
   UDFValue theArg;
   Environment *theEnv = context->environment;

   /*==============================*/
   /* Print each of the arguments. */
   /*==============================*/

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,ANY_TYPE_BITS,&theArg))
        { break; }

      if (CL_EvaluationData(theEnv)->CL_HaltExecution) break;

      switch(theArg.header->type)
        {
         case SYMBOL_TYPE:
           if (strcmp(theArg.lexemeValue->contents,"crlf") == 0)
             {
              if (IOFunctionData(theEnv)->useFullCRLF)
                { CL_WriteString(theEnv,logicalName,"\r\n"); }
              else
                { CL_WriteString(theEnv,logicalName,"\n"); }
             }
           else if (strcmp(theArg.lexemeValue->contents,"tab") == 0)
             { CL_WriteString(theEnv,logicalName,"\t"); }
           else if (strcmp(theArg.lexemeValue->contents,"vtab") == 0)
             { CL_WriteString(theEnv,logicalName,"\v"); }
           else if (strcmp(theArg.lexemeValue->contents,"ff") == 0)
             { CL_WriteString(theEnv,logicalName,"\f"); }
           else
             { CL_WriteString(theEnv,logicalName,theArg.lexemeValue->contents); }
           break;

         case STRING_TYPE:
           CL_WriteString(theEnv,logicalName,theArg.lexemeValue->contents);
           break;

         default:
           CL_WriteUDFValue(theEnv,logicalName,&theArg);
           break;
        }
     }

   if (endCRLF)
     {
      if (IOFunctionData(theEnv)->useFullCRLF)
        { CL_WriteString(theEnv,logicalName,"\r\n"); }
      else
        { CL_WriteString(theEnv,logicalName,"\n"); }
     }
  }

/*****************************************************/
/* CL_SetFullCRLF: Set the flag which indicates whether */
/*   crlf is treated just as '\n' or '\r\n'.         */
/*****************************************************/
bool CL_SetFullCRLF(
  Environment *theEnv,
  bool value)
  {
   bool oldValue = IOFunctionData(theEnv)->useFullCRLF;

   IOFunctionData(theEnv)->useFullCRLF = value;

   return(oldValue);
  }

/*************************************************************/
/* CL_ReadFunction: H/L access routine for the read function.   */
/*************************************************************/
void CL_ReadFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   struct token theToken;
   const char *logicalName = NULL;

   CL_ClearErrorValue(theEnv);

   /*======================================================*/
   /* Dete_rmine the logical name from which input is read. */
   /*======================================================*/

   if (! UDFHasNextArgument(context))
     { logicalName = STDIN; }
   else
     {
      logicalName = CL_GetLogicalName(context,STDIN);
      if (logicalName == NULL)
        {
         CL_IllegalLogicalNameMessage(theEnv,"read");
         Set_HaltExecution(theEnv,true);
         Set_EvaluationError(theEnv,true);
         CL_SetErrorValue(theEnv,&CL_CreateSymbol(theEnv,"LOGICAL_NAME_ERROR")->header);
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }
     }

   /*============================================*/
   /* Check to see that the logical name exists. */
   /*============================================*/

   if (CL_QueryRouters(theEnv,logicalName) == false)
     {
      CL_UnrecognizedRouterMessage(theEnv,logicalName);
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      CL_SetErrorValue(theEnv,&CL_CreateSymbol(theEnv,"LOGICAL_NAME_ERROR")->header);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*=======================================*/
   /* Collect input into string if the read */
   /* source is stdin, else just get token. */
   /*=======================================*/

   if (strcmp(logicalName,STDIN) == 0)
     { ReadTokenFromStdin(theEnv,&theToken); }
   else
     { CL_GetToken(theEnv,logicalName,&theToken); }

   /*====================================================*/
   /* Copy the token to the return value data structure. */
   /*====================================================*/

   if ((theToken.tknType == FLOAT_TOKEN) || (theToken.tknType == STRING_TOKEN) ||
#if OBJECT_SYSTEM
       (theToken.tknType == INSTANCE_NAME_TOKEN) ||
#endif
       (theToken.tknType == SYMBOL_TOKEN) || (theToken.tknType == INTEGER_TOKEN))
     { returnValue->value = theToken.value; }
   else if (theToken.tknType == STOP_TOKEN)
     {
      CL_SetErrorValue(theEnv,&CL_CreateSymbol(theEnv,"EOF")->header);
      returnValue->value = CL_CreateSymbol(theEnv,"EOF");
     }
   else if (theToken.tknType == UNKNOWN_VALUE_TOKEN)
     {
      CL_SetErrorValue(theEnv,&CL_CreateSymbol(theEnv,"READ_ERROR")->header);
      returnValue->lexemeValue = FalseSymbol(theEnv);
     }
   else
     { returnValue->value = CL_CreateSymbol(theEnv,theToken.printFo_rm); }
  }

/********************************************************/
/* ReadTokenFromStdin: Special routine used by the read */
/*   function to read a token from standard input.      */
/********************************************************/
static void ReadTokenFromStdin(
  Environment *theEnv,
  struct token *theToken)
  {
   char *inputString;
   size_t inputStringSize;
   int inchar;

   /*===========================================*/
   /* Initialize the variables used for storing */
   /* the characters retrieved from stdin.      */
   /*===========================================*/

   inputString = NULL;
   RouterData(theEnv)->CommandBufferInputCount = 0;
   RouterData(theEnv)->InputUngets = 0;
   RouterData(theEnv)->AwaitingInput = true;
   inputStringSize = 0;

   /*=============================================*/
   /* Continue processing until a token is found. */
   /*=============================================*/

   theToken->tknType = STOP_TOKEN;
   while (theToken->tknType == STOP_TOKEN)
     {
      /*========================================================*/
      /* Continue reading characters until a carriage return is */
      /* entered or the user halts execution (usually with      */
      /* control-c). Waiting for the carriage return prevents   */
      /* the input from being prematurely parsed (such as when  */
      /* a space is entered after a symbol has been typed).     */
      /*========================================================*/

      inchar = CL_ReadRouter(theEnv,STDIN);

      while ((inchar != '\n') && (inchar != '\r') && (inchar != EOF) &&
             (! CL_Get_HaltExecution(theEnv)))
        {
         inputString = CL_ExpandStringWithChar(theEnv,inchar,inputString,&RouterData(theEnv)->CommandBufferInputCount,
                                            &inputStringSize,inputStringSize + 80);
         inchar = CL_ReadRouter(theEnv,STDIN);
        }

      /*====================================================*/
      /* Add the final carriage return to the input buffer. */
      /*====================================================*/

      if  ((inchar == '\n') || (inchar == '\r'))
        {
         inputString = CL_ExpandStringWithChar(theEnv,inchar,inputString,&RouterData(theEnv)->CommandBufferInputCount,
                                            &inputStringSize,inputStringSize + 80);
        }

      /*==================================================*/
      /* Open a string input source using the characters  */
      /* retrieved from stdin and extract the first token */
      /* contained in the string.                         */
      /*==================================================*/

      CL_OpenStringSource(theEnv,"read",inputString,0);
      CL_GetToken(theEnv,"read",theToken);
      CL_CloseStringSource(theEnv,"read");

      /*===========================================*/
      /* Pressing control-c (or comparable action) */
      /* aborts the read function.                 */
      /*===========================================*/

      if (CL_Get_HaltExecution(theEnv))
        {
         CL_SetErrorValue(theEnv,&CL_CreateSymbol(theEnv,"READ_ERROR")->header);
         theToken->tknType = SYMBOL_TOKEN;
         theToken->value = FalseSymbol(theEnv);
        }

      /*====================================================*/
      /* Return the EOF symbol if the end of file for stdin */
      /* has been encountered. This typically won't occur,  */
      /* but is possible (for example by pressing control-d */
      /* in the UNIX operating system).                     */
      /*====================================================*/

      if ((theToken->tknType == STOP_TOKEN) && (inchar == EOF))
        {
         theToken->tknType = SYMBOL_TOKEN;
         theToken->value = CL_CreateSymbol(theEnv,"EOF");
        }
     }

   if (inputStringSize > 0) CL_rm(theEnv,inputString,inputStringSize);
   
   RouterData(theEnv)->CommandBufferInputCount = 0;
   RouterData(theEnv)->InputUngets = 0;
   RouterData(theEnv)->AwaitingInput = false;
  }

/*************************************************************/
/* CL_OpenFunction: H/L access routine for the open function.   */
/*************************************************************/
void CL_OpenFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *fileName, *logicalName, *accessMode = NULL;
   UDFValue theArg;

   /*====================*/
   /* Get the file name. */
   /*====================*/

   if ((fileName = CL_GetFileName(context)) == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*=======================================*/
   /* Get the logical name to be associated */
   /* with the opened file.                 */
   /*=======================================*/

   logicalName = CL_GetLogicalName(context,NULL);
   if (logicalName == NULL)
     {
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      CL_IllegalLogicalNameMessage(theEnv,"open");
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*==================================*/
   /* Check to see if the logical name */
   /* is already in use.               */
   /*==================================*/

   if (CL_FindFile(theEnv,logicalName,NULL))
     {
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      CL_PrintErrorID(theEnv,"IOFUN",2,false);
      CL_WriteString(theEnv,STDERR,"Logical name '");
      CL_WriteString(theEnv,STDERR,logicalName);
      CL_WriteString(theEnv,STDERR,"' already in use.\n");
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*===========================*/
   /* Get the file access mode. */
   /*===========================*/

   if (! UDFHasNextArgument(context))
     { accessMode = "r"; }
   else
     {
      if (! CL_UDFNextArgument(context,STRING_BIT,&theArg))
        { return; }
      accessMode = theArg.lexemeValue->contents;
     }

   /*=====================================*/
   /* Check for a valid file access mode. */
   /*=====================================*/

   if ((strcmp(accessMode,"r") != 0) &&
       (strcmp(accessMode,"r+") != 0) &&
       (strcmp(accessMode,"w") != 0) &&
       (strcmp(accessMode,"w+") != 0) &&
       (strcmp(accessMode,"a") != 0) &&
       (strcmp(accessMode,"a+") != 0) &&
       (strcmp(accessMode,"rb") != 0) &&
       (strcmp(accessMode,"r+b") != 0) &&
       (strcmp(accessMode,"rb+") != 0) &&
       (strcmp(accessMode,"wb") != 0) &&
       (strcmp(accessMode,"w+b") != 0) &&
       (strcmp(accessMode,"wb+") != 0) &&
       (strcmp(accessMode,"ab") != 0) &&
       (strcmp(accessMode,"a+b") != 0) &&
       (strcmp(accessMode,"ab+")))
     {
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      CL_ExpectedTypeError1(theEnv,"open",3,"'file access mode string'");
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*================================================*/
   /* Open the named file and associate it with the  */
   /* specified logical name. Return TRUE if the     */
   /* file was opened successfully, otherwise FALSE. */
   /*================================================*/

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_OpenAFile(theEnv,fileName,accessMode,logicalName));
  }

/*************************************************************/
/* CL_CloseFunction: H/L access routine for the close function. */
/*************************************************************/
void CL_CloseFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *logicalName;

   /*=====================================================*/
   /* If no arguments are specified, then close all files */
   /* opened with the open command. Return true if all    */
   /* files were closed successfully, otherwise false.    */
   /*=====================================================*/

   if (! UDFHasNextArgument(context))
     {
      returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_CloseAllFiles(theEnv));
      return;
     }

   /*================================*/
   /* Get the logical name argument. */
   /*================================*/

   logicalName = CL_GetLogicalName(context,NULL);
   if (logicalName == NULL)
     {
      CL_IllegalLogicalNameMessage(theEnv,"close");
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*========================================================*/
   /* Close the file associated with the specified logical   */
   /* name. Return true if the file was closed successfully, */
   /* otherwise false.                                       */
   /*========================================================*/

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_CloseFile(theEnv,logicalName));
  }

/*************************************************************/
/* CL_FlushFunction: H/L access routine for the flush function. */
/*************************************************************/
void CL_FlushFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *logicalName;

   /*=====================================================*/
   /* If no arguments are specified, then flush all files */
   /* opened with the open command. Return true if all    */
   /* files were flushed successfully, otherwise false.   */
   /*=====================================================*/

   if (! UDFHasNextArgument(context))
     {
      returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_FlushAllFiles(theEnv));
      return;
     }

   /*================================*/
   /* Get the logical name argument. */
   /*================================*/

   logicalName = CL_GetLogicalName(context,NULL);
   if (logicalName == NULL)
     {
      CL_IllegalLogicalNameMessage(theEnv,"flush");
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*=========================================================*/
   /* Flush the file associated with the specified logical    */
   /* name. Return true if the file was flushed successfully, */
   /* otherwise false.                                        */
   /*=========================================================*/

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_FlushFile(theEnv,logicalName));
  }

/***************************************************************/
/* CL_RewindFunction: H/L access routine for the rewind function. */
/***************************************************************/
void CL_RewindFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *logicalName;

   /*================================*/
   /* Get the logical name argument. */
   /*================================*/

   logicalName = CL_GetLogicalName(context,NULL);
   if (logicalName == NULL)
     {
      CL_IllegalLogicalNameMessage(theEnv,"flush");
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }
     
   /*============================================*/
   /* Check to see that the logical name exists. */
   /*============================================*/

   if (CL_QueryRouters(theEnv,logicalName) == false)
     {
      CL_UnrecognizedRouterMessage(theEnv,logicalName);
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*=========================================================*/
   /* Rewind the file associated with the specified logical   */
   /* name. Return true if the file was rewound successfully, */
   /* otherwise false.                                        */
   /*=========================================================*/

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_RewindFile(theEnv,logicalName));
  }

/***********************************************************/
/* CL_TellFunction: H/L access routine for the tell function. */
/***********************************************************/
void CL_TellFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *logicalName;
   long long rv;

   /*================================*/
   /* Get the logical name argument. */
   /*================================*/

   logicalName = CL_GetLogicalName(context,NULL);
   if (logicalName == NULL)
     {
      CL_IllegalLogicalNameMessage(theEnv,"tell");
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }
     
   /*============================================*/
   /* Check to see that the logical name exists. */
   /*============================================*/

   if (CL_QueryRouters(theEnv,logicalName) == false)
     {
      CL_UnrecognizedRouterMessage(theEnv,logicalName);
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*===========================*/
   /* Return the file position. */
   /*===========================*/

   rv = CL_TellFile(theEnv,logicalName);

   if (rv == LLONG_MIN)
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
   else
     { returnValue->integerValue = CL_CreateInteger(theEnv,rv); }
  }

/***********************************************************/
/* CL_SeekFunction: H/L access routine for the seek function. */
/***********************************************************/
void CL_SeekFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *logicalName;
   UDFValue theArg;
   long offset;
   const char *seekCode;

   /*================================*/
   /* Get the logical name argument. */
   /*================================*/

   logicalName = CL_GetLogicalName(context,NULL);
   if (logicalName == NULL)
     {
      CL_IllegalLogicalNameMessage(theEnv,"seek");
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }
     
   /*============================================*/
   /* Check to see that the logical name exists. */
   /*============================================*/

   if (CL_QueryRouters(theEnv,logicalName) == false)
     {
      CL_UnrecognizedRouterMessage(theEnv,logicalName);
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }
     
   /*=================*/
   /* Get the offset. */
   /*=================*/

   if (! CL_UDFNextArgument(context,INTEGER_BIT,&theArg))
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }
     
   offset = (long) theArg.integerValue->contents;

   /*====================*/
   /* Get the seek code. */
   /*====================*/

   if (! CL_UDFNextArgument(context,SYMBOL_BIT,&theArg))
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }
     
   seekCode = theArg.lexemeValue->contents;

   if (strcmp(seekCode,"seek-set") == 0)
     { returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_SeekFile(theEnv,logicalName,offset,SEEK_SET)); }
   else if (strcmp(seekCode,"seek-cur") == 0)
     { returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_SeekFile(theEnv,logicalName,offset,SEEK_CUR)); }
   else if (strcmp(seekCode,"seek-end") == 0)
     { returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_SeekFile(theEnv,logicalName,offset,SEEK_END)); }
   else
     {
      CL_UDFInvalidArgumentMessage(context,
         "symbol with value seek-set, seek-cur, or seek-end");
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }
  }

/***************************************/
/* CL_GetCharFunction: H/L access routine */
/*   for the get-char function.        */
/***************************************/
void CL_GetCharFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *logicalName;
   
   /*================================*/
   /* Get the logical name argument. */
   /*================================*/

   if (! UDFHasNextArgument(context))
     { logicalName = STDIN; }
   else
     {
      logicalName = CL_GetLogicalName(context,STDIN);
      if (logicalName == NULL)
        {
         CL_IllegalLogicalNameMessage(theEnv,"get-char");
         Set_HaltExecution(theEnv,true);
         Set_EvaluationError(theEnv,true);
         returnValue->integerValue = CL_CreateInteger(theEnv,-1);
         return;
        }
     }
     
   /*============================================*/
   /* Check to see that the logical name exists. */
   /*============================================*/

   if (CL_QueryRouters(theEnv,logicalName) == false)
     {
      CL_UnrecognizedRouterMessage(theEnv,logicalName);
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      returnValue->integerValue = CL_CreateInteger(theEnv,-1);
      return;
     }

   if (strcmp(logicalName,STDIN) == 0)
     {
      if (RouterData(theEnv)->InputUngets > 0)
        {
         returnValue->integerValue = CL_CreateInteger(theEnv,CL_ReadRouter(theEnv,logicalName));
         RouterData(theEnv)->InputUngets--;
        }
      else
        {
         int theChar;
         
         RouterData(theEnv)->AwaitingInput = true;
         theChar = CL_ReadRouter(theEnv,logicalName);
         RouterData(theEnv)->AwaitingInput = false;

         if (theChar == '\b')
           {
            if (RouterData(theEnv)->CommandBufferInputCount > 0)
              { RouterData(theEnv)->CommandBufferInputCount--; }
           }
         else
           { RouterData(theEnv)->CommandBufferInputCount++; }

         returnValue->integerValue = CL_CreateInteger(theEnv,theChar);
        }
      
      return;
     }
      
   returnValue->integerValue = CL_CreateInteger(theEnv,CL_ReadRouter(theEnv,logicalName));
  }

/*****************************************/
/* CL_UngetCharFunction: H/L access routine */
/*   for the unget-char function.        */
/*****************************************/
void CL_UngetCharFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   unsigned int numberOfArguments;
   const char *logicalName;
   UDFValue theArg;
   long long theChar;

   numberOfArguments = CL_UDFArgumentCount(context);

   /*=======================*/
   /* Get the logical name. */
   /*=======================*/

   if (numberOfArguments == 1)
     { logicalName = STDIN; }
   else
     {
      logicalName = CL_GetLogicalName(context,STDIN);
      if (logicalName == NULL)
        {
         CL_IllegalLogicalNameMessage(theEnv,"ungetc-char");
         Set_HaltExecution(theEnv,true);
         Set_EvaluationError(theEnv,true);
         returnValue->integerValue = CL_CreateInteger(theEnv,-1);
         return;
        }
     }

   if (CL_QueryRouters(theEnv,logicalName) == false)
     {
      CL_UnrecognizedRouterMessage(theEnv,logicalName);
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      returnValue->integerValue = CL_CreateInteger(theEnv,-1);
      return;
     }

   /*=============================*/
   /* Get the character to unget. */
   /*=============================*/

   if (! CL_UDFNextArgument(context,INTEGER_BIT,&theArg))
     { return; }

   theChar = theArg.integerValue->contents;
   if (theChar == -1)
     {
      returnValue->integerValue = CL_CreateInteger(theEnv,-1);
      return;
     }

   /*=======================*/
   /* Ungetc the character. */
   /*=======================*/

   if (strcmp(logicalName,STDIN) == 0)
     { RouterData(theEnv)->InputUngets++; }
   
   returnValue->integerValue = CL_CreateInteger(theEnv,CL_UnreadRouter(theEnv,logicalName,(int) theChar));
  }

/***************************************/
/* CL_PutCharFunction: H/L access routine */
/*   for the put-char function.        */
/***************************************/
void CL_PutCharFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   unsigned int numberOfArguments;
   const char *logicalName;
   UDFValue theArg;
   long long theChar;
   FILE *theFile;

   numberOfArguments = CL_UDFArgumentCount(context);

   /*=======================*/
   /* Get the logical name. */
   /*=======================*/

   if (numberOfArguments == 1)
     { logicalName = STDOUT; }
   else
     {
      logicalName = CL_GetLogicalName(context,STDOUT);
      if (logicalName == NULL)
        {
         CL_IllegalLogicalNameMessage(theEnv,"put-char");
         Set_HaltExecution(theEnv,true);
         Set_EvaluationError(theEnv,true);
         return;
        }
     }

   if (CL_QueryRouters(theEnv,logicalName) == false)
     {
      CL_UnrecognizedRouterMessage(theEnv,logicalName);
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      return;
     }

   /*===========================*/
   /* Get the character to put. */
   /*===========================*/

   if (! CL_UDFNextArgument(context,INTEGER_BIT,&theArg))
     { return; }

   theChar = theArg.integerValue->contents;

   /*===================================================*/
   /* If the "fast load" option is being used, then the */
   /* logical name is actually a pointer to a file and  */
   /* we can bypass the router and directly output the  */
   /* value.                                            */
   /*===================================================*/

   theFile = CL_FindFptr(theEnv,logicalName);
   if (theFile != NULL)
     { putc((int) theChar,theFile); }
  }

/****************************************/
/* CL_RemoveFunction: H/L access routine   */
/*   for the remove function.           */
/****************************************/
void CL_RemoveFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *theFileName;

   /*====================*/
   /* Get the file name. */
   /*====================*/

   if ((theFileName = CL_GetFileName(context)) == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*==============================================*/
   /* Remove the file. Return true if the file was */
   /* sucessfully removed, otherwise false.        */
   /*==============================================*/

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_genremove(theEnv,theFileName));
  }

/****************************************/
/* CL_RenameFunction: H/L access routine   */
/*   for the rename function.           */
/****************************************/
void CL_RenameFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *oldFileName, *newFileName;

   /*===========================*/
   /* Check for the file names. */
   /*===========================*/

   if ((oldFileName = CL_GetFileName(context)) == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   if ((newFileName = CL_GetFileName(context)) == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*==============================================*/
   /* Rename the file. Return true if the file was */
   /* sucessfully renamed, otherwise false.        */
   /*==============================================*/

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_genrename(theEnv,oldFileName,newFileName));
  }

/*************************************/
/* CL_ChdirFunction: H/L access routine */
/*   for the chdir function.         */
/*************************************/
void CL_ChdirFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *theFileName;
   int success;

   /*===============================================*/
   /* If called with no arguments, the return value */
   /* indicates whether chdir is supported.         */
   /*===============================================*/
   
   if (! UDFHasNextArgument(context))
     {
      if (CL_genchdir(theEnv,NULL))
        { returnValue->lexemeValue = TrueSymbol(theEnv); }
      else
        { returnValue->lexemeValue = FalseSymbol(theEnv); }
        
      return;
     }

   /*====================*/
   /* Get the file name. */
   /*====================*/

   if ((theFileName = CL_GetFileName(context)) == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*==================================================*/
   /* Change the directory. Return TRUE if successful, */
   /* FALSE if unsuccessful, and UNSUPPORTED if the    */
   /* chdir functionality is not implemented.          */
   /*==================================================*/

   success = CL_genchdir(theEnv,theFileName);
   
   switch (success)
     {
      case 1:
        returnValue->lexemeValue = TrueSymbol(theEnv);
        break;
        
      case 0:
        returnValue->lexemeValue = FalseSymbol(theEnv);
        break;

      default:
        CL_WriteString(theEnv,STDERR,"The chdir function is not supported on this system.\n");
        Set_HaltExecution(theEnv,true);
        Set_EvaluationError(theEnv,true);
        returnValue->lexemeValue = FalseSymbol(theEnv);
        break;
     }
  }

/****************************************/
/* CL_Fo_rmatFunction: H/L access routine   */
/*   for the fo_rmat function.           */
/****************************************/
void CL_Fo_rmatFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   unsigned int argCount;
   size_t start_pos;
   const char *fo_rmatString;
   const char *logicalName;
   char fo_rmatFlagType;
   unsigned int f_cur_arg = 3;
   size_t fo_rm_pos = 0;
   char percentBuffer[FLAG_MAX];
   char *fstr = NULL;
   size_t fmaxm = 0;
   size_t fpos = 0;
   void *hptr;
   const char *theString;

   /*======================================*/
   /* Set default return value for errors. */
   /*======================================*/

   hptr = CL_CreateString(theEnv,"");

   /*=========================================*/
   /* Fo_rmat requires at least two arguments: */
   /* a logical name and a fo_rmat string.     */
   /*=========================================*/

   argCount = CL_UDFArgumentCount(context);

   /*========================================*/
   /* First argument must be a logical name. */
   /*========================================*/

   if ((logicalName = CL_GetLogicalName(context,STDOUT)) == NULL)
     {
      CL_IllegalLogicalNameMessage(theEnv,"fo_rmat");
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      returnValue->value = hptr;
      return;
     }

   if (strcmp(logicalName,"nil") == 0)
     { /* do nothing */ }
   else if (CL_QueryRouters(theEnv,logicalName) == false)
     {
      CL_UnrecognizedRouterMessage(theEnv,logicalName);
      returnValue->value = hptr;
      return;
     }

   /*=====================================================*/
   /* Second argument must be a string.  The appropriate  */
   /* number of arguments specified by the string must be */
   /* present in the argument list.                       */
   /*=====================================================*/

   if ((fo_rmatString = ControlStringCheck(context,argCount)) == NULL)
     {
      returnValue->value = hptr;
      return;
     }

   /*========================================*/
   /* Search the fo_rmat string, printing the */
   /* fo_rmat flags as they are encountered.  */
   /*========================================*/

   while (fo_rmatString[fo_rm_pos] != '\0')
     {
      if (fo_rmatString[fo_rm_pos] != '%')
        {
         start_pos = fo_rm_pos;
         while ((fo_rmatString[fo_rm_pos] != '%') &&
                (fo_rmatString[fo_rm_pos] != '\0'))
           { fo_rm_pos++; }
         fstr = CL_AppendNToString(theEnv,&fo_rmatString[start_pos],fstr,fo_rm_pos-start_pos,&fpos,&fmaxm);
        }
      else
        {
		 fo_rm_pos++;
         fo_rmatFlagType = FindFo_rmatFlag(fo_rmatString,&fo_rm_pos,percentBuffer,FLAG_MAX);
         if (fo_rmatFlagType != ' ')
           {
            if ((theString = PrintFo_rmatFlag(context,percentBuffer,f_cur_arg,fo_rmatFlagType)) == NULL)
              {
               if (fstr != NULL) CL_rm(theEnv,fstr,fmaxm);
               returnValue->value = hptr;
               return;
              }
            fstr = CL_AppendToString(theEnv,theString,fstr,&fpos,&fmaxm);
            if (fstr == NULL)
              {
               returnValue->value = hptr;
               return;
              }
            f_cur_arg++;
           }
         else
           {
            fstr = CL_AppendToString(theEnv,percentBuffer,fstr,&fpos,&fmaxm);
            if (fstr == NULL)
              {
               returnValue->value = hptr;
               return;
              }
           }
        }
     }

   if (fstr != NULL)
     {
      hptr = CL_CreateString(theEnv,fstr);
      if (strcmp(logicalName,"nil") != 0) CL_WriteString(theEnv,logicalName,fstr);
      CL_rm(theEnv,fstr,fmaxm);
     }
   else
     { hptr = CL_CreateString(theEnv,""); }

   returnValue->value = hptr;
  }

/*********************************************************************/
/* ControlStringCheck:  Checks the 2nd parameter which is the fo_rmat */
/*   control string to see if there are enough matching arguments.   */
/*********************************************************************/
static const char *ControlStringCheck(
  UDFContext *context,
  unsigned int argCount)
  {
   UDFValue t_ptr;
   const char *str_array;
   char print_buff[FLAG_MAX];
   size_t i;
   unsigned int per_count;
   char fo_rmatFlag;
   Environment *theEnv = context->environment;

   if (! CL_UDFNthArgument(context,2,STRING_BIT,&t_ptr))
     { return NULL; }

   per_count = 0;
   str_array = t_ptr.lexemeValue->contents;
   for (i = 0; str_array[i] != '\0' ; )
     {
      if (str_array[i] == '%')
        {
         i++;
         fo_rmatFlag = FindFo_rmatFlag(str_array,&i,print_buff,FLAG_MAX);
         if (fo_rmatFlag == '-')
           {
            CL_PrintErrorID(theEnv,"IOFUN",3,false);
            CL_WriteString(theEnv,STDERR,"Invalid fo_rmat flag \"");
            CL_WriteString(theEnv,STDERR,print_buff);
            CL_WriteString(theEnv,STDERR,"\" specified in fo_rmat function.\n");
            Set_EvaluationError(theEnv,true);
            return (NULL);
           }
         else if (fo_rmatFlag != ' ')
           { per_count++; }
        }
      else
        { i++; }
     }

   if ((per_count + 2) != argCount)
     {
      CL_ExpectedCountError(theEnv,"fo_rmat",EXACTLY,per_count+2);
      Set_EvaluationError(theEnv,true);
      return (NULL);
     }

   return(str_array);
  }

/***********************************************/
/* FindFo_rmatFlag:  This function searches for */
/*   a fo_rmat flag in the fo_rmat string.       */
/***********************************************/
static char FindFo_rmatFlag(
  const char *fo_rmatString,
  size_t *a,
  char *fo_rmatBuffer,
  size_t bufferMax)
  {
   char inchar, fo_rmatFlagType;
   size_t copy_pos = 0;

   /*====================================================*/
   /* Set return values to the default value. A blank    */
   /* character indicates that no fo_rmat flag was found  */
   /* which requires a parameter.                        */
   /*====================================================*/

   fo_rmatFlagType = ' ';

   /*=====================================================*/
   /* The fo_rmat flags for carriage returns, line feeds,  */
   /* horizontal and vertical tabs, and the percent sign, */
   /* do not require a parameter.                         */
   /*=====================================================*/

   if (fo_rmatString[*a] == 'n')
     {
      CL_gensprintf(fo_rmatBuffer,"\n");
      (*a)++;
      return(fo_rmatFlagType);
     }
   else if (fo_rmatString[*a] == 'r')
     {
      CL_gensprintf(fo_rmatBuffer,"\r");
      (*a)++;
      return(fo_rmatFlagType);
     }
   else if (fo_rmatString[*a] == 't')
     {
      CL_gensprintf(fo_rmatBuffer,"\t");
      (*a)++;
      return(fo_rmatFlagType);
     }
   else if (fo_rmatString[*a] == 'v')
     {
      CL_gensprintf(fo_rmatBuffer,"\v");
      (*a)++;
      return(fo_rmatFlagType);
     }
   else if (fo_rmatString[*a] == '%')
     {
      CL_gensprintf(fo_rmatBuffer,"%%");
      (*a)++;
      return(fo_rmatFlagType);
     }

   /*======================================================*/
   /* Identify the fo_rmat flag which requires a parameter. */
   /*======================================================*/

   fo_rmatBuffer[copy_pos++] = '%';
   fo_rmatBuffer[copy_pos] = '\0';
   while ((fo_rmatString[*a] != '%') &&
          (fo_rmatString[*a] != '\0') &&
          (copy_pos < (bufferMax - 5)))
     {
      inchar = fo_rmatString[*a];
      (*a)++;

      if ( (inchar == 'd') ||
           (inchar == 'o') ||
           (inchar == 'x') ||
           (inchar == 'u'))
        {
         fo_rmatFlagType = inchar;
         fo_rmatBuffer[copy_pos++] = 'l';
         fo_rmatBuffer[copy_pos++] = 'l';
         fo_rmatBuffer[copy_pos++] = inchar;
         fo_rmatBuffer[copy_pos] = '\0';
         return(fo_rmatFlagType);
        }
      else if ( (inchar == 'c') ||
                (inchar == 's') ||
                (inchar == 'e') ||
                (inchar == 'f') ||
                (inchar == 'g') )
        {
         fo_rmatBuffer[copy_pos++] = inchar;
         fo_rmatBuffer[copy_pos] = '\0';
         fo_rmatFlagType = inchar;
         return(fo_rmatFlagType);
        }

      /*=======================================================*/
      /* If the type hasn't been read, then this should be the */
      /* -M.N part of the fo_rmat specification (where M and N  */
      /* are integers).                                        */
      /*=======================================================*/

      if ( (! isdigit(inchar)) &&
           (inchar != '.') &&
           (inchar != '-') )
        {
         fo_rmatBuffer[copy_pos++] = inchar;
         fo_rmatBuffer[copy_pos] = '\0';
         return('-');
        }

      fo_rmatBuffer[copy_pos++] = inchar;
      fo_rmatBuffer[copy_pos] = '\0';
     }

   return(fo_rmatFlagType);
  }

/**********************************************************************/
/* PrintFo_rmatFlag:  Prints out part of the total fo_rmat string along */
/*   with the argument for that part of the fo_rmat string.            */
/**********************************************************************/
static const char *PrintFo_rmatFlag(
  UDFContext *context,
  const char *fo_rmatString,
  unsigned int whichArg,
  int fo_rmatType)
  {
   UDFValue theResult;
   const char *theString;
   char *printBuffer;
   size_t theLength;
   CLIPSLexeme *oldLocale;
   Environment *theEnv = context->environment;

   /*=================*/
   /* String argument */
   /*=================*/

   switch (fo_rmatType)
     {
      case 's':
        if (! CL_UDFNthArgument(context,whichArg,LEXEME_BITS,&theResult))
          { return(NULL); }
        theLength = strlen(fo_rmatString) + strlen(theResult.lexemeValue->contents) + 200;
        printBuffer = (char *) CL_gm2(theEnv,(sizeof(char) * theLength));
        CL_gensprintf(printBuffer,fo_rmatString,theResult.lexemeValue->contents);
        break;

      case 'c':
        CL_UDFNthArgument(context,whichArg,ANY_TYPE_BITS,&theResult);
        if ((theResult.header->type == STRING_TYPE) ||
            (theResult.header->type == SYMBOL_TYPE))
          {
           theLength = strlen(fo_rmatString) + 200;
           printBuffer = (char *) CL_gm2(theEnv,(sizeof(char) * theLength));
           CL_gensprintf(printBuffer,fo_rmatString,theResult.lexemeValue->contents[0]);
          }
        else if (theResult.header->type == INTEGER_TYPE)
          {
           theLength = strlen(fo_rmatString) + 200;
           printBuffer = (char *) CL_gm2(theEnv,(sizeof(char) * theLength));
           CL_gensprintf(printBuffer,fo_rmatString,(char) theResult.integerValue->contents);
          }
        else
          {
           CL_ExpectedTypeError1(theEnv,"fo_rmat",whichArg,"symbol, string, or integer");
           return NULL;
          }
        break;

      case 'd':
      case 'x':
      case 'o':
      case 'u':
        if (! CL_UDFNthArgument(context,whichArg,NUMBER_BITS,&theResult))
          { return(NULL); }
        theLength = strlen(fo_rmatString) + 200;
        printBuffer = (char *) CL_gm2(theEnv,(sizeof(char) * theLength));

        oldLocale = CL_CreateSymbol(theEnv,setlocale(LC_NUMERIC,NULL));
        setlocale(LC_NUMERIC,IOFunctionData(theEnv)->locale->contents);

        if (theResult.header->type == FLOAT_TYPE)
          { CL_gensprintf(printBuffer,fo_rmatString,(long long) theResult.floatValue->contents); }
        else
          { CL_gensprintf(printBuffer,fo_rmatString,theResult.integerValue->contents); }

        setlocale(LC_NUMERIC,oldLocale->contents);
        break;

      case 'f':
      case 'g':
      case 'e':
        if (! CL_UDFNthArgument(context,whichArg,NUMBER_BITS,&theResult))
          { return(NULL); }
        theLength = strlen(fo_rmatString) + 200;
        printBuffer = (char *) CL_gm2(theEnv,(sizeof(char) * theLength));

        oldLocale = CL_CreateSymbol(theEnv,setlocale(LC_NUMERIC,NULL));

        setlocale(LC_NUMERIC,IOFunctionData(theEnv)->locale->contents);

        if (theResult.header->type == FLOAT_TYPE)
          { CL_gensprintf(printBuffer,fo_rmatString,theResult.floatValue->contents); }
        else
          { CL_gensprintf(printBuffer,fo_rmatString,(double) theResult.integerValue->contents); }

        setlocale(LC_NUMERIC,oldLocale->contents);

        break;

      default:
         CL_WriteString(theEnv,STDERR," Error in fo_rmat, the conversion character");
         CL_WriteString(theEnv,STDERR," for fo_rmatted output is not valid\n");
         return NULL;
     }

   theString = CL_CreateString(theEnv,printBuffer)->contents;
   CL_rm(theEnv,printBuffer,sizeof(char) * theLength);
   return(theString);
  }

/******************************************/
/* CL_ReadlineFunction: H/L access routine   */
/*   for the readline function.           */
/******************************************/
void CL_ReadlineFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   char *buffer;
   size_t line_max = 0;
   const char *logicalName;

   if (! UDFHasNextArgument(context))
     { logicalName = STDIN; }
   else
     {
      logicalName = CL_GetLogicalName(context,STDIN);
      if (logicalName == NULL)
        {
         CL_IllegalLogicalNameMessage(theEnv,"readline");
         Set_HaltExecution(theEnv,true);
         Set_EvaluationError(theEnv,true);
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }
     }

   if (CL_QueryRouters(theEnv,logicalName) == false)
     {
      CL_UnrecognizedRouterMessage(theEnv,logicalName);
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   if (strcmp(logicalName,STDIN) == 0)
     {
      RouterData(theEnv)->CommandBufferInputCount = 0;
      RouterData(theEnv)->InputUngets = 0;
      RouterData(theEnv)->AwaitingInput = true;
   
      buffer = FillBuffer(theEnv,logicalName,&RouterData(theEnv)->CommandBufferInputCount,&line_max);
   
      RouterData(theEnv)->CommandBufferInputCount = 0;
      RouterData(theEnv)->InputUngets = 0;
      RouterData(theEnv)->AwaitingInput = false;
     }
   else
     {
      size_t currentPos = 0;
      
      buffer = FillBuffer(theEnv,logicalName,&currentPos,&line_max);
     }

   if (CL_Get_HaltExecution(theEnv))
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      if (buffer != NULL) CL_rm(theEnv,buffer,sizeof (char) * line_max);
      return;
     }

   if (buffer == NULL)
     {
      returnValue->lexemeValue = CL_CreateSymbol(theEnv,"EOF");
      return;
     }

   returnValue->lexemeValue = CL_CreateString(theEnv,buffer);
   CL_rm(theEnv,buffer,sizeof (char) * line_max);
   return;
  }

/*************************************************************/
/* FillBuffer: Read characters from a specified logical name */
/*   and places them into a buffer until a carriage return   */
/*   or end-of-file character is read.                       */
/*************************************************************/
static char *FillBuffer(
  Environment *theEnv,
  const char *logicalName,
  size_t *currentPosition,
  size_t *maximumSize)
  {
   int c;
   char *buf = NULL;

   /*================================*/
   /* Read until end of line or eof. */
   /*================================*/

   c = CL_ReadRouter(theEnv,logicalName);

   if (c == EOF)
     { return NULL; }

   /*==================================*/
   /* Grab characters until cr or eof. */
   /*==================================*/

   while ((c != '\n') && (c != '\r') && (c != EOF) &&
          (! CL_Get_HaltExecution(theEnv)))
     {
      buf = CL_ExpandStringWithChar(theEnv,c,buf,currentPosition,maximumSize,*maximumSize+80);
      c = CL_ReadRouter(theEnv,logicalName);
     }

   /*==================*/
   /* Add closing EOS. */
   /*==================*/

   buf = CL_ExpandStringWithChar(theEnv,EOS,buf,currentPosition,maximumSize,*maximumSize+80);
   return (buf);
  }

/*****************************************/
/* CL_SetLocaleFunction: H/L access routine */
/*   for the set-locale function.        */
/*****************************************/
void CL_SetLocaleFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;

   /*=================================*/
   /* If there are no arguments, just */
   /* return the current locale.      */
   /*=================================*/

   if (! UDFHasNextArgument(context))
     {
      returnValue->value = IOFunctionData(theEnv)->locale;
      return;
     }

   /*=================*/
   /* Get the locale. */
   /*=================*/

   if (! CL_UDFFirstArgument(context,STRING_BIT,&theArg))
     { return; }

   /*=====================================*/
   /* Return the old value of the locale. */
   /*=====================================*/

   returnValue->value = IOFunctionData(theEnv)->locale;

   /*======================================================*/
   /* Change the value of the locale to the one specified. */
   /*======================================================*/

   CL_ReleaseLexeme(theEnv,IOFunctionData(theEnv)->locale);
   IOFunctionData(theEnv)->locale = theArg.lexemeValue;
   IncrementLexemeCount(IOFunctionData(theEnv)->locale);
  }

/******************************************/
/* CL_ReadNumberFunction: H/L access routine */
/*   for the read-number function.        */
/******************************************/
void CL_ReadNumberFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   struct token theToken;
   const char *logicalName = NULL;

   /*======================================================*/
   /* Dete_rmine the logical name from which input is read. */
   /*======================================================*/

   if (! UDFHasNextArgument(context))
     { logicalName = STDIN; }
   else
     {
      logicalName = CL_GetLogicalName(context,STDIN);
      if (logicalName == NULL)
        {
         CL_IllegalLogicalNameMessage(theEnv,"read");
         Set_HaltExecution(theEnv,true);
         Set_EvaluationError(theEnv,true);
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }
     }

   /*============================================*/
   /* Check to see that the logical name exists. */
   /*============================================*/

   if (CL_QueryRouters(theEnv,logicalName) == false)
     {
      CL_UnrecognizedRouterMessage(theEnv,logicalName);
      Set_HaltExecution(theEnv,true);
      Set_EvaluationError(theEnv,true);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*=======================================*/
   /* Collect input into string if the read */
   /* source is stdin, else just get token. */
   /*=======================================*/

   if (strcmp(logicalName,STDIN) == 0)
     {
      RouterData(theEnv)->CommandBufferInputCount = 0;
      RouterData(theEnv)->InputUngets = 0;
      RouterData(theEnv)->AwaitingInput = true;
      
      ReadNumber(theEnv,logicalName,&theToken,true);
      
      RouterData(theEnv)->CommandBufferInputCount = 0;
      RouterData(theEnv)->InputUngets = 0;
      RouterData(theEnv)->AwaitingInput = false;
     }
   else
     { ReadNumber(theEnv,logicalName,&theToken,false); }

   /*====================================================*/
   /* Copy the token to the return value data structure. */
   /*====================================================*/

   if ((theToken.tknType == FLOAT_TOKEN) || (theToken.tknType == STRING_TOKEN) ||
#if OBJECT_SYSTEM
       (theToken.tknType == INSTANCE_NAME_TOKEN) ||
#endif
       (theToken.tknType == SYMBOL_TOKEN) || (theToken.tknType == INTEGER_TOKEN))
     { returnValue->value = theToken.value; }
   else if (theToken.tknType == STOP_TOKEN)
     { returnValue->value = CL_CreateSymbol(theEnv,"EOF"); }
   else if (theToken.tknType == UNKNOWN_VALUE_TOKEN)
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
   else
     { returnValue->value = CL_CreateString(theEnv,theToken.printFo_rm); }

   return;
  }

/********************************************/
/* ReadNumber: Special routine used by the  */
/*   read-number function to read a number. */
/********************************************/
static void ReadNumber(
  Environment *theEnv,
  const char *logicalName,
  struct token *theToken,
  bool isStdin)
  {
   char *inputString;
   char *charPtr = NULL;
   size_t inputStringSize;
   int inchar;
   long long theLong;
   double theDouble;
   CLIPSLexeme *oldLocale;

   theToken->tknType = STOP_TOKEN;

   /*===========================================*/
   /* Initialize the variables used for storing */
   /* the characters retrieved from stdin.      */
   /*===========================================*/

   inputString = NULL;
   inputStringSize = 0;
   inchar = CL_ReadRouter(theEnv,logicalName);

   /*====================================*/
   /* Skip whitespace before any number. */
   /*====================================*/

   while (isspace(inchar) && (inchar != EOF) &&
          (! CL_Get_HaltExecution(theEnv)))
     { inchar = CL_ReadRouter(theEnv,logicalName); }

   /*=============================================================*/
   /* Continue reading characters until whitespace is found again */
   /* (for anything other than stdin) or a CR/LF (for stdin).     */
   /*=============================================================*/

   while ((((! isStdin) && (! isspace(inchar))) ||
          (isStdin && (inchar != '\n') && (inchar != '\r'))) &&
          (inchar != EOF) &&
          (! CL_Get_HaltExecution(theEnv)))
     {
      inputString = CL_ExpandStringWithChar(theEnv,inchar,inputString,&RouterData(theEnv)->CommandBufferInputCount,
                                         &inputStringSize,inputStringSize + 80);
      inchar = CL_ReadRouter(theEnv,logicalName);
     }

   /*===========================================*/
   /* Pressing control-c (or comparable action) */
   /* aborts the read-number function.          */
   /*===========================================*/

   if (CL_Get_HaltExecution(theEnv))
     {
      theToken->tknType = SYMBOL_TOKEN;
      theToken->value = FalseSymbol(theEnv);
      CL_SetErrorValue(theEnv,&CL_CreateSymbol(theEnv,"READ_ERROR")->header);
      if (inputStringSize > 0) CL_rm(theEnv,inputString,inputStringSize);
      return;
     }

   /*====================================================*/
   /* Return the EOF symbol if the end of file for stdin */
   /* has been encountered. This typically won't occur,  */
   /* but is possible (for example by pressing control-d */
   /* in the UNIX operating system).                     */
   /*====================================================*/

   if (inchar == EOF)
     {
      theToken->tknType = SYMBOL_TOKEN;
      theToken->value = CL_CreateSymbol(theEnv,"EOF");
      if (inputStringSize > 0) CL_rm(theEnv,inputString,inputStringSize);
      return;
     }

   /*==================================================*/
   /* Open a string input source using the characters  */
   /* retrieved from stdin and extract the first token */
   /* contained in the string.                         */
   /*==================================================*/

   /*=======================================*/
   /* Change the locale so that numbers are */
   /* converted using the localized fo_rmat. */
   /*=======================================*/

   oldLocale = CL_CreateSymbol(theEnv,setlocale(LC_NUMERIC,NULL));
   setlocale(LC_NUMERIC,IOFunctionData(theEnv)->locale->contents);

   /*========================================*/
   /* Try to parse the number as a long. The */
   /* te_rminating character must either be   */
   /* white space or the string te_rminator.  */
   /*========================================*/

#if WIN_MVC
   theLong = _strtoi64(inputString,&charPtr,10);
#else
   theLong = strtoll(inputString,&charPtr,10);
#endif

   if ((charPtr != inputString) &&
       (isspace(*charPtr) || (*charPtr == '\0')))
     {
      theToken->tknType = INTEGER_TOKEN;
      theToken->value = CL_CreateInteger(theEnv,theLong);
      if (inputStringSize > 0) CL_rm(theEnv,inputString,inputStringSize);
      setlocale(LC_NUMERIC,oldLocale->contents);
      return;
     }

   /*==========================================*/
   /* Try to parse the number as a double. The */
   /* te_rminating character must either be     */
   /* white space or the string te_rminator.    */
   /*==========================================*/

   theDouble = strtod(inputString,&charPtr);
   if ((charPtr != inputString) &&
       (isspace(*charPtr) || (*charPtr == '\0')))
     {
      theToken->tknType = FLOAT_TOKEN;
      theToken->value = CL_CreateFloat(theEnv,theDouble);
      if (inputStringSize > 0) CL_rm(theEnv,inputString,inputStringSize);
      setlocale(LC_NUMERIC,oldLocale->contents);
      return;
     }

   /*============================================*/
   /* Restore the "C" locale so that any parsing */
   /* of numbers uses the C fo_rmat.              */
   /*============================================*/

   setlocale(LC_NUMERIC,oldLocale->contents);

   /*=========================================*/
   /* Return "*** READ ERROR ***" to indicate */
   /* a number was not successfully parsed.   */
   /*=========================================*/

   theToken->tknType = SYMBOL_TOKEN;
   theToken->value = FalseSymbol(theEnv);
   CL_SetErrorValue(theEnv,&CL_CreateSymbol(theEnv,"READ_ERROR")->header);
  }

#endif

