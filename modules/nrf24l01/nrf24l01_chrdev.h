#ifndef _NRF24L01_CHRDEV_H_
#define _NRF24L01_CHRDEV_H_

#include "nrf24l01_core.h"

int chrdev_alloc(nrf24l01_t* nrf);
int chrdev_free(nrf24l01_t* nrf);

#endif
