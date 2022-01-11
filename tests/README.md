# Tests

Below is a description of what each test does.

## Teacher Provided Tests

- `copy_to_external_errors`: Check if the copy to external FS functions throws
  errors when the external file path does not exist or the source file (inside TFS)
  does not exist.
- `copy_to_external_simple`: Create file with small content and try to copy it to an external file.
- `test1`: Write and read to/from file with small string.
- `write_10_blocks_simple`: Fill a file up to the 10 blocks, but only writing one block at a time.
- `write_10_blocks_spill`: Fill a file up to the 10 blocks, but writes may write to more than one block at a time.
- `write_more_than_10_blocks_simple`: Fill a file over 10 blocks, but only writing one block at a time.

## Student Made Tests

- `thread_copy_to_external`: Copy various files multiple times concurrently to the external FS,
  and compare their contents with the original.
- `thread_create_files`: Create as many file as possible, in order to test concurrency of `inode_create`.
- `thread_read_same_file`: Fill a file with large content and read from it on multiple threads at the same time.
- `thread_same_fs`: Test writing and reading to/from the same file descriptor on multiple threads concurrently.
- `thread_trunc_append`: Write to new files concurrently, and then append and/or truncate them
  concurrently as well, verifying the end result.
- `thread_write_new_files`: Create various files in different thread with different content,
  ensuring there is spill while writing, and then compares with the original content on the main thread.
- `write_big_file`
- `write_more_than_10_blocks_simple`: Fill a file over 10 blocks, but writes may write to more than one block at a time.
