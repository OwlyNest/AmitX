/*
	* fs/smkfs/btree.c - B+ Tree Implementation (G1, complete)
	* Author:   amity
	* Date:     Wed Jul 29 17:38:42 2026
	* Copyright © 2026 OwlyNest
*/

/*
	* Phase 4 rewrite: full multi-level B+ tree.
	*
	* On-disk layout
	* --------------
	* Every node is one block: a 56-byte _SMKFS_BTREE_NODE header followed
	* by a sorted array of fixed-size entries. Leaf entries are
	* {record_id, name[256]}, internal (index) entries are
	* {child_block, key[256]}. Both are 264 bytes, so every node holds at
	* most SMKFS_BTR_LEAF_MAX == SMKFS_BTR_INDEX_MAX == 15 entries.
	*
	* Internal entries use fence keys: entry[i].key is the lowest key
	* reachable through entry[i].child_block. A node with key_count == n
	* therefore has exactly n children, and entry[0].key is the lowest key
	* of the whole subtree. Searches descend to the child with the largest
	* fence key <= the search key.
	*
	* Invariants maintained here
	* --------------------------
	*   - entries are strictly sorted by key, keys are unique
	*   - non-root nodes hold between MIN and MAX entries
	*     (the root leaf may be empty: that is an empty directory)
	*   - leaf nodes are linked left-to-right through right_sibling
	*   - parent_block always names the node's parent (0 at the root)
	*   - fence keys are exact: after every insert/delete, entry[i].key
	*     equals the lowest key currently in child i's subtree
	*
	* All node buffers are heap-allocated; nothing in this file puts a
	* node-sized array on the (small) kernel stack.
*/

/* --- Styling Instructions ---
	* Encoding:                      UTF-8, Unix line endings
	* Text font:                     Monospace
	* Line width:                    Max 80 characters
	* Indentation:                   Use 4 spaces
	* Brace style:                   Same line as control statement
	* Inline comments:               Column 40, wherever possible, else, whole multiple of 20
	* Section headers:               Use 3 '-' characters before and after
	* Pointer notation:              Next to variable name, not type
	* Binary operations:             Space around operator
	* Empty parameter list:          Use (void) instead of ()
	* Statements and declarations:   Max one per line
*/

/* --- Macros ---*/

/* --- Includes ---*/
#include <fs/smkfs.h>
#include <fs/smkfs_internal.h>
#include <mm/heap.h>
#include <lib/string.h>
#include <screen/printk.h>

/* --- Typedefs - Structs - Enums ---*/

/* --- Globals ---*/

/* --- Prototypes ---*/
static SIZE_T btree_entry_size(ULONG flags);
static PCCHAR btree_key_at(PUCHAR raw, ULONG index);
static VOID   btree_key_copy(CHAR *dst, PCCHAR src);
static VOID   btree_format(PUCHAR raw, ULONG flags, SMKFS_BLOCK parent);
static SMKFS_STATUS btree_load(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PUCHAR raw);
static SMKFS_STATUS btree_store(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PUCHAR raw);
static SMKFS_STATUS btree_set_parent(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, SMKFS_BLOCK parent);
static ULONG  btree_leaf_lower_bound(const _SMKFS_BTREE_NODE *node, PCCHAR key);
static ULONG  btree_index_child(const _SMKFS_BTREE_NODE *node, PCCHAR key);
static SMKFS_STATUS btree_insert_rec(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PCCHAR key, ULONGLONG value, _SMKFS_BTREE_SPLIT *split);
static SMKFS_STATUS btree_leaf_insert(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PUCHAR raw, PCCHAR key, ULONGLONG value, _SMKFS_BTREE_SPLIT *split);
static SMKFS_STATUS btree_leaf_split_insert(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PUCHAR raw, ULONG pos, PCCHAR key, ULONGLONG value, _SMKFS_BTREE_SPLIT *split);
static SMKFS_STATUS btree_index_insert(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PUCHAR raw, PCCHAR key, ULONGLONG value, _SMKFS_BTREE_SPLIT *split);
static SMKFS_STATUS btree_index_split_insert(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PUCHAR raw, ULONG pos, PCCHAR sep, SMKFS_BLOCK right_block, _SMKFS_BTREE_SPLIT *split);
static SMKFS_STATUS btree_delete_rec(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PCCHAR key, LONG is_root, LONG *out_underflow);
static SMKFS_STATUS btree_fix_underflow(_SMKFS_MOUNT *mnt, PUCHAR parent_raw, ULONG child_idx);
static SMKFS_STATUS btree_refresh_fence(_SMKFS_MOUNT *mnt, PUCHAR parent_raw, ULONG child_idx);

/* --- Functions ---*/

/* --- Low-level node helpers --- */

static SIZE_T btree_entry_size(ULONG flags) {
    if (flags & SMKFS_BTN_LEAF) return sizeof(_SMKFS_BTREE_LEAF_ENTRY);
    return sizeof(_SMKFS_BTREE_INDEX_ENTRY);
}

/* Key of entry `index`, whatever the node type. */
static PCCHAR btree_key_at(PUCHAR raw, ULONG index) {
    _SMKFS_BTREE_NODE *node = (_SMKFS_BTREE_NODE *)raw;

    if (node->flags & SMKFS_BTN_LEAF) {
        _SMKFS_BTREE_LEAF_ENTRY *entries;
        entries = (_SMKFS_BTREE_LEAF_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE));
        return entries[index].name;
    }

    {
        _SMKFS_BTREE_INDEX_ENTRY *entries;
        entries = (_SMKFS_BTREE_INDEX_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE));
        return entries[index].key;
    }
}

static VOID btree_key_copy(CHAR *dst, PCCHAR src) {
    strncpy(dst, src, SMKFS_NAME_LEN - 1);
    dst[SMKFS_NAME_LEN - 1] = '\0';
}

/* Initialise a raw block as an empty node. */
static VOID btree_format(PUCHAR raw, ULONG flags, SMKFS_BLOCK parent) {
    _SMKFS_BTREE_NODE *node = (_SMKFS_BTREE_NODE *)raw;

    memset(raw, 0, SMKFS_BLOCK_SIZE);
    header_init(&node->header, SMKFS_ST_BTREE_NODE, sizeof(_SMKFS_BTREE_NODE), flags);
    node->parent_block = parent;
    node->flags = flags;
    node->key_count = 0;
    node->right_sibling = 0;
}

/*
 * Read a node block and prove it is a sane B+ tree node before any
 * caller looks at the payload: magic/version/type, bounded length,
 * CRC32C, and a key count that physically fits in one block.
*/

static SMKFS_STATUS btree_load(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PUCHAR raw) {
    _SMKFS_BTREE_NODE *node = (_SMKFS_BTREE_NODE *)raw;
    ULONG max_entries;

    if (block == 0) return SMKFS_ERR_INVAL;
    if (read_block(mnt, block, raw) != SMKFS_OK) return SMKFS_ERR_IO;
    if (header_validate(&node->header, SMKFS_ST_BTREE_NODE) != SMKFS_OK) {
        return SMKFS_ERR_CORRUPT;
    }
    if (node->header.length < sizeof(_SMKFS_BTREE_NODE) ||
        node->header.length > SMKFS_BLOCK_SIZE) {
        return SMKFS_ERR_CORRUPT;
    }
    if (header_checksum_verify(&node->header, raw, node->header.length) != SMKFS_OK) {
        return SMKFS_ERR_CORRUPT;
    }

    max_entries = (node->flags & SMKFS_BTN_LEAF) ? SMKFS_BTR_LEAF_MAX : SMKFS_BTR_INDEX_MAX;
    if (node->key_count > max_entries) return SMKFS_ERR_CORRUPT;
    if (node->header.length - sizeof(_SMKFS_BTREE_NODE) <
        node->key_count * btree_entry_size(node->flags)) {
        return SMKFS_ERR_CORRUPT;
    }

    return SMKFS_OK;
}

/*
 * Write a node back: recompute the length from the key count, refresh
 * the checksum, journal the change, then hit the disk.
*/
static SMKFS_STATUS btree_store(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PUCHAR raw) {
    _SMKFS_BTREE_NODE *node = (_SMKFS_BTREE_NODE *)raw;
    PUCHAR old_raw;
    SMKFS_STATUS ret;

    if (block == 0) return SMKFS_ERR_INVAL;

    node->header.length = (ULONG)(sizeof(_SMKFS_BTREE_NODE) + node->key_count * btree_entry_size(node->flags));
    header_checksum_update(&node->header, raw, node->header.length);

    old_raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!old_raw) return SMKFS_ERR_NOMEM;
    if (read_block(mnt, block, old_raw) != SMKFS_OK) {
        memset(old_raw, 0, SMKFS_BLOCK_SIZE);
    }

    ret = journal_log_write(mnt, block, old_raw, raw, node->header.length);
    free(old_raw);
    if (ret != SMKFS_OK) {
        return ret;
    }

    ret = write_block(mnt, block, raw);
    if (ret != SMKFS_OK) {
        printk("[SmKFS] btree_store FAILED: block=%llu\n", block);
        return SMKFS_ERR_IO;
    }
    return SMKFS_OK;
}

/* Point a child's parent_block at a (new) parent. */
static SMKFS_STATUS btree_set_parent(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, SMKFS_BLOCK parent) {
    PUCHAR raw;
    SMKFS_STATUS ret;

    raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!raw) return SMKFS_ERR_NOMEM;

    ret = btree_load(mnt, block, raw);
    if (ret == SMKFS_OK) {
        ((_SMKFS_BTREE_NODE *)raw)->parent_block = parent;
        ret = btree_store(mnt, block, raw);
    }

    free(raw);
    return ret;
}

/* First leaf slot whose key is >= key; key_count if all are smaller. */
static ULONG btree_leaf_lower_bound(const _SMKFS_BTREE_NODE *node, PCCHAR key) {
    const _SMKFS_BTREE_LEAF_ENTRY *entries;
    ULONG pos = 0;

    entries = (const _SMKFS_BTREE_LEAF_ENTRY *) ((PCUCHAR)node + sizeof(_SMKFS_BTREE_NODE));
    while (pos < node->key_count && strcmp(entries[pos].name, key) < 0) {
        pos++;
    }
    return pos;
}

/*
 * Fence-key descent: pick the child with the largest fence key <= key.
 * Entry 0 is the fallback, so a key smaller than every fence still
 * lands in the leftmost subtree.
*/

static ULONG btree_index_child(const _SMKFS_BTREE_NODE *node, PCCHAR key) {
    const _SMKFS_BTREE_INDEX_ENTRY *entries;
    ULONG i = 0;

    entries = (const _SMKFS_BTREE_INDEX_ENTRY *) ((PCUCHAR)node + sizeof(_SMKFS_BTREE_NODE));
    while (i + 1 < node->key_count && strcmp(entries[i + 1].key, key) <= 0) {
        i++;
    }
    return i;
}

SMKFS_STATUS btree_node_read(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, _SMKFS_BTREE_NODE *node, PVOID payload, SIZE_T payload_size) {
    PUCHAR raw;
    SIZE_T payload_len;
    SMKFS_STATUS ret;

    if (!node || block == 0) return SMKFS_ERR_INVAL;
    if (payload_size > 0 && !payload) return SMKFS_ERR_INVAL;

    raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!raw) return SMKFS_ERR_NOMEM;

    ret = btree_load(mnt, block, raw);
    if (ret != SMKFS_OK) {
        free(raw);
        return ret;
    }

    memcpy(node, raw, sizeof(*node));
    payload_len = node->header.length - sizeof(*node);

    if (payload_len > 0) {
        if (payload_len > payload_size) {
            free(raw);
            return SMKFS_ERR_TOO_BIG;
        }
        memcpy(payload, raw + sizeof(*node), payload_len);
    }

    free(raw);
    return SMKFS_OK;
}

/*
 * Write a node built in memory. The caller's node must carry the final
 * flags and key_count; the length and checksum are recomputed here.
*/

SMKFS_STATUS btree_node_write(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block_id, const _SMKFS_BTREE_NODE *node, PCVOID payload, SIZE_T payload_size) {
    PUCHAR raw;
    SIZE_T expected;
    SMKFS_STATUS ret;

    if (!node || block_id == 0) return SMKFS_ERR_INVAL;
    if (payload_size > 0 && !payload) return SMKFS_ERR_INVAL;

    expected = node->key_count * btree_entry_size(node->flags);
    if (payload_size != expected) return SMKFS_ERR_INVAL;

    raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!raw) return SMKFS_ERR_NOMEM;

    memset(raw, 0, SMKFS_BLOCK_SIZE);
    memcpy(raw, node, sizeof(*node));
    if (payload_size > 0) {
        memcpy(raw + sizeof(*node), payload, payload_size);
    }

    ret = btree_store(mnt, block_id, raw);
    free(raw);
    return ret;
}

SMKFS_STATUS btree_search(_SMKFS_MOUNT *mnt, SMKFS_BLOCK root_block, PCCHAR key, ULONGLONG *out_value) {
    PUCHAR raw;
    SMKFS_BLOCK block;
    SMKFS_STATUS ret = SMKFS_OK;

    if (!mnt || !key || !out_value || root_block == 0) return SMKFS_ERR_INVAL;

    raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!raw) return SMKFS_ERR_NOMEM;

    block = root_block;
    while (1) {
        _SMKFS_BTREE_NODE *node;
        ULONG idx;

        ret = btree_load(mnt, block, raw);
        if (ret != SMKFS_OK) break;

        node = (_SMKFS_BTREE_NODE *)raw;

        if (node->flags & SMKFS_BTN_LEAF) {
            _SMKFS_BTREE_LEAF_ENTRY *entries;
            ULONG pos;

            entries = (_SMKFS_BTREE_LEAF_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE));
            pos = btree_leaf_lower_bound(node, key);
            if (pos < node->key_count &&
                strcmp(entries[pos].name, key) == 0) {
                *out_value = entries[pos].record_id;
                ret = SMKFS_OK;
            } else {
                ret = SMKFS_ERR_NOTFOUND;
            }
            break;
        }

        if (node->key_count == 0) {
            ret = SMKFS_ERR_CORRUPT;
            break;
        }

        idx = btree_index_child(node, key);
        block = ((_SMKFS_BTREE_INDEX_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE)))[idx].child_block;
    }

    free(raw);
    return ret;
}

/*
 * Insert into a leaf. On a duplicate key the stored value is updated
 * in place. When the leaf is full the insert is combined with the
 * split so the entries are only moved once.
*/

static SMKFS_STATUS btree_leaf_insert(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PUCHAR raw, PCCHAR key, ULONGLONG value, _SMKFS_BTREE_SPLIT *split) {
    _SMKFS_BTREE_NODE *node = (_SMKFS_BTREE_NODE *)raw;
    _SMKFS_BTREE_LEAF_ENTRY *entries;
    ULONG pos;

    entries = (_SMKFS_BTREE_LEAF_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE));
    pos = btree_leaf_lower_bound(node, key);

    if (pos < node->key_count && strcmp(entries[pos].name, key) == 0) {
        entries[pos].record_id = value;
        return btree_store(mnt, block, raw);
    }

    if (node->key_count >= SMKFS_BTR_LEAF_MAX) {
        return btree_leaf_split_insert(mnt, block, raw, pos, key, value, split);
    }

    memmove(&entries[pos + 1], &entries[pos],
            (node->key_count - pos) * sizeof(_SMKFS_BTREE_LEAF_ENTRY));
    memset(&entries[pos], 0, sizeof(_SMKFS_BTREE_LEAF_ENTRY));
    btree_key_copy(entries[pos].name, key);
    entries[pos].record_id = value;
    node->key_count++;

    return btree_store(mnt, block, raw);
}

/*
 * Split a full leaf around the new entry. The left half stays in the
 * existing block, the right half moves to a freshly allocated block
 * which is linked into the sibling chain. The separator handed to the
 * parent is the right node's first key - its low fence.
*/

static SMKFS_STATUS btree_leaf_split_insert(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PUCHAR raw, ULONG pos, PCCHAR key, ULONGLONG value, _SMKFS_BTREE_SPLIT *split) {
    _SMKFS_BTREE_NODE *node = (_SMKFS_BTREE_NODE *)raw;
    _SMKFS_BTREE_NODE *rnode;
    _SMKFS_BTREE_LEAF_ENTRY *entries;
    _SMKFS_BTREE_LEAF_ENTRY *rentries;
    _SMKFS_BTREE_LEAF_ENTRY *tmp;
    PUCHAR right_raw;
    SMKFS_BLOCK right_block;
    ULONG total;
    ULONG left_count;
    ULONG right_count;
    SMKFS_STATUS ret;

    entries = (_SMKFS_BTREE_LEAF_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE));
    total = node->key_count + 1;
    left_count = total / 2;
    right_count = total - left_count;

    tmp = (_SMKFS_BTREE_LEAF_ENTRY *)malloc(total * sizeof(_SMKFS_BTREE_LEAF_ENTRY));
    right_raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!tmp || !right_raw) {
        free(tmp);
        free(right_raw);
        return SMKFS_ERR_NOMEM;
    }

    /* Merge the old entries and the new one into sorted order */
    memcpy(tmp, entries, pos * sizeof(_SMKFS_BTREE_LEAF_ENTRY));
    memset(&tmp[pos], 0, sizeof(_SMKFS_BTREE_LEAF_ENTRY));
    btree_key_copy(tmp[pos].name, key);
    tmp[pos].record_id = value;
    memcpy(&tmp[pos + 1], &entries[pos], (node->key_count - pos) * sizeof(_SMKFS_BTREE_LEAF_ENTRY));

    right_block = bitmap_alloc(mnt);
    if (right_block == 0) {
        free(tmp);
        free(right_raw);
        return SMKFS_ERR_NOSPC;
    }

    /* Left keeps the lower half in place */
    memcpy(entries, tmp, left_count * sizeof(_SMKFS_BTREE_LEAF_ENTRY));
    node->key_count = left_count;

    /* Right gets the upper half and inherits the sibling link */
    btree_format(right_raw, SMKFS_BTN_LEAF, node->parent_block);
    rnode = (_SMKFS_BTREE_NODE *)right_raw;
    rnode->key_count = right_count;
    rnode->right_sibling = node->right_sibling;
    rentries = (_SMKFS_BTREE_LEAF_ENTRY *)(right_raw + sizeof(_SMKFS_BTREE_NODE));
    memcpy(rentries, &tmp[left_count], right_count * sizeof(_SMKFS_BTREE_LEAF_ENTRY));

    node->right_sibling = right_block;

    split->split = 1;
    split->right_block = right_block;
    btree_key_copy(split->sep_key, rentries[0].name);

    ret = btree_store(mnt, block, raw);
    if (ret == SMKFS_OK) ret = btree_store(mnt, right_block, right_raw);

    free(tmp);
    free(right_raw);
    return ret;
}

/*
 * Insert into an internal node: descend to the right child, then
 * absorb the child's split (if any) into this node.
 *
 * Fence maintenance: if the new key is smaller than the chosen
 * child's fence, the child's low key just became this key, so the
 * fence is rewritten before the node is stored.
*/

static SMKFS_STATUS btree_index_insert(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PUCHAR raw, PCCHAR key, ULONGLONG value, _SMKFS_BTREE_SPLIT *split) {
    _SMKFS_BTREE_NODE *node = (_SMKFS_BTREE_NODE *)raw;
    _SMKFS_BTREE_INDEX_ENTRY *entries;
    _SMKFS_BTREE_SPLIT child_split;
    SMKFS_STATUS ret;
    ULONG child_idx;
    ULONG pos;
    LONG fix_fence;

    entries = (_SMKFS_BTREE_INDEX_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE));
    child_idx = btree_index_child(node, key);
    fix_fence = (strcmp(key, entries[child_idx].key) < 0);

    ret = btree_insert_rec(mnt, entries[child_idx].child_block, key, value, &child_split);
    if (ret != SMKFS_OK) return ret;

    if (fix_fence) btree_key_copy(entries[child_idx].key, key);

    if (!child_split.split) {
        if (fix_fence) return btree_store(mnt, block, raw);
        return SMKFS_OK;
    }

    /* The child split: find where its separator belongs here */
    pos = 0;
    while (pos < node->key_count && strcmp(entries[pos].key, child_split.sep_key) < 0) {
        pos++;
    }

    if (node->key_count >= SMKFS_BTR_INDEX_MAX) {
        return btree_index_split_insert(mnt, block, raw, pos,
                                        child_split.sep_key,
                                        child_split.right_block, split);
    }

    memmove(&entries[pos + 1], &entries[pos], (node->key_count - pos) * sizeof(_SMKFS_BTREE_INDEX_ENTRY));
    memset(&entries[pos], 0, sizeof(_SMKFS_BTREE_INDEX_ENTRY));
    entries[pos].child_block = child_split.right_block;
    btree_key_copy(entries[pos].key, child_split.sep_key);
    node->key_count++;

    return btree_store(mnt, block, raw);
}

/*
 * Split a full internal node around the new separator entry. Unlike a
 * leaf split, the children that move to the right node must have
 * their parent_block rewritten.
*/
static SMKFS_STATUS btree_index_split_insert(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PUCHAR raw, ULONG pos, PCCHAR sep, SMKFS_BLOCK right_block, _SMKFS_BTREE_SPLIT *split) {
    _SMKFS_BTREE_NODE *node = (_SMKFS_BTREE_NODE *)raw;
    _SMKFS_BTREE_NODE *rnode;
    _SMKFS_BTREE_INDEX_ENTRY *entries;
    _SMKFS_BTREE_INDEX_ENTRY *rentries;
    _SMKFS_BTREE_INDEX_ENTRY *tmp;
    PUCHAR right_raw;
    SMKFS_BLOCK new_block;
    ULONG total;
    ULONG left_count;
    ULONG right_count;
    ULONG i;
    SMKFS_STATUS ret;

    entries = (_SMKFS_BTREE_INDEX_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE));
    total = node->key_count + 1;
    left_count = total / 2;
    right_count = total - left_count;

    tmp = (_SMKFS_BTREE_INDEX_ENTRY *)malloc(total * sizeof(_SMKFS_BTREE_INDEX_ENTRY));
    right_raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!tmp || !right_raw) {
        free(tmp);
        free(right_raw);
        return SMKFS_ERR_NOMEM;
    }

    memcpy(tmp, entries, pos * sizeof(_SMKFS_BTREE_INDEX_ENTRY));
    memset(&tmp[pos], 0, sizeof(_SMKFS_BTREE_INDEX_ENTRY));
    tmp[pos].child_block = right_block;
    btree_key_copy(tmp[pos].key, sep);
    memcpy(&tmp[pos + 1], &entries[pos], (node->key_count - pos) * sizeof(_SMKFS_BTREE_INDEX_ENTRY));

    new_block = bitmap_alloc(mnt);
    if (new_block == 0) {
        free(tmp);
        free(right_raw);
        return SMKFS_ERR_NOSPC;
    }

    memcpy(entries, tmp, left_count * sizeof(_SMKFS_BTREE_INDEX_ENTRY));
    node->key_count = left_count;

    btree_format(right_raw, node->flags & ~SMKFS_BTN_ROOT, node->parent_block);
    rnode = (_SMKFS_BTREE_NODE *)right_raw;
    rnode->key_count = right_count;
    rentries = (_SMKFS_BTREE_INDEX_ENTRY *)(right_raw + sizeof(_SMKFS_BTREE_NODE));
    memcpy(rentries, &tmp[left_count], right_count * sizeof(_SMKFS_BTREE_INDEX_ENTRY));

    /* The moved children now live under the new node */
    for (i = 0; i < right_count; i++) {
        ret = btree_set_parent(mnt, rentries[i].child_block, new_block);
        if (ret != SMKFS_OK) {
            free(tmp);
            free(right_raw);
            return ret;
        }
    }

    split->split = 1;
    split->right_block = new_block;
    btree_key_copy(split->sep_key, rentries[0].key);

    ret = btree_store(mnt, block, raw);
    if (ret == SMKFS_OK) ret = btree_store(mnt, new_block, right_raw);

    free(tmp);
    free(right_raw);
    return ret;
}

static SMKFS_STATUS btree_insert_rec(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PCCHAR key, ULONGLONG value, _SMKFS_BTREE_SPLIT *split) {
    PUCHAR raw;
    SMKFS_STATUS ret;

    split->split = 0;
    split->right_block = 0;
    split->sep_key[0] = '\0';

    raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!raw) return SMKFS_ERR_NOMEM;

    ret = btree_load(mnt, block, raw);
    if (ret == SMKFS_OK) {
        _SMKFS_BTREE_NODE *node = (_SMKFS_BTREE_NODE *)raw;

        if (node->flags & SMKFS_BTN_LEAF) {
            ret = btree_leaf_insert(mnt, block, raw, key, value, split);
        } else {
            ret = btree_index_insert(mnt, block, raw, key, value, split);
        }
    }

    free(raw);
    return ret;
}

SMKFS_STATUS btree_insert(_SMKFS_MOUNT *mnt, SMKFS_BLOCK root_block, PCCHAR key, ULONGLONG value, SMKFS_BLOCK *new_root) {
    _SMKFS_BTREE_SPLIT split;
    SMKFS_STATUS ret;
    PUCHAR raw;
    _SMKFS_BTREE_NODE *node;
    _SMKFS_BTREE_INDEX_ENTRY *entries;
    SMKFS_BLOCK new_block;
    CHAR left_low[SMKFS_NAME_LEN];

    if (!mnt || !key || !new_root) return SMKFS_ERR_INVAL;
    if (key[0] == '\0') return SMKFS_ERR_INVAL;
    if (strlen(key) > SMKFS_BTR_MAX_KEY) return SMKFS_ERR_TOO_BIG;

    /* Empty tree: the first node is a leaf and the root at once */
    if (root_block == 0) {
        _SMKFS_BTREE_LEAF_ENTRY *first;

        new_block = bitmap_alloc(mnt);
        if (new_block == 0) return SMKFS_ERR_NOSPC;

        raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
        if (!raw) return SMKFS_ERR_NOMEM;

        btree_format(raw, SMKFS_BTN_LEAF | SMKFS_BTN_ROOT, 0);
        node = (_SMKFS_BTREE_NODE *)raw;
        node->key_count = 1;
        first = (_SMKFS_BTREE_LEAF_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE));
        btree_key_copy(first->name, key);
        first->record_id = value;

        ret = btree_store(mnt, new_block, raw);
        free(raw);
        if (ret == SMKFS_OK) *new_root = new_block;
        return ret;
    }

    ret = btree_insert_rec(mnt, root_block, key, value, &split);
    if (ret != SMKFS_OK) return ret;

    if (!split.split) {
        *new_root = root_block;
        return SMKFS_OK;
    }

    /*
     * The root split, so the tree grows one level: build a new
     * internal root pointing at the old root and its new right
     * neighbor, then fix up the two children.
    */

    new_block = bitmap_alloc(mnt);
    if (new_block == 0) return SMKFS_ERR_NOSPC;

    raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!raw) return SMKFS_ERR_NOMEM;

    ret = btree_load(mnt, root_block, raw);
    if (ret != SMKFS_OK) {
        free(raw);
        return ret;
    }

    node = (_SMKFS_BTREE_NODE *)raw;
    btree_key_copy(left_low, btree_key_at(raw, 0));
    node->flags &= ~SMKFS_BTN_ROOT;
    node->header.flags = node->flags;
    node->parent_block = new_block;
    ret = btree_store(mnt, root_block, raw);
    if (ret != SMKFS_OK) {
        free(raw);
        return ret;
    }

    ret = btree_set_parent(mnt, split.right_block, new_block);
    if (ret != SMKFS_OK) {
        free(raw);
        return ret;
    }

    btree_format(raw, SMKFS_BTN_ROOT, 0);
    node = (_SMKFS_BTREE_NODE *)raw;
    node->key_count = 2;
    entries = (_SMKFS_BTREE_INDEX_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE));
    entries[0].child_block = root_block;
    btree_key_copy(entries[0].key, left_low);
    entries[1].child_block = split.right_block;
    btree_key_copy(entries[1].key, split.sep_key);

    ret = btree_store(mnt, new_block, raw);
    free(raw);
    if (ret == SMKFS_OK) *new_root = new_block;
    return ret;
}

/*
 * Re-read a child's low key into its parent fence. Used after a
 * delete removed the child's previous minimum key.
*/

static SMKFS_STATUS btree_refresh_fence(_SMKFS_MOUNT *mnt, PUCHAR parent_raw, ULONG child_idx) {
    _SMKFS_BTREE_INDEX_ENTRY *pentries;
    PUCHAR child_raw;
    SMKFS_STATUS ret;

    pentries = (_SMKFS_BTREE_INDEX_ENTRY *)(parent_raw + sizeof(_SMKFS_BTREE_NODE));

    child_raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!child_raw) return SMKFS_ERR_NOMEM;

    ret = btree_load(mnt, pentries[child_idx].child_block, child_raw);
    if (ret == SMKFS_OK) {
        if (((_SMKFS_BTREE_NODE *)child_raw)->key_count > 0) {
            btree_key_copy(pentries[child_idx].key, btree_key_at(child_raw, 0));
        }
    }

    free(child_raw);
    return ret;
}

/*
 * Repair a child that fell below the minimum occupancy.
 *
 * First try to borrow one entry from a sibling that can spare it;
 * otherwise merge. The left node always survives a merge, so the leaf
 * sibling chain only ever needs one pointer rewritten on the node we
 * already hold.
 *
 * The parent's raw image is updated in memory; the caller stores it.
*/

static SMKFS_STATUS btree_fix_underflow(_SMKFS_MOUNT *mnt, PUCHAR parent_raw, ULONG child_idx) {
    _SMKFS_BTREE_NODE *pnode = (_SMKFS_BTREE_NODE *)parent_raw;
    _SMKFS_BTREE_INDEX_ENTRY *pentries;
    _SMKFS_BTREE_NODE *cnode;
    _SMKFS_BTREE_NODE *snode;
    SMKFS_BLOCK child_block;
    SMKFS_BLOCK left_block;
    SMKFS_BLOCK right_block;
    PUCHAR child_raw = NULL;
    PUCHAR sib_raw = NULL;
    PUCHAR centries;
    PUCHAR sentries;
    SMKFS_STATUS ret;
    ULONG min;
    SIZE_T esz;
    LONG is_leaf;
    ULONG i;

    pentries = (_SMKFS_BTREE_INDEX_ENTRY *)(parent_raw + sizeof(_SMKFS_BTREE_NODE));
    child_block = pentries[child_idx].child_block;
    left_block = (child_idx > 0) ? pentries[child_idx - 1].child_block : 0;
    right_block = (child_idx + 1 < pnode->key_count) ? pentries[child_idx + 1].child_block : 0;

    if (left_block == 0 && right_block == 0) return SMKFS_ERR_CORRUPT;

    child_raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!child_raw) return SMKFS_ERR_NOMEM;

    ret = btree_load(mnt, child_block, child_raw);
    if (ret != SMKFS_OK) goto out;

    cnode = (_SMKFS_BTREE_NODE *)child_raw;
    is_leaf = (cnode->flags & SMKFS_BTN_LEAF) != 0;
    min = is_leaf ? SMKFS_BTR_LEAF_MIN : SMKFS_BTR_INDEX_MIN;
    esz = is_leaf ? sizeof(_SMKFS_BTREE_LEAF_ENTRY) : sizeof(_SMKFS_BTREE_INDEX_ENTRY);
    centries = child_raw + sizeof(_SMKFS_BTREE_NODE);

    /* 1) Borrow the left sibling's last entry */
    if (left_block != 0) {
        sib_raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
        if (!sib_raw) {
            ret = SMKFS_ERR_NOMEM;
            goto out;
        }

        ret = btree_load(mnt, left_block, sib_raw);
        if (ret != SMKFS_OK) goto out;

        snode = (_SMKFS_BTREE_NODE *)sib_raw;
        if (snode->key_count > min) {
            sentries = sib_raw + sizeof(_SMKFS_BTREE_NODE);

            memmove(centries + esz, centries, cnode->key_count * esz);
            memcpy(centries, sentries + (snode->key_count - 1) * esz, esz);
            snode->key_count--;
            cnode->key_count++;

            if (!is_leaf) {
                SMKFS_BLOCK moved;
                moved = ((_SMKFS_BTREE_INDEX_ENTRY *)centries)[0].child_block;
                ret = btree_set_parent(mnt, moved, child_block);
                if (ret != SMKFS_OK) goto out;
            }

            /* The child's low key changed */
            btree_key_copy(pentries[child_idx].key, btree_key_at(child_raw, 0));

            ret = btree_store(mnt, left_block, sib_raw);
            if (ret == SMKFS_OK) ret = btree_store(mnt, child_block, child_raw);
            goto out;
        }

        free(sib_raw);
        sib_raw = NULL;
    }

    /* 2) Borrow the right sibling's first entry */
    if (right_block != 0) {
        sib_raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
        if (!sib_raw) {
            ret = SMKFS_ERR_NOMEM;
            goto out;
        }

        ret = btree_load(mnt, right_block, sib_raw);
        if (ret != SMKFS_OK) goto out;

        snode = (_SMKFS_BTREE_NODE *)sib_raw;
        if (snode->key_count > min) {
            sentries = sib_raw + sizeof(_SMKFS_BTREE_NODE);

            memcpy(centries + cnode->key_count * esz, sentries, esz);
            memmove(sentries, sentries + esz, (snode->key_count - 1) * esz);
            cnode->key_count++;
            snode->key_count--;

            if (!is_leaf) {
                SMKFS_BLOCK moved;
                moved = ((_SMKFS_BTREE_INDEX_ENTRY *)centries)[cnode->key_count - 1].child_block;
                ret = btree_set_parent(mnt, moved, child_block);
                if (ret != SMKFS_OK) goto out;
            }

            /* The right sibling's low key changed */
            btree_key_copy(pentries[child_idx + 1].key, btree_key_at(sib_raw, 0));

            ret = btree_store(mnt, child_block, child_raw);
            if (ret == SMKFS_OK) ret = btree_store(mnt, right_block, sib_raw);
            goto out;
        }

        free(sib_raw);
        sib_raw = NULL;
    }

    /* 3) Merge into the left sibling; the left node survives */
    if (left_block != 0) {
        sib_raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
        if (!sib_raw) {
            ret = SMKFS_ERR_NOMEM;
            goto out;
        }

        ret = btree_load(mnt, left_block, sib_raw);
        if (ret != SMKFS_OK) goto out;

        snode = (_SMKFS_BTREE_NODE *)sib_raw;
        sentries = sib_raw + sizeof(_SMKFS_BTREE_NODE);

        memcpy(sentries + snode->key_count * esz, centries, cnode->key_count * esz);

        if (!is_leaf) {
            for (i = 0; i < cnode->key_count; i++) {
                SMKFS_BLOCK moved;
                moved = ((_SMKFS_BTREE_INDEX_ENTRY *)centries)[i].child_block;
                ret = btree_set_parent(mnt, moved, left_block);
                if (ret != SMKFS_OK) goto out;
            }
        }

        snode->key_count += cnode->key_count;
        if (is_leaf) snode->right_sibling = cnode->right_sibling;

        /* Drop the absorbed child's fence from the parent */
        memmove(&pentries[child_idx], &pentries[child_idx + 1], (pnode->key_count - child_idx - 1) * sizeof(_SMKFS_BTREE_INDEX_ENTRY));
        pnode->key_count--;

        bitmap_free_range(mnt, child_block, 1);
        ret = btree_store(mnt, left_block, sib_raw);
        goto out;
    }

    /* 4) No left sibling: absorb the right sibling into the child */
    sib_raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!sib_raw) {
        ret = SMKFS_ERR_NOMEM;
        goto out;
    }

    ret = btree_load(mnt, right_block, sib_raw);
    if (ret != SMKFS_OK) goto out;

    snode = (_SMKFS_BTREE_NODE *)sib_raw;
    sentries = sib_raw + sizeof(_SMKFS_BTREE_NODE);

    memcpy(centries + cnode->key_count * esz, sentries, snode->key_count * esz);

    if (!is_leaf) {
        for (i = 0; i < snode->key_count; i++) {
            SMKFS_BLOCK moved;
            moved = ((_SMKFS_BTREE_INDEX_ENTRY *)sentries)[i].child_block;
            ret = btree_set_parent(mnt, moved, child_block);
            if (ret != SMKFS_OK) goto out;
        }
    }

    cnode->key_count += snode->key_count;
    if (is_leaf) cnode->right_sibling = snode->right_sibling;

    /* Drop the absorbed right sibling's fence from the parent */
    memmove(&pentries[child_idx + 1], &pentries[child_idx + 2], (pnode->key_count - child_idx - 2) * sizeof(_SMKFS_BTREE_INDEX_ENTRY));
    pnode->key_count--;

    bitmap_free_range(mnt, right_block, 1);
    ret = btree_store(mnt, child_block, child_raw);

out:
    free(child_raw);
    free(sib_raw);
    return ret;
}

static SMKFS_STATUS btree_delete_rec(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, PCCHAR key, LONG is_root, LONG *out_underflow) {
    PUCHAR raw;
    _SMKFS_BTREE_NODE *node;
    SMKFS_STATUS ret;

    *out_underflow = 0;

    raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!raw) return SMKFS_ERR_NOMEM;

    ret = btree_load(mnt, block, raw);
    if (ret != SMKFS_OK) goto out;

    node = (_SMKFS_BTREE_NODE *)raw;

    if (node->flags & SMKFS_BTN_LEAF) {
        _SMKFS_BTREE_LEAF_ENTRY *entries;
        ULONG pos;

        entries = (_SMKFS_BTREE_LEAF_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE));
        pos = btree_leaf_lower_bound(node, key);
        if (pos >= node->key_count || strcmp(entries[pos].name, key) != 0) {
            ret = SMKFS_ERR_NOTFOUND;
            goto out;
        }

        memmove(&entries[pos], &entries[pos + 1], (node->key_count - pos - 1) * sizeof(_SMKFS_BTREE_LEAF_ENTRY));
        node->key_count--;
        memset(&entries[node->key_count], 0, sizeof(_SMKFS_BTREE_LEAF_ENTRY));

        ret = btree_store(mnt, block, raw);
        if (ret == SMKFS_OK && !is_root && node->key_count < SMKFS_BTR_LEAF_MIN) {
            *out_underflow = 1;
        }
        goto out;
    }

    {
        _SMKFS_BTREE_INDEX_ENTRY *entries;
        SMKFS_BLOCK child_block;
        ULONG child_idx;
        LONG child_underflow = 0;
        LONG dirty = 0;

        entries = (_SMKFS_BTREE_INDEX_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE));
        if (node->key_count == 0) {
            ret = SMKFS_ERR_CORRUPT;
            goto out;
        }

        child_idx = btree_index_child(node, key);
        child_block = entries[child_idx].child_block;

        ret = btree_delete_rec(mnt, child_block, key, 0, &child_underflow);
        if (ret != SMKFS_OK) goto out;

        if (child_underflow) {
            ret = btree_fix_underflow(mnt, raw, child_idx);
            if (ret != SMKFS_OK) goto out;
            dirty = 1;
        }

        /*
         * If the deleted key was the child's lowest, the fence still
         * names the old minimum. A stale-low fence never misroutes a
         * search for a key that exists, but exact fences keep the tree
         * debuggable, so refresh it from the child.
         *
         * After a merge-into-left the child's entry is gone, which the
         * block comparison below detects.
        */
        
        if (child_idx < node->key_count &&
            entries[child_idx].child_block == child_block &&
            strcmp(key, entries[child_idx].key) == 0) {
            ret = btree_refresh_fence(mnt, raw, child_idx);
            if (ret != SMKFS_OK) goto out;
            dirty = 1;
        }

        if (dirty) {
            ret = btree_store(mnt, block, raw);
            if (ret != SMKFS_OK) goto out;
        }

        if (!is_root && node->key_count < SMKFS_BTR_INDEX_MIN) {
            *out_underflow = 1;
        }
    }

out:
    free(raw);
    return ret;
}

SMKFS_STATUS btree_delete(_SMKFS_MOUNT *mnt, SMKFS_BLOCK root_block, PCCHAR key, SMKFS_BLOCK *new_root) {
    SMKFS_STATUS ret;
    LONG underflow = 0;
    PUCHAR raw;
    _SMKFS_BTREE_NODE *node;

    if (!mnt || !key || !new_root || root_block == 0) return SMKFS_ERR_INVAL;

    ret = btree_delete_rec(mnt, root_block, key, 1, &underflow);
    if (ret != SMKFS_OK) return ret;

    raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!raw) return SMKFS_ERR_NOMEM;

    ret = btree_load(mnt, root_block, raw);
    if (ret != SMKFS_OK) {
        free(raw);
        return ret;
    }

    node = (_SMKFS_BTREE_NODE *)raw;

    /*
     * An internal root reduced to a single child wastes a level:
     * promote the child. An empty root leaf is kept as-is - that is
     * simply an empty directory.
    */

    if (!(node->flags & SMKFS_BTN_LEAF) && node->key_count == 1) {
        SMKFS_BLOCK child;
        PUCHAR child_raw;
        _SMKFS_BTREE_NODE *cnode;

        child = ((_SMKFS_BTREE_INDEX_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE)))[0].child_block;

        child_raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
        if (!child_raw) {
            free(raw);
            return SMKFS_ERR_NOMEM;
        }

        ret = btree_load(mnt, child, child_raw);
        if (ret == SMKFS_OK) {
            cnode = (_SMKFS_BTREE_NODE *)child_raw;
            cnode->flags |= SMKFS_BTN_ROOT;
            cnode->header.flags = cnode->flags;
            cnode->parent_block = 0;
            ret = btree_store(mnt, child, child_raw);
        }

        free(child_raw);

        if (ret == SMKFS_OK) {
            bitmap_free_range(mnt, root_block, 1);
            *new_root = child;
        }
    } else {
        *new_root = root_block;
    }

    free(raw);
    return ret;
}

/*
 * Visit every key/value pair in key order: descend the leftmost spine
 * to the first leaf, then follow right_sibling to the end.
*/

SMKFS_STATUS btree_iterate(_SMKFS_MOUNT *mnt, SMKFS_BLOCK root_block, LONG (*cb)(PCCHAR key, ULONGLONG value, PVOID ctx), PVOID ctx) {
    PUCHAR raw;
    SMKFS_BLOCK block;
    SMKFS_STATUS ret;

    if (!mnt || !cb || root_block == 0) return SMKFS_ERR_INVAL;

    raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!raw) return SMKFS_ERR_NOMEM;

    /* Leftmost descent */
    block = root_block;
    while (1) {
        _SMKFS_BTREE_NODE *node;

        ret = btree_load(mnt, block, raw);
        if (ret != SMKFS_OK) {
            free(raw);
            return ret;
        }

        node = (_SMKFS_BTREE_NODE *)raw;
        if (node->flags & SMKFS_BTN_LEAF) break;
        if (node->key_count == 0) {
            free(raw);
            return SMKFS_ERR_CORRUPT;
        }

        block = ((_SMKFS_BTREE_INDEX_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE)))[0].child_block;
    }

    /* Leaf-chain walk */
    while (block != 0) {
        _SMKFS_BTREE_NODE *node;
        _SMKFS_BTREE_LEAF_ENTRY *entries;
        ULONG i;

        ret = btree_load(mnt, block, raw);
        if (ret != SMKFS_OK) {
            free(raw);
            return ret;
        }

        node = (_SMKFS_BTREE_NODE *)raw;
        if (!(node->flags & SMKFS_BTN_LEAF)) {
            free(raw);
            return SMKFS_ERR_CORRUPT;
        }

        entries = (_SMKFS_BTREE_LEAF_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE));
        for (i = 0; i < node->key_count; i++) {
            LONG r = cb(entries[i].name, entries[i].record_id, ctx);
            if (r != 0) {
                free(raw);
                return r;
            }
        }

        block = node->right_sibling;
    }

    free(raw);
    return SMKFS_OK;
}

VOID btree_dump_recursive(_SMKFS_MOUNT *mnt, SMKFS_BLOCK block, LONG depth) {
    PUCHAR raw;
    _SMKFS_BTREE_NODE *node;
    LONG indent = depth * 2;
    ULONG i;

    if (block == 0) return;

    raw = (PUCHAR)malloc(SMKFS_BLOCK_SIZE);
    if (!raw) return;

    if (btree_load(mnt, block, raw) != SMKFS_OK) {
        printk("%*sNode %llu: <unreadable>\n", indent, "", block);
        free(raw);
        return;
    }

    node = (_SMKFS_BTREE_NODE *)raw;
    printk("%*sNode %llu: %s%s, keys=%u, parent=%llu, sibling=%llu\n",
           indent, "", block,
           (node->flags & SMKFS_BTN_LEAF) ? "LEAF" : "INDEX",
           (node->flags & SMKFS_BTN_ROOT) ? "+ROOT" : "",
           node->key_count, node->parent_block, node->right_sibling);

    if (node->flags & SMKFS_BTN_LEAF) {
        _SMKFS_BTREE_LEAF_ENTRY *entries;
        entries = (_SMKFS_BTREE_LEAF_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE));
        for (i = 0; i < node->key_count; i++) {
            printk("%*s  '%s' -> %llu\n", indent, "", entries[i].name, entries[i].record_id);
        }
    } else {
        _SMKFS_BTREE_INDEX_ENTRY *entries;
        entries = (_SMKFS_BTREE_INDEX_ENTRY *)(raw + sizeof(_SMKFS_BTREE_NODE));
        for (i = 0; i < node->key_count; i++) {
            printk("%*s  fence '%s' -> child %llu\n", indent, "", entries[i].key, entries[i].child_block);
            btree_dump_recursive(mnt, entries[i].child_block, depth + 1);
        }
    }

    free(raw);
}