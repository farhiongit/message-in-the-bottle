Thread-safe message queue for thread synchronization
====================================================

> This project implements in C the concept of channels like the one found in Go language.
> The messages exchanged through those channels (here called bottles) are strongly typed.

I have recently read about the Go language.
The minimalistic grammar and overall simplicity (as compared to C++ or Java) are nice features,
even if the language still needs optimization and is sometime tricky.

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

> BOTTLE (*T*) \* **BOTTLE_CREATE** (*T*, [size_t capacity = UNBUFFERED])`
>
> *The second argument is optional and defaults to `UNBUFFERED` (see below).*

To transport messages of type *T*, just create a message queue with:

`BOTTLE(` *T* `) *`*bottle* ` = BOTTLE_CREATE (` *T* `);`

*T* could be any standard or user defined (`typedef`) type, simple or composed structure (`struct`).

For instance, to create a message queue *b* for exchanging integers between threads, use:

`BOTTLE (int) *b = BOTTLE_CREATE (int);`

The message queue is a strongly typed (yes, it is a hand-made template container) FIFO queue.

### Unbuffered message queue

The created queue is unbuffered by default.
Such an unbuffered queue is suitable to synchronize threads running concurrently.

### Buffered message queue

If needed, buffered queues can be used in some cases (for instance to limit the number of thread workers).
To create a buffered message queue, pass its *capacity* as an optional (positive integer) second argument of `BOTTLE_CREATE` :

`BOTTLE(` *T* `) *`*bottle* ` = BOTTLE_CREATE (` *T* `, ` *capacity* `);`

- A *capacity* set to a positive integer defines a buffered queue of limited capacity.

    This could be used to manage tokens: *capacity* is then the number of available tokens.
    Call `BOTTLE_TRY_FILL` to request a token, and call `BOTTLE_TRY_DRAIN` to release a token.
    The bottle is then used as a container of controlled capacity.
    `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY` need not be used in this case.

- A *capacity* set to `UNLIMITED` defines a buffered queue of unlimited capacity (only limited by system resources).

    This could be useful to exchange data between unsynchronized (concurrent or sequential) treatments,
    some writing (with `BOTTLE_TRY_FILL`), other reading (with `BOTTLE_TRY_DRAIN`).
    The bottle is then used as a trivial thread-safe FIFO queue.
    `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY` need not be used in this case.

- A *capacity* set to `UNBUFFERED` defines an unbuffered queue (default).

*The usage of buffered queues is neither required nor recommended for thread synchronization.
It would partly unsynchronize threads and use a significant amount of memory.*

> size_t **BOTTLE_CAPACITY** (BOTTLE (*T*) \*bottle)

> size_t **BOTTLE_LEVEL** (BOTTLE (*T*) \*bottle)

`BOTTLE_CAPACITY` returns the capacity of the bottle.
`BOTTLE_LEVEL` returns the level of the bottle (or `0` for unlimited capacity).

## Exchanging messages between thread

Sender threads communicate synchronously with receiver threads by exchanging messages through the bottle:
the bottle has a mouth where it can be filled with messages and a tap from where it can be drained.

![alt text](Bottle.jpg "A classy FIFO message queue")

<small>(c) Davis & Waddell - EcoGlass Oil Bottle with Tap Large 5 Litre | Peter's of Kensington</small>

### Receiving messages

> int **BOTTLE_DRAIN** (BOTTLE (*T*) \*bottle, [*T*& message])
>
> *The second argument `message` is optional. If omitted, the bottle is drained but the message is not fetched and is lost.*
>
> *It is of type T&.
> It is passed as a **reference**, and not as a pointer to T.
> Therefore, it might be modified by the callee (just a little bit of macro magic here).*


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

### Sending messages

> int **BOTTLE_FILL** (BOTTLE (*T*) \*bottle, [*T* message])
>
> *The second argument is optional. If omitted, an arbitrary unspecified dummy message is used to fill the bottle.*

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
  (i.e. after the call to functions `BOTTLE_DRAIN` or `BOTTLE_TRY_DRAIN`), and after use of the message,
  deallocate all the resources of the message (those previously allocated by the sender).

## Closing communication

> void **BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY** (BOTTLE (*T*) \*bottle)

When concurrent threads are synchronized by an exchange of messages, the transmitter must informs receivers
when it has finished sending messages, so that receivers won't need to wait for extra messages.

As the bottle won't be filled any more, we can close the bottle:
the function `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY` will seal the mouth of the bottle
(i.e. close the transmitter side of the bottle),
unblock all receivers and wait for the bottle to be empty.
It:

1. prevents any new message from being sent in the bottle (just in case) :
  `BOTTLE_FILL` and `BOTTLE_TRY_FILL` will return 0 immediately (without blocking).
2. waits for the bottle to be emptied (by the calls to `BOTTLE_DRAIN` in the receivers).
3. asks for any blocked `BOTTLE_DRAIN` calls (called by the receivers) to unblock and to finish their job:
  `BOTTLE_DRAIN` will be asked to return immediately with value 0.

`BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`:

- acts as if it was sending an end-of-file in the bottle.

    Therefore, the call to `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY` *must be done*
    after the feeders have finished their work (either at the end of the feeder treatment,
    or sequentially just after the feeder treatment).

- waits for all the eaters to finish their work before returning.

After the call to `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`, eaters are still able to and *must* process the remaining messages
in the bottle.

The user program *must* wait for all the eaters to be finished before destroying the bottle.

As said above, `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY` is *only useful when the bottle is used to synchronize concurrent
threads* and need not be used in other cases (thread-safe shared FIFO queue).

## Destruction of a bottle

> void **BOTTLE_DESTROY** (BOTTLE (*T*) \*bottle)

Thereafter, once *all the receivers are done* in the user program, and the bottle is not needed anymore,
it can be destroyed safely with `BOTTLE_DESTROY`.

Note that `BOTTLE_DESTROY` does not call `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY` by default because the user program *should
ensure* that all receivers have returned from calls to `BOTTLE_DRAIN` between `BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`
and `BOTTLE_DESTROY` (usually, waiting for the receivers to finish with a `pthread_join` might suffice),
otherwise, some thread resources might not be released properly (mutexes and conditions).

## Other features

### Unblocking message queue functions

> int **BOTTLE_TRY_DRAIN** (BOTTLE (*T*) \*bottle, [*T* message])
>
> *The second argument is optional. If omitted, the bottle is drained but the message is lost.*

> int **BOTTLE_TRY_FILL** (BOTTLE (*T*) \*bottle, [*T* message])
>
> *The second argument is optional. If omitted, an arbitrary unspecified dummy message is used.*

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

> void **BOTTLE_PLUG** (BOTTLE (*T*) \*bottle)

> void **BOTTLE_UNPLUG** (BOTTLE (*T*) \*bottle)

Finally, the communication between sender and receiver threads can be stopped and restarted at will if needed.

The mouth of the bottle can be:

- plugged (stopping communication) with `BOTTLE_PLUG`,
- unplugged (to restart communication) with `BOTTLE_UNPLUG`.

### Hidden data

If the content of the messages is not needed, the argument *message* can be *omitted* in calls to
`BOTTLE_TRY_DRAIN`, `BOTTLE_DRAIN`, `BOTTLE_TRY_FILL` and `BOTTLE_FILL`.

In this case, a bottle is simply used as a synchronization method or a token counter.

# Source files

- [`bottle.h`](bottle.h) declares the user interface documented here.
- [`bottle_impl.h`](bottle_impl.h) defines the user programming interface.
- [`vfunc.h`](vfunc.h) is used by [`bottle.h`](bottle.h).

# Examples

All sources should be compiled with the option `-pthread`.

## Unbuffered bottle: Thread synchronization

### Simple example

The source below is a simple example of the standard use of the API for thread synchronization.

```c
#include <unistd.h>
#include <stdio.h>

#include "bottle_impl.h"
typedef const char * Message;
DECLARE_BOTTLE (Message)
DEFINE_BOTTLE (Message)

static void *
eat (void *arg)
{
  Message m;
  BOTTLE (Message) * bottle = arg;
  while (BOTTLE_DRAIN (bottle, m))
    printf ("%s\n", m);
  return 0;
}

int
main (void)
{
  BOTTLE (Message) * bottle = BOTTLE_CREATE (Message);

  pthread_t eater;
  pthread_create (&eater, 0, eat, bottle);

  Message police[] = { "I'll send an SOS to the world", "I hope that someone gets my", "Message in a bottle" };
  for (size_t i = 0; i < 2 * sizeof (police) / sizeof (*police); i++)
    BOTTLE_FILL (bottle, police[i / 2]), sleep (1);

  BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY (bottle);
  pthread_join (eater, 0);
  BOTTLE_DESTROY (bottle);
}
```

Comments:

- A bottle is created (`BOTTLE_CREATE`) to communicate synchronously between two threads, a transmitter and a receiver:
  it is a template container which type is `BOTTLE (Message)`, where `Message` is a user-deined type.
- The `eat` function (the receiver which calls `BOTTLE_DRAIN`) is in one thread `eater` (but there could be several), which pace is synchronized with the transmitter.
  That's what it's all about: synchonizing threads with messages !
- The sending loop (the transmitter which calls `BOTTLE_FILL`) is in the same thread (here the `main` thread) as the one which has created the bottle.
  This protects against the bottle being closed during the transmission phase. This is a good practice as it helps keep things easier.
- The program closes the bottle (`BOTTLE_CLOSE_AND_WAIT_UNTIL_EMPTY`)
  and waits for the eater thread to finish (`pthread_join (eater, 0)`)
  before destroying the bottle (`BOTTLE_DESTROY`).

### Advanced example

[`bottle_example.c`](bottle_example.c) is a complete example of a program (compile with option `-pthread`)
using a synchronized thread-safe FIFO message queue.

## Buffered bottle of limited capacity: Token management

Tokens can be managed with a buffered bottle, in this very naive model:

```c
#include <stdio.h>
#include "bottle_impl.h"
typedef int Token;
DECLARE_BOTTLE (Token)
DEFINE_BOTTLE (Token)

#define PRINT_TOKEN printf (" (%lu/%lu).\n", BOTTLE_LEVEL (tokens_in_use), BOTTLE_CAPACITY (tokens_in_use))
#define GET_TOKEN   printf ("Token requested: %s", BOTTLE_TRY_FILL (tokens_in_use) ? "OK" : "NOK")
#define LET_TOKEN   printf ("Token released:  %s", BOTTLE_TRY_DRAIN (tokens_in_use) ? "OK" : "NOK")

int main (void)
{
  BOTTLE (Token) * tokens_in_use = BOTTLE_CREATE (Token, 3);

  GET_TOKEN ; PRINT_TOKEN ;
  GET_TOKEN ; PRINT_TOKEN ;
  GET_TOKEN ; PRINT_TOKEN ;
  GET_TOKEN ; PRINT_TOKEN ;
  GET_TOKEN ; PRINT_TOKEN ;

  LET_TOKEN ; PRINT_TOKEN ;

  GET_TOKEN ; PRINT_TOKEN ;

  BOTTLE_DESTROY (tokens_in_use);
}
```

displays:

```
Token requested: OK (1/3).
Token requested: OK (2/3).
Token requested: OK (3/3).
Token requested: NOK (3/3).
Token requested: NOK (3/3).
Token released:  OK (2/3).
Token requested: OK (3/3).
```

Note how optional arguments *message* are omitted in calls to `BOTTLE_TRY_DRAIN` and `BOTTLE_TRY_FILL`.

## Buffered bottle of unlimited capacity: FIFO thread-safe queue

A bottle can be used as a basic buffered FIFO queue, thread-safe, sharable between several treatements,
being (like here) or not in the same thread.

```c
#include <stdio.h>
#include "bottle_impl.h"
DECLARE_BOTTLE (int)
DEFINE_BOTTLE (int)

static void write (BOTTLE (int) *b)
{
  for (int i = 1 ; i <= 3 ; i++)
    BOTTLE_TRY_FILL (b, i);
}

static void read (BOTTLE (int) *b)
{
  int i;
  while (BOTTLE_TRY_DRAIN (b, i))
    printf ("%i\n", i);
}

int main (void)
{
  BOTTLE (int) * fifo = BOTTLE_CREATE (int, UNLIMITED);

  write (fifo);
  read (fifo);

  BOTTLE_DESTROY (fifo);
}
```

**Have fun !**
