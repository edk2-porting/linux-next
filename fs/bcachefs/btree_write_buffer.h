/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _BCACHEFS_BTREE_WRITE_BUFFER_H
#define _BCACHEFS_BTREE_WRITE_BUFFER_H

int bch2_btree_write_buffer_flush_locked(struct btree_trans *);
int bch2_btree_write_buffer_flush_nocheck_rw(struct btree_trans *);
int bch2_btree_write_buffer_flush_sync(struct btree_trans *);
int bch2_btree_write_buffer_tryflush(struct btree_trans *);

int bch2_btree_insert_keys_write_buffer(struct btree_trans *);

void bch2_fs_btree_write_buffer_exit(struct bch_fs *);
int bch2_fs_btree_write_buffer_init(struct bch_fs *);

#endif /* _BCACHEFS_BTREE_WRITE_BUFFER_H */
