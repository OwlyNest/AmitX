# SmKFS Ground Zero
## Shadow-mode Kernel File System
### Architecture Specification v0.1

## 1. Design Philosophy

SmKFS is the native filesystem of the Shadow kernel and the default filesystem for Phonon.

It is designed around five principles:

- simplicity of implementation
- long-term extensibility
- predictable performance
- strong metadata integrity
- efficient storage utilization

Rather than inheriting historical constraints from existing filesystems, SmKFS adopts proven concepts from ext4 and NTFS while exposing a clean, modern on-disk format.

The filesystem is record-oriented, extent-based, and attribute-driven.

---

# 2. Core Goals

The filesystem shall:

- support files of arbitrary size
- support directories containing millions of entries
- minimize fragmentation
- minimize metadata overhead
- recover cleanly after unexpected shutdown
- remain extensible without incompatible format revisions

Everything else is secondary.

---

# 3. Filesystem Philosophy

Every filesystem object is a **record**.

Records describe objects.

Objects do not directly describe storage.

Storage is allocated independently by the allocator.

This creates a clear separation between:

```
Filesystem Object

↓

Metadata

↓

Logical Data Layout

↓

Allocator

↓

Physical Storage
```

Each layer has a single responsibility.

---

# 4. Records

Every object begins life as a record.

Objects include:

- files
- directories
- symbolic links
- device nodes
- future object types

A record contains a collection of typed attributes.

A record never has fixed metadata fields beyond its mandatory header.

---

# 5. Attributes

Attributes are the fundamental building blocks of SmKFS.

Examples include:

- Name
- Data
- Security
- Permissions
- Timestamps
- Owner
- Extended metadata
- Integrity information

The filesystem never assumes a fixed collection beyond those required for basic operation.

New functionality is introduced by defining additional attribute types rather than modifying record layouts.

---

# 6. Variable Sized Records

Records are variable-sized.

Small objects consume little metadata.

Large metadata structures may expand naturally without forcing overflow records until necessary.

Benefits:

- improved space efficiency
- fewer metadata extensions
- better cache utilization
- easier future expansion

---

# 7. Resident Data

Very small files may store their contents directly inside the record.

Example:

```
notes.txt

Header
Attributes
File Data (37 bytes)
```

No separate storage allocation is required.

When data exceeds available record space, it automatically becomes extent-backed.

---

# 8. Extents

SmKFS is extent-native.

All non-resident data is described using extents.

An extent describes:

```
Logical Offset

↓

Physical Block

↓

Length
```

No indirect blocks exist.

No legacy pointer trees exist.

Every large file is simply a list of extents.

---

# 9. Logical vs Physical Layout

File metadata describes logical ordering only.

The allocator determines physical placement.

```
Logical File

Extent A
Extent B
Extent C

↓

Allocator

↓

Disk Locations
```

This separation allows future allocators to optimize independently for HDDs, SSDs, NVMe, or other storage media without changing the file format.

---

# 10. Allocation

Storage allocation is handled by a dedicated allocator.

Primary goals:

- contiguous allocation
- delayed allocation
- fragmentation avoidance
- locality preservation

Allocation decisions should occur as late as practical.

---

# 11. Free Space Management

Free storage is tracked using allocation bitmaps.

Each allocation unit has one corresponding bit.

```
0 = Free

1 = Allocated
```

Bitmap scanning provides efficient allocation and simplifies integrity checking.

---

# 12. Directories

Directories are indexed structures.

Linear directory scans are never the primary lookup mechanism.

Directory contents are stored using B+ Trees.

Keys:

```
Filename
```

Values:

```
Record Identifier
```

This provides near logarithmic lookup regardless of directory size.

---

# 13. Journaling

SmKFS journals metadata.

The journal guarantees filesystem consistency following crashes or unexpected power loss.

User file data is not required to be journaled.

Metadata integrity has priority over complete write ordering of user data.

---

# 14. Locality

Objects that are likely to be accessed together should be stored near one another whenever practical.

Examples:

- directory and children
- metadata and extents
- neighboring files

This concept is inspired by ext4 block groups but is not tied to a specific physical layout.

---

# 15. Extensibility

SmKFS shall never require structural redesign for new features.

Future additions should be implemented through:

- new attribute types
- new record types
- optional metadata structures

Backward compatibility is maintained by allowing unknown attributes to be safely ignored when appropriate.

---

# 16. Scalability

SmKFS is designed to scale from small removable media to multi-terabyte storage.

The architecture assumes:

- millions of files
- extremely large directories
- large contiguous allocations
- future storage technologies

No architectural limits should depend on assumptions common in legacy filesystems.

---

# 17. Golden Design Rules

1. Everything is a record.
2. Records consist of typed attributes.
3. Extents are the sole mechanism for non-resident data.
4. Metadata is journaled.
5. Small files remain resident whenever possible.
6. Directories are indexed using B+ Trees.
7. Allocation favors contiguous extents through delayed allocation.
8. New functionality is added through attributes, not structural revisions.
9. The on-disk format must remain understandable and debuggable.
10. Reliability takes precedence over clever optimization.
11. Everything on disk should be self-describing.

18. Self-Describing Structures

In accordance with Design Rule 11, every major on-disk structure shall begin with a common header that identifies the structure and provides sufficient information for validation and parsing. Every structure in the file system starts with exactly the same cannonical header:
struct SmKFS_Header{
    char     Magic[4];
    uint16_t Version;
    uint16_t Type;
    uint32_t Length;
    uint32_t Flags;
    uint32_t Checksum;
}

This leaves no special cases, no guessing, after reading the first ~20 bytes the parser know all information about the the object that it needs.

Additional fields may be included where required by the structure.

This requirement applies to all primary filesystem structures, including but not limited to:
- Records
- B+ Tree nodes
- Journal entries
- Allocation bitmaps
- Allocator metadata
- Superblocks
- Future structure types

The purpose of this design is to allow the filesystem driver, recovery utilities, and diagnostic tools to identify and validate any structure directly from its on-disk representation without relying on external context.

Self-describing structures improve:
- corruption detection
- filesystem recovery
- offline consistency checking
- forward compatibility
- version migration
- debugging and forensic analysis

# Checksum
The checksum covers the entire structure; header + payload. The checksum field itself is treated as zero during computation.
Process for writing:
1. Fill in all fields of the structure
2. Set checksum = 0
3. Compute checksum over length bytes starting from the header
4. Store result in checksum
5. Write to disk

Process for reading/verifying:
1. Read structure from disk
2. Save checksum value
3. Set checksum field to zero in memory
4. Compute checksum over length bytes
5. Compare with saved value

**Important**: length is the total on-disk size of the structure (header + payload). Not just the header. Not just the payload. The checksum validates the entire self-describing unit.  
**Algorithm**: A simple CRC-like approach: accumulate with a polynomial, rotate left by one, XOR with byte. It's robust enough for corruption detection and trivial to implement.

# Records
**Record storage semantics**: Records live in the data area. record_alloc finds free blocks via the bitmap, writes a record there, and returns the record_id (which is also the block number where the record lives). Simple, self-describing, no separate inode table. record_count and next_record_id in the superblock track metadata but not physical location.

**Record layout on disk**: A record is one or more blocks. The first block contains smkfs_record_t header followed by packed attributes. If attributes exceed one block, the record has an SMKFS_ATTRT_EXTENTS attribute describing where the overflow blocks are. But for now, records are single-block (4KB). The header's length field tells us the total size.

**Attribute layout**: Attributes are packed sequentially after the record header. Each starts with smkfs_attr_header_t (type, flags, id, length), followed by length bytes of data. SMKFS_ATTRT_END (0x0000) terminates the list. The id field can be used for multi-valued attributes (e.g., multiple extents).

**record_id**: Since records are stored in data blocks, record_id == block_number. This is elegant; no indirection. The superblock's root_record is the block number of the root directory record.

# API
The SmKFS API is divided into four levels

## Level 4: Static Internal Interface
These are the primitives everything else builds on. They don't know about paths, directories, or user concepts. They know about blocks, records, attributes, extents, trees, and the journal.

### Block I/O
`static int read_block(uint64_t block, void *buf);`  
**Purpose**: Read a single 4096-byte block from disk into a buffer. This is the fundamental read primitive, every other "read" in SmKFS eventually calls this.  
**Design note**: The IDE driver works in 512-byte sectors. One SmKFS block is 8 sectors. We translate the block number to LBA, then read 8 consecutive sectors. We use the drive number stored in the global drive_num.  
**Parameters**:
- block: logical block number within the filesystem (0 = superblock)
- buf: destination buffer, must be at least SMKFS_BLOCK_SIZE bytes  

**Returns**: 0 on success, -1 on error (IDE failure)  

---

`static int write_block(uint64_t block, const void *buf);`  
**Purpose**: Write a single 4096-byte block from a buffer to disk. The fundamental write primitive. Same sector-by-sector translation as read_block.  
**Parameters**:
- block: logical block number within the filesystem
- buf: source buffer, must contain at least SMKFS_BLOCK_SIZE bytes

**Returns**: 0 on success, -1 on error  

---

### Header
`static void header_init(smkfs_header_t *h, uint16_t type, uint32_t length, uint32_t flags);`  
**Purpose**: opulate a canonical header with magic, version, type, length, and flags. Does NOT compute checksum; caller fills payload then calls header_checksum_update.  
**Parameters**:
- h: pointer to header to initialize
- type: structure type (SMKFS_ST_*)
- length: total size of structure in bytes
- flags: structure-specific flags

---

`static int header_validate(const smkfs_header_t *h, uint16_t expected_type);`  
**Purpose**: Check magic, version, and type match expectations. Does NOT verify checksum, caller does that separately if needed.  
**Parameters**:
- h: pointer to header read from disk
- expected_type: what structure type we expect here  

**Returns**: 0 if valid, -1 if magic/version/type mismatch

---

### Checksum
`static uint32_t checksum_compute(const void *data, size_t len);`  
**Purpose**: Compute checksum over arbitrary data. The checksum field is assumed zero if it's part of the data.
**Parameters**:
- data: pointer to data
- len: number of bytes  

**Returns**: 32-bit checksum value

---

`static void header_checksum_update(smkfs_header_t *h, const void *data, size_t len);`  
**Purpose**: Compute checksum over entire structure and store it in header. Caller must have already called header_init and filled payload.  
**Parameters**:
- h: pointer to header (at start of structure)
- data: same pointer as h, for clarity; the structure to checksum
- len: same as h->length, total structure size

---

`static int header_checksum_verify(const smkfs_header_t *h, const void *data, size_t len);`  
**Purpose**: Verify checksum of a structure read from disk.
**Parameters**:
- h: pointer to header
- data: pointer to entire structure
- len: total structure size

**Returns**: 0 if checksum valid, -1 if corrupt

---


### Bitmap
`static void bitmap_set(uint64_t block);`  
**Purpose**: Mark a single block as allocated in the bitmap.  
**Parameters**:
- block: absolute block number to mark allocated

---

`static void bitmap_clear(uint64_t block);`  
**Purpose**: Mark a single block as free in the bitmap.  
**Parameters**:
- block: absolute block number to mark free

---

`static int bitmap_test(uint64_t block);`  
**Purpose**: Check if a block is allocated.  
**Parameters**:
- block: absolute block number to test

**Returns**: 1 if allocated, 0 if free, -1 if invalid block number

---

`static uint64_t bitmap_alloc_range(uint32_t count);`  
**Purpose**: Find and allocate a contiguous range of count free blocks. This is the core of extent-based allocation; we want contiguous runs.  
**Parameters**:
- count: number of contiguous blocks needed

**Returns**: first absolute block number of allocated range, or -1 on fail  
**Algorithm**: Scan bitmap linearly, looking for count consecutive free bits. First-fit. Not optimal, but simple and predictable.

---

`static uint64_t bitmap_alloc(void);`  
**Purpose**: Allocate a single block. Wrapper around bitmap_alloc_range(1).  
**Returns**: absolute block number, or -1 on fail

---

`static void bitmap_free_range(uint64_t start, uint32_t count);`  
**Purpose**: Mark a contiguous range of blocks as free.  
**Parameters**:
- start: absolute block number of first block to free
- count: number of blocks to free

---


### Record
`static int record_read(uint64_t record_id, smkfs_record_t *rec, void *attr_buf, size_t buf_size);`  
**Purpose**: Read a record header and its attributes from disk.  
**Parameters**:
- record_id: block number where record lives
- rec: output buffer for record header
- attr_buf: output buffer for attributes
- buf_size: size of attr_buf

**Returns**: bytes of attributes read, or -1 on error

---

`static int record_write(uint64_t record_id, const smkfs_record_t *rec, const void *attr_buf);`  
**Purpose**: Write a record header and attributes to disk.  
**Parameters**:
- record_id: block number where record lives
- rec: record header (must have valid header.length)
- attr_buf: attribute data

**Returns**: 0 on success, -1 on error

---

`static uint64_t record_alloc(uint16_t object_type);`  
**Purpose**: Allocate a new block, initialize it as a record, write to disk.  
**Parameters**:
- object_type: SMKFS_ROT_FILE, DIR, etc.

**Returns**: record_id (block number), or -1 on fail

---

`static void record_free(uint64_t record_id);`  
**Purpose**: Free all resources owned by a record (extents, then the record block itself).  
**Parameters**:
- record_id: block number of record to free

---

`static int record_find_attr(const void *attr_buf, uint16_t attr_type, void **out_attr, size_t out_len);`  
**Purpose**: Scan attribute list for first matching type.  
**Parameters**:
- attr_buf: pointer to attribute data
- attr_type: attribute type to find
- out_attr: output pointer to attribute data (after header)
- out_len: output length of attribute data

**Returns**: 0 if found, -1 if not found

---

`static int record_add_attr(void *attr_buf, size_t buf_size, uint16_t attr_type, const void *data, size_t data_len);`  
**Purpose**: Append a new attribute to the attribute list. Replaces existing attribute of same type if present.  
**Parameters**:
- attr_buf: attribute buffer to modify
- buf_size: total size of attr_buf
- attr_type: type to add
- data: attribute data
- data_len: length of data

**Returns**: 0 on success, -1 if no space

---

`static int record_remove_attr(void *attr_buf, uint16_t attr_type);`  
**Purpose**: Remove an attribute from the list by compacting subsequent attributes.  
**Parameters**:
- attr_buf: attribute buffer to modify
- attr_type: type to remove

**Returns**: 0 if removed or not found, -1 on error

---


### Extent
`static int extent_resolve(uint64_t record_id, uint64_t logical_block, smkfs_extent_t *out);`  
**Purpose**: Find which extent covers a given logical block.  
**Parameters**:
- record_id: record to query
- logical_block: block offset within the file
- out: output extent

**Returns**: 0 if found, -1 if not found

---

`static int extent_add(uint64_t record_id, uint64_t logical_block, uint64_t physical_block, uint32_t out);`  
**Purpose**: Add a new extent to a record's extent list  
**Parameters**:
- record_id: record to modify
- logical_block: starting logical offset
- physical_block: starting physical block
- count: number of blocks

**Returns**: 0 on success, -1 on error

---

`static void extent_remove_all(uint64_t record_id);`  
**Purpose**: Free all blocks described by all extents in a record, then remove the extents attribute.  
**Parameters**:
- record_id: record to clean

---


### B+ tree
`static int btree_search(uint64_t root_block, const char *key, uint64_t *out_value);`  
**Purpose**: Search a B+ tree for a key and return the associated value  
**Paramters**:
- root_block: starting block of the tree
- key: lookup key
- out_value: destination for the matched record id.

**Returns**: 0 on success, -1 when the key is not found or input is invalid  
**Algorithm**: Descends through the tree by following child pointers or, for leaf nodes, scans the sorted names until the match is found and then follows sibling links

---

`static int btree_insert(uint64_t root_block, const char *key, uint64_t value, uint64_t *new_root);`  
**Purpose**: Insert a key/value pair into a B+ tree.  
**Parameters**:
- root_block: root block of the tree
- key: key to insert
- value: record id to store
- new_root: output root block

**Returns**: 0 on success, -1 on fail.  
**Algorithm**: Creates a new root leaf when the tree is empty; otherwise delegates to the recursive insertion routine.


---

`static int btree_delete(uint64_t root_block, const char *key, uint64_t *new_root);`  
**purpose**: Remove a key/value pair from a B+ tree.  
**parameters**:
- root_block: root block of the tree
- key: key to remove
- new_root: output root block after the operation.

**Returns**: 0 on success, -1 when the key is absent or input is invalid.  
**Algorithm**: Reads the leaf node, removes the entry from the sorted payload, rewrites the node, and returns the current root block.

---

`static int btree_iterate(uint64_t root_block, int (*cb)(uint64_t key, uint64_t value, void *ctx), void *ctx);`  
**Purpose**: Visit every key/value pair in a B+ tree in key order.  
**Parameters**:
- root_block: root block of the tree
- cb: callback invoked for each entry
- ctx: caller context passed to the callback

**Returns**: 0 when the walk completes, -1 on traversal or callback errors  
**Algorithm**: Reads the current leaf node, invokes the callback for each entry, and then follows the right sibling link until the tree is exhausted.

---


## Journal
`static int journal_start_transaction(void);`  
**Purpose**: Begin a new transaction. Sets the in-transaction flag. Returns a transaction ID (sequence number of first entry)  
**Returns**: -1 if already in a transaction, otherwise the next sequence number

---

`static int journal_log_write(uint64_t block, const void *old_data, const void *new_data, size_t len);`  
**Purpose**: Log a block write for undo-redo. Stores both old and new data so the journal can restore the old state (undo) or reapply the new state (redo) 
**Parameters**:
- block: absolute block number being modified
- old_data: original block contents
- new_data: new block contents
- len: data length (up to SMKFS_BLOCK_SIZE)

**Returns**: 0 on success, -1 on fail (not in transaction, length too large, or write error)

---

`static int journal_log_alloc(uint64_t block, uint32_t count);`  
**Purpose**: Log a block allocation. For redo: mark blocks allocated. For undo: mark blocks free  
**Parameters**:
- block: first absolute block number being allocated
- count: number of contiguous blocks allocated

**Returns**: 0 on success, -1 on fail


---

`static int journal_log_free(uint64_t block, uint32_t count);`  
**Purpose**: Log a block free. For redo: mark blocks free. For undo: mark blocks allocated  
**Parameters**:
- block: first absolute block number beeing freed
- count: number of contiguous blocks freed

**Returns**: 0 on success, -1 on fail

---

`static int journal_commit(void);`  
**Purpose**: Write a commit record, clear in-transaction flag. The commit record marks all previous entries in this transaction as durable  
**Returns**: 0 on success, -1 on fail (not in transaction or write error)

---

`static int journal_replay(void);`  
**Purpose**: Scan journal, find uncommitted transactions, replay them. Called during smkfs_mount if journal is non-empty  
**Returns**: 0 on success, -1 on fail  
**Algorithm**: Scan all journal blocks. Group entries by transaction (entries between commits). If last group has no commit, replay: for WRITE, restore old_data; for ALLOC, free blocks; for FREE, allocate blocks. Then clear journal

---


## Level 3: Kernel Interface
These are what the VFS layer calls. They operate on record IDs and raw structures, not paths. The kernel translates paths to record IDs before calling these.

`int smkfs_mount(uint8_t drive);`  
**Purpose**: Initialize the filesystem, read and validate the superblock, replay the journal if needed, and mark the filesystem as mounted. This is the entry point for making a SmKFS volume accessible  
**Parameters**:
- drive: IDE drive number to mount

**Returns**: 0 on success, -1 on fail

---

`int smkfs_unmount(void);`  
**Purpose**: Safely dismount the filesystem. Flushes the journal, writes the superblock to disk, and clears the mounted flag  
**Returns**: 0 on success, -1 on fail

---

`int smkfs_sync(void);`  
**Purpose**: Force all filesystem state to disk without unmounting. Writes the superblock and flushes any pending journal entries.  
**Returns**: 0 on success, -1 on fail

---

`int smkfs_lookup_by_name(uint64_t dir_record, const char *name, uint64_t *out_record);`  
**Purpose**: Search a directory's B+ tree for a filename and return the associated record ID. This is the core name-to-record resolution used by path traversal  
**Parameters**:
- dir_record: block number of the directory record
- name: filename to look up
- out_record: output pointer for the found record ID

**Returns**: 0 on success, -1 if not found or invalid input

---

`int smkfs_create_record(uint16_t object_type, uint64_t parent_dir, const char *name, uint64_t *out_record);` 
**Purpose**: Create a new filesystem object, store its name in its own record, add to parent directory's B+ tree  
**Parameters**:
- object_type: SMKFS_ROT_FILE, SMKFS_ROT_DIR, etc.
- parent_dir: block number of parent directory record
- name: name for the new object
- out_record: output pointer for new record ID

**Returns**: 0 on success, -1 on fail

---
 
`int smkfs_delete_record(uint64_t record_id);`  
**Purpose**: Remove a record from its parent directory and free all resources  
**Parameters**:
- record_id: block number of record to delete

**Returns**: 0 on success, -1 on fail

---

`int smkfs_rename(uint64_t record_id, uint64_t new_parent, const char *new_name);`  
**Purpose**: Move a record to a new parent directory and/or rename it. Updates the record's stored name, removes from old parent's B+ tree, inserts into new parent's B+ tree  
**Parameters**:
- record_id: block number of record to rename
- new_parent: block number of new parent directory
- new_name: new name for the object

**Returns**: 0 on success, -1 on fail

---

`int smkfs_read(uint64_t record_id, uint64_t offset, size_t len, void *buf);`  
**Purpose**: Read data from a file record at a given offset  
**Parameters**:
- record_id: block number of file record
- offset: byte offset within file
- len: number of bytes to read
- buf: destination buffer

**Returns**: bytes read, or -1 on error

---

`int smkfs_write(uint64_t record_id, uint64_t offset, size_t len, const void *buf);`  
**Purpose**: Write data to a file record at a given offset. Allocates blocks as needed  
**Parameters**:
- record_id: block number of file record
- offset: byte offset within file
- len: number of bytes to write
- buf: source buffer

**Returns**: bytes written, or -1 on error

---

`int smkfs_truncate(uint64_t record_id, uint64_t new_size);`  
**Purpose**: Resize a file. If shrinking, free extents beyond new size. If growing, update size (blocks allocated on demand by write)  
**Parameters**:
- record_id: block number of file record
- new_size: new file size in bytes

**Returns**: 0 on success, -1 on error

---

`int smkfs_getattr(uint64_t record_id, smkfs_record_t *rec, void *attr_buf, size_t buf_size);`  
**Purpose**: Read a record's header and all attributes.  
**Parameters**:
- record_id: block number of record
- rec: output buffer for record header
- attr_buf: output buffer for attributes
- buf_size: size of attr_buf

**Returns**: bytes of attributes read, or -1 on error

---

`int smkfs_setattr(uint64_t record_id, uint16_t attr_type, const void *data, size_t len);` 
**Purpose**: Add or replace an attribute on a record  
**Parameters**:
- record_id: block number of record
- attr_type: attribute type to set
- data: attribute data
- len: data length

**Returns**: 0 on success, -1 on error

---
 

## Level 2: User Interface
These are the POSIX-like calls a userspace program makes. Phonon's transparency principle means these should explain themselves; good error messages, clear behavior, no hidden magic.

`int path_lookup(const char *path, uint64_t *out_record)`  
**Purpose**: resolves a path string to a record_id  
**Parameters**:
- path: path to resolve
- out_record: record_id corrensponding to path

**Returns**: 0 on success, -1 on fail

---

`int smkfs_open(const char *path, int flags);`  
**Purpose**: Open a file by path, returning a file descriptor handle. The fd tracks current record_id and file offset for subsequent read/write/seek operations   
**Parameters**:
- path: absolute path to file
- flags: open flags (O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, etc.)

&&: fd on success, -1 on fail

---

`int smkfs_close(int fd);`  
**Purpose**: Release a file descriptor  
**Parameters**:
- fd: file descriptor to close

**Returns**: 0 on success, -1 on fail

---

`int smkfs_read_file(int fd, void *buf, size_t len);`  
**Purpose**: Read from an open file at current offset  
**Parameters**:
- fd: file descriptor
- buf: destination buffer
- len: bytes to read

**Returns**: bytes read, or -1 on error

---

`int smkfs_write_file(int fd, const void *buf, size_t len);`  
**Purpose**: Write to an open file at current offset  
**Parameters**:
- fd: file descriptor
- buf: source buffer
- len: bytes to write

**Returns**: bytes written, or -1 on error

---

`int smkfs_seek(int fd, int64_t offset, int whence);`  
**Purpose**: Adjust file offset in fd table  
**Parameters**:
- fd: file descriptor
- offset: offset value
- whence: SEEK_SET (0), SEEK_CUR (1), SEEK_END (2)

**Returns**: new offset, or -1 on error

---

`int smkfs_create_file(const char *path, uint16_t permissions);`  
**Purpose**: Create a new file at the given path  
**Parameters**:
- path: drive-prefixed path
- permissions: file permissions (stored as attribute)

**Returns**: 0 on success, -1 on fail

---

`int smkfs_delete_file(const char *path);`  
**Purpose**: Delete a file by path  
**Parameters**:
- path: path of file to delete

**Returns**: 0 on success, -1 on fail

---

`int smkfs_mkdir(const char *path);`  
**Purpse**: Create a directory
**Parameters**:
- path: path of directory to create

**Returns**: 0 on success, -1 on fail

---

`int smkfs_rmdir(const char *path);`  
**Purpse**: Delete a directory
**Parameters**:
- path: path of directory to delete

**Returns**: 0 on success, -1 on fail

---

`int readdir_cb(const char *key, uint64_t value, void *ctx);`  
**Purpose**: btree_iterate callback used by smkfs_readdir. Copies one B+ tree key/value pair (filename, record_id) into the caller's dirent array via the readdir_ctx_t passed as ctx  
**Parameters**:
- key: filename string from the B+ tree entry
- value: record_id associated with that filename
- ctx: pointer to a readdir_ctx_t; holds the destination entries array, its capacity (max), and the running count

**Returns**: 0 on success, -1 if ctx->count has reached ctx->max (signals btree_iterate to stop early)

---

`int smkfs_readdir(const char *path, smkfs_dirent_t *entries, size_t max_entries, size_t *out_count);`  
**Purpose**: Read directory entries. Resolves path to its record, confirms it's a directory, finds its B+ tree root via the SMKFS_ATTRT_DATA attribute, then iterates the tree collecting name/record_id pairs via readdir_cb  
**Parameters**:
- path: drive-prefixed path of the directory to read
- entries: caller-provided array to receive directory entries
- max_entries: capacity of the entries array; iteration stops once this many entries have been collected
- out_count: output pointer, set to the number of entries actually written into entries

**Returns**: 0 on success, -1 on error (not mounted, invalid path, not a directory, or missing data attribute)

---

`int smkfs_stat(const char *path, smkfs_record_t *rec, void *attr_buf, size_t buf_size);`  
**Purpose**: Get file/directory statistics by reading the record and its attributes  
**Parameters**:
- path: drive-prefixed path
- rec: output buffer for record header
- attr_buf: output buffer for attributes
- buf_size: size of attr_buf

**Returns**: 0 on success, -1 on error

---

`int smkfs_chmod(const char *path, uint16_t permissions);`  
**Purpose**: Change file permissions  
**Parameters**:
- path: path of file to modify
- permissions: new file permissions

**Returns**: 0 on success, -1 on error

---

`int smkfs_chown(const char *path, uint32_t uid, uint32_t gid);`  
**Purpose**: Change the owner and group of a file or directory. Packs uid and gid into a single 64-bit SMKFS_ATTRT_OWNER attribute (uid in the high 32 bits, gid in the low 32 bits) and writes it via smkfs_setattr  
**Parameters**:
- path: drive-prefixed path of the file or directory to modify
- uid: new User IDentifier, stored in the upper 32 bits of the owner attribute
- gid: new Group IDentifier, stored in the lower 32 bits of the owner attribute

**Returns**: 0 on success, -1 on error (not mounted, invalid path, lookup failure, or setattr failure)

---


## Level 1: Administrative Interface
These are the "does this even work" functions. The ones that create a filesystem from nothing, verify it's healthy, and repair it if not.

`int smkfs_mkfs(uint8_t drive, uint64_t total_blocks);`  
**Purpose**: Create a new SmKFS filesystem on the specified drive. Writes the superblock, bitmap, root directory record, empty journal, and initializes all metadata structures  
**Parameters**:
- drive: drive number to format
- total_blocks: total number of 4KB blocks available on the drive

**Returns**: 0 on success, -1 on fail  
**Algorithm**
Algorithm:
1. Validate total_blocks is sufficient (minimum ~16 blocks for superblock + bitmap + journal + root dir + data)
2. Calculate layout: superblock at 0, bitmap after, journal after bitmap, data area after journal
3. Write zeroed superblock with magic, version, layout parameters
4. Zero and write bitmap blocks
5. Zero and write journal blocks
6. Create root directory record with empty B+ tree
7. Write final superblock with checksum

---

`int smkfs_fsck(uint8_t drive);`  
**Purpose**: Verify filesystem consistency. Check superblock, bitmap, and basic record integrity. Report errors, optionally repair  
**Parameters**:
- drive: drive number to check
**Returns**: 0 if clean, 1 if errors found and fixed, -1 if unrecoverable

---

`int smkfs_dump_superblock(void);`  
**Purpose**: Print superblock information for debugging and transparency. Displays volume parameters, layout, and state  
**Returns**: 0 on success, -1 if not mounted

---

`int smkfs_dump_record(uint64_t record_id);`  
**Purpose**: Print a record's header and all attributes for debugging and transparency  
**Parameters**:
- record_id: block number of record to dump
**Returns**: 0 on success, -1 on fail

---

`int smkfs_dump_journal(void);`  
**Purpose**: Print all journal entries for debugging and crash analysis  
**Returns**: 0 on success, -1 if not mounted

---

`int smkfs_dump_btree(uint64_t root_block);`
**Purpose**: Recursively print B+ tree structure for debugging  
**Parameters**:
- root_block: root block of tree to dump

**Returns**: 0 on success, -1 on fail

---
