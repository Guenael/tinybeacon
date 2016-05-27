/* 
 * FreeBSD License
 * Copyright (c) 2016, Guenael 
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */


#include "cpu.h"

#include "twi.h"
#include "gps.h"
#include "pll.h"
#include "usart.h"

#include "morse.h"
#include "pi4.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
 


int main (void) {
	/* CKDIV8 fuse is set -- Frequency is divided by 8 at start : Manually update to 4MHz */
	cli();
	CLKPR = _BV(CLKPCE); // Enable change of CLKPS bits
	CLKPR = _BV(CLKPS1); // Set prescaler to 4 = System clock 2.5 MHz
	sei();

    /* DEBUG Enable I2C output */
    DDRC   |= _BV(DDC4);  // Enable output
    DDRC   |= _BV(DDC5);  // Enable output

	/* LED : Set pin 11 of PORT-PD7 for output*/
	DDRD |= _BV(DDD7);

	/* Peform the sub-modules initialisation */
	twi_init();
	_delay_ms(10);

	/* Enable GPS for time sync, osc & locator */
	gpsInit();  // I2C Have to be init before the PLL !
	_delay_ms(10);

	/* Init for the AD PLL */
	pllInit();
	_delay_ms(10);
	
	/* Set the default I2C address of the PLL */
	gpsSetAddr(0x42);
	_delay_ms(10);

	/* For now, used for DEBUG purpose only. Future : CLI for freq settings & modes */
	usartInit();
	_delay_ms(10);

	/* uBlox GPS init & settings */
	gpsSetPulseTimer();
	_delay_ms(10);

	/* ADF4355-2 init & settings */
	pllProgramInit();
	_delay_ms(10);

	/* Prepare the message to encode for PI4 message */
	pi4Encode();

	/* Get the GPS postion & calculate the locator (NO dynamic update) */
	gpsGetNMEA();
	gpsExtractLocator();

	while(1) {
	   	/* Start SEQ : Turn on the LED (pin 11) */
		PORTD |= _BV(PORTD7);

	    /* 1st part : Send PI4 message, 25 sec */
	    pi4Send();

	    /* 2nd part : Send morse message */
		morse2TonesSendMessage();

		/* 3th part : Send a carrier, 10 sec, same frequency */
		pllUpdate(1);
		pllPA(1);
    	pllRfOutput(1);
        _delay_ms(10000);
	    pllRfOutput(0);
    	pllPA(0);

		/* End SEQ : Turn off the LED (pin 11) */
		PORTD &= ~_BV(PORTD7);

	    /* Update the GPS data for the next time align */
	    gpsGetNMEA();
	    gpsExtractStrings();
	    
	    /* Align on an minute for the next message */
	    gpsTimeAling1M();

	}

	/* This case never happens :) Useless without powermanagement... */
	return 0;
}