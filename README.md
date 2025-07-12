# Procuder/consumer pattern for thread communication and/or synchronisation in C.

> The ultimate synchronisation mechanism between threads through messages (channels as in Go), for C language, extended.

> This project implements a communication and synchronisation mechanism between producer and consumer threads.

## General description

There's a famous concept in Go called channels :

  - Some threads can produce and send messages, which are received and consumed by other threads.
  - Exchanged message are strongly type -- they are bound to a chosen (or void) type of objects specific to each channel of communication.

The pattern of message driven synchronisation is of interest because it is intuitive and of a higher level
of abstraction than mutexes and conditions:

- some threads `A` (usually called the senders or the producers) fill a queue of messages ;
- some other threads `B` (usually called the receivers or the consumers) eat up the queue of messages.

Threads B are gracefully synchronised with threads A.

Therefore, I grabbed my copy of the (still great) book "Programming with POSIX threads" by Butenhof, where the pattern is mentioned, to
implement it for my favourite language, C.

And here it is, with some improvements compared to Go : communication channels can be synchronised with an adjustable constraint:

  - either tight: producer (or team of producers) and consumer (or team of consumers) work at same pace ;
  - or loose : producer (or team of producers) and consumer (or team of consumers) work at their own pace ;
  - or anything in between.

Under the hook, this pattern is a thread-safe FIFO message queue suited for thread synchronisation.

It hides mutexes and thread synchronisation complexity behind simple objects, called *bottles*, similar for example to the concept of *channels* found in language Go
(which shows a nice and minimalist grammar and overall simplicity, as compared to C++ or Java).

## Quick start

Here are three use cases.

### Use case #1 : for thread synchronisation between threads

The API is simple :

  1. define the type of messages which can be exchanged through the channel, here called *bottle* ;
  2. declare and define the associated type of channel (*bottle*) ;
  3. create a channel ;
  4. send messages ;
  5. close the channel ;
  6. receive messages ;
  7. destroy the channel.

#### 1. Define the type of messages to exchange

If the type of messages is a basic type (int, double, char, ...), this step is not needed.

Otherwise, define the type with a single word identifier.

For instance:

```c
typedef int *pi_t;
typedef longle double ld_t;
typedef int (*f_i2i_t) (int);
typedef struct { ... } msg_t;
```

#### 2. Declare and define the type of channel to use

If the type of message to exchange is `msg_t` as defined on step 1, add this on top of your file:

```c
#include "bottle_impl.h"        // Include necessary stuff
bottle_type_declare (msg_t);    // Declare the template of bottle
bottle_type_define (msg_t);     // Define the template of bottle
```

#### 3. Create the bottle on the sender side

On the sender side, usually the main thread, create a bottle:

```c
bottle_t (msg_t) * bottle = bottle_create (msg_t);
```

#### 4. Send messages (on the sender side obviously)

If `msg` is a variable of type `msg_t`:

```c
msg_t msg;
/* initialise msg here */
bottle_send (bottle, msg);
```

#### 5. Close the bottle on the sender side once all messages have been sent

On the sender side, usually the main thread, create a bottle:

```c
bottle_close (bottle);
```

#### 6. Receive messages (on the receiver side obviously)

This step runs synchronously with step 4 on the receiver threads (one or several).

If `msg` is a variable of type `msg_t`:

```c
msg_t msg;
while (bottle_recv (bottle, &msg))
{
  ...
}
```

The while loop will not end before the bottle is closed with `bottle_close`.

#### 7. Destroy the bottle on the sender side after all messages have been received by the receiver threads

On the sender side, usually the main thread, create a bottle:

```c
bottle_destroy (bottle);
```

#### Notes

  - The bottle should usually be destroyed (`bottle_destroy`) by the thread that created it.
  - The bottle should not be destroyed before all the senders and receivers have stopped using it.
    `pthread_join` could be used to wait for sender and receiver threads to finish.
  - If there are several senders, each sender should stop sending messages if `bottle_send` or `bottle_try_send` return `0` with `errno` set to `ECONNABORTED`.
  - The bottle could as well be closed (`bottle_close`) by a receiver (there can be several) to ask the senders (there can be several) to stop sending messages.
    Senders should then stop sending messages if `bottle_send` or `bottle_try_send` return `0` with `errno` set to `ECONNABORTED`.
  - In a never ending process (such as a service or the back-end side of an application), `bottle_close` and `bottle_destroy` may not be called.

#### Example

Look at [bottle_simple_example.c](examples/bottle_simple_example.c).

### Use case #2 : as a counting semaphore to control access to a common resource by multiple threads

#### 1. Create the semaphore

Let's create a counting semaphore of ten tokens.
The type `char` of the message queue is unimportant.

After the prerequisite declaration:

```c
#include "bottle_impl.h"
bottle_type_declare (char);
bottle_type_define (char);
```

the semaphore can be initialised with:

```c
bottle_t (char) * sem = bottle_create (char, 10);
while (bottle_try_send (sem));
```

#### 2. Request a token

Blocks and waits if no tokens are available.

```c
bottle_recv (sem);
```

#### 3. Release a token

```c
bottle_send (sem);
```

#### 4. Destroy the semaphore

```c
while (bottle_try_rcv (sem));
bottle_destroy (sem);
```

#### Example

Look at [semaphore.c](examples/semaphore.c).

### Use case #3 : as a FIFO message queue

After the prerequisite declaration:

```c
#include "bottle_impl.h"
bottle_type_declare (msg_t);
bottle_type_define (msg_t);
```

#### 1. Create the FIFO message queue
```c
bottle_t (msg_t) * fifo = bottle_create (msg_t, capacity);
```

where `capacity` can be either a positive integer or `UNLIMITED`.

#### 2. Send a message to the FIFO queue

```c
errno = 0;
if (!bottle_try_send (fifo, i))
{
  if (!errno))   // the queue is full
    ...
  else
    ...          // the queue is closed
}
```

#### 3. Receive a message from the queue
```c
errno = 0;
if (!bottle_recv_send (fifo, i))
{
  if (!errno))   // the queue is empty
    ...
  else
    ...          // the queue is closed
}
```

#### 4. Close and destroy the queue

```c
bottle_close(fifo);
bottle_destroy (fifo);
```

#### Example

Look at [bottle_fifo_example.c](examples/bottle_fifo_example.c).

**Have fun !**

## Synopsis of the user interface

Before use:
1. include header file `bottle_impl.h`.
2. instantiate a template of a given type of messages with `bottle_type_declare` and `bottle_type_define`.

For instance, to exchange message texts between threads, start code with:
```c
#include "bottle_impl.h"
typedef const char * TextMessage;   // A user-defined type of messages.
bottle_type_declare (TextMessage);  // Declares the usage of bottles for the user-defined type.
bottle_type_define (TextMessage);   // Defines the usage of bottles for the user-defined type.
```

| Description |||
|-|-|--|
|**Declaration and definition of bottle type** |
||Type declaration      | `bottle_type_declare(`*T*`)`
||Type definition       | `bottle_type_define(`*T*`)`
||Type                  | `bottle_t(`*T*`)`
|**Allocation of a bottle** |
|*Dynamic allocation*   |
||Create                | `bottle_create`
||Destroy               | `bottle_destroy`
|*Automatic allocation* |
||Declare and create    | `bottle_auto`
|**Sending and receiving** |
|*Blocking* |
||Send message          | `bottle_send`
||Receive message       | `bottle_recv`
|*Non blocking* |
||Try sending message   | `bottle_try_send`
||Try receiving message | `bottle_try_recv`
|**Closing** |
||Close sending channel | `bottle_close`
|**Halting** |
||Plug                  | `bottle_plug`
||Unplug                | `bottle_unplug`

At creation (with `bottle_create` or `bottle_auto`), the capacity of the bottle can be optionally specified with an extra argument.

| Capacity | Behaviour of communicating threads |
|----------|-----------|
| `0` or `UNBUFFERED` (default) | Rendez-vous between threads. Threads are strictly synchronised (recommended).
| `1`         | Threads run at same pace and are loosely synchronised.
| > `1`       | Threads are not synchronised.
| `UNLIMITED` | Threads are not synchronised. The capacity of the queue grows as needed (not recommended). Therefore, threads sending messages never block.

Whatever the capacity,

- messages are guaranteed to be received in the order they were sent ;
- messages are never lost (except if a bottle would erroneously be destroyed while still in use).

See below for details.

### Source files

  - [`bottle.h`](bottle.h) defines the user interface documented here.
  - [`bottle_impl.h`](bottle_impl.h) implements the user programming interface.
  - [`vfunc.h`](vfunc.h) is used by [`bottle.h`](bottle.h). It permits usage of optional parameters in function signatures.
    Functions `bottle_create`, `bottle_auto`, `bottle_recv`, `bottle_send`, `bottle_try_recv`, `bottle__try_send` accept an optional argument, that be be *omitted*.
    This is brought by the use of the tricky macro VFUNC defined in `vfunc.h` (see http://stackoverflow.com/questions/11761703/overloading-macro-on-number-of-arguments).

In case a library interface would expose a bottle,

  - [`bottle.h`](bottle.h) should be included in the header of the library ;
  - [`bottle_impl.h`](bottle_impl.h) should be included in the implementation of the library.

## About thread communication through messages

Threads can communicate and synchronise using the producer/consumer pattern. Consumer threads (possibly several) trigger when they receive
messages sent by producer threads (possibly several).

Each message is consumed by at most one consumer thread.

Messages are managed in a thread-safe message queue (called *bottle* thereafter).

The size of the message queue can be chosen at creation of a bottle with an optional argument *capacity* :

  - 0, `UNBUFFERED` or `DEFAULT` (unbuffered, à la Go) : threads exchanging messages are then *strictly synchronised*. This is the default and *recommended* option.

      - Communication succeeds only when the sender and receiver are both ready.
        Think of it as a hand delivery between the sender and the receiver. Both have to meet and to wait for each other.
      - It defines a meeting point that guaranties that a sender and a receiver in *two different threads* have to be ready in order to exchange a message.
        A sending thread can't receive its own message.
      - When a message is sent on the synchronous channel, it blocks the calling sending thread until there is another thread attempting to receive a message
        from the channel, at which point the receiving thread gets the message and both threads continue execution.
      - When a message is received from the channel, it blocks the calling receiving thread until there is another thread attempting to send a message on the
        channel, at which point both threads continue execution.

  - 1 (buffered, à la Rust) : threads exchanging messages are then *loosely synchronised*.

      - Senders and receivers threads run at the same pace, messages being alternatively sent and received (thus synchronously).
        Think of it as a delivery of a package in a mailbox: the sender and the receiver don't need to meet but they have to keep coordinated.
      - A sender thread can send a message without waiting for a receiver to read it : the receiver might read the pending message later (thus loose synchronicity).
      - A thread could receive a message it has previously sent (should a thread send and receive messages through the same bottle, which is unusual).
        This might lead to races between the sender and the receiver (see the example [`bottle_perf.c`, test2](examples/bottle_perf.c)).

  - N >= 2 : threads are *not synchronised*.

      - Sending threads block if the number of pending message would exceed N.

  - `UNLIMITED` : threads are *not synchronised* as above but the size of the queue grows and shrinks as needed.

      - Threads sending messages *never block*.

### Unbuffered message queue

The created bottles are *unbuffered by default*.
Such an unbuffered queue is suitable for a tight synchronisation between threads running concurrently whereas buffered channels are not.

See [here](https://wingolog.org/archives/2017/06/29/a-new-concurrent-ml) for an analysis of thread synchronisation through messages, which reads something like:

>  An asynchronous (buffered) queue is not a channel, at least in its classic sense. As they were originally formulated in "Communicating Sequential Processes" by Tony Hoare,
>  channels are meeting-places.
>  Processes meet at a channel to exchange values; whichever party arrives first has to wait for the other party to show up.
>  The message that is handed off in a channel send/receive operation is never "owned" by the channel; it is either owned by a sender who is
>  waiting at the meeting point for a receiver, or it's accepted by a receiver. After the transaction is complete, both parties continue on.
>
>  Meeting-place (unbuffered) channels are strictly more expressive than buffered channels.
>  - In Go (and in this herewith implementation in C), whose channels are unbuffered by default, you can use a channel for *deterministic* thread communication.
>  - On the contrary, implementation of channels in Rust are effectively a minimum buffer size of 1 and therefore are not strict CSP.

See the example [`bottle_perf.c`, test2](examples/bottle_perf.c) for an illustration of this discussion.
Also read [here](https://stackoverflow.com/a/53348817) and [there](https://users.rust-lang.org/t/0-capacity-bounded-channel/68357) for more.

### Buffered message queue

Even if unbuffered queues are required for thread synchronisation, buffered queues can be needed for specific use cases:

- In case of a very high rate (more than 100k per second) of exchanged messages
  between the sender thread and the receiver thread,
  where context switch overhead between threads would be counter-productive. The buffer capacity will allow to process
  sending and receiving messages by chunks, therefore reducing the number of context switch.
- In case the receiver is slower to process received messages than the sender to send them.
  Using a buffered message queue avoids blocking the sender inadequately,
  where an unbuffered queue would tune the pace of the sender on the one of the receiver, slowing down the sender thread.

The capacity of the message queue is tunable, from 1 to infinity:

- A *capacity* set to a positive integer defines a buffered queue of fixed and limited capacity.

    This configuration relaxes the synchronisation between threads.

    This could also be used to manage tokens: *capacity* is then the number of available tokens.
    Call `bottle_try_send` to request a token, and call `bottle_try_recv` to release a token.
    The bottle is then used as a container of controlled capacity.
    `bottle_close` need not be used in this case.
    There is such an example below.

- A *capacity* set to `UNLIMITED` defines a queue of (almost) infinite capacity.

  This configuration allows easy communication between producers and consumers without any constraints on synchronisation.
  It can be useful for asynchronous I/O for example, as in [`hanoi.c`](examples/hanoi.c).

  It allocates and desallocates capacity dynamically as needed for messages in the pipe between producers and consumers.
  It should be used with care as it could exhaust memory if the producer rate exceeds dramatically the consumer rate.

  This option is not available if `LIMITED_BUFFER` is defined as a macro at compile-time.

Buffered queues are implemented as pre-allocated arrays rather than as conventional linked-list:
elements of the array are reused to transport all messages,
whereas with a linked list, each message would require a dynamically allocated new element in the list, adding memory management overhead.
This design is inspired by the [LMAX Disruptor pattern](https://lmax-exchange.github.io/disruptor/).

*As said before, the usage of buffered queues is neither required nor recommended for thread synchronisation, as it would partly unsynchronise threads and uses a larger amount of memory ; `DEFAULT` unbuffered queues are suitable for most purposes.*

## Description of the user interface

Hereafter, *T* is a type, either standard or user defined.

### Declaration (allocation) of bottles

 Bottles can be created either dynamically or with an automatically allocation.

#### Creation of a bottle, dynamically

```c
bottle_t (T) *bottle_create (T, [size_t capacity = DEFAULT])
```
> *The second argument is optional (it can be omitted) and defaults to `DEFAULT` for an unbuffered bottle (see **About thread communication through messages** above).*

To create a **buffered** bottle, pass its *capacity* (as a second argument) to `bottle_create` (either a positive integer or `UNLIMITED`).

But if there is no special reason to use a buffered bottle, **an unbuffered bottle should be used**.

To transport messages of type *T*, `bottle_create` creates a pointer to a dynamically allocated message queue (usually on the sender side).

*T* could be any standard or user defined (`typedef`) type, simple or composed structure (`struct`).

For instance, to create a pointer to a message queue *b* for exchanging integers between threads, use:

`bottle_t (int) *b = bottle_create (int);`

The message queue is **a strongly typed** (it is a hand-made template container) FIFO queue.

##### Destruction of a bottle dynamically allocated

```c
void bottle_destroy (bottle_t (T) *bottle)
```

Thereafter, once *all the receivers are done* (see **Closing communication** below) in the user program,
and the bottle is not needed anymore, it can be destroyed safely with `bottle_destroy`.

Notes:

  - The bottle should be destroyed (`bottle_destroy`) by the thread that created it.
  - The bottle should not be destroyed before all the senders and receivers have stopped using it.
    `pthread_join` could be used to wait for sender and receiver threads to finish.
  - In a never ending process (such as a service or the back-end side of an application), `bottle_destroy` may not be called.

#### Declaration of local (automatic) variable

Rather than creating pointers to bottles, local variables of type bottle_t (*T*) can as well be declared and initialised with `bottle_auto` (usually on the sender side).
These variables behave like automatic variables: resources are automatically allocated at declaration and deallocated at end of scope.

```c
bottle_auto (variable_name, T, [size_t capacity = DEFAULT])
```

> `bottle_auto` can only be applied to auto function scope variables; it may not be applied to parameters
> or variables with static storage duration.

Note:

- `bottle_auto` is only available with compilers `gcc` and `clang` as it makes use of the variable attribute `cleanup`.
- `bottle_auto (b, int, 3)` is similar to what could be `bottle_t<int> b(3)` in C++ (if template class bottle_t were declared).

Here is an example of automatic allocation. The variable *b* is declared as a `bottle_t (time_t)` and initialised.
It is destroyed when the variable goes out of scope.

```c
#include <time.h>
#include "bottle_impl.h"

bottle_type_declare (time_t);
bottle_type_define (time_t);

int main (void)
{
  bottle_auto (b, time_t, 1);    // Declare an automatic variable b of type bottle_t (time_t)
  bottle_send (&b, time (0));    // Send a message through the bottle
  time_t val;
  bottle_recv (&b, &val);        // Receive a message through the bottle and store it in `val`
  // b is deallocated automatically when going out of scope.
}
```

### Exchanging messages between threads

Sender threads communicate with receiver threads by exchanging messages through the bottle:
the bottle has a mouth where it can be filled with messages and a tap from where it can be drained.

![alt text](Bottle.jpg "A classy FIFO message queue")

<small>(c) Davis & Waddell - EcoGlass Oil Bottle with Tap Large 5 Litre | Peter's of Kensington</small>

#### Receiving messages

```c
int bottle_recv (bottle_t (T) *bottle, [T *message])
```
> *The second argument `message` is an optional variable name. If omitted, the bottle is drained but the message is not fetched and is lost.*

The receivers can receive messages (draining form the tap), as long as the bottle is not empty and not closed, by calling `bottle_recv`.
The received message feeds the variable `message`.

- `bottle_recv` returns 0 immediately (with `errno` set to `ECONNABORTED`) if there is no data to receive
  **and** the bottle was previously closed (by `bottle_close`, see below).

    This condition (returned value equal to 0) should be handled
    by the receivers to detect the end of communication between threads (end of reception of data from the bottle).

- If there is data to receive (and even if the bottle has been closed in the meantime), `bottle_recv` receives a *message* from the bottle and returns 1 immediately.

  Messages are received in the **exact order** they have been sent, whatever the capacity of the buffer defined by `bottle_create`.

- Otherwise (there is no data to receive and the bottle is not closed), `bottle_recv` waits
  until there is data to receive. *It is precisely this behaviour that ensures synchronicity between the sender and the receiver*.

Therefore `bottle_recv` returns 1 if a message has been successfully received from the bottle, 0 otherwise.

#### Sending messages

```c
int bottle_send (bottle_t (T) *bottle, [T message])
```
> *The second argument is optional. If omitted, an arbitrary unspecified dummy message is used to fill the bottle.*

The senders can send messages (filling in through the mouth of the bottle)
by calling `bottle_send (`*bottle*`, `*message*`)`, as long as the mouth is open
and the bottle is not closed.

- `bottle_send` waits in those cases:

    - If the bottle was plugged (by `bottle_plug`, see below), `bottle_send` blocks until the bottle is unplugged (by `bottle_unplug`).
    - If the message queue is **unbuffered** (`UNBUFFERED` or `DEFAULT`), `bottle_send` blocks until some receiver is ready to receive (with `bottle_recv` or `bottle_try_recv`)
      the sent value.
      *It is precisely this behaviour that ensures synchronicity between the sender and the receiver*.
    - If the message queue  is **buffered** and the buffer is of limited capacity and full,
      `bottle_send` blocks until some receiver has retrieved at least one value previously sent (with `bottle_recv` or `bottle_try_recv`).
      
      Note: If the bottle has an `UNLIMITED` capacity, it can (almost) never be full as its capacity increases automatically as necessary to accept new messages (like a skin balloon).

- `bottle_send` returns 0 (with `errno` set to `ECONNABORTED`) in those cases:

    - immediately, without waiting, if the bottle was previously closed (by `bottle_close`, see below).
    - as soon as the bottle is closed (by `bottle_close`) while waiting.

      This returned value might indicates an error in the logic of the user program as
      it should be avoided to close a bottle while senders are still using it.

      Anyhow, this condition (returned value equal to 0) should be handled
      by the senders (which should stop the transmission of any data to the bottle).
      A sender should stop sending messages if `bottle_send` returns `0` with `errno` set to `ECONNABORTED`.

- In other cases, `bottle_send` sends the message in the bottle and returns 1.

Therefore `bottle_send` returns 1 if a message has been successfully sent in the bottle, 0 otherwise.

#### Resources management

Messages are *passed by value* between the senders and receivers: when a message is sent in or received from a bottle, it is *copied*.
This guarantees thread-safety and independence between the sender thread and the receiver thread.

  - whatever the changes made by the sender on a message *after it was sent*, it will not affect the message received by the receiver:
    the received message is an exact copy of the message as it was at the moment it was sent.
  - whatever the changes made by the receiver on a received message, it will not affect the message sent by the sender.

Nevertheless, in particular and unusual case where the message type *T* would be or would contain (in a structure)
pointers to allocated resources (such as memory with `malloc`/`free` or file descriptor with `fopen`/`fclose` for instance),
the user program must respect those simple rules:

- The **sender**:

    - *must* allocate resources of the message before sending it (i.e. before the call to functions `bottle_send` or `bottle_try_send`);

    - *should not* try to access those allocated resources after the message has been sent (i.e. after the call to functions `bottle_send` or `bottle_try_send`).

    Indeed, as soon as a message is sent, it is owned by the receiver (which could modify it)
    and **does not belong to the sender anymore**.


- The **receiver** *must*, after use of the received message (with a call to functions `bottle_recv` or `bottle_try_recv`),
  deallocate (release) all the resources of the message (those previously allocated by the sender).

### Closing communication

```c
void bottle_close (bottle_t (T) *bottle)
```

When concurrent threads are synchronised by an exchange of messages, the senders must inform the receivers
when they have finished sending messages, so that receivers won't need to wait for extra messages (see `bottle_recv` above).

In other words, as soon as all messages have been sent through the bottle (it won't be filled with any more messages),
the bottle can be closed on the sender side and the receivers will be notified.

To do so, the function `bottle_close` seals the mouth of the bottle
(i.e. closes the transmitter side of the bottle permanently),
and unblocks all receivers waiting for messages.

The call to `bottle_close`, *on the sender side*:

1. prevents any new message from being sent in the bottle (just in case) :
  `bottle_send` and `bottle_try_send` will return 0 immediately (without blocking).
2. and then asks for any remaining blocked calls to `bottle_recv` (called by the receivers) to unblock and
   to finish their job:
   all pending calls to `bottle_recv` will be asked to return immediately with value 0 (see above) if there are no more messages to receive.

`bottle_close` acts as if it were sending an end-of-file in the bottle.

Notes:

- Once closed, a bottle can't be reopen.
  Therefore, the call to `bottle_close` *must be done*
  after the senders have finished their work (either at the end
  or sequentially just after the sender treatment).

- `bottle_close` can be called several times without any arm and without any further effect.

- After the call to `bottle_close` by the sender, receivers are still able to (and *should*) process the remaining messages in the bottle to avoid any memory leak due to unprocessed remaining messages.

- The user program *must* wait for all the receiver treatments to finish
  (usually, waiting for the receivers to finish with a `pthread_join` on the sender side might suffice)
  before destroying the bottle (with `bottle_destroy`).

- The bottle could as well be closed (`bottle_close`) by a receiver (there can be several) to ask the senders (there can be several) to stop sending messages.
  Senders should then stop sending messages if `bottle_send` or `bottle_try_send` return `0` with `errno` set to `ECONNABORTED`.

- As said above, `bottle_close` is *only useful when the bottle is used to synchronise concurrent
threads* on sender and receiver sides and need not be used in other cases (thread-safe shared FIFO queue).

- In a never ending process (such as a service or the back-end side of an application), `bottle_close` may not be called.

- `bottle_close` does not call `bottle_destroy` by default because the user program *should
ensure* that all receivers have finished their work between `bottle_close`
and `bottle_destroy`.

### Other features

#### Non-blocking message queue functions

```c
int bottle_try_recv (bottle_t (T) *bottle, [T *message])
```
> *The second argument is optional. If omitted, the bottle is drained but the message is lost.*

```c
int bottle_try_send (bottle_t (T) *bottle, [T message])
```
> *The second argument is optional. If omitted, an arbitrary unspecified dummy message is used.*

These are non-blocking versions of the filling and draining functions.

These functions return immediately without blocking. *They are **not** suited for thread synchronisation* and are of limited use.

When used, the message queue looses its thread-synchronisation feature and
behaves like a simple thread-safe FIFO message queue:

- Receivers can receive messages without blocking with `bottle_try_recv`.

    - `bottle_try_recv` returns 0 (with `errno` set to `ENOTSUP`) if the bottle capacity is `UNBUFFERED` (unauthorised).
    - `bottle_try_recv` returns 0 (with `errno` set to `ECONNABORTED`) if the bottle is empty and closed.

      A receiver should stop receiving messages if `bottle_try_recv` returns `0` with `errno` set to `ECONNABORTED`.

    - `bottle_try_recv` returns 0 if the bottle is empty.
      This indicates that a call to `bottle_recv` would have blocked.
    - Otherwise, it receives a *message* (it modifies the value of the second argument *message*)
      from the bottle and returns 1.

- Senders can send messages without blocking with `bottle_try_send`.

    - `bottle_try_send` returns 0 (with `errno` set to `ENOTSUP`) if the bottle capacity is `UNBUFFERED` (unauthorised).
    - `bottle_try_send` returns 0 (with `errno` set `ECONNABORTED`) if the bottle is closed.

      This might indicates an error in the user program as it should be avoided to close a bottle
      while senders are still using it.

      A sender should stop sending messages if `bottle_try_send` returns `0` with `errno` set to `ECONNABORTED`.

    - `bottle_try_send` returns 0 if the bottle is plugged or already full.
      This indicates that a call to `bottle_send` would have blocked.
    - Otherwise, it sends a *message* in the bottle and returns 1.

#### Halting communication

```c
void bottle_plug (bottle_t (T) *bottle)
void bottle_unplug (bottle_t (T) *bottle)
```

The communication between sender and receiver threads can be stopped and restarted at will if needed.
This could be used by the receiver or a third thread to control the sender.

The mouth of the bottle can be:

- plugged (stopping communication) with `bottle_plug`,
- unplugged (to restart communication) with `bottle_unplug`.

When plugged, sent messages are block even though the bottle is not full and the receiver is ready.
Messages are unblocked as soon as the bottle is unplugged.

Note that the tap of the bottle remains open in order to keep receiving previously sent messages.

`bottle_plug` and `bottle_unplug` can be called several times in a row without arm and without effect.

#### Hidden data

If the content of the messages is not needed, the argument *message* can be *omitted* in calls to
`bottle_try_recv`, `bottle_recv`, `bottle_try_send` and `bottle_send`.

In this case, a bottle is simply used as a synchronisation method or a token counter.

## Examples

All [examples](examples) should be compiled with the option `-pthread`.
See [Makefile](examples/Makefile) for details.

### Unbuffered bottle

#### Thread synchronisation

[`bottle_example.c`](examples/bottle_example.c) is a complete example of a program using a synchronised thread-safe FIFO message queue.

### Buffered bottle

#### MT safe FIFO

Buffered bottles behave like a [multi-thread safe FIFO queue](examples/bottle_fifo_example).

#### High performance message exchanges

When high performance of exchanges is required between threads (more than 100 000 messages per seconds), a *buffered* bottle is a good choice (reminder: in other cases, an unbuffered bottle is far enough.)

This enhances performance by a factor of about 40, because it cuts off the concurrency overhead.
In the example [`bottle_perf.c`](examples/bottle_perf.c), it takes about 20 seconds to exchange 25 millions messages between asynchronous threads (on my computer with 4 cores.)

[`bottle_perf.c`](examples/bottle_perf.c) also shows how races can occur in case of *buffered* bottles.

#### Token management

Tokens can be managed with a buffered bottle, in the very naive model of the exampla [`bottle_token_example.c`](examples/bottle_token_example.c).

Note:

- the use of a local automatic variable `tokens_in_use` declared by `bottle_auto`,
- how optional arguments *message* are omitted in calls to `bottle_try_recv` and `bottle_try_send`.

#### FIFO

[`hanoi.c`](examples/hanoi.c) demonstrates that the order of the received messages is the very same as the order of the messages sent in the bottle:

  - the main thread (`main`) solves the Towers of Hanoï (recursively), determining the sequence of moves,
    and transmitting theses moves to another thread through an unlimited bottle (it could nevertheless be unbuffered or of limited size with the same result).
  - another thread mimics blindly the moves as told by the main thread, in the exact same order to achieve the same result.

## Under the hood (internals)

Each instance of a bottle :

  - contains a FIFO queue `queue` (see below.)
  - two states `closed` and `frozen` indicating a bottle was closed or plugged respectively.

### Thread synchronisation

The pattern for a thread-safe FIFO queue (that's what a bottle is) is explained by Butenhof in his book "Programming with POSIX threads".

Each instance of a bottle is protected by a mutex for thread-safeness and two conditions for synchronisation.

```c
    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
```

Each time the bottle is accessed for modification (plugging and unplugging, filling and emptying, closing),
the action is guarded by the mutex (call of `pthread_mutex_lock/unlock` on `mutex`).

Each time a message is sent in the bottle,
  - if the bottle is already full, the caller waits until the bottle is not full anymore or is closed (call of `pthread_cond_wait` on `not_full`).
  - the bottle signals it is not empty (call of `pthread_cond_signal` on `not_empty`)

Each time a message is received from the bottle,
  - if the bottle is empty, the caller waits until the bottle is filled in or closed (call of `pthread_cond_wait` on `not_empty`).
  - the bottle signals it is not full (call of `pthread_cond_signal` on `not_full`)

When the bottle is closed, it signals it is not empty (call of `pthread_cond_signal` on `not_empty`) and not full (call of `pthread_cond_signal` on `not_full`)
so that pending senders and receivers are unblocked.

### Message queue buffer... ring

The FIFO queue of a bottle is defined as:

```c
    struct {
      TYPE*   buffer;      // Array containing the messages (at most capacity)
      size_t  capacity;    // Maximum number of elements in the queue (size of the array)
      int     unlimited;   // Indicates that the capacity can be extended automatically as required
      size_t  size;        // Number of messages currently in the queue (<= capacity)
      TYPE*   reader_head; // Position where to read the next value
      TYPE*   writer_head; // Position where to write the next value
    }
```

where:
  - `TYPE` is the type of messages stored in the queue (this type is defined at creation of the bottle, see below.)
  - `capacity` is the capacity of the buffer (equal to 1 for `UNBUFFERED` bottles.)
  - `size` is the number of messages currently in the bottle (sent but not read yet.)
  - `unlimited` indicates (when non zero) that the capacity can be extended automatically as required (see below.)

When a message is added in the queue, it is copied at some position in the buffer (`*pos = message`).
When a message is removes from the queue, it is copied from some position in the buffer (`message = *pos`).

Let's consider a naive implementation of the buffer where written messages are appended, starting from the beginning of the buffer.
  1. After writing messages *a*, *b* ,*c* in a buffer of size 4, the buffer would look like `[abc_]` where `_` indicates an empty position.
  1. After reading the first message (*a*), we get `[bc__]`.
  1. After writing a message *d* (appended at the end of the buffer), we get `[bcd_]`.
  1. After reading the next message (*b*), we get `[cd__]`.
  1. After writing a message *e* (appended at the end of the buffer), we get `[cde_]`.
  1. After reading the two next messages (*c* and *d*), we get `[e___]`.

This requires shifting all the elements of the buffer every time it is read, which is time consuming (linear time with buffer size).

To avoid shifting and to work at constant time whatever the size of the buffer,
it is better to keep track of the next position where to write (the `writer_head`) and where to read (the `reader_head`):
  1. After writing messages *a*, *b* ,*c* in an empty buffer of size 4, the buffer looks like `[abc_]`.
     The writer head is on the next position after *c*. The reader head is on the first position (*a*).
  1. After reading the first message (*a*) at the reader head position, we get `[_bc_]`.
     The reader head moves to the next position after *a* (*b*).
  1. After writing a message *d* (appended at the writer head position), we get `[_bcd]`.

     Instead of shifting the entire buffer to create space at the end of the buffer for the next message (`[_bcd]` to `[bcd_]`),
     the writer head moves to the next position after *d*, which is at the beginning of the buffer.

     The buffer is actually a *ring* : when a header reaches the end of the buffer, it overflows at the beginning of the buffer,
     ready for the next message to be written.
     This permits an easy recycling of the buffer without shifting involved. 

  1. After reading the next message (*b*), we get `[__cd]`. The reader head moves on *c*.
  1. After writing a message *e* ( at the writer head position), we get `[e_cd]`. The writer head moves to the next position after *e*.
  1. After reading the two next messages (*c* and *d*), we get `[e___]`.
     The reader head overflows at the beginning of the buffer, on the first position, ready for the next message to be read (*e*).
  1. After writing messages *f*, *g*, *h* (appended at the writer head position), we get `[efgh]`.
     The writer head overflows and moves at the beginning of the buffer, on the first position.

     Therefore, the reader and writer heads do overlap *after writing*, indicating that the buffer is *full*.

  1. After reading the two next messages (*e* and *f*), we get `[__gh]`.
     The reader head  moves on *g*.
  1. After writing a message *i* (at the writer head position), we get `[i_gh]`.
     The writer head moves to the position after *i*.
  1. After reading the three next messages (*g*, *h* and *i*), we get `[____]`.
     The reader head overflows and moves to the second position.

     Therefore, the reader and writer heads do overlap *after reading*, indicating that the buffer is *empty*.

  1. After writing a message *j* (at the writer head position), we get `[_j__]`.

As said, the buffer is actually a ring:
  - Reader and writer heads overflow at the beginning of the buffer when they reach its end.
  - A message is written at writer head position, after which the writer head is moved to the next position.
  - A message is read at reader head position, after which the reader head is moved to the next position.
  - The buffer is full when the writer head reaches the reader head position *after writing* (i.e. sending a message in the bottle).
  - The buffer is empty when the reader head reaches the writer head position *after reading* (i.e. receiveing a message from the bottle).

### Buffer of unlimited capacity

When a bottle is declared with an infinite (`UNLIMITED`) capacity, it is automatically expanded when the bottle is full ;
more space is created in the buffer:

  1. Suppose the buffer is `[__ab]`.
  2. When *c* and *d* are written, the buffer goes `[cdab]` and the reader and writer heads are both on *a*, indicating that the buffer is full.
  3. If *e* is written,
     - before writing, space is added between the read head and the writer head: `[cd____ab]` ;
     - the reader head is then on *a*, and the writer head after *d* ;
     - *e* can then be written: `[cde___ab]`

The number of empty spaces created is ruled by the (macro) function `QUEUE_UNLIMITED_CAPACITY_GROWTH_RULE`
which yields the new overall capacity after expansion from the actual capacity.

By default, the capacity is doubled:

```c
#define QUEUE_UNLIMITED_CAPACITY_GROWTH_RULE(capacity) ((capacity)*2)
```

Therefore, in the previous example, the capacity is expanded from 4 to 8, creating 4 new positions in the buffer.

On the opposite, the buffer is shrunk after reading in case the overall capacity exceeds `QUEUE_UNLIMITED_CAPACITY_GROWTH_RULE(size)`
where `size` is the number of messages in the buffer. All empty positions are removes before the reader head and after the writer head
(this leading to a full buffer.)

### Bottle template

This section explains how the type of the messages can be defined at creation of the bottle (at compile-time) and
how things like `bottle_t(int) *b = bottle_create(int)` and `bottle_send (b, 5)` work (the C-like style is used here). 

`bottle_t(` *TYPE* `)` is in fact a template that is instantiated at compile-time with the type *TYPE* specified (it uses some kind of genericity.)

When `bottle_type_declare(int)` and `bottle_type_define(int)` are added in the global part of the code,
it creates the object `bottle_int` and a bunch of functions like `bottle_create_int` and `bottle_send_int` (to name a few) behind the scene.

When `bottle_create(int)` is called, it is translated into a call to `bottle_create_int` at compile time using the macro:

```c
#define bottle_create( TYPE, capacity ) \
  bottle_create_*TYPE*(capacity)
```

At creation of a bottle, the instance of the bottle is tied to its set of functions depending of its type `TYPE` with something like:

```c
  const struct
  {
    int (*Fill) (bottle_*TYPE* *self, TYPE message);
    int  (*TryFill) (bottle_*TYPE* *self, TYPE message);
    int (*Drain) (bottle_*TYPE* *self, TYPE *message);
    int (*TryDrain) (bottle_*TYPE* *self, TYPE *message);
    void (*Close) (bottle_*TYPE* *self);
    void (*Destroy) (bottle_*TYPE* *self);
  } *vtable = { bottle_send_*TYPE*, bottle_try_send_*TYPE*, bottle_recv_*TYPE*, bottle_try_recv_*TYPE*, bottle_close_*TYPE*, bottle_destroy_*TYPE* };
```

A call to `bottle_send(b, 5)` will therefore be translated into a call to `Fill(b, 5)` at compile time with a macro like:

```c
#define bottle_send(self, message)  \
  ((self)->vtable->Fill ((self), (message)))
```

which in turn will be translated into a call to `bottle_send_int(b, 5)` at runtime.

The names of the macros and functions are slightly different in the code (where macro-like style is used) but this is the concept.

This logic uses ideas from [the blog of Randy Gaul](http://www.randygaul.net/2012/08/10/generic-programming-in-c/).
