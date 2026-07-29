/*
	* fs/smkfs/dir.c - Directory Operations
	* Author:   amity
	* Date:     Wed Jul 29 17:38:51 2026
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

int smkfs_lookup_by_name(uint64_t dir_record, const char *name, uint64_t *out_record) {
    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    uint64_t *root_block;

    if (!mounted || !name || !out_record) return SMKFS_ERR_INVAL;
    
	if (record_read(dir_record, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return SMKFS_ERR_IO;
	}
    
	if (rec.object_type != SMKFS_ROT_DIR) return SMKFS_ERR_INVAL;
    
	if (record_find_attr(attr_buf, SMKFS_ATTRT_DATA, (void **)&root_block, NULL) != 0) {
        return SMKFS_ERR_NOTFOUND;
	}

    return btree_search(*root_block, name, out_record);
}

int smkfs_create_record(uint16_t object_type, uint64_t parent_dir, const char *name, uint64_t *out_record) {
    int ret = SMKFS_ERR_IO;
    size_t attr_buf_size = SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t);
    uint8_t *parent_attr;
    uint8_t *new_attr;
    smkfs_record_t parent_rec;
    uint64_t new_id;
    uint64_t btree_root = 0;
    uint64_t *parent_btree;

    if (journal_start_transaction() < 0) {
        return SMKFS_ERR_JOURNAL;
	}

    parent_attr = (uint8_t *)malloc(attr_buf_size);
    new_attr = (uint8_t *)malloc(attr_buf_size);
    if (!parent_attr || !new_attr) {
        free(parent_attr);
        free(new_attr);
        journal_commit();
        return SMKFS_ERR_NOMEM;
    }

    if (!mounted || !name || !out_record)
        goto cleanup;

    new_id = record_alloc(object_type);
    if (new_id == 0) {
        printk("[SmKFS] create_record: record_alloc failed\n");
        goto cleanup;
    }

    if (object_type == SMKFS_ROT_DIR) {
        uint8_t *empty_leaf;
        smkfs_btree_node_t *node;

        empty_leaf = (uint8_t *)malloc(SMKFS_BLOCK_SIZE);
        if (!empty_leaf) {
            record_free(new_id);
            goto cleanup;
        }

        memset(empty_leaf, 0, SMKFS_BLOCK_SIZE);
        node = (smkfs_btree_node_t *)empty_leaf;
        header_init(&node->header, SMKFS_ST_BTREE_NODE, sizeof(smkfs_btree_node_t), SMKFS_BTN_LEAF | SMKFS_BTN_ROOT);
        node->parent_block = 0;
        node->key_count = 0;
        node->right_sibling = 0;
        header_checksum_update(&node->header, empty_leaf, sizeof(smkfs_btree_node_t));

        btree_root = bitmap_alloc();
        if (btree_root == 0) {
            record_free(new_id);
            free(empty_leaf);
            goto cleanup;
        }

        if (write_block(btree_root, empty_leaf) != 0) {
            bitmap_clear(btree_root);
            record_free(new_id);
            free(empty_leaf);
            goto cleanup;
        }

        free(empty_leaf);
    }

    if (record_read(parent_dir, &parent_rec, parent_attr,
            attr_buf_size) < 0) {
        if (btree_root) bitmap_clear(btree_root);
        record_free(new_id);
        goto cleanup;
    }

    if (record_find_attr(parent_attr, SMKFS_ATTRT_DATA, (void **)&parent_btree, NULL) != 0) {
        printk("[SmKFS] create_record: DATA attr missing on parent %llu\n", parent_dir);
        if (btree_root) bitmap_clear(btree_root);
        record_free(new_id);
        goto cleanup;
    }

    uint64_t new_root = *parent_btree;
    if (btree_insert(*parent_btree, name, new_id, &new_root) != 0) {
        printk("[SmKFS] create_record: btree_insert failed, root=%llu\n", *parent_btree);
        if (btree_root) bitmap_clear(btree_root);
        record_free(new_id);
        goto cleanup;
    }

    memset(new_attr, 0, attr_buf_size);
    smkfs_attr_header_t term;
    term.type = SMKFS_ATTRT_END;
    term.flags = 0;
    term.id = 0;
    term.length = 0;
    memcpy(new_attr, &term, sizeof(term));

    record_add_attr(new_attr, attr_buf_size, SMKFS_ATTRT_NAME, name, strlen(name) + 1);
    record_add_attr(new_attr, attr_buf_size, SMKFS_ATTRT_PARENT, &parent_dir, sizeof(parent_dir));
    if (btree_root) {
        record_add_attr(new_attr, attr_buf_size, SMKFS_ATTRT_DATA, &btree_root, sizeof(btree_root));
    }

    smkfs_record_t new_rec;
    header_init(&new_rec.header, SMKFS_ST_RECORD, sizeof(smkfs_record_t), 0);
    new_rec.record_id = new_id;
    new_rec.object_type = object_type;
    new_rec.attr_count = btree_root ? 3 : 2;
    new_rec.header.length = sizeof(smkfs_record_t) + attr_buf_total_len(new_attr);

    if (record_write(new_id, &new_rec, new_attr) != 0) {
        printk("[SmKFS] create_record: record_write(new_id=%llu) failed\n", new_id);
        if (btree_root) bitmap_clear(btree_root);
        record_free(new_id);
        goto cleanup;
    }

    parent_rec.attr_count++;
    record_add_attr(parent_attr, attr_buf_size, SMKFS_ATTRT_DATA, &new_root, sizeof(new_root));
    parent_rec.header.length = sizeof(smkfs_record_t) + attr_buf_total_len(parent_attr);
    if (record_write(parent_dir, &parent_rec, parent_attr) != 0) {
        printk("[SmKFS] create_record: record_write(parent_dir=%llu) failed\n", parent_dir);
        record_free(new_id);
        goto cleanup;
    }

    *out_record = new_id;
    ret = SMKFS_OK;

cleanup:
    free(parent_attr);
    free(new_attr);
    journal_commit();
    return ret;
}

int smkfs_delete_record(uint64_t record_id) {
    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    char *name;
    uint64_t *parent_ptr;
    uint64_t parent_dir;
    uint64_t *parent_btree;
    uint8_t parent_attr[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t parent_rec;
    uint64_t new_root;

    if (!mounted || record_id == 0) return SMKFS_ERR_INVAL;
    
	if (record_read(record_id, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return SMKFS_ERR_IO;
	}

    if (rec.object_type == SMKFS_ROT_DIR) {
        uint64_t *dir_btree;
        if (record_find_attr(attr_buf, SMKFS_ATTRT_DATA, (void **)&dir_btree, NULL) == 0) {
            smkfs_btree_node_t node;
            smkfs_btree_leaf_entry_t entries[64];
            if (btree_node_read(*dir_btree, &node, entries, sizeof(entries)) == 0) {
                if (node.key_count > 0) {
                    printk("[SmKFS] Directory not empty\n");
                    return SMKFS_ERR_NOTEMPTY;
                }
            }
        }
    }

    if (record_find_attr(attr_buf, SMKFS_ATTRT_NAME, (void **)&name, NULL) != 0) {
        return SMKFS_ERR_NOTFOUND;
	}
    
	if (record_find_attr(attr_buf, SMKFS_ATTRT_PARENT, (void **)&parent_ptr, NULL) != 0) {
        return SMKFS_ERR_NOTFOUND;
	}
	
    parent_dir = *parent_ptr;

    if (record_read(parent_dir, &parent_rec, parent_attr, sizeof(parent_attr)) < 0) {
        return SMKFS_ERR_IO;
	}
    
	if (record_find_attr(parent_attr, SMKFS_ATTRT_DATA, (void **)&parent_btree, NULL) != 0) {
        return SMKFS_ERR_NOTFOUND;
	}

    new_root = *parent_btree;
    if (btree_delete(*parent_btree, name, &new_root) != 0) {
        return SMKFS_ERR_NOTFOUND;
	}

    record_free(record_id);

    parent_rec.attr_count++;
    record_add_attr(parent_attr, sizeof(parent_attr), SMKFS_ATTRT_DATA, &new_root, sizeof(new_root));
    record_write(parent_dir, &parent_rec, parent_attr);

    return SMKFS_OK;
}

int smkfs_rename(uint64_t record_id, uint64_t new_parent, const char *new_name) {
    uint8_t rec_attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    uint8_t old_parent_attr[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    uint8_t new_parent_attr[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec, old_parent_rec, new_parent_rec;
    uint64_t *old_parent_ptr;
    uint64_t old_parent;
    uint64_t *old_btree_ptr;
    uint64_t *new_btree_ptr;
    uint64_t new_root;

    if (!mounted || !new_name) return SMKFS_ERR_INVAL;

    if (record_read(record_id, &rec, rec_attr_buf, sizeof(rec_attr_buf)) < 0) {
        return SMKFS_ERR_IO;
	}
	
	if (record_find_attr(rec_attr_buf, SMKFS_ATTRT_PARENT, (void **)&old_parent_ptr, NULL) != 0) {
        return SMKFS_ERR_NOTFOUND;
	}

    old_parent = *old_parent_ptr;

    if (record_read(old_parent, &old_parent_rec, old_parent_attr, sizeof(old_parent_attr)) < 0) {
        return SMKFS_ERR_IO;
	}
    
		if (record_find_attr(old_parent_attr, SMKFS_ATTRT_DATA, (void **)&old_btree_ptr, NULL) != 0) {
        return SMKFS_ERR_NOTFOUND;
	}

    if (record_read(new_parent, &new_parent_rec, new_parent_attr, sizeof(new_parent_attr)) < 0) {
        return SMKFS_ERR_IO;
	}
    
		if (record_find_attr(new_parent_attr, SMKFS_ATTRT_DATA, (void **)&new_btree_ptr, NULL) != 0) {
        return SMKFS_ERR_NOTFOUND;
	}

    uint8_t old_name_buf[SMKFS_NAME_LEN];
    void *old_name_ptr;
    if (record_find_attr(rec_attr_buf, SMKFS_ATTRT_NAME, &old_name_ptr, NULL) != 0) {
        return SMKFS_ERR_NOTFOUND;
	}

    strncpy((char *)old_name_buf, (const char *)old_name_ptr, SMKFS_NAME_LEN - 1);
    old_name_buf[SMKFS_NAME_LEN - 1] = '\0';

    new_root = *old_btree_ptr;
    if (btree_delete(*old_btree_ptr, (const char *)old_name_buf, &new_root) != 0) {
        return SMKFS_ERR_NOTFOUND;
	}

    record_add_attr(old_parent_attr, sizeof(old_parent_attr), SMKFS_ATTRT_DATA, &new_root, sizeof(new_root));
    
	if (record_write(old_parent, &old_parent_rec, old_parent_attr) != 0) {
        return SMKFS_ERR_IO;
	}

    new_root = *new_btree_ptr;
    if (btree_insert(*new_btree_ptr, new_name, record_id, &new_root) != 0) {
        return SMKFS_ERR_NOSPC;
	}

    record_add_attr(new_parent_attr, sizeof(new_parent_attr), SMKFS_ATTRT_DATA, &new_root, sizeof(new_root));

    if (old_parent != new_parent) {
        
		if (record_write(new_parent, &new_parent_rec, new_parent_attr) != 0) {
            return SMKFS_ERR_IO;
		}
    }

    record_add_attr(rec_attr_buf, sizeof(rec_attr_buf), SMKFS_ATTRT_NAME, new_name, strlen(new_name) + 1);
    record_add_attr(rec_attr_buf, sizeof(rec_attr_buf), SMKFS_ATTRT_PARENT, &new_parent, sizeof(new_parent));
    if (record_write(record_id, &rec, rec_attr_buf) != 0) {
        return SMKFS_ERR_IO;
	}

    return SMKFS_OK;
}

static int readdir_cb(const char *key, uint64_t value, void *ctx) {
    readdir_ctx_t *c = (readdir_ctx_t *)ctx;
    if (c->count >= c->max) return 1;
    strncpy(c->entries[c->count].name, key, SMKFS_NAME_LEN - 1);
    c->entries[c->count].name[SMKFS_NAME_LEN - 1] = '\0';
    c->entries[c->count].record_id = value;
    c->count++;
    return 0;
}

int smkfs_readdir(const char *path, smkfs_dirent_t *entries, size_t max_entries, size_t *out_count) {
    uint64_t dir_record;
    uint8_t attr_buf[SMKFS_BLOCK_SIZE - sizeof(smkfs_record_t)];
    smkfs_record_t rec;
    uint64_t *btree_root;
    readdir_ctx_t ctx;

    if (!mounted || !path || !entries || !out_count) {
        return SMKFS_ERR_INVAL;
	}

    if (path[1] != ':' || path[2] != '/') return SMKFS_ERR_INVAL;

    if (path_lookup(path, &dir_record) != 0) return SMKFS_ERR_NOTFOUND;

    if (record_read(dir_record, &rec, attr_buf, sizeof(attr_buf)) < 0) {
        return SMKFS_ERR_IO;
	}
    
		if (rec.object_type != SMKFS_ROT_DIR) return SMKFS_ERR_INVAL;
    
	if (record_find_attr(attr_buf, SMKFS_ATTRT_DATA, (void **)&btree_root, NULL) != 0) {
        return SMKFS_ERR_NOTFOUND;
	}

    ctx.entries = entries;
    ctx.max = max_entries;
    ctx.count = 0;

    btree_iterate(*btree_root, readdir_cb, &ctx);

    *out_count = ctx.count;
    return SMKFS_OK;
}