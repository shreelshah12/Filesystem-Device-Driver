////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_cache.c
//  Description    : This is the implementation of the cache for the 
//                   FS3 filesystem interface.
//
//

// Includes
#include <cmpsc311_log.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Project Includes
#include <fs3_cache.h>
#include <fs3_controller.h>

//
// Support Macros/Data
FS3_CACHE* caches;
FS3_CACHE_SCORES scores;
int cacheSize = 0;
int currentLRUTime = 0;
int maxCacheLines;
//
// Implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_init_cache
// Description  : Initialize the cache with a fixed number of cache lines
//
// Inputs       : cachelines - the number of cache lines to include in cache
// Outputs      : 0 if successful, -1 if failure

int fs3_init_cache(uint16_t cachelines) {
    // Allocate space to cache
    caches = realloc(caches, cachelines*sizeof(FS3_CACHE));
    maxCacheLines = cachelines;
    if(caches == NULL){
        free(caches);
        logMessage( LOG_ERROR_LEVEL, "Caches allocation failed");
    }
    return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_close_cache
// Description  : Close the cache, freeing any buffers held in it
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int fs3_close_cache(void)  {
    free(caches);
    caches = NULL;
    return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_put_cache
// Description  : Put an element in the cache
//
// Inputs       : trk - the track number of the sector to put in cache
//                sct - the sector number of the sector to put in cache
// Outputs      : 0 if inserted, -1 if not inserted

int fs3_put_cache(FS3TrackIndex trk, FS3SectorIndex sct, void *buf) {
    int LRUTime = caches[0].CTIME;
    int targetIndex = 0;
    //iterate through cache to find the specified cache or the cache entry with the LRU
    for(int x = 0; x<cacheSize; x++){
        if(trk == caches[x].CTRACK && sct == caches[x].CSECTOR){
            //Update the cache entry if needed
            if(memcmp(caches[x].CDATA, buf, FS3_SECTOR_SIZE) != 0){
                memcpy(caches[x].CDATA, buf, FS3_SECTOR_SIZE);           
            }
            return(0);
        }
        int lastLRUTime = LRUTime;
        LRUTime = fmin(LRUTime, caches[x].CTIME);

        if(lastLRUTime != LRUTime){
            targetIndex = x;
        }
    }
    //If for loop finishes and method is still running, then entry was not found in cache
    //Insert new cache record if cache is not full
    if(cacheSize<maxCacheLines){
        caches[cacheSize].CSECTOR = sct;
        caches[cacheSize].CTRACK = trk;
        memcpy(caches[cacheSize].CDATA, buf, FS3_SECTOR_SIZE);
        caches[cacheSize].CTIME = currentLRUTime;
        currentLRUTime++;
        cacheSize++;
    }else{
    //Replace the cache record with the LRU with new cache record
        caches[targetIndex].CSECTOR = sct;
        caches[targetIndex].CTRACK = trk;
        memcpy(caches[targetIndex].CDATA, buf, FS3_SECTOR_SIZE);
        caches[targetIndex].CTIME = currentLRUTime;
        currentLRUTime++;
    }
    return(-1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_get_cache
// Description  : Get an element from the cache (
//
// Inputs       : trk - the track number of the sector to find
//                sct - the sector number of the sector to find
// Outputs      : returns NULL if not found or failed, pointer to buffer if found

void * fs3_get_cache(FS3TrackIndex trk, FS3SectorIndex sct)  {
    //Iterate through cache to find matching cache with track and sector
    for(int x = 0; x<cacheSize; x++){
        if(trk == caches[x].CTRACK && sct == caches[x].CSECTOR){
            return(caches[x].CDATA);
        }
    }
    return( NULL );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_log_cache_metrics
// Description  : Log the metrics for the cache 
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int fs3_log_cache_metrics(void) {
    printf("This program registered %ld hits and %ld misses. Hit/Miss ratio: %f\n",
     scores.FS3_HITS, scores.FS3_MISSES, (float)scores.FS3_HITS/(scores.FS3_MISSES + scores.FS3_HITS));
    return(0);
}