/*
 * diagnostics.c
 *
 *  Created on: Feb 4, 2011
 *      Author: James Harwood
 *
 * This module is free software and there is NO WARRANTY.
 * No restrictions on use. You can use, modify and redistribute it for
   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
 */

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include "diagnostics.h"
#include "clock.h"

#define FLASH_TIME (CLOCK_SECOND/4)
#define PAUSE_TIME (CLOCK_SECOND*2)

#define LED_PORT LPC_GPIO0
#define LED_PIN 26

struct diag_state {
  struct pt pt;
  struct timer timer;
  uint8_t num_flashes;
  uint8_t limit;
  uint8_t count;
};

static struct diag_state s;

void diag_init()
{
	PT_INIT(&s.pt);
	s.count = 0;
	s.limit = 0;
	s.num_flashes = 0;

	// Set P0_22 to 00 - GPIO
	LPC_PINCON->PINSEL1	&= (~(3 << 12));
	LED_PORT->FIODIR |= (1 << LED_PIN);
	LED_PORT->FIOCLR = (1 << LED_PIN);
}

void diag_set(uint8_t num)
{
	s.num_flashes = num;
}

static
PT_THREAD(handle_diag(void))
{
	PT_BEGIN(&s.pt);

	s.limit = s.num_flashes;

	if(s.limit > 0) {
		for(s.count = 0; s.count < s.limit; s.count++) {
			LED_PORT->FIOSET = (1 << LED_PIN);
			timer_set(&s.timer, FLASH_TIME);
			PT_WAIT_UNTIL(&s.pt, timer_expired(&s.timer));
			LED_PORT->FIOCLR = (1 << LED_PIN);
			timer_set(&s.timer, FLASH_TIME);
			PT_WAIT_UNTIL(&s.pt, timer_expired(&s.timer));
		}
		timer_set(&s.timer, PAUSE_TIME);
		PT_WAIT_UNTIL(&s.pt, timer_expired(&s.timer));
	}
	PT_END(&s.pt);
}


void diag_appcall()
{
	handle_diag();
}
