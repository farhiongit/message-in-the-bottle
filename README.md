Thread-safe message queue for thread synchronization
====================================================

I have recently read about the Go language. I was not really convinced by the concept.
The minimalistic grammar and overall simplicity (as compared to C++ or Java) are nice feature,
but the language still needs optimisation and is sometime tricky.

Nevertheless, I noticed the use of the gracious message queue pattern (called channels in Go) for
thread (called goroutines) synchronization.

This pattern is a FIFO thread-safe message queue suited for thread synchronization.

The pattern of message driven synchronization is of interest because it is intuitive and of a higher level
of abstraction than mutexes and conditions:

- some threads `A` fill the message queue
- some other threads `B` eat up the message queue.

Threads B are gracefully synchronized with threads A.

Therefore, I grabbed my copy of the (excellent) book "Programming with POSIX threads" by Butenhof, where the pattern is mentionned, to
implement it for my old favorite language C.

And here it is.

# Insights of the user interface

## Creation of a bottle

To transport messages of type *T*, just create a message queue with:

`BOTTLE(` *T* `) *`*bottle* ` = BOTTLE_CREATE (` *T* `);`

For instance, to create a message queue *b* for exchanging integers between threads, use:

`BOTTLE (int) *b = BOTTLE_CREATE (int);`

The message queue is a strongly typed (yes, it is a hand-made template container) FIFO queue.

The queue is unbuffered by default.

### Buffered message queue

If needed, buffered queues can be used in rare cases (for instance to limit the number of thread workers).
To create a buffered message queue, pass its *capacity* as an optional (positive integer) second argument of `BOTTLE_CREATE` :

`BOTTLE(` *T* `) *`*bottle* ` = BOTTLE_CREATE (` *T* `, ` *capacity* `);`

- A *capacity* set to `INFINITE_CAPACITY` defines a buffered queue of inifinite capacity.
- A *capacity* set to `UNBUFFERED` defines an unbuffered queue (default).

*The usage of buffered queues is usually not necessary and should be avoided.*

## Exchanging messages between thread

Sender threads communicate synchronously with receiver threads by exchanging messages through the bottle:
the bottle has a mouth where it can be filled with messages and a tap from where it can be drained.

![alt text](Bottle.jpg "A classy FIFO message queue")

<small>(c) Davis & Waddell - EcoGlass Oil Bottle with Tap Large 5 Litre | Peter's of Kensington</small>

### Receiving messages

The receivers can receive messages (drainig form the tap), as long as the bottle is not closed, by calling `BOTTLE_DRAIN (`*bottle*`, `*message*`)`.

- `BOTTLE_DRAIN` returns 0 (with `errno` set to `ECONNABORTED`) if there is no data to receive and the bottle was
       closed (by `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`).

    This condition (returned value equal to 0 and `errno` equal to `ECONNABORTED`) should be handled
    by the eaters and stop the reception of any data from the bottle.

- If there is data to receive, `BOTTLE_DRAIN` receives a *message* from the bottle
      (it modifies the value of the second argument *message*) and returns 1.
- Otherwise (there is no data to receive and the bottle is not closed), `BOTTLE_DRAIN` blocks
        until there is data to receive.

Therefore `BOTTLE_DRAIN` returns 1 if a message has been sucessfully received from the bottle.

Please notice that the second argument *message* is of type *T*, and not a pointer to *T*,
even though it might be modified by the callee (macro magic here).

### Sending messages
  
The senders can send messages (filling in through the mouth of the bottle)
by calling `BOTTLE_FILL (`*bottle*`, `*message*`)`, as long as the mouth is open
and the botlle is not closed.

- `BOTTLE_FILL` returns 0 (with `errno` set to `ECONNABORTED`) in those cases:

    - immediately without blocking if the bottle was closed (by `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`).
    - after unblocking immediately when the bottle is closed (by `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`).

      This most probably indicates an error in the user program as it should be avoided to close a bottle
      while senders are still using it.

      Anyhow, this condition (returned value equal to 0 and `errno` equal to `ECONNABORTED`) should be handled
      by the feeders and stop the transmission of any data to the bottle.

- `BOTTLE_FILL` blocks in those cases:

    - If the bottle was plugged (by `BOTTLE_PLUG`).
    - If the message queue is unbuffered, `BOTTLE_FILL` blocks until some receiver has received
    the value sent by a previous sucessful call to `BOTTLE_FILL` or `BOTTLE_TRY_FILL`.
    - If it is buffered and the buffer is full, `BOTTLE_FILL` blocks until some receiver has retrieved a value
    (with `BOTTLE_DRAIN` or `BOTTLE_TRY_DRAIN`).

- In other cases, `BOTTLE_FILL` sends the message in the bottle and returns 1.

Therefore `BOTTLE_FILL` returns 1 if a message has been sucessfully sent in the bottle.

### Resources management

In case the message type *T* would be or would contain (in a structure) allocated resources
(such as memory with `malloc`/`free` or file descriptor with `fopen`/`fclose` for instance),
the user program must respect those simple rules:

- The **sender**:

    - *must* allocate resources of the message before sending it
      (i.e. before the call to functions `BOTTLE_FILL` or `BOTTLE_TRY_FILL`);
    - *should not* access those allocated ressources after the message has been sent. Indeed, from this point,
      the message is owned by the receiver (which could modify it) and does not belong to the sender anymore.

- The **receiver** *must*, after receiving a message
  (i.e. after the call to functions `BOTTLE_DRAIN` or `BOTTLE_TRY_DRAIN`),
  deallocate, after use, all the resources of the message (those previously allocated by the sender).

## Closing communication

When any more messages are sent in the bottle,
receivers must be informed it is not necessary to block and wait for messages anymore.

As the bottle won't be filled any more, we can close the bottle:

The function `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY` will unblock all receivers and wait for the bottle to be empty.
It:

1. prevents any new message from being sent in the bottle (just in case) :
  `BOTTLE_FILL` and `BOTTLE_TRY_FILL` will return 0 immediately (without blocking). 
2. waits for the bottle to be emptied by calls to `BOTTLE_DRAIN` (called by the receivers).
3. asks for any blocked calls to `BOTTLE_DRAIN` (called by the receivers) to stop waiting for food and to finish their job:
  `BOTTLE_DRAIN` will be asked to return immediately with value 0.

This call *must be done in the thread that controls the execution of the feeders*,
after the feeders have finished their work.

After the call to `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`, eaters will still process the remaining messages in the bottle.
The user program *must* wait for all the eaters to be finished before going on.

## Destruction of a bottle

Thereafter, once *all the receivers are done* in the user program, and the bottle is not needed anymore,
it can be destroyed safely with `BOTTLE_DESTROY`.

Note that `BOTTLE_DESTROY` does not call `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY` by default because the user program *should
ensure* that all receivers have returned from calls to `BOTTLE_DRAIN` between `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`
and `BOTTLE_DESTROY` (usually, waiting for the receivers to finish with a `pthread_join` might suffice),
otherwise, some thread resources might not be released properly (mutexes and conditions).

## Other features

### Unblocking message queue functions

There are also unblocking versions of the filling and drainig functions.

These functions return immediately without blocking. *They are **not** suited for thread synchronization* and are of limited use.

When used, the message queue looses its thread-synchronization feature and
behaves like a simple thread-safe FIFO message queue:

- Receivers can receive messages without blocking with `BOTTLE_TRY_DRAIN (`*bottle*`, `*message*`)`.

    - `BOTTLE_TRY_DRAIN` returns 0 (with `errno` set to `ECONNABORTED`) if the bottle is empty and closed.
    - `BOTTLE_TRY_DRAIN` returns 0 (with `errno` set to `EWOULDBLOCK`) if the bottle is empty.
    - Otherwise, it receives a *message* (it modifies the value of the second argument *message*)
      from the bottle and returns 1.

- Senders can send messages without blocking with `BOTTLE_TRY_FILL (`*bottle*`, `*message*`)`.

    - `BOTTLE_TRY_FILL` returns 0 if the bottle is closed (with `errno` set `ECONNABORTED`).
    This most probably indicates an error in the user program as it should be avoided to close a bottle
    while senders are still using it.
    - Otherwise, `BOTTLE_TRY_FILL` returns 0 if the bottle is plugged (with `errno` set `EWOULDBLOCK`) or already full.
    This indicates that a call to `BOTTLE_FILL` would have blocked.
    - Otherwise, it sends a *message* in the bottle and returns 1.

Notice that the second argument *message* is of type *T*, and not a pointer to *T*,
even though it might be modified by `BOTTLE_TRY_DRAIN` (macro magic here).

### Interrupting communication

Finally, the communication between sender and receiver threads can be stopped and restarted at will if needed.

The mouth of the bottle can be:

- plugged (stopping communication) with `BOTTLE_PLUG`,
- unplugged (to restart communication) with `BOTTLE_UNPLUG`.

# Example

All sources should be compiled with the option `-pthread`.

## Simple example

The source below is an example of the standard use of the API.

```c
#include <unistd.h>
#include <stdio.h>

#include "bottle_impl.h"
typedef const char *cstring;
DECLARE_BOTTLE (cstring)
DEFINE_BOTTLE (cstring)

static void *
eat (void *arg)
{
  cstring s;
  BOTTLE (cstring) * bottle = arg;
  while (BOTTLE_DRAIN (bottle, s))
    printf ("%s\n", s);
  return 0;
}

int
main (void)
{
  BOTTLE (cstring) * bottle = BOTTLE_CREATE (cstring);

  pthread_t eater;
  pthread_create (&eater, 0, eat, bottle);

  cstring police[] = { "I'll send an SOS to the world", "I hope that someone gets my", "Message in a bottle" };
  for (size_t i = 0; i < 2 * sizeof (police) / sizeof (*police); i++)
    BOTTLE_FILL (bottle, police[i / 2]), sleep (1);

  BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY (bottle);

  pthread_join (eater, 0);

  BOTTLE_DESTROY (bottle);
}
```

Remarks:

- The sending loop is in the main thread (the one creating the bottle) inside which the bottle can not be closed. This keeps things easier.
- The eater is one thread, which pace is synchronized with the transmiter. That's what it's all about: synchonizing threads with messages !
- The program closes the bottle and waits for the eater thread to finish before destroying the bottle.

## Advanced example

`bottle_example.c` is a complete example of a program (compile with option `-pthread`)
using a synchronized thread-safe FIFO message queue.

**Have fun !**
