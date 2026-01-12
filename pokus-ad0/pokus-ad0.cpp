#include <avr/io.h>
#include <avr/interrupt.h>
#define F_CPU (20000000UL)
#include <util/delay.h>

#include <stdlib.h> // tohle je kvùli rand a srand
//kvùli constexpr je do c++ compiller / miscellanous pøidáno: -std=c++11

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
#define TLAC_chan	ADC_MUXPOS_AIN0_gc

#define PIP_port	PORTA
#define PIP_bm		(1<<3)

#define F_CPUkor	(F_CPU + ((F_CPU * (int8_t)SIGROW.OSC20ERR5V) / 1024L))
#define uartBaud(br) ((float)(F_CPUkor*64 / (16*(float)br)) + 0.5)

#define UART_bd		19200

volatile uint32_t	ms10 = 0;
volatile uint8_t	msSync = 0, sekSync = 0;

constexpr float	R_par(float Ra, float Rb) {return (Ra*Rb) / (Ra+Rb);}

void meritStart(uint8_t chan, uint8_t ref=255) { // ===========================
	ADC0.MUXPOS = chan;
	if (ref!=255) {
		VREF.CTRLA = ref;
		ADC0_CTRLC = ADC_REFSEL_INTREF_gc + ADC_PRESC_DIV64_gc;
		} else {
		ADC0_CTRLC = ADC_REFSEL_VDDREF_gc + ADC_PRESC_DIV64_gc;
	}
	ADC0.INTFLAGS = ADC_RESRDY_bm;
	ADC0.COMMAND=ADC_STCONV_bm;
}

void meritRestart() { // ======================================================
	ADC0.INTFLAGS = ADC_RESRDY_bm;
	ADC0.COMMAND=ADC_STCONV_bm;
}

bool atomMsTest(uint32_t val) { // ============================================
	__asm("cli");
	bool t = ms10<val;
	__asm("sei");
	return t;
}

void atomMs0() { // ===========================================================
	__asm("cli");
	ms10=0;
	__asm("sei");
}

const uint8_t TLAC_cnt  = 4;
const uint8_t TLAC_none = 0;
const uint8_t TLAC_tl1  = 1<<0;
const uint8_t TLAC_tl2  = 1<<1;
const uint8_t TLAC_tl3  = 1<<2;
const uint8_t TLAC_tl4  = 1<<3;
const uint8_t TLAC_all  = 15;

uint8_t tlacitka = TLAC_none;	// stav tlaèítek
uint16_t tlacADval = 0;			// ADC pro kontrolu
uint8_t tlacTmpVal = TLAC_none;	// stav tlaèítek pro kontrolu
uint16_t adcSPS = 0;

void cekej(uint32_t t) { // ===================================================
	uint32_t delayMs10 = t*10UL; // máme 10 kHz
	static uint16_t 
	
	// ms10 = 0;
	atomMs0();
	do {
		if (ADC0.INTFLAGS & ADC_RESRDY_bm) { // je domìøeno
			if (ADC0.MUXPOS == TLAC_chan) { // bylo to od tlaèítek
				const uint8_t TLAC_undef = TLAC_all+1;
				const uint16_t TLAC_toler = 25*64;

				// real:     0, 647, 794, 859 (, 1022)
				// teoretic: 0, 511, 681, 767 (, 1023)
				const uint16_t TLAC_vals[TLAC_cnt+1] = { // [1023*64] z Vdd, nezavisi na UPDI
					TLAC_toler, 647*64, 794*64, 859*64, 65535-TLAC_toler
				};
				// [TLAC_cnt+1] - TLAC_none je navíc
				const uint8_t TLAC_outs[TLAC_cnt+1] = {
					TLAC_tl1, TLAC_tl2, TLAC_tl3, TLAC_tl4, TLAC_none
				};

				tlacADval = ADC0.RES;
				meritRestart();
				
				static uint8_t	tlacTmp = TLAC_none;
				static uint8_t	tlacLast = TLAC_none;
				static uint8_t	tlacCnt = 0;
				uint8_t n;
				for (n=0; n<TLAC_cnt+1; n++) {
					if (tlacADval >= TLAC_vals[n]-TLAC_toler && tlacADval <= TLAC_vals[n]+TLAC_toler) break;
				}
				if (n>=TLAC_cnt+1) tlacTmp = TLAC_undef;
				else tlacTmp = TLAC_outs[n];
					
				tlacTmpVal = tlacTmp;
				if (tlacTmp != tlacLast || tlacTmp == TLAC_undef) tlacCnt=0;
				if (tlacCnt>30) {
					tlacitka = tlacTmp;
				} else {
					tlacCnt++;
				}
				tlacLast = tlacTmp;
			} else {
				// kdyby byl jinej kanál
			}	
		}
		
		if (msSync==1) { // -----------------------------------------
			msSync=0;
			// pøibližnì každou milisekundu
		}
			
		if (sekSync==1) { // ----------------------------------------
			sekSync=0;
			// pøibližnì každou sekundu
		}
		
	} while (atomMsTest(delayMs10)); // (ms10 < delayMs10)
}

uint8_t cekejNaTl(uint8_t mask) { // ==========================================
	while ((tlacitka & mask)) cekej(0);
	while (!(tlacitka & mask)) cekej(0);
	return tlacitka;
}

int ledA = 0;
int ledB = 0;
void ledky (int a, int b) { // ================================================
	ledA = a;
	ledB = b;
}

void bargrafy(int a, int b) { // ==============================================
	int A = (a<=12) ? a : 12;
	int B = (b<=12) ? b : 12;
	
	ledky((a<=12) ? 4095 >> 12-a : 4095-1024, (b<=12) ? 4095 >> 12-B : 4095-1024);
}

volatile int pipCnt = 0;
uint8_t		 pipPer = 4; // poèet tikù 10 kHz na pùlvlnu

// t [ms], f [Hz]
void pipat(int t, int f) { // ================================================
	pipPer = 5000 / f;
	pipCnt = t * 10 / pipPer;
	PIP_port.OUTSET = PIP_bm;
}
void pipat(int t) {
	pipat(t, 2000);
}

void putchar2(char c) { // ====================================================
	while ((USART0.STATUS & USART_DREIF_bm) == 0);
	USART0.TXDATAL=c;
}

void print(char t[]) { // text ================================================
	uint8_t n;
	
	for (n=0; n<255; n++) {
		if (t[n]==0) break;
		putchar2(t[n]);
	}
}
void print(long xx, int8_t m=0) { // cislo, mist (0 auto) =====================
	char t[10];
	signed char n=0;
	long x;
	
	x=xx;
	if (x<0) {x=-xx; putchar2('-');}
	for (n=0; n<9; n++) {
		t[n]=x % 10 + '0';
		x/=10;
		if (m<=0 && x==0) break;
	}
	if (m<0 && n<-m) {
		print("0.");
		for (uint8_t i=1; i<-m-n; i++) putchar2('0');
	}
	for (n=(m>0)? m-1: n; n>=0; n--) {
		putchar2(t[n]);
		if (m<0 && n==-m) putchar2('.');
	}
}
void print(float val, uint8_t mi) { // cislo, des.mist ========================
	float v = val;
	for (uint8_t n=0; n<mi; n++) v*=10.0;
	print(static_cast<long>(v), -mi);
}

uint32_t merit(uint8_t chan, uint32_t div=128UL, uint16_t vzorku=128, uint8_t ref=VREF_ADC0REFSEL_4V34_gc) {
	ADC0.MUXPOS = chan;
	if (ref!=255) {
		VREF.CTRLA = ref;
		ADC0_CTRLC = ADC_REFSEL_INTREF_gc + ADC_PRESC_DIV64_gc;
	} else {
		ADC0_CTRLC = ADC_REFSEL_VDDREF_gc + ADC_PRESC_DIV64_gc;
	}
	uint32_t sum = 0;
	for (uint16_t n=0; n<vzorku/64; n++) {
		ADC0.INTFLAGS = ADC_RESRDY_bm;
		ADC0.COMMAND=ADC_STCONV_bm;
		while(!(ADC0.INTFLAGS & ADC_RESRDY_bm)) cekej(0);
		sum += ADC0.RES;
	}
	return sum / div;
}
float merit(uint8_t chan, float coef, uint16_t vzorku=128, uint8_t ref=VREF_ADC0REFSEL_4V34_gc) {
	uint32_t sum = merit(chan, 1UL, vzorku, ref);
	return static_cast<float>(sum) / static_cast<float>(vzorku) * coef;
}




int main(void) { // ###########################################################
	_PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, 0); // nedìlit CLK, bude 20 MHz
	// ------------------------------------------------------------------------
	SW_port.DIR = SW_bm; // 0b00010000;
	
	TLAC_port.DIRCLR = TLAC_bm;
	TLAC_port.PIN0CTRL = PORT_ISC_INPUT_DISABLE_gc; //PORT_ISC_INTDISABLE_gc;
	
	LED_port.DIR = 0b1111;
	LEDTR_port.DIRSET = 0b1111;
	
	PORTA.DIRSET = 1<<1;				// TxD
	PORTA.PIN2CTRL = PORT_PULLUPEN_bm;	// RxD
	
	TCA0.SINGLE.CTRLA = 0;
	TCA0.SINGLE.CTRLB = 0;
	TCA0.SINGLE.CTRLC = 0;
	TCA0.SINGLE.CTRLD = 0;
	TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
	TCA0.SINGLE.PER = F_CPU/10000UL; // perioda bude 10 kHz
	TCA0.SINGLE.CTRLA = TCA_SINGLE_ENABLE_bm;
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
	// ---------------------------------------------------------------------------------------------
	// sériový port
	PORTMUX.CTRLB|=PORTMUX_USART0_ALTERNATE_gc;		// RxD - PORTA.2, TxD - PORTA.1
	USART0.CTRLA = 0;								// tx bez int, rx bez int, 485 off
	USART0.CTRLC = USART_CMODE_ASYNCHRONOUS_gc + USART_CHSIZE_8BIT_gc;
	USART0.BAUD = uartBaud(UART_bd);
	USART0.CTRLB = USART_TXEN_bm;
	// ------------------------------------------------------------------------
	VREF_CTRLA = VREF_ADC0REFSEL_0V55_gc;				// 0,55 V
	VREF_CTRLB = VREF_ADC0REFEN_bm;						// always on
	// ------------------------------------------------------------------------
	ADC0_CTRLA = ADC_RUNSTBY_bm + ADC_FREERUN_bm + ADC_ENABLE_bm;	// standby run on, freerun on, enable
	ADC0_CTRLB = ADC_SAMPNUM_ACC64_gc;					// zapnout akumulaci 64 vzorkù
	ADC0_CTRLC = ADC_REFSEL_INTREF_gc + ADC_PRESC_DIV64_gc;
	ADC0_CTRLD = 0; //ADC_INITDLY_DLY16_gc + (2<<ADC_SAMPDLY_gp); // init delay 16, samp delay 2?
	ADC0_CTRLE=0;										// No Window Comparison (default)
	ADC0_SAMPCTRL=0;									// Sample Length = 2+reg
	ADC0_INTCTRL = 0;//ADC_RESRDY_bm;					// no int
	ADC0_DBGCTRL=0;										// not work in break
	ADC0_CALIB = ADC_DUTYCYC_DUTY25_gc;					// 25% Duty Cycle (high 25% and low 75%) must be used for ADCclk ? 1.5 MHz
	// ---------------------------------------------------------------------------------------------
		
	_delay_ms (200); // pokud by náhodou hned na zaèátku bylo pípání, nejde programovat pøes xnano
	__asm("sei");
	
	meritStart(TLAC_chan); // zahájit mìøení na kanálu tlaèítka
	
	int a=1;
	int b=1;
	
	while (1) {
		//uint32_t x = merit(TLAC_chan, 1280UL, 1280, 255);
		//uint32_t x = merit(TLAC_chan, 64UL, 64000, 255);
		//a = static_cast<int>(x/(1024000/12));
		a = tlacitka;
		//b = tlacADval/(64 * TLAC_adCyklu * 1023/12);
		bargrafy(a, b);
		//ledky(4095>>(12-a), 1<<b);
		if (++b>11) {b=0; print("\n");}
			
		print(tlacADval); print(" ");
		print(tlacTmpVal); print(" / ");

		cekej(30);
		continue;
		
		
		
		//ledky(0b10011011, 0b1110011111);
		
		/*while (TLAC_port.IN & TLAC_bm);
		while (!(TLAC_port.IN & TLAC_bm)) a++;
		srand(a);
		
		for (int n=0; n<50; n++) {
			cekej(100);
			a = rand() % 13;
			b = rand() % 13;
			bargrafy(a, b);
			
			pipat(10);
		}
		pipat(50, 500);*/
		for (uint8_t n=0; n<200; n++) {
			cekej(10);
			//bargrafy(static_cast<int>(adData[1] * 50), static_cast<int>(adData[2] * 50));
			//bargrafy(adTest[1] / (640 ), tlacitka);
		}
		
		//print("bb \n");

		
		//print(adTlacSample); putchar2('\t');
		//print(tlacitka);	 putchar2('\n');
	}
}

const uint8_t cUsartRxBufLen = 44;
volatile char usartRxBuff[cUsartRxBufLen];
volatile uint8_t usartRxBuffIend = 0;	// index za poslední pøijatý znak
volatile uint8_t usartRxMsgComplete = 0;// 1 dokonèen pøíjem (pøijata mezera)
volatile uint8_t usartRxMsgFail = 0;	// 1 buff overflow / 2 timeout
volatile uint8_t usartRxMsgTout = 255;	// 0-253 bìží, 254 tout err, 255 zastaveno

ISR (USART0_RXC_vect) { // ====================================================

	USART0.STATUS |= USART_RXCIF_bm;
}

// pøerušení od pøeteèení timeru TCA0
ISR (TCA0_OVF_vect) { // ======================================================
	static char ledIdx = 0;
	static char pipIdx = 0;
	
	ms10++;
	msSync = 1;
	if (++pipIdx > pipPer) {
		pipIdx = 0;
		if (pipCnt>0) {
			pipCnt--;
			PIP_port.DIRTGL = PIP_bm;
		} else {
			PIP_port.DIRCLR = PIP_bm;
		}
	}
	
	LED_port.OUTCLR = 0b1111;
	LEDTR_port.OUTCLR = 0b1111;
	LEDTR_port.OUTSET = 1 << (LEDTR_Gbp + (ledIdx % 3));
	
	if (ledIdx < 3) {
		LEDTR_port.OUTCLR = LEDTR_ABbm;
		LED_port.OUTSET = (ledB >> ledIdx * 4) & 0b1111;
	} else if (ledIdx < 6) {
		LEDTR_port.OUTSET = LEDTR_ABbm;
		LED_port.OUTSET = (ledA >> (ledIdx - 3) * 4) & 0b1111;
	}
	
	if (++ledIdx > 50) ledIdx=0;
	
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

// pøerušení od ADC - 23796 kS/s / 374 S/s pøi ACC64 / 8 ch + 3 ign, 10 cyk - 374/110=3,4 c/s
ISR (ADC0_RESRDY_vect) { // ===================================================

	ADC0.INTFLAGS = ADC_RESRDY_bm;
}
