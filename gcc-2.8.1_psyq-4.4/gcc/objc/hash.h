/* -*-c-*-
 * This is a general purpose hash object.
 *
 * The hash object used throughout the run-time
 *  is an integer hash.  The key and data is of type
 *  void*.  The hashing function converts the key to
 *  an integer and computes it hash value.
 *
  $Header$
  $Author$
  $Date$
  $Log$
*/
 

#ifndef _hash_INCLUDE_GNU
#define _hash_INCLUDE_GNU

                                                /* If someone is using a c++
                                                  compiler then adjust the 
                                                  types in the file back 
                                                  to C. */
#ifdef __cplusplus
extern "C" {
#endif

#include  <sys/types.h>


/*
 * This data structure is used to hold items
 *  stored in a hash table.  Each node holds 
 *  a key/value pair.
 *
 * Items in the cache are really of type void*.
 */
typedef struct cache_node {
  struct cache_node*  nextNode;                   /* Pointer to next entry on
                                                    the list.  NULL indicates
                                                    end of list. */
  void*               theKey;                     /* Key used to locate the
                                                    value.  Used to locate
                                                    value when more than one
                                                    key computes the same hash
                                                    value. */
  void*               theValue;                   /* Value stored for the
                                                    key. */
} CacheNode, *CacheNode_t;


/*
 * This data structure is the cache.
 *
 * It must be passed to all of the hashing routines
 *  (except for new).
 */
typedef struct cache {
  /*
   * Variables used to implement the
   *  hash itself.
   */
  CacheNode_t (* theNodeTable )[];                /* Pointer to an array of
                                                    hash nodes. */
  u_int       numberOfBuckets,                    /* Number of buckets 
                                                    allocated for the hash
                                                    table (number of array
                                                    entries allocated for
                                                    "theCache"). */
              mask,                               /* Mask used when computing
                                                    a hash value.  The number
                                                    of bits set in the mask
                                                    is contained in the next
                                                    member variable. */
              numberOfMaskBits;                   /* Number of bits used for
                                                    the mask.  Useful for 
                                                    efficient hash value
                                                    calculation. */
  /*
   * Variables used to implement indexing
   *  through the hash table.
   */
  u_int       lastBucket;                         /* Tracks which entry in the
                                                    array where the last value
                                                    was returned. */
} Cache, *Cache_t;


                                                /* Prototypes for hash
                                                  functions. */
                                                /* Allocate and initialize 
                                                  a hash table.  Hash table 
                                                  size taken as a parameter. 
                                                    A value of 0 is not 
                                                  allowed. */ 
Cache_t hash_new( u_int numberOfBuckets );
                                                /* Deallocate all of the
                                                  hash nodes and the cache
                                                  itself. */
void hash_delete( Cache_t theCache );
                                                /* Add the key/value pair
                                                  to the hash table.  assert()
                                                  if the key is already in
                                                  the hash. */
void hash_add( Cache_t theCache, void* aKey, void* aValue );
                                                /* Remove the key/value pair
                                                  from the hash table.  
                                                  assert() if the key isn't 
                                                  in the table. */
void hash_remove( Cache_t theCache, void* aKey );
                                                /* Given key, return its 
                                                  value.  Return NULL if the
                                                  key/value pair isn't in
                                                  the hash. */
void* hash_value_for_key( Cache_t theCache, void* aKey );
                                                /* Used to index through the
                                                  hash table.  Start with NULL
                                                  to get the first entry.
                                                  
                                                  Successive calls pass the
                                                  value returned previously.
                                                  ** Don't modify the hash
                                                  during this operation *** 
                                                  
                                                  Cache nodes are returned
                                                  such that key or value can
                                                  ber extracted. */
CacheNode_t hash_next( Cache_t theCache, CacheNode_t aCacheNode );


#ifdef __cplusplus
}
#endif

#endif
