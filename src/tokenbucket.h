/*
 *   Token-Bucket queue.
 *   Handles multiple buckets in a linked list
 */

// libc includes
#include <stdlib.h>
#ifdef DEBUG_BUCKETQUEUE
  #include <stdio.h>
#endif

// varnish includes
#include <cache/cache.h>
#include <vsha256.h>

/*
 *  A bucket item.
 *  This structure wraps a single call from outside Varnish
 *  and keeps track of how many requests are made per second
 *  per caller IP address.
 */
struct __bucketItem {
  // object hash. needed to select the correct bucket list
  unsigned char *objectDigest;
  // bucket capacity
  double capacity;
  // bucket hit ratio;
  double ratio;
  // last accessed timestamp
  double lastAccess;
  // resource requester
  unsigned char *requester;
  // requested resource that we want to rate-limit
  unsigned char *resource;
  // num tokens
  double tokens;
  // next item
  struct __bucketItem *nextBucket;
  // previous item
  struct __bucketItem *prevBucket;
};

typedef struct __bucketItem bucket;

/*
 * Function prototypes.
 */

// allocate a new bucket in memory
bucket *allocateBucket(unsigned char *key, unsigned char*requester, unsigned char*resource, unsigned int digest_len, double bucketCapacity, double hitRatio);

// free a bucket and release memory
void freeBucket(bucket *item);

// remove bucket from the linked list
void removeBucket(bucket *item);

// search bucket in the linked list
bucket *searchBucket(bucket *headOfQueue, unsigned char *key, unsigned int keylen);

// add bucket to the linked list
bucket *addBucket(bucket *item, bucket *headOfQueue);

// destroy queue (CAUTION! this completely frees all entries in the linked list)
void freeBucketQueue(bucket *headOfQueue);

// garbage collector function
void cleanBucketQueue(bucket *headOfQueue, double timestamp);

