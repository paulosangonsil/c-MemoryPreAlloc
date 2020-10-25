/**
    \file
 */

#ifndef _prealloc_h
    #define                         _prealloc_h

    /** The minimum acceptable object life to be
        managed by the pre-allocator. */
    #define _LIFETIMEPERC           0.3

    /** The map file name (on disk). */
    #define MEMMAPFILENAME          "memmap.bin"

    /** Error on the memory pre-allocation. */
    #define ERROR_PREALLOC          (-1)
    /** The application changed its request sequence. */
    #define ERROR_WRONGREQ          (-2)

    /** The map file wasn't found. */
    #define ERROR_MAPNOTFOUND       (-3)
    /** There was an error when reading the map file. */
    #define ERROR_READINGMAP        (-4)
    /** There was an error when writing the map file. */
    #define ERROR_WRITINGMAP        (-5)

    /* For the non-MS compilers */
    #ifndef _MSC_VER
        #define _DEBUG

        /** Shortcut for void *. */
        typedef void *              PVOID;
        /** Shortcut for unsigned long type. */
        typedef unsigned long       ULONG;
        /** Shortcut for unsigned char type. */
        typedef unsigned char       BYTE;
        /** Shortcut for double type. */
        typedef double              DOUBLE;
        /** Shortcut for int type. */
        typedef int                 INT;
        /** Shortcut for int pointer type. */
        typedef INT *               PINT;
        /** Shortcut for char type. */
        typedef char               _TCHAR;

        /** Success execution. */
        #define ERROR_SUCCESS       0

        #define FALSE               0
        #define TRUE                1

        #define Sleep               usleep
        #define _tmain              main

        #define O_BINARY            0
        #define _S_IWRITE           0
    #endif

    #define malloc_o                malloc
    #define free_o                  free

    /** Types for bDataType argument of the WriteMapFile function. */
    enum    _enum_wmf_datatype
    {
        TYPE_ALLOC      = 0,    /**< Request is a allocation. */
        TYPE_DEALLOC    = 1     /**< Request is a deallocation. */
    };


    /** Structure for the malloc () request. */
    typedef
    struct  _req_alloc
    {
        clock_t tTime;  /**< Allocation request time - relative
                              to the process start. */

        size_t  szReqSz;/**< Request size. */

        PVOID   pAddr;  /**< Address to allocated. */
    }   REQALLOC, *P_REQALLOC;  /**< Pointer to the _req_alloc structure. */


    /** Structure for the free () request. */
    typedef
    struct  _req_dealloc
    {
        time_t  tTime;  /**< Deallocation request time - relative
                              to the process start. */

        PVOID   pAddr;  /**< Address to be deallocated. */
    }   REQDEALLOC, *P_REQDEALLOC;  /**< Pointer to the _req_dealloc structure. */


    /** Structure for the internal memory management - using
        the chain model. */
    typedef
    struct  _intern_mng
    {
        ULONG           ulID;           /**< Request sequential number. */

        PVOID           pAddr;          /**< Request address. */

        size_t          szReqSz;        /**< Request size. */

        struct
        _intern_mng     *pNextAddrs;    /**< Other request addresses
                                             coalesced with this one. */
    }   INTERNMNG, *P_INTERNMNG;        /**< Pointer to the _intern_mng structure. */


    /** Structure of the map file. */
    typedef
    struct  _file_header
    {
        ULONG   ulQtyRecs,      /**< The number of records in the file. */
                ulStatNum;      /**< Times the statistical algorithm
                                     runned in this process. */
    }   FILEHEADER, *P_FILEHEADER;  /**< Pointer to the _file_header structure. */


    /** Structure for storing the requests in the file. */
    typedef
    struct  _file_rec_disk
    {
        ULONG       ulID;           /**< Request sequential number. */

        union
        {
            BYTE        bType;      /**< Request type: allocation or
                                         deallocation - only
                                         valid before the SortFileRecs
                                         (BYLIFETIME) call. */

            time_t      tLifeTime;  /**< The object life time - only
                                         valid after the SortFileRecs
                                         (BYLIFETIME) call. */
        }   u;  /**< Type/Lifetime structure union. */

        time_t      tTime;          /**< Request time - relative
                                         to the process start. */

        size_t      szReqSz;        /**< Request size. */

        PVOID       pAddr;          /**< Address used in the request. */
    }   FILERECD, *P_FILERECD;      /**< Pointer to the _file_rec_disk structure. */


    /** Structure for manipulate in memory the requests
        stored in the file. */
    typedef
    struct  _file_rec
    {
        FILERECD    FileRec;    /**< The read request. */

        struct
        _file_rec   *pNextRec;  /**< Next record. */
    }   FILEREC,
        *P_FILEREC,     /**< Pointer to the _file_rec structure. */
        **PP_FILEREC;   /**< Pointer to pointer the _file_rec structure. */


    /** Structure for the global management. */
    typedef
    struct  _glb_mng
    {
        INT         iError;     /**< To indicate the last error. */

        ULONG       ulReqsNum,  /**< Requests sequential number:
                                     the last executed. */
                    ulLastReq,  /**< The last managed request. */
                    ulStatNum,  /**< Times the statistical algorithm
                                     runned in this process. */
                    ulProcDur,  /**< The process duration (relative to
                                     the last run). This will
                                     contain the last (more recent)
                                     request done. */
                    ulTotalMem; /**< The total amount allocated. */

        P_INTERNMNG pRequests;  /**< Next record. After the PlanStrategy ()
                                     execution, this will reflect the main
                                     address. */
    }   GLBMNG, *P_GLBMNG;      /**< Pointer to the _glb_mng structure. */

    PVOID   malloc_i (size_t);
    void    free_i (PVOID);

#endif
