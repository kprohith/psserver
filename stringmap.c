// stringmap.c
// Author: Rohith Kotia Palakirti

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

/*
* Struct Definitions
*/

/* StringMapItem Struct
* -----------------------------------------------
* Structure to hold the key and item value of each StringMap element
*
* key: pointer to key of element
* item: pointer to value stored at the key
*/
typedef struct StringMapItem {
    char* key;
    void* item;
} StringMapItem;

/* StringMap Struct
* -----------------------------------------------
* Structure to hold a list of StringMapItem
*
* sm: list holding multiple StringMapItem entries
* count: total count of entries in sm
*/
typedef struct StringMap {
    StringMapItem** sm;
    int count;
} StringMap;

/*
 * Function Prototypes
 */
StringMap* stringmap_init(void);
void stringmap_free(StringMap* sm);
void* stringmap_search(StringMap* sm, char* key);
int stringmap_add(StringMap* sm, char* key, void* item);
int stringmap_remove(StringMap* sm, char* key);
StringMapItem* stringmap_iterate(StringMap* sm, StringMapItem* prev);

/* StringMap* stringmap_init(void)
* -----------------------------------------------
* Allocate, initialise and return a new, empty StringMap
*
* Returns: a newly created StringMap
*/
StringMap* stringmap_init(void) {
    StringMap* sm = malloc(sizeof(StringMap));
    sm->sm = malloc(sizeof(StringMapItem *));
    sm->count = 0;
    return sm;
}

/* void stringmap_free(StringMap* sm)
* -----------------------------------------------
* Free all memory associated with a StringMap.
* frees stored key strings but does not free() the (void *)item pointers
* in each StringMapItem. Does nothing if sm is NULL.
*
* sm: StringMap to be freed
*/
void stringmap_free(StringMap* sm) {
    if (sm == NULL) {
        return;
    }
    for (int i = 0; i < sm->count; i++) {
        free(sm->sm[i]->key);
        free(sm->sm[i]);
    }
    free(sm->sm);
    free(sm);
}

/* void* stringmap_search(StringMap* sm, char* key)
* -----------------------------------------------
* Search a stringmap for a given key, returning a pointer to the entry
* if found, else NULL. If not found or sm is NULL or key is NULL 
* then returns NULL.
*
* sm: StringMap to be searched
* key: entry that is to be looked up
*
* Returns: NULL is key is not found, pointer to entry if found
*/
void* stringmap_search(StringMap* sm, char* key) {
    if (sm == NULL || key == NULL) {
        return NULL;
    }
    for (int i = 0; i < sm->count; i++) {
        if (strcmp(sm->sm[i]->key, key) == 0) {
            return sm->sm[i]->item;
        }
    }
    return NULL;
}

/* int stringmap_add(StringMap* sm, char* key, void* item)
* -----------------------------------------------
* Add an item into the stringmap, return 1 if success else 0 (e.g. an item
* with that key is already  present or any one of the arguments is NULL).
* The 'key' string is copied before being stored in the stringmap.
* The item pointer is stored as-is, no attempt is made to copy its contents.
*
* Returns: 1 on success, 0 on failure (duplicate entry or invalid sm)
*/
int stringmap_add(StringMap* sm, char* key, void* item) {
    if (sm == NULL || key == NULL || item == NULL) {
        return 0;
    }
    if (stringmap_search(sm, key) != NULL) {
        return 0;
    }
    StringMapItem* smi = malloc(sizeof(StringMapItem));
    smi->key = malloc(sizeof(char) * (strlen(key) + 1));
    strcpy(smi->key, key);
    smi->item = item;
    sm->sm = realloc(sm->sm, sizeof(StringMapItem *) * (sm->count + 1));
    sm->sm[sm->count] = smi;
    sm->count++;
    return 1;
}

/* int stringmap_remove(StringMap* sm, char* key)
* -----------------------------------------------
* Initiates and runs the program
*
* Removes an entry from a stringmap
* free()s the StringMapItem and the copied key string, but not
* the item pointer.
*
* Returns: 1 if success else 0 (e.g. item not present or any argument is NULL)
*/
int stringmap_remove(StringMap* sm, char* key) {
    if (sm == NULL || key == NULL) {
        return 0;
    }
    for (int i = 0; i < sm->count; i++) {
        if (strcmp(sm->sm[i]->key, key) == 0) {
            free(sm->sm[i]->key);
            free(sm->sm[i]);
            for (int j = i; j < sm->count - 1; j++) {
                sm->sm[j] = sm->sm[j + 1];
            }
            sm->count--;
            sm->sm = realloc(sm->sm, sizeof(StringMapItem *) * sm->count);
            return 1;
        }
    }
    return 0;
}

/* StringMapItem* stringmap_iterate(StringMap* sm, StringMapItem* prev)
* -----------------------------------------------
* Iterate through the stringmap - if prev is NULL then the first entry is
* returned otherwise prev should be a value returned from a previous call
* to stringmap_iterate() and the "next" entry will be returned.
* This operation is not thread-safe - any changes to the stringmap between
* successive calls to stringmap_iterate may result in undefined behaviour.
* Returns NULL if no more items to examine or sm is NULL.
* There is no expectation that items are returned in a particular order (i.e.
* the order does not have to be the same order in which items were added).
*
* sm: StringMap to iterate over
* prev: previous StringMapItem
*
* Returns: next StringMapItem* if present, else NULL
*/
StringMapItem* stringmap_iterate(StringMap* sm, StringMapItem* prev) {
    if (sm == NULL) {
        return NULL;
    }
    if (prev == NULL) {
        return sm->sm[0];
    }
    for (int i = 0; i < sm->count; i++) {
        if (sm->sm[i] == prev) {
            if (i == sm->count - 1) {
                return NULL;
            }
            return sm->sm[i + 1];
        }
    }
    return NULL;
}
