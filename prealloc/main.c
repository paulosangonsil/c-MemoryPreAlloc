/* prealloc.cpp : Defines the entry point for the console application. */

#include "stdafx.h"

#include "prealloc.c"

/*#undef free_i
#undef malloc_i
#define free_i  free
#define malloc_i  malloc
*/

typedef
struct  _allocations
{
    INT     iSize;

    PVOID   pMem;
}   _ALLOC, *_PALLOC;

void    AllocateBlocks (_PALLOC pBlockList, PINT pSizes)
{
    INT     iCurrItem;

    /* Iterate the integer list, doing the requested
       allocations */
    for (iCurrItem = 0; pSizes [iCurrItem] != 0; iCurrItem++)
    {
        pBlockList [iCurrItem].iSize = pSizes [iCurrItem];

        pBlockList [iCurrItem].pMem = \
                        malloc_i (pBlockList [iCurrItem].iSize);
    }

    return;
}

void    DeallocateBlocks (_PALLOC pBlockList, PINT pSizes)
{
    INT     iCurrItem;

    /* Iterate the integer list, doing the requested
       allocations */
    for (iCurrItem = 0; pSizes [iCurrItem] != 0; iCurrItem++)
    {
        free_i (pBlockList [iCurrItem].pMem);
    }

    return;
}

void    FillAllocList (PINT pSizeList)
{
    return;
}

void    StaticAlloc (void)
{
    _ALLOC  AllocList [110];

    INT     iBlocksSize []  = {4, 2, 48, 36, 5, 8, 1, 53, 4, 2,
                               4, 2, 48, 36, 5, 8, 1, 53, 4, 2,
                               4, 2, 48, 36, 5, 8, 1, 53, 4, 2,
                               4, 2, 48, 36, 5, 8, 1, 53, 4, 2,
                               4, 2, 48, 36, 5, 8, 1, 53, 4, 2,
                               4, 2, 48, 36, 5, 8, 1, 53, 4, 2,
                               4, 2, 48, 36, 5, 8, 1, 53, 4, 2,
                               4, 2, 48, 36, 5, 8, 1, 53, 4, 2,
                               4, 2, 48, 36, 5, 8, 1, 53, 4, 2,
                               4, 2, 48, 36, 5, 8, 1, 53, 4, 2,
                               4, 2, 48, 36, 5, 8, 1, 53, 4, 2,
                               0};

    AllocateBlocks (AllocList, iBlocksSize);
    Sleep (2);
    DeallocateBlocks (AllocList, iBlocksSize);

    return;
}

void    DynamicAlloc (void)
{
    #define _MAXITEM        117

    _ALLOC  AllocList [_MAXITEM];

    INT     iCurrItem;

    /* initialize random seed */
    srand ( time (NULL) );

    /* Iterate the integer list, doing random allocations */
    for (iCurrItem = 0; iCurrItem < _MAXITEM; iCurrItem++)
    {
        AllocList [iCurrItem].pMem = malloc_i (rand () % 100);
        Sleep (2);
    }

    while (--iCurrItem > -1)
    {
        free_i (AllocList [iCurrItem].pMem);
    }

    #undef _MAXITEM

    return;
}

int _tmain (int argc, _TCHAR* argv[])
{
/*    StaticAlloc (); */
    DynamicAlloc ();

    return (0);
}
