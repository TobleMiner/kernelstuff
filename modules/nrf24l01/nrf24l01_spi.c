#include <linux/spi/spi.h>
#include <linux/vmalloc.h>

#include "nrf24l01_core.h"
#include "nrf24l01_spi.h"
#include "nrf24l01_reg.h"
#include "nrf24l01_cmd.h"

static char nrf24l01_reg_masks[] = {
	NRF24L01_REG_MASK_CONFIG,
	NRF24L01_REG_MASK_EN_AA,
	NRF24L01_REG_MASK_EN_RXADDR,
	NRF24L01_REG_MASK_SETUP_AW,
	NRF24L01_REG_MASK_SETUP_RETR,
	NRF24L01_REG_MASK_RF_CH,
	NRF24L01_REG_MASK_RF_SETUP,
	NRF24L01_REG_MASK_STATUS,
	NRF24L01_REG_MASK_OBSERVE_TX,
	NRF24L01_REG_MASK_CD,
	NRF24L01_REG_MASK_RX_ADDR_PO,
	NRF24L01_REG_MASK_RX_ADDR_P1,
	NRF24L01_REG_MASK_RX_ADDR_P2,
	NRF24L01_REG_MASK_RX_ADDR_P3,
	NRF24L01_REG_MASK_RX_ADDR_P4,
	NRF24L01_REG_MASK_RX_ADDR_P5,
	NRF24L01_REG_MASK_TX_ADDR,
	NRF24L01_REG_MASK_RX_PW_P0,
	NRF24L01_REG_MASK_RX_PW_P1,
	NRF24L01_REG_MASK_RX_PW_P2,
	NRF24L01_REG_MASK_RX_PW_P3,
	NRF24L01_REG_MASK_RX_PW_P4,
	NRF24L01_REG_MASK_RX_PW_P5,
	NRF24L01_REG_MASK_FIFO_STATUS,
	0,
	0,
	0,
	0,
	NRF24L01_REG_MASK_DYNPD,
	NRF24L01_REG_MASK_FEATURE
};

#define NRF24L01_NUM_REGS 30


int nrf24l01_write_short_reg(void* ctx, unsigned int reg, unsigned int val)
{
	char data[] = { NRF24L01_CMD_W_REGISTER | (u8)reg, (u8)val };
	return spi_write(((struct nrf24l01_t*)ctx)->spi, (char*)&data, 2);
}

int nrf24l01_write_short_reg_masked(void* ctx, unsigned int reg, unsigned int val)
{
	if(reg < 0 || reg >= NRF24L01_NUM_REGS)
		return -EINVAL;
	val &= nrf24l01_reg_masks[reg];
	return nrf24l01_write_short_reg(ctx, reg, val);
}

int nrf24l01_read_short_reg(void* ctx, unsigned int reg, unsigned int* val)
{
	ssize_t err;
	err = spi_w8r8(((struct nrf24l01_t*)ctx)->spi, NRF24L01_CMD_R_REGISTER | (u8)reg);
	if(err < 0)
		return err;
	*val = err;
	return 0;
}

static int nrf24l01_spi_write(struct nrf24l01_t* nrf, unsigned char* data, unsigned int len)
{
	return spi_write(nrf->spi, data, len);
}

static int nrf24l01_spi_cmd_write(struct nrf24l01_t* nrf, u8 cmd, unsigned char* data, unsigned int len)
{
	int err;
	unsigned char* regwrite = vmalloc(len + 1);
	if(!regwrite)
		return -ENOMEM;
	*regwrite = cmd;
	memcpy(regwrite + 1, data, len);
	err = nrf24l01_spi_write(nrf, regwrite, len + 1); 
	vfree(regwrite);
	return err;
}

// There is no read only mode
static int nrf24l01_spi_cmd_read(struct nrf24l01_t* nrf, u8 cmd, unsigned char* dest, unsigned int len)
{
	int err = -ENOMEM;
	unsigned char *read_buffer, *write_buffer;
	struct spi_transfer trans = {
		.len = len + 1
	};
	write_buffer = vzalloc(len + 1);
	if(!write_buffer)
		goto exit_noalloc;
	*write_buffer = cmd;
	read_buffer = vmalloc(len + 1);
	if(!read_buffer)
		goto exit_writealloc;
	trans.tx_buf = write_buffer;
	trans.rx_buf = read_buffer;
	err = spi_sync_transfer(nrf->spi, &trans, 1);
	memcpy(dest, read_buffer + 1, len);
	vfree(read_buffer);
exit_writealloc:
	vfree(write_buffer);
exit_noalloc:
	return err;		
}

int nrf24l01_spi_write_reg(struct nrf24l01_t* nrf, unsigned int reg, unsigned char* data, unsigned int len)
{
	return nrf24l01_spi_cmd_write(nrf, NRF24L01_CMD_W_REGISTER | (u8)reg, data, len);
}

int nrf24l01_spi_read_reg(struct nrf24l01_t* nrf, unsigned int reg, unsigned char* data, unsigned int len)
{
	return nrf24l01_spi_cmd_read(nrf, NRF24L01_CMD_R_REGISTER | (u8)reg, data, len);
}

int nrf24l01_spi_read_rx_pld(struct nrf24l01_t* nrf, unsigned char* data, unsigned int len)
{
	return nrf24l01_spi_cmd_read(nrf, NRF24L01_CMD_R_RX_PAYLOAD, data, len);
}

int nrf24l01_spi_write_tx_pld(struct nrf24l01_t* nrf, unsigned char* data, unsigned int len)
{
	return nrf24l01_spi_cmd_write(nrf, NRF24L01_CMD_W_TX_PAYLOAD, data, len);
}

int nrf24l01_spi_flush_tx(struct nrf24l01_t* nrf)
{
	unsigned char flush_tx = NRF24L01_CMD_FLUSH_TX;
	return nrf24l01_spi_write(nrf, &flush_tx, 1);
}

int nrf24l01_spi_flush_rx(struct nrf24l01_t* nrf)
{
	unsigned char flush_rx = NRF24L01_CMD_FLUSH_RX;
	return nrf24l01_spi_write(nrf, &flush_rx, 1);
}

int nrf24l01_spi_reuse_tx_pl(struct nrf24l01_t* nrf)
{
	unsigned char reuse_tx_pl = NRF24L01_CMD_REUSE_TX_PL;
	return nrf24l01_spi_write(nrf, &reuse_tx_pl, 1);
}

int nrf24l01_spi_read_rx_pl_width(struct nrf24l01_t* nrf, unsigned int* width)
{
	int err;
	u8 cmd = NRF24L01_CMD_R_RX_PL_WID;
	if((err = spi_w8r8(nrf->spi, cmd)) < 0)
		return err;
	*width = (unsigned int)err;
	return 0;
}

int nrf24l01_spi_write_ack_pld(struct nrf24l01_t* nrf, unsigned int pipe, unsigned char* data, unsigned int len)
{
	return nrf24l01_spi_cmd_write(nrf, NRF24L01_CMD_W_ACK_PAYLOAD | (u8)pipe, data, len);
}

int nrf24l01_spi_write_tx_pld_no_ack(struct nrf24l01_t* nrf, unsigned char* data, unsigned int len)
{
	return nrf24l01_spi_cmd_write(nrf, NRF24L01_CMD_W_TX_PAYLOAD_NO_ACK, data, len);
}

int nrf24l01_spi_nop(struct nrf24l01_t* nrf)
{
	unsigned char nop = NRF24L01_CMD_NOP;
	return nrf24l01_spi_write(nrf, &nop, 1);
}

int nrf24l01_spi_read_status(struct nrf24l01_t* nrf, unsigned char* status)
{
	unsigned char write_buffer = NRF24L01_CMD_NOP;
	struct spi_transfer xfer = {
		.tx_buf	= &write_buffer,
		.rx_buf	= status,
		.len	= 1
	};
	return spi_sync_transfer(nrf->spi, &xfer, 1);
}
