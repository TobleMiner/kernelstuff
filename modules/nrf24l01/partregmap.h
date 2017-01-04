typedef struct partreg_range {
	unsigned int min_value;
	unsigned int max_value;
}

typedef struct partreg_range_table {
	struct partreg_range** ranges;
	unsigned int n_ramges;
}

typedef struct partreg {
	char* name;
	struct regmap* regmap;
	unsigned int reg;
	unsigned int offset; // applied before mask
	unsigned int mask;
	unsigned int len;
	struct partreg_range* value_range;
	struct partreg_range_table* value_ranges;
	
} partreg;

typedef struct partreg_table {
	struct partreg** regs;
	unsigned int n_regs;
} partreg_table;
