/*
	* fs/smkfs/btree.c - B+ Tree Implementation
	* Author:   amity
	* Date:     Wed Jul 29 17:38:42 2026
	* Copyright © 2026 OwlyNest
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

/* --- Functions ---*/

int btree_node_read(smkfs_mount_t *mnt, uint64_t block, smkfs_btree_node_t *node, void *payload, size_t payload_size) {
    uint8_t raw[SMKFS_BLOCK_SIZE];
    
	if (!node || block == 0) return SMKFS_ERR_INVAL;
    if (payload_size > 0 && !payload) return SMKFS_ERR_INVAL;
    if (read_block(mnt, block, raw) != 0) return SMKFS_ERR_IO;
    
	memcpy(node, raw, sizeof(*node));
    
	if (header_validate(&node->header, SMKFS_ST_BTREE_NODE) != 0) {
        return SMKFS_ERR_CORRUPT;
	}
    
	if (header_checksum_verify(&node->header, raw, node->header.length) != 0) {
        return SMKFS_ERR_CORRUPT;
	}

    if (node->header.length < sizeof(*node)) return SMKFS_ERR_CORRUPT;
	
    if (node->header.length - sizeof(*node) > payload_size) {
		return SMKFS_ERR_TOO_BIG;
	}

    memcpy(payload, raw + sizeof(*node), node->header.length - sizeof(*node));
    return SMKFS_OK;
}

int btree_node_write(smkfs_mount_t *mnt, uint64_t block_id, const smkfs_btree_node_t *node,  const void *payload, size_t payload_size) {
    uint8_t *raw, *old_raw;
    int ret;

    if (!node || block_id == 0) return SMKFS_ERR_INVAL;
    if (payload_size > 0 && !payload) return SMKFS_ERR_INVAL;

    raw = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    old_raw = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
    if (!raw || !old_raw) {
        free(raw);
        free(old_raw);
        return SMKFS_ERR_NOMEM;
    }

    if (read_block(mnt, block_id, old_raw) != 0) {
        memset(old_raw, 0, SMKFS_BLOCK_SIZE);
	}

    memset(raw, 0, SMKFS_BLOCK_SIZE);
    memcpy(raw, node, sizeof(*node));
    if (payload_size > 0) memcpy(raw + sizeof(*node), payload, payload_size);
    ((smkfs_btree_node_t *)raw)->header.length = sizeof(*node) + payload_size;
    header_checksum_update(&((smkfs_btree_node_t *)raw)->header, raw, ((smkfs_btree_node_t *)raw)->header.length);

    journal_log_write(mnt, block_id, old_raw, raw, sizeof(*node) + payload_size);

    ret = write_block(mnt, block_id, raw);
    
	if (ret != 0) {
        printk("[SmKFS] btree_node_write FAILED: block_id=%llu\n", block_id);
    }

    free(raw);
    free(old_raw);
    return (ret == 0) ? SMKFS_OK : SMKFS_ERR_IO;
}

static int btree_leaf_insert_entry(smkfs_btree_node_t *node, smkfs_btree_leaf_entry_t *entries, uint32_t *count, const char *key, uint64_t value) {
    uint32_t pos = 0;
    uint32_t max_entries = (uint32_t)((SMKFS_BLOCK_SIZE - sizeof(smkfs_btree_node_t)) / sizeof(smkfs_btree_leaf_entry_t));

    if (!node || !entries || !count || !key) return SMKFS_ERR_INVAL;
    for (uint32_t i = 0; i < *count; i++) {
        int cmp = strcmp(entries[i].name, key);
        if (cmp == 0) {
            entries[i].record_id = value;
            return SMKFS_OK;
        }
        if (cmp > 0) {
            pos = i;
            break;
        }
        pos = i + 1;
    }

    if (*count >= max_entries) return 1;
    memmove(&entries[pos + 1], &entries[pos], ((*count - pos) * sizeof(smkfs_btree_leaf_entry_t)));
    memset(&entries[pos], 0, sizeof(smkfs_btree_leaf_entry_t));
    strncpy(entries[pos].name, key, SMKFS_NAME_LEN - 1);
    entries[pos].record_id = value;
    (*count)++;
    return SMKFS_OK;
}

static int btree_leaf_remove_entry(smkfs_btree_node_t *node, smkfs_btree_leaf_entry_t *entries, uint32_t *count, const char *key) {
    uint32_t index = 0;

    if (!node || !entries || !count || !key) return SMKFS_ERR_INVAL;
    for (uint32_t i = 0; i < *count; i++) {
        if (strcmp(entries[i].name, key) == 0) {
            index = i;
            break;
        }
    }
    if (index >= *count) return SMKFS_ERR_NOTFOUND;
    memmove(&entries[index], &entries[index + 1], ((*count - index - 1) * sizeof(smkfs_btree_leaf_entry_t)));
    memset(&entries[*count - 1], 0, sizeof(smkfs_btree_leaf_entry_t));
    (*count)--;
    return SMKFS_OK;
}

static int btree_insert_recursive(smkfs_mount_t *mnt, uint64_t block, const char *key, uint64_t value, uint64_t *new_root) {
    smkfs_btree_node_t node;
    smkfs_btree_leaf_entry_t entries[64];
    uint32_t count = 0;
    uint64_t new_leaf = 0;
    uint64_t next_sibling = 0;
    uint32_t split_point = 0;
    if (!key || !new_root || block == 0) return SMKFS_ERR_INVAL;
    if (btree_node_read(mnt, block, &node, entries, sizeof(entries)) != 0) {
        return SMKFS_ERR_IO;
	}

    if (!(node.flags & SMKFS_BTN_LEAF)) return SMKFS_ERR_INVAL;

    count = node.key_count;
    if (btree_leaf_insert_entry(&node, entries, &count, key, value) == 1) {
        new_leaf = bitmap_alloc(mnt);
        if (new_leaf == 0) return SMKFS_ERR_NOSPC;
        split_point = count / 2;
        next_sibling = node.right_sibling;
        node.right_sibling = new_leaf;
        node.key_count = split_point;
        
		if (btree_node_write(mnt, block, &node, entries, node.key_count * sizeof(smkfs_btree_leaf_entry_t)) != 0) {
            return SMKFS_ERR_IO;
		}

        memset(&node, 0, sizeof(node));
        header_init(&node.header, SMKFS_ST_BTREE_NODE, sizeof(node), SMKFS_BTN_LEAF);
        node.parent_block = 0;
        node.flags = SMKFS_BTN_LEAF;
        node.key_count = count - split_point;
        node.right_sibling = next_sibling;
        
		if (btree_node_write(mnt, new_leaf, &node, &entries[split_point], node.key_count * sizeof(smkfs_btree_leaf_entry_t)) != 0) {
            return SMKFS_ERR_IO;
		}

        *new_root = block;
        return SMKFS_OK;
    }

    node.key_count = count;
    return btree_node_write(mnt, block, &node, entries, count * sizeof(smkfs_btree_leaf_entry_t));
}

int btree_search(smkfs_mount_t *mnt, uint64_t root_block, const char *key, uint64_t *out_value) {
    uint8_t block[SMKFS_BLOCK_SIZE];
    smkfs_btree_node_t node;
    smkfs_btree_leaf_entry_t entries[64];
    smkfs_btree_index_entry_t index_entries[64];

    if (!key || !out_value || root_block == 0) return SMKFS_ERR_INVAL;
    if (read_block(mnt, root_block, block) != 0) return SMKFS_ERR_IO;
    
	memcpy(&node, block, sizeof(node));
    
	if (header_validate(&node.header, SMKFS_ST_BTREE_NODE) != 0) {
        return SMKFS_ERR_CORRUPT;
	}
    
	if (header_checksum_verify(&node.header, block, node.header.length) != 0) {
        return SMKFS_ERR_CORRUPT;
	}

    if (node.flags & SMKFS_BTN_LEAF) {
        memcpy(entries, block + sizeof(node), node.key_count * sizeof(smkfs_btree_leaf_entry_t));

        for (uint32_t i = 0; i < node.key_count; i++) {
            if (strcmp(entries[i].name, key) == 0) {
                *out_value = entries[i].record_id;
                return SMKFS_OK;
            }
        }

        if (node.right_sibling != 0) {
            return btree_search(mnt, node.right_sibling, key, out_value);
        }

        return SMKFS_ERR_NOTFOUND;
    }

    memcpy(index_entries, block + sizeof(node), node.key_count * sizeof(smkfs_btree_index_entry_t));

    for (uint32_t i = 0; i < node.key_count; i++) {
        if (strcmp(key, index_entries[i].prefix) < 0) {
            return btree_search(mnt, index_entries[i].child_block, key, out_value);
        }
    }

    return btree_search(mnt, index_entries[node.key_count - 1].child_block, key, out_value);
}

int btree_insert(smkfs_mount_t *mnt, uint64_t root_block, const char *key, uint64_t value, uint64_t *new_root) {
    uint64_t new_block = 0;
    smkfs_btree_node_t node;
    smkfs_btree_leaf_entry_t entries[64];

    if (!key || !new_root) return SMKFS_ERR_INVAL;

    if (root_block == 0) {
        new_block = bitmap_alloc(mnt);
        if (new_block == 0) return SMKFS_ERR_NOSPC;
        memset(&node, 0, sizeof(node));
        header_init(&node.header, SMKFS_ST_BTREE_NODE, sizeof(node), SMKFS_BTN_LEAF | SMKFS_BTN_ROOT);
        node.parent_block = 0;
        node.flags = SMKFS_BTN_LEAF | SMKFS_BTN_ROOT;
        node.key_count = 1;
        memset(entries, 0, sizeof(entries));
        strncpy(entries[0].name, key, SMKFS_NAME_LEN - 1);
        entries[0].record_id = value;

        if (btree_node_write(mnt, new_block, &node, entries, sizeof(smkfs_btree_leaf_entry_t)) != 0) {
            return SMKFS_ERR_IO;
		}

        *new_root = new_block;
        return SMKFS_OK;
    }
    return btree_insert_recursive(mnt, root_block, key, value, new_root);
}

int btree_delete(smkfs_mount_t *mnt, uint64_t root_block, const char *key, uint64_t *new_root) {
    smkfs_btree_node_t node;
    smkfs_btree_leaf_entry_t entries[64];
    uint32_t count = 0;

    if (!key || !new_root || root_block == 0) return SMKFS_ERR_INVAL;

    if (btree_node_read(mnt, root_block, &node, entries, sizeof(entries)) != 0) {
        return SMKFS_ERR_IO;
	}

    count = node.key_count;
    if (btree_leaf_remove_entry(&node, entries, &count, key) != 0) {
        return SMKFS_ERR_NOTFOUND;
	}

    node.key_count = count;

    if (btree_node_write(mnt, root_block, &node, entries, count * sizeof(smkfs_btree_leaf_entry_t)) != 0) {
        return SMKFS_ERR_IO;
	}

    *new_root = root_block;
    return SMKFS_OK;
}

int btree_iterate(smkfs_mount_t *mnt, uint64_t root_block, int (*cb)(const char *key, uint64_t value, void *ctx), void *ctx) {
    uint8_t block[SMKFS_BLOCK_SIZE];
    smkfs_btree_node_t node;
    smkfs_btree_leaf_entry_t entries[64];

    if (!cb || !ctx || root_block == 0) return SMKFS_ERR_INVAL;
    if (read_block(mnt, root_block, block) != 0) return SMKFS_ERR_IO;

    memcpy(&node, block, sizeof(node));

    if (header_validate(&node.header, SMKFS_ST_BTREE_NODE) != 0) {
        return SMKFS_ERR_CORRUPT;
	}

    if (header_checksum_verify(&node.header, block, node.header.length) != 0) {
        return SMKFS_ERR_CORRUPT;
	}

    memcpy(entries, block + sizeof(node), node.key_count * sizeof(smkfs_btree_leaf_entry_t));
    for (uint32_t i = 0; i < node.key_count; i++) {
        int r = cb(entries[i].name, entries[i].record_id, ctx);
        if (r != 0) return r;
    }
    if (node.right_sibling != 0) {
        return btree_iterate(mnt, node.right_sibling, cb, ctx);
    }
    return SMKFS_OK;
}

void btree_dump_recursive(smkfs_mount_t *mnt, uint64_t block, int depth) {
    uint8_t raw[SMKFS_BLOCK_SIZE];
    smkfs_btree_node_t node;
    smkfs_btree_leaf_entry_t leaf_entries[64];
    smkfs_btree_index_entry_t index_entries[64];
    int indent = depth * 2;

    if (block == 0) return;
    if (read_block(mnt, block, raw) != 0) return;

    memcpy(&node, raw, sizeof(node));
    if (header_validate(&node.header, SMKFS_ST_BTREE_NODE) != 0) return;

    printk("%*sNode %llu: %s, keys=%u, parent=%llu, sibling=%llu\n", indent, "", block, (node.flags & SMKFS_BTN_LEAF) ? "LEAF" : "INDEX", node.key_count, node.parent_block, node.right_sibling);

    if (node.flags & SMKFS_BTN_LEAF) {
        memcpy(leaf_entries, raw + sizeof(node), node.key_count * sizeof(smkfs_btree_leaf_entry_t));
        for (uint32_t i = 0; i < node.key_count; i++) {
            printk("%*s  '%s' -> %llu\n", indent, "", leaf_entries[i].name, leaf_entries[i].record_id);
        }
    } else {
        memcpy(index_entries, raw + sizeof(node), node.key_count * sizeof(smkfs_btree_index_entry_t));
        for (uint32_t i = 0; i < node.key_count; i++) {
            printk("%*s  '%s' -> child %llu\n", indent, "", index_entries[i].prefix, index_entries[i].child_block);
            btree_dump_recursive(mnt, index_entries[i].child_block, depth + 1);
        }
    }
}