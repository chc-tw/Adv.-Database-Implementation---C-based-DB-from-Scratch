# Assignment 2: Logging And Recovery

## Project Description

The goal of the lab is to implement `Logging` and `Recovery` mechanism in buzzdb. You need to implement a write ahead logging (WAL) under No-Force/Steal policy. You need to log every page-level write operation and transaction command.

## Program Specification

We have provided you with `LogManager` stub that contains APIs that you need to implement. You should not modify the signature of the pre-defined functions. You should also not modify any predefined member variables. You are however allowed to add new helper private functions/member variables in order to correctly realize the functionality.

Note

You **only** need to modify the code in `src/log/log_manager.cc` and `src/include/log/log_manager.h`.

The lab can be divided into two tasks:

### Task #1 - Logging

To achieve the goal of atomicity and durability, the database system must output to stable storage information describing the modifications made by any transaction, this information can help us ensure that all modifications performed by committed transactions are reflected in the database (perhaps during the course of recovery actions after a crash). It can also help us ensure that no modifications made by an aborted or crashed transaction persist in the database. The most widely used structure for recording database modifications is the log. The log is a sequence of log records, recording all the update activities in the database. The buzzdb database system has a global `LogManager` object which is responsible logging information. You need not worry about making the calls to the LogManager. The other components of the system are responsible for correctly calling the `LogManager`. The HeapSegment will explicitly call the log\_update before making any update operations. The `TransactionManager` will invoke `log_txn_begin` when a transaction begins, `log_commit` before commiting a transaction and `log_abort` in case a transaction has to be aborted.

#### API Requirements and Hints

// Returns the count of total number of log records in the log file
void LogManager::get\_total\_log\_records()

/\*\*
\* Increment the ABORT\_RECORD count.
\* Rollback the provided transaction.
\* Add abort log record to the log file.
\* Remove from the active transactions.
\*/
void LogManager::log\_abort(uint64\_t txn\_id, BufferManager& buffer\_manager)

/\*\*
\* Increment the COMMIT\_RECORD count
\* Add commit log record to the log file
\* Remove from the active transactions
\*/
void LogManager::log\_commit(uint64\_t txn\_id)

/\*\*
\* Increment the UPDATE\_RECORD count
\* Add the update log record to the log file
\*/
void LogManager::log\_update(uint64\_t txn\_id, uint64\_t page\_id, uint64\_t length, uint64\_t offset, std::byte\* before\_img, std::byte\* after\_img)


/\*\*
\* Increment the BEGIN\_RECORD count
\* Add the begin log record to the log file
\* Add to the active transactions
\*/
void LogManager::log\_txn\_begin( uint64\_t txn\_id)

/\*\*
\* Increment the CHECKPOINT\_RECORD count
\* Flush all dirty pages to the disk (USE: buffer\_manager.flush\_all\_pages())
\* Add the checkpoint log record to the log file
\*/
void LogManager::log\_checkpoint( BufferManager& buffer\_manager)

### Task #2 - Recovery

The next part of the lab is to implement the ability for the DBMS to recover its state from the log file.

#### API Requirements and Hints

/\*\*
\* Recover the state of the DBMS after crash. It is broken down into three phases:
\*
\* @Analysis Phase:
\*            1. Get the active transactions and commited transactions
\*            2. Restore the txn\_id\_to\_first\_log\_record
\* @Redo Phase:
\*            1. Redo the entire log tape to restore the buffer page
\*            2. For UPDATE logs: write the after\_img to the buffer page
\*            3. For ABORT logs: rollback the transactions
\*    @Undo Phase
\*            1. Rollback the transactions which are active and not commited
\*/
void LogManager::recovery(BufferManager& buffer\_manager)


/\*\*
 \* Use txn\_id\_to\_first\_log\_record to get the begin of the current transaction
\* Walk through the log tape and rollback the changes by writing the before
\* image of the tuple on the buffer page.
\* Note: There might be other transactions' log records interleaved, so be careful to
\* only undo the changes corresponding to current transactions.
\*/
void LogManager::rollback\_txn(uint64\_t txn\_id,  BufferManager& buffer\_manager);

For this lab, you are free to choose any data structure for the log record. Here, we provide an example log record structure that you might find helpful.

{
   "BEGIN\_RECORD or COMMIT\_RECORD or ABORT\_RECORD":\[
      "LOGTYPE",
      "TXN\_ID"
   \],
   "UPDATE\_RECORD":\[
      "LOGTYPE",
      "TXN\_ID",
      "PAGEID",
      "LENGTH",
      "OFFSET",
      "BEFORE\_IMG",
      "AFTER\_IMG"
   \],
   "CHECKPOINT\_RECORD":\[
      "LOGTYPE",
      "ACTIVE\_TXNS",
      "TXN\_1",
      "TXN\_1\_FIRST\_LOG\_OFFSET",
      "...",
      "TXN\_K",
      "TXN\_K\_FIRST\_LOG\_OFFSET"
   \]
}

In order to simplify the complexity of the lab, we provide you with an implementation of `BufferManager`. We have also provided you with a `File` class which has read/write interfaces that will be helpful.

/\*\*
\* Functionality of the buffer manager that might be handy

Flush all the dirty pages to the disk
   buffer\_manager.flush\_all\_pages():

Write @data of @length at an @offset the buffer page @page\_id
   BufferFrame& frame = buffer\_manager.fix\_page(page\_id, true);
   memcpy(&frame.get\_data()\[offset\], data, length);
   buffer\_manager.unfix\_page(frame, true);

\* Read and Write from/to the log\_file
   log\_file\_-\>read\_block(offset, size, data);

   Usage:
   uint64\_t txn\_id;
   log\_file\_-\>read\_block(offset, sizeof(uint64\_t), reinterpret\_cast<char \*\>(&txn\_id));
   log\_file\_-\>write\_block(reinterpret\_cast<char \*\> (&txn\_id), offset, sizeof(uint64\_t));
\*/

To successfully pass all the test cases, you need to add required implementation in the `log_manager.cc` and `log_manager.h`. We have also added detailed comments in the code skeleton.

Note

**What are the files like slotted\_page.cc, heap\_file.cc, etc?**

These files are related to the storage manager. Heap file organization is one way of managing pages in the files on disk in a database. A heap file can be treated as an unordered collection of pages with tuples stored in random order. And slotted pages refer to the layout schema of these pages (how the tuples are stored in a page). You don’t need to modify or change anything in these files for this assignment. Assume them as black-boxes that take care of the tuple inserts.

### Prerequisites

You need to follow the instructions mentioned in the [setup](https://buzzdb-docs.readthedocs.io/part2/setup.html) document.

Download the handout shared via Canvas.

### Instructions for execution

\[vm\] $ cd buzzdb-logging-recovery
\[vm\] $ mkdir build
\[vm\] $ cd build
\[vm\] $ cmake -DCMAKE\_BUILD\_TYPE=Release ..
\[vm\] $ make
\[vm\] $ ctest

We treat compiler warnings as errors. Your project will fail to build if there are any compiler warnings.

## General Instructions

Testing for correctness involves more than just seeing if a few test cases produce the correct output. There are certain types of errors (memory errors and memory leaks) that usually surface after the system has been running for a longer period of time. You should use valgrind to isolate such errors.

You will get a listing of memory errors in your program. If you have programmed in Java you should keep in mind that C++ does not have automatic garbage collection, so each new must ultimately be matched by a corresponding delete. Otherwise all the memory in the system might be used up. Valgrind can be used to detect such memory leaks as well. More information about valgrind can be found at: [http://www.valgrind.org/docs/manual/index.html](http://www.valgrind.org/docs/manual/index.html).

## Submitting Your Assignment

bash submit.sh <name\>

You will be submitting your assignment on Gradescope. You are expected to run `submit.sh` and submit the generated zip to the autograder.

## Grading

The maximum score on this assignment is 100. If you get 100 on the autograder that is your score.

## Frequently Asked Questions

-   In the analysis phase, are we meant to get the active transaction information from the latest checkpoint, then read forward from that point in the log to find any other started or committed transactions?
    
    > -   Yes, that is correct.
    >     
    
-   Why do we need to redo the entire log in the redo phase? Wouldn’t it be sufficient to redo the log from the latest checkpoint (assuming there is one, otherwise we would certainly have to replay the entire log)?
    
    > -   Yes, you redo from the latest checkpoint.
    >     
    
-   In the undo phase, should we write an abort record to the log before rolling back a transaction, or is it sufficient to roll back the transaction and remove it from the active transactions?
    
    > -   You need to roll back any changes done by the transaction and remove them from the active transactions. No need to log an abort record.
    >     
    
-   What is the offset of the log file, and how to use it?
    
    > -   The offset helps to maintain where to write in the log file. You can think of it seeking the offset number of bytes in the file and then reading or writing.
    >     
    
-   I wanted to confirm that apart from maintaining log records, we also need to maintain the ATT and DPT data structures for the recovery phase right?
    
    > -   Yes, you have to maintain the ATT data structure. You might not need DPT but it’s up to your design.
    >     
    
-   I am a bit stuck on the rollback\_txn. I walk through the log tape and find all update records in a given transaction, but how do I rollback the changes? It feels like I would have to call heap manager and write the change to the page. Or do I just create a new Update record with the inverse of the update I am reversing?
    
    > -   You can use BufferManager::fix\_page() and then write the change into the page
    >