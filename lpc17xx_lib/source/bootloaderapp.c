/*
 * bootloaderapp.c
 *
 *  Created on: Feb 5, 2011
 *      Author: James Harwood
 *
 * This module is free software and there is NO WARRANTY.
 * No restrictions on use. You can use, modify and redistribute it for
   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
 */

#include "bootloaderapp.h"
#include "config.h"
#include "pt.h"
#include "timer.h"
#include "flash.h"
#include "tftp.h"
#include "diagnostics.h"

#define RETRY_TIME (CLOCK_SECOND*60)

struct bl_state {
  struct pt pt;
  char state;
  struct timer timer;
  uint16_t retries;
};

static struct bl_state s;

void bl_init()
{
	PT_INIT(&s.pt);

}
static
PT_THREAD(handle_bl(void))
{
	PT_BEGIN(&s.pt);
	s.retries = 3;
	do {
		flash_init();
		tftpc_init();
		tftpc_get(FIRMWARE_FILENAME);
		PT_WAIT_UNTIL(&s.pt, !tftpc_busy());

		uint8_t rc = tftpc_result();
		if(rc == TFTPC_FLASH_ERROR) {
			// any flash write errors, make sure the code doesn't run
			invalidate_code();
		}

		// valid app code in place?
		if(!check_valid_code()) {
			// figure out what happened
			// and set indicator
			switch(rc) {
			case TFTPC_SUCCESS:
				diag_set(1);
				s.retries--;
				break;
			case TFTPC_SERVER_DOWN:
				diag_set(2);
				break;
			case TFTPC_FILE_NOT_FOUND:
				diag_set(3);
				break;
			case TFTPC_ERROR:
				diag_set(4);
				s.retries--;
				break;
			case TFTPC_FLASH_ERROR:
				diag_set(5);
				s.retries--;
				break;
			}

		} else {
			run_app();
		}

		timer_set(&s.timer, RETRY_TIME);
		PT_WAIT_UNTIL(&s.pt, timer_expired(&s.timer));
	} while (s.retries > 0);

	while(1) {
		PT_YIELD(&s.pt);
	}

	PT_END(&s.pt);
}

void bl_appcall()
{
	handle_bl();
}
