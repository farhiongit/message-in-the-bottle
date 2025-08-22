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
// Revised: 24/05/2025
// Contact: lfarhi@sfr.fr
////////////////////////////////////////////////////

/* A thread-safe and generic implementation of a message queue for thread communication and synchronisation */

#ifndef __BOTTLE_H__
#  define __BOTTLE_H__

#  include "vfunc.h"
#  include <threads.h>
#  include <errno.h>
#  include <stdio.h>

#  ifndef LIMITED_BUFFER
#    define UNLIMITED  ((size_t) -1)    /* Unbound buffer size (not recommended) */
#  endif
#  define UNBUFFERED   ((size_t)  0)    /* Unbuffered capacity for perfect thread synchronisation */
#  define DEFAULT      UNBUFFERED
                                /* Default is unbuffered (à la Go) */

#  define DECLARE_BOTTLE( TYPE )     \
\
  struct _BOTTLE_##TYPE;           \
\
  typedef struct _BOTTLE_VTABLE_##TYPE                            \
  {                                                               \
    int (*Fill) (struct _BOTTLE_##TYPE *self, TYPE message);      \
    int  (*TryFill) (struct _BOTTLE_##TYPE *self, TYPE message);  \
    int (*Drain) (struct _BOTTLE_##TYPE *self, TYPE *message);    \
    int (*TryDrain) (struct _BOTTLE_##TYPE *self, TYPE *message); \
    void (*Plug) (struct _BOTTLE_##TYPE *self);                   \
    void (*Unplug) (struct _BOTTLE_##TYPE *self);                 \
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
    size_t                       capacity; /* Declared capacity of the bottle at creation.
                                              Can be > 0 (and not -1) : buffered ;
                                              can be equal to 0, see https://users.rust-lang.org/t/0-capacity-bounded-channel/68357/20 : unbuffered ;
                                              can be -1 : unbounded */ \
    int                          closed;    \
    int                          frozen;    \
    mtx_t                        mutex;     \
    cnd_t                        not_empty; \
    cnd_t                        not_full;  \
    int                          not_reading;\
    cnd_t                        reading;   \
    int                          not_writing;\
    cnd_t                        writing;   \
    const _BOTTLE_VTABLE_##TYPE *vtable;    \
    TYPE __dummy__;                         \
  } BOTTLE_##TYPE;                          \
\
  BOTTLE_##TYPE *BOTTLE_CREATE_##TYPE( size_t capacity );  \
  void BOTTLE_INIT_##TYPE (BOTTLE_##TYPE *self, size_t capacity);  \
  struct __useless_struct_to_allow_trailing_semicolon__

#  define BOTTLE( TYPE )  BOTTLE_##TYPE

/// BOTTLE (T) * BOTTLE_CREATE ([T], [size_t capacity = DEFAULT])
#  define BOTTLE_CREATE1( TYPE ) \
  BOTTLE_CREATE_##TYPE(DEFAULT)
#  define BOTTLE_CREATE2( TYPE, capacity ) \
  BOTTLE_CREATE_##TYPE(capacity)
#  define BOTTLE_CREATE(...) VFUNC(BOTTLE_CREATE, __VA_ARGS__)

/// int BOTTLE_FILL (BOTTLE (T) *bottle, [T message])
#  define BOTTLE_FILL2(self, message)  \
  ((self)->vtable->Fill ((self), (message)))
#  define BOTTLE_FILL1(self)  \
  ((self)->vtable->Fill ((self), ((self)->__dummy__)))
#  define BOTTLE_FILL(...) VFUNC(BOTTLE_FILL, __VA_ARGS__)

/// int BOTTLE_TRY_FILL (BOTTLE (T) *bottle, [T message])
#  define BOTTLE_TRY_FILL2(self, message)  \
  ((self)->vtable->TryFill ((self), (message)))
#  define BOTTLE_TRY_FILL1(self)  \
  ((self)->vtable->TryFill ((self), ((self)->__dummy__)))
#  define BOTTLE_TRY_FILL(...) VFUNC(BOTTLE_TRY_FILL, __VA_ARGS__)

/// int BOTTLE_DRAIN (BOTTLE (T) *bottle, [T *message])
#  define BOTTLE_DRAIN2(self, message)  \
  ((self)->vtable->Drain ((self), (message)))
#  define BOTTLE_DRAIN1(self)  \
  ((self)->vtable->Drain ((self), &((self)->__dummy__)))
#  define BOTTLE_DRAIN(...) VFUNC(BOTTLE_DRAIN, __VA_ARGS__)

/// int BOTTLE_TRY_DRAIN (BOTTLE (T) *bottle, [T *message])
#  define BOTTLE_TRY_DRAIN2(self, message)  \
  ((self)->vtable->TryDrain ((self), (message)))
#  define BOTTLE_TRY_DRAIN1(self)  \
  ((self)->vtable->TryDrain ((self), &((self)->__dummy__)))
#  define BOTTLE_TRY_DRAIN(...) VFUNC(BOTTLE_TRY_DRAIN, __VA_ARGS__)

/// void BOTTLE_PLUG (BOTTLE (T) *bottle)
#  define BOTTLE_PLUG(self)  \
  do { (self)->vtable->Plug ((self)); } while (0)

/// void BOTTLE_UNPLUG (BOTTLE (T) *bottle)
#  define BOTTLE_UNPLUG(self)  \
  do { (self)->vtable->Unplug ((self)); } while (0)

/// void BOTTLE_CLOSE (BOTTLE (T) *bottle)
#  define BOTTLE_CLOSE(self)  \
  do { (self)->vtable->Close ((self)); } while (0)

/// void BOTTLE_DESTROY (BOTTLE (T) *bottle)
#  define BOTTLE_DESTROY(self)  \
  do { (self)->vtable->Destroy ((self)); } while (0)

/// BOTTLE (T) * BOTTLE_DECL (variable_name, [T], [size_t capacity = DEFAULT])
#  if defined(__GNUC__) || defined (__clang__)
#    define BOTTLE_DECL3(var, TYPE, capacity)  \
__attribute__ ((cleanup (BOTTLE_CLEANUP_##TYPE))) BOTTLE_##TYPE var; BOTTLE_INIT_##TYPE (&var, capacity)
#    define BOTTLE_DECL2(var, TYPE) BOTTLE_DECL3(var, TYPE, DEFAULT)
#    define BOTTLE_DECL(...) VFUNC(BOTTLE_DECL, __VA_ARGS__)
#  endif

/// A more C like syntax
#  define bottle_type_declare(...)  DECLARE_BOTTLE(__VA_ARGS__)
#  define bottle_type_define(...)   DEFINE_BOTTLE(__VA_ARGS__)

#  define bottle_t(type)            BOTTLE(type)
#  define bottle_create(...)        BOTTLE_CREATE(__VA_ARGS__)
#  define bottle_auto(...)          BOTTLE_DECL(__VA_ARGS__)

#  define bottle_send(...)          BOTTLE_FILL(__VA_ARGS__)
#  define bottle_try_send(...)      BOTTLE_TRY_FILL(__VA_ARGS__)
#  define bottle_recv(...)          BOTTLE_DRAIN(__VA_ARGS__)
#  define bottle_try_recv(...)      BOTTLE_TRY_DRAIN(__VA_ARGS__)

#  define bottle_close(self)        BOTTLE_CLOSE(self)
#  define bottle_destroy(self)      BOTTLE_DESTROY(self)

#  define bottle_plug(self)         BOTTLE_PLUG(self)
#  define bottle_unplug(self)       BOTTLE_UNPLUG(self)

#endif
