   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  11/13/17             */
   /*                                                     */
   /*                COMMAND LINE MODULE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides a set of routines for processing        */
/*   commands entered at the top level prompt.               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Refactored several functions and added         */
/*            additional functions for use by an interface   */
/*            layered on top of CLIPS.                       */
/*                                                           */
/*      6.30: Local variables set with the bind function     */
/*            persist until a reset/clear command is issued. */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Metrowerks CodeWarrior (MAC_MCW, IBM_MCW) is   */
/*            no longer supported.                           */
/*                                                           */
/*            UTF-8 support.                                 */
/*                                                           */
/*            Command history and editing support            */
/*                                                           */
/*            Used CL_genstrcpy instead of strcpy.              */
/*                                                           */
/*            Added before command execution callback        */
/*            function.                                      */
/*                                                           */
/*            Fixed CL_RouteCommand return value.               */
/*                                                           */
/*            Added AwaitingInput flag.                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Added code to keep track of pointers to        */
/*            constructs that are contained externally to    */
/*            to constructs, DanglingConstructs.             */
/*                                                           */
/*            Added STDOUT and STDIN logical name            */
/*            definitions.                                   */
/*                                                           */
/*      6.40: Added call to CL_FlushParsingMessages to clear    */
/*            message buffer after each command.             */
/*                                                           */
/*            Added Env prefix to Get_EvaluationError and     */
/*            Set_EvaluationError functions.                  */
/*                                                           */
/*            Added Env prefix to CL_Get_HaltExecution and       */
/*            Set_HaltExecution functions.                    */
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
/*            UDF redesign.                                  */
/*                                                           */
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*            Removed fflush of stdin.                       */
/*                                                           */
/*            Use of ?<var>, $?<var>, ?*<var>, and  $?*var*  */
/*            by itself at the command prompt and within     */
/*            the eval function now consistently returns the */
/*            value of  the variable.                        */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "setup.h"
#include "constant.h"

#include "argacces.h"
#include "constrct.h"
#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "fileutil.h"
#include "memalloc.h"
#include "multifld.h"
#include "pprint.h"
#include "prcdrfun.h"
#include "prcdrpsr.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"
#include "strngrtr.h"
#include "symbol.h"
#include "sysdep.h"
#include "utility.h"

#include "commline.h"

/***************/
/* DEFINITIONS */
/***************/

#define NO_SWITCH         0
#define BATCH_SWITCH      1
#define BATCH_STAR_SWITCH 2
#define LOAD_SWITCH       3

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if ! RUN_TIME
   static int                     DoString(const char *,int,bool *);
   static int                     DoComment(const char *,int);
   static int                     DoWhiteSpace(const char *,int);
   static void                    DefaultGetNextEvent(Environment *);
#endif
   static void                    DeallocateCommandLineData(Environment *);

/****************************************************/
/* CL_InitializeCommandLineData: Allocates environment */
/*    data for command line functionality.          */
/****************************************************/
void CL_InitializeCommandLineData(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,COMMANDLINE_DATA,sizeof(struct commandLineData),DeallocateCommandLineData);

#if ! RUN_TIME
   CommandLineData(theEnv)->BannerString = BANNER_STRING;
   CommandLineData(theEnv)->EventCallback = DefaultGetNextEvent;
#endif
  }

/*******************************************************/
/* DeallocateCommandLineData: Deallocates environment */
/*    data for the command line functionality.        */
/******************************************************/
static void DeallocateCommandLineData(
  Environment *theEnv)
  {
#if ! RUN_TIME
   if (CommandLineData(theEnv)->CommandString != NULL)
     { CL_rm(theEnv,CommandLineData(theEnv)->CommandString,CommandLineData(theEnv)->MaximumCharacters); }

   if (CommandLineData(theEnv)->CurrentCommand != NULL)
     { CL_ReturnExpression(theEnv,CommandLineData(theEnv)->CurrentCommand); }
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
  }

/*************************************************/
/* CL_RerouteStdin: Processes the -f, -f2, and -l   */
/*   options available on machines which support */
/*   argc and arv command line options.          */
/*************************************************/
void CL_RerouteStdin(
  Environment *theEnv,
  int argc,
  char *argv[])
  {
   int i;
   int theSwitch = NO_SWITCH;

   /*======================================*/
   /* If there aren't enough arguments for */
   /* the -f argument, then return.        */
   /*======================================*/

   if (argc < 3)
     { return; }

   /*=====================================*/
   /* If argv was not passed then return. */
   /*=====================================*/

   if (argv == NULL) return;

   /*=============================================*/
   /* Process each of the command line arguments. */
   /*=============================================*/

   for (i = 1 ; i < argc ; i++)
     {
      if (strcmp(argv[i],"-f") == 0) theSwitch = BATCH_SWITCH;
#if ! RUN_TIME
      else if (strcmp(argv[i],"-f2") == 0) theSwitch = BATCH_STAR_SWITCH;
      else if (strcmp(argv[i],"-l") == 0) theSwitch = LOAD_SWITCH;
#endif
      else if (theSwitch == NO_SWITCH)
        {
         CL_PrintErrorID(theEnv,"SYSDEP",2,false);
         CL_WriteString(theEnv,STDERR,"Invalid option '");
         CL_WriteString(theEnv,STDERR,argv[i]);
         CL_WriteString(theEnv,STDERR,"'.\n");
        }

      if (i > (argc-1))
        {
         CL_PrintErrorID(theEnv,"SYSDEP",1,false);
         CL_WriteString(theEnv,STDERR,"No file found for '");

         switch(theSwitch)
           {
            case BATCH_SWITCH:
               CL_WriteString(theEnv,STDERR,"-f");
               break;

            case BATCH_STAR_SWITCH:
               CL_WriteString(theEnv,STDERR,"-f2");
               break;

            case LOAD_SWITCH:
               CL_WriteString(theEnv,STDERR,"-l");
           }

         CL_WriteString(theEnv,STDERR,"' option.\n");
         return;
        }

      switch(theSwitch)
        {
         case BATCH_SWITCH:
            Open_Batch(theEnv,argv[++i],true);
            break;

#if (! RUN_TIME) && (! BLOAD_ONLY)
         case BATCH_STAR_SWITCH:
            CL_BatchStar(theEnv,argv[++i]);
            break;

         case LOAD_SWITCH:
            CL_Load(theEnv,argv[++i]);
            break;
#endif
        }
     }
  }

#if ! RUN_TIME

/***************************************************/
/* CL_ExpandCommandString: Appends a character to the */
/*   command string. Returns true if the command   */
/*   string was successfully expanded, otherwise   */
/*   false. Expanding the string also includes     */
/*   adding a backspace character which reduces    */
/*   string's length.                              */
/***************************************************/
bool CL_ExpandCommandString(
  Environment *theEnv,
  int inchar)
  {
   size_t k;

   k = RouterData(theEnv)->CommandBufferInputCount;
   CommandLineData(theEnv)->CommandString = CL_ExpandStringWithChar(theEnv,inchar,CommandLineData(theEnv)->CommandString,&RouterData(theEnv)->CommandBufferInputCount,
                                        &CommandLineData(theEnv)->MaximumCharacters,CommandLineData(theEnv)->MaximumCharacters+80);
   return((RouterData(theEnv)->CommandBufferInputCount != k) ? true : false);
  }

/******************************************************************/
/* CL_FlushCommandString: Empties the contents of the CommandString. */
/******************************************************************/
void CL_FlushCommandString(
  Environment *theEnv)
  {
   if (CommandLineData(theEnv)->CommandString != NULL) CL_rm(theEnv,CommandLineData(theEnv)->CommandString,CommandLineData(theEnv)->MaximumCharacters);
   CommandLineData(theEnv)->CommandString = NULL;
   CommandLineData(theEnv)->MaximumCharacters = 0;
   RouterData(theEnv)->CommandBufferInputCount = 0;
   RouterData(theEnv)->InputUngets = 0;
   RouterData(theEnv)->AwaitingInput = true;
  }

/*********************************************************************************/
/* CL_SetCommandString: Sets the contents of the CommandString to a specific value. */
/*********************************************************************************/
void CL_SetCommandString(
  Environment *theEnv,
  const char *str)
  {
   size_t length;

   CL_FlushCommandString(theEnv);
   length = strlen(str);
   CommandLineData(theEnv)->CommandString = (char *)
                   CL_genrealloc(theEnv,CommandLineData(theEnv)->CommandString,
                              CommandLineData(theEnv)->MaximumCharacters,
                              CommandLineData(theEnv)->MaximumCharacters + length + 1);

   CL_genstrcpy(CommandLineData(theEnv)->CommandString,str);
   CommandLineData(theEnv)->MaximumCharacters += (length + 1);
   RouterData(theEnv)->CommandBufferInputCount += length;
  }

/*************************************************************/
/* CL_SetNCommandString: Sets the contents of the CommandString */
/*   to a specific value up to N characters.                 */
/*************************************************************/
void CL_SetNCommandString(
  Environment *theEnv,
  const char *str,
  unsigned length)
  {
   CL_FlushCommandString(theEnv);
   CommandLineData(theEnv)->CommandString = (char *)
                   CL_genrealloc(theEnv,CommandLineData(theEnv)->CommandString,
                              CommandLineData(theEnv)->MaximumCharacters,
                              CommandLineData(theEnv)->MaximumCharacters + length + 1);

   CL_genstrncpy(CommandLineData(theEnv)->CommandString,str,length);
   CommandLineData(theEnv)->CommandString[CommandLineData(theEnv)->MaximumCharacters + length] = 0;
   CommandLineData(theEnv)->MaximumCharacters += (length + 1);
   RouterData(theEnv)->CommandBufferInputCount += length;
  }

/******************************************************************************/
/* CL_AppendCommandString: Appends a value to the contents of the CommandString. */
/******************************************************************************/
void CL_AppendCommandString(
  Environment *theEnv,
  const char *str)
  {
   CommandLineData(theEnv)->CommandString = CL_AppendToString(theEnv,str,CommandLineData(theEnv)->CommandString,&RouterData(theEnv)->CommandBufferInputCount,&CommandLineData(theEnv)->MaximumCharacters);
  }

/******************************************************************************/
/* CL_InsertCommandString: Inserts a value in the contents of the CommandString. */
/******************************************************************************/
void CL_InsertCommandString(
  Environment *theEnv,
  const char *str,
  unsigned int position)
  {
   CommandLineData(theEnv)->CommandString =
      CL_InsertInString(theEnv,str,position,CommandLineData(theEnv)->CommandString,
                     &RouterData(theEnv)->CommandBufferInputCount,&CommandLineData(theEnv)->MaximumCharacters);
  }

/************************************************************/
/* CL_AppendNCommandString: Appends a value up to N characters */
/*   to the contents of the CommandString.                  */
/************************************************************/
void CL_AppendNCommandString(
  Environment *theEnv,
  const char *str,
  unsigned length)
  {
   CommandLineData(theEnv)->CommandString = CL_AppendNToString(theEnv,str,CommandLineData(theEnv)->CommandString,length,&RouterData(theEnv)->CommandBufferInputCount,&CommandLineData(theEnv)->MaximumCharacters);
  }

/*****************************************************************************/
/* CL_GetCommandString: Returns a pointer to the contents of the CommandString. */
/*****************************************************************************/
char *CL_GetCommandString(
  Environment *theEnv)
  {
   return(CommandLineData(theEnv)->CommandString);
  }

/**************************************************************************/
/* CL_CompleteCommand: Dete_rmines whether a string fo_rms a complete command. */
/*   A complete command is either a constant, a variable, or a function   */
/*   call which is followed (at some point) by a carriage return. Once a  */
/*   complete command is found (not including the parenthesis),           */
/*   extraneous parenthesis and other tokens are ignored. If a complete   */
/*   command exists, then 1 is returned. 0 is returned if the command was */
/*   not complete and without errors. -1 is returned if the command       */
/*   contains an error.                                                   */
/**************************************************************************/
int CL_CompleteCommand(
  const char *mstring)
  {
   int i;
   char inchar;
   int depth = 0;
   bool moreThanZero = false;
   bool complete;
   bool error = false;

   if (mstring == NULL) return 0;

   /*===================================================*/
   /* Loop through each character of the command string */
   /* to dete_rmine if there is a complete command.      */
   /*===================================================*/

   i = 0;
   while ((inchar = mstring[i++]) != EOS)
     {
      switch(inchar)
        {
         /*======================================================*/
         /* If a carriage return or line feed is found, there is */
         /* at least one completed token in the command buffer,  */
         /* and parentheses are balanced, then a complete        */
         /* command has been found. Otherwise, remove all white  */
         /* space beginning with the current character.          */
         /*======================================================*/

         case '\n' :
         case '\r' :
           if (error) return(-1);
           if (moreThanZero && (depth == 0)) return 1;
           i = DoWhiteSpace(mstring,i);
           break;

         /*=====================*/
         /* Remove white space. */
         /*=====================*/

         case ' ' :
         case '\f' :
         case '\t' :
           i = DoWhiteSpace(mstring,i);
           break;

         /*======================================================*/
         /* If the opening quotation of a string is encountered, */
         /* dete_rmine if the closing quotation of the string is  */
         /* in the command buffer. Until the closing quotation   */
         /* is found, a complete command can not be made.        */
         /*======================================================*/

         case '"' :
           i = DoString(mstring,i,&complete);
           if ((depth == 0) && complete) moreThanZero = true;
           break;

         /*====================*/
         /* Process a comment. */
         /*====================*/

         case ';' :
           i = DoComment(mstring,i);
           if (moreThanZero && (depth == 0) && (mstring[i] != EOS))
             {
              if (error) return -1;
              else return 1;
             }
           else if (mstring[i] != EOS) i++;
           break;

         /*====================================================*/
         /* A left parenthesis increases the nesting depth of  */
         /* the current command by 1. Don't bother to increase */
         /* the depth if the first token encountered was not   */
         /* a parenthesis (e.g. for the command string         */
         /* "red (+ 3 4", the symbol red already fo_rms a       */
         /* complete command, so the next carriage return will */
         /* cause evaluation of red--the closing parenthesis   */
         /* for "(+ 3 4" does not have to be found).           */
         /*====================================================*/

         case '(' :
           if ((depth > 0) || (moreThanZero == false))
             {
              depth++;
              moreThanZero = true;
             }
           break;

         /*====================================================*/
         /* A right parenthesis decreases the nesting depth of */
         /* the current command by 1. If the parenthesis is    */
         /* the first token of the command, then an error is   */
         /* generated.                                         */
         /*====================================================*/

         case ')' :
           if (depth > 0) depth--;
           else if (moreThanZero == false) error = true;
           break;

         /*=====================================================*/
         /* If the command begins with any other character and  */
         /* an opening parenthesis hasn't yet been found, then  */
         /* skip all characters on the same line. If a carriage */
         /* return or line feed is found, then a complete       */
         /* command exists.                                     */
         /*=====================================================*/

         default:
           if (depth == 0)
             {
              if (IsUTF8MultiByteStart(inchar) || isprint(inchar))
                {
                 while ((inchar = mstring[i++]) != EOS)
                   {
                    if ((inchar == '\n') || (inchar == '\r'))
                      {
                       if (error) return -1;
                       else return 1;
                      }
                   }
                 return 0;
                }
             }
           break;
        }
     }

   /*====================================================*/
   /* Return 0 because a complete command was not found. */
   /*====================================================*/

   return 0;
  }

/***********************************************************/
/* DoString: Skips over a string contained within a string */
/*   until the closing quotation mark is encountered.      */
/***********************************************************/
static int DoString(
  const char *str,
  int pos,
  bool *complete)
  {
   int inchar;

   /*=================================================*/
   /* Process the string character by character until */
   /* the closing quotation mark is found.            */
   /*=================================================*/

   inchar = str[pos];
   while (inchar  != '"')
     {
      /*=====================================================*/
      /* If a \ is found, then the next character is ignored */
      /* even if it is a closing quotation mark.             */
      /*=====================================================*/

      if (inchar == '\\')
        {
         pos++;
         inchar = str[pos];
        }

      /*===================================================*/
      /* If the end of input is reached before the closing */
      /* quotation mark is found, the return the last      */
      /* position that was reached and indicate that a     */
      /* complete string was not found.                    */
      /*===================================================*/

      if (inchar == EOS)
        {
         *complete = false;
         return(pos);
        }

      /*================================*/
      /* Move on to the next character. */
      /*================================*/

      pos++;
      inchar = str[pos];
     }

   /*======================================================*/
   /* Indicate that a complete string was found and return */
   /* the position of the closing quotation mark.          */
   /*======================================================*/

   pos++;
   *complete = true;
   return(pos);
  }

/*************************************************************/
/* DoComment: Skips over a comment contained within a string */
/*   until a line feed or carriage return is encountered.    */
/*************************************************************/
static int DoComment(
  const char *str,
  int pos)
  {
   int inchar;

   inchar = str[pos];
   while ((inchar != '\n') && (inchar != '\r'))
     {
      if (inchar == EOS)
        { return(pos); }

      pos++;
      inchar = str[pos];
     }

   return(pos);
  }

/**************************************************************/
/* DoWhiteSpace: Skips over white space consisting of spaces, */
/*   tabs, and fo_rm feeds that is contained within a string.  */
/**************************************************************/
static int DoWhiteSpace(
  const char *str,
  int pos)
  {
   int inchar;

   inchar = str[pos];
   while ((inchar == ' ') || (inchar == '\f') || (inchar == '\t'))
     {
      pos++;
      inchar = str[pos];
     }

   return(pos);
  }

/********************************************************************/
/* CL_CommandLoop: Endless loop which waits for user commands and then */
/*   executes them. The command loop will bypass the EventFunction  */
/*   if there is an active batch file.                              */
/********************************************************************/
void CL_CommandLoop(
  Environment *theEnv)
  {
   int inchar;

   CL_WriteString(theEnv,STDOUT,CommandLineData(theEnv)->BannerString);
   Set_HaltExecution(theEnv,false);
   Set_EvaluationError(theEnv,false);

   CL_CleanCurrentGarbageFrame(theEnv,NULL);
   CL_CallPeriodicTasks(theEnv);

   CL_PrintPrompt(theEnv);
   RouterData(theEnv)->CommandBufferInputCount = 0;
   RouterData(theEnv)->InputUngets = 0;
   RouterData(theEnv)->AwaitingInput = true;

   while (true)
     {
      /*===================================================*/
      /* If a batch file is active, grab the command input */
      /* directly from the batch file, otherwise call the  */
      /* event function.                                   */
      /*===================================================*/

      if (CL_BatchActive(theEnv) == true)
        {
         inchar = LLGetc_Batch(theEnv,STDIN,true);
         if (inchar == EOF)
           { (*CommandLineData(theEnv)->EventCallback)(theEnv); }
         else
           { CL_ExpandCommandString(theEnv,(char) inchar); }
        }
      else
        { (*CommandLineData(theEnv)->EventCallback)(theEnv); }

      /*=================================================*/
      /* If execution was halted, then remove everything */
      /* from the command buffer.                        */
      /*=================================================*/

      if (CL_Get_HaltExecution(theEnv) == true)
        {
         Set_HaltExecution(theEnv,false);
         Set_EvaluationError(theEnv,false);
         CL_FlushCommandString(theEnv);
         CL_WriteString(theEnv,STDOUT,"\n");
         CL_PrintPrompt(theEnv);
        }

      /*=========================================*/
      /* If a complete command is in the command */
      /* buffer, then execute it.                */
      /*=========================================*/

      CL_ExecuteIfCommandComplete(theEnv);
     }
  }

/***********************************************************/
/* CL_CommandLoop_Batch: Loop which waits for commands from a  */
/*   batch file and then executes them. Returns when there */
/*   are no longer any active batch files.                 */
/***********************************************************/
void CL_CommandLoop_Batch(
  Environment *theEnv)
  {
   Set_HaltExecution(theEnv,false);
   Set_EvaluationError(theEnv,false);

   CL_CleanCurrentGarbageFrame(theEnv,NULL);
   CL_CallPeriodicTasks(theEnv);

   CL_PrintPrompt(theEnv);
   RouterData(theEnv)->CommandBufferInputCount = 0;
   RouterData(theEnv)->InputUngets = 0;
   RouterData(theEnv)->AwaitingInput = true;

   CL_CommandLoop_BatchDriver(theEnv);
  }

/************************************************************/
/* CL_CommandLoopOnceThen_Batch: Loop which waits for commands  */
/*   from a batch file and then executes them. Returns when */
/*   there are no longer any active batch files.            */
/************************************************************/
void CL_CommandLoopOnceThen_Batch(
  Environment *theEnv)
  {
   if (! CL_ExecuteIfCommandComplete(theEnv)) return;

   CL_CommandLoop_BatchDriver(theEnv);
  }

/*********************************************************/
/* CL_CommandLoop_BatchDriver: Loop which waits for commands */
/*   from a batch file and then executes them. Returns   */
/*   when there are no longer any active batch files.    */
/*********************************************************/
void CL_CommandLoop_BatchDriver(
  Environment *theEnv)
  {
   int inchar;

   while (true)
     {
      if (Get_Halt_CommandLoop_Batch(theEnv) == true)
        {
         CloseAll_BatchSources(theEnv);
         Set_Halt_CommandLoop_Batch(theEnv,false);
        }

      /*===================================================*/
      /* If a batch file is active, grab the command input */
      /* directly from the batch file, otherwise call the  */
      /* event function.                                   */
      /*===================================================*/

      if (CL_BatchActive(theEnv) == true)
        {
         inchar = LLGetc_Batch(theEnv,STDIN,true);
         if (inchar == EOF)
           { return; }
         else
           { CL_ExpandCommandString(theEnv,(char) inchar); }
        }
      else
        { return; }

      /*=================================================*/
      /* If execution was halted, then remove everything */
      /* from the command buffer.                        */
      /*=================================================*/

      if (CL_Get_HaltExecution(theEnv) == true)
        {
         Set_HaltExecution(theEnv,false);
         Set_EvaluationError(theEnv,false);
         CL_FlushCommandString(theEnv);
         CL_WriteString(theEnv,STDOUT,"\n");
         CL_PrintPrompt(theEnv);
        }

      /*=========================================*/
      /* If a complete command is in the command */
      /* buffer, then execute it.                */
      /*=========================================*/

      CL_ExecuteIfCommandComplete(theEnv);
     }
  }

/**********************************************************/
/* CL_ExecuteIfCommandComplete: Checks to dete_rmine if there */
/*   is a completed command and if so executes it.        */
/**********************************************************/
bool CL_ExecuteIfCommandComplete(
  Environment *theEnv)
  {
   if ((CL_CompleteCommand(CommandLineData(theEnv)->CommandString) == 0) ||
       (RouterData(theEnv)->CommandBufferInputCount == 0) ||
       (RouterData(theEnv)->AwaitingInput == false))
     { return false; }

   if (CommandLineData(theEnv)->BeforeCommandExecutionCallback != NULL)
     {
      if (! (*CommandLineData(theEnv)->BeforeCommandExecutionCallback)(theEnv))
        { return false; }
     }

   CL_FlushPPBuffer(theEnv);
   CL_SetPPBufferStatus(theEnv,false);
   RouterData(theEnv)->CommandBufferInputCount = 0;
   RouterData(theEnv)->InputUngets = 0;
   RouterData(theEnv)->AwaitingInput = false;
   CL_RouteCommand(theEnv,CommandLineData(theEnv)->CommandString,true);
   CL_FlushPPBuffer(theEnv);
#if (! BLOAD_ONLY)
   CL_FlushParsingMessages(theEnv);
#endif
   Set_HaltExecution(theEnv,false);
   Set_EvaluationError(theEnv,false);
   CL_FlushCommandString(theEnv);

   CL_CleanCurrentGarbageFrame(theEnv,NULL);
   CL_CallPeriodicTasks(theEnv);

   CL_PrintPrompt(theEnv);

   return true;
  }

/*******************************/
/* CL_CommandCompleteAndNotEmpty: */
/*******************************/
bool CL_CommandCompleteAndNotEmpty(
  Environment *theEnv)
  {
   if ((CL_CompleteCommand(CommandLineData(theEnv)->CommandString) == 0) ||
       (RouterData(theEnv)->CommandBufferInputCount == 0) ||
       (RouterData(theEnv)->AwaitingInput == false))
     { return false; }

   return true;
  }

/*******************************************/
/* CL_PrintPrompt: Prints the command prompt. */
/*******************************************/
void CL_PrintPrompt(
   Environment *theEnv)
   {
    CL_WriteString(theEnv,STDOUT,COMMAND_PROMPT);

    if (CommandLineData(theEnv)->AfterPromptCallback != NULL)
      { (*CommandLineData(theEnv)->AfterPromptCallback)(theEnv); }
   }

/*****************************************/
/* CL_PrintBanner: Prints the CLIPS banner. */
/*****************************************/
void CL_PrintBanner(
   Environment *theEnv)
   {
    CL_WriteString(theEnv,STDOUT,CommandLineData(theEnv)->BannerString);
   }

/************************************************/
/* CL_SetAfterPromptFunction: Replaces the current */
/*   value of AfterPromptFunction.              */
/************************************************/
void CL_SetAfterPromptFunction(
  Environment *theEnv,
  AfterPromptFunction *funptr)
  {
   CommandLineData(theEnv)->AfterPromptCallback = funptr;
  }

/***********************************************************/
/* CL_SetBeforeCommandExecutionFunction: Replaces the current */
/*   value of BeforeCommandExecutionFunction.              */
/***********************************************************/
void CL_SetBeforeCommandExecutionFunction(
  Environment *theEnv,
  BeforeCommandExecutionFunction *funptr)
  {
   CommandLineData(theEnv)->BeforeCommandExecutionCallback = funptr;
  }

/*********************************************************/
/* CL_RouteCommand: Processes a completed command. Returns  */
/*   true if a command could be parsed, otherwise false. */
/*********************************************************/
bool CL_RouteCommand(
  Environment *theEnv,
  const char *command,
  bool printResult)
  {
   UDFValue returnValue;
   struct expr *top;
   const char *commandName;
   struct token theToken;
   int danglingConstructs;

   if (command == NULL)
     { return false; }

   /*========================================*/
   /* Open a string input source and get the */
   /* first token from that source.          */
   /*========================================*/

   CL_OpenStringSource(theEnv,"command",command,0);

   CL_GetToken(theEnv,"command",&theToken);

   /*=====================*/
   /* CL_Evaluate constants. */
   /*=====================*/

   if ((theToken.tknType == SYMBOL_TOKEN) || (theToken.tknType == STRING_TOKEN) ||
       (theToken.tknType == FLOAT_TOKEN) || (theToken.tknType == INTEGER_TOKEN) ||
       (theToken.tknType == INSTANCE_NAME_TOKEN))
     {
      CL_CloseStringSource(theEnv,"command");
      if (printResult)
        {
         CL_PrintAtom(theEnv,STDOUT,CL_TokenTypeToType(theToken.tknType),theToken.value);
         CL_WriteString(theEnv,STDOUT,"\n");
        }
      return true;
     }

   /*=====================*/
   /* CL_Evaluate variables. */
   /*=====================*/

   if ((theToken.tknType == GBL_VARIABLE_TOKEN) ||
       (theToken.tknType == MF_GBL_VARIABLE_TOKEN) ||
       (theToken.tknType == SF_VARIABLE_TOKEN) ||
       (theToken.tknType == MF_VARIABLE_TOKEN))
     {
      CL_CloseStringSource(theEnv,"command");
      top = CL_GenConstant(theEnv,CL_TokenTypeToType(theToken.tknType),theToken.value);
      CL_EvaluateExpression(theEnv,top,&returnValue);
      rtn_struct(theEnv,expr,top);
      if (printResult)
        {
         CL_WriteUDFValue(theEnv,STDOUT,&returnValue);
         CL_WriteString(theEnv,STDOUT,"\n");
        }
      return true;
     }

   /*========================================================*/
   /* If the next token isn't the beginning left parenthesis */
   /* of a command or construct, then whatever was entered   */
   /* cannot be evaluated at the command prompt.             */
   /*========================================================*/

   if (theToken.tknType != LEFT_PARENTHESIS_TOKEN)
     {
      CL_PrintErrorID(theEnv,"COMMLINE",1,false);
      CL_WriteString(theEnv,STDERR,"Expected a '(', constant, or variable.\n");
      CL_CloseStringSource(theEnv,"command");
      return false;
     }

   /*===========================================================*/
   /* The next token must be a function name or construct type. */
   /*===========================================================*/

   CL_GetToken(theEnv,"command",&theToken);
   if (theToken.tknType != SYMBOL_TOKEN)
     {
      CL_PrintErrorID(theEnv,"COMMLINE",2,false);
      CL_WriteString(theEnv,STDERR,"Expected a command.\n");
      CL_CloseStringSource(theEnv,"command");
      return false;
     }

   commandName = theToken.lexemeValue->contents;

   /*======================*/
   /* CL_Evaluate constructs. */
   /*======================*/

#if (! RUN_TIME) && (! BLOAD_ONLY)
   {
    CL_BuildError errorFlag;

    errorFlag = CL_ParseConstruct(theEnv,commandName,"command");
    if (errorFlag != BE_CONSTRUCT_NOT_FOUND_ERROR)
      {
       CL_CloseStringSource(theEnv,"command");
       if (errorFlag == BE_PARSING_ERROR)
         {
          CL_WriteString(theEnv,STDERR,"\nERROR:\n");
          CL_WriteString(theEnv,STDERR,CL_GetPPBuffer(theEnv));
          CL_WriteString(theEnv,STDERR,"\n");
         }
       CL_DestroyPPBuffer(theEnv);
       
       CL_SetWarningFileName(theEnv,NULL);
       CL_SetErrorFileName(theEnv,NULL);

       if (errorFlag == BE_NO_ERROR) return true;
       else return false;
      }
   }
#endif

   /*========================*/
   /* Parse a function call. */
   /*========================*/

   danglingConstructs = ConstructData(theEnv)->DanglingConstructs;
   CommandLineData(theEnv)->Parsing_TopLevelCommand = true;
   top = CL_Function2Parse(theEnv,"command",commandName);
   CommandLineData(theEnv)->Parsing_TopLevelCommand = false;
   CL_ClearParsedBindNames(theEnv);

   /*================================*/
   /* Close the string input source. */
   /*================================*/

   CL_CloseStringSource(theEnv,"command");

   /*=========================*/
   /* CL_Evaluate function call. */
   /*=========================*/

   if (top == NULL)
     {
      CL_SetWarningFileName(theEnv,NULL);
      CL_SetErrorFileName(theEnv,NULL);
      ConstructData(theEnv)->DanglingConstructs = danglingConstructs;
      return false;
     }

   CL_ExpressionInstall(theEnv,top);

   CommandLineData(theEnv)->CL_Evaluating_TopLevelCommand = true;
   CommandLineData(theEnv)->CurrentCommand = top;
   CL_EvaluateExpression(theEnv,top,&returnValue);
   CommandLineData(theEnv)->CurrentCommand = NULL;
   CommandLineData(theEnv)->CL_Evaluating_TopLevelCommand = false;

   CL_ExpressionDeinstall(theEnv,top);
   CL_ReturnExpression(theEnv,top);
   ConstructData(theEnv)->DanglingConstructs = danglingConstructs;
   
   CL_SetWarningFileName(theEnv,NULL);
   CL_SetErrorFileName(theEnv,NULL);

   /*=================================================*/
   /* Print the return value of the function/command. */
   /*=================================================*/

   if ((returnValue.header->type != VOID_TYPE) && printResult)
     {
      CL_WriteUDFValue(theEnv,STDOUT,&returnValue);
      CL_WriteString(theEnv,STDOUT,"\n");
     }

   return true;
  }

/*****************************************************************/
/* DefaultGetNextEvent: Default event-handling function. Handles */
/*   only keyboard events by first calling CL_ReadRouter to get a   */
/*   character and then calling CL_ExpandCommandString to add the   */
/*   character to the CommandString.                             */
/*****************************************************************/
static void DefaultGetNextEvent(
  Environment *theEnv)
  {
   int inchar;

   inchar = CL_ReadRouter(theEnv,STDIN);

   if (inchar == EOF) inchar = '\n';

   CL_ExpandCommandString(theEnv,(char) inchar);
  }

/*************************************/
/* CL_SetEventFunction: Replaces the    */
/*   current value of EventFunction. */
/*************************************/
EventFunction *CL_SetEventFunction(
  Environment *theEnv,
  EventFunction *theFunction)
  {
   EventFunction *tmp_ptr;

   tmp_ptr = CommandLineData(theEnv)->EventCallback;
   CommandLineData(theEnv)->EventCallback = theFunction;
   return tmp_ptr;
  }

/****************************************/
/* CL_TopLevelCommand: Indicates whether a */
/*   top-level command is being parsed. */
/****************************************/
bool CL_TopLevelCommand(
  Environment *theEnv)
  {
   return(CommandLineData(theEnv)->Parsing_TopLevelCommand);
  }

/***********************************************************/
/* CL_GetCommandCompletionString: Returns the last token in a */
/*   string if it is a valid token for command completion. */
/***********************************************************/
const char *CL_GetCommandCompletionString(
  Environment *theEnv,
  const char *theString,
  size_t maxPosition)
  {
   struct token lastToken;
   struct token theToken;
   char lastChar;
   const char *rs;
   size_t length;

   /*=========================*/
   /* Get the command string. */
   /*=========================*/

   if (theString == NULL) return("");

   /*=========================================================================*/
   /* If the last character in the command string is a space, character       */
   /* return, or quotation mark, then the command completion can be anything. */
   /*=========================================================================*/

   lastChar = theString[maxPosition - 1];
   if ((lastChar == ' ') || (lastChar == '"') ||
       (lastChar == '\t') || (lastChar == '\f') ||
       (lastChar == '\n') || (lastChar == '\r'))
     { return(""); }

   /*============================================*/
   /* Find the last token in the command string. */
   /*============================================*/

   CL_OpenTextSource(theEnv,"CommandCompletion",theString,0,maxPosition);
   ScannerData(theEnv)->IgnoreCompletionErrors = true;
   CL_GetToken(theEnv,"CommandCompletion",&theToken);
   CL_CopyToken(&lastToken,&theToken);
   while (theToken.tknType != STOP_TOKEN)
     {
      CL_CopyToken(&lastToken,&theToken);
      CL_GetToken(theEnv,"CommandCompletion",&theToken);
     }
   CL_CloseStringSource(theEnv,"CommandCompletion");
   ScannerData(theEnv)->IgnoreCompletionErrors = false;

   /*===============================================*/
   /* Dete_rmine if the last token can be completed. */
   /*===============================================*/

   if (lastToken.tknType == SYMBOL_TOKEN)
     {
      rs = lastToken.lexemeValue->contents;
      if (rs[0] == '[') return (&rs[1]);
      return lastToken.lexemeValue->contents;
     }
   else if (lastToken.tknType == SF_VARIABLE_TOKEN)
     { return lastToken.lexemeValue->contents; }
   else if (lastToken.tknType == MF_VARIABLE_TOKEN)
     { return lastToken.lexemeValue->contents; }
   else if ((lastToken.tknType == GBL_VARIABLE_TOKEN) ||
            (lastToken.tknType == MF_GBL_VARIABLE_TOKEN) ||
            (lastToken.tknType == INSTANCE_NAME_TOKEN))
     { return NULL; }
   else if (lastToken.tknType == STRING_TOKEN)
     {
      length = strlen(lastToken.lexemeValue->contents);
      return CL_GetCommandCompletionString(theEnv,lastToken.lexemeValue->contents,length);
     }
   else if ((lastToken.tknType == FLOAT_TOKEN) ||
            (lastToken.tknType == INTEGER_TOKEN))
     { return NULL; }

   return("");
  }

/****************************************************************/
/* Set_Halt_CommandLoop_Batch: Sets the CL_Halt_CommandLoop_Batch flag. */
/****************************************************************/
void Set_Halt_CommandLoop_Batch(
  Environment *theEnv,
  bool value)
  {
   CommandLineData(theEnv)->CL_Halt_CommandLoop_Batch = value;
  }

/*******************************************************************/
/* Get_Halt_CommandLoop_Batch: Returns the CL_Halt_CommandLoop_Batch flag. */
/*******************************************************************/
bool Get_Halt_CommandLoop_Batch(
  Environment *theEnv)
  {
   return(CommandLineData(theEnv)->CL_Halt_CommandLoop_Batch);
  }

#endif

