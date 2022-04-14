/* $Id: s3c2410_nand.c,v 1.4 2008/12/11 12:18:17 ecd Exp $
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


typedef struct {
	uint32_t			nfconf;
	uint32_t			nfcmd;
	uint32_t			nfaddr;
	uint32_t			nfdata;
	uint32_t			nfstat;
	uint32_t			nfecc;

	unsigned int		nr_regs;
	s3c2410_offset_t	*regs;
} s3c2410_nand_t;


static int
s3c2410_nand_data_init(s3c2410_nand_t *nand)
{
	s3c2410_offset_t regs[] = {
		S3C2410_OFFSET(NAND, NFCONF, 0x00000000, nand->nfconf),
		S3C2410_OFFSET(NAND, NFCMD, 0x00000000, nand->nfcmd),
		S3C2410_OFFSET(NAND, NFADDR, 0x00000000, nand->nfaddr),
		S3C2410_OFFSET(NAND, NFDATA, 0x00000000, nand->nfdata),
		S3C2410_OFFSET(NAND, NFSTAT, 0x00000000, nand->nfstat),
		S3C2410_OFFSET(NAND, NFECC, 0x00000000, nand->nfecc),
	};

	memset(nand, 0, sizeof(s3c2410_nand_t));

	nand->regs = malloc(sizeof(regs));
	if (NULL == nand->regs) {
		fprintf(stderr, "%s:%u: Out of memory\n",
			__FUNCTION__, __LINE__);
		return -ENOMEM;
	}

	memcpy(nand->regs, regs, sizeof(regs));
	nand->nr_regs = sizeof(regs) / sizeof(regs[0]);

	return 0;
}

uint32_t
s3c2410_nand_read(void *opaque, target_phys_addr_t offset)
{
	s3c2410_nand_t *nand = opaque;
	s3c2410_offset_t *reg;

#ifdef QEMU_OLD
	offset -= S3C2410_NAND_BASE;
#endif
	if (! S3C2410_OFFSET_OK(nand, offset)) {
		return ~(0);
	}

	reg = S3C2410_OFFSET_ENTRY(nand, offset);

#ifdef DEBUG_S3C2410_NAND
	printf("read  %s [%08x] %s [%08x] data %08x\n",
		"s3c2410-nand", S3C2410_NAND_BASE,
		reg->name, offset, *(reg->datap));
#endif

	return *(reg->datap);
}

void
s3c2410_nand_write(void *opaque, target_phys_addr_t offset, uint32_t data)
{
	s3c2410_nand_t *nand = opaque;
	s3c2410_offset_t *reg;

#ifdef QEMU_OLD
	offset -= S3C2410_NAND_BASE;
#endif
	if (! S3C2410_OFFSET_OK(nand, offset)) {
		return;
	}

	reg = S3C2410_OFFSET_ENTRY(nand, offset);

#ifdef DEBUG_S3C2410_NAND
	printf("write %s [%08x] %s [%08x] data %08x\n",
		"s3c2410-nand", S3C2410_NAND_BASE,
		reg->name, offset, data);
#endif

	*(reg->datap) = data;
}


static int
s3c2410_nand_load(x49gp_module_t *module, GKeyFile *key)
{
	s3c2410_nand_t *nand = module->user_data;
	s3c2410_offset_t *reg;
	int error = 0;
	int i;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	for (i = 0; i < nand->nr_regs; i++) {
		reg = &nand->regs[i];

		if (NULL == reg->name)
			continue;

		if (x49gp_module_get_u32(module, key, reg->name,
					 reg->reset, reg->datap))
			error = -EAGAIN;
	}

	return error;
}

static int
s3c2410_nand_save(x49gp_module_t *module, GKeyFile *key)
{
	s3c2410_nand_t *nand = module->user_data;
	s3c2410_offset_t *reg;
	int i;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	for (i = 0; i < nand->nr_regs; i++) {
		reg = &nand->regs[i];

		if (NULL == reg->name)
			continue;

		x49gp_module_set_u32(module, key, reg->name, *(reg->datap));
	}

	return 0;
}

static int
s3c2410_nand_reset(x49gp_module_t *module, x49gp_reset_t reset)
{
	s3c2410_nand_t *nand = module->user_data;
	s3c2410_offset_t *reg;
	int i;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	for (i = 0; i < nand->nr_regs; i++) {
		reg = &nand->regs[i];

		if (NULL == reg->name)
			continue;

		*(reg->datap) = reg->reset;
	}

	return 0;
}

static CPUReadMemoryFunc *s3c2410_nand_readfn[] =
{
	s3c2410_nand_read,
	s3c2410_nand_read,
	s3c2410_nand_read
};

static CPUWriteMemoryFunc *s3c2410_nand_writefn[] =
{
	s3c2410_nand_write,
	s3c2410_nand_write,
	s3c2410_nand_write
};

static int
s3c2410_nand_init(x49gp_module_t *module)
{
	s3c2410_nand_t *nand;
	int iotype;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	nand = malloc(sizeof(s3c2410_nand_t));
	if (NULL == nand) {
		fprintf(stderr, "%s:%u: Out of memory\n",
			__FUNCTION__, __LINE__);
		return -ENOMEM;
	}
	if (s3c2410_nand_data_init(nand)) {
		free(nand);
		return -ENOMEM;
	}

	module->user_data = nand;

#ifdef QEMU_OLD
	iotype = cpu_register_io_memory(0, s3c2410_nand_readfn,
					s3c2410_nand_writefn, nand);
#else
	iotype = cpu_register_io_memory(s3c2410_nand_readfn,
					s3c2410_nand_writefn, nand);
#endif
printf("%s: iotype %08x\n", __FUNCTION__, iotype);
	cpu_register_physical_memory(S3C2410_NAND_BASE, S3C2410_MAP_SIZE, iotype);
	return 0;
}

static int
s3c2410_nand_exit(x49gp_module_t *module)
{
	s3c2410_nand_t *nand;

#ifdef DEBUG_X49GP_MODULES
	printf("%s: %s:%u\n", module->name, __FUNCTION__, __LINE__);
#endif

	if (module->user_data) {
		nand = module->user_data;
		if (nand->regs)
			free(nand->regs);
		free(nand);
	}

	x49gp_module_unregister(module);
	free(module);

	return 0;
}

int
x49gp_s3c2410_nand_init(x49gp_t *x49gp)
{
	x49gp_module_t *module;

	if (x49gp_module_init(x49gp, "s3c2410-nand",
			      s3c2410_nand_init,
			      s3c2410_nand_exit,
			      s3c2410_nand_reset,
			      s3c2410_nand_load,
			      s3c2410_nand_save,
			      NULL, &module)) {
		return -1;
	}

	return x49gp_module_register(module);
}
