/*******************************************************************************
Low Voltage Cutoff firmware


Voltage divider values for different cell configurations:
1S (3v):
	R1	= 120k
	R2	= 68k
	Ivd	= 16uA
	Co	= 3.04v
2S (6v):
	R1	= 330k
	R2	= 75k
	Ivd	= 15uA
	Co	= 5.94v
3S (9v):
	R1	= 330k
	R2	= 47k
	Ivd	= 23uA
	Co	= 8.82v
4S (12v):
	R1	= 820k
	R2	= 82k
	Ivd	= 13uA
	Co	= 12.1v

Note:
	Supply voltage is fed to R1.  R2 is connected to GND.  Output is between R1 & R2.
		^
		|
		8 (r1)
		|
		|-----> (vdiv out)
		|
		8 (r2)
		|
		GND

*******************************************************************************/


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>



#define SWITCH_OUT	PB4


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// setup the chip
void init(void)
{
	// configure sleep mode
	sleep_enable();
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);

	// disable peripherals
	TCCR0A	= 0;				// disable Timer0
	TCCR0B	= 0;
	ADCSRA	= 0;				// disable ADC

	// prepare the analog comparator
	ADCSRB	&= ~(1<<ACME);
	ACSR	=	(0<<ACD) |		// enable comparator
				(1<<ACBG) |		// use 1.1v bandgap reference
				(0<<ACIE) |		// disable interrupt
				(0<<ACIS1) |	// interrupt on change
				(0<<ACIS0);
	DIDR0	=	(1<<AIN1D) |	// disable digital input
				(1<<AIN0D);

	// prepare switch output pin
	DDRB	|= (1<<SWITCH_OUT);
	PORTB	|= (1<<SWITCH_OUT);

	// prepare watch-dog timer
	MCUSR &= ~(1<<WDRF);

	// write logical one to WDCE and WDE
	// keep old prescaler setting to prevent unintentional time-out
	WDTCR |= (1<<WDCE) | (1<<WDE);

	// setup watchdog timer for interrupt mode on
	// 1s intervals (pg 42 attiny13 datasheet)
	WDTCR =	(1<<WDTIE)	|		// configure WDT for interrupt mode (pg 41)
			(0<<WDE)	|		//
			(0<<WDP3)	|		// set WD prescaler to 1s intervals
			(1<<WDP2)	|
			(1<<WDP1)	|
			(0<<WDP0);

	sei();
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Main loop
int main(void)
{
	init();

	while(1)
	{
		// sleep forever - we only awaken for the WDT
		sleep_cpu();
	}

	return 0;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Watch dog interrupt handler, running on 1s interval
ISR(WDT_vect)
{
	wdt_reset();

	/*
	Datasheet, page 78:
	The Analog Comparator compares the input values on the positive pin AIN0 and negative pin
	AIN1. When the voltage on the positive pin AIN0 is higher than the voltage on the negative pin
	AIN1, the Analog Comparator output, ACO, is set.

	AIN0 > AIN1	  ACO
	----   ----	  ----
	 1		0	   1
	 0		1	   0

	AIN1 (- pin) is connected to the voltage divider.
	AIN0 (+ pin) is connected to the internal 1.1v reference

	When ACO is set, AIN0 > AIN1.  AIN1 is too low, meaning the battery voltage is
	too low.  Turn off the MOSFET when ACO is set.  Since we have a P-CHANNEL mosfet,
	ACO can directly drive the state of the MOSFET switch.
	*/

	if ((ACSR & (1<<ACO)) != 0)		// ACO is 0
		PORTB |= (1<<SWITCH_OUT);	// Turn off P-CHANNEL MOSFET
	else
		PORTB &= ~(1<<SWITCH_OUT);	// Turn on P-CHANNEL MOSFET
}
