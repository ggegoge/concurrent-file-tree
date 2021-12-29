
# Concurrent dir tree

A C program representing a directory tree structure which may be used
concurrently.

_'tis a task for a university course_

### Task
Write a program that simulates a directory tree containing structure that may be used
concurrently by multiple threads. The implementation should provide the `Tree.h`
interface.

Required functions from `Tree.h`:
  * `Tree* tree_new()` returns a new heap-alloc'd tree
  * `int tree_create(const char* path)` like `mkdir path`
  * `int tree_remove(const char* path)` like `rmdir path`
  * `int tree_move(const char* source, const char* target)` like `mv source target`
  * `void tree_free(Tree*)` destroys a heap-alloc'd tree and frees all of the memory used
    by it
  
Note: functions return various error codes as per the tasks specification.
  
The implementation needs to support **concurrent execution** with multiple
threads.  It needs to be not only _concurrent_ but _paralell_ as well ie. if some
actions on the tree may be performed at the same time they better be.

### Implementation

The `HashMap` was given and not created by me, same with majority of
`path_utils` and the simple `err` utilities for terminating processes with
style. They were provided by the task authors.

`Tree` and `rw` modules are mine.

  * `Tree` -- implementation of the `Tree.h` interface
  * `rw` -- my implementation of a _readers & writers_ style locking mechanism
