// buildstats.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "../prealloc/prealloc.c"

#define     _STATSFILE      "memstats.txt"

P_FILEREC   pGlobal     = NULL;


void    writeStatsFile (PCHAR pTxt)
{
    static
    INT             statsFile       = 0;

    if (statsFile == 0)
    {
        statsFile = open (_STATSFILE, O_WRONLY |
                        O_CREAT | O_TRUNC | O_BINARY, _S_IWRITE);

        if (statsFile == 0)
        {
            printf ("writeStatsFile: erro ao abrir o arquivo p/ escrita\n");

            goto fEnd;
        }
    }

    write ( statsFile, pTxt, strlen (pTxt) );

fEnd:
    return;
}

void    dumpMemMapFile (void)
{
    #define     MAXLINE     100 + 2

    CHAR        pLine [MAXLINE] = {0};

    P_FILEREC   pFileRec        = NULL;

    INT         iRet;

    /* Get the application allocation behavior */
    iRet = ReadMapFile (&pFileRec);

    /* There was an error on the read */
    if (pFileRec == NULL)
    {
        printf ("dumpMemMapFile: erro na ReadMapFile\n");

        goto fEnd;
    }

    for (pGlobal = pFileRec; pFileRec != NULL; pFileRec = pFileRec->pNextRec)
    {
        sprintf (&pLine [0], "ulID: %d\x0D\x0A  Type: %s\x0D\x0A",
                    pFileRec->FileRec.ulID,
                    (pFileRec->FileRec.u.bType == TYPE_DEALLOC) ? "Dealloc" : "Alloc");
        writeStatsFile (&pLine [0]);

        sprintf (&pLine [0], "  Time: %d\x0D\x0A  Size: %d\x0D\x0A  Addr: 0x%X\x0D\x0A",
                    pFileRec->FileRec.tTime,
                    pFileRec->FileRec.szReqSz,
                    pFileRec->FileRec.pAddr);
        writeStatsFile (&pLine [0]);
    }

    iRet = SortFileRecs (&pGlobal, (BYTE) FIRSTSORT);

    if (iRet != ERROR_SUCCESS)
    {
        printf ("dumpMemMapFile: erro na SortFileRecs\n");

        goto fEnd;
    }

    writeStatsFile ("\x0D\x0Aid;hora_malloc;hora_free;tempo_vida;tam\x0D\x0A");
    for (pFileRec = pGlobal; pFileRec != NULL; pFileRec = pFileRec->pNextRec)
    {
        sprintf (&pLine [0], "%d;%d;%d;%d;%d\x0D\x0A",
                    pFileRec->FileRec.ulID,
                    pFileRec->FileRec.tTime,
                    pFileRec->FileRec.tTime + pFileRec->FileRec.u.tLifeTime,
                    pFileRec->FileRec.u.tLifeTime,
                    pFileRec->FileRec.szReqSz);
        writeStatsFile (&pLine [0]);
    }

fEnd:
    #undef      MAXLINE
    return;
}

#define     MAXHOLE     29000

typedef
struct  _free_hole
{
    BYTE        bUsed;

    UINT        uSz,
                uInUse;
}   FREEHOLE, *P_FREEHOLE, **PP_FREEHOLE;  /**< Pointer to the _req_alloc structure. */

INT     remFreeHoles (P_FREEHOLE pList, INT iSz, PINT pFrag)
{
    INT     iCount,
            iHolesUsed   = 0;

    for (iCount = 0; iCount < MAXHOLE; iCount++)
    {
        if ( (pList [iCount].bUsed == TRUE) &&
                    (pList [iCount].uInUse == 0) )
        {
            iHolesUsed++;

            if (pList [iCount].uSz >= iSz)
            {
                pList [iCount].uInUse = iSz;

                break;
            }

            *pFrag += pList [iCount].uSz;
        }
    }

    if ( (iCount == MAXHOLE) && (iHolesUsed) )
        iCount = TRUE; // The req caused the frag
    else
        iCount  = FALSE;

    return (iCount);
}

size_t  szReq (PVOID pAddr)
{
    P_FILEREC   pFileRec;

    for (pFileRec = pGlobal; pFileRec != NULL;
                        pFileRec = pFileRec->pNextRec)
    {
        if ( (pFileRec->FileRec.u.bType == TYPE_ALLOC) &&
             (pFileRec->FileRec.pAddr == pAddr) )
            break;
    }

    return ( (pFileRec) ? pFileRec->FileRec.szReqSz : 0 );
}

void    insFreeHoles (P_FREEHOLE pList, INT iSz)
{
    INT     iCount;

    for (iCount = 0; iCount < MAXHOLE; iCount++)
    {
        if (pList [iCount].uInUse == iSz)
        {
            pList [iCount].uInUse = 0;

            break;
        }
    }

    if (iCount == MAXHOLE)
    {
        for (iCount = 0; iCount < MAXHOLE; iCount++)
        {
            if (pList [iCount].bUsed == FALSE)
            {
                pList [iCount].bUsed = TRUE;

                pList [iCount].uSz = iSz;

                break;
            }
        }
    }

    return;
}

void    buildFragStats (void)
{
    INT         iRet,
                iFrag,
                iTotal                  = 0;

    #define     MAXLINE     100 + 2

    CHAR        pLine [MAXLINE]         = {0},
                *pLast                  = &pLine [67];

    FREEHOLE    pFreeHoles  [MAXHOLE]   = {0};

    P_FILEREC   pFileRec                = NULL;

    FLOAT       fFrag;

    /* Get the application allocation behavior */
    iRet = ReadMapFile (&pFileRec);

    /* There was an error on the read */
    if (pFileRec == NULL)
    {
        printf ("dumpMemMapFile: erro na ReadMapFile\n");

        goto fEnd;
    }

    for (pGlobal = pFileRec; pFileRec != NULL;
                        pFileRec = pFileRec->pNextRec)
    {
        switch (pFileRec->FileRec.u.bType)
        {
            case TYPE_DEALLOC:
            {
                iRet = szReq (pFileRec->FileRec.pAddr);

                if (iRet)
                {
                    iTotal -= iRet;

                    insFreeHoles (pFreeHoles, iRet);
                }

                break;
            }

            default:
            case TYPE_ALLOC:
            {
                iFrag = 0;

                iRet = remFreeHoles (pFreeHoles,
                        pFileRec->FileRec.szReqSz, &iFrag);

                iTotal += pFileRec->FileRec.szReqSz;

                if (iRet)
                {
                    if (pFileRec->FileRec.szReqSz >= iFrag)
                        break;

                    fFrag = (FLOAT) ( (FLOAT) iFrag/*pFileRec->FileRec.szReqSz*/ / (FLOAT) iTotal/*iFrag*/ );

                    sprintf (&pLine [0], "%.4f;", fFrag);

                    if ( (strcmp (&pLine [0], "0.0000;") != 0) &&
                        ( (strlen (pLast) == 0) ||
                            ( strcmp (&pLine [0], pLast) ) ) )
                    /*if ( (pFileRec->FileRec.ulID > 608) &&
                        (pFileRec->FileRec.ulID < 765) )*/
                    {
                        strcpy (pLast, &pLine [0]);

                        sprintf (
                            (PSTR) ( (ULONG) &pLine [0] + strlen (&pLine [0]) ),
                                    "%d;\x0D\x0A", pFileRec->FileRec.ulID);

                        writeStatsFile (&pLine [0]);
                    }
                }

                break;
            }
        }
    }

fEnd:
    #undef      MAXLINE
    return;
}

#undef      MAXHOLE

INT main (INT argc, PCHAR argv[])
{
//    dumpMemMapFile ();
    buildFragStats ();
    return 0;
}
