xmem2
=====

Arduino Mega 128x/256x xmem compatable library with autosize features and real preemptive multitasking.

<pre>
Supports both RuggedCircuits RAM expansions automatically.
Supports Andy Brown's RAM expansion too.

Can support up to 255 banks of external memory, with minor sketch changes.

Stuff you can't do with the stock xmem:
     Supports moving the stack to external memory, You can even specify how much.
     Supports up to 16 preemptive tasks on a stock Quadram.
     Each task contains it's own stack and malloc arena.
     CPU/Context switching friendly Sleep()
     Lock_Acquire(), Lock_Release(), Yield()
     Ability to determine parent task.
     Ability to detect when a task is running or done.
     Ability to determine if parent task owns child task.
     NEW: inter-task message pipes as a stream or block.
</pre>
