-To Run the Code:
* Locate the src/ folder.
* Type "python3 traceconverter.py" in terminal.
* Type "make" in terminal to compile the code.
* Type "./cachesim trace.tr" in terminal to run the code with the given logfile.

-To Change the Parameters for the Caches:
* Go to memory.c and change global variables for L1-instruction-cache, L1-Data-cache, L2-cache.
* Each cache has the changeable parameters; Size (Kilobits), Blocksize (Bytes), Associativity-level (Integer), Policy (Write policy, Integer)
* To use Write-Back policy, set value to 1.
* To use Write-Through policy, set value to 0.

-To run the test-memory trace:
*Rename the current logfile.
*Rename the "cachetest"-logfile to "logfile".
* Run the code
