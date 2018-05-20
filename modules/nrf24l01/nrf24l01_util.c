#include <linux/types.h>
#include <linux/kernel.h>

#include "nrf24l01_core.h"

u64 nrf24l01_addr_host_to_addr_nrf(struct nrf24l01_t* nrf, u64 addr, unsigned int addr_width)
{
	if(nrf->flags.addr_be)
	{
		addr = cpu_to_be64(addr);
		addr >>= 8 * (sizeof(addr) - addr_width);
	}
	else
	{
		addr = cpu_to_le64(addr);
	}
	return addr;
}

u64 nrf24l01_addr_nrf_to_addr_host(struct nrf24l01_t* nrf, u64 addr, unsigned int addr_width)
{
	if(nrf->flags.addr_be)
	{
		addr <<= 8 * (sizeof(addr) - addr_width);
		addr = be64_to_cpu(addr);
	}
	else
	{
		addr = le64_to_cpu(addr);
	}
	return addr;
}
