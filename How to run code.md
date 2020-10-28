*Cache simulator

to run the code:
1. navigate to the "src" file in the terminal
2. Write: "python3 traceconverter.py" in terminal.
3. Write: "make" to compile the program
4. Write: "./cachesim trace.tr" to run the program

OBS!
The trace.tr file has been made from a given program, using valgrind to create this file.
The trace.tr file need to be given as an input for the simulator to work


-To Change the Parameters for the Caches:

1. Go to memory.c and change global variables for L1-instruction-cache, L1-Data-cache, L2-cache.
2. Each cache has the changeable parameters; Size (Kilobits), Blocksize (Bytes), Associativity-level (Integer), Policy (Write policy, Integer)
3. To use Write-Back policy, set value to 1.
4. To use Write-Through policy, set value to 0.
5. To run the test-memory trace: *Rename the current logfile. *Rename the "cachetest"-logfile to "logfile".
