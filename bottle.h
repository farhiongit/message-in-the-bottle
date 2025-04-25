/*******
 * Copyright 2018-2025 Laurent Farhi
 *
 *  This file is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This file is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this file.  If not, see <http://www.gnu.org/licenses/>.
 *****/

////////////////////////////////////////////////////
// Author:  Laurent Farhi
// Date:    08/05/2017
// Contact: lfarhi@sfr.fr
////////////////////////////////////////////////////

/* A thread-safe, template, implementation of a message queue */

/*TODO:
   - Use C11 thread rather than explicit pthreads.
 */

#pragma once

#ifndef __BOTTLE_H__
#define __BOTTLE_H__

#include "vfunc.h"
#include <pthread.h>
#include <errno.h>
#include <stdio.h>

// To be translated.
#define TBT(text) (text)

#define BOTTLE_ASSERT3(cond, msg, fatal) \
  do {\
    if (!(cond)) \
    {\
      const char* _msg = msg; \
      if (_msg && *_msg) fprintf(stderr, TBT("%1$s: %2$s"), fatal ? TBT("FATAL ERROR") : TBT("WARNING"), TBT(_msg));\
      if (fatal) pthread_exit(0) ;\
    }\
  } while(0)
#define BOTTLE_ASSERT(cond) \
  do {\
    if (!(cond)) \
    {\
      fprintf(stderr, TBT("%6$s: Thread %5$#lx: %1$s (condition '%1$s' is false in function %2$s at %3$s:%4$i)\n"),#cond,__func__,__FILE__,__LINE__, pthread_self(), TBT("FATAL ERROR"));\
      pthread_exit(0) ;\
    }\
  } while(0)

#define UNLIMITED  ((size_t) -1)          /* Unbound buffer size (not recommended) */
#define UNBUFFERED ((size_t)  0)          /* Default unbuffered (à la Go) capacity for perfect thread synchronisation : communication succeeds only when the sender and receiver are both ready. */
#define DEFAULT    ((size_t)  1)          /* Default (à la Rust) capacity for perfect thread synchronisation (see https://stackoverflow.com/a/53348817 and https://users.rust-lang.org/t/0-capacity-bounded-channel/68357) */

#define DECLARE_BOTTLE( TYPE )     \
\
  struct _BOTTLE_##TYPE;           \
\
  typedef struct _BOTTLE_VTABLE_##TYPE                            \
  {                                                               \
    int (*Fill) (struct _BOTTLE_##TYPE *self, TYPE message);      \
    int  (*TryFill) (struct _BOTTLE_##TYPE *self, TYPE message);  \
    int (*Drain) (struct _BOTTLE_##TYPE *self, TYPE *message);    \
    int (*TryDrain) (struct _BOTTLE_##TYPE *self, TYPE *message); \
    void (*Close) (struct _BOTTLE_##TYPE *self);                  \
    void (*Destroy) (struct _BOTTLE_##TYPE *self);                \
  } _BOTTLE_VTABLE_##TYPE;                                        \
\
  typedef struct _BOTTLE_##TYPE             \
  {                                         \
    struct _queue_##TYPE {                  \
      TYPE*  buffer;      /* Array containing the messages */           \
      TYPE*  reader_head; /* Position where to read the next value */   \
      TYPE*  writer_head; /* Position where to write the next value */  \
      size_t size;        /* Number of messages currently in the queue (<= capacity) */ \
      size_t capacity;    /* Maximum number of elements in the queue (size of the array) */ \
      int    unlimited;   /* Indicates that the capacity can be extended automatically as required */ \
    } queue;                                \
    size_t                       capacity; /* Capacity of the bottle. Can be equal to 0. See https://users.rust-lang.org/t/0-capacity-bounded-channel/68357/20 */ \
    int                          closed;    \
    int                          frozen;    \
    pthread_mutex_t              mutex;     \
    pthread_cond_t               not_empty; \
    pthread_cond_t               not_full;  \
    const _BOTTLE_VTABLE_##TYPE *vtable;    \
    TYPE __dummy__;                         \
  } BOTTLE_##TYPE;                          \
\
  BOTTLE_##TYPE *BOTTLE_CREATE_##TYPE( size_t capacity );  \
  void BOTTLE_INIT_##TYPE (BOTTLE_##TYPE *self, size_t capacity);  \
  struct __useless_struct_to_allow_trailing_semicolon__

#define BOTTLE( TYPE )  BOTTLE_##TYPE

/// BOTTLE (T) * BOTTLE_CREATE ([T], [size_t capacity = DEFAULT])
#define BOTTLE_CREATE1( TYPE ) \
  BOTTLE_CREATE_##TYPE(DEFAULT)
#define BOTTLE_CREATE2( TYPE, capacity ) \
  BOTTLE_CREATE_##TYPE(capacity)
#define BOTTLE_CREATE(...) VFUNC(BOTTLE_CREATE, __VA_ARGS__)

/// int BOTTLE_FILL (BOTTLE (T) *bottle, [T message])
#define BOTTLE_FILL2(self, message)  \
  ((self)->vtable->Fill ((self), (message)))
#define BOTTLE_FILL1(self)  \
  ((self)->vtable->Fill ((self), ((self)->__dummy__)))
#define BOTTLE_FILL(...) VFUNC(BOTTLE_FILL, __VA_ARGS__)

/// int BOTTLE_TRY_FILL (BOTTLE (T) *bottle, [T message])
#define BOTTLE_TRY_FILL2(self, message)  \
  ((self)->vtable->TryFill ((self), (message)))
#define BOTTLE_TRY_FILL1(self)  \
  ((self)->vtable->TryFill ((self), ((self)->__dummy__)))
#define BOTTLE_TRY_FILL(...) VFUNC(BOTTLE_TRY_FILL, __VA_ARGS__)

/// int BOTTLE_DRAIN (BOTTLE (T) *bottle, [T message])
#define BOTTLE_DRAIN2(self, message)  \
  ((self)->vtable->Drain ((self), &(message)))
#define BOTTLE_DRAIN1(self)  \
  ((self)->vtable->Drain ((self), &((self)->__dummy__)))
#define BOTTLE_DRAIN(...) VFUNC(BOTTLE_DRAIN, __VA_ARGS__)

/// int BOTTLE_TRY_DRAIN (BOTTLE (T) *bottle, [T message])
#define BOTTLE_TRY_DRAIN2(self, message)  \
  ((self)->vtable->TryDrain ((self), &(message)))
#define BOTTLE_TRY_DRAIN1(self)  \
  ((self)->vtable->TryDrain ((self), &((self)->__dummy__)))
#define BOTTLE_TRY_DRAIN(...) VFUNC(BOTTLE_TRY_DRAIN, __VA_ARGS__)

/// void BOTTLE_PLUG (BOTTLE (T) *bottle)
#define BOTTLE_PLUG(self)  \
  do { BOTTLE_ASSERT (!pthread_mutex_lock (&(self)->mutex));   \
       (self)->frozen = 1 ;                                    \
       BOTTLE_ASSERT (!pthread_mutex_unlock (&(self)->mutex)); \
     } while(0)

/// void BOTTLE_UNPLUG (BOTTLE (T) *bottle)
#define BOTTLE_UNPLUG(self)  \
  do { BOTTLE_ASSERT (!pthread_mutex_lock (&(self)->mutex));   \
       (self)->frozen = 0 ;                                    \
       BOTTLE_ASSERT (!pthread_mutex_unlock (&(self)->mutex)); \
       BOTTLE_ASSERT (!pthread_cond_signal (&self->not_full)); \
     } while(0)

/// int BOTTLE_IS_PLUGGED (BOTTLE (T) *bottle)
#define BOTTLE_IS_PLUGGED(self) ((self)->frozen)

/// void BOTTLE_CLOSE (BOTTLE (T) *bottle)
#define BOTTLE_CLOSE(self)  \
  do { (self)->vtable->Close ((self)); } while (0)

/// int BOTTLE_IS_CLOSED (BOTTLE (T) *bottle)
#define BOTTLE_IS_CLOSED(self) ((self)->closed)

/// void BOTTLE_DESTROY (BOTTLE (T) *bottle)
#define BOTTLE_DESTROY(self)  \
  do { (self)->vtable->Destroy ((self)); } while (0)

/// size_t BOTTLE_CAPACITY (BOTTLE (T) *bottle)
#define BOTTLE_CAPACITY(self) ((self)->capacity)

/// BOTTLE (T) * BOTTLE_DECL (variable_name, [T], [size_t capacity = DEFAULT])
#if defined(__GNUC__) || defined (__clang__)
#define BOTTLE_DECL3(var, TYPE, capacity)  \
__attribute__ ((cleanup (BOTTLE_CLEANUP_##TYPE))) BOTTLE_##TYPE var; BOTTLE_INIT_##TYPE (&var, capacity)
#define BOTTLE_DECL2(var, TYPE) BOTTLE_DECL3(var, TYPE, DEFAULT)
#define BOTTLE_DECL(...) VFUNC(BOTTLE_DECL, __VA_ARGS__)
#endif

/// A more C like syntax
#define bottle_type_declare(...)  DECLARE_BOTTLE(__VA_ARGS__)
#define bottle_type_define(...)   DEFINE_BOTTLE(__VA_ARGS__)

#define bottle_t(type)            BOTTLE(type)
#define bottle_create(...)        BOTTLE_CREATE(__VA_ARGS__)
#define bottle_auto(...)          BOTTLE_DECL(__VA_ARGS__)

#define bottle_send(...)          BOTTLE_FILL(__VA_ARGS__)
#define bottle_try_send(...)      BOTTLE_TRY_FILL(__VA_ARGS__)
#define bottle_recv(...)          BOTTLE_DRAIN(__VA_ARGS__)
#define bottle_try_recv(...)      BOTTLE_TRY_DRAIN(__VA_ARGS__)

#define bottle_close(self)        BOTTLE_CLOSE(self)
#define bottle_destroy(self)      BOTTLE_DESTROY(self)

#define bottle_capacity(self)     BOTTLE_CAPACITY(self)
#define bottle_is_closed(self)    BOTTLE_IS_CLOSED(self)

#define bottle_plug(self)         BOTTLE_PLUG(self)
#define bottle_unplug(self)       BOTTLE_UNPLUG(self)
#define bottle_is_plugged(self)   BOTTLE_IS_PLUGGED(self)

#endif
