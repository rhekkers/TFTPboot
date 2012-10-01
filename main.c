/*
===============================================================================
 Name        : main.c
 Author      : James Harwood
 Version     : 1.0

 * This module is free software and there is NO WARRANTY.
 * No restrictions on use. You can use, modify and redistribute it for
   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
===============================================================================
*/

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif


#include <string.h>
#include "lpc17xx_emac.h"

#include "config.h"
#include "timer.h"
#include "uip.h"
#include "uip_arp.h"
#include "tapdev.h"
#include "diagnostics.h"
#include "bootloaderapp.h"


#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

/*--------------------------- uip_log ---------------------------------*/

void uip_log(char *m)
{
  //printf("uIP log message: %s\n", m);
}

/*--------------------------- main ---------------------------------*/

unsigned int i;
uip_ipaddr_t ipaddr;	/* local IP address */
struct timer periodic_timer, arp_timer, boot_timer;

int main(void) {
	
	// clock init
	clock_init();
	// two timers for tcp/ip
	timer_set(&periodic_timer, CLOCK_SECOND / 2); /* 0.5s */
	timer_set(&arp_timer, CLOCK_SECOND * 10);	/* 10s */

	diag_init();

	// ethernet init
	tapdev_init();

	// Initialize the uIP TCP/IP stack.
	uip_init();

	uip_ipaddr(ipaddr, MYIP1,MYIP2,MYIP3,MYIP4);
	uip_sethostaddr(ipaddr);
	uip_ipaddr(ipaddr, DRTR1,DRTR2,DRTR3,DRTR4);
	uip_setdraddr(ipaddr);
	uip_ipaddr(ipaddr, SMSK1, SMSK2, SMSK3, SMSK4);
    uip_setnetmask(ipaddr);


	bl_init();


	while(1)
	{
		diag_appcall();
		bl_appcall();

		/* receive packet and put in uip_buf */
		uip_len = tapdev_read(uip_buf);
    	if(uip_len > 0)		/* received packet */
    	{
      		if(BUF->type == htons(UIP_ETHTYPE_IP))	/* IP packet */
      		{
	      		uip_arp_ipin();
	      		uip_input();
	      		/* If the above function invocation resulted in data that
	         		should be sent out on the network, the global variable
	         		uip_len is set to a value > 0. */

	      		if(uip_len > 0)
        		{
	      			uip_arp_out();
	        		tapdev_send(uip_buf,uip_len);
	      		}
      		}
	      	else if(BUF->type == htons(UIP_ETHTYPE_ARP))	/*ARP packet */
	      	{
	        	uip_arp_arpin();
		      	/* If the above function invocation resulted in data that
		         	should be sent out on the network, the global variable
		         	uip_len is set to a value > 0. */
		      	if(uip_len > 0)
	        	{
		        	tapdev_send(uip_buf,uip_len);	/* ARP ack*/
		      	}
	      	}
    	}
    	else if(timer_expired(&periodic_timer))	/* no packet but periodic_timer time out (0.5s)*/
    	{
      		timer_reset(&periodic_timer);

      		for(i = 0; i < UIP_CONNS; i++)
      		{
      			uip_periodic(i);
		        /* If the above function invocation resulted in data that
		           should be sent out on the network, the global variable
		           uip_len is set to a value > 0. */
		        if(uip_len > 0)
		        {
		          uip_arp_out();
		          tapdev_send(uip_buf,uip_len);
		        }
      		}
#if UIP_UDP
			for(i = 0; i < UIP_UDP_CONNS; i++) {
				uip_udp_periodic(i);
				/* If the above function invocation resulted in data that
				   should be sent out on the network, the global variable
				   uip_len is set to a value > 0. */
				if(uip_len > 0) {
				  uip_arp_out();
				  tapdev_send(uip_buf,uip_len);
				}
			}
#endif /* UIP_UDP */
	     	/* Call the ARP timer function every 10 seconds. */
			if(timer_expired(&arp_timer))
			{
				timer_reset(&arp_timer);
				uip_arp_timer();
			}
    	}


	}
	return 0 ;
}
