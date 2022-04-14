/* $Id: module.c,v 1.5 2008/12/11 12:18:17 ecd Exp $
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include <x49gp.h>

int
x49gp_modules_init(x49gp_t *x49gp)
{
	x49gp_module_t *module;
	int error;

#ifdef DEBUG_X49GP_MODULES
	printf("%s:%u:\n", __FUNCTION__, __LINE__);
#endif

	phys_ram_size = 0;

	list_for_each_entry(module, &x49gp->modules, list) {
		error = module->init(module);
		if (error) {
			return error;
		}
	}

	phys_ram_base = mmap(0, phys_ram_size, PROT_NONE, MAP_SHARED | MAP_ANON, -1, 0);
	if (phys_ram_base == (uint8_t *) -1) {
		fprintf(stderr, "%s: can't mmap %08x anonymous bytes\n",
			__FUNCTION__, phys_ram_size);
		exit(1);
	}

printf("%s: phys_ram_base: %p\n", __FUNCTION__, phys_ram_base);

	phys_ram_dirty = qemu_vmalloc(phys_ram_size >> TARGET_PAGE_BITS);
	memset(phys_ram_dirty, 0xff, phys_ram_size >> TARGET_PAGE_BITS);

#ifndef QEMU_OLD
        {
          ram_addr_t x49gp_ram_alloc(ram_addr_t size, uint8_t *base);
          x49gp_ram_alloc(phys_ram_size, phys_ram_base);
        }
#endif

	return 0;
}

int
x49gp_modules_exit(x49gp_t *x49gp)
{
	x49gp_module_t *module, *next;
	int error;

#ifdef DEBUG_X49GP_MODULES
	printf("%s:%u:\n", __FUNCTION__, __LINE__);
#endif

	list_for_each_entry_safe_reverse(module, next, &x49gp->modules, list) {
		error = module->exit(module);
		if (error) {
			return error;
		}
	}

	return 0;
}

int
x49gp_modules_reset(x49gp_t *x49gp, x49gp_reset_t reset)
{
	x49gp_module_t *module;
	int error;

#ifdef DEBUG_X49GP_MODULES
	printf("%s:%u:\n", __FUNCTION__, __LINE__);
#endif

	list_for_each_entry(module, &x49gp->modules, list) {
		error = module->reset(module, reset);
		if (error) {
			return error;
		}
	}

	return 0;
}

int
x49gp_modules_load(x49gp_t *x49gp, const char *filename)
{
	x49gp_module_t *module;
	GError *gerror = NULL;
	int error, result;

#ifdef DEBUG_X49GP_MODULES
	printf("%s:%u:\n", __FUNCTION__, __LINE__);
#endif

	x49gp->config = g_key_file_new();
	if (NULL == x49gp->config) {
		fprintf(stderr, "%s:%u: g_key_file_new: Out of memory\n",
			__FUNCTION__, __LINE__);
		return -ENOMEM;
	}

	if (! g_key_file_load_from_file(x49gp->config, filename, 0, &gerror)) {
		fprintf(stderr, "%s:%u: g_key_file_load_from_file: %s\n",
			__FUNCTION__, __LINE__, gerror->message);
		g_key_file_free(x49gp->config);
		return -EIO;
	}

	result = 0;

	list_for_each_entry(module, &x49gp->modules, list) {
		error = module->load(module, x49gp->config);
		if (error) {
			if (error == -EAGAIN) {
				result = -EAGAIN;
			} else {
				return error;
			}
		}
	}

{
	extern unsigned char *phys_ram_base;

printf("%s: phys_ram_base: %p\n", __FUNCTION__, phys_ram_base);
printf("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
	phys_ram_base[0],
	phys_ram_base[1],
	phys_ram_base[2],
	phys_ram_base[3],
	phys_ram_base[4],
	phys_ram_base[5],
	phys_ram_base[6],
	phys_ram_base[7]);
}

	return result;
}

int
x49gp_modules_save(x49gp_t *x49gp, const char *filename)
{
	x49gp_module_t *module;
	GError *gerror = NULL;
	gchar *data;
	gsize length;
	int error;
	int fd;

#ifdef DEBUG_X49GP_MODULES
	printf("%s:%u:\n", __FUNCTION__, __LINE__);
#endif

	list_for_each_entry(module, &x49gp->modules, list) {
		error = module->save(module, x49gp->config);
		if (error) {
			return error;
		}
	}

	data = g_key_file_to_data(x49gp->config, &length, &gerror);
	if (NULL == data) {
		fprintf(stderr, "%s:%u: g_key_file_to_data: %s\n",
			__FUNCTION__, __LINE__, gerror->message);
		return -ENOMEM;
	}

	fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0) {
		error = -errno;
		fprintf(stderr, "%s:%u: open %s: %s\n",
			__FUNCTION__, __LINE__, filename, strerror(errno));
		g_free(data);
		return error;
	}


	if (write(fd, data, length) != length) {
		error = -errno;
		fprintf(stderr, "%s:%u: write %s: %s\n",
			__FUNCTION__, __LINE__, filename, strerror(errno));
		close(fd);
		g_free(data);
		return error;
	}

	close(fd);
	g_free(data);

	return 0;
}

int
x49gp_module_register(x49gp_module_t *module)
{
	x49gp_t *x49gp = module->x49gp;

#ifdef DEBUG_X49GP_MODULES
	printf("%s:%u: %s\n", __FUNCTION__, __LINE__, module->name);
#endif

	list_add_tail(&module->list, &x49gp->modules);

	return 0;
}

int
x49gp_module_unregister(x49gp_module_t *module)
{
#ifdef DEBUG_X49GP_MODULES
	printf("%s:%u: %s\n", __FUNCTION__, __LINE__, module->name);
#endif

	list_del(&module->list);

	return 0;
}

char *
x49gp_module_get_filename(x49gp_module_t *module, GKeyFile *key,
			  const char *name)
{
	char *filename;
	char *basename;
	const char *home;
	char *path;

	filename = g_key_file_get_string(key, module->name, name, NULL);
	if (NULL == filename) {
		fprintf(stderr, "%s: %s:%u: key \"%s\" not found\n",
			module->name, __FUNCTION__, __LINE__, name);
		return NULL;
	}

	if (g_path_is_absolute(filename)) {
		return filename;
	}

	home = g_get_home_dir();

	basename = g_key_file_get_string(key, "x49gp", "basename", NULL);
	if (NULL == basename) {
		fprintf(stderr, "%s: %s:%u: key \"basename\" not found\n",
			"x49gp", __FUNCTION__, __LINE__);
		g_free(filename);
		return NULL;
	}

	path = g_build_filename(home, basename, filename, NULL);
	if (NULL == path) {
		fprintf(stderr, "%s: %s:%u: Out of memory\n",
			module->name, __FUNCTION__, __LINE__);
	}

	g_free(filename);
	g_free(basename);

	return path;
}

int
x49gp_module_get_int(x49gp_module_t *module, GKeyFile *key, const char *name,
		     int reset, int *valuep)
{
	uint32_t value;
	int error;

	error = x49gp_module_get_u32(module, key, name, reset, &value);
	if (0 == error) {
		*valuep = value;
	}
	return error;
}

int
x49gp_module_set_int(x49gp_module_t *module, GKeyFile *key,
		     const char *name, int value)
{
	char data[16];

	snprintf(data, sizeof(data), "%d", value);

	g_key_file_set_value(key, module->name, name, data);

	return 0;
}

int
x49gp_module_get_uint(x49gp_module_t *module, GKeyFile *key, const char *name,
		      unsigned int reset, unsigned int *valuep)
{
	return x49gp_module_get_u32(module, key, name, reset, valuep);
}

int
x49gp_module_set_uint(x49gp_module_t *module, GKeyFile *key,
		      const char *name, unsigned int value)
{
	char data[16];

	snprintf(data, sizeof(data), "%u", value);

	g_key_file_set_value(key, module->name, name, data);

	return 0;
}

int
x49gp_module_get_u32(x49gp_module_t *module, GKeyFile *key,
		     const char *name, uint32_t reset, uint32_t *valuep)
{
	GError *gerror = NULL;
	char *data, *end;
	uint32_t value;

	data = g_key_file_get_value(key, module->name, name, &gerror);
	if (NULL == data) {
		fprintf(stderr, "%s: %s:%u: key \"%s\" not found\n",
			module->name, __FUNCTION__, __LINE__, name);
		*valuep = reset;
		return -EAGAIN;
	}

	value = strtoul(data, &end, 0);
	if ((end == data) || (*end != '\0')) {
		*valuep = reset;
		g_free(data);
		return -EAGAIN;
	}

	*valuep = value;

	g_free(data);
	return 0;
}

int
x49gp_module_set_u32(x49gp_module_t *module, GKeyFile *key,
		     const char *name, uint32_t value)
{
	char data[16];

	snprintf(data, sizeof(data), "0x%08x", value);

	g_key_file_set_value(key, module->name, name, data);

	return 0;
}

int
x49gp_module_set_u64(x49gp_module_t *module, GKeyFile *key,
		     const char *name, uint64_t value)
{
	char data[32];

	snprintf(data, sizeof(data), "0x%016" PRIx64 "", value);

	g_key_file_set_value(key, module->name, name, data);

	return 0;
}

int
x49gp_module_get_u64(x49gp_module_t *module, GKeyFile *key,
		     const char *name, uint64_t reset, uint64_t *valuep)
{
	GError *gerror = NULL;
	char *data, *end;
	uint64_t value;

	data = g_key_file_get_value(key, module->name, name, &gerror);
	if (NULL == data) {
		fprintf(stderr, "%s: %s:%u: key \"%s\" not found\n",
			module->name, __FUNCTION__, __LINE__, name);
		*valuep = reset;
		return -EAGAIN;
	}

	value = strtoull(data, &end, 0);
	if ((end == data) || (*end != '\0')) {
		*valuep = reset;
		g_free(data);
		return -EAGAIN;
	}

	*valuep = value;

	g_free(data);
	return 0;
}

int
x49gp_module_get_string(x49gp_module_t *module, GKeyFile *key,
			const char *name, char *reset, char **valuep)
{
	GError *gerror = NULL;
	char *data;

	data = g_key_file_get_value(key, module->name, name, &gerror);
	if (NULL == data) {
		fprintf(stderr, "%s: %s:%u: key \"%s\" not found\n",
			module->name, __FUNCTION__, __LINE__, name);
		*valuep = strdup(reset);
		return -EAGAIN;
	}

	*valuep = data;
	return 0;
}

int
x49gp_module_init(x49gp_t *x49gp, const char *name,
		  int (*init)(x49gp_module_t *),
		  int (*exit)(x49gp_module_t *),
		  int (*reset)(x49gp_module_t *, x49gp_reset_t),
		  int (*load)(x49gp_module_t *, GKeyFile *),
		  int (*save)(x49gp_module_t *, GKeyFile *),
		  void *user_data, x49gp_module_t **modulep)
{
	x49gp_module_t *module;

	module = malloc(sizeof(x49gp_module_t));
	if (NULL == module) {
		fprintf(stderr, "%s: %s:%u: Out of memory\n",
			name, __FUNCTION__, __LINE__);
		return -1;
	}
	memset(module, 0, sizeof(x49gp_module_t));

	module->name = name;

	module->init = init;
	module->exit = exit;
	module->reset = reset;
	module->load = load;
	module->save = save;

	module->user_data = user_data;

//	module->mutex = g_mutex_new();
	module->x49gp = x49gp;

	*modulep = module;
	return 0;
}
