xmem2
=====

Arduino Mega 1280/2560 and PJRC Teensy2.0++ xmem compatible library with auto-size features and real preemptive multitasking.

<pre>
* IMPORTANT! PLEASE USE Arduino 1.0.5 or better!
* Older versions HAVE MAJOR BUGS AND WILL NOT WORK AT ALL!
* Use of gcc-avr and lib-c that is newer than the Arduino version is even better.


Supports both RuggedCircuits RAM expansions automatically.
Supports Andy Brown's RAM expansion too.
Teensy2.0++ details coming soon.

Can support up to 255 banks of external memory, with minor sketch changes.
Supports all the regular xmem features.
Full replacement for the stock xmem, old sketches will still work.

Stuff you can't do with the stock xmem:
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

A real-time operating system is an operating system intended to serve real-time
application requests within a certain deadline.

Since xmem2 is not deterministic, doesn't have any notion of a deadline, and it
does not serve any application requests, it isn't an RTOS. Even granting a lock
is non-deterministic as to which task gets the lock next. If you have throughput
critical code, you can hog nearly 100% of the CPU if you need to, for as long
as you need it.

xmem2 is only a bare metal preemptive multitasking scheduler.
This is done in the minimum amount of code possible, with the least amount of
side effects.

Most important differences from an RTOS is there is no wakeup for an event,
and no guaranteed predictable deadline.


A full description of differences is <A HREF="http://www.chibios.org/dokuwiki/doku.php?id=chibios:articles:rtos_concepts">HERE.</A>

<b>
A non real time system is a system where there are no deadlines involved.
Non-RT systems could be described as follows:

”A non real time system is a system where the programmed reaction to a stimulus
will certainly happen sometime in the future”.
</b>


</pre>
