   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/11/17             */
   /*                                                     */
   /*                    MEMORY MODULE                    */
   /*******************************************************/

/*************************************************************/
/* Purpose: Memory allocation routines.                      */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed CL_HaltExecution check from the           */
/*            Env_ReleaseMem function. DR0863                 */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove compiler warnings.    */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems.                   */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Removed genlongalloc/genlongfree functions.    */
/*                                                           */
/*            Added get_mem and rtn_mem macros.              */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Removed deallocating message parameter from    */
/*            Env_ReleaseMem.                                 */
/*                                                           */
/*            Removed support for BLOCK_MEMORY.              */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*************************************************************/

#include <stdio.h>

#include "setup.h"

#include "constant.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "prntutil.h"
#include "router.h"
#include "utility.h"

#include <stdlib.h>

#if WIN_MVC
#include <malloc.h>
#endif

#define STRICT_ALIGN_SIZE sizeof(double)

#define SpecialMalloc(sz) malloc((STD_SIZE) sz)
#define SpecialFree(ptr) free(ptr)

/********************************************/
/* CL_InitializeMemory: Sets up memory tables. */
/********************************************/
void
CL_InitializeMemory (Environment * theEnv)
{
  CL_AllocateEnvironmentData (theEnv, MEMORY_DATA, sizeof (struct memoryData),
			      NULL);

  MemoryData (theEnv)->OutOfMemoryCallback = CL_DefaultOutOfMemoryFunction;

#if (MEM_TABLE_SIZE > 0)
  MemoryData (theEnv)->MemoryTable = (struct memoryPtr **)
    malloc ((STD_SIZE) (sizeof (struct memoryPtr *) * MEM_TABLE_SIZE));

  if (MemoryData (theEnv)->MemoryTable == NULL)
    {
      CL_PrintErrorID (theEnv, "MEMORY", 1, true);
      CL_WriteString (theEnv, STDERR, "Out of memory.\n");
      CL_ExitRouter (theEnv, EXIT_FAILURE);
    }
  else
    {
      int i;

      for (i = 0; i < MEM_TABLE_SIZE; i++)
	MemoryData (theEnv)->MemoryTable[i] = NULL;
    }
#else // MEM_TABLE_SIZE == 0
  MemoryData (theEnv)->MemoryTable = NULL;
#endif
}

/***************************************************/
/* CL_genalloc: A generic memory allocation function. */
/***************************************************/
void *
CL_genalloc (Environment * theEnv, size_t size)
{
  void *memPtr;

  memPtr = malloc (size);

  if (memPtr == NULL)
    {
      CL_ReleaseMem (theEnv,
		     (long long) ((size * 5 > 4096) ? size * 5 : 4096));
      memPtr = malloc (size);
      if (memPtr == NULL)
	{
	  CL_ReleaseMem (theEnv, -1);
	  memPtr = malloc (size);
	  while (memPtr == NULL)
	    {
	      if ((*MemoryData (theEnv)->OutOfMemoryCallback) (theEnv, size))
		return NULL;
	      memPtr = malloc (size);
	    }
	}
    }

  MemoryData (theEnv)->MemoryAmount += size;
  MemoryData (theEnv)->MemoryCalls++;

  return memPtr;
}

/***********************************************/
/* CL_DefaultOutOfMemoryFunction: Function called */
/*   when the KB runs out of memory.           */
/***********************************************/
bool
CL_DefaultOutOfMemoryFunction (Environment * theEnv, size_t size)
{
#if MAC_XCD
#pragma unused(size)
#endif

  CL_PrintErrorID (theEnv, "MEMORY", 1, true);
  CL_WriteString (theEnv, STDERR, "Out of memory.\n");
  CL_ExitRouter (theEnv, EXIT_FAILURE);
  return true;
}

/**********************************************************/
/* CL_SetOutOfMemoryFunction: Allows the function which is   */
/*   called when the KB runs out of memory to be changed. */
/**********************************************************/
OutOfMemoryFunction *
CL_SetOutOfMemoryFunction (Environment * theEnv,
			   OutOfMemoryFunction * functionPtr)
{
  OutOfMemoryFunction *tmpPtr;

  tmpPtr = MemoryData (theEnv)->OutOfMemoryCallback;
  MemoryData (theEnv)->OutOfMemoryCallback = functionPtr;
  return tmpPtr;
}

/****************************************************/
/* CL_genfree: A generic memory deallocation function. */
/****************************************************/
void
CL_genfree (Environment * theEnv, void *waste, size_t size)
{
  free (waste);

  MemoryData (theEnv)->MemoryAmount -= size;
  MemoryData (theEnv)->MemoryCalls--;
}

/******************************************************/
/* CL_genrealloc: Simple (i.e. dumb) version of realloc. */
/******************************************************/
void *
CL_genrealloc (Environment * theEnv,
	       void *oldaddr, size_t oldsz, size_t newsz)
{
  char *newaddr;
  unsigned i;
  size_t limit;

  newaddr = ((newsz != 0) ? (char *) CL_gm2 (theEnv, newsz) : NULL);

  if (oldaddr != NULL)
    {
      limit = (oldsz < newsz) ? oldsz : newsz;
      for (i = 0; i < limit; i++)
	{
	  newaddr[i] = ((char *) oldaddr)[i];
	}
      for (; i < newsz; i++)
	{
	  newaddr[i] = '\0';
	}
      CL_rm (theEnv, oldaddr, oldsz);
    }

  return ((void *) newaddr);
}

/********************************/
/* CL_MemUsed: C access routine */
/*   for the mem-used command.  */
/********************************/
long long
CL_MemUsed (Environment * theEnv)
{
  return MemoryData (theEnv)->MemoryAmount;
}

/***********************************/
/* CL_MemRequests: C access routine   */
/*   for the mem-requests command. */
/***********************************/
long long
CL_MemRequests (Environment * theEnv)
{
  return MemoryData (theEnv)->MemoryCalls;
}

/***************************************/
/* CL_UpdateMemoryUsed: Allows the amount */
/*   of memory used to be updated.     */
/***************************************/
long long
CL_UpdateMemoryUsed (Environment * theEnv, long long value)
{
  MemoryData (theEnv)->MemoryAmount += value;
  return MemoryData (theEnv)->MemoryAmount;
}

/*******************************************/
/* CL_UpdateMemoryRequests: Allows the number */
/*   of memory requests to be updated.     */
/*******************************************/
long long
CL_UpdateMemoryRequests (Environment * theEnv, long long value)
{
  MemoryData (theEnv)->MemoryCalls += value;
  return MemoryData (theEnv)->MemoryCalls;
}

/**********************************/
/* CL_ReleaseMem: C access routine   */
/*   for the release-mem command. */
/**********************************/
long long
CL_ReleaseMem (Environment * theEnv, long long maximum)
{
  struct memoryPtr *tmpPtr, *memPtr;
  unsigned int i;
  long long returns = 0;
  long long amount = 0;

  for (i = (MEM_TABLE_SIZE - 1); i >= sizeof (char *); i--)
    {
      CL_YieldTime (theEnv);
      memPtr = MemoryData (theEnv)->MemoryTable[i];
      while (memPtr != NULL)
	{
	  tmpPtr = memPtr->next;
	  CL_genfree (theEnv, memPtr, i);
	  memPtr = tmpPtr;
	  amount += i;
	  returns++;
	  if ((returns % 100) == 0)
	    {
	      CL_YieldTime (theEnv);
	    }
	}
      MemoryData (theEnv)->MemoryTable[i] = NULL;
      if ((amount > maximum) && (maximum > 0))
	{
	  return amount;
	}
    }

  return amount;
}

/*****************************************************/
/* CL_gm1: Allocates memory and sets all bytes to zero. */
/*****************************************************/
void *
CL_gm1 (Environment * theEnv, size_t size)
{
  struct memoryPtr *memPtr;
  char *tmpPtr;
  size_t i;

  if ((size < sizeof (char *)) || (size >= MEM_TABLE_SIZE))
    {
      tmpPtr = (char *) CL_genalloc (theEnv, size);
      for (i = 0; i < size; i++)
	{
	  tmpPtr[i] = '\0';
	}
      return ((void *) tmpPtr);
    }

  memPtr = (struct memoryPtr *) MemoryData (theEnv)->MemoryTable[size];
  if (memPtr == NULL)
    {
      tmpPtr = (char *) CL_genalloc (theEnv, size);
      for (i = 0; i < size; i++)
	{
	  tmpPtr[i] = '\0';
	}
      return ((void *) tmpPtr);
    }

  MemoryData (theEnv)->MemoryTable[size] = memPtr->next;

  tmpPtr = (char *) memPtr;
  for (i = 0; i < size; i++)
    {
      tmpPtr[i] = '\0';
    }

  return ((void *) tmpPtr);
}

/*****************************************************/
/* CL_gm2: Allocates memory and does not initialize it. */
/*****************************************************/
void *
CL_gm2 (Environment * theEnv, size_t size)
{
#if (MEM_TABLE_SIZE > 0)
  struct memoryPtr *memPtr;
#endif

  if (size < sizeof (char *))
    {
      return CL_genalloc (theEnv, size);
    }

#if (MEM_TABLE_SIZE > 0)
  if (size >= MEM_TABLE_SIZE)
    return CL_genalloc (theEnv, size);

  memPtr = (struct memoryPtr *) MemoryData (theEnv)->MemoryTable[size];
  if (memPtr == NULL)
    {
      return CL_genalloc (theEnv, size);
    }

  MemoryData (theEnv)->MemoryTable[size] = memPtr->next;

  return ((void *) memPtr);
#else
  return CL_genalloc (theEnv, size);
#endif
}

/****************************************/
/* CL_rm: Returns a block of memory to the */
/*   CL_maintained pool of free memory.    */
/****************************************/
void
CL_rm (Environment * theEnv, void *str, size_t size)
{
#if (MEM_TABLE_SIZE > 0)
  struct memoryPtr *memPtr;
#endif

  if (size == 0)
    {
      CL_SystemError (theEnv, "MEMORY", 1);
      CL_ExitRouter (theEnv, EXIT_FAILURE);
      return;
    }

  if (size < sizeof (char *))
    {
      CL_genfree (theEnv, str, size);
      return;
    }

#if (MEM_TABLE_SIZE > 0)
  if (size >= MEM_TABLE_SIZE)
    {
      CL_genfree (theEnv, str, size);
      return;
    }

  memPtr = (struct memoryPtr *) str;
  memPtr->next = MemoryData (theEnv)->MemoryTable[size];
  MemoryData (theEnv)->MemoryTable[size] = memPtr;
#else
  CL_genfree (theEnv, str, size);
#endif
}

/***************************************************/
/* CL_PoolSize: Returns number of bytes in free pool. */
/***************************************************/
unsigned long
CL_PoolSize (Environment * theEnv)
{
  unsigned long cnt = 0;

#if (MEM_TABLE_SIZE > 0)
  int i;
  struct memoryPtr *memPtr;

  for (i = sizeof (char *); i < MEM_TABLE_SIZE; i++)
    {
      memPtr = MemoryData (theEnv)->MemoryTable[i];
      while (memPtr != NULL)
	{
	  cnt += (unsigned long) i;
	  memPtr = memPtr->next;
	}
    }
#endif

  return (cnt);
}

/***************************************************************/
/* CL_Actual_PoolSize : Returns number of bytes DOS requires to    */
/*   store the free pool.  This routine is functionally        */
/*   equivalent to pool_size on anything other than the IBM-PC */
/***************************************************************/
unsigned long
CL_Actual_PoolSize (Environment * theEnv)
{
  return (CL_PoolSize (theEnv));
}

/*****************************************/
/* CL_SetConserveMemory: Allows the setting */
/*    of the memory conservation flag.   */
/*****************************************/
bool
CL_SetConserveMemory (Environment * theEnv, bool value)
{
  bool ov;

  ov = MemoryData (theEnv)->ConserveMemory;
  MemoryData (theEnv)->ConserveMemory = value;
  return ov;
}

/****************************************/
/* CL_GetConserveMemory: Returns the value */
/*    of the memory conservation flag.  */
/****************************************/
bool
CL_GetConserveMemory (Environment * theEnv)
{
  return MemoryData (theEnv)->ConserveMemory;
}

/**************************/
/* CL_genmemcpy:             */
/**************************/
void
CL_genmemcpy (char *dst, char *src, unsigned long size)
{
  unsigned long i;

  for (i = 0L; i < size; i++)
    dst[i] = src[i];
}
