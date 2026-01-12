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

#define PIP_port	PORTA
#define PIP_bm		(1<<3)

#define F_CPUkor	(F_CPU + ((F_CPU * (int8_t)SIGROW.OSC20ERR5V) / 1024L))
#define uartBaud(br) ((float)(F_CPUkor*64 / (16*(float)br)) + 0.5)

#define UART_bd		19200

const uint8_t TLAC_cnt  = 4;
const uint8_t TLAC_none = 0;
const uint8_t TLAC_tl1  = 1<<0;
const uint8_t TLAC_tl2  = 1<<1;
const uint8_t TLAC_tl3  = 1<<2;
const uint8_t TLAC_tl4  = 1<<3;
const uint8_t TLAC_all  = 15<<1;

const uint8_t TLAC_chan = 1;

const uint16_t TLAC_vals[TLAC_cnt] = { // % z 5 V, UPDI nezap
	0, 63, 77, 84
};
const uint16_t TLAC_valsU[TLAC_cnt] = { // % z 5 V, UPDI zapojeno (xnano)
	//0, 71, 83, 88
	// 65460
	0+20, 46620, 54500, 57755
};

volatile uint32_t	ms10 = 0;
volatile uint8_t	msSync = 0, sekSync = 0, adSync = 0;

#define ADC_prescaller ADC_PRESC_DIV64_gc

const uint8_t	ADC_mxMask = 31;
const uint8_t	ADC_mxRefVcc = 255;
const uint8_t	ADC_mxRef05 = 255-1;
const uint8_t	ADC_mxRef11 = 255-2;
const uint8_t	ADC_mxRef15 = 255-3;
const uint8_t	ADC_mxRef25 = 255-4;
const uint8_t	ADC_mxRef43 = 255-5;

const uint8_t	ADC_chanCnt = 8;
const uint8_t	ADC_cycles = 10;
const uint8_t	ADC_chanTab[ADC_chanCnt] = { // ADC_MUXPOS_AIN0_gc = 0
	ADC_mxRefVcc,
	ADC_MUXPOS_AIN0_gc,	// tlaèítka
	ADC_mxRef25,
	4,	// 400 V
	7,	// x10
	5,	// 5 A
	ADC_mxRef11,
	8	// 1 V (BNC)
};
const float		ADC_samples = 64.0 * static_cast<float>(ADC_cycles);
const float		ADC_maxVal = 1023.0;
const float		ADC_toRef = 1.0 / (ADC_samples * ADC_maxVal);

constexpr float	R_par(float Ra, float Rb) {return (Ra*Rb) / (Ra+Rb);}

float			adConvCoefs[ADC_chanCnt] = {
	1.0,
	1.0 * ADC_toRef, // tlac
	1.0,
	(1.0+(1300.0*2.0)/R_par(30.0, 7.5)) * ADC_toRef * 2.5, // 400Vac
	(1.0+(27.0/3.0)) * ADC_toRef * 2.5, // x10
	0.018 * (1.0+(27.0/3.0)) * ADC_toRef * 2.5, // 5A
	1.0,
	ADC_toRef * 1.1 // 1V
};
volatile uint32_t adRawData[ADC_chanCnt];
float			adData[ADC_chanCnt];
uint32_t		adTest[ADC_chanCnt];

const uint16_t TLAC_sampSyncVal = 12345;
volatile uint16_t adTlacSample = TLAC_sampSyncVal;
uint8_t				tlacitka = TLAC_none;

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

void cekej(uint32_t t) { // ===================================================
	uint32_t delayMs = t*10UL; // máme 10 kHz
	atomMs0();
	do {
		if (msSync==1) { // -----------------------------------------
			msSync=0;
		}
			
		if (adTlacSample!=TLAC_sampSyncVal) { // --------------------
			static uint8_t tlacCnt = 0;
			static uint8_t tlacLast = TLAC_none;
			uint8_t tlacTmp = TLAC_none;
			static bool tlacSemafor = 0;
			
			for (uint8_t n=0; n<TLAC_cnt; n++) {
				uint16_t tv = TLAC_valsU[n];
				if (adTlacSample >= tv-20 && adTlacSample <= tv+20) {
					tlacTmp |= TLAC_tl1 << n;
				}
			}
			adTlacSample = TLAC_sampSyncVal;
			if (tlacTmp != tlacLast) tlacCnt = 0;
			if (++tlacCnt>2) {
				tlacitka = tlacTmp;
			}
			tlacLast = tlacTmp;
			
			/*
			
			if (!tlacSemafor) if (++tlacCnt>50) {
				tlacSemafor = 1; // semafor kvùli pip
				#ifndef EEP_plus
				// zaèal stisk TL..
				if (tlacitka == TLAC_none && tlacTmp == TLAC_tl1) { // pøepnout p-seq
					progPrepnout=1;
					skipCekani=1;
				}
				uint32_t tmp = krokCas;
				if (tlacitka == TLAC_none && tlacTmp == TLAC_tl2) { // zrychlit p-seq
					tmp *= 10000;
					tmp /= 11892; // 2^(1/4) = 1,18920711..
					//tlacSemafor = 1;
					if (tmp >= KROK_min) {
						krokCas = static_cast<uint16_t>(tmp);
						pip(1);
						} else {
						krokCas = KROK_min;
						pip(2);
					}
					eeprom_update_word(&eKrokCas, krokCas);
					//tlacSemafor = 0;
				}
				if (tlacitka == TLAC_none && tlacTmp == TLAC_tl3) { // zpomalit p-seq
					tmp *= 11892; // 2^(1/4) = 1,18920711..
					tmp /= 10000;
					//tlacSemafor = 1;
					if (tmp <= KROK_max) {
						krokCas = static_cast<uint16_t>(tmp);
						pip(1);
						} else {
						krokCas = KROK_max;
						pip(2);
					}
					eeprom_update_word(&eKrokCas, krokCas);
					//tlacSemafor = 0;
				}
				#endif
				tlacSemafor = 0;
				tlacitka = tlacTmp;
			}
			tlacLast = tlacTmp;*/
		}
			
		if (sekSync==1) { // ----------------------------------------
			sekSync=0;

		}
		if (adSync==1) { // -----------------------------------------
			
			for (uint8_t n=0; n<ADC_chanCnt; n++) {
				adTest[n] = adRawData[n];
				//if (n==TLAC_chan) continue;
				adData[n] = static_cast<float>(adRawData[n]) * adConvCoefs[n];
				adRawData[n] = 0;
			}
			adSync=0;
			SW_port.OUTTGL = SW_bm;
		}
		
	} while (atomMsTest(delayMs));
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
	PORTMUX.CTRLB|=PORTMUX_USART0_ALTERNATE_gc;			// RxD - PORTA.2, TxD - PORTA.1
	USART0.CTRLA = USART_RXCIE_bm;						// tx bez int, rx int, 485 off
	USART0_CTRLC = USART_CMODE_ASYNCHRONOUS_gc + USART_CHSIZE_8BIT_gc;
	USART0_BAUD = uartBaud(UART_bd);
	USART0.STATUS |= USART_RXCIF_bm;					// konec IF
	USART0_CTRLB = USART_RXEN_bm + USART_TXEN_bm;		// tx en, rx en
	// ------------------------------------------------------------------------
	VREF_CTRLA = VREF_ADC0REFSEL_0V55_gc;				// 0,55 V
	VREF_CTRLB = VREF_ADC0REFEN_bm;						// always on
	// ------------------------------------------------------------------------
	ADC0_CTRLA = ADC_RUNSTBY_bm + ADC_FREERUN_bm + ADC_ENABLE_bm;	// standby run on, freerun on, enable
	ADC0_CTRLB = ADC_SAMPNUM_ACC64_gc;					// zapnout akumulaci 64 vzorkù
	ADC0_CTRLC = ADC_REFSEL_INTREF_gc + ADC_prescaller;
	ADC0_CTRLD = 0; //ADC_INITDLY_DLY16_gc + (2<<ADC_SAMPDLY_gp); // init delay 16, samp delay 2?
	ADC0_CTRLE=0;										// No Window Comparison (default)
	ADC0_SAMPCTRL=0;									// Sample Length = 2+reg
	ADC0_MUXPOS=ADC_chanTab[0];							// ADC input pin
	ADC0_COMMAND=ADC_STCONV_bm;							//?Start Conversion
	ADC0_EVCTRL=0;										// no events
	ADC0_INTCTRL = ADC_RESRDY_bm;						// int result ready
	ADC0_INTFLAGS = ADC_RESRDY_bm;						// clear IF
	ADC0_DBGCTRL=0;										// not work in break
	ADC0_CALIB = ADC_DUTYCYC_DUTY25_gc;					// 25% Duty Cycle (high 25% and low 75%) must be used for ADCclk ? 1.5 MHz
	// ---------------------------------------------------------------------------------------------
	
	
	
	__asm("sei");
	
	_delay_ms (200); // pokud by náhodou hned na zaèátku bylo pípání, nejde programovat pøes xnano
	
	int a=1;
	int b=1;
	ADC0.CTRLC = ADC_SAMPCAP_bm + ADC_REFSEL_VDDREF_gc + ADC_prescaller;
	
	while (1) {
		
		
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
			bargrafy(adTest[1] / (640 ), tlacitka);
		}
		
		//print("bb \n");
		for (uint8_t n=0; n<ADC_chanCnt; n++) {
			print(n); putchar2(' ');
			print(static_cast<float>(adTest[n] * ADC_toRef * 100.0), 1); print(" % ");
			print((adTest[n]));
			putchar2('\t');
		}
		print(adTlacSample); putchar2('\t');
		print(tlacitka);	 putchar2('\n');
	}
}

const uint8_t cUsartRxBufLen = 44;
volatile char usartRxBuff[cUsartRxBufLen];
volatile uint8_t usartRxBuffIend = 0;	// index za poslední pøijatý znak
volatile uint8_t usartRxMsgComplete = 0;// 1 dokonèen pøíjem (pøijata mezera)
volatile uint8_t usartRxMsgFail = 0;	// 1 buff overflow / 2 timeout
volatile uint8_t usartRxMsgTout = 255;	// 0-253 bìží, 254 tout err, 255 zastaveno

ISR (USART0_RXC_vect) { // ====================================================
	ledky(4095,0);
	if (usartRxMsgComplete==0) {
		if (usartRxBuffIend==0) usartRxMsgTout=0;
		if (usartRxBuffIend>=cUsartRxBufLen-1) {
			usartRxMsgTout=255;
			usartRxBuffIend=255;
			usartRxMsgFail |= 1;
		}
		usartRxBuff[usartRxBuffIend] = USART0.RXDATAL;
		usartRxBuffIend++;

		if (usartRxBuff[usartRxBuffIend-1]==' ') {
			usartRxMsgComplete=1;
			usartRxMsgTout = 255;
		}
		} else {
		volatile char dummy = USART0.RXDATAL;
	}
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
	static uint8_t mxIdx=0, cykly=0;
	static uint8_t ignore = 3; // tohle je divný, normálnì staèilo ignorovat 1
	
	if (ignore!=0) {
		ignore--;
		ADC0.INTFLAGS = ADC_RESRDY_bm;
		return;
	}
	if (mxIdx==TLAC_chan && adTlacSample==TLAC_sampSyncVal) adTlacSample = ADC0.RES;
	if (adSync==0) {
		adRawData[mxIdx] += static_cast<uint32_t>(ADC0.RES);
		
		if (++mxIdx>=ADC_chanCnt) {
			mxIdx=0;
			if (++cykly>=ADC_cycles) {cykly=0; adSync=1;}
		}
		//ADC0.MUXPOS = ADC_chanTab[mxIdx];
		ignore = 2;
		
		if (ADC_chanTab[mxIdx] <= ADC_mxMask) {
			ADC0.MUXPOS = ADC_chanTab[mxIdx];
		} else switch (ADC_chanTab[mxIdx]) {
			case ADC_mxRefVcc:
				ADC0.CTRLC = ADC_SAMPCAP_bm + ADC_REFSEL_VDDREF_gc + ADC_prescaller;
				break;
			case ADC_mxRef05:
				ADC0.CTRLC = ADC_REFSEL_INTREF_gc + ADC_prescaller;
				VREF.CTRLA = VREF_ADC0REFSEL_0V55_gc;
				break;
			case ADC_mxRef11:
				ADC0.CTRLC = ADC_SAMPCAP_bm + ADC_REFSEL_INTREF_gc + ADC_prescaller;
				VREF.CTRLA = VREF_ADC0REFSEL_1V1_gc;
				break;
			case ADC_mxRef15:
				ADC0.CTRLC = ADC_SAMPCAP_bm + ADC_REFSEL_INTREF_gc + ADC_prescaller;
				VREF.CTRLA = VREF_ADC0REFSEL_1V5_gc;
				break;
			case ADC_mxRef25:
				ADC0.CTRLC = ADC_SAMPCAP_bm + ADC_REFSEL_INTREF_gc + ADC_prescaller;
				VREF.CTRLA = VREF_ADC0REFSEL_2V5_gc;
				break;
			case ADC_mxRef43:
				ADC0.CTRLC = ADC_SAMPCAP_bm + ADC_REFSEL_INTREF_gc + ADC_prescaller;
				VREF.CTRLA = VREF_ADC0REFSEL_4V34_gc;
				break;
			default:
				ADC0.MUXPOS = ADC_MUXPOS_GND_gc;
		}
	}
	ADC0.INTFLAGS = ADC_RESRDY_bm;
}