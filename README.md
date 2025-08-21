# BuzzDB Database System Labs

This repository contains four laboratory assignments for building a database management system called BuzzDB. Each lab focuses on implementing core database system components.

## Lab 1: External Sort

Implementation of an external sorting algorithm that can sort large datasets that don't fit in main memory. The lab involves:

- Dividing input into smaller runs that fit within memory constraints
- Sorting individual runs in memory using std::sort
- Implementing a K-way merge algorithm to combine sorted runs
- Using disk I/O efficiently with temporary files
- Memory management within specified constraints

**Key Skills**: External algorithms, memory management, disk I/O optimization

## Lab 2: Logging and Recovery

Implementation of Write-Ahead Logging (WAL) and recovery mechanisms for database transactions. The lab covers:

- Implementing logging for all page-level write operations and transaction commands
- Building a recovery system using No-Force/Steal policy
- Three-phase recovery: Analysis, Redo, and Undo phases
- Transaction rollback functionality
- Ensuring atomicity and durability of database operations

**Key Skills**: Transaction processing, crash recovery, logging protocols

## Lab 3: Concurrency Control

Implementation of two-phase locking for transaction isolation and atomicity. The lab includes:

- Building a lock manager to track locks held by each transaction
- Implementing shared and exclusive locks with proper blocking behavior
- Deadlock detection and resolution mechanisms
- Transaction completion and abort handling
- Thread-safe operations and race condition prevention

**Key Skills**: Concurrency control, deadlock handling, synchronization primitives

## Lab 4: Query Optimizer

Implementation of a cost-based query optimizer using selectivity estimation and join ordering. The lab involves:

- Building histogram-based statistics for selectivity estimation
- Implementing cost estimation for table scans and joins
- Creating a Selinger-style join optimizer for optimal query plans
- Estimating join cardinalities and filter selectivities
- Generating left-deep join plans with minimal cost

**Key Skills**: Query optimization, cost estimation, statistics, join algorithms

## Testing

All labs include comprehensive unit tests and can be validated using Valgrind for memory leak detection. Compiler warnings are treated as errors to ensure code quality.
