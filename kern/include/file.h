/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

#include <limits.h> // __OPEN_MAX

#define INVALID_FD(fd) (fd < 0 || fd >= __OPEN_MAX) ? 0 : 1

/**
 * Open File Entry (ofe)
 *
 * Open file entry is removed once all file descriptors have been closed.
 */
struct open_file {
    int of_flags;         // O_RDONLY, O_WRONLY, O_RDWR
    struct vnode *vn;     // vnode
    struct lock *of_lock; // open file lock
    off_t of_offset;      // open file offset
    int fd_count;         // number of file descriptors referring this file
};

/**
 * Open file entry (ofe) functions
 */
struct open_file *ofe_create(void);                                // create ofe
int ofe_destroy(struct open_file *file);                           // destroy ofe
int ofe_init(struct open_file **file, int flags, struct vnode *vn); // initalises ofe

/**
 * Open File Table (oft)
 *
 * An OF table is simply an array of pointers to an open file struct.
 *
 * Only one OF table exists in OS.
 */
struct open_file_table {
    struct open_file *of_entries[__OPEN_MAX];
};

extern struct open_file_table *OFT;

/**
 * Open file table (oft) functions
 */
int oft_bootstrap(void);                                       // initialise oft
int oft_destroy(void);                                         // destroy oft
struct open_file *oft_create_ofe(int flags, struct vnode *vn); // create ofe in the oft
int oft_close_ofe(struct vnode *vn);                           // close ofe in the oft
struct open_file *oft_search(struct vnode *vn);

/**
 * File Descriptor Table (fdt)
 *
 * A FD table exists per process and has an array of pointers to an open file
 * struct. The FD table is not responsible for the management of the open file
 * table nor open file entry.
 *
 * There may be multiple file descriptors to the same open file entry.
 *
 * The index of the fd_entries array is the file descriptor integer.
 */
struct file_descriptor_table {
    struct open_file *fd_entries[__OPEN_MAX];
};

/**
 * File descriptor table (fdt) functions
 */
struct file_descriptor_table *fdt_init(void);     // initialise fdt
int fdt_destroy(void);                            // destroy fdt
int fdt_new_fd(struct open_file *file, int *fd);  // create new fd to file
int fdt_close_fd(int *fd);                        // closes fd
int fdt_find_ofe(int fd, struct open_file **ofe); // find required file in fdt

#endif /* _FILE_H_ */
