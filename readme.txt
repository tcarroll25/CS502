Tyler Carroll
CS502
Project Part 2
Readme File

This directory contains all the source code for the operating system, as well
as the test outputs. This operating system runs in a linux enviroment.

Making:

To make the project simply type "make". To perform a clean type "make clean".

Running:

To run the operating system run the following command:

./os <test_name>

Valid Test Names:
test1a
test1b
test1c
test1d
test1e
test1f
test1g
test1h
test1i
test1j
test1k
test2a
test2b
test2c
test2d
test2e
test2f
test2g

Test Results:
Test results are present in the outputs folder.
All tests run with a 100% success rate.
Test1e correctly hangs at the end of the test. The last system call in test1e is a SUSPEND_PROCESS(-1). This
operating system allows the current process to suspend itself so the end of
the test will hang because there are no other processes to run.
