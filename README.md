xmem2
=====

Arduino Mega 1280/2560 and PJRC Teensy++2.0 xmem compatible library with auto-size features and real preemptive multitasking. Also provides ISR safe malloc for all AVR arduinos, and all Teensy products from PJRC.com.

<pre>
Teensy 3.x support:
ISR safe libc memory management. 
No multitasking features at this time.

<HR>

AVR Support:

* IMPORTANT! PLEASE USE Arduino 1.0.5 or better!
      Older versions HAVE MAJOR BUGS AND WILL NOT WORK AT ALL!
      Use of gcc-avr and lib-c that is newer than the Arduino version is even better.

* Supports both RuggedCircuits RAM expansions automatically.

* Supports Andy Brown's RAM expansion too.

* Teensy++2.0 now supported. See doc/Teensypp2_RAM.pdf for details.

* Can support up to 255 banks of external memory, with minor sketch changes.

* Supports all the regular xmem features.

* Full replacement for the stock xmem, old sketches will still work.

* Stuff you can't do with the stock xmem:
        Supports moving the stack to external memory, You can even specify how much.
        ISR and context safe memory management (malloc and friends).
        Multitasking:
                Supports up to 16 preemptive tasks on a stock Quadram.
                Each task contains it's own stack and malloc arena.
                CPU/Context switching friendly Sleep()
                Lock_Acquire(), Lock_Release(), Yield()
                Ability to determine parent task.
                Ability to detect when a task is running or done.
                Ability to determine if parent task owns child task.
                inter-task message pipes as a character stream or block of data.

PLEASE READ CAREFULLY:

xmem2's multitasking is not, and should not be considered a 'RTOS'.

I'll repeat this again, just to pound it into your skull...
xmem2's multitasking is not, and should not be considered a 'RTOS'.


A real-time operating system is an operating system intended to serve real-time
application requests within a certain deadline.

Since xmem2 is not deterministic, doesn't have any notion of a deadline, and it
does not serve any application requests, it isn't an RTOS. Even granting a lock
is non-deterministic as to which task gets the lock next. If you have throughput
critical code, you can hog nearly 100% of the CPU if you need to, for as long
as you need it. Just use the SoftCLI and SoftSEI methods which do not block ISR,
but do protect the current task from preemption. Sure you can also do this with
a lock, but the methods are actually a little bit faster, and the task switching
code is bypassed entirely.

xmem2 is only a bare metal preemptive multitasking scheduler.
This is done in the minimum amount of code possible, with the least amount of
side effects possible.

Most important differences from an RTOS is there is no wakeup for an event,
and no guaranteed predictable deadline. For a good example, consider locks.
Locks have no timeout, thus you could indeed deadlock and wait forever.
Locks are granted when all processes ahead of yours has released it. Another
process could even yield its CPU time to the one you are interested in, but some
other process still holds the lock ahead of you. This results in the very
unpredictable nature of events, and actually ends up utilizing CPU time better.
I like the idea of using the CPU to do something useful instead of goofing with
worries about who gets woken up next, and all the bloat and latency baggage
that comes with it.


A full description of differences is <A HREF="http://www.chibios.org/dokuwiki/doku.php?id=chibios:articles:rtos_concepts">HERE.</A>

<b>
A non real time system is a system where there are no deadlines involved.
Non-RT systems could be described as follows:

”A non real time system is a system where the programmed reaction to a stimulus
will certainly happen sometime in the future”.
</b>

THEREFORE:
xmem2's multitasking is not, and should not be considered a 'RTOS'.

</pre>
