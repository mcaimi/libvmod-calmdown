/*
 *   Token-Bucket queue.
 *   Handles multiple buckets in a linked list
 */

#include "tokenbucket.h"

// allocate bucket
bucket *allocateBucket(unsigned char *key, unsigned char *requester, unsigned char *request, unsigned int digest_len, double hitRatio, double bucketCapacity) {
  bucket *newItem = NULL;

  // allocate memory for new bucket
  newItem = (struct __bucketItem *)malloc(sizeof(struct __bucketItem));
  if (newItem == NULL)
     return NULL;

  #ifdef DEBUG_BUCKETQUEUE
    printf("%s: 0x%X\n","allocateBucket(): Bucket allocated at", newItem);
  #endif

  // fill in data into new bucket
  newItem->capacity = bucketCapacity;
  newItem->ratio = hitRatio;
  newItem->objectDigest = (unsigned char *)malloc((digest_len*sizeof(char)) + 1);
  if (newItem->objectDigest == NULL) {

    #ifdef DEBUG_BUCKETQUEUE
      printf("%s: 0x%X\n","allocateBucket(): Failed to allocate memory for objectDigest", newItem);
    #endif

    freeBucket(newItem);
    return NULL;
  }

  bzero(newItem->objectDigest, digest_len + 1);
  memcpy(newItem->objectDigest, key, digest_len);

  newItem->requester = (unsigned char*)malloc(strlen(requester) + 1);
  newItem->resource = (unsigned char*)malloc(strlen(resource) + 1);
  if ((newItem->requester == NULL) || (newItem->resource == NULL)) {
    #ifdef DEBUG_BUCKETQUEUE
      printf("%s: 0x%X\n","allocateBucket(): Failed to allocate memory for request/resource data", newItem);
    #endif

    freeBucket(newItem);
    return NULL;
  }

  bzero(newItem->requester, strlen(requester) + 1);
  memcpy(newItem->requester, requester, strlen(requester));

  bzero(newItem->resource, strlen(resource) + 1);
  memcpy(newItem->resource, resource, strlen(resource));

  #ifdef DEBUG_BUCKETQUEUE
    printf("%s: %s\n","allocateBucket(): objectDigest value is ", newItem->objectDigest);
  #endif

  // adjust pointers
  newItem->nextBucket = NULL;
  newItem->prevBucket = NULL;

  // return data
  return (struct __bucketItem *)newItem;
}

// free bucket
void freeBucket(bucket *item) {
  if (item != NULL) {
    // free digest memory
    if (item->objectDigest != NULL) {

    #ifdef DEBUG_BUCKETQUEUE
      printf("%s: 0x%X\n","freeBucket(): Deallocating Object Hash", item->objectDigest);
    #endif

      free(item->objectDigest);
      item->objectDigest = NULL;
    }
    if (item->requester != NULL) {

    #ifdef DEBUG_BUCKETQUEUE
      printf("%s: 0x%X\n","freeBucket(): Deallocating requester", item->requester);
    #endif

      free(item->requester);
      item->requester = NULL;
    }
    if (item->resource != NULL) {

    #ifdef DEBUG_BUCKETQUEUE
      printf("%s: 0x%X\n","freeBucket(): Deallocating resource", item->resource);
    #endif

      free(item->resource);
      item->resource = NULL;
    }

    // free the rest
    #ifdef DEBUG_BUCKETQUEUE
      printf("%s: 0x%X\n","freeBucket(): Deallocating Bucket", item);
    #endif

    free(item);
  }

  // no more things to do
  return;
}

// remove bucket
void removeBucket(bucket *item) {
  // check if NULL
  if (item != NULL) {
    // fix connections
    if (item->prevBucket != NULL) {

      #ifdef DEBUG_BUCKETQUEUE
        printf("%s: 0x%X with 0x%X\n","removeBucket(): Linking object ", (item->prevBucket)->nextBucket, item->nextBucket);
      #endif

      (item->prevBucket)->nextBucket = item->nextBucket;

      // free item
      freeBucket(item);
    } else {
      // free item
      freeBucket(item);
      item = NULL;
    }
  }
}

// search bucket
bucket *searchBucket(bucket *headOfQueue, unsigned char *key, unsigned int keylen) {
  bucket *holder = headOfQueue;

  // iterate
  while (holder != NULL) {
    if (memcmp(holder->objectDigest, key, keylen) == 0) {
      // found!
      #ifdef DEBUG_BUCKETQUEUE
        printf("%s: 0x%X\n","searchBucket(): Found Bucket at", holder);
      #endif
      return holder;
    }

    // advance...
    #ifdef DEBUG_BUCKETQUEUE
        printf("-->%s: 0x%X\n","searchBucket(): recursing at address", holder->nextBucket);
    #endif
    holder = holder->nextBucket;
  }

  // not found
  #ifdef DEBUG_BUCKETQUEUE
        printf("%s\n","searchBucket(): Bucket is NULL, maybe the first one?");
  #endif
  return holder;
}

// get last bucket
bucket *lastBucket(bucket *headOfQueue) {
  if (headOfQueue == NULL) {
    return NULL;
  }
  else if (headOfQueue->nextBucket == NULL) {

    #ifdef DEBUG_BUCKETQUEUE
        printf("%s: 0x%X\n","lastBucket(): Last item in queue is at", headOfQueue);
    #endif

    return headOfQueue;
  }
  else return lastBucket(headOfQueue->nextBucket);
}

// add bucket to queue
bucket *addBucket(bucket *item, bucket *headOfQueue) {
  if (headOfQueue == NULL) {
    // ad first item
    #ifdef DEBUG_BUCKETQUEUE
        printf("%s: 0x%X\n","addBucket(): adding new bucket at address", item);
    #endif
    headOfQueue = item;
  } else {
    // search last item in queue
    bucket *lastInQueue = lastBucket(headOfQueue);

    #ifdef DEBUG_BUCKETQUEUE
        printf("%s: 0x%X after 0x%X\n","addBucket(): adding new bucket at address", item, lastInQueue);
    #endif

    // add bucket in place
    lastInQueue->nextBucket = item;
    item->prevBucket = lastInQueue;
  }

  return headOfQueue;
}

// destroy queue (CAUTION!)
void freeBucketQueue(bucket *headOfQueue) {
  if (headOfQueue->nextBucket != NULL) {

    #ifdef DEBUG_BUCKETQUEUE
        printf("--> %s: 0x%X\n","freeBucketQueue(): recursing at address", headOfQueue);
    #endif

    freeBucketQueue(headOfQueue->nextBucket);
  }

  // free resources
  #ifdef DEBUG_BUCKETQUEUE
    printf("%s: 0x%X\n","freeBucketQueue(): freeing bucket at address", headOfQueue);
  #endif
  freeBucket(headOfQueue);
}

// garbage collector function
void cleanBucketQueue(bucket *headOfQueue, double timestamp) {
  bucket *previous;
  bucket *next;
  bucket *holder = headOfQueue;

  #ifdef DEBUG_BUCKETQUEUE
    printf("cleanBucketQueue(): Starting list walk from address: 0x%X\n", holder);
  #endif

  // remove dead entries
  while (holder != NULL) {
    #ifdef DEBUG_BUCKETQUEUE
      printf("cleanBucketQueue(): Head Of Queue is not NULL...\n");
      printf("cleanBucketQueue(): ratio %f, timedelta %f, tokens %f, capacity %f\n", holder->ratio, (timestamp - holder->lastAccess), holder->tokens, holder->capacity);
    #endif

    if (timestamp - holder->lastAccess > holder->capacity) {
      // rewire the bucket queue
      previous = holder->prevBucket;
      next = holder->nextBucket;
      // ok, do it.
      #ifdef DEBUG_BUCKETQUEUE
        printf("%s: new NEXT: 0x%X, new PREV: 0x%X\n","cleanBucketQueue(): Rewiring addresses:", next, previous);
      #endif

      if (previous != NULL) {
        if (previous->nextBucket != NULL) previous->nextBucket = next;
      }

      if (next != NULL) {
       if (next->prevBucket != NULL) next->prevBucket = previous;
      }

      #ifdef DEBUG_BUCKETQUEUE
        printf("%s: 0x%X\n","cleanBucketQueue(): freeing old bucket at address", holder);
      #endif
      // free bucket
      freeBucket(holder);
    }

    // advance...
    #ifdef DEBUG_BUCKETQUEUE
        printf("-->%s: 0x%X\n","cleanBucketQueue(): recursing at address", holder->nextBucket);
    #endif
    holder = holder->nextBucket;
  }
}
