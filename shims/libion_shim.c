/*
 * libio2.so - minimal ION front-end for the chopin camera HAL.
 *
 * libmtkcam_imgbuf.so gets its DT_NEEDED repointed here (libion.so ->
 * libio2.so, same string length) so these symbols resolve to us first while
 * the real libion.so is still loaded for everything else.
 *
 * Why: helpMapPhyAddrIon() calls
 *	 ion_import(devfd, dmabufFd, &handle)     // three arguments
 * and the AOSP libion.so shipped here rejects that before reaching the
 * kernel, because its legacy probe encodes the wrong struct size:
 *
 *	libion probe:          ioctl(dev, 0xbff04906)   size=16368, dir=READ
 *	kernel ION_IOC_CUSTOM: 0xc0104906               size=16,    dir=RW
 *
 * The probe fails, libion latches "not legacy", and ion_import returns
 * -EINVAL without doing anything. helpMapPhyAddrIon then fails,
 * IIonImageBufferHeap::create() returns null, LscImgBuf::mImageBuffer is
 * null, and LscImgBuf::editTable() dies on the null dereference.
 *
 * The kernel is fine: dma-heap buffers from /dev/dma_heap/mtk_mm are exported
 * through ION's own dma_buf_ops, so ION_IOC_IMPORT accepts them.
 */
#include <fcntl.h>
#include <unistd.h>

extern void *dlsym(void *, const char *);
#define RTLD_NEXT ((void *)-1L)

#define ION_IOC_IMPORT_MTK 0xc0084905UL
#define ION_IOC_MAP_MTK    0xc0084902UL
#define ION_IOC_SHARE_MTK  0xc0084904UL
#define ION_IOC_FREE_MTK   0xc0044901UL
#define ION_IOC_CUSTOM_MTK 0xc0104906UL

typedef int ion_user_handle_t;

struct fd_data { ion_user_handle_t handle; int fd; };

static int g_fd = -2;

static long mtk_ioctl(int fd, unsigned long req, void *arg)
{
	register long x0 __asm__("x0") = fd;
	register long x1 __asm__("x1") = (long)req;
	register long x2 __asm__("x2") = (long)arg;
	register long x8 __asm__("x8") = 29;
	__asm__ volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
	return x0;
}

static int ion_dev(void)
{
	int fd;

	if (g_fd >= 0)
		return g_fd;
	if (g_fd != -2)
		return -1;
	fd = open("/dev/ion", O_RDONLY | O_CLOEXEC);
	g_fd = (fd >= 0) ? fd : -1;
	return g_fd;
}

int ion_open(void)
{
	int fd = ion_dev();
	int (*real)(void) = (int (*)(void))dlsym(RTLD_NEXT, "ion_open");

	/* Prefer our own handle so we know it stays valid. */
	if (fd >= 0)
		return fd;
	return real ? real() : -1;
}

int mt_ion_open(const char *name)
{
	(void)name;
	return ion_dev();
}

/* ion_import(device_fd, dmabuf_fd, ion_user_handle_t *out) */
int ion_import(int devfd, int dmabuf, ion_user_handle_t *out)
{
	struct fd_data arg;
	int fd = (devfd >= 0) ? devfd : ion_dev();

	if (fd < 0)
		return -9;              /* EBADF */

	arg.handle = 0;
	arg.fd = dmabuf;
	if (mtk_ioctl(fd, ION_IOC_IMPORT_MTK, &arg) != 0)
		return -22;
	if (out)
		*out = arg.handle;
	return 0;
}

int ion_custom_ioctl(int devfd, unsigned int cmd, unsigned long arg)
{
	struct { unsigned int c; unsigned long a; } cd;
	int fd = (devfd >= 0) ? devfd : ion_dev();

	if (fd < 0)
		return -9;
	cd.c = cmd;
	cd.a = arg;
	return (int)mtk_ioctl(fd, ION_IOC_CUSTOM_MTK, &cd);
}

int ion_free(int devfd, ion_user_handle_t handle)
{
	struct { ion_user_handle_t h; } arg;
	int fd = (devfd >= 0) ? devfd : ion_dev();

	if (fd < 0)
		return -9;
	arg.h = handle;
	return (int)mtk_ioctl(fd, ION_IOC_FREE_MTK, &arg);
}

int ion_share(int devfd, ion_user_handle_t handle, int *share_fd)
{
	struct fd_data arg;
	int fd = (devfd >= 0) ? devfd : ion_dev();

	if (fd < 0 || !share_fd)
		return -9;
	arg.handle = handle;
	arg.fd = -1;
	if (mtk_ioctl(fd, ION_IOC_SHARE_MTK, &arg) != 0)
		return -22;
	*share_fd = arg.fd;
	return 0;
}

/* Everything else keeps coming from the real library. */
int ion_close(int fd)
{
	int (*real)(int) = (int (*)(int))dlsym(RTLD_NEXT, "ion_close");
	if (fd >= 0 && fd == g_fd)
		return 0;               /* keep our shared fd open */
	return real ? real(fd) : 0;
}

/*
 * The CCU memory manager branches on this: legacy==0 sends it down a path
 * that fails and leaves CcuBuffer all-zero, so CcuDrvImp::boot() later
 * memsets a NULL pointer and aborts. MediaTek's kernel provides the classic
 * ION_IOC_ALLOC/IMPORT/MAP ioctls, so take the legacy path -- ion_alloc_fd()
 * below is backed by /dev/dma_heap/mtk_mm, whose buffers ION exports itself
 * and can therefore import.
 */
int ion_is_legacy(int fd)
{
	(void)fd;
	return 1;
}

int ion_is_using_modular_heaps(int fd)
{
	int (*real)(int) = (int (*)(int))dlsym(RTLD_NEXT, "ion_is_using_modular_heaps");
	return real ? real(fd) : 0;
}

int ion_map(int fd, ion_user_handle_t h, size_t len, int prot, int flags,
	    unsigned char **ptr, int *map_fd)
{
	int (*real)(int, ion_user_handle_t, size_t, int, int,
		    unsigned char **, int *) =
		(int (*)(int, ion_user_handle_t, size_t, int, int,
			 unsigned char **, int *))dlsym(RTLD_NEXT, "ion_map");
	if (!real)
		return -22;
	return real(ion_dev(), h, len, prot, flags, ptr, map_fd);
}

int ion_sync_fd(int fd, int handle_fd)
{
	int (*real)(int, int) = (int (*)(int, int))dlsym(RTLD_NEXT, "ion_sync_fd");
	if (!real)
		return -22;
	return real(ion_dev(), handle_fd);
}

int ion_query_heap_cnt(int fd, int *cnt)
{
	int (*real)(int, int *) = (int (*)(int, int *))dlsym(RTLD_NEXT, "ion_query_heap_cnt");
	if (!real)
		return -22;
	return real(ion_dev(), cnt);
}

int ion_query_get_heaps(int fd, int cnt, void *buf)
{
	int (*real)(int, int, void *) =
		(int (*)(int, int, void *))dlsym(RTLD_NEXT, "ion_query_get_heaps");
	if (!real)
		return -22;
	return real(ion_dev(), cnt, buf);
}

/* Allocation still goes through dma-heap and is then ION-importable. */
int ion_alloc_fd(int devfd, size_t len, size_t align, unsigned int flags, int *out_fd)
{
	struct { unsigned long len; unsigned int fd; unsigned int fd_flags;
		 unsigned long heap_flags; } d;
	int dh, ret;

	(void)devfd; (void)align; (void)flags;
	if (!out_fd)
		return -22;

	dh = open("/dev/dma_heap/mtk_mm", O_RDONLY | O_CLOEXEC);
	if (dh < 0)
		return -9;
	d.len = len;
	d.fd = 0;
	d.fd_flags = 0x80000;
	d.heap_flags = 0;
	ret = (int)mtk_ioctl(dh, 0xc0184800UL, &d);
	close(dh);
	if (ret < 0)
		return ret;
	*out_fd = (int)d.fd;
	return 0;
}


/*
 * ion_alloc_mm(device_fd, len, align, flags, ion_user_handle_t *handle)
 *
 * The CCU memory manager takes the legacy path and calls this before
 * ion_alloc_fd(). libion.so would do the same thing, but it refuses every
 * request after its legacy probe fails, so provide it here on top of the
 * same dma-heap source.
 */
int ion_alloc_mm(int devfd, size_t len, size_t align, unsigned int flags,
		 ion_user_handle_t *handle)
{
	struct fd_data arg;
	int fd = (devfd >= 0) ? devfd : ion_dev();
	int ret;

	if (fd < 0 || !handle)
		return -9;

	arg.handle = 0;
	arg.fd = -1;
	/* ION_IOC_ALLOC: 0xc0204900, struct ion_allocation_data (32 bytes) */
	{
		struct {
			unsigned long len;
			unsigned long align;
			unsigned int heap_id_mask;
			unsigned int flags;
			ion_user_handle_t handle;
		} a;
		a.len = len;
		a.align = align ? align : 4096;
		a.heap_id_mask = 1u << 10;   /* multimedia heap id 10 */
		a.flags = flags;
		a.handle = 0;
		ret = (int)mtk_ioctl(fd, 0xc0204900UL, &a);
		if (ret == 0)
			*handle = a.handle;
	}
	return ret;
}
