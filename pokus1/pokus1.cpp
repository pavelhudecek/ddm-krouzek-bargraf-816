#include <avr/io.h>
#define F_CPU (20000000UL/6)
#include <util/delay.h>

#define SW_port		PORTB
#define SW_bm		(1<<4)

#define LED_port	PORTC
#define LEDTR_port	PORTB
#define LEDTR_Rbp	3
#define LEDTR_Ybp	2
#define LEDTR_Gbp	1
#define LEDTR_ABbm	(1<<0)

#define TLAC_port	PORTA
#define TLAC_bm		(1<<0)

#define MX_rychlost	1

void ledky (unsigned int a, int b) {
	for (char n=0; n<3; n++) {
		LEDTR_port.OUTCLR = 0b1111;
		LEDTR_port.OUTSET = 1 << (LEDTR_Gbp + n);
		
		// (0b101011001111 >> 4) & 0b00001111
	    LED_port.OUTSET = (a >> n * 4) & 0b1111;
		_delay_ms(MX_rychlost);
		LED_port.OUTCLR = 0b1111;
		_delay_ms (MX_rychlost);	
	}
	for (char n=0; n<3; n++) {
		LEDTR_port.OUTCLR = 0b1111;
		LEDTR_port.OUTSET = 1 << (LEDTR_Gbp + n);
	    LEDTR_port.OUTSET = LEDTR_ABbm;
		
		LED_port.OUTSET = (b >> n * 4) & 0b1111;
		_delay_ms(MX_rychlost);
		LED_port.OUTCLR = 0b1111;
		_delay_ms (MX_rychlost);
	}
}


int main(void) {
	SW_port.DIR = SW_bm; // 0b00010000;
	
	LED_port.DIR = 0b1111;
	LEDTR_port.DIRSET = 0b1111;
	
    while (1) {
		int cRychlostPoohybu = 50;
		
		SW_port.OUTTGL = SW_bm;
		
		//ledky(0b10011011, 0b1110011111);
		
		for (int x=0; x<12; x++)
			for (int i=0; i<cRychlostPoohybu/12; i++) ledky (1<<x, 2048>>x);
		
		for (int x=11; x>=0; x--)
			for (int i=0; i<cRychlostPoohybu/12; i++) ledky (1<<x, 2048>>x);
		
    }
}