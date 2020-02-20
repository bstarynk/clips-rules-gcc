   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*              FILE COMMANDS HEADER FILE              */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for file commands including    */
/*   batch, dribble-on, dribble-off, save, load, bsave, and  */
/*   bload.                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.40: Split from filecom.c                           */
/*                                                           */
/*************************************************************/

#ifndef _H_fileutil

#pragma once

#define _H_fileutil

   bool                           CL_DribbleOn(Environment *,const char *);
   bool                           CL_DribbleActive(Environment *);
   bool                           CL_DribbleOff(Environment *);
   void                           CL_AppendDribble(Environment *,const char *);
   int                            LLGetcCL_Batch(Environment *,const char *,bool);
   bool                           CL_Batch(Environment *,const char *);
   bool                           OpenCL_Batch(Environment *,const char *,bool);
   bool                           OpenStringCL_Batch(Environment *,const char *,const char *,bool);
   bool                           RemoveCL_Batch(Environment *);
   bool                           CL_BatchActive(Environment *);
   void                           CloseAllCL_BatchSources(Environment *);
   bool                           CL_BatchStar(Environment *,const char *);

#endif /* _H_fileutil */






