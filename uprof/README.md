## History

- v1: Ring Buffer like ftrace

- v2: rhashtable for resizable entry

- v3: rhashtable + sequence file API

Interface development became much more convenient due to seq_file API.
Instead of managing print buffer manually, seq_file API internally handles resizable buffer.
SEQ API is called as below sequence.

```c
seq_open() → seq_read() → [start → show → next → show → ... → stop] → seq_release()
```

## Improvements

- target for malloc/fentry at glibc. (currently only static binary available)

- ELF parsing inside kernel
