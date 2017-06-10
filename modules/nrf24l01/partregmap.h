#ifndef _LINUX_PARTREGMAP_H_
#define _LINUX_PARTREGMAP_H_

typedef struct partreg_range {
	unsigned int min_value;
	unsigned int max_value;
} partreg_range;

#define partreg_reg_range(low, high) { .min_value = low, .max_value = high }

typedef struct partreg_range_table {
	struct partreg_range** ranges;
	unsigned int n_ranges;
} partreg_range_table;

typedef struct partreg {
	char* name;
	struct regmap* regmap; // optional
	unsigned int reg;
	unsigned int offset; // applied before mask
	unsigned int* mask;
	unsigned int len;
	void* ctx;
	int (* len_func)(void* ctx, struct partreg* reg, unsigned int* len);
	int (* reg_write)(void* ctx, struct partreg* reg, unsigned int* data, unsigned int len);
	int (* reg_read)(void* ctx, struct partreg* reg, unsigned int* data, unsigned int len);
	struct partreg_range* value_range;
	struct partreg_range_table* value_ranges;
} partreg;

typedef struct partreg_table {
	struct partreg** regs;
	unsigned int n_regs;
} partreg_table;

typedef struct partreg_template {
	char* name;
	unsigned int reg;
	unsigned int offset;
	unsigned int* mask;
	unsigned int len;
	int (* len_func)(void* ctx, struct partreg* reg, unsigned int* len);
	int (* reg_write)(void* ctx, struct partreg* reg, unsigned int* data, unsigned int len);
	int (* reg_read)(void* ctx, struct partreg* reg, unsigned int* data, unsigned int len);
	struct partreg_range* value_range;
	struct partreg_range_table* value_ranges;	
} partreg_template;

typedef struct partreg_layout {
	struct partreg_template** regs;
	unsigned int n_regs;
} partreg_layout;

int partreg_write(struct partreg* reg, unsigned int* value, unsigned int maxlen);
int partreg_read(struct partreg* reg, unsigned int* value, unsigned int maxlen);
int partreg_table_write(struct partreg_table* table, unsigned int reg, unsigned int* value, unsigned int maxlen);
int partreg_table_read(struct partreg_table* table, unsigned int reg, unsigned int* value, unsigned int maxlen);
struct partreg* partreg_create_reg(struct partreg_template* template, struct regmap* regmap, void* ctx);
void partreg_free_reg(struct partreg* partreg);
struct partreg_table* partreg_create_table(struct partreg_layout* layout, struct regmap* regmap, void* ctx);
void partreg_free_table(struct partreg_table* table);

#endif
