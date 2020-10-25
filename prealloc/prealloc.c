/**
    \file
 */

#include "stdafx.h"
#include "prealloc.h"


/** The manager main data structure. */
static
GLBMNG  glbMng          = {0};


#ifndef _MSC_VER
    clock_t clock_i (void)
    {
        struct
        tms tTime   = {0};

        times (&tTime);

        return (tTime.tms_stime + tTime.tms_utime);
    }
#else
    #define clock_i clock
#endif

/**
    \brief  Function for read the process management
    records from its file.

    The function will accomplish the task of open, read
    and fill the FILEREC pointer.

    @param      ppFileRec           Pointer to pointer to the
                                    structure to hold the read
                                    file data.

    @return     TRUE                There was an error processing
                                    the map file.
    @return     FALSE               Success.
 */

INT     ReadMapFile (PP_FILEREC ppFileRec)
{
    FILEHEADER  fhHeader;

    P_FILEREC   pFileRec    = (ppFileRec) ? *ppFileRec : (P_FILEREC) NULL;

    /* First, open the file */
    FILE        *fileMap    = fopen (MEMMAPFILENAME, "rb");

    ULONG       ulCount;

    INT         iRet        = FALSE;

    if (fileMap == NULL)
    {
        iRet = ERROR_MAPNOTFOUND;

        goto fEnd;
    }

    /* Get the file header */
    fread (&fhHeader, sizeof (FILEHEADER), 1, fileMap);

    if ( ferror (fileMap) )
    {
        iRet = ERROR_READINGMAP;

        goto fEnd;
    }

    /* We'll allocate memory only if we've not done it before */
    if (pFileRec == NULL)
    {
        /* Calculate the size needed for reading the whole map file */
        ulCount = fhHeader.ulQtyRecs *
                    ( sizeof (FILERECD) + sizeof (FILEREC) );

        /* See if there's request to be read */
        if (ulCount == 0)
        {
            iRet = ERROR_READINGMAP;

            goto fEnd;
        }

        /* Allocate the needed memory */
        *ppFileRec = pFileRec = (P_FILEREC) malloc_o (ulCount);

        if (pFileRec == NULL)
        {
            iRet = ERROR_PREALLOC;

#if ( (! defined(_MSC_VER)) && defined(_DEBUG) )
            openlog ("ReadMapFile", LOG_ODELAY, LOG_DEBUG);

            syslog (LOG_DEBUG, "malloc_o failed");

            closelog ();
#endif

            goto fEnd;
        }
    }

    /* Read every request from the file */
    ulCount = 0;

    do
    {
        fread (&pFileRec->FileRec, sizeof (FILERECD), 1, fileMap);

        if ( ferror (fileMap) )
        {
            iRet = ERROR_READINGMAP;

            goto fEnd;
        }

       if (++ulCount < fhHeader.ulQtyRecs)
        {
            pFileRec->pNextRec =
                (P_FILEREC) ( (ULONG) pFileRec + ( sizeof (FILEREC) ) );

            pFileRec = pFileRec->pNextRec;
        }
        else
            pFileRec->pNextRec = NULL;
    }
    while (ulCount < fhHeader.ulQtyRecs);

fEnd:
    if (fileMap != NULL)
        fclose (fileMap);

    /* There was an error in the processing */
    if (iRet != ERROR_SUCCESS)
    {
        if (pFileRec != NULL)
            free_o (pFileRec);
    }

    return (iRet);
}


/**
    \brief  Function for write the memory requests to the
    map file.

    This function will write to the map file all the memory
    requests.

    @param      pData               The used address.
    @param      szReq               The address size, when needed.
    @param      bDataType           To indicate the pData type.
 */

void    WriteMapFile (PVOID pData, size_t szReq, BYTE bDataType)
{
    /* First, open the file */
    static
    INT             fileMap     = 0;

    FILERECD        FileRec     = {0};

    FILEHEADER      fhHeader    = {0};

    P_REQALLOC      pAlloc      = NULL;

    P_REQDEALLOC    pDealloc    = NULL;

    if (fileMap == 0)
    {
        fileMap = open (MEMMAPFILENAME, O_WRONLY |
                        O_CREAT | O_BINARY, _S_IWRITE);

        if (fileMap == 0)
        {
            glbMng.iError = ERROR_WRITINGMAP;   /* Indicate an error */

            goto fEnd;
        }
    }

    /* Fill the request structure first */
    FileRec.tTime = clock_i ();

    FileRec.szReqSz = szReq;

    FileRec.pAddr = pData;

    FileRec.ulID = ++glbMng.ulReqsNum;

    FileRec.u.bType = bDataType;

    /* Write the file header */
    fhHeader.ulQtyRecs = glbMng.ulReqsNum;
    fhHeader.ulStatNum = glbMng.ulStatNum;

    lseek (fileMap, 0, SEEK_SET);

    /* Store the file header */
    write ( fileMap, &fhHeader, sizeof (FILEHEADER) );

    /* Go to the file end */
    if (lseek (fileMap, 0, SEEK_END) == -1)
    {
        glbMng.iError = ERROR_WRITINGMAP;   /* Indicate an error */

        goto fEnd;
    }

    /* And write the request structure */
    write ( fileMap, &FileRec, sizeof (FILERECD) );

fEnd:
/*  We've found that not closing the file map improves
    the algorithm performance.
    close (fileMap);*/

    return;
}


/** The sort types for the SortFileRecs function. */

enum    sorttypes
{
    FIRSTSORT,                  /**< The first sort indicator. */
    BYLIFETIME      = FIRSTSORT,/**< Sort by the request lifetime. */
    BYSIZE,                     /**< Sort by size (ascending). */
    BYTIME,                     /**< Sort by the time the request was
                                     made (relative to the program start). */
    MAXSORT                     /**< The enum end indicator. */
};


/**
    \brief  Function that will sort the map file records by
    a specified type.

    This function must do the sorting in the map file records,
    by request address (grouping alloc/dealloc) and by its life time.

    @param      ppFileRecs          The read records #ReadMapFile.
    @param      bSort               The sort types #sorttypes.

    @return     TRUE                There was an error sorting
                                    the requests.
    @return     FALSE               Success.
 */

INT     SortFileRecs (PP_FILEREC ppFileRecs, BYTE bSort)
{
    P_FILEREC   pFileRec    = *ppFileRecs,
                pNextFRec   = pFileRec->pNextRec,
                pBeforeFRec = NULL,
                pTmpFRec    = NULL,
                pFirstRec   = NULL;

    INT         iRet        = FALSE;

    /* There's only one request */
    if (pNextFRec == NULL)
        return (TRUE);

    /* First we'll assure if the requests are grouped by
       address (alloc->dealloc) */
    do
    {
        /* Store the process last execution duration */
        if (glbMng.ulProcDur < (unsigned) pFileRec->FileRec.tTime)
            glbMng.ulProcDur = (ULONG) pFileRec->FileRec.tTime;

        /* Do the sort */
        if ( (pFileRec->FileRec.pAddr == pNextFRec->FileRec.pAddr) &&
            (pNextFRec->FileRec.u.bType == TYPE_DEALLOC) )
        {
            iRet = TRUE;

            if (pFileRec->pNextRec != pNextFRec)
            {
                pTmpFRec = pFileRec->pNextRec;
                pFileRec->pNextRec = pNextFRec;

                if (pFirstRec)
                    pFirstRec->pNextRec = pNextFRec->pNextRec;

                pNextFRec->pNextRec = pTmpFRec;

                pBeforeFRec = pFileRec->pNextRec;
            }

            pFileRec = pNextFRec->pNextRec;

            /* The next request */
            if (pFileRec != NULL)
            {
                pFirstRec = pNextFRec;
                pNextFRec = pFileRec->pNextRec;
            }
        }
        else
        {
            /* The next request */
            pFirstRec = pNextFRec;
            pNextFRec = pNextFRec->pNextRec;
        }

        /* Even if one request don't have a corresponding free(),
           we must look for the other requests */
        if (pNextFRec == NULL)
        {
            /* If the request don't have its free() pair,
               it should be deleted from our list */
            if (iRet == FALSE)
            {
                if (pBeforeFRec == NULL)
                    *ppFileRecs = pFileRec->pNextRec;
                else
                    pBeforeFRec->pNextRec = pFileRec->pNextRec;
            }
            else
                pBeforeFRec = pFileRec;

            pFileRec = pFileRec->pNextRec;

            if (pFileRec != NULL)
                pNextFRec = pFileRec->pNextRec;
        }

        iRet = FALSE;
    }
    while ( (pFileRec != NULL) && (pNextFRec != NULL) );


    pFileRec = *ppFileRecs;

    pNextFRec = pFileRec->pNextRec;

    if (bSort == BYLIFETIME)
    {
        /* Get the alloc/dealloc pair to get the object life time */
        do
        {
            pFileRec->FileRec.u.tLifeTime = \
                    pFileRec->pNextRec->FileRec.tTime -
                                    pFileRec->FileRec.tTime;

            /* Discard the deallocation request */
            pFileRec->pNextRec = pFileRec->pNextRec->pNextRec;

            /* Update the current request */
            pFileRec = pFileRec->pNextRec;
        }
        while ( (pFileRec != NULL) && (pFileRec->pNextRec != NULL) );

        pBeforeFRec = NULL;
        pFirstRec = pFileRec = *ppFileRecs;

        /* Now we'll really sort in an descending way
           the requests by the object life time */
        do
        {
            /* If the following condition resolves to
               TRUE, means that all the valids request
               was already processed */
            if (pFileRec->FileRec.u.tLifeTime < 1)
            {
                if (pBeforeFRec)
                   pBeforeFRec->pNextRec = NULL;

                break;
            }

            /* We've reached the allocations pairs */
            if (pFileRec->FileRec.u.tLifeTime < \
                    pFileRec->pNextRec->FileRec.u.tLifeTime)
            {
                /* Update the chain order */
                if (pFileRec == pFirstRec)
                    pFirstRec = pFileRec->pNextRec;

                pTmpFRec = pFileRec->pNextRec->pNextRec;
                pFileRec->pNextRec->pNextRec = pFileRec;
                pFileRec->pNextRec = pTmpFRec;

                /* Restart the search */
                pFileRec = pFirstRec;
                pBeforeFRec = NULL;
            }
            else
            {
                pBeforeFRec = pFileRec;
                pFileRec = pFileRec->pNextRec;
            }
        }
        while ( (pFileRec != NULL) && (pFileRec->pNextRec != NULL) );
    }

    return (iRet);
}


/**
    \brief  Algorithm to select the requests to be controlled.

    This procedure will elect only the requests really necessary
    to be controlled. In this point not all the requests must be
    controlled: think about the requests that have a short life time.
    Of course this time is relative to the total application
    execution time. As its life time is 'short', there'll no need
    to control that.

    @param      ppFileRecs          The read records: @see ReadMapFile.
 */

void    ElectRecs (PP_FILEREC ppFileRecs)
{
    P_FILEREC   pFileRec    = *ppFileRecs,
                pBeforeFRec = NULL;

    DOUBLE      dPerc;

    if (glbMng.ulProcDur == 0)
    {
        *ppFileRecs = NULL;

        return;
    }

    do
    {
        dPerc = ( (DOUBLE)
                ( (DOUBLE) pFileRec->FileRec.u.tLifeTime /
                                    (DOUBLE) glbMng.ulProcDur ) );

        /* Only the requests with more than _LIFETIMEPERC of the
           process lifetime should be controlled */
        if ( (dPerc < _LIFETIMEPERC) || (pFileRec->FileRec.szReqSz == 0) )
        {
            /* Case where the first request won't be tracked -
               We'll update the pointer */
            if (pFileRec == *ppFileRecs)
            {
                *ppFileRecs = pFileRec = pFileRec->pNextRec;

                pBeforeFRec = NULL;
            }
            else
            {
                pBeforeFRec->pNextRec = pFileRec->pNextRec;

                pFileRec = pFileRec->pNextRec;
            }
        }

        else
        {
#if ( (! defined(_MSC_VER)) && defined(_DEBUG) )
            openlog ("ElectRecs", LOG_ODELAY, LOG_DEBUG);

            syslog (LOG_DEBUG, "%d;%d",
                        pFileRec->FileRec.ulID,
                        pFileRec->FileRec.szReqSz);

            closelog ();
#endif

            pBeforeFRec = pFileRec;

            pFileRec = pFileRec->pNextRec;
        }
    }
    while (pFileRec != NULL);

    return;
}


/**
    \brief  Procedure to do the final arrangements in the ellected requests.

    This function will fill the GLBMNG.pRequests pointer with the ellected
    requests.

    @param      pFileRecs           The read records: @see ReadMapFile.
 */

void    MakeInternReqs (P_FILEREC pFileRecs)
{
    FILERECD    FileRec;

    P_FILEREC   pFileRec;

    P_INTERNMNG pRequest;

    /* First create the file */
    FILE        *fileTmp    = tmpfile ();

    ULONG       ulCount     = 0,
                ulPos       = 0;

    if (fileTmp == NULL)
    {
        glbMng.iError = ERROR_WRITINGMAP;   /* Indicate an error */

#if ( (! defined(_MSC_VER)) && defined(_DEBUG) )
        openlog ("MakeInternReqs", LOG_ODELAY, LOG_DEBUG);

        syslog (LOG_DEBUG, "tmpfile failed");

        closelog ();
#endif

        goto fEnd;
    }

    /* Put the elected records in the disk */
    for (pFileRec = pFileRecs;
                pFileRec != NULL; pFileRec = pFileRec->pNextRec)
    {
        ulPos++;

        /* Sum the size needed for the allocations */
        ulCount += pFileRec->FileRec.szReqSz;

        fwrite (&pFileRec->FileRec, sizeof (FILERECD), 1, fileTmp);
    }

    /* Give back the used memory... */
    free_o (glbMng.pRequests);

    /* The total ammount we'll use */
    glbMng.ulTotalMem = ulCount;

    /* ... Just to get it again, exactly the size needed */
    ulCount += ulPos * sizeof (INTERNMNG);

    glbMng.pRequests = (P_INTERNMNG) malloc_o (ulCount);

    if (glbMng.pRequests == NULL)
    {
        glbMng.iError = ERROR_PREALLOC;   /* Indicate an error */

#if ( (! defined(_MSC_VER)) && defined(_DEBUG) )
        openlog ("MakeInternReqs", LOG_ODELAY, LOG_DEBUG);

        syslog (LOG_DEBUG, "malloc_o failed");

        closelog ();
#endif

        goto fEnd;
    }

    pRequest = glbMng.pRequests;

    /* Get to the file start */
    rewind (fileTmp);

    /* Get everything from the disk, now putting it in
       the right structure */
    for (ulCount = ulPos; ulCount > 0; ulCount--)
    {
        fread ( (PVOID) &FileRec, sizeof (FILERECD), 1, fileTmp );

        /* Fill the global structure */
        pRequest->ulID = FileRec.ulID;

        if (pRequest->ulID > glbMng.ulLastReq)
            glbMng.ulLastReq = pRequest->ulID;

        pRequest->szReqSz = FileRec.szReqSz;

        /* Calculate the address to be returned to the application */
        ulPos = (ULONG) pRequest + sizeof (INTERNMNG);

        pRequest->pAddr = (PVOID) ulPos;

        /* The last request must have its 'next' pointer as NULL */
        if ( (ulCount - 1) == 0 )
            pRequest->pNextAddrs = NULL;
        else
        {
            pRequest->pNextAddrs = \
                    (P_INTERNMNG) (ulPos + pRequest->szReqSz);

            pRequest = pRequest->pNextAddrs;
        }
    }

fEnd:
    if (fileTmp != NULL)
        fclose (fileTmp);

    return;
}


/**
    \brief  Function that will do the pre-allocation, according
    to the application allocation behavior.

    This function will analyze the allocation requests read from
    the map file. Will do the pre-allocation of the needed memory
    for the elected requests.
 */

void    PlanTheStrategy (void)
{
    ULONG       ulCount     = FIRSTSORT;

    P_FILEREC   pFileRec    = NULL;

    INT         iRet;

    /* The strategy already is planned */
    if (glbMng.ulReqsNum != 0)
        return;

    /* Get the application allocation behavior */
    iRet = ReadMapFile (&pFileRec);

    /* There was an error on the read */
    if (iRet != ERROR_SUCCESS)
    {
        glbMng.iError = iRet;   /* Indicate an error */

        goto fEnd;
    }

    /* Just to maintain the track of the original alloc.
       In the next calls this pointer probably will be modified
       to reflect the sort and election */
    glbMng.pRequests = (P_INTERNMNG) pFileRec;

    /* Do the sort */
    iRet = SortFileRecs (&pFileRec, (BYTE) ulCount);

    if (iRet != ERROR_SUCCESS)
    {
        glbMng.iError = iRet;

        goto fEnd;
    }

    /* Do the election in the records/requests */
    ElectRecs (&pFileRec);

    /* If there's no request elected, we won't do anything */
    if (pFileRec == NULL)
    {
        glbMng.iError = ERROR_PREALLOC;

        goto fEnd;
    }

    /* Make the real requests for use in the internal management */
    MakeInternReqs (pFileRec);

fEnd:
    /* Free the used memory */
    if (glbMng.iError == ERROR_PREALLOC)
    {
        free_o (glbMng.pRequests);

        glbMng.pRequests = NULL;
    }

    return;
}


/**
    \brief  Malloc's wrapper.

    This function will manage the memory, doing the necessary work
    to apply the pre-allocation algorithm.
 */

PVOID   malloc_i (size_t szSize)
{
    P_INTERNMNG     pRequest,
                    pNextFree   = NULL;

    PVOID           pRet        = NULL;

    size_t          szInUse;

    /* The management already gone wrong? */
    if (glbMng.iError == ERROR_SUCCESS)
    {
        if (glbMng.ulReqsNum > glbMng.ulLastReq)
            /* So just pass the request ahead */
            goto fEnd;
    }
    else if (glbMng.iError != ERROR_WRONGREQ)
        /* So just pass the request ahead */
        goto fEnd;

    /* Is the first execution? */
    if (glbMng.ulReqsNum == 0)
    {
        PlanTheStrategy ();

        /* Case where was an error in the map loading */
        if (glbMng.iError)
        {
            goto fEnd;
        }
    }

    glbMng.ulReqsNum++;

    for (pRequest = glbMng.pRequests;
            pRequest != NULL; pRequest = pRequest->pNextAddrs)
    {
        /* This request isn't available anymore */
        if (pRequest->szReqSz == -1)
            continue;

        /* If the run plan yet is valid */
        if (glbMng.iError != ERROR_WRONGREQ)
        {
            /* Surely this request isn't managed */
            if (pRequest->ulID > glbMng.ulReqsNum)
                break;

            /* Store, sequentially, the space in use by the application */
            else if (pRequest->ulID < glbMng.ulReqsNum)
                szInUse = pRequest->szReqSz;

            /* We must store also the sequential next pointer
               not yet in use */
            if (pNextFree == NULL)
                pNextFree = (P_INTERNMNG) pRequest->pAddr;

            /* This request is being managed? */
            else if (glbMng.ulReqsNum == pRequest->ulID)
            {
                /* The request isn't the same we profiled */
                if (pRequest->szReqSz != szSize)
                    glbMng.iError = ERROR_WRONGREQ;
            }
        }

        /* If not, we'll try to use what we already requested. There's a
           great probability that all the requests profiled will
           be called at sometime by the application */
        if (pRequest->szReqSz == szSize)
        {
            /* Return the pre-allocated memory */
            pRet = pRequest->pAddr;

            break;
        }
    }

fEnd:
    if (pRet == NULL)
        pRet = malloc_o (szSize);
    else
    {
#if ( (! defined(_MSC_VER)) && defined(_DEBUG) )
        openlog ("prealloc", LOG_ODELAY, LOG_DEBUG);

        syslog (LOG_DEBUG, "%d;%d",
                    pRequest->ulID,
                    pRequest->szReqSz);

        closelog ();
#endif

        /* Signalize that this request is already in use */
        pRequest->szReqSz = -1;
    }

    /* In case if an error, we simply will pass the call ahead */
    if (glbMng.iError)
    {
        if (glbMng.iError == ERROR_MAPNOTFOUND)
        {
            WriteMapFile (pRet, szSize, TYPE_ALLOC);
        }
    }

    return (pRet);
}


/**
    \brief  Free's wrapper.

    This function will complete the work done by the malloc.
    Mainly this function will pass the pMem pointer to the
    system free's, watching to see if that pointer is beeing
    managed by the pre-allocator algorithm.
 */

void    free_i (PVOID pMem)
{
    P_INTERNMNG     pRequest    = NULL,
                    pBefore     = NULL;

    BYTE            bProcess    = FALSE;

    /* The management already gone wrong or there's nothing
       to free? */
    if ( ( (glbMng.iError) && (glbMng.iError != ERROR_WRONGREQ) ) ||
            (glbMng.pRequests == NULL) )
    {
        /* So just pass the request ahead */
        goto fEnd;
    }

    glbMng.ulReqsNum++;

    for (pRequest = glbMng.pRequests;
            pRequest != NULL; pBefore = pRequest,
                              pRequest = pRequest->pNextAddrs)
    {
        /* We found the request */
        if (pRequest->pAddr == pMem)
        {
            if (glbMng.iError == ERROR_SUCCESS)
            {
                /* Is this request the last managed? */
                if (pRequest->ulID == glbMng.ulLastReq)
                    bProcess = TRUE;
            }
            else
            {
                /* Update the previous next pointer */
                if ( (pBefore != NULL) && (pBefore->pNextAddrs) )
                    pBefore->pNextAddrs = pRequest->pNextAddrs;
                else
                    /* Indicate that the first request was already
                       freed by the application */
                    pRequest->szReqSz = 0;

                if ( (glbMng.pRequests->pNextAddrs == NULL) &&
                                (glbMng.pRequests->szReqSz == 0) )
                    bProcess = TRUE;
            }

            if (bProcess == TRUE)
            {
                free_o (glbMng.pRequests);

                glbMng.pRequests = NULL;

#if ( (! defined(_MSC_VER)) && defined(_DEBUG) )
                openlog ("free_i", LOG_ODELAY, LOG_DEBUG);

                syslog (LOG_DEBUG, "glbMng.pRequests freed");

                closelog ();
#endif
            }

            break;
        }
    }

fEnd:
    /* In case if an error, we simply will pass the call ahead */
    if (glbMng.iError != ERROR_SUCCESS)
    {
        if (glbMng.iError == ERROR_MAPNOTFOUND)
        {
            WriteMapFile (pMem, 0, TYPE_DEALLOC);
        }

        if ( (glbMng.iError != ERROR_WRONGREQ) && (pRequest == NULL) )
            free_o (pMem);
    }

    return;
}
