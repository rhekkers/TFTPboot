/*
 * flash.c
 *
 *  Created on: Feb 4, 2011
 *      Author: James Harwood
 *
 * This module is free software and there is NO WARRANTY.
 * No restrictions on use. You can use, modify and redistribute it for
   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
 */

#include "flash.h"
#include "LPC17xx.h"
#include "lpc17xx_systick.h"

#define IAP_ADDRESS 0x1FFF1FF1

#define PREPARE_SECTOR		50
#define COPY_RAM_TO_FLASH	51
#define ERASE_SECTOR		52
#define BLANK_CHECK_SECTOR  53

#define FLASH_START_SECTOR 16
#define FLASH_START_ADDR	0x010000

static uint32_t param_table[5];
static uint32_t result_table[5];

static uint32_t *p_dest;
static uint32_t cur_sector;
static uint8_t first_block;

void iap_entry(uint32_t param_tab[], uint32_t result_tab[])
{
    void (*iap)(uint32_t [], uint32_t []);

    iap = (void (*)(uint32_t [], uint32_t []))IAP_ADDRESS;
    iap(param_tab,result_tab);
}

uint32_t erase_current_sector()
{
	__disable_irq();
    param_table[0] = PREPARE_SECTOR;
    param_table[1] = cur_sector;
    param_table[2] = cur_sector;
    param_table[3] = SystemCoreClock/1000;
    iap_entry(param_table,result_table);

    if( result_table[0] != 0 ) {
    	__enable_irq();
		return result_table[0];
	}

    param_table[0] = ERASE_SECTOR;
    param_table[1] = cur_sector;
    param_table[2] = cur_sector;
    param_table[3] = SystemCoreClock/1000;
    iap_entry(param_table,result_table);

    __enable_irq();
	return result_table[0];
}

void flash_init()
{
	p_dest = (uint32_t *)FLASH_START_ADDR;
	cur_sector = FLASH_START_SECTOR;
	first_block = 1;
}

void invalidate_code() {
	cur_sector = FLASH_START_SECTOR;
	erase_current_sector();
}

uint8_t check_valid_code()
{
	uint32_t *p, check,i;

	param_table[0] = BLANK_CHECK_SECTOR;
	param_table[1] = FLASH_START_SECTOR;
	param_table[2] = FLASH_START_SECTOR;
	iap_entry(param_table,result_table);

	if( result_table[0] == 0 ) {
		return 0;
	}

	check = 0;
	p = (uint32_t *)FLASH_START_ADDR;

	for (i = 0; i < 8; i++) {
		check += *p;
		p++;
	}

	if(check != 0) {
		return 0;
	}
	return 1;
}

uint32_t flash_write_block(uint32_t *src)
{
	if(first_block) {
		first_block = 0;
		uint32_t rc = erase_current_sector();
		if(rc != 0) {
			return rc;
		}
	}
	__disable_irq();
    param_table[0] = PREPARE_SECTOR;
    param_table[1] = cur_sector;
    param_table[2] = cur_sector;
    param_table[3] = SystemCoreClock/1000;
    iap_entry(param_table,result_table);

    if( result_table[0] != 0 ) {
    	__enable_irq();
		return result_table[0];
	}

    param_table[0] = COPY_RAM_TO_FLASH;
    param_table[1] = (uint32_t)p_dest;
    param_table[2] = (uint32_t)src;
    param_table[3] = 512;
    param_table[4] = SystemCoreClock/1000;
    iap_entry(param_table,result_table);

    __enable_irq();

    if( result_table[0] != 0 ) {
		return result_table[0];
	}

    p_dest += (512 / 4);
    if((((uint32_t)p_dest) & 0x07fff) == 0) {
    	cur_sector++;
    	return erase_current_sector();
    }
    return 0;
}

void NVIC_SCBDeInit(void)
{
	uint8_t tmp;

	SCB->ICSR = 0x0A000000;
	SCB->VTOR = 0x00000000;
	SCB->AIRCR = 0x05FA0000;
	SCB->SCR = 0x00000000;
	SCB->CCR = 0x00000000;

	for (tmp = 0; tmp < 32; tmp++) {
		SCB->SHP[tmp] = 0x00;
	}

	SCB->SHCSR = 0x00000000;
	SCB->CFSR = 0xFFFFFFFF;
	SCB->HFSR = 0xFFFFFFFF;
	SCB->DFSR = 0xFFFFFFFF;
}

void NVIC_DeInit(void)
{
	uint8_t tmp;

	/* Disable all interrupts */
	NVIC->ICER[0] = 0xFFFFFFFF;
	NVIC->ICER[1] = 0x00000001;
	/* Clear all pending interrupts */
	NVIC->ICPR[0] = 0xFFFFFFFF;
	NVIC->ICPR[1] = 0x00000001;

	/* Clear all interrupt priority */
	for (tmp = 0; tmp < 32; tmp++) {
		NVIC->IP[tmp] = 0x00;
	}
}

void SYSTICK_Cmd(uint8_t NewState)
{
	//CHECK_PARAM(PARAM_FUNCTIONALSTATE(NewState));

	if(NewState == 1)
		//Enable System Tick counter
		SysTick->CTRL |= ST_CTRL_ENABLE;
	else
		//Disable System Tick counter
		SysTick->CTRL &= ~ST_CTRL_ENABLE;
}

void SYSTICK_IntCmd(uint8_t NewState)
{
	//CHECK_PARAM(PARAM_FUNCTIONALSTATE(NewState));

	if(NewState == 1)
		//Enable System Tick counter
		SysTick->CTRL |= ST_CTRL_TICKINT;
	else
		//Disable System Tick counter
		SysTick->CTRL &= ~ST_CTRL_TICKINT;
}

void run_app()
{
	void (*user_code_entry)(void);

	unsigned *p;	// used for loading address of reset handler from user flash

	//NVIC_DeInit();
	//NVIC_SCBDeInit();

	SYSTICK_Cmd(0);
	SYSTICK_IntCmd(0);

	/* Change the Vector Table to the USER_FLASH_START
	in case the user application uses interrupts */
	SCB->VTOR = (FLASH_START_ADDR & 0x1FFFFF80);

	// Load contents of second word of user flash - the reset handler address
	// in the applications vector table
	p = (unsigned *)(FLASH_START_ADDR + 4);

	// Set user_code_entry to be the address contained in that second word
	// of user flash
	user_code_entry = (void *) *p;

	// Jump to user application
  user_code_entry();

}
