#include <linux/errno.h>
#include <linux/err.h>
#include <linux/vmalloc.h>

#include "nrf24l01_reg.h"
#include "nrf24l01_spi.h"
#include "nrf24l01_core.h"
#include "nrf24l01_functions.h"
#include "partregmap.h"

static struct partreg_range range_config_prim_rx = partreg_reg_range(0, 1);
static unsigned int mask_config_prim_rx = 0b1;

static struct partreg_template reg_config_prim_rx = {
	.name = "config_prim_rx",
	.reg = NRF24L01_REG_CONFIG,
	.offset = 0,
	.mask = &mask_config_prim_rx,
	.len = 1,
	.value_range = &range_config_prim_rx
};

static struct partreg_range range_config_pwr_up = partreg_reg_range(0, 1);
static unsigned int mask_config_pwr_up = 0b10;

static struct partreg_template reg_config_pwr_up = {
	.name = "config_pwr_up",
	.reg = NRF24L01_REG_CONFIG,
	.offset = 1,
	.mask = &mask_config_pwr_up,
	.len = 1,
	.value_range = &range_config_pwr_up
};

static struct partreg_range range_config_crco = partreg_reg_range(0, 1);
static unsigned int mask_config_crco = 0b100;

static struct partreg_template reg_config_crco = {
	.name = "config_crco",
	.reg = NRF24L01_REG_CONFIG,
	.offset = 2,
	.mask = &mask_config_crco,
	.len = 1,
	.value_range = &range_config_crco
};

static struct partreg_range range_config_en_crc = partreg_reg_range(0, 1);
static unsigned int mask_config_en_crc = 0b1000;

static struct partreg_template reg_config_en_crc = {
	.name = "config_en_crc",
	.reg = NRF24L01_REG_CONFIG,
	.offset = 3,
	.mask = &mask_config_en_crc,
	.len = 1,
	.value_range = &range_config_en_crc
};

static struct partreg_range range_config_mask_max_rt = partreg_reg_range(0, 1);
static unsigned int mask_config_mask_max_rt = 0b10000;

static struct partreg_template reg_config_mask_max_rt = {
	.name = "config_mask_max_rt",
	.reg = NRF24L01_REG_CONFIG,
	.offset = 4,
	.mask = &mask_config_mask_max_rt,
	.len = 1,
	.value_range = &range_config_mask_max_rt
};

static struct partreg_range range_config_mask_tx_ds = partreg_reg_range(0, 1);
static unsigned int mask_config_mask_tx_ds = 0b100000;

static struct partreg_template reg_config_mask_tx_ds = {
	.name = "config_mask_tx_ds",
	.reg = NRF24L01_REG_CONFIG,
	.offset = 5,
	.mask = &mask_config_mask_tx_ds,
	.len = 1,
	.value_range = &range_config_mask_tx_ds
};

static struct partreg_range range_config_mask_rx_dr = partreg_reg_range(0, 1);
static unsigned int mask_config_mask_rx_dr = 0b1000000;

static struct partreg_template reg_config_mask_rx_dr = {
	.name = "config_rx_dr",
	.reg = NRF24L01_REG_CONFIG,
	.offset = 6,
	.mask = &mask_config_mask_rx_dr,
	.len = 1,
	.value_range = &range_config_mask_rx_dr
};

static struct partreg_range range_en_aa_enaa_p0 = partreg_reg_range(0, 1);
static unsigned int mask_en_aa_enaa_p0 = 0b1;

static struct partreg_template reg_en_aa_enaa_p0 = {
	.name = "en_aa_enaa_p0",
	.reg = NRF24L01_REG_EN_AA,
	.offset = 0,
	.mask = &mask_en_aa_enaa_p0,
	.len = 1,
	.value_range = &range_en_aa_enaa_p0
};

static struct partreg_range range_en_aa_enaa_p1 = partreg_reg_range(0, 1);
static unsigned int mask_en_aa_enaa_p1 = 0b10;

static struct partreg_template reg_en_aa_enaa_p1 = {
	.name = "en_aa_enaa_p1",
	.reg = NRF24L01_REG_EN_AA,
	.offset = 1,
	.mask = &mask_en_aa_enaa_p1,
	.len = 1,
	.value_range = &range_en_aa_enaa_p1
};

static struct partreg_range range_en_aa_enaa_p2 = partreg_reg_range(0, 1);
static unsigned int mask_en_aa_enaa_p2 = 0b100;

static struct partreg_template reg_en_aa_enaa_p2 = {
	.name = "en_aa_enaa_p2",
	.reg = NRF24L01_REG_EN_AA,
	.offset = 2,
	.mask = &mask_en_aa_enaa_p2,
	.len = 1,
	.value_range = &range_en_aa_enaa_p2
};

static struct partreg_range range_en_aa_enaa_p3 = partreg_reg_range(0, 1);
static unsigned int mask_en_aa_enaa_p3 = 0b1000;

static struct partreg_template reg_en_aa_enaa_p3 = {
	.name = "en_aa_enaa_p3",
	.reg = NRF24L01_REG_EN_AA,
	.offset = 3,
	.mask = &mask_en_aa_enaa_p3,
	.len = 1,
	.value_range = &range_en_aa_enaa_p3
};

static struct partreg_range range_en_aa_enaa_p4 = partreg_reg_range(0, 1);
static unsigned int mask_en_aa_enaa_p4 = 0b10000;

static struct partreg_template reg_en_aa_enaa_p4 = {
	.name = "en_aa_enaa_p4",
	.reg = NRF24L01_REG_EN_AA,
	.offset = 4,
	.mask = &mask_en_aa_enaa_p4,
	.len = 1,
	.value_range = &range_en_aa_enaa_p4
};

static struct partreg_range range_en_aa_enaa_p5 = partreg_reg_range(0, 1);
static unsigned int mask_en_aa_enaa_p5 = 0b100000;

static struct partreg_template reg_en_aa_enaa_p5 = {
	.name = "en_aa_enaa_p5",
	.reg = NRF24L01_REG_EN_AA,
	.offset = 5,
	.mask = &mask_en_aa_enaa_p5,
	.len = 1,
	.value_range = &range_en_aa_enaa_p5
};

static struct partreg_range range_en_rxaddr_erx_p0 = partreg_reg_range(0, 1);
static unsigned int mask_en_rxaddr_erx_p0 = 0b1;

static struct partreg_template reg_en_rxaddr_erx_p0 = {
	.name = "en_rxaddr_erx_p0",
	.reg = NRF24L01_REG_EN_RXADDR,
	.offset = 0,
	.mask = &mask_en_rxaddr_erx_p0,
	.len = 1,
	.value_range = &range_en_rxaddr_erx_p0
};

static struct partreg_range range_en_rxaddr_erx_p1 = partreg_reg_range(0, 1);
static unsigned int mask_en_rxaddr_erx_p1 = 0b10;

static struct partreg_template reg_en_rxaddr_erx_p1 = {
	.name = "en_rxaddr_erx_p1",
	.reg = NRF24L01_REG_EN_RXADDR,
	.offset = 1,
	.mask = &mask_en_rxaddr_erx_p1,
	.len = 1,
	.value_range = &range_en_rxaddr_erx_p1
};

static struct partreg_range range_en_rxaddr_erx_p2 = partreg_reg_range(0, 1);
static unsigned int mask_en_rxaddr_erx_p2 = 0b100;

static struct partreg_template reg_en_rxaddr_erx_p2 = {
	.name = "en_rxaddr_erx_p2",
	.reg = NRF24L01_REG_EN_RXADDR,
	.offset = 2,
	.mask = &mask_en_rxaddr_erx_p2,
	.len = 1,
	.value_range = &range_en_rxaddr_erx_p2
};

static struct partreg_range range_en_rxaddr_erx_p3 = partreg_reg_range(0, 1);
static unsigned int mask_en_rxaddr_erx_p3 = 0b1000;

static struct partreg_template reg_en_rxaddr_erx_p3 = {
	.name = "en_rxaddr_erx_p3",
	.reg = NRF24L01_REG_EN_RXADDR,
	.offset = 3,
	.mask = &mask_en_rxaddr_erx_p3,
	.len = 1,
	.value_range = &range_en_rxaddr_erx_p3
};

static struct partreg_range range_en_rxaddr_erx_p4 = partreg_reg_range(0, 1);
static unsigned int mask_en_rxaddr_erx_p4 = 0b10000;

static struct partreg_template reg_en_rxaddr_erx_p4 = {
	.name = "en_rxaddr_erx_p4",
	.reg = NRF24L01_REG_EN_RXADDR,
	.offset = 4,
	.mask = &mask_en_rxaddr_erx_p4,
	.len = 1,
	.value_range = &range_en_rxaddr_erx_p4
};

static struct partreg_range range_en_rxaddr_erx_p5 = partreg_reg_range(0, 1);
static unsigned int mask_en_rxaddr_erx_p5 = 0b100000;

static struct partreg_template reg_en_rxaddr_erx_p5 = {
	.name = "en_rxaddr_erx_p5",
	.reg = NRF24L01_REG_EN_RXADDR,
	.offset = 5,
	.mask = &mask_en_rxaddr_erx_p5,
	.len = 1,
	.value_range = &range_en_rxaddr_erx_p5
};

static struct partreg_range range_setup_aw_aw = partreg_reg_range(1, 0b11);
static unsigned int mask_setup_aw_aw = 0b11;

static struct partreg_template reg_setup_aw_aw = {
	.name = "setup_aw_aw",
	.reg = NRF24L01_REG_SETUP_AW,
	.offset = 0,
	.mask = &mask_setup_aw_aw,
	.len = 1,
	.value_range = &range_setup_aw_aw
};

static struct partreg_range range_setup_retr_arc = partreg_reg_range(0, 0b1111);
static unsigned int mask_setup_retr_arc = 0b1111;

static struct partreg_template reg_setup_retr_arc = {
	.name = "setup_retr_arc",
	.reg = NRF24L01_REG_SETUP_RETR,
	.offset = 0,
	.mask = &mask_setup_retr_arc,
	.len = 1,
	.value_range = &range_setup_retr_arc
};

static struct partreg_range range_setup_retr_ard = partreg_reg_range(0, 0b1111);
static unsigned int mask_setup_retr_ard = 0b11110000;

static struct partreg_template reg_setup_retr_ard = {
	.name = "setup_retr_ard",
	.reg = NRF24L01_REG_SETUP_RETR,
	.offset = 4,
	.mask = &mask_setup_retr_ard,
	.len = 1,
	.value_range = &range_setup_retr_ard
};

static struct partreg_range range_rf_ch_rf_ch = partreg_reg_range(0, 0b01111111);
static unsigned int mask_rf_ch_rf_ch = 0b01111111;

static struct partreg_template reg_rf_ch_rf_ch = {
	.name = "rf_ch_rf_ch",
	.reg = NRF24L01_REG_RF_CH,
	.offset = 0,
	.mask = &mask_rf_ch_rf_ch,
	.len = 1,
	.value_range = &range_rf_ch_rf_ch
};

static struct partreg_range range_rf_setup_rf_pwr = partreg_reg_range(0, 0b11);
static unsigned int mask_rf_setup_rf_pwr = 0b110;

static struct partreg_template reg_rf_setup_rf_pwr = {
	.name = "rf_setup_rf_pwr",
	.reg = NRF24L01_REG_RF_SETUP,
	.offset = 1,
	.mask = &mask_rf_setup_rf_pwr,
	.len = 1,
	.value_range = &range_rf_setup_rf_pwr
};

static struct partreg_range range_rf_setup_rf_dr_hi = partreg_reg_range(0, 0b001);
static struct partreg_range range_rf_setup_rf_dr_lo = partreg_reg_range(0b100, 0b100);

static struct partreg_range* range_rf_setup_rf_dr[] = {
	&range_rf_setup_rf_dr_hi,
	&range_rf_setup_rf_dr_lo
};

static struct partreg_range_table range_table_rf_setup_rf_dr = {
	.ranges = range_rf_setup_rf_dr,
	.n_ranges = 2
};

static unsigned int mask_rf_setup_rf_dr = 0b101000;

static struct partreg_template reg_rf_setup_rf_dr = {
	.name = "rf_setup_rf_dr",
	.reg = NRF24L01_REG_RF_SETUP,
	.offset = 3,
	.mask = &mask_rf_setup_rf_dr,
	.len = 1,
	.value_ranges = &range_table_rf_setup_rf_dr
};

static struct partreg_range range_rf_setup_pll_lock = partreg_reg_range(0, 1);
static unsigned int mask_rf_setup_pll_lock = 0b10000;

static struct partreg_template reg_rf_setup_pll_lock = {
	.name = "rf_setup_pll_lock",
	.reg = NRF24L01_REG_RF_SETUP,
	.offset = 4,
	.mask = &mask_rf_setup_pll_lock,
	.len = 1,
	.value_range = &range_rf_setup_pll_lock
};

static struct partreg_range range_rf_setup_cont_wave = partreg_reg_range(0, 1);
static unsigned int mask_rf_setup_cont_wave = 0b10000000;

static struct partreg_template reg_rf_setup_cont_wave = {
	.name = "rf_setup_cont_wave",
	.reg = NRF24L01_REG_RF_SETUP,
	.offset = 7,
	.mask = &mask_rf_setup_cont_wave,
	.len = 1,
	.value_range = &range_rf_setup_cont_wave
};

static struct partreg_range range_status_tx_full = partreg_reg_range(0, 1);
static unsigned int mask_status_tx_full = 0b1;

static struct partreg_template reg_status_tx_full = {
	.name = "status_tx_full",
	.reg = NRF24L01_REG_STATUS,
	.offset = 0,
	.mask = &mask_status_tx_full,
	.len = 1,
	.value_range = &range_status_tx_full,
	.reg_read = nrf24l01_reg_get_status
};

static struct partreg_range range_status_rx_p_no = partreg_reg_range(0, 0b111);
static unsigned int mask_status_rx_p_no = 0b1110;

static struct partreg_template reg_status_rx_p_no = {
	.name = "status_rx_p_no",
	.reg = NRF24L01_REG_STATUS,
	.offset = 1,
	.mask = &mask_status_rx_p_no,
	.len = 1,
	.value_range = &range_status_rx_p_no,
	.reg_read = nrf24l01_reg_get_status
};

static struct partreg_range range_status_max_rt = partreg_reg_range(0, 1);
static unsigned int mask_status_max_rt = 0b10000;

static struct partreg_template reg_status_max_rt = {
	.name = "status_max_rt",
	.reg = NRF24L01_REG_STATUS,
	.offset = 4,
	.mask = &mask_status_max_rt,
	.len = 1,
	.value_range = &range_status_max_rt,
	.reg_read = nrf24l01_reg_get_status,
	.reg_write = nrf24l01_reg_set_status
};

static struct partreg_range range_status_tx_ds = partreg_reg_range(0, 1);
static unsigned int mask_status_tx_ds = 0b100000;

static struct partreg_template reg_status_tx_ds = {
	.name = "status_tx_ds",
	.reg = NRF24L01_REG_STATUS,
	.offset = 5,
	.mask = &mask_status_tx_ds,
	.len = 1,
	.value_range = &range_status_tx_ds,
	.reg_read = nrf24l01_reg_get_status,
	.reg_write = nrf24l01_reg_set_status
};

static struct partreg_range range_status_rx_dr = partreg_reg_range(0, 1);
static unsigned int mask_status_rx_dr = 0b1000000;

static struct partreg_template reg_status_rx_dr = {
	.name = "status_rx_dr",
	.reg = NRF24L01_REG_STATUS,
	.offset = 6,
	.mask = &mask_status_rx_dr,
	.len = 1,
	.value_range = &range_status_rx_dr,
	.reg_read = nrf24l01_reg_get_status,
	.reg_write = nrf24l01_reg_set_status
};

static struct partreg_range range_observe_tx_arc_cnt = partreg_reg_range(0, 0b1111);
static unsigned int mask_observe_tx_arc_cnt = 0b00001111;

static struct partreg_template reg_observe_tx_arc_cnt = {
	.name = "observe_tx_arc_cnt",
	.reg = NRF24L01_REG_OBSERVE_TX,
	.offset = 0,
	.mask = &mask_observe_tx_arc_cnt,
	.len = 1,
	.value_range = &range_observe_tx_arc_cnt
};

static struct partreg_range range_observe_tx_plos_cnt = partreg_reg_range(0, 0b1111);
static unsigned int mask_observe_tx_plos_cnt = 0b11110000;

static struct partreg_template reg_observe_tx_plos_cnt = {
	.name = "observe_tx_plos_cnt",
	.reg = NRF24L01_REG_OBSERVE_TX,
	.offset = 4,
	.mask = &mask_observe_tx_plos_cnt,
	.len = 1,
	.value_range = &range_observe_tx_plos_cnt
};

static struct partreg_range range_rpd_rpd = partreg_reg_range(0, 0b1);
static unsigned int mask_rpd_rpd = 0b11110000;

static struct partreg_template reg_rpd_rpd = {
	.name = "rpd_rpd",
	.reg = NRF24L01_REG_RPD,
	.offset = 0,
	.mask = &mask_rpd_rpd,
	.len = 1,
	.value_range = &range_rpd_rpd
};

static struct partreg_template reg_rx_addr_p0 = {
	.name = "rx_addr_p0",
	.reg = NRF24L01_REG_RX_ADDR_P0,
	.offset = 0,
	.len_func = nrf24l01_reg_get_addr_len,
	.reg_write = nrf24l01_reg_addr_write,
	.reg_read = nrf24l01_reg_addr_read
};

static struct partreg_template reg_rx_addr_p1 = {
	.name = "rx_addr_p1",
	.reg = NRF24L01_REG_RX_ADDR_P1,
	.offset = 0,
	.len_func = nrf24l01_reg_get_addr_len,
	.reg_write = nrf24l01_reg_addr_write,
	.reg_read = nrf24l01_reg_addr_read
};

static struct partreg_template reg_rx_addr_p2 = {
	.name = "rx_addr_p2",
	.reg = NRF24L01_REG_RX_ADDR_P2,
	.offset = 0,
	.len = 1
};

static struct partreg_template reg_rx_addr_p3 = {
	.name = "rx_addr_p3",
	.reg = NRF24L01_REG_RX_ADDR_P3,
	.offset = 0,
	.len = 1
};

static struct partreg_template reg_rx_addr_p4 = {
	.name = "rx_addr_p4",
	.reg = NRF24L01_REG_RX_ADDR_P4,
	.offset = 0,
	.len = 1
};

static struct partreg_template reg_rx_addr_p5 = {
	.name = "rx_addr_p5",
	.reg = NRF24L01_REG_RX_ADDR_P5,
	.offset = 0,
	.len = 1
};

static struct partreg_template reg_tx_addr = {
	.name = "tx_addr",
	.reg = NRF24L01_REG_TX_ADDR,
	.offset = 0,
	.len_func = nrf24l01_reg_get_addr_len,
	.reg_write = nrf24l01_reg_addr_write,
	.reg_read = nrf24l01_reg_addr_read
};

static struct partreg_range range_rx_pw_p0 = partreg_reg_range(0, NRF24L01_PACKET_MAX_LENGTH);
static unsigned int mask_rx_pw_p0 = 0b00111111;

static struct partreg_template reg_rx_pw_p0 = {
	.name = "rx_pw_p0",
	.reg = NRF24L01_REG_RX_PW_P0,
	.offset = 0,
	.mask = &mask_rx_pw_p0,
	.len = 1,
	.value_range = &range_rx_pw_p0
};

static struct partreg_range range_rx_pw_p1 = partreg_reg_range(0, NRF24L01_PACKET_MAX_LENGTH);
static unsigned int mask_rx_pw_p1 = 0b00111111;

static struct partreg_template reg_rx_pw_p1 = {
	.name = "rx_pw_p1",
	.reg = NRF24L01_REG_RX_PW_P1,
	.offset = 0,
	.mask = &mask_rx_pw_p1,
	.len = 1,
	.value_range = &range_rx_pw_p1
};

static struct partreg_range range_rx_pw_p2 = partreg_reg_range(0, NRF24L01_PACKET_MAX_LENGTH);
static unsigned int mask_rx_pw_p2 = 0b00111111;

static struct partreg_template reg_rx_pw_p2 = {
	.name = "rx_pw_p2",
	.reg = NRF24L01_REG_RX_PW_P2,
	.offset = 0,
	.mask = &mask_rx_pw_p2,
	.len = 1,
	.value_range = &range_rx_pw_p2
};

static struct partreg_range range_rx_pw_p3 = partreg_reg_range(0, NRF24L01_PACKET_MAX_LENGTH);
static unsigned int mask_rx_pw_p3 = 0b00111111;

static struct partreg_template reg_rx_pw_p3 = {
	.name = "rx_pw_p3",
	.reg = NRF24L01_REG_RX_PW_P3,
	.offset = 0,
	.mask = &mask_rx_pw_p3,
	.len = 1,
	.value_range = &range_rx_pw_p3
};

static struct partreg_range range_rx_pw_p4 = partreg_reg_range(0, NRF24L01_PACKET_MAX_LENGTH);
static unsigned int mask_rx_pw_p4 = 0b00111111;

static struct partreg_template reg_rx_pw_p4 = {
	.name = "rx_pw_p4",
	.reg = NRF24L01_REG_RX_PW_P4,
	.offset = 0,
	.mask = &mask_rx_pw_p4,
	.len = 1,
	.value_range = &range_rx_pw_p4
};

static struct partreg_range range_rx_pw_p5 = partreg_reg_range(0, NRF24L01_PACKET_MAX_LENGTH);
static unsigned int mask_rx_pw_p5 = 0b00111111;

static struct partreg_template reg_rx_pw_p5 = {
	.name = "rx_pw_p5",
	.reg = NRF24L01_REG_RX_PW_P5,
	.offset = 0,
	.mask = &mask_rx_pw_p5,
	.len = 1,
	.value_range = &range_rx_pw_p5
};

static struct partreg_range range_fifo_status_rx_empty = partreg_reg_range(0, 1);
static unsigned int mask_fifo_status_rx_empty = 0b00000001;

static struct partreg_template reg_fifo_status_rx_empty = {
	.name = "fifo_status_rx_empty",
	.reg = NRF24L01_REG_FIFO_STATUS,
	.offset = 0,
	.mask = &mask_fifo_status_rx_empty,
	.len = 1,
	.value_range = &range_fifo_status_rx_empty
};

static struct partreg_range range_fifo_status_rx_full = partreg_reg_range(0, 1);
static unsigned int mask_fifo_status_rx_full = 0b00000010;

static struct partreg_template reg_fifo_status_rx_full = {
	.name = "fifo_status_rx_full",
	.reg = NRF24L01_REG_FIFO_STATUS,
	.offset = 1,
	.mask = &mask_fifo_status_rx_full,
	.len = 1,
	.value_range = &range_fifo_status_rx_full
};

static struct partreg_range range_fifo_status_tx_empty = partreg_reg_range(0, 1);
static unsigned int mask_fifo_status_tx_empty = 0b00010000;

static struct partreg_template reg_fifo_status_tx_empty = {
	.name = "fifo_status_tx_empty",
	.reg = NRF24L01_REG_FIFO_STATUS,
	.offset = 4,
	.mask = &mask_fifo_status_tx_empty,
	.len = 1,
	.value_range = &range_fifo_status_tx_empty
};

static struct partreg_range range_fifo_status_tx_full = partreg_reg_range(0, 1);
static unsigned int mask_fifo_status_tx_full = 0b00100000;

static struct partreg_template reg_fifo_status_tx_full = {
	.name = "fifo_status_tx_full",
	.reg = NRF24L01_REG_FIFO_STATUS,
	.offset = 5,
	.mask = &mask_fifo_status_tx_full,
	.len = 1,
	.value_range = &range_fifo_status_tx_full
};

static struct partreg_range range_fifo_status_tx_reuse = partreg_reg_range(0, 1);
static unsigned int mask_fifo_status_tx_reuse = 0b01000000;

static struct partreg_template reg_fifo_status_tx_reuse = {
	.name = "fifo_status_tx_reuse",
	.reg = NRF24L01_REG_FIFO_STATUS,
	.offset = 6,
	.mask = &mask_fifo_status_tx_reuse,
	.len = 1,
	.value_range = &range_fifo_status_tx_reuse
};

static struct partreg_range range_dynpd_dpl_p0 = partreg_reg_range(0, 1);
static unsigned int mask_dynpd_dpl_p0 = 0b00000001;

static struct partreg_template reg_dynpd_dpl_p0 = {
	.name = "fifo_dynpd_dpl_p0",
	.reg = NRF24L01_REG_DYNPD,
	.offset = 0,
	.mask = &mask_dynpd_dpl_p0,
	.len = 1,
	.value_range = &range_dynpd_dpl_p0
};

static struct partreg_range range_dynpd_dpl_p1 = partreg_reg_range(0, 1);
static unsigned int mask_dynpd_dpl_p1 = 0b00000010;

static struct partreg_template reg_dynpd_dpl_p1 = {
	.name = "fifo_dynpd_dpl_p1",
	.reg = NRF24L01_REG_DYNPD,
	.offset = 1,
	.mask = &mask_dynpd_dpl_p1,
	.len = 1,
	.value_range = &range_dynpd_dpl_p1
};

static struct partreg_range range_dynpd_dpl_p2 = partreg_reg_range(0, 1);
static unsigned int mask_dynpd_dpl_p2 = 0b00000100;

static struct partreg_template reg_dynpd_dpl_p2 = {
	.name = "fifo_dynpd_dpl_p2",
	.reg = NRF24L01_REG_DYNPD,
	.offset = 2,
	.mask = &mask_dynpd_dpl_p2,
	.len = 1,
	.value_range = &range_dynpd_dpl_p2
};

static struct partreg_range range_dynpd_dpl_p3 = partreg_reg_range(0, 1);
static unsigned int mask_dynpd_dpl_p3 = 0b00001000;

static struct partreg_template reg_dynpd_dpl_p3 = {
	.name = "fifo_dynpd_dpl_p3",
	.reg = NRF24L01_REG_DYNPD,
	.offset = 3,
	.mask = &mask_dynpd_dpl_p3,
	.len = 1,
	.value_range = &range_dynpd_dpl_p3
};

static struct partreg_range range_dynpd_dpl_p4 = partreg_reg_range(0, 1);
static unsigned int mask_dynpd_dpl_p4 = 0b00010000;

static struct partreg_template reg_dynpd_dpl_p4 = {
	.name = "fifo_dynpd_dpl_p4",
	.reg = NRF24L01_REG_DYNPD,
	.offset = 4,
	.mask = &mask_dynpd_dpl_p4,
	.len = 1,
	.value_range = &range_dynpd_dpl_p4
};

static struct partreg_range range_dynpd_dpl_p5 = partreg_reg_range(0, 1);
static unsigned int mask_dynpd_dpl_p5 = 0b00100000;

static struct partreg_template reg_dynpd_dpl_p5 = {
	.name = "fifo_dynpd_dpl_p5",
	.reg = NRF24L01_REG_DYNPD,
	.offset = 5,
	.mask = &mask_dynpd_dpl_p5,
	.len = 1,
	.value_range = &range_dynpd_dpl_p5
};

static struct partreg_range range_feature_en_dyn_ack = partreg_reg_range(0, 1);
static unsigned int mask_feature_en_dyn_ack = 0b00000001;

static struct partreg_template reg_feature_en_dyn_ack = {
	.name = "feature_en_dyn_ack",
	.reg = NRF24L01_REG_FEATURE,
	.offset = 0,
	.mask = &mask_feature_en_dyn_ack,
	.len = 1,
	.value_range = &range_feature_en_dyn_ack
};

static struct partreg_range range_feature_en_ack_pay = partreg_reg_range(0, 1);
static unsigned int mask_feature_en_ack_pay = 0b00000010;

static struct partreg_template reg_feature_en_ack_pay = {
	.name = "feature_en_ack_pay",
	.reg = NRF24L01_REG_FEATURE,
	.offset = 1,
	.mask = &mask_feature_en_ack_pay,
	.len = 1,
	.value_range = &range_feature_en_ack_pay
};

static struct partreg_range range_feature_en_dpl = partreg_reg_range(0, 1);
static unsigned int mask_feature_en_dpl = 0b00000100;

static struct partreg_template reg_feature_en_dpl = {
	.name = "feature_en_dpl",
	.reg = NRF24L01_REG_FEATURE,
	.offset = 2,
	.mask = &mask_feature_en_dpl,
	.len = 1,
	.value_range = &range_feature_en_dpl
};

static struct partreg_template* nrf24l01_regs[] = {
	&reg_config_prim_rx,
	&reg_config_pwr_up,
	&reg_config_crco,
	&reg_config_en_crc,
	&reg_config_mask_max_rt,
	&reg_config_mask_tx_ds,
	&reg_config_mask_rx_dr,
	&reg_en_aa_enaa_p0,
	&reg_en_aa_enaa_p1,
	&reg_en_aa_enaa_p2,
	&reg_en_aa_enaa_p3,
	&reg_en_aa_enaa_p4,
	&reg_en_aa_enaa_p5,
	&reg_en_rxaddr_erx_p0,
	&reg_en_rxaddr_erx_p1,
	&reg_en_rxaddr_erx_p2,
	&reg_en_rxaddr_erx_p3,
	&reg_en_rxaddr_erx_p4,
	&reg_en_rxaddr_erx_p5,
	&reg_setup_aw_aw,
	&reg_setup_retr_arc,
	&reg_setup_retr_ard,
	&reg_rf_ch_rf_ch,
	&reg_rf_setup_rf_pwr,
	&reg_rf_setup_rf_dr,
	&reg_rf_setup_pll_lock,
	&reg_rf_setup_cont_wave,
	&reg_status_tx_full,
	&reg_status_rx_p_no,
	&reg_status_max_rt,
	&reg_status_tx_ds,
	&reg_status_rx_dr,
	&reg_observe_tx_arc_cnt,
	&reg_observe_tx_plos_cnt,
	&reg_rpd_rpd,
	&reg_rx_addr_p0,
	&reg_rx_addr_p1,
	&reg_rx_addr_p2,
	&reg_rx_addr_p3,
	&reg_rx_addr_p4,
	&reg_rx_addr_p5,
	&reg_tx_addr,
	&reg_rx_pw_p0,
	&reg_rx_pw_p1,
	&reg_rx_pw_p2,
	&reg_rx_pw_p3,
	&reg_rx_pw_p4,
	&reg_rx_pw_p5,
	&reg_fifo_status_rx_empty,
	&reg_fifo_status_rx_full,
	&reg_fifo_status_tx_empty,
	&reg_fifo_status_tx_full,
	&reg_fifo_status_tx_reuse,
	&reg_dynpd_dpl_p0,
	&reg_dynpd_dpl_p1,
	&reg_dynpd_dpl_p2,
	&reg_dynpd_dpl_p3,
	&reg_dynpd_dpl_p4,
	&reg_dynpd_dpl_p5,
	&reg_feature_en_dyn_ack,
	&reg_feature_en_ack_pay,
	&reg_feature_en_dpl
};

static struct partreg_layout nrf24l01_reg_layout = {
	.regs = nrf24l01_regs,
	.n_regs = sizeof(nrf24l01_regs) / sizeof(struct partreg_template*),
};

int nrf24l01_create_partregs(struct nrf24l01_t* nrf)
{
	nrf->reg_table = partreg_create_table(&nrf24l01_reg_layout, nrf->regmap_short, nrf);	
	if(IS_ERR(nrf->reg_table))
		return PTR_ERR(nrf->reg_table);
	return 0;
}

void nrf24l01_free_partregs(nrf24l01_t* nrf)
{
	partreg_free_table(nrf->reg_table);
}

int nrf24l01_reg_addr_read(void* ctx, struct partreg* reg, unsigned int* data, unsigned int len)
{
	return nrf24l01_spi_read_reg((nrf24l01_t*)ctx, reg->reg, (unsigned char*)data, len);
}

int nrf24l01_reg_addr_write(void* ctx, struct partreg* reg, unsigned int* data, unsigned int len)
{
	return nrf24l01_spi_write_reg((nrf24l01_t*)ctx, reg->reg, (unsigned char*)data, len);
}

int nrf24l01_reg_get_addr_len(void* ctx, struct partreg* reg, unsigned int* len)
{
	return nrf24l01_get_address_width((nrf24l01_t*) ctx, len);
}

int nrf24l01_reg_set_status(void* ctx, struct partreg* reg, unsigned int* data, unsigned int len)
{
	unsigned int value = *data;
	if(len < 1)
	{
		return -EINVAL;
	}
	value <<= reg->offset;
	value &= *reg->mask;
	return nrf24l01_write_short_reg_masked((nrf24l01_t*)ctx, reg->reg, value);
}

int nrf24l01_reg_get_status(void* ctx, struct partreg* reg, unsigned int* data, unsigned int len)
{
	int err;
	unsigned int status = 0;
	if(len < 1)
	{
		return -EINVAL;
	}
	if((err = nrf24l01_spi_read_status((nrf24l01_t*)ctx, (unsigned char*)&status)))
	{
		return err;
	}
	status &= *reg->mask;
	status >>= reg->offset;
	*data = status;
	return err;
}
