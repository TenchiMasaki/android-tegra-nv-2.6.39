#ifndef __LINUX_REGMAP_H
#define __LINUX_REGMAP_H

/*
 * Register map access API
 *
 * Copyright 2011 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/module.h>
#include <linux/bug.h>
#include <linux/err.h>

struct module;
struct device;
struct i2c_client;
struct irq_domain;
struct spi_device;
struct regmap;
struct regmap_range_cfg;

/* An enum of all the supported cache types */
enum regcache_type {
	REGCACHE_NONE,
	REGCACHE_RBTREE,
	REGCACHE_COMPRESSED,
	REGCACHE_FLAT,
};

/**
 * Default value for a register.  We use an array of structs rather
 * than a simple array as many modern devices have very sparse
 * register maps.
 *
 * @reg: Register address.
 * @def: Register default value.
 */
struct reg_default {
	unsigned int reg;
	unsigned int def;
};
#define CONFIG_REGMAP 1
#ifdef CONFIG_REGMAP

enum regmap_endian {
	/* Unspecified -> 0 -> Backwards compatible default */
	REGMAP_ENDIAN_DEFAULT = 0,
	REGMAP_ENDIAN_BIG,
	REGMAP_ENDIAN_LITTLE,
	REGMAP_ENDIAN_NATIVE,
};

/**
 * A register range, used for access related checks
 * (readable/writeable/volatile/precious checks)
 *
 * @range_min: address of first register
 * @range_max: address of last register
 */
struct regmap_range {
	unsigned int range_min;
	unsigned int range_max;
};

#define regmap_reg_range(low, high) { .range_min = low, .range_max = high, }

/*
 * A table of ranges including some yes ranges and some no ranges.
 * If a register belongs to a no_range, the corresponding check function
 * will return false. If a register belongs to a yes range, the corresponding
 * check function will return true. "no_ranges" are searched first.
 *
 * @yes_ranges : pointer to an array of regmap ranges used as "yes ranges"
 * @n_yes_ranges: size of the above array
 * @no_ranges: pointer to an array of regmap ranges used as "no ranges"
 * @n_no_ranges: size of the above array
 */
struct regmap_access_table {
	const struct regmap_range *yes_ranges;
	unsigned int n_yes_ranges;
	const struct regmap_range *no_ranges;
	unsigned int n_no_ranges;
};

typedef void (*regmap_lock)(void *);
typedef void (*regmap_unlock)(void *);
struct regmap_config {
	const char *name;

	int reg_bits;
	int reg_stride;
	int pad_bits;
	int val_bits;
	bool (*writeable_reg)(struct device *dev, unsigned int reg);
	bool (*readable_reg)(struct device *dev, unsigned int reg);
	bool (*volatile_reg)(struct device *dev, unsigned int reg);
	bool (*precious_reg)(struct device *dev, unsigned int reg);
	int (*reg_volatile_set)(struct device *dev, unsigned int reg,
				bool is_volatile);
	regmap_lock lock;
	regmap_unlock unlock;
	void *lock_arg;

	int (*reg_read)(void *context, unsigned int reg, unsigned int *val);
	int (*reg_write)(void *context, unsigned int reg, unsigned int val);

	bool fast_io;

	unsigned int max_register;
	const struct regmap_access_table *wr_table;
	const struct regmap_access_table *rd_table;
	const struct regmap_access_table *volatile_table;
	const struct regmap_access_table *precious_table;
	const struct reg_default *reg_defaults;
	unsigned int num_reg_defaults;
	enum regcache_type cache_type;
	const void *reg_defaults_raw;
	unsigned int num_reg_defaults_raw;

	u8 read_flag_mask;
	u8 write_flag_mask;

	bool use_single_rw;

	enum regmap_endian reg_format_endian;
	enum regmap_endian val_format_endian;

	const struct regmap_range_cfg *ranges;
	unsigned int num_ranges;
};

/**
 * Configuration for indirectly accessed or paged registers.
 * Registers, mapped to this virtual range, are accessed in two steps:
 *     1. page selector register update;
 *     2. access through data window registers.
 *
 * @name: Descriptive name for diagnostics
 *
 * @range_min: Address of the lowest register address in virtual range.
 * @range_max: Address of the highest register in virtual range.
 *
 * @page_sel_reg: Register with selector field.
 * @page_sel_mask: Bit shift for selector value.
 * @page_sel_shift: Bit mask for selector value.
 *
 * @window_start: Address of first (lowest) register in data window.
 * @window_len: Number of registers in data window.
 */
struct regmap_range_cfg {
	const char *name;

	/* Registers of virtual address range */
	unsigned int range_min;
	unsigned int range_max;

	/* Page selector for indirect addressing */
	unsigned int selector_reg;
	unsigned int selector_mask;
	int selector_shift;

	/* Data window (per each page) */
	unsigned int window_start;
	unsigned int window_len;
};

struct regmap_async;
typedef int (*regmap_hw_write)(struct device *dev, const void *data,
			       size_t count);
typedef int (*regmap_hw_gather_write)(struct device *dev,
				      const void *reg, size_t reg_len,
				      const void *val, size_t val_len);
typedef int (*regmap_hw_async_write)(void *context,
				     const void *reg, size_t reg_len,
				     const void *val, size_t val_len,
				     struct regmap_async *async);
typedef int (*regmap_hw_read)(struct device *dev,
			      const void *reg_buf, size_t reg_size,
			      void *val_buf, size_t val_size);
typedef struct regmap_async *(*regmap_hw_async_alloc)(void);
typedef void (*regmap_hw_free_context)(void *context);

/**
 * Description of a hardware bus for the register map infrastructure.
 *
 * @list: Internal use.
 * @type: Bus type, used to identify bus to be used for a device.
 * @write: Write operation.
 * @gather_write: Write operation with split register/value, return -ENOTSUPP
 *                if not implemented  on a given device.
 * @read: Read operation.  Data is returned in the buffer used to transmit
 *         data.
 * @owner: Module with the bus implementation, used to pin the implementation
 *         in memory.
 * @read_flag_mask: Mask to be set in the top byte of the register when doing
 *                  a read.
 */
struct regmap_bus {
	bool fast_io;
	struct list_head list;
	struct bus_type *type;
	regmap_hw_write write;
	regmap_hw_gather_write gather_write;
	regmap_hw_async_write async_write;
	regmap_hw_read read;
	struct module *owner;
	regmap_hw_free_context free_context;
	regmap_hw_async_alloc async_alloc;
	u8 read_flag_mask;
	enum regmap_endian reg_format_endian_default;
	enum regmap_endian val_format_endian_default;
};

struct regmap *regmap_init(struct device *dev,
			   const struct regmap_bus *bus,
			   void *bus_context,
			   const struct regmap_config *config);
struct regmap *regmap_init_i2c(struct i2c_client *i2c,
			       const struct regmap_config *config);
struct regmap *regmap_init_spi(struct spi_device *dev,
			       const struct regmap_config *config);
struct regmap *regmap_init_mmio_clk(struct device *dev, const char *clk_id,
				    void __iomem *regs,
				    const struct regmap_config *config);

struct regmap *devm_regmap_init(struct device *dev,
				const struct regmap_bus *bus,
				void *bus_context,
				const struct regmap_config *config);
struct regmap *devm_regmap_init_i2c(struct i2c_client *i2c,
				    const struct regmap_config *config);
struct regmap *devm_regmap_init_spi(struct spi_device *dev,
				    const struct regmap_config *config);
struct regmap *devm_regmap_init_mmio_clk(struct device *dev, const char *clk_id,
					 void __iomem *regs,
					 const struct regmap_config *config);

/**
 * regmap_init_mmio(): Initialise register map
 *
 * @dev: Device that will be interacted with
 * @regs: Pointer to memory-mapped IO region
 * @config: Configuration for register map
 *
 * The return value will be an ERR_PTR() on error or a valid pointer to
 * a struct regmap.
 */
static inline struct regmap *regmap_init_mmio(struct device *dev,
					void __iomem *regs,
					const struct regmap_config *config)
{
	return regmap_init_mmio_clk(dev, NULL, regs, config);
}

/**
 * devm_regmap_init_mmio(): Initialise managed register map
 *
 * @dev: Device that will be interacted with
 * @regs: Pointer to memory-mapped IO region
 * @config: Configuration for register map
 *
 * The return value will be an ERR_PTR() on error or a valid pointer
 * to a struct regmap.  The regmap will be automatically freed by the
 * device management code.
 */
static inline struct regmap *devm_regmap_init_mmio(struct device *dev,
					void __iomem *regs,
					const struct regmap_config *config)
{
	return devm_regmap_init_mmio_clk(dev, NULL, regs, config);
}

void regmap_exit(struct regmap *map);
int regmap_reinit_cache(struct regmap *map,
			const struct regmap_config *config);
struct regmap *dev_get_regmap(struct device *dev, const char *name);
int regmap_write(struct regmap *map, unsigned int reg, unsigned int val);
int regmap_raw_write(struct regmap *map, unsigned int reg,
		     const void *val, size_t val_len);
int regmap_bulk_write(struct regmap *map, unsigned int reg, const void *val,
			size_t val_count);
int regmap_raw_write_async(struct regmap *map, unsigned int reg,
			   const void *val, size_t val_len);
int regmap_read(struct regmap *map, unsigned int reg, unsigned int *val);
int regmap_raw_read(struct regmap *map, unsigned int reg,
		    void *val, size_t val_len);
int regmap_bulk_read(struct regmap *map, unsigned int reg, void *val,
		     size_t val_count);
int regmap_update_bits(struct regmap *map, unsigned int reg,
		       unsigned int mask, unsigned int val);
int regmap_update_bits_lazy(struct regmap *map, unsigned int reg,
		       unsigned int mask, unsigned int val);
int regmap_update_bits_check(struct regmap *map, unsigned int reg,
			     unsigned int mask, unsigned int val,
			     bool *change);
int regmap_get_val_bytes(struct regmap *map);
int regmap_async_complete(struct regmap *map);
bool regmap_can_raw_write(struct regmap *map);

int regcache_sync(struct regmap *map);
int regcache_sync_region(struct regmap *map, unsigned int min,
			 unsigned int max);
void regcache_cache_only(struct regmap *map, bool enable);
void regcache_cache_bypass(struct regmap *map, bool enable);
void regcache_mark_dirty(struct regmap *map);
int regcache_volatile_set(struct regmap *map, unsigned int reg,
			  bool is_volatile);

int regmap_register_patch(struct regmap *map, const struct reg_default *regs,
			  int num_regs);

static inline bool regmap_reg_in_range(unsigned int reg,
				       const struct regmap_range *range)
{
	return reg >= range->range_min && reg <= range->range_max;
}

bool regmap_reg_in_ranges(unsigned int reg,
			  const struct regmap_range *ranges,
			  unsigned int nranges);

/**
 * Description of an IRQ for the generic regmap irq_chip.
 *
 * @reg_offset: Offset of the status/mask register within the bank
 * @mask:       Mask used to flag/control the register.
 * @type_reg_offset: Offset register for the irq type setting.
 * @type_rising_mask: Mask bit to configure RISING type irq.
 * @type_falling_mask: Mask bit to configure FALLING type irq.
 */
struct regmap_irq {
	unsigned int reg_offset;
	unsigned int mask;
	unsigned int type_reg_offset;
	unsigned int type_rising_mask;
	unsigned int type_falling_mask;
};

/**
 * Description of a generic regmap irq_chip.  This is not intended to
 * handle every possible interrupt controller, but it should handle a
 * substantial proportion of those that are found in the wild.
 *
 * @name:        Descriptive name for IRQ controller.
 *
 * @status_base: Base status register address.
 * @mask_base:   Base mask register address.
 * @ack_base:    Base ack address.  If zero then the chip is clear on read.
 * @wake_base:   Base address for wake enables.  If zero unsupported.
 * @type_base:   Base address for irq type.  If zero unsupported.
 * @irq_reg_stride:  Stride to use for chips where registers are not contiguous.
 * @runtime_pm:  Hold a runtime PM lock on the device when accessing it.
 * @type_invert: Invert the type flags.
 *
 * @num_regs:    Number of registers in each control bank.
 * @irqs:        Descriptors for individual IRQs.  Interrupt numbers are
 *               assigned based on the index in the array of the interrupt.
 * @num_irqs:    Number of descriptors.
 * @num_type_reg:    Number of type registers.
 * @type_reg_stride: Stride to use for chips where type registers are not
 *			contiguous.
 */
struct regmap_irq_chip {
	const char *name;

	unsigned int status_base;
	unsigned int mask_base;
	unsigned int ack_base;
	unsigned int wake_base;
	unsigned int type_base;
	unsigned int irq_reg_stride;
	unsigned int mask_invert;
	unsigned int wake_invert;
	bool runtime_pm;
	unsigned int type_invert;

	int num_regs;

	const struct regmap_irq *irqs;
	int num_irqs;

	int num_type_reg;
	unsigned int type_reg_stride;
};

struct regmap_irq_chip_data;

int regmap_add_irq_chip(struct regmap *map, int irq, int irq_flags,
			int irq_base, const struct regmap_irq_chip *chip,
			struct regmap_irq_chip_data **data);
void regmap_del_irq_chip(int irq, struct regmap_irq_chip_data *data);
void regmap_shutdown_irq_chip(struct regmap_irq_chip_data *d);
int regmap_irq_suspend_noirq(struct regmap_irq_chip_data *d);
int regmap_irq_resume(struct regmap_irq_chip_data *d);
int regmap_irq_chip_get_base(struct regmap_irq_chip_data *data);
int regmap_irq_get_virq(struct regmap_irq_chip_data *data, int irq);
struct irq_domain *regmap_irq_get_domain(struct regmap_irq_chip_data *data);
#else
/*
 * These stubs should only ever be called by generic code which has
 * regmap based facilities, if they ever get called at runtime
 * something is going wrong and something probably needs to select
 * REGMAP.
 */

static inline int regmap_write(struct regmap *map, unsigned int reg,
			       unsigned int val)
{
	WARN_ONCE(1, "regmap API is disabled");
	return -EINVAL;
}

static inline int regmap_raw_write(struct regmap *map, unsigned int reg,
				   const void *val, size_t val_len)
{
	WARN_ONCE(1, "regmap API is disabled");
	return -EINVAL;
}

static inline int regmap_raw_write_async(struct regmap *map, unsigned int reg,
					 const void *val, size_t val_len)
{
	WARN_ONCE(1, "regmap API is disabled");
	return -EINVAL;
}

static inline int regmap_bulk_write(struct regmap *map, unsigned int reg,
				    const void *val, size_t val_count)
{
	WARN_ONCE(1, "regmap API is disabled");
	return -EINVAL;
}

static inline int regmap_read(struct regmap *map, unsigned int reg,
			      unsigned int *val)
{
	WARN_ONCE(1, "regmap API is disabled");
	return -EINVAL;
}

static inline int regmap_raw_read(struct regmap *map, unsigned int reg,
				  void *val, size_t val_len)
{
	WARN_ONCE(1, "regmap API is disabled");
	return -EINVAL;
}

static inline int regmap_bulk_read(struct regmap *map, unsigned int reg,
				   void *val, size_t val_count)
{
	WARN_ONCE(1, "regmap API is disabled");
	return -EINVAL;
}

static inline int regmap_update_bits(struct regmap *map, unsigned int reg,
				     unsigned int mask, unsigned int val)
{
	WARN_ONCE(1, "regmap API is disabled");
	return -EINVAL;
}

static inline int regmap_update_bits_check(struct regmap *map,
					   unsigned int reg,
					   unsigned int mask, unsigned int val,
					   bool *change)
{
	WARN_ONCE(1, "regmap API is disabled");
	return -EINVAL;
}

static inline int regmap_get_val_bytes(struct regmap *map)
{
	WARN_ONCE(1, "regmap API is disabled");
	return -EINVAL;
}

static inline int regcache_sync(struct regmap *map)
{
	WARN_ONCE(1, "regmap API is disabled");
	return -EINVAL;
}

static inline int regcache_sync_region(struct regmap *map, unsigned int min,
				       unsigned int max)
{
	WARN_ONCE(1, "regmap API is disabled");
	return -EINVAL;
}

static inline void regcache_cache_only(struct regmap *map, bool enable)
{
	WARN_ONCE(1, "regmap API is disabled");
}

static inline void regcache_cache_bypass(struct regmap *map, bool enable)
{
	WARN_ONCE(1, "regmap API is disabled");
}

static inline void regcache_mark_dirty(struct regmap *map)
{
	WARN_ONCE(1, "regmap API is disabled");
}

static int regcache_volatile_set(struct regmap *map, unsigned int reg,
				 bool is_volatile)
{
	WARN_ONCE(1, "regmap API is disabled");
	return -EINVAL;
}

static inline void regmap_async_complete(struct regmap *map)
{
	WARN_ONCE(1, "regmap API is disabled");
}

static inline int regmap_register_patch(struct regmap *map,
					const struct reg_default *regs,
					int num_regs)
{
	WARN_ONCE(1, "regmap API is disabled");
	return -EINVAL;
}

static inline struct regmap *dev_get_regmap(struct device *dev,
					    const char *name)
{
	return NULL;
}
#endif
#endif
