/* Copyright (c) 2007-2014 Sam Trenholme
 *
 * TERMS
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * This software is provided 'as is' with no guarantees of correctness or
 * fitness for purpose.
 */

#include "DwStr.h"
#include "DwStr_functions.h"
#include "DwRadioGatun.h"
#include "DwMararc.h"
#include "DwSys.h"
#include <stdint.h>
#include <stdio.h> /* For reading and writing the hash to a file */
#include <time.h>

#include "DwHash.h"
/* The default value for the multiply constant is a 31-bit random prime
 * number; this number is generated by the RandomPrime program, whose
 * output is the file DwRandPrime.h */
#include "DwRandPrime.h"

uint32_t add_constant = 0x74fc65a2;

/* Mararc (OK, dwood2rc) parameters that are set in DwMararc.c */
extern int32_t key_n[];

/* Mararc parameters */
int32_t cache_size = 0;

/* Timestamp */
extern int64_t the_time;

/* Functions for hashing DwStr objects, and using said hash to store
 * information in a table */

/* Called before reading dwood3rc, this sets add_constant based on
 * secret.txt in Windows and /dev/urandom in Unix */
void set_add_constant() {
        FILE *in = 0;
        dwr_rg *quick_n_dirty = 0;
        dw_str *seedit = 0;
        int counter = 0;
        time_t timestamp = 0;

#ifdef MINGW
        in = fopen("secret.txt","rb");
#else
        in = fopen("/dev/urandom","rb");
#endif /* MINGW */

        if(in == 0) {
                goto catch_set_add_constant;
        }

        seedit = dw_create(14);

        if(seedit == 0) {
                goto catch_set_add_constant;
        }

        seedit->len = 11;
        for(counter = 0; counter < 8; counter++) {
                *(seedit->str + counter) = getc(in);
        }
        timestamp = time(0);
        *(seedit->str + 8) = (timestamp & 0xff);
        *(seedit->str + 9) = (timestamp & 0xff00) >> 8;
        *(seedit->str + 10) = (timestamp & 0xff0000) >> 16;

        quick_n_dirty = dwr_init_rg(seedit);
        dw_destroy(seedit);
        if(quick_n_dirty == 0) {
                goto catch_set_add_constant;
        }

        add_constant ^= dwr_rng(quick_n_dirty);
        add_constant ^= (dwr_rng(quick_n_dirty) << 15);
        dwr_zap(quick_n_dirty);
        fclose(in);

        return;

catch_set_add_constant:
#ifndef MINGW
        printf("Warning: Can not set add_constant\n");
#endif
        if(in != 0) {
                fclose(in);
        }
        return;
}

/* Set global variables based on dwood3rc parameters */
void dwh_process_mararc_params() {
        int32_t possible_magic_number;

        cache_size = get_key_n(DWM_N_maximum_cache_elements,32,16777216,-1);

        /* Since magic_number is set in DwRandPrime.h, we reset it from
         * dwood2rc this way */
        possible_magic_number = key_n[DWM_N_hash_magic_number];
        if(possible_magic_number > 0x40000000) {
                dw_log_string("Warning: hash_magic_number disabled",0);
        }
        dw_log_hex("add_constant is set to 0x",add_constant,10);

}

/* Quick and dirty hash compressor; this quickly makes a random-looking
 * 32-bit number based on the contents of the string obj */
uint32_t dwh_hash_compress(dw_str *obj) {
        uint32_t out = 0;
        uint32_t tmp = 0;

        int offset = 0;

        if(dw_assert_sanity(obj) == -1) {
                return 0;
        }

        while(offset < obj->len) {
                /* Make four bytes of the DwStr a number starting at the
                 * given offset; should the offset push us past the end
                 * of the string, pad with 0s; offset of 0 is top of string.
                 * This is done inline because it makes the hash 30%-75%
                 * faster compared to a separate function */
                if(offset + 3 < obj->len) {
                        tmp = (*(obj->str + offset   ) << 24) |
                              (*(obj->str + offset + 1) << 16) |
                              (*(obj->str + offset + 2) <<  8) |
                              *(obj->str + offset + 3);
                } else {
                        tmp = *(obj->str + offset) << 24;
                        if(offset + 1 < obj->len) {
                                tmp |= *(obj->str + offset + 1) << 16;
                                if(offset + 2 < obj->len) {
                                        tmp |= *(obj->str + offset + 2) << 8;
                                }
                        }
                }
                /* Done getting 4 bytes from string */
                out ^= tmp;
                out *= MUL_CONSTANT;
                out += add_constant;
                out = (out >> 19) | (out << 13);
                offset += 4;
        }

        out ^= obj->len;
        out *= MUL_CONSTANT;
        out += add_constant;
        out = (out >> 19) | (out << 13);

#ifdef HASH_DEBUG
        dw_stdout(obj);
        printf("%08x\n",out);
#endif /* HASH_DEBUG */

        return out;
}

/* Zap (destroy) a created hash; this assumes the hash has no elements
 * to be freed; this does *not* destroy the elements because it's used
 * in dwh_hash_autogrow (where we create a new hash and move the old
 * elements to the new hash)
 */

void dwh_hash_zap(dw_hash *hash) {
        if(hash == 0) {
                return;
        }
        if(hash->hash != 0) {
                free(hash->hash);
        }
        free(hash);
}

/* Completely deallocate a hash, including destroying all elements in the
 * hash. */

void dwh_hash_nuke(dw_hash *hash) {
        int32_t count = 0, noloop = 0;
        dw_element *point = 0, *next = 0;

        /* Iterate through hash; get rid of everything */
        for(count = 0; count <= hash->mask; count++) {
                point = hash->hash[count];
                for(noloop = 0; noloop < 10000 && point != 0; noloop++) {
                        next = point->next;
                        if(point->fila != 0) {
                                free(point->fila);
                        }
                        dw_destroy(point->key);
                        dw_destroy(point->value);
                        free(point);
                        point = next;
                }
        }
        dwh_hash_zap(hash);
}

/* Create a new hash with "elements" number of elements */
dw_hash *dwh_hash_init(uint32_t elements) {
        uint32_t power_two = 1;
        uint32_t a = 0;
        dw_hash *out;
        dw_element **list;

        if(elements == 0 && cache_size > 0) {
                elements = cache_size;
        } else if(elements < 2) {
                elements = 2;
        } else if(elements > 134217728) { /* 2 ** 27 */
                elements = 134217728;
        }

        /* Determine the mask for indexing hash elements */
        a = elements;
        while(a > 0) {
                power_two <<= 1;
                a >>= 1;
        }

        /* This is deallocated in the destructor for dw_hash */
        list = calloc((power_two + 1),sizeof(dw_element *));
        if(list == 0) {
                return 0;
        }

        out = dw_malloc(sizeof(dw_hash));
        if(out == 0) {
                free(list);
                return 0;
        }

        out->fila = 0;
        out->hash = list;
        power_two--; /* So it's a usable mask */
        out->mask = power_two;
        out->size = 0;
        out->max = elements;

        return out;
}

/* Given a hash, and pointer to a (newly created) element in the hash, add
 * that element to the top of the fila queue.  If the fila queue is empty,
 * initalize the fila queue */
int dwh_fila_new(dw_hash *hash, dw_element *val) {
        dw_fila *new = 0;

        new = dw_malloc(sizeof(dw_fila));

        if(hash == 0 || val == 0 || new == 0) {
                goto catch_dwh_fila_new;
        }

        new->record = val;
        val->fila = new;

        if(hash->fila == 0) {
                new->last = new;
                new->next = new;
                hash->fila = new;
                return 1;
        }

        new->next = hash->fila;
        new->last = hash->fila->last;
        hash->fila->last->next = new;
        hash->fila->last = new;
        hash->fila = new;
        return 1;

catch_dwh_fila_new:
        if(new != 0) {
                free(new);
                new = 0;
        }
        return -1;
}

/* Given a pointer to a hash and hash fila element, remove that element from
 * the hash; depending on the second argument, we will also remove
 * the hash the fila points to.  This is a private function. */
int dwh_fila_zap(dw_hash *hash, dw_fila *zap, int get_hash) {
        dw_element *record = 0;
        dw_fila *change = 0;

        if(hash == 0 || zap == 0) {
                return -1;
        }

        if(get_hash == 1) {
                record = zap->record;
        }

        change = zap->next; /* Corner case: Second to last element */
        zap->last->next = change;
        change->last = zap->last;
        if(zap == hash->fila) {
                hash->fila = change;
        }
        if(zap == hash->fila) { /* Corner case: Zapping only fila element */
                hash->fila = 0;
        }

        free(zap);
        if(record != 0) {
                dwh_zap(hash,0,record,1);
        }
        return 1;
}

/* Given a pointer to a hash and hash fila element, put the element at the
 * top of the fila list.  This is a private function; we call this function
 * whenever we fetch an element from the hash */
int dwh_fila_fetch(dw_hash *hash, dw_fila *get) {
        dw_fila *change = 0;

        if(hash == 0 || get == 0 || hash->fila == 0) {
                return -1;
        }

        /* Lets not deal with the corner case of there only being one
         * element (Also, save time when already at top) */
        if(get == hash->fila) {
                return 1;
        }

        /* Pluck fila from wherever it is */
        change = get->next; /* Corner case: Only 2 elements in fila */
        get->last->next = get->next;
        change->last = get->last;
        /* And put fila at top of list */
        get->last = hash->fila->last;
        get->next = hash->fila;
        hash->fila->last->next = get;
        hash->fila->last = get;
        hash->fila = get;
        return 1;
}

/* Given a hash and hash key, find an element in the dw_hash.  This is a
 * private function called only by other functions in DwHash.c.  Return
 * pointer to element if found, 0 if not found (or error) */
dw_element *dwh_grep(dw_hash *hash, dw_str *key) {
        dw_element *seek = 0;
        int32_t index = 0;
        int max = 0;
        if(hash == 0 || key == 0) {
                return 0;
        }

        index = dwh_hash_compress(key) & hash->mask;

        seek = hash->hash[index];

        max = 0;
        while(seek != 0 && max < 32000) {
                if(dw_issame(key,seek->key) == 1) {
                        return seek;
                }
                seek = seek->next;
                max++;
        }
        return 0;
}

/* Copy an element from the hash and put it in a js_str object which we
 * give to whoever calls the function.  Should the element not be found,
 * or an error occurs, return 0.  If the element is in the hash but
 * expired, we will return 0 if ignore_expire is 0, and the element if
 * ignore_expire is 1.  It is up to the calling function to free the
 * memory used by the new dw_str object */
dw_str *dwh_get(dw_hash *hash, dw_str *key, int ignore_expire, int use_fila) {
        dw_element *seek = 0;
        dw_str *out = 0;

#ifdef XTRA_STUFF
	printf("Calling dwh_get with key ");
	dw_stdout(key);
#endif

        seek = dwh_grep(hash,key);
        if(seek == 0) { /* Not found */
                return 0;
        }

        /* Check to see if a record has expired.  There are corner cases where
         * records can appear far in the future, so records which expire
         * in over three years are considered already expired */
        if(use_fila == 1 && ignore_expire != 1 && (get_time() > seek->expire
                        || (get_time() + 24219648000LL) < seek->expire)
                        && seek->expire != 0) {
                return 0;
        }

        out = dw_copy(seek->value);
        if(out == 0) {
                return 0;
        }

        /* Move the element we just fetched to the top */
        if(use_fila == 1 && seek->immutable == 0) {
                dwh_fila_fetch(hash,seek->fila);
        }

#ifdef XTRA_STUFF
	printf("dwh_get got element with value ");
	dw_stdout(out);
#endif

        return out;

}

/* What is the TTL for a given hash element?  Answer in seconds */
int32_t dwh_get_ttl(dw_hash *hash, dw_str *key) {
        dw_element *seek = 0;
        int64_t time = 0;

        seek = dwh_grep(hash,key);
        if(seek == 0) {
                return -1;
        }

        time = seek->expire - get_time();
        time >>= 8;
        if(time < 0 || time > 31536000 /* One year */) {
                return -1;
        }

        return time;
}

/* Given a hash and hash key (or just a direct pointer to the element in the
 * hash), remove an element from the hash. -1 on error;
 * 1 on success; 0 if no such element in the hash.  While this is a public
 * function, the Deadwood-2 cache code should have no need to call this
 * function.  When called from outside DwHash, seek must have
 * a value of 0. */
int dwh_zap(dw_hash *hash, dw_str *key, dw_element *seek, int use_fila) {

        if(seek == 0) {
                if(dw_assert_sanity(key) == -1) {
                        return -1;
                }

                seek = dwh_grep(hash,key);
        }

        if(seek == 0) {
                return 0;
        }

        if(seek->immutable == 1) {
                return -1; /* We are not allowed to remove this element */
        }

        /* Remove this element from the linked list */
        if(seek->prev != 0) {
                *(seek->prev) = seek->next;
        }
        if(seek->next != 0) {
                seek->next->prev = seek->prev;
        }

        /* Remove this element from the fila */
        if(key != 0 && use_fila == 1) {
                /* We have to avoid circular calls, since we call this
                 * from dwh_fila_zap() */
                dwh_fila_zap(hash,seek->fila,0);
        }

        /* Free the memory used by this element */
        dw_destroy(seek->key);
        dw_destroy(seek->value);
        free(seek);
        if(hash->size > 0) {
                hash->size--;
        }
        return 1;

}

/* Used by dwh_add: Initialize a new element in the dw_hash */
dw_element *dwh_init_element(dw_str *key, dw_str *value, int32_t ttl,
                dw_hash *hash) {
        dw_element *new = 0;
        dw_str *key_copy = 0, *val_copy = 0;
        dw_element *seek = 0;

        /* Initialize the new element */
        /* Destructor: This element is destroyed when either the user
         * asks for it to be destroyed, or when the hash is full and this
         * element is at the bottom of the "fila" list */
        new = dw_malloc(sizeof(dw_element));

        key_copy = dw_copy(key);
        val_copy = dw_copy(value);

        if(new == 0 || key_copy == 0 || val_copy == 0) {
                goto catch_init_element;
        }

        new->key = key_copy;
        new->value = val_copy;
        new->fila = 0; /* About to be filled in, but not here */
        new->immutable = 0; /* Normal element, can be deleted */

        if(ttl < 30 && ttl != -2) {
                ttl = 30;
        } else if(ttl > 63072000) { /* Two years */
                ttl = 63072000;
        }
        if(ttl != -2) {
                new->expire = get_time() + ((int64_t)ttl << 8);
        } else { /* Special case: Retain TTL */
                seek = dwh_grep(hash,key);
                if(seek == 0) { /* Not found; use a short TTL */
                        new->expire = get_time() + 7680;
                } else { /* Do not change TTL */
                        new->expire = seek->expire;
                }
        }
        new->next = 0; /* About to get filled in, but not here */
        new->prev = 0; /* About to be filled in, but not here */

        return new;

catch_init_element:
        if(new != 0) {
                free(new);
                new = 0;
        }
        if(key_copy != 0) {
                dw_destroy(key_copy);
                key_copy = 0;
        }
        if(val_copy != 0) {
                dw_destroy(val_copy);
                val_copy = 0;
        }
        return 0;
}

/* Add an already created hash element to the hash.  Private. */
int dwh_place_new(dw_hash *hash, dw_element *new, int use_fila) {
        int32_t index = 0;

        if(hash == 0 || new == 0) {
                return -1;
        }

        /* If there is already an element in the hash with the same key,
         * zap it */
        if(dwh_zap(hash,new->key,0,use_fila) == -1) {
                return -1;
        }

        /* Put the new element in the hash index linked list */
        index = dwh_hash_compress(new->key) & hash->mask;
        new->next = hash->hash[index];
        new->prev = &(hash->hash[index]);
        if(new->next != 0) {
                new->next->prev = &(new->next);
        }
        hash->hash[index] = new;

        /* Add the new element to the "fila" */
        if(use_fila == 1) {
                dwh_fila_new(hash,new);

                /* If the hash is "full", zap the element at the bottom of
                 * the "fila" */
                hash->size++;
                if(hash->size >= hash->max) {
                        dwh_fila_zap(hash,hash->fila->last,1);
                }
        } else {
                hash->size++;
        }

        return 1;
}

/* Add a (key,value) pair to a given dw_hash.  If an element with the
 * same name already exists, remove the element already in the
 * hash and replace it with this element.
 *
 * If ttl has a value of -2, this is a special value that means
 * "however long an already existing element has; otherwise 30 seconds"
 */
int dwh_add(dw_hash *hash, dw_str *key, dw_str *value, int32_t ttl,
            int use_fila) {
        dw_element *new = 0;

        /* Sanity checks */
        if(hash == 0 || key == 0 || value == 0) {
                goto catch_dwh_add;
        }
        if(dw_assert_sanity(key) == -1 || dw_assert_sanity(value) == -1) {
                goto catch_dwh_add;
        }

        if(ttl > 0 || ttl == -2) {
                new = dwh_init_element(key,value,ttl,hash);
                if(new == 0) {
                        goto catch_dwh_add;
                }

                /* Place the new element in the hash */
                if(dwh_place_new(hash,new,use_fila) == -1) {
                        goto catch_dwh_add;
                }
                if(use_fila == 2) {
                        new->immutable = 1;
                        new->expire = 0; /* Never expire */
                }
        }

        return 1;

catch_dwh_add:
        if(new != 0) {
                free(new);
                new = 0;
        }

        return -1;
}

/* Make a hash bigger.  This code *only* works for hashes without a fila;
 * if the hash has a fila, this program will just return an error.
 *
 * This routine destroys the hash pointed to and returns a newly
 * resized hash if the hash needs to change size; otherwise the
 * code returns a pointer to the old hash.
 */

dw_hash *dwh_hash_autogrow(dw_hash *hash) {
        dw_hash *new = 0;
        int32_t count = 0, noloop = 0;
        dw_element *point = 0, *next = 0;
        if(hash->fila != 0) { /* Hashes with filas not supported */
                return hash;
        }
        if(hash->size < hash->mask) { /* Too small to autogrow */
                return hash;
        }
        if(((hash->mask + 1) & hash->mask) != 0) { /* Sanity power-2 check */
                return hash;
        }
        new = dwh_hash_init(hash->mask + 1); /* Create new hash */
        if(new == 0) { /* If not created, keep old hash */
                return hash;
        }
        /* Iterate through old hash, move stuff to new hash */
        for(count = 0; count <= hash->mask; count++) {
                point = hash->hash[count];
                for(noloop = 0; noloop < 10000 && point != 0; noloop++) {
                        next = point->next;
                        dwh_place_new(new,point,0);
                        point = next;
                }
        }
        dwh_hash_zap(hash);
        return new;
}


/* The HSCK code is code that makes sure the hash is consistant, and
 * that there is no memory corruption in the hash */

#ifdef HSCK

/* Check a single fila element for errors */
void dwh_hsck_onefila(int index, dw_fila *seek) {
        if(seek->last == 0) {
                printf("WARNING: Element %d @ %p has last of 0\n",
                        index,seek);
                return;
        }
        if(seek->last->next != seek) {
                printf("WARNING: next @ %p doesn't point to %p\n",
                        seek->last, seek);
        }
        if(seek->next == 0) {
                printf("WARNING: Element %d @ %p has next of 0\n",
                        index,seek);
                return;
        }
        if(seek->next->last != seek) {
                printf("WARNING: last @ %p doesn't point to %p\n",
                        seek->next, seek);
        }
        if(seek->record == 0) {
                printf("WARNING: Element %d @ %p has 0 record\n",
                        index,seek);
                return;
        }
        if(seek->record->fila != seek) {
                printf("WARNING: record @ %p doesn't point to %p\n",
                        seek->record, seek);
        }
}

/* Verify that the fila list of a hash is good; this is used for debugging
 * purposes */
int dwh_hsck_fila(dw_hash *hash) {
        dw_fila *seek = 0;
        int32_t index = 0;

        if(hash == 0) {
                return -1;
        }
        if(hash->fila == 0) {
                if(hash->size != 0) {
                        printf("WARNING: No hash->fila and %d elements\n",
                               hash->size);
                }
                return 0; /* 0 elements */
        }

        seek = hash->fila->last;
        for(index = 1; index < 16777218; index++) {
                dwh_hsck_onefila(index,seek);
                if(seek == hash->fila) {
                        break;
                }
                seek = seek->last;
        }

        if(index != hash->size) {
                printf("WARNING: hash->size is %d but %d fila records found\n",
                        hash->size,index);
        }

        return index;

}

/* Verify that all hash linked list chains are good */
int dwh_hsck_chains(dw_hash *hash) {
        dw_element *seek = 0;
        dw_element **last = 0;
        int32_t index = 1;
        int32_t count = 0;

        if((hash->mask & (hash->mask + 1)) != 0) {
                printf("WARNING: Mask is not a power of 2 minus 1\n");
                return -1;
        }

        while(index != 0) {
                index++;
                index &= hash->mask;
                seek = hash->hash[index];
                last = &(hash->hash[index]);
                while(seek != 0) {
                        count++;
                        if(seek->prev != last) {
                                printf("WARNING: Bad prev at %p\n",seek);
                        }
                        last = &(seek->next);
                        seek = seek->next;
                }
        }

        if(count != hash->size) {
                printf("WARNING: hash->size is %d but %d records found\n",
                        hash->size,count);
        }

        return count;
}

#endif /* HSCK */

/* Write a 32-bit number to the stdio filehandle; private function */
void dwh_put_int32(FILE *handle, int32_t num) {
        int counter = 0;
        uint8_t val = 0;
        /* If the number is negative, put 0xffffffff in the file */
        if(num < 0) {
                for(counter = 0; counter < 4; counter++) {
                        putc(255,handle);
                }
                return;
        }
        for(counter = 24; counter >= 0; counter -= 8) {
                val = num >> counter;
                val &= 0xff; /* Probably not needed */
                putc(val,handle);
        }
}

/* Write a 64-bit number to the stdio filehandle; private function */
void dwh_put_int64(FILE *handle, int64_t num) {
        int counter = 0;
        uint8_t val = 0;
        /* If the number is negative, put 0x0000000000000001 in the file.
         * (not 0, which means "never expire") No, Deadwood will not put
         * an accurate timestamp before March 20, 1979 in the cache file.
         * If you wonder: Blake's 7 Gambit's original broadcast is our epoch */
        if(num < 0) {
                for(counter = 0; counter < 7; counter++) {
                        putc(0,handle);
                }
                putc(1,handle);
                return;
        }
        for(counter = 56; counter >= 0; counter -= 8) {
                val = num >> counter;
                val &= 0xff; /* Probably not needed */
                putc(val,handle);
        }
}

/* Write a dw_string to the stdio filehandle; this first writes the
 * length to the file, followed by the string itself; this is a
 * private function */
void dwh_put_dwstr(FILE *handle, dw_str *str) {
        int counter = 0;
        int val = 0;

        if(dw_assert_sanity(str) == -1) {
                dwh_put_int32(handle,-1);
                return;
        }

        if(str->len > 16777216) {
                dwh_put_int32(handle,-1);
                return;
        }

        dwh_put_int32(handle,str->len);

        for(counter = 0; counter < str->len; counter++) {
                val = *(str->str + counter);
                val &= 0xff;
                putc(val,handle);
        }

}

/* Write a single hash element to the stdio filehandle; this writes the
 * key, followed by the value, followed by the expire time of the element */
void dwh_put_hash_element(FILE *handle, dw_element *element) {
        if(element == 0 || element->key == 0 || element->expire == 0 ||
           element->key->len == 0 || element->value->len == 0) {
                return;
        }

        dwh_put_dwstr(handle,element->key);
        dwh_put_dwstr(handle,element->value);
        dwh_put_int64(handle,element->expire);

}

/* Write an entire hash (assosciated array) to a given file; 1 on
 * success; -1 on error.  This is a public function. */
int dwh_write_hash(dw_hash *hash, char *filename) {
        FILE *handle = 0;
        dw_fila *seek = 0;
        int index = 0;

        if(hash == 0 || filename == 0) {
                return -1;
        }

        handle = fopen(filename,"wb");

        if(handle == 0) {
                dw_log_3strings("Could not open hash at ",filename,
                                " for writing",1);
#ifndef MINGW
                perror("System said");
#endif /* MINGW */
                return -1;
        }

        /* Mark this file as a "Deadwood 2" file */
        dwh_put_int32(handle,0x00445733); /* '\0'DW3 */
        /* Put a 32-bit number in which is 0; for possible future use */
        dwh_put_int32(handle,0);

        /* We go backwards so, when reading the hash, the least important
         * elements are put in first, since each new element put in the
         * hash is made more important than the elements already in the
         * hash */
        if(hash->fila != 0) { /* So we don't segfault on empty hash */
                seek = hash->fila->last;
                for(index = 0; index < 16777217; index++) {
                        dwh_put_hash_element(handle,seek->record);
                        if(seek == hash->fila) {
                                break;
                        }
                        seek = seek->last;
                }
        }

        fclose(handle);
        return 1;
}

/* Read a 32-bit big-endian binary value from a file; if the value is
 * less than 0, return -1; if EOF while getting the value, return -2.
 * Private function. */
int32_t dwh_get_int32(FILE *handle) {
        int32_t out = 0;
        uint8_t get = 0;
        int counter = 0;

        if(feof(handle)) {
                return -2;
        }

        get = fgetc(handle);
        if(get > 127) {
                if(feof(handle)) {
                        return -2;
                }
                return -1;
        }
        get &= 0xff; /* Probably not needed... */
        out = get << 24;

        for(counter = 16; counter >= 0; counter -= 8) {
                get = fgetc(handle);
                if(feof(handle)) {
                        return -2;
                }
                get &= 0xff; /* Probably not needed... */
                out |= get << counter;
        }

        return out;
}

/* Read a 64-bit big-endian binary value from a file; if the value is
 * less than 0, return -1; if EOF while getting the value, return -2.
 * Private function. */
int64_t dwh_get_int64(FILE *handle) {
        int64_t out = 0;
        uint8_t get = 0;
        int counter = 0;

        if(feof(handle)) {
                return -2;
        }

        get = getc(handle);
        if(get > 127) {
                if(feof(handle)) {
                        return -2;
                }
                return -1;
        }
        get &= 0xff; /* Probably not needed... */
        out = (int64_t)get << 56;

        for(counter = 48; counter >= 0; counter -= 8) {
                get = getc(handle);
                if(feof(handle)) {
                        return -2;
                }
                get &= 0xff; /* Probably not needed... */
                out |= (int64_t)get << counter;
        }

        return out;
}

/* Read a dw_str object from a file; should the length be invalid (less than
 * zero), return a zero-length string.  Private function. */
dw_str *dwh_get_dwstr(FILE *handle) {
        dw_str *out = 0;
        int32_t len = 0;
        uint8_t get = 0;
        uint8_t *str = 0;

        len = dwh_get_int32(handle);

        if(len == -2) { /* EOF reached */
                return 0;
        }

        if(len < 0) {
                len = 0;
        }

        out = dw_create(len + 1);

        if(out == 0 || dw_assert_sanity(out) == -1) {
                dw_destroy(out);
                return 0;
        }

        out->len = len;
        str = out->str;

        for(;len > 0; len--) {
                get = getc(handle);
                if(feof(handle)) {
                        get = 0;
                }
                *str = get;
                str++;
        }

        return out;

}

/* Get a single hash element from the file (Which we will then place in the
 * hash) */
dw_element *dwh_get_hash_element(FILE *handle) {
        dw_element *out = 0;

        /* These elements are destroyed when deleted from the hash */
        out=dw_malloc(sizeof(dw_element));
        if(out == 0) {
                return 0;
        }
        out->key = 0;
        out->value = 0;

        out->key = dwh_get_dwstr(handle);
        out->value = dwh_get_dwstr(handle);
        out->expire = dwh_get_int64(handle);
        if(out->key == 0 || out->value == 0 || out->expire < 2) {
                goto catch_dwh_get_hash_element;
        }
        if(out->key->len == 0 || out->value->len == 0) {
                goto catch_dwh_get_hash_element;
        }
        return out;

catch_dwh_get_hash_element:
        if(out->key != 0) {
                free(out->key);
                out->key = 0;
        }
        if(out->value != 0) {
                free(out->value);
                out->value = 0;
        }
        if(out != 0) {
                free(out);
                out = 0;
        }
        return 0;
}

/* Read a hash from a file, and return the hash read from the file
 * as a hash pointer to the calling function */
dw_hash *dwh_read_hash(char *filename) {
        FILE *handle = 0;
        dw_hash *new = 0;
        dw_element *ele = 0;
        int32_t max = 0, counter = 0;

        if(filename == 0) {
                return 0;
        }

        handle = fopen(filename,"rb");
        if(handle == 0) {
                dw_log_3strings("Could not open hash at ",filename,
                                " for reading",1);
#ifndef MINGW
                perror("System said");
#endif /* MINGW */
                return 0;
        }

        if(dwh_get_int32(handle) != 0x00445733) /* '\0'DW3; file marker */ {
                return 0;
        }

        dwh_get_int32(handle); /* Read and ignore 32-bit value */

        /* Maximum number of elements in hash */
        if(cache_size > 32 && cache_size < 16777218) {
                max = cache_size; /* Get from dwood2rc */
        } else {
                max = 1024; /* Arbitrary value when cache_size is strange */
        }

        new = dwh_hash_init(max);
        if(new == 0) {
                return 0;
        }

        counter = 0;
        while(!feof(handle) && counter < 16777218) { /* Add elements to hash */
                ele = dwh_get_hash_element(handle);
                if(ele != 0) {
                        if(dwh_place_new(new,ele,1) == -1) {
                                break;
                        }
                } else {
                        break;
                }
                counter++;
        }

        fclose(handle);
        return new;
}

#ifdef HAVE_MAIN
extern void dwr_belt(); /* Keep -Wall happy */

/* This is just a test to see how fast Radio Gatun is */
uint32_t dwh_rg32_hash_compress(dw_str *obj) {
        dwr_rg *a;
        uint32_t out = 0;
        a = dwr_init_rg(obj);
        dwr_belt(a->mill, a->belt);
        out = *(a->mill + 1);
        dwr_zap(a);
        return out;
}

#include <unistd.h>
#include <signal.h>

uint32_t count = 0;
int ga = 0;

void alarm_handler() {
        printf("%d\n",count);
        count = 0;
        ga = 1;
}

int main() {
        dw_str *a, *z;
        dw_hash *h;
        int b;
        a = dw_create(2);
        a->len = 1;
        h = dwh_hash_init(64);
        set_time();
        for(b = 0; b < 255; b++) {
                printf("%d ",b);
                *(a->str) = b;
                dwh_add(h,a,a,1024,1);
                dwh_add(h,a,a,1024,1);
                *(a->str) = 77;
                z = dwh_get(h,a,0,1);
                dw_destroy(z);
        }
        b = 36;
        set_time();
        while(b != 254) {
                printf("%d ",b);
                *(a->str) = b;
                b += 37;
                b %= 255;
                z = dwh_get(h,a,0,1);
                dw_stdout(z);
                dw_destroy(z);
        }
        set_time();
        for(b = 0; b < 255; b++) {
                *(a->str) = b;
                dwh_add(h,a,a,1024,1);
                dwh_add(h,a,a,1024,1);
        }
        dwh_write_hash(h,"writehash_test");
#ifdef HSCK
        printf("%d fila elements in hash\n",dwh_hsck_fila(h));
        printf("%d chain elements in hash\n",dwh_hsck_chains(h));
#endif /* HSCK */
        for(b = 0; b < 255; b++) {
                *(a->str) = b;
                dwh_zap(h,a,0,1);
                dwh_zap(h,a,0,1);
        }
        dw_destroy(a);
        dwh_hash_nuke(h);
        h = dwh_read_hash("writehash_test");
#ifdef HSCK
        printf("%d fila elements in hash\n",dwh_hsck_fila(h));
        printf("%d chain elements in hash\n",dwh_hsck_chains(h));
#endif /* HSCK */
        dwh_write_hash(h,"writehash_test2");
        dwh_hash_nuke(h);
        h = dwh_read_hash("writehash_test2");
#ifdef HSCK
        printf("%d fila elements in hash\n",dwh_hsck_fila(h));
        printf("%d chain elements in hash\n",dwh_hsck_chains(h));
#endif /* HSCK */
        dwh_write_hash(h,"writehash_test3");
        /* Test non-fila hash */
        dwh_hash_nuke(h);
        printf("---AUTOGROW---\n");
        h = dwh_hash_init(2);
        a = dw_create(2);
        a->len = 1;
        for(b = 0; b < 245; b++) {
                *(a->str) = b;
                dwh_add(h,a,a,1024,0);
                dwh_add(h,a,a,1024,0);
                printf("Hash is at %p size %d mask %d\n",h,h->size,h->mask);
                h = dwh_hash_autogrow(h);
                printf("Hash is now at %p\n",h);
                *(a->str) = 77;
                z = dwh_get(h,a,0,0);
                dw_destroy(z);
                z = 0;
        }
        dw_destroy(z);
        z = 0;
        dw_destroy(a);
        a = 0;
        dwh_hash_nuke(h);
        return 0;
}
#endif /* HAVE_MAIN */
