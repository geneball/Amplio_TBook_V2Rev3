//*********************************  tbUtil.c
//  error handling and memory management utilities
//
//    fault handlers to replace dummies in startup_TARGET.s 
//
//	Nov2019 

#include "main.h"
#include "tbook.h"
#include "os_tick.h"

#if defined( USE_LCD )
#include "GLCD_Config.h"
#include "Board_GLCD.h"
#endif 

#include <stdlib.h>

// #include "cmsis_os2.h"		// for TestProc osMutex*

void 										setTxtColor( uint16_t c );
#define LCD_COLOR_RED           (uint16_t)0xF800
#define LCD_COLOR_BLACK         (uint16_t)0x0000
#define LCD_COLOR_BLUE          (uint16_t)0x001F

// GPIO utilities based on GPIO_ID enumeration & GPIO_Signal[] in main.h
GPIO_Def_t  						gpio_def[ MAX_GPIO ];														// array of signal definitions, indexed by GPIO_ID 
void 										gDef( GPIO_ID gpid, char *signal ){							// define gpio_signal[ ID ] from name e.g. "PC8"
	// GPIO_ID  signal  								eg   'PA4'   => GPIOA, pin 4, active high  
	//          "Pxdd_|cc" where:					e.g. 'PE12_' => GPIOE, pin 12, active low
	//             x  = A,B,C,D,E,F,G,H -- GPIO port
	//             dd = 0..15  -- GPIO pin #
	//             _  = means 'active low', i.e, 0 if pressed
	//						 cc = 0..15 -- alternate function code
	if ( gpio_def[ gpid ].id == gpid ) tbErr( "GPIO redefined" );
	
	GPIO_TypeDef *	port = NULL;  	// address of GPIO port -- from STM32Fxxxx.h
	IRQn_Type 			intnum = WWDG_IRQn;  // default 0
	uint16_t 				pin = 0;
	uint8_t					altFn = 0;
	int 						siglen = strlen( signal );
	int							active = 1;		// default to active high-- 1 when LOGICALLY active

	if ( siglen > 0 ){
		switch ( signal[1] ){
			case 'A':  port = GPIOA; break;	// 0x400200000
			case 'B':  port = GPIOB; break;	// 0x400200400
			case 'C':  port = GPIOC; break;	// 0x400200800
			case 'D':  port = GPIOD; break;	// 0x400200C00  ODR: 0x400200C14
			case 'E':  port = GPIOE; break;	// 0x400201000
			case 'F':  port = GPIOF; break;	// 0x400201400
			case 'G':  port = GPIOG; break;	// 0x400201800
		}
		pin = atoi( &signal[2] );		// stops at non-digit
		switch ( pin ){
			case 0:		intnum = EXTI0_IRQn;	break;
			case 1:		intnum = EXTI1_IRQn;	break;
			case 2:		intnum = EXTI2_IRQn;	break;
			case 3:		intnum = EXTI3_IRQn;	break;
			case 4:		intnum = EXTI4_IRQn;	break;
			case 5:		
			case 6:		
			case 7:		
			case 8:		
			case 9:		
				intnum = EXTI9_5_IRQn;	
				break;
			case 10:		
			case 11:		
			case 12:		
			case 13:		
			case 14:		
			case 15:		
				intnum = EXTI15_10_IRQn;
				break;
		}
		if ( strchr( signal, '_' ) != NULL )	// contains a _ => active low
			active = 0;	// '_' for active low input
		char *af = strchr( signal, '|' );
		if ( af != NULL ) // alternate function-- "|12" => AF=0xC
			altFn = atoi( &af[1] );
	}
	gpio_def[ gpid ].id 		= gpid;		// ID of this entry
	gpio_def[ gpid ].port 	= port;		// mem-mapped address of GPIO port
	gpio_def[ gpid ].pin 		= pin;		// pin within port
	gpio_def[ gpid ].altFn  = (AFIO_REMAP) altFn;	// alternate function index 0..15
	gpio_def[ gpid ].intq 	= intnum;	// EXTI int num
	gpio_def[ gpid ].signal = signal;	// string name of GPIO pin
	gpio_def[ gpid ].active = active;	// signal when active (for keys, input state when pressed)
}
void 										GPIO_DefineSignals( GPIO_Signal def[] ){
	for (int i=0; i < 1000; i++ ){
		if ( def[ i ].id == gINVALID ) return;	// hit end of list
		gDef( def[ i ].id, def[ i ].name );
	}
	tbErr( "too many GPIOs defined" );
}


void										gUnconfig( GPIO_ID id ){			// reset GPIO to default
#if defined( TBOOKREV2B )	
	GPIO_PinConfigure( gpio_def[ id ].port, gpio_def[ id ].pin, GPIO_MODE_INPUT, GPIO_OTYP_PUSHPULL, GPIO_SPD_LOW, GPIO_PUPD_NONE );
	GPIO_AFConfigure ( gpio_def[ id ].port, gpio_def[ id ].pin, AF0 );   // reset to AF0
//#undefine STM3210E_EVAL
#endif
#if defined( STM3210E_EVAL )
	GPIO_PinConfigure( gpio_def[ id ].port, gpio_def[ id ].pin, GPIO_IN_ANALOG, GPIO_MODE_INPUT );
#endif
}
void										gConfigI2S( GPIO_ID id ){		// config gpio as high speed pushpull in/out
#if defined( TBOOKREV2B )	
	GPIO_PinConfigure( gpio_def[ id ].port, gpio_def[ id ].pin, GPIO_MODE_AF, GPIO_OTYP_PUSHPULL, GPIO_SPD_FAST, GPIO_PUPD_NONE );
	GPIO_AFConfigure ( gpio_def[ id ].port, gpio_def[ id ].pin, gpio_def[ id ].altFn );   // set AF
#endif
#if defined( STM3210E_EVAL )
	GPIO_PinConfigure( gpio_def[ led ].port, gpio_def[ led ].pin, GPIO_OUT_PUSH_PULL, GPIO_MODE_OUT10MHZ );
#endif
}
void										gConfigOut( GPIO_ID id ){		// config gpio as low speed pushpull output -- power control, etc
#if defined( TBOOKREV2B )	
	AFIO_REMAP af = gpio_def[ id ].altFn;
	GPIO_MODE md = af!=0? GPIO_MODE_AF : GPIO_MODE_OUT;
	GPIO_PinConfigure( gpio_def[ id ].port, gpio_def[ id ].pin, md, GPIO_OTYP_PUSHPULL, GPIO_SPD_LOW, GPIO_PUPD_NONE );
	GPIO_AFConfigure ( gpio_def[ id ].port, gpio_def[ id ].pin, af );   // set AF
#endif
#if defined( STM3210E_EVAL )
	GPIO_PinConfigure( gpio_def[ led ].port, gpio_def[ led ].pin, GPIO_OUT_PUSH_PULL, GPIO_MODE_OUT10MHZ );
#endif
}
void										gConfigIn( GPIO_ID key, bool pulldown ){		// configure GPIO as low speed input, either pulldown or pullup
#if defined( TBOOKREV2B )	
	GPIO_PinConfigure( gpio_def[ key ].port, gpio_def[ key ].pin, GPIO_MODE_INPUT, GPIO_OTYP_PUSHPULL, GPIO_SPD_LOW, pulldown? GPIO_PUPD_PDN : GPIO_PUPD_PUP );
	GPIO_AFConfigure ( gpio_def[ key ].port, gpio_def[ key ].pin, AF0 );   //  AF 0
#endif
#if defined( STM3210E_EVAL )
	GPIO_PinConfigure( gpio_def[ key ].port, gpio_def[ key ].pin, GPIO_IN_FLOATING, GPIO_MODE_INPUT );
#endif
}
void										gConfigKey( GPIO_ID key ){		// configure GPIO as low speed pulldown input ( keys )
	gConfigIn( key, true );	  // pulldown
}
void										gConfigADC( GPIO_ID id ){		// configure GPIO as ANALOG input ( battery voltage levels )
#if defined( TBOOKREV2B )	
	GPIO_PinConfigure( gpio_def[ id ].port, gpio_def[ id ].pin, GPIO_MODE_ANALOG, GPIO_OTYP_PUSHPULL, GPIO_SPD_LOW, GPIO_PUPD_NONE );
	GPIO_AFConfigure ( gpio_def[ id ].port, gpio_def[ id ].pin, gpio_def[ id ].altFn );   //  set AF
#endif
#if defined( STM3210E_EVAL )
	//?? GPIO_PinConfigure( gpio_def[ key ].port, gpio_def[ key ].pin, GPIO_IN_FLOATING, GPIO_MODE_INPUT );
#endif
}
void 										gSet( GPIO_ID gpio, uint8_t on ){								// set the LOGICAL state of a GPIO output pin
	uint8_t act = gpio_def[ gpio ].active;
	uint8_t lev = on? act : 1-act; 		// on => pressed else 1-pressed
  GPIO_PinWrite( gpio_def[ gpio ].port, gpio_def[ gpio ].pin, lev );
}
bool			 							gGet( GPIO_ID gpio ){														// => LOGICAL state of a GPIO input, e.g. True if 'PA0_'==0 or 'PB3'==1
	GPIO_TypeDef *	port = gpio_def[ gpio ].port;
	if (port==NULL) return false;		// undefined: always false
  return GPIO_PinRead( port, gpio_def[ gpio ].pin )== gpio_def[ gpio ].active;
}
bool			 							gOutVal( GPIO_ID gpio ){												// => LOGICAL state of a GPIO OUTput, e.g. True if 'PA0_'.ODR==0 or 'PB3'.ODR==1
	GPIO_TypeDef *	port = gpio_def[ gpio ].port;
	if (port==NULL) return false;		// undefined: always false
	int pin = gpio_def[ gpio ].pin, actval = gpio_def[ gpio ].active;
	return ((port->ODR >> pin) & 1) == actval;
}
// flash -- debug on LED / KEYPAD 
void 										flashLED( const char *s ){											// 'GGRR__' in .1sec
	for ( int i=0; i<strlen(s); i++ ){
		switch ( s[i] ){
			case 'G': gSet( gGREEN, 1 ); break;
			case 'R': gSet( gRED, 1 ); break;
			default: break;
		}
		tbDelay_ms( 100 );
		gSet( gGREEN, 0 );	
		gSet( gRED, 0 );
	}
}
void 										flashCode( int v ){															// e.g. 9 => "RR__GG__GG__RR______"
	for ( int i=0; i<4; i++ )
		flashLED( ((v & (1<<i))==0)? "GGG__":"RRR__" );
	flashLED("__");
}
void										flashInit( ){																		// init keypad GPIOs for debugging	
	LED_Init( gGREEN );	// blinks then off
	LED_Init( gRED );		// blinks then off
	for ( GPIO_ID id = gHOME; id <= gTABLE;  id++ )
		gConfigKey( id ); // low speed pulldown input
	tbDelay_ms(1000);
}

// MPU Identifiers
char CPU_ID[20], TB_ID[20];

void 										initIDs(){																		// initialize CPU_ID & TB_ID strings
	typedef struct {	// MCU device & revision
		// Ref Man: 30.6.1 or 31.6.1: MCU device ID code
		// stm32F412: 0xE0042000	== 0x30006441 => CPU441_c 
		// stm32F10x: 0xE0042000  rev: 0x1000  dev: 0xY430  (XL density) Y reserved == 6
		// stm32F411xC/E: 0xE0042000  rev: 0x1000  dev: 0x431
		uint16_t dev_id 	: 12;
		uint16_t z13_15 	:  4;
		uint16_t rev_id  	: 16;
	}  MCU_ID;
	MCU_ID *id = (MCU_ID *) DBGMCU;
  char rev = '?';
	switch (id->rev_id){
		case 0x1001: rev = 'z';
		case 0x2000: rev = 'b';
		case 0x3000: rev = 'c';
	}
	sprintf( CPU_ID, "CPU%x_%c", id->dev_id, rev );

	struct {						// STM32 unique device ID 
		// Ref Man: 30.2 Unique device ID register  
		// stm32F411xC/E: 0x1FFF7A10  32bit id, 8bit waf_num ascii, 56bit lot_num ascii
		// stm32F412: 		0x1FFF7A10  32,32,32 -- no explanation
		// stm32F103xx: 	0x1FFFF7E8  16, 16, 32, 32 -- no explanation
		uint16_t x;
		uint16_t y;
		uint8_t wafer;
		char lot[11];		// lot in 0..6
	}  stmID;
	
//	uint32_t * uniqID = (uint32_t *) UID_BASE;	// as 32 bit int array
//	dbgLog( "UID: %08x \n %08x %08x \n", uniqID[0], uniqID[1], uniqID[2] );
	
	memcpy( &stmID, (const void *)UID_BASE, 12 );
	stmID.lot[7] = 0;  // null terminate the lot string
	sprintf( TB_ID, "%04x.%04x.%x.%s", stmID.x, stmID.y, stmID.wafer, stmID.lot );
}

void showRTC( ){
	int pDt=0,Dt=1, pTm=0,Tm=1;
	while (pDt != Dt || pTm != Tm){
		pDt = Dt;
		pTm = Tm;
		Dt = RTC->DR;
		Tm = RTC->TR;
	}
	
	uint8_t yr, mon, date, day, hr, min, sec;
	yr =  ((Dt>>20) & 0xF)*10 + ((Dt>>16) & 0xF);
	day = ((Dt>>13) & 0x7);
	mon = ((Dt>>12) & 0x1)*10 + ((Dt>>8) & 0xF);
	date =((Dt>> 4) & 0x3)*10 + (Dt & 0xF);
	
	hr =  ((Tm>>20) & 0x3)*10 + ((Tm>>16) & 0xF);
	min = ((Tm>>12) & 0x7)*10 + ((Tm>>8) & 0xF);
	sec = ((Tm>> 4) & 0x7)*10 + (Tm & 0xF);
	
	char * wkdy[] = { "", "Mon","Tue","Wed","Thu","Fri","Sat","Sun" };
	char * month[] = { "", "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
	char dttm[50];
	sprintf(dttm, "%s %d-%s-%d %d:%d:%d", wkdy[day], date, month[mon], yr, hr,min,sec );
	logEvtNS( "RTC", "DtTm", dttm );
  dbgLog("RTC: %s \n", dttm );
}

static uint32_t lastTmStmp = 0;
static uint32_t lastHalTick = 0, HalSameCnt = 0;
 
uint32_t 								tbTimeStamp(){																	// return msecs since boot
	if ( osKernelGetState()==osKernelRunning ) 
		lastTmStmp =  osKernelGetTickCount();
	return lastTmStmp;
}
int 										delayReq, actualDelay;
uint32_t 								HAL_GetTick(void){															// OVERRIDE for CMSIS drivers that use HAL
	int tic = tbTimeStamp();
	HalSameCnt = tic==lastHalTick? HalSameCnt+1 : 0;
	if ( HalSameCnt > 200 )
		tbErr( "HalTick stopped" );
	
	lastHalTick = tic;
	return lastHalTick;
}
void 										tbDelay_ms( int ms ) {  												// Delay execution for a specified number of milliseconds
	int stTS = tbTimeStamp();
	if ( ms==0 ) ms = 1;
	delayReq = ms;
	actualDelay = 0;
	if ( osKernelGetState()==osKernelRunning ){
		osDelay( ms );
		actualDelay = tbTimeStamp() - stTS;
	}
	if ( actualDelay==0 ){ // didn't work-- OS is running, or clock isn't
		ms *= ( SystemCoreClock/10000 );
		while (ms--) { __nop(); __nop(); __nop(); __nop(); __nop(); __nop(); }
	}
}
static int 							tbAllocTotal = 0;																// track total heap allocations
void *									tbAlloc( int nbytes, const char *msg ){					// malloc() & check for error
	tbAllocTotal += nbytes;
	void *mem = (void *) malloc( nbytes );
	dbgEvt( TB_Alloc, nbytes, (int)mem, tbAllocTotal, 0 );
	if ( mem==NULL ){
		  errLog( "out of heap %s: tbAllocTotal=%d \n", msg, tbAllocTotal );
			tbErr("out of heap");
	}
	return mem;
}
// debug logging
void 										usrLog( const char * fmt, ... ){
	va_list arg_ptr;
	va_start( arg_ptr, fmt );
	enableLCD();
	setTxtColor( LCD_COLOR_BLACK );
//	printf(" ");
	vprintf( fmt, arg_ptr );
	va_end( arg_ptr );
	disableLCD();
}
void 										dbgLog( const char * fmt, ... ){
	va_list arg_ptr;
	va_start( arg_ptr, fmt );
	enableLCD();
	setTxtColor( LCD_COLOR_BLUE );
//	printf("d: ");
	vprintf( fmt, arg_ptr );
	va_end( arg_ptr );
	disableLCD();
}
void 										errLog( const char * fmt, ... ){
	va_list arg_ptr;
	va_start( arg_ptr, fmt );
	enableLCD();
	setTxtColor( LCD_COLOR_RED );
	printf("E: ");
	vprintf( fmt, arg_ptr );
	va_end( arg_ptr );
	printf("\n");
	disableLCD();
}

void 										tbErr( const char * fmt, ... ){											// report fatal error
	va_list arg_ptr;
	va_start( arg_ptr, fmt );
	vprintf( fmt, arg_ptr );
	va_end( arg_ptr );
//	errLog( "TBErr: %s \n", msg );
	//logEvtS( "*** tbError: ", msg );
	dbgEvtS( TB_Error, fmt );
	
	while ( true ){ 
//	for (int i=0; i<20; i++){
		flashLED( "RRR__GGG__" );
	}
	
//	gSet( gRED, 1 );
//	gSet( gGREEN, 1 );
//	__breakpoint(0);
//	ledFg( "R8_2 R8_2 R8_20!" );
}
void										tbShw( const char *s, char **p1, char **p2 ){  	// assign p1, p2 to point into s for debugging
	short len = strlen(s);
	*p1 = len>32? (char *)&s[32] : "";
	if (p2 != p1)
		*p2 = len>64? (char *)&s[64] : "";
}


// fault handlers *****************************************************
int divTst(int lho, int rho){	// for div/0 testing
    return lho/rho;
}

typedef struct {		// saved registers on exception
	uint32_t	R0;
	uint32_t	R1;
	uint32_t	R2;
	uint32_t	R3;
	uint32_t	R12;
	uint32_t	LR;
	uint32_t	PC;
	uint32_t	PSR;
} svFault_t;
typedef struct  {			// static svSCB for easy access in debugger
	uint32_t	ICSR;	// Interrupt Control & State Register (# of exception)
	uint32_t	CFSR;	// Configurable Fault Status Register
	uint32_t	HFSR;	// HardFault Status Register
	uint32_t	DFSR;	// Debug Fault Status Register 
	uint32_t	AFSR; // Auxiliary Fault Status Register
	uint32_t	BFAR;	// BusFault Address Register (if cfsr & 0x0080)
	uint32_t	MMAR;	// MemManage Fault Address Register (if cfsr & 0x0080)
	uint32_t	EXC_RET;	// exception return link register
	uint32_t	SP;		// active stack pointer after svFault
	
} svSCB_t;
static svSCB_t svSCB;

/****** printf to Dbg.Scr ******/
const int xMAX = dbgChs; 				// typedef in tbook.h
const int yMAX = dbgLns;
static int  cX = 0, cY = 0;
#if !defined( USE_LCD )
void InitLCD( const char * hdr ){
}
void LCDwriteln( char * s ){
}
void enableLCD( void ){
}
void disableLCD( void ){
}
void clearHdr( void ){
}
void printHdr( int x, int y, const char *s ){
}
void setTxtColor( uint16_t c ){	// set color of scroll text
}
#endif

void 										initPrintf( const char *hdr ){
	for (int i=0; i<dbgLns; i++)
		Dbg.Scr[i][0] = 0;
	cX = 0; 
	cY = 0;
	InitLCD( hdr );					// eval_LCD if STM3210E
	printf( "%s\n", hdr );
}
void 										stdout_putchar( char ch ){
	if ( ch=='\n' || cX >= xMAX ){ // end of line
		LCDwriteln( &Dbg.Scr[cY][0] );

		cY = (cY+1) % yMAX;		// mv to next line
		cX = 0;
		strcpy( Dbg.Scr[(cY+1)%yMAX],  "=================" );	// marker line after current
		if ( ch=='\n' ) return;
	}
	Dbg.Scr[cY][cX++] = ch;
	Dbg.Scr[cY][cX] = 0;
}	






int dbgEvtCnt = 0, dbgEvtMin = 0, dbgEvtMax = 1000000;
void 										dbgEVR( int id, int a1, int a2, int a3, int a4 ){
	dbgEvtCnt++;
	if ( dbgEvtCnt < dbgEvtMin ) return;
	if ( dbgEvtCnt > dbgEvtMax ) return;
	EventRecord4( id, a1, a2, a3, a4 ); 
}
void 										dbgEvt( int id, int a1, int a2, int a3, int a4 ){ EventRecord4( id, a1, a2, a3, a4 ); }
void 										dbgEvtD( int id, const void *d, int len ){ EventRecordData( id, d, len ); }
void 										dbgEvtS( int id, const char *d ){ EventRecordData( id, d, strlen(d) ); }

struct  {
	char 			*devNm;		
	void			*devBase;

	struct {
						char 			*Nm;
						uint32_t 	off;
	}  regs[20];
} devD[] = 
{ 
	{ "RCC", RCC, {
			{ "C", 0x00 }, { "PLL", 0x04 }, { "CFGR", 0x08 }, { "AHB1ENR", 0x30 }, { "AHB2ENR", 0x34 },
			{ "APB1ENR", 0x40 }, { "APB2ENR", 0x44 }, { "PLLI2S", 0x84 }, { "DCKCFG", 0x8C },{ "CKGATEN", 0x90 }, { "DCKCFG2", 0x94 }, {NULL,0}
		}
	},
	{ "DMA1", DMA1, {
			{ "LIS", 0x00 }, { "HIS", 0x04 }, { "LIFC", 0x08 }, { "HIFC", 0x0C }, 
				{ "S4C", 0x70 }, { "S4NDT", 0x74 }, { "S4PA", 0x78 }, { "S4M0A", 0x7C }, { "S4M1A", 0x80 }, {NULL,0}
		}
	},
	{ "DMA2", DMA2, {
			{ "LIS", 0x00 }, { "HIS", 0x04 }, { "LIFC", 0x08 }, { "HIFC", 0x0C }, 
				{ "S3C", 0x58 }, { "S3NDT", 0x5c }, { "S3PA", 0x60 }, { "S3M0A", 0x64 }, { "S3M1A", 0x68 }, 
				{ "S6C", 0xa0 }, { "S3NDT", 0xa4 }, { "S3PA", 0xa8 }, { "S3M0A", 0xac }, { "S3M1A", 0xb0 }, {NULL,0}
		}
	},
	{ "EXTI", EXTI, {
			{ "IM", 0x00 }, { "EM", 0x04 }, { "RTS", 0x08 }, { "FTS", 0x0C }, { "SWIE", 0x10 }, {NULL,0}
		}
	},
	{ "I2C1", I2C1, {
			{ "CR1", 0x00 }, { "CR2", 0x04 }, { "OA1", 0x08 }, { "OA2", 0x0C }, { "S1", 0x14 }, { "S2", 0x18 }, { "TRISE", 0x20 }, { "FLT", 0x24 }, {NULL,0}
		}
	},
	{ "SPI2", SPI2, {
			{ "CR1", 0x00 }, { "CR2", 0x04 }, { "SR", 0x08 }, { "I2SCFG", 0x1C }, { "I2SP", 0x20 }, {NULL,0}
		}
	},
	{ "SPI3", SPI3, {
			{ "CR1", 0x00 }, { "CR2", 0x04 }, { "SR", 0x08 }, { "I2SCFG", 0x1C }, { "I2SP", 0x20 }, {NULL,0}
		}
	},
	{ "SDIO", SDIO, {
			{ "PWR", 0x00 }, { "CLKCR", 0x04 }, { "ARG", 0x08 }, { "CMD", 0x0C }, { "RESP", 0x10 }, { "DCTRL", 0x2C }, 
				{ "STA", 0x34 }, { "ICR", 0x38 },  { "MASK", 0x3C }, {NULL,0}
		}
	},
	
	{ "A", GPIOA, { 
			{"MD", 0x00 }, { "TY", 0x04 }, { "SP", 0x08 }, { "PU", 0x0C }, { "AL", 0x20 },  { "AH", 0x24 }, {NULL,0}
		} 
	}, 
	{ "B", GPIOB, { 
			{"MD", 0x00 }, { "TY", 0x04 }, { "SP", 0x08 }, { "PU", 0x0C }, { "AL", 0x20 },  { "AH", 0x24 }, {NULL,0}
		} 
	}, 
	{ "C", GPIOC, { 
			{"MD", 0x00 }, { "TY", 0x04 }, { "SP", 0x08 }, { "PU", 0x0C }, { "AL", 0x20 },  { "AH", 0x24 }, {NULL,0}
		} 
	}, 
	{ "D", GPIOD, { 
			{"MD", 0x00 }, { "TY", 0x04 }, { "SP", 0x08 }, { "PU", 0x0C }, { "AL", 0x20 },  { "AH", 0x24 }, {NULL,0}
		} 
	},
	{ "E", GPIOE, { 
			{"MD", 0x00 }, { "TY", 0x04 }, { "SP", 0x08 }, { "PU", 0x0C }, { "AL", 0x20 },  { "AH", 0x24 }, {NULL,0}
		} 
	},
	{ NULL, 0, { {NULL, 0} }}
};
const int MXR = 200;
uint32_t 	prvDS[MXR]={0}, currDS[MXR];

void 										chkDevState( char *loc, bool reset ){ // report any device changes 
	int idx = 0;		// assign idx for regs in order
	dbgLog( "chDev: %s\n", loc );
	for ( int i=0; devD[i].devNm!=NULL; i++ ){
		uint32_t * d = devD[i].devBase;
		dbgLog( "%s\n", devD[i].devNm );
		for ( int j=0; devD[i].regs[j].Nm != NULL; j++){
			char * rNm = devD[i].regs[j].Nm;
			int wdoff = devD[i].regs[j].off >> 2;
			uint32_t val = d[ wdoff ];	// read REG at devBase + regOff
			currDS[idx] = val;			
			if ( (reset && val!=0) || val != prvDS[idx] ){
				dbgLog( ".%s %08x\n", rNm, val ); 
			}
			prvDS[ idx ] = currDS[ idx ];
			idx++;
			if (idx>=MXR) tbErr("too many regs");
		}
	}
}

void 										HardFault_Handler_C( svFault_t *svFault, uint32_t linkReg ){
// fault handler --- added to startup_stm32f10x_xl.s
// for 2:NMIHandler, 3:HardFaultHandler, 4:MemManage_Handler, 5:BusFault_Handler, 6:UsageFault_Handler 
// It extracts the location of stack frame and passes it to the handler written
// in C as a pointer.
//
//HardFaultHandler
//	MRS		 R0, MSP
//	B      HardFault_Handler_C

	svSCB.ICSR = SCB->ICSR;		// Interrupt Control & State Register (# of exception)
	svSCB.CFSR = SCB->CFSR;		// Configurable Fault Status Register
	svSCB.HFSR = SCB->HFSR;		// HardFault Status Register
  svSCB.DFSR = SCB->DFSR;		// Debug Fault Status Register
  svSCB.AFSR = SCB->AFSR;		// Auxiliary Fault Status Register
  svSCB.BFAR = SCB->BFAR;		// BusFault Address Register (if cfsr & 0x0080)
  svSCB.MMAR = SCB->MMFAR;	// MemManage Fault Address Register (if cfsr & 0x0080)
	svSCB.EXC_RET = linkReg;	// save exception_return
	svSCB.SP 	 = (uint32_t) svFault;		// sv point to svFault block on (active) stack

  char *fNms[] = {
		"??", 
		"??", 
		"NMI", //2 
		"Hard", // 3
		"MemManage", // 4
		"Bus", // 5 
		"Usage" // 6 
	};
	int vAct = svSCB.ICSR & SCB_ICSR_VECTACTIVE_Msk;
	enableLCD();
	printf( "Fault: 0x%x = %s \n",  vAct, vAct<7? fNms[vAct]:"" );
	int cfsr = svSCB.CFSR, usgF = cfsr >> 16, busF = (cfsr & 0xFF00) >> 8, memF = cfsr & 0xFF;
	dbgEvt( TB_Fault, vAct, cfsr, 0,0 );
	
	printf( "CFSR: 0x%08x \n", cfsr );
	printf( "EXC_R: 0x%08x \n", svSCB.EXC_RET );
	printf( "  sp: 0x%08x \n ", svSCB.SP + (sizeof svFault) );
  if ( usgF ) printf( "  Usg: 0x%04x \n", usgF );
  if ( busF ){
		printf( "  Bus: 0x%02x \n   BFAR: 0x%08x \n", busF, svSCB.BFAR );
		printAudSt(); // show audio state for bus faults
	}
  if ( memF ) printf( "  Mem: 0x%02x \n   MMAR: 0x%08x \n", memF, svSCB.MMAR );
	
  printf( "PC: 0x%08x \n", svFault->PC );
	tbErr(" Fault" );
}
void 										RebootToDFU( void ){													// reboot into SystemMemory -- Device Firmware Update bootloader
	// STM32F412vet
  //  location 0 holds 0x20019EE8  -- stack top on reboot
	// Addresses  0x00000000 .. 0x00003FFFF can be remapped to one of:
	//   Flash  	0x08000000 .. 0x080FFFFF
	//   SRAM			0x20000000 .. 0x2003FFFF
	//   SysMem   0x1FFF0000 .. 0x1FFFFFFF  
	// Loc 0 (stack pointer) in different remaps
	//   Flash  	0x08000000:  20019EE8
	//   SRAM			0x20000000:  00004E20  (not stack ptr)
	//   SysMem   0x1FFF0000:  20002F48
	//  
	
	const uint32_t *SYSMEM = (uint32_t *) 0x1FFF0000;
	void (*Reboot)(void) = (void (*)(void)) 0x1FFF0004;		// function pointer to SysMem start address (for DFU)
	
	// AN2606 STM32 microcontroller system memory boot mode  pg. 24 says:
	//  before jumping to SysMem boot code, you must:
	RCC->AHB1ENR = 0;																			// 1) disable all peripheral clocks
	RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;									//  except ENABLE the SYSCFG register clock
	RCC->CR &= ~( RCC_CR_PLLI2SON | RCC_CR_PLLON );				// 2) disable used PLL(s)
	EXTI->IMR = 0;    																		// 3) disable all interrupts
	EXTI->PR = 0;    																			// 4) clear pending interrupts
	
	int SysMemInitialSP = *SYSMEM; 												// get initial SP value from SysMem[ 0 ]
	__set_MSP( SysMemInitialSP );	 												// set the main SP
	__DSB();  // wait for all memory accesses to complete
	__ISB();	// flush any prefetched instructions
	SYSCFG->MEMRMP = 1;  																	// remaps 0..FFFF to SysMem at 0x1FFF0000
	__ISB();	// flush any prefetched instructions
  Reboot();						 																	// jump to offset 0x4 (in SysMem)
  while (1);
}
uint32_t 								osRtxErrorNotify (uint32_t code, void *object_id) {	// osRtx error callback
  (void)object_id;

  switch (code) {
    case osRtxErrorStackUnderflow:
      tbErr("osErr StackUnderflow");// Stack overflow detected for thread (thread_id=object_id)
      break;
    case osRtxErrorISRQueueOverflow:
      tbErr("osErr ISRQueueOverflow");// ISR Queue overflow detected when inserting object (object_id)
      break;
    case osRtxErrorTimerQueueOverflow:
      tbErr("osErr TimerQueueOverflow");// User Timer Callback Queue overflow detected for timer (timer_id=object_id)
      break;
    case osRtxErrorClibSpace:
      tbErr("osErr ClibSpace");// Standard C/C++ library libspace not available: increase OS_THREAD_LIBSPACE_NUM
      break;
    case osRtxErrorClibMutex:
      tbErr("osErr ClibMutex");// Standard C/C++ library mutex initialization failed
      break;
    default:
      tbErr("osErr unknown");// Reserved
      break;
	}
	return 0;
}

// END tbUtil
