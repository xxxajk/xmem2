xmem2
=====

Arduino Mega 128x/256x xmem compatable library with autosize features and real preemptive multitasking.

<pre>
Supports both RuggedCircuits RAM expansions automatically.
Supports Andy Brown's RAM expansion too.

Can support up to 255 banks of external memory, with minor sketch changes.

NEW: Supports moving the stack to external memory.
     You can even specify how much.

NEW: Supports up to 16 preemptive tasks on Quadram!
     Each task contains it's own stack and malloc arena!
     Tasks can communicate to each other via the on-chip SRAM.
     *NOTE: ONLY TESTED on Mega1280!
            Mega2560 *MIGHT* work. If it doesn't then let me know!
            I don't own a Mega2560 yet.

NEW: CPU cycle friendly Sleep()
     Lock_Acquire(), Lock_Release(), Yield()
     Ability to determine parent task.
     Ability to detect when a task is running or done.
     Ability to determine if parent task owns child task.
</pre>
