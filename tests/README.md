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

- `thread_create_files`: Create as many file as possible, in order to test concurrency of `inode_create`.
- `thread_read_10`
- `thread_trunc_append`
- `thread_write_new_files`
- `thread_write_read`
- `thread_big_file`
- `write_more_than_10_blocks_simple`: Fill a file over 10 blocks, but writes may write to more than one block at a time.
