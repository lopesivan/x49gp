/* $Id: s3c2410_sram.c,v 1.8 2008/12/11 12:18:17 ecd Exp $
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include <x49gp.h>
#include <s3c2410.h>
#include <byteorder.h>

typedef struct {
	void		*data;
	int		fd;
	uint32_t	offset;
	size_t		size;
} filemap_t;

static int
s3c2410_sram_load(x49gp_module_t *module, GKeyFile *key)
{
	filemap_t *filemap = module->user_data;
	char *filename;
	int error;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	filename = x49gp_module_get_filename(module, key, "filename");
	if (NULL == filename) {
		fprintf(stderr, "%s: %s:%u: key \"filename\" not found\n",
			module->name, __FUNCTION__, __LINE__);
		return -ENOENT;
	}

	filemap->fd = open(filename, O_RDWR);
	if (filemap->fd < 0) {
		error = -errno;
		fprintf(stderr, "%s: %s:%u: open %s: %s\n",
			module->name, __FUNCTION__, __LINE__,
			filename, strerror(errno));
		g_free(filename);
		return error;
	}

	filemap->data = mmap(phys_ram_base + filemap->offset, S3C2410_SRAM_SIZE,
			     PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED,
			     filemap->fd, 0);
	if (filemap->data == (void *) -1) {
		error = -errno;
		fprintf(stderr, "%s: %s:%u: mmap %s: %s\n",
			module->name, __FUNCTION__, __LINE__,
			filename, strerror(errno));
		g_free(filename);
		close(filemap->fd);
		filemap->fd = -1;
		return error;
	}
	filemap->size = S3C2410_SRAM_SIZE;

	g_free(filename);

	x49gp_schedule_lcd_update(module->x49gp);
	return 0;
}

static int
s3c2410_sram_save(x49gp_module_t *module, GKeyFile *key)
{
	filemap_t *filemap = module->user_data;
	int error;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	error = msync(filemap->data, filemap->size, MS_ASYNC);
	if (error) {
		fprintf(stderr, "%s:%u: msync: %s\n",
			__FUNCTION__, __LINE__, strerror(errno));
		return error;
	}

	error = fsync(filemap->fd);
	if (error) {
		fprintf(stderr, "%s:%u: fsync: %s\n",
			__FUNCTION__, __LINE__, strerror(errno));
		return error;
	}

	return 0;
}

static int
s3c2410_sram_reset(x49gp_module_t *module, x49gp_reset_t reset)
{
#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	return 0;
}

static int
s3c2410_sram_init(x49gp_module_t *module)
{
	filemap_t *filemap;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	filemap = malloc(sizeof(filemap_t));
	if (NULL == filemap) {
		fprintf(stderr, "%s:%u: Out of memory\n",
			__FUNCTION__, __LINE__);
		return -ENOMEM;
	}

	filemap->size = 0;
	filemap->fd = -1;

	module->user_data = filemap;

	filemap->data = (void *) -1;
	filemap->offset = phys_ram_size;
	phys_ram_size += S3C2410_SRAM_SIZE;

	cpu_register_physical_memory(S3C2410_SRAM_BASE, S3C2410_SRAM_SIZE,
				     filemap->offset | IO_MEM_RAM);

	return 0;
}

static int
s3c2410_sram_exit(x49gp_module_t *module)
{
	filemap_t *filemap;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	if (module->user_data) {
		filemap = module->user_data;

		if (filemap->data != (void *) -1) {
			munmap(filemap->data, filemap->size);
		}
		if (filemap->fd >= 0) {
			close(filemap->fd);
		}

		free(filemap);
	}

	x49gp_module_unregister(module);
	free(module);

	return 0;
}

int
x49gp_s3c2410_sram_init(x49gp_t *x49gp)
{
	x49gp_module_t *module;

	if (x49gp_module_init(x49gp, "s3c2410-sram",
			      s3c2410_sram_init,
			      s3c2410_sram_exit,
			      s3c2410_sram_reset,
			      s3c2410_sram_load,
			      s3c2410_sram_save,
			      NULL, &module)) {
		return -1;
	}

	return x49gp_module_register(module);
}
