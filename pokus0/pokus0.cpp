// pokus0: Ovìøit, zda jsou všechny LEDky a tranzistory správnì zapojené

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

int main(void) {
	SW_port.DIR = SW_bm; // 0b00010000;
	
	LED_port.DIR = 0b1111;
	LEDTR_port.DIRSET = 0b1111;
	
    while (1) {
		for (uint8_t sloupec=0; sloupec<2; sloupec++) {
			LEDTR_port.OUTTGL = LEDTR_ABbm;
			
			for (uint8_t barva=0; barva<3; barva++) {
				LEDTR_port.OUTCLR = 255 - LEDTR_ABbm - SW_bm;
				LEDTR_port.OUTSET = 1 << LEDTR_Gbp + barva;
				
				LED_port.OUT = 0b1111;
				_delay_ms(1);
				LED_port.OUT = 0;
				_delay_ms(200);
			}
		}
		SW_port.OUTTGL = SW_bm;
    }
}

/* Úkoly:
1. Udìlat rychlý multiplex, aby oba bargafy na pohled celé svítily.
2. Udìlat aby se výraznì lišil jejich jas.
3. Udìlat funkci, která jednoduše ovládá všechny LEDky bitama ve 2 èíslech,
   nìco jako ledky(0b11100011, 0b0101010).
*/