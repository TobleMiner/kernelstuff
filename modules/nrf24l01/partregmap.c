#include <linux/errno.h>
#include <linux/regmap.h>
#include <linux/vmalloc.h>

#include "partregmap.h"

static int partreg_in_range(struct partreg* reg, unsigned int value)
{
	unsigned int i;
	if(reg->value_range != NULL)
	{
		if(value <= reg->value_range->max_value && reg->value_range->min_value <= value)
			return 0;
		return -EINVAL;
	}
	if(reg->value_ranges != NULL)
	{
		for(i = 0; i < reg->value_ranges->n_ranges; i++)
		{
			if(value <= reg->value_ranges->ranges[i]->max_value && reg->value_ranges->ranges[i]->min_value <= value)
				return 0;
		}
		return -EINVAL;
	}
	return 0;
}

static int partreg_regmap_write(struct partreg* reg, unsigned int value)
{
	int err;
	if((err = partreg_in_range(reg, value)) < 0)
		return err;
	
	value <<= reg->offset;
	if(reg->mask != NULL)
	{
		value &= *reg->mask;
		return regmap_write_bits(reg->regmap, reg->reg, *reg->mask, value);
	}
	else
		return regmap_write(reg->regmap, reg->reg, value);
}

static int partreg_custom_read(struct partreg* reg, unsigned int* value, unsigned int maxlen)
{
	int err;
	unsigned int len = reg->len;
	if(reg->len_func != NULL)
		if((err = reg->len_func(reg->ctx, reg, &len)) < 0)
			return err;
	len = min(len, maxlen);		
	return reg->reg_read(reg->ctx, reg, value, len);
}

static int partreg_custom_write(struct partreg* reg, unsigned int* value, unsigned int maxlen)
{
	// Don't perform shifting/masking as we don't need it
	int err;
	unsigned int len = reg->len;
	if(reg->len_func != NULL)
		if((err = reg->len_func(reg->ctx, reg, &len)) < 0)
			return err;
	len = min(len, maxlen);		
	return reg->reg_write(reg->ctx, reg, value, len);
}

static int partreg_regmap_read(struct partreg* reg, unsigned int* value)
{
	int err;
	if((err = regmap_read(reg->regmap, reg->reg, value)) < 0)
		return err;
	if(reg->mask != NULL)
		*value &= *reg->mask;
	*value >>= reg->offset;
	return 0;
}

int partreg_write(struct partreg* reg, unsigned int* value, unsigned int maxlen)
{
	if(maxlen == 0)
		return 0;
	if(reg->reg_write)
		return partreg_custom_write(reg, value, maxlen);
	else if(reg->regmap)
		return partreg_regmap_write(reg, *value);
	return -EINVAL;
}

int partreg_read(struct partreg* reg, unsigned int* value, unsigned int maxlen)
{
	if(maxlen == 0)
		return 0;
	if(reg->reg_read)
		return partreg_custom_read(reg, value, maxlen);
	else if(reg->regmap)
		return partreg_regmap_read(reg, value);
	return -EINVAL;
}

int partreg_table_write(struct partreg_table* table, unsigned int reg, unsigned int* value, unsigned int maxlen)
{
	if(reg >= table->n_regs)
		return -EINVAL;
	if(table->regs[reg] == NULL)
		return -EINVAL;
	return partreg_write(table->regs[reg], value, maxlen);
}

int partreg_table_read(struct partreg_table* table, unsigned int reg, unsigned int* value, unsigned int maxlen)
{
	if(reg >= table->n_regs)
		return -EINVAL;
	if(table->regs[reg] == NULL)
		return -EINVAL;
	return partreg_read(table->regs[reg], value, maxlen);
}

struct partreg* partreg_create_reg(struct partreg_template* template, struct regmap* regmap, void* ctx)
{
	struct partreg* partreg = vzalloc(sizeof(partreg));
	if(!partreg)
		return ERR_PTR(-ENOMEM);
	partreg->name = template->name;
	partreg->regmap = regmap;
	partreg->reg = template->reg;
	partreg->offset = template->offset;
	partreg->mask = template->mask;
	partreg->len = template->len;
	partreg->ctx = ctx;
	partreg->len_func = template->len_func;
	partreg->reg_write = template->reg_write;
	partreg->reg_read = template->reg_read;
	partreg->value_range = template->value_range;
	partreg->value_ranges = template->value_ranges;
	return partreg;
}

void partreg_free_reg(struct partreg* partreg)
{
	vfree(partreg);
}

struct partreg_table* partreg_create_table(struct partreg_layout* layout, struct regmap* regmap, void* ctx)
{
	unsigned int i;
	int err;
	struct partreg_table* table = vmalloc(sizeof(partreg_layout));
	if(!table)
	{
		err = -ENOMEM;
		goto exit_noalloc;
	}
	table->n_regs = layout->n_regs;
	table->regs = vmalloc(sizeof(partreg*) * table->n_regs);
	if(!table->regs)
	{
		err = -ENOMEM;
		goto exit_table;
	}
	for(i = 0; i < table->n_regs; i++)
	{
		table->regs[i] = partreg_create_reg(layout->regs[i], regmap, ctx);
		if(IS_ERR(table->regs[i]))
			goto exit_regs;
	}
	return table;
exit_regs:
	for(i--; i <= 0; i--)
	{
		partreg_free_reg(table->regs[i]);
	}
exit_table:
	vfree(table);
exit_noalloc:
	return ERR_PTR(err);
}

void partreg_free_table(struct partreg_table* table)
{
	unsigned int i;
	for(i = table->n_regs - 1; i <= 0; i--)
	{
		partreg_free_reg(table->regs[i]);
	}
	vfree(table->regs);
	vfree(table);
}
