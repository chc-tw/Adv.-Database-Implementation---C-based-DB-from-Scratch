# Project Description
In this lab, you will implement a simple two-phase locking to ensure isolation and atomicity of the transactions. You will need to implement a locking manager to track the locks held by each transaction and grant locks to transactions as they are needed. The remainder of this document describes what is involved in adding concurrent ransaction support and provides a basic outline of how you might add this support to your database.

Please start as early as possible. Locking and transactions can be quite tricky to debug!

# Program Specification
We have provided you with BufferManager stub that contains APIs that you need to implement. You should not modify the signature of the pre-defined functions. You should also not modify any predefined member variables. You are however allowed to add new helper private functions/member variables in order to correctly realize the functionality. You have to work on the buffer_manager.cc and buffer_manager.h files.

## Granting Locks
You will need to create data structures that keep track of which locks each transaction holds and then check to see if a lock should be granted to a transaction when it is requested.

You will need to implement shared and exclusive locks; recall that these work as follows:

- Before a transaction can read an object, it must have a shared lock on it.
- Before a transaction can write an object, it must have an exclusive lock on it.
- Multiple transactions can have a shared lock on an object.
- Only one transaction may have an exclusive lock on an object.
- If transaction t is the only transaction holding a shared lock on an object o, t may upgrade its lock on o to an exclusive lock.
- If a transaction requests a lock that it should not be granted, your code should block, waiting for that lock to become available (i.e., be released by another transaction running in a different thread).

You need to be especially careful to avoid race conditions when writing the code that acquires locks – think about how you will ensure that correct behavior results if two threads request the same lock at the same time

## API Requirements and Hints

```cpp
/**
This API is called when the transaction txn_id wants to acquire lock on page (page_id).
The exclusive boolean defines whether the transaction needs a shared lock (when false) or
an exclusive lock (when true).
*/
BufferFrame &fix_page(uint64_t txn_id, uint64_t page_id, bool exclusive);

/**
This API is called by the tester to unfix a page acquired by a transaction.
It releases all the locks acquired by the txn_id on this page and if dirty
flushes that to disk.
*/
void unfix_page(uint64_t txn_id, BufferFrame& page, bool is_dirty);
```
NOTE: It is not mandatory for your Lock Manager implementation to follow the same outline as given below. This is given just for your reference and to guide your design process.

Hints for implementing the logic for managing the locks

- Create a separate class for managing the locks. Let it have the functions for
  - Acquiring locks
  - Releasing locks
  - Checking for deadlocks (need to check if acquiring a lock might cause a deadlock. If yes, abort the transaction.)
  - Ending a transaction (need to do the cleanup works)

- Create a separate class for managing the locks on a Frame. It should keep track of transactions holding a lock on the frame and transactions waiting for a lock on the frame.

- Ensure atomicity on the operations on the Lock Manager

- Some resources that might be useful: unique_lock and conditional_variable .

After this you should be able to pass all the BufferManagerTest test cases.

## Transactions
In BuzzDB, each query has a txn_id. When a query is completed, the corresponding transaction calls the transaction_complete() function. Calling this function ensures that all the dirty pages held by this transaction are flushed to the disk, and all the corresponding locks are released. Similarly, transaction_abort function is called if we need to abort the transaction. It discards all the changes made by the transaction and also releases all the locks.

### API Requirements and Hints
```cpp
/**
   Flush dirty pages to disk
   Release all the locks
*/
     void transaction_complete(uint64_t txn_id);

/**
   Discard dirty pages
   Release all the locks
*/
     void transaction_abort(uint64_t txn_id);
```
After this you should be able to pass all the BufferManagerTransactionTest test cases.

# Deadlocks and Aborts
It is possible for transactions in BuzzDB to deadlock. You will need to detect this situation and throw a transaction_abort_error. There are many possible ways to detect deadlock. For example, you may implement a simple timeout policy that aborts a transaction if it has not completed after a given period of time. Alternately, you may implement cycle-detection in a dependency graph data structure. In this scheme, you would check for cycles in a dependency graph whenever you attempt to grant a new lock, and abort something if a cycle exists.

You should implement deadlock detection and resolution in BufferManager. Most likely, you will want to check for a deadlock whenever a transaction attempts to acquire a lock and finds another transaction is holding the lock. You have many design decisions for your deadlock resolution system, but it is not necessary to do something complicated. You should ensure that your code aborts transactions properly when a deadlock occurs, by throwing a transaction_abort_error. This error will be caught by the code executing the transaction (e.g., DeadlockTest), which should call transaction_abort() to cleanup after the transaction. You are not expected to automatically restart a transaction which fails due to a deadlock – you can assume that higher level code will take care of this.

## Prerequisites
You need to follow the instructions mentioned in the setup document.

Download the handout shared via Canvas.

## Instructions for execution

```bash
[vm] $ cd buzzdb-concurrency-control
[vm] $ mkdir build
[vm] $ cd build
[vm] $ cmake -DCMAKE_BUILD_TYPE=Release ..
[vm] $ make
[vm] $ ctest
```

We treat compiler warnings as errors. Your project will fail to build if there are any compiler warnings.

#General Instructions
Testing for correctness involves more than just seeing if a few test cases produce the correct output. There are certain types of errors (memory errors and memory leaks) that usually surface after the system has been running for a longer period of time. You should use valgrind to isolate such errors. Command to run valgrind can be found here.

You will get a listing of memory errors in your program. If you have programmed in Java you should keep in mind that C++ does not have automatic garbage collection, so each new must ultimately be matched by a corresponding delete. Otherwise all the memory in the system might be used up. Valgrind can be used to detect such memory leaks as well. More information about valgrind can be found at: http://www.valgrind.org/docs/manual/index.html.

# Submitting Your Assignment
```bash
submit.sh <name>
```
You will be submitting your assignment on Gradescope. You are expected to run submit.sh and submit the generated zip to the autograder.

You can use REPORT.md to describe the following design and program criteria (optional). In case you don’t complete all the testcases, we will award you partial points based on the report.

# Grading
The maximum score on this assignment is 100. If you get 100 on the autograder that is your score. If you get score less than 100 we will award partial points based on the report.