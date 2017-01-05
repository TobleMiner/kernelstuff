#include <linux/errno.h>
#include <linux/regmap.h>

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
		value &= *reg->mask;

	return regmap_write_bits(reg->regmap, reg->reg, *reg->mask, value);
}

static int partreg_custom_read(struct partreg* reg, unsigned int* value, unsigned int maxlen)
{
	unsigned int len = reg->len;
	if(reg->len_func != NULL)
		len = reg->len_func(reg->ctx, reg->reg);
	len = min(len, maxlen);		
	return reg->reg_read(reg->ctx, reg->reg, value, len);
}

static int partreg_custom_write(struct partreg* reg, unsigned int* value, unsigned int maxlen)
{
	// Don't perform shifting/masking as we don't need it
	unsigned int len = reg->len;
	if(reg->len_func != NULL)
		len = reg->len_func(reg->ctx, reg->reg);
	len = min(len, maxlen);		
	return reg->reg_write(reg->ctx, reg->reg, value, len);
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
