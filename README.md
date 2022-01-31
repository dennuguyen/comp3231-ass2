# Group 253 - Assignment 2

This README is the assignment spec summarised.

[Click here to see assignment 2 specification](https://cgi.cse.unsw.edu.au/~cs3231/21T1/assignments/asst2/)

## Deadlines

10% Bonus = 27 March 1700

Final = 1 April 1700

## What Needs to be Done

The following file system (FS) system calls need to be implemented and documented:
- open()
- read()
- write()
- lseek()
- close()
- dup2()

Consider:
- what primitive operations exist?
- what user program errors exist?
- how to handle user program errors?
- which functions need to change?
- which structures need to be implemented?
- how to keep track of open files?

What should we not do:
- allow the program to crash

We can assume:
- a single process runs at a time
- to use the existing VFS layer to implement our system calls
- design and implementaiton must *not* assume a single process will ever exist at a time
- file descriptors (fd) 0, 1, 2 are for stdin, stdout, stderr respectively
- fd1 and fd2 must start attached to "con:" - modify runprogram()

These are the files we'll mainly be working in:
- kern/include/syscall.h
- kern/include/file.h
- kern/syscall/file.c

## Documentation

Small document to be submitted must contain:
- identify basic issues in assignment
- describe solutions to issues
- plain ASCII
- word-wrapped
- ~500-1000 words
- ~/cs3231/asst2-src/asst2-design.txt

A marker should know the following from our document:
- What data structures have you added and what function do they perform?
- What are any significant issues surround managing the data structures and state do they contain?
- What data structures are per-process and what structures are shared between processes?
- Are there any issues related to transferring data to and from applications?
- If fork() was implemented, what concurrency issues would be introduced to your implementation?

## Resources

- Section 10.6.3 and "NFS implementation" in Section 10.6.4, Tannenbaum, Modern Operating Systems.
- Section 6.4 and Section 6.5, McKusick et al., The Design and Implementation of the 4.4 BSD Operating System.
- Chapter 8, Vahalia, Unix Internals: the new frontiers.
- The original [VFS paper is available here](https://cgi.cse.unsw.edu.au/~cs3231/21T1/assignments/asst2/kleiman86vnodes.pdf)
