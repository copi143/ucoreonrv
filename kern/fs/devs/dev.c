#include <defs.h>
#include <string.h>
#include <stat.h>
#include <dev.h>
#include <inode.h>
#include <unistd.h>
#include <error.h>
#include <log.h>

/*
 * dev_open - Called for each open().
 */
static int
dev_open(struct inode *node, uint32_t open_flags) {
    infof("Attempting to open device, flags: 0x%x", open_flags);

    if (open_flags & (O_CREAT | O_TRUNC | O_EXCL | O_APPEND)) {
        errorf("Invalid flags for opening device: 0x%x", open_flags);
        return -E_INVAL;
    }

    struct device *dev = vop_info(node, device);
    int ret = dop_open(dev, open_flags);
    if (ret == 0) {
        infof("Device opened successfully.");
    } else {
        errorf("Failed to open device, error code: %d", ret);
    }
    return ret;
}


/*
 * dev_close - Called on the last close(). Just pass through.
 */
static int
dev_close(struct inode *node) {
    infof("Closing device.");

    struct device *dev = vop_info(node, device);
    int ret = dop_close(dev);
    if (ret == 0) {
        infof("Device closed successfully.");
    } else {
        errorf("Failed to close device, error code: %d", ret);
    }
    return ret;
}


/*
 * dev_read -Called for read. Hand off to iobuf.
 */
static int
dev_read(struct inode *node, struct iobuf *iob) {
    infof("Reading from device");

    struct device *dev = vop_info(node, device);
    int ret = dop_io(dev, iob, 0);
    if (ret == 0) {
        infof("Read successful");
    } else {
        errorf("Failed to read from device, error code: %d", ret);
    }
    return ret;
}

/*
 * dev_write -Called for write. Hand off to iobuf.
 */
static int
dev_write(struct inode *node, struct iobuf *iob) {
    infof("Writing to device");

    struct device *dev = vop_info(node, device);
    int ret = dop_io(dev, iob, 1);
    if (ret == 0) {
        infof("Write successful");
    } else {
        errorf("Failed to write to device, error code: %d", ret);
    }
    return ret;
}


/*
 * dev_ioctl - Called for ioctl(). Just pass through.
 */
static int
dev_ioctl(struct inode *node, int op, void *data) {
    infof("Sending ioctl command to device, op: %d", op);

    struct device *dev = vop_info(node, device);
    int ret = dop_ioctl(dev, op, data);
    if (ret == 0) {
        infof("Ioctl command executed successfully.");
    } else {
        errorf("Failed to execute ioctl command, error code: %d", ret);
    }
    return ret;
}


/*
 * dev_fstat - Called for stat().
 *             Set the type and the size (block devices only).
 *             The link count for a device is always 1.
 */
static int
dev_fstat(struct inode *node, struct stat *stat) {
    infof("Fetching device stats.");

    int ret = vop_gettype(node, &(stat->st_mode));
    if (ret != 0) {
        errorf("Failed to fetch device type, error code: %d", ret);
        return ret;
    }

    struct device *dev = vop_info(node, device);
    stat->st_nlinks = 1;
    stat->st_blocks = dev->d_blocks;
    stat->st_size = stat->st_blocks * dev->d_blocksize;

    infof("Device stats fetched successfully: blocks=%d, blocksize=%d",
          stat->st_blocks, dev->d_blocksize);
    return 0;
}


/*
 * dev_gettype - Return the type. A device is a "block device" if it has a known
 *               length. A device that generates data in a stream is a "character
 *               device".
 */
static int
dev_gettype(struct inode *node, uint32_t *type_store) {
    struct device *dev = vop_info(node, device);
    *type_store = (dev->d_blocks > 0) ? S_IFBLK : S_IFCHR;

    infof("Device type retrieved: %s",
          (*type_store == S_IFBLK) ? "block device" : "character device");
    return 0;
}


/*
 * dev_tryseek - Attempt a seek.
 *               For block devices, require block alignment.
 *               For character devices, prohibit seeking entirely.
 */
static int
dev_tryseek(struct inode *node, off_t pos) {
    struct device *dev = vop_info(node, device);
    if (dev->d_blocks > 0) {
        if ((pos % dev->d_blocksize) == 0) {
            if (pos >= 0 && pos < dev->d_blocks * dev->d_blocksize) {
                return 0;
            }
        }
    }
    return -E_INVAL;
}

/*
 * dev_lookup - Name lookup.
 *
 * One interesting feature of device:name pathname syntax is that you
 * can implement pathnames on arbitrary devices. For instance, if you
 * had a graphics device that supported multiple resolutions (which we
 * don't), you might arrange things so that you could open it with
 * pathnames like "video:800x600/24bpp" in order to select the operating
 * mode.
 *
 * However, we have no support for this in the base system.
 */
static int
dev_lookup(struct inode *node, char *path, struct inode **node_store) {
    if (*path != '\0') {
        return -E_NOENT;
    }
    vop_ref_inc(node);
    *node_store = node;
    return 0;
}

/*
 * Function table for device inodes.
 */
static const struct inode_ops dev_node_ops = {
    .vop_magic = VOP_MAGIC,
    .vop_open = dev_open,
    .vop_close = dev_close,
    .vop_read = dev_read,
    .vop_write = dev_write,
    .vop_fstat = dev_fstat,
    .vop_ioctl = dev_ioctl,
    .vop_gettype = dev_gettype,
    .vop_tryseek = dev_tryseek,
    .vop_lookup = dev_lookup,
};

#define init_device(x)                                  \
    do {                                                \
        extern void dev_init_##x(void);                 \
        dev_init_##x();                                 \
    } while (0)

/* dev_init - Initialization functions for builtin vfs-level devices. */
void dev_init(void) {
    infof("Initializing devices.");
    init_device(stdin);
    init_device(stdout);
    init_device(disk0);
    infof("Devices initialized successfully.");
}
/* dev_create_inode - Create inode for a vfs-level device. */
struct inode *
dev_create_inode(void) {
    struct inode *node;
    if ((node = alloc_inode(device)) != NULL) {
        vop_init(node, &dev_node_ops, NULL);
    }
    return node;
}

