#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>
#include <proc.h>

/**
 * Global Variables
 */
struct open_file_table *OFT;

struct open_file *ofe_create(void) {
    struct open_file *ofe;

    ofe = NULL;
    ofe = kmalloc(sizeof(*ofe));
    if (ofe == NULL) {
        return NULL;
    }

    return ofe;
}

int ofe_destroy(struct open_file *file) {
    lock_destroy(file->of_lock);
    kfree(file);
    return 0;
}

/**
 * Initialise an open file entry.
 */
int ofe_init(struct open_file **ret_ofe, int flags, struct vnode *vn) {
    struct open_file *ofe;
    ofe = kmalloc(sizeof(struct open_file));
    ofe->of_lock = lock_create("file lock");
    if (ofe->of_lock == NULL) {
        vfs_close(vn);
        kfree(ofe);
        return ENOMEM;
    }
    ofe->vn = vn;
    ofe->of_offset = 0;
    ofe->of_flags = flags & O_ACCMODE;
    ofe->fd_count = 1;

    *ret_ofe = ofe;

    return 0;
}

/**
 * Open file table is created and initialised within the bootup process.
 * Stdin, stdout, stderr are attached to console and placed into Open file table in order.
 */
int oft_bootstrap(void) {
    struct open_file *std_in;
	struct open_file *std_out;
	struct open_file *std_err;
	struct vnode *retvn_in;
	struct vnode *retvn_out;
	struct vnode *retvn_err;
	int result;
	char con_str[5];

    OFT = NULL;
    OFT = kmalloc(sizeof(struct open_file_table));
    if (OFT == NULL) {
        return ENOMEM;
    }
    for (int i = 0; i < __OPEN_MAX; i++) {
        OFT->of_entries[i] = NULL;
    }

    strcpy(con_str, "con:");
    result = vfs_open(con_str, O_RDONLY, 0, &retvn_in);
	if (result) {
		return result;
	}
	result = ofe_init(&std_in, O_RDONLY, retvn_in);
	OFT->of_entries[0] = std_in;

	strcpy(con_str, "con:");
    result = vfs_open(con_str, O_WRONLY, 0, &retvn_out);
	if (result) {
		return result;
	}
	result = ofe_init(&std_out, O_WRONLY, retvn_out);
	OFT->of_entries[1] = std_out;
	
	strcpy(con_str, "con:");
    result = vfs_open(con_str, O_WRONLY, 0, &retvn_err);
	if (result) {
		return result;
	}
	result = ofe_init(&std_err, O_WRONLY, retvn_err);
	OFT->of_entries[2] = std_err;

    return 0;
}

int oft_destroy(void) {
    for (int i = 0; i < __OPEN_MAX; i++) {
        if (OFT->of_entries[i] != NULL) {
            ofe_destroy(OFT->of_entries[i]);
        }
    }
    kfree(OFT);
    return 0;
}

struct open_file *oft_search(struct vnode *vn) {
    int i;
    for (i = 0; i < __OPEN_MAX; i++) {
        if (OFT->of_entries[i] != NULL) {
            if (OFT->of_entries[i]->vn == vn) {
                return OFT->of_entries[i];
            }
        }
    }
    return NULL;
}

struct open_file *oft_create_ofe(int flags, struct vnode *vn) {
    int i;
    int result;
    struct open_file *ofe;

    ofe = oft_search(vn);
    if (ofe != NULL) {
        return ofe;
    }
    
    result = ofe_init(&ofe, flags, vn);
    KASSERT(result == 0);

    for (i = 0; i < __OPEN_MAX; i++) {
        if (OFT->of_entries[i] == NULL) {
            OFT->of_entries[i] = ofe;
            return ofe;
        }
    }

    return NULL;
}

int oft_close_ofe(struct vnode *vn) {
    int i;

    for (i = 0; i < __OPEN_MAX; i++) {
        if (OFT->of_entries[i]->vn == vn) {
            ofe_destroy(OFT->of_entries[i]);
            OFT->of_entries[i] = NULL;
            return 0;
        }
    }
    return EBADF;
}

/**
 * Allocates the file descriptor table in memory and returns the pointer to it.
 */
struct file_descriptor_table *fdt_init(void) {
    struct file_descriptor_table *fdt;
    int i;

    fdt = kmalloc(sizeof(*fdt));
    if (fdt == NULL) {
        return NULL;
    }

    for (i = 0; i < __OPEN_MAX; i++) {
        fdt->fd_entries[i] = NULL;
    }

    return fdt;
}

/**
 * Destroys the file descriptor table in memory.
 */
int fdt_destroy(void) {
    struct file_descriptor_table *fdt;
    int i;

    fdt = curproc->p_ft;
    if (fdt == NULL) {
        return EFAULT;
    }

    for (i = 0; i < __OPEN_MAX; i++) {
        kfree(fdt->fd_entries[i]);
        fdt->fd_entries[i] = NULL;
    }

    kfree(fdt);
    fdt = NULL;
    return 0;
}

/**
 * Creates a new file descriptor to the open file.
 */
int fdt_new_fd(struct open_file *file, int *fd) {
    struct file_descriptor_table *fdt;
    int i;

    fdt = curproc->p_ft;
    if (fdt == NULL) {
        return EFAULT;
    }

    // Search file descriptor table for next NULL position
    for (i = 0; i < __OPEN_MAX; i++) {
        if (fdt->fd_entries[i] == NULL) {
            lock_acquire(file->of_lock);
            fdt->fd_entries[i] = file;
            fdt->fd_entries[i]->fd_count++;
            *fd = i;
            lock_release(file->of_lock);
            return 0;
        }
    }

    // File descriptor table is full
    return EMFILE;
}

/**
 * Invalidates the given file descriptor by assigning the file pointer to NULL.
 */
int fdt_close_fd(int *fd) {
    struct file_descriptor_table *fdt;

    fdt = curproc->p_ft;
    if (fdt == NULL) {
        return EFAULT;
    }

    // Check if given file descriptor is already invalidated
    if (fdt->fd_entries[*fd] == NULL) {
        return EBADF;
    }

    fdt->fd_entries[*fd]->fd_count--;
    fdt->fd_entries[*fd] = NULL;
    return 0;
}

/**
 * Finds the open file entry from the file descriptor table given the file
 * descriptor.
 */
int fdt_find_ofe(int fd, struct open_file **ofe) {
    struct file_descriptor_table *fdt;

    fdt = curproc->p_ft;
    if (fdt == NULL) {
        kprintf("EFAULT\n");
        return EFAULT;
    }

    if (fd < 0 || fd > __OPEN_MAX) {
        kprintf("EBADF1\n");
        return EBADF;
    }

    *ofe = fdt->fd_entries[fd];
	if (*ofe== NULL) {
        kprintf("EBADF2\n");
		return EBADF;
	}

    return 0;
}

/**
 * sys_open
 */
int sys_open(char *file_name, int flags, mode_t mode, int *retfd) {
    char fname[PATH_MAX];
    int result;
    struct vnode *vn;
    struct open_file *ofe;

    strcpy(fname, file_name);

    result = vfs_open(file_name, flags, mode, &vn);
    if (result) {
        return result;
    }

    ofe = oft_create_ofe(flags, vn);
    KASSERT(ofe != NULL);

    ofe->of_offset = 0; // reset offset when file is opened

    /* check invalid flags */
    KASSERT(ofe->of_flags == O_RDONLY || ofe->of_flags == O_WRONLY ||
            ofe->of_flags == O_RDWR);

    /* place the file in the FD table, getting the file descriptor */
    *retfd = -1;
    result = fdt_new_fd(ofe, retfd);
    if (result) {
        ofe_destroy(ofe);
        vfs_close(vn);
        return result;
    }

    return 0;
}

/**
 * sys_close
 */
int sys_close(int fd) {
    struct open_file *ofe;
    int result;

    result = fdt_find_ofe(fd, &ofe);
    if (result) {
        return result;
    }

    lock_acquire(ofe->of_lock);

    if (ofe->fd_count == 1) {
        vfs_close(ofe->vn);
        result = fdt_close_fd(&fd);
        if (result) {
            return result;
        }
        lock_release(ofe->of_lock);
        result = oft_close_ofe(ofe->vn);
        if (result) {
            return result;
        }
    }
    else {
        vfs_close(ofe->vn);
        result = fdt_close_fd(&fd);
        if (result) {
            return result;
        }
        lock_release(ofe->of_lock);
    }
    
    return 0;
}

/**
 * sys_read
 */
int sys_read(int fd, void *buf, size_t buflen, int *retlen) {
    struct open_file *ofe;
    struct iovec iov;
    struct uio kuio;
    int result;
    char *buftmp;
    
    buftmp = NULL;
    buftmp = (char*)kmalloc(sizeof(*buf) * buflen);
    *retlen = -1;
    
    result = fdt_find_ofe(fd, &ofe);
    if (result) {
        goto cleanupB;
    }

    if (ofe->of_flags == O_WRONLY) {
        result = EACCES;
        goto cleanupB;
	}

    lock_acquire(ofe->of_lock);

    uio_kinit(&iov, &kuio, buftmp, buflen, ofe->of_offset, UIO_READ);

    result = VOP_READ(ofe->vn, &kuio);
    if (result) {
        goto cleanupA;
    }
    
    ofe->of_offset = kuio.uio_offset; // update offset into file
    *retlen = buflen - kuio.uio_resid; // get how much of buffer is read

    // negative retlen case will be handled in copyout
	if (*retlen != 0) {
        result = copyout(buftmp, (userptr_t)buf, *retlen);
        if (result) {
            goto cleanupA;
        }
    }

    result = 0;

cleanupA:
    lock_release(ofe->of_lock);

cleanupB:
    kfree(buftmp);
    buftmp = NULL;

    return result;
}

int sys_write(int fd, void *buf, size_t buflen, int *retlen) {
    struct open_file *ofe;
    struct iovec iov;
    struct uio kuio;
    int result;
    char *buftmp;
    
    buftmp = NULL;
    buftmp = (char*)kmalloc(sizeof(*buf) * buflen);
    *retlen = -1;

	result = fdt_find_ofe(fd, &ofe);
	if (result) {
        goto cleanupB;
	}

    if (ofe->of_flags == O_RDONLY) {
        result = EACCES;
        goto cleanupB;
    }

    result = copyin((const_userptr_t)buf, buftmp, buflen);
    if (result) {
        goto cleanupB;
    }

    lock_acquire(ofe->of_lock);

    uio_kinit(&iov, &kuio, buftmp, buflen, ofe->of_offset, UIO_WRITE);

    result = VOP_WRITE(ofe->vn, &kuio);
    if (result) {
        goto cleanupA;
    }

    ofe->of_offset = kuio.uio_offset; // update offset into file
    *retlen = buflen - kuio.uio_resid; // get how much of buffer is written

    result = 0;

cleanupA:
    lock_release(ofe->of_lock);

cleanupB:
    kfree(buftmp);
    buftmp = NULL;

    return result;
}

int sys_lseek(int fd, off_t pos, int whence, off_t *retval) {
    struct stat tmp_stat;
    struct open_file *ofe;
    off_t offset;
    int result;
    bool result_bool;

    result = fdt_find_ofe(fd, &ofe);
    if (result) {
        return result;
    }

    lock_acquire(ofe->of_lock);

    switch (whence) {
    case SEEK_SET:
        offset = pos;
        break;

    case SEEK_CUR:
        offset = ofe->of_offset + pos;
        break;

    case SEEK_END:
        result = VOP_STAT(ofe->vn, &tmp_stat);
		if(result) {
            return result;
        }	
		offset = tmp_stat.st_size + pos;

    default:
        lock_release(ofe->of_lock);
        return EINVAL;
    }

    if(offset < 0) {
		lock_release(ofe->of_lock);
		return EINVAL;
	}

	result_bool = VOP_ISSEEKABLE(ofe->vn);
	if(result_bool != true){
		return result;
    }
    ofe->of_offset = offset;
    *retval = offset;
    lock_release(ofe->of_lock);
    return 0;
}

int sys_dup2(int oldfd, int newfd) {
    struct file_descriptor_table *fdt = curproc->p_ft;
    int result;
    struct open_file *ofe;

    if (INVALID_FD(oldfd) || INVALID_FD(newfd)) {
		return EBADF;
	}

    ofe = fdt->fd_entries[oldfd];
    if (ofe == NULL) {
        return EBADF;
    }

    if (oldfd == newfd) {
        return 0;
    }

    if (fdt->fd_entries[newfd] != NULL) {
		result = sys_close(newfd);
		if (result) {
			return result;
		}
	}

    lock_acquire(ofe->of_lock);
	ofe->fd_count++;
	lock_release(ofe->of_lock);

    fdt->fd_entries[newfd] = ofe;

    return 0;
}
