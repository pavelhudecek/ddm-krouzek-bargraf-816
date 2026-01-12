#include <avr/io.h>
#define F_CPU (20000000UL/6UL)
#include <util/delay.h>
#include <avr/interrupt.h>

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

int ledA;
int ledB;

void ledky (int a, int b) {
	ledA = a;
	ledB = b;
}

void bargrafy(int a, int b) {
	ledky(4095 >> 12-a, 4095 >> 12-b);
}


int main(void) {
	SW_port.DIR = SW_bm; // 0b00010000;
	
	LED_port.DIR = 0b1111;
	LEDTR_port.DIRSET = 0b1111;
	
	TCA0.SINGLE.CTRLA = 0;
	TCA0.SINGLE.CTRLB = 0;
	TCA0.SINGLE.CTRLC = 0;
	TCA0.SINGLE.CTRLD = 0;
	TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
	TCA0.SINGLE.PER = F_CPU/10000UL;
	TCA0.SINGLE.CTRLA = TCA_SINGLE_ENABLE_bm;
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
	
	__asm("sei");
	
	 int a=1;
	 int b=1;
	while (1) {
		SW_port.OUTTGL = SW_bm;
		
		//ledky(0b10011011, 0b1110011111);
       		
		if (!(TLAC_port.IN & TLAC_bm)) {
			bargrafy(a, b);
			a++;
			b++;
			_delay_ms (100);
		}
		 if (a==13) {
			 _delay_ms (100);
			 a=1;
			 b=1;
			 _delay_ms (100);
		 }
	}
}
        

ISR (TCA0_OVF_vect) {
	static char n = 0;
	
	LED_port.OUTCLR = 0b1111;
	LEDTR_port.OUTCLR = 0b1111;
	LEDTR_port.OUTSET = 1 << (LEDTR_Gbp + (n % 3));
	
	if (n > 2) {
		LEDTR_port.OUTSET = LEDTR_ABbm;
		LED_port.OUTSET = (ledA >> (n - 3) * 4) & 0b1111;
	} else if (n<6) {
		LEDTR_port.OUTCLR = LEDTR_ABbm;
		LED_port.OUTSET = (ledB >> n * 4) & 0b1111;
	} else {
		// 6-25 zhasnuto
	}
	
	n++;
	if (n > 25) n=0;
	
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}