   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*               CLIPS Version 6.40  08/06/16          */
   /*                                                     */
   /*                     MAIN MODULE                     */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Moved CL_UserFunctions and EnvCL_UserFunctions to    */
/*            the new userfunctions.c file.                  */
/*                                                           */
/*      6.40: Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Moved CatchCtrlC to CL_main.c.                    */
/*                                                           */
/*************************************************************/

/***************************************************************************/
/*                                                                         */
/* Permission is hereby granted, free of charge, to any person obtaining   */
/* a copy of this software and associated documentation files (the         */
/* "Software"), to deal in the Software without restriction, including     */
/* without limitation the rights to use, copy, modify, merge, publish,     */
/* distribute, and/or sell copies of the Software, and to peCL_rmit persons   */
/* to whom the Software is furnished to do so.                             */
/*                                                                         */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS */
/* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF              */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT   */
/* OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY  */
/* CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES */
/* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN   */
/* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF */
/* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.          */
/*                                                                         */
/***************************************************************************/

#include "clips.h"

#if   UNIX_V || LINUX || DARWIN || UNIX_7 || WIN_GCC || WIN_MVC
#include <signal.h>
#endif

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if UNIX_V || LINUX || DARWIN || UNIX_7 || WIN_GCC || WIN_MVC
   static void                    CatchCtrlC(int);
#endif

/***************************************/
/* LOCAL INTERNAL VARIABLE DEFINITIONS */
/***************************************/

   static Environment            *CL_mainEnv;

/****************************************/
/* main: Starts execution of the expert */
/*   system development environment.    */
/****************************************/
int main(
  int argc,
  char *argv[])
  {
   CL_mainEnv = CL_CreateEnvironment();

#if UNIX_V || LINUX || DARWIN || UNIX_7 || WIN_GCC || WIN_MVC
   signal(SIGINT,CatchCtrlC);
#endif

   CL_RerouteStdin(CL_mainEnv,argc,argv);
   CL_CommandLoop(CL_mainEnv);

   /*==================================================================*/
   /* Control does not noCL_rmally return from the CL_CommandLoop function.  */
   /* However if you are embedding CLIPS, have replaced CL_CommandLoop    */
   /* with your own embedded calls that will return to this point, and */
   /* are running software that helps detect memory leaks, you need to */
   /* add function calls here to deallocate memory still being used by */
   /* CLIPS. If you have a multi-threaded application, no environments */
   /* can be currently executing.                                      */
   /*==================================================================*/

   CL_DestroyEnvironment(CL_mainEnv);

   return -1;
  }

#if UNIX_V || LINUX || DARWIN || UNIX_7 || WIN_GCC || WIN_MVC || DARWIN
/***************/
/* CatchCtrlC: */
/***************/
static void CatchCtrlC(
  int sgnl)
  {
   SetCL_HaltExecution(CL_mainEnv,true);
   CloseAllCL_BatchSources(CL_mainEnv);
   signal(SIGINT,CatchCtrlC);
  }
#endif
