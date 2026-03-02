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

void ledky (int a, int b) { // ======================================
	ledA = a;
	ledB = b;
}

void bargrafy(int a, int b) { // ====================================
	ledky(4095 >> 12-a, 4095 >> 12-b);
}

volatile uint16_t	ms10=0;
volatile char		sekSync=0, msSync=0;
char				ledJas = 20;

void cekej(uint16_t n) { // =========================================
	ms10=0;
	
	do {
		if (msSync==1) {
			msSync = 0;
			
			static int jas = 20 << 4;
			
			if (++jas > (30 << 4)) jas=0;
			
			ledJas = static_cast<char>(jas >> 4);
			
		}
		
		if (sekSync==1) {
			sekSync=0;
			
			SW_port.OUTTGL = SW_bm;
		}
		
		
		
	} while(ms10 < n*10);
}


int main(void) { // #################################################
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
		
		
		//ledky(0b10011011, 0b1110011111);
       		
		if (!(TLAC_port.IN & TLAC_bm)) {
			bargrafy(a, b);
			a++;
			b++;
			cekej (100);
		}
		cekej(0);
		if (a==13) {
			cekej (100);
			a=1;
			b=1;
			cekej (100);
		}
	}
}
        

ISR (TCA0_OVF_vect) { // ============================================
	static char n = 0;
	static int	sekCnt = 0;
	static char	msCnt = 0;
	
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
		// 6 az 6+ledJas zhasnuto
	}
	
	n++;
	if (n > 6+ledJas) n=0;
	
	
	ms10++;
	
	if (++sekCnt>10000) {
		sekCnt = 0;
		sekSync = 1;
	}
	if (++msCnt>10) {
		msCnt = 0;
		msSync = 1;
	}
	
	
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}