//  Gene Ball Dec-2019: based on keil/arm/pack/amr/cmsis/pack/example/cmsis_driver/I2S_LBC18xx.c
//  configured only for TalkingBook Rev2b: F412vet & AK4637 codec
//     control of the AK4637 is managed in ak4xxx.c   (AK registers written over I2C1)
//     audio data travels over I2S-- managed in this file
//  this driver is based on the SAI CMSIS interface, but handles only one configuration
//     currently:
//         1) SPI3 is used to generate an accurate 12MHz clock to the AK4637 on I2S3_MCK
//         2) AK4637 is configured to generate I2S clocks at the desired audio frequency
//               (on I2S2_CK & I2S2_WS)
//         3) SPI2 is operated in I2S slave mode with double-buffer DMA, to send/receive stereo samples
//     see RM0402, STM32F412xx Reference Manual, Section 26.6
//      uses 4 GPIO pins:
//				SD: Serial Data (mapped on MOSI pin     I2S2_SD )
//				WS: Word Select (mapped on NSS pin as   I2S2_WS )
//				CK: Serial Clock (mapped on SCK pin as  I2S2_CK )
//				MCK: Master Clock (mapped separately as I2S3_MCK )  12MHz reference clock to codec
//	Usage: 
//		Driver_SAI0.Initialize( cb_func ) -- allocate & initialize I2S & codec hardware, register event callback fn
//		Driver_SAI0.PowerControl( ARM_POWER_FULL );		// power up audio
//		Driver_SAI0.Control( control, 0, freq); 		 // control settings & audio_frequency
//      supported playback values for 'control':
//				ARM_SAI_CONFIGURE_TX | ARM_SAI_MODE_SLAVE |			// configure Transmitter as Slave
//    		ARM_SAI_ASYNCHRONOUS | ARM_SAI_PROTOCOL_I2S |		// Asynchronous  I2S Protocol
//		  	ARM_SAI_DATA_SIZE(16) |													// 16-bit samples
//  preload buf0 & buf1 from audio file, then:
//		Driver_SAI0.Send( buf0, nsamples );									// transmit audio buffer
//		Driver_SAI0.Send( buf1, nsamples );									// transmit audio buffer
//  then preload buf2
//		-> cb_func( evts ) 																	// called by ISR (on error or buffer complete)
//      should call Driver_SAI0.Send() to chain buffers, then preload the next buffer
//  the last buffer should have zero padding to let the codec pipeline empty of real data,
//  then call
//		Driver_SAI0.Control( ARM_SAI_ABORT_SEND, arg, 0 );	// required at end with arg==0, to shutdown I2S device
//		Driver_SAI0.PowerControl( ARM_POWER_OFF ) 					// power down codec
//  and optionally
//		Driver_SAI0.Uninitialize( )													// shut down I2S & I2C, release GPIOs
/* -------------------------------------------------------------------------- 
 * Copyright (c) 2013-2016 ARM Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * $Date:        02. March 2016
 * $Revision:    V1.4
 *
 * Driver:       Driver_SAI0, Driver_SAI1
 * Configured:   via RTE_Device.h configuration file
 * Project:      SAI (I2S used for SAI) Driver for NXP STM32F10X
 * --------------------------------------------------------------------------
 * Use the following configuration settings in the middleware component
 * to connect to this driver.
 *
 *   Configuration Setting                 Value   SAI Interface
 *   ---------------------                 -----   -------------
 *   Connect to hardware via Driver_SAI# = 0       use SAI0 (I2S0)
 *   Connect to hardware via Driver_SAI# = 1       use SAI1 (I2S1)
 * -------------------------------------------------------------------------- */

/* History:
 *  Version 1.4
 *    - Driver update to work with GPDMA_STM32F10X ver.: 1.3
 *  Version 1.3
 *    - Corrected PowerControl function for conditional Power full (driver must be initialized)
 *  Version 1.2
 *    - PowerControl for Power OFF and Uninitialize functions made unconditional.
 *    - Corrected status bit-field handling, to prevent race conditions.
 *  Version 1.1
 *    - Improved sampling frequency divider calculation
 *  Version 1.0
 *    - Initial release
 */
#include "tbook.h"						// includes main.h with GPIO_IDs:   gI2S2_SD,	gI2S2_WS,	gI2S2_CK,	gI2S2_MCK

#include "I2S_STM32F4xx.h"		// could be SAI instead of I2S
#include "ak4xxx.h"
#include <math.h>
void dbgToggleVolume( void );		// DEBUG

/* REPLACED: RTE_Device configuration
// ************** Configuration based on RTE_Device
// 		require RTE_Device definitions for I2S0, TX_DMA, RX_DMA
//#if (!defined(RTE_I2S0))
//#error "I2S missing in RTE_Device.h. Please update RTE_Device.h!"
//#endif
//#if (!defined(RTE_I2S0_TX_DMA))
//#error "I2S Transmit DMA missing in RTE_Device.h. Please update RTE_Device.h!"
//#endif
//#if (!defined(RTE_I2S0_RX_DMA))
//#error "I2S Receive DMA missing in RTE_Device.h. Please update RTE_Device.h!"
//#endif
*/

#define ARM_SAI_DRV_VERSION ARM_DRIVER_VERSION_MAJOR_MINOR(1,5)   // driver version

static const ARM_DRIVER_VERSION DriverVersion = {		// Driver Version
  ARM_SAI_API_VERSION,
  ARM_SAI_DRV_VERSION
};

/* REPLACED: RTE_Device configuration --- replaced with gpio_def[ GPIO_ID ] 
// I2S0 data structures, initialized based on RTE_Device.h definitions
static SPI_PIN I2S_ws = 	{ RTE_I2S0_WS_PORT_DEF, 	RTE_I2S0_WS_BIT_DEF  };
static SPI_PIN I2S_sck = 	{ RTE_I2S0_SCK_PORT_DEF, 	RTE_I2S0_SCK_BIT_DEF };
static SPI_PIN I2S_sd = 	{ RTE_I2S0_SD_PORT_DEF, 	RTE_I2S0_SD_BIT_DEF  };
static SPI_PIN I2S_mck = 	{ RTE_I2S0_MCK_PORT_DEF, 	RTE_I2S0_MCK_BIT_DEF };
static DMA_INFO I2S_DMA_Rx = {	
  I2S0_RX_DMA_Instance,
  I2S0_RX_DMA_Number,
  I2S0_RX_DMA_Channel,
  I2S0_RX_DMA_Priority
};

static DMA_INFO I2S_DMA_Tx = {	
  I2S0_TX_DMA_Instance,
  I2S0_TX_DMA_Number,
  I2S0_TX_DMA_Channel,
  I2S0_TX_DMA_Priority
};
*/

static I2S_INFO I2S2_Info = {0};		// run-time info block

static const I2S_RESOURCES I2S2_Resources = {		// capabilities & hardware links
  {  								// capabilities:
    1,   ///< supports asynchronous Transmit/Receive
    0,   ///< supports synchronous Transmit/Receive
    0,   ///< supports user defined Protocol
    1,   ///< supports I2S Protocol
    0,   ///< supports MSB/LSB justified Protocol
    0,   ///< supports PCM short/long frame Protocol
    0,   ///< supports AC'97 Protocol
    1,   ///< supports Mono mode
    0,   ///< supports Companding
    1,   ///< supports MCLK (Master Clock) pin
    0,   ///< supports Frame error event: \ref ARM_SAI_EVENT_FRAME_ERROR
  },
  I2S2,    					// instance: Pointer to I2S device-- SPI2 == I2S2
	I2S3,							// I2S device for clock
/* REMOVED: RTE_Device based pin config
//  {  // I2S0 RX&TX Pin configuration
//    &I2S_sck,
//    &I2S_ws,
//    &I2S_sd,
//    &I2S_mck,
//  },
*/
	DMA1,							// DMA controller for both TX & RX
	DMA1_Stream4, 		// DMA Stream for SPI2==I2S2 TX 
	DMA1_Stream3,			// DMA Stream for SPI2==I2S2 RX
  I2S2_IRQn,				// intq_num
  &I2S2_Info				// I2S_INFO
};

typedef enum { ctlAbort=0, ctlPause=1, ctlResume=2 } controlTyp;

//DEBUG
int BrkCnt = 200;							// DEBUG:  break during audio send
int initialUnderRunCnt = 0;		// DEBUG: count any underruns at the start of a buffer

// forward decls for self calls
static int32_t 								I2S2only_Send( const void *data, uint32_t nSamples, I2S_RESOURCES *i2s );
static int32_t 								I2S_Control( uint32_t control, uint32_t arg1, uint32_t arg2, I2S_RESOURCES *i2s );
static int32_t 								I2S_PowerControl( ARM_POWER_STATE state, I2S_RESOURCES *i2s );

// internal functions
int32_t 											todo( char *msg ){ 																										// report use of NotYetImplemented
	errLog( "TODO: %s", msg ); 
	return 0; 
}
void 													reset_I2S_Info( I2S_RESOURCES *i2s ) {																// initialized run-time info
  i2s->info->status.frame_error   = 0U;
  i2s->info->status.rx_busy       = 0U;
  i2s->info->status.rx_overflow   = 0U;
  i2s->info->status.tx_busy       = 0U;
  i2s->info->status.tx_underflow  = 0U;

	// default for stereo -- but set by I2S_Control( Configure )
	i2s->info->data_bytes = 2; // = 2 channels * 16bit samples
//	i2s->info->dataPtr = NULL; 	
//	i2s->info->monoMode = false;
//	i2s->info->firstChannel = false;
	
	i2s->info->tx_cnt = 0;
	i2s->info->tx_req = 0;
	i2s->info->rx_cnt = 0;
	i2s->info->rx_req = 0;
}
void													controlTXMT( I2S_RESOURCES *i2s, controlTyp typ ){  											// abort I2S Transmit operation
  // stop DMA transmission
	// if pause:
	//    reset DMA to resume if re-enabled, leave I2S2 enabled
	// else
	//    disable I2S2
	
	I2S_INFO *info = i2s->info;
	DMA_Stream_TypeDef * dma = DMA1_Stream4;				// DMA stream for TX
	
	dma->CR &= ~DMA_SxCR_TCIE;											// reset interrupt on complete-- since disabling will cause one
	
	while ( (dma->CR & DMA_SxCR_EN) != 0 )					// reset DMA CR.EN to pause
		dma->CR &= 	~DMA_SxCR_EN;											// keep trying till its true
	
	// reset any leftover int reqs  -- (there should be a TCIF)
	DMA1->HIFCR &= ~( DMA_HISR_TCIF4 |DMA_HISR_HTIF4 |DMA_HISR_TEIF4 |DMA_HISR_DMEIF4 | DMA_HISR_FEIF4  );
	uint32_t nSent;
	switch ( typ ){

		case ctlAbort: 
			i2s->instance->I2SCFGR &= ~I2S_MODE_ENAB;			// disable I2S device
			break;
		
		case ctlPause: // set up DMA so re-enabling will pick up at correct spot  
			nSent = info->tx_req - dma->NDTR;			// see how many samples were sent before pause
			if ( (dma->CR & DMA_SxCR_CT)==0 )								// and update memAddr of current buffer 
				dma->M0AR += nSent;														// to restart after that
			else
				dma->M1AR += nSent;														// to restart after that
			break;
			
		case ctlResume: 
			dma->CR |= 	DMA_SxCR_EN;												// re-enable DMA
			break;
	}
}
void													abortRCV( I2S_RESOURCES *i2s ){  																			// abort I2S receive operation
	i2s->instance->I2SCFGR 	&= ~I2S_MODE_ENAB;										// disable I2S device
	i2s->instance->CR2 			&= ~(I2S_CR2_RXNEIE | I2S_CR2_ERRIE);	// disable interrupts on receive non-empty & error
	reset_I2S_Info( i2s );
}


static void		 								I2S3_ClockEnable( bool enab ){ 								// enable/disable I2S3 clock generator -- as base clock for AK4637 codec
	// TBookRev2B device outputs I2S3_MCK to codec
	// 	enable(disable) I2S clock generation on spi3 device
	// 	output to I2S3_MCK which is used to generate PLL clock on codec, which then acts as I2S master
	//  FIXED to I2S3(SPI3)  
	SPI_TypeDef *spi = SPI3;										// == I2S3
	// clock is generated by putting the device into I2S Master mode Reception  RM0402 26.6.5
	// I2SSTD stereo 16-bit samples:  
	//  CPOL=0  DATLEN=00 CHLEN=0 
	//  I2SCFG=11 (Master receive)  I2SSTD=00 (Philips)

	// first-- disable any ongoing reception (even if going to re-enable)
	// switch off sequence required: RM0402: 26.6.5 Reception sequence
	// 1) wait for second to last RXNE
	// 2) wait 1 I2S clock cycle
	// 3) Disable SPI_CR2.I2SE = 0
	// this all seems about properly receiving N stereo samples-- so disregard it & just shut off I2SE
	
	spi->I2SCFGR 	&= ~I2S_MODE_ENAB;			// disable I2S3 device (0x200)
	
	if ( enab ){
		// make sure the RCC->PLLI2S is running & ready
		RCC->CR |= RCC_CR_PLLI2SON;					// enable the I2S_clk generator PLLI2S 
		while ( (RCC->CR & RCC_CR_PLLI2SRDY)== 0 )
			tbDelay_ms( 1 );
		
		// RM0402  STM32F412xx Reference Manual
		//  I2SCLK from PLLI2S = 96MHz by default
		//  	fs = PLLI2S/ (256*( 2*I2SDIV + ODD )
		//  set to fs = 46875, which produces MCLK of ( fs*256 ) = 12000000
		//  div = 4, odd = 0
		// configure SPI2 I2SCFGR & I2SPR registers -- default modes @ 46875
	
		// SPI3: setup I2S Master Mode Receive
		// 1) set I2SDIV = 4 ODD=0  => fs=46875  I2S3_MCLK = 12000000Hz
		// 2) set CKPOL=0 MCKOE=1 
		// 3) SET I2SMOD=1  I2SSTD=11 DATLEN=00 CHLEN=0 I2SCFG=11
		spi->I2SCFGR = I2S_MODE | I2S_CFG_MASTER_RX;	// I2Smode Master Receive (0x5400) ASTREN=0 I2SSTD=0 CKPOL=0 DATLEN=00 CHLEN=0
		spi->I2SPR   = I2S_MCLKOUTPUT_ENABLE | 4;   	// MCKOE & DIV=4  (0x100)
		spi->CR2 		 = 0;															// disable all interrupts & DMA
		// enable I2S to start clock
		spi->I2SCFGR 	|= I2S_MODE_ENAB;								// enable I2S device, set I2SE (0x200)
	} 
}
static int32_t 								I2S_Configure( I2S_RESOURCES *i2s, uint32_t freq, bool monoMode ){ 		// set audio frequency
#ifdef TBOOKREV2B
	// RM0402  STM32F412xx Reference Manual
  //  I2SCLK from PLLI2S = 96MHz by default
  //  	fs = PLLI2S/ (256*( 2*I2SDIV + ODD )
	//  
	I2S3_ClockEnable( true );			// configure & start I2S3 to generate 12MHz on I2S3_MCK

	i2s->instance->I2SCFGR &=  ~I2S_MODE_ENAB;						// make sure I2S is disabled while changing config
	
  // set SPI2 I2SCFGR & I2SPR registers
	i2s->instance->I2SCFGR = I2S_MODE | I2S_CFG_SLAVE_TX;  // I2SMode & I2SCFG = Slave Transmit
	// defaults:  I2SSTD = 00 = Phillips, DATLEN = 00, CHLEN = 0  16bits/channel

	i2s->instance->I2SPR   = 0;		// reset unused PreScaler
	
//	i2s->info->monoMode = monoMode;
	i2s->info->flags |= I2S_FLAG_CONFIGURED;
	
	// MUST be AFTER SPI2->I2SCFGR.I2SCFG is set to SLAVE_TX
	ak_SetMasterFreq( freq );			// set AK4637 to MasterMode, 12MHz ref input to PLL, audio @ 'freq'
	
//	ak_SpeakerEnable( true );		// power upspeaker amp (w/ mute to avoid pop)
//	ak_SetMute( false );				// unmute -- ready to play
#endif
	
#ifdef STM3210E_EVAL
//----------------------- I2SPR: I2SDIV and ODD Calculation -----------------
	// RM0008  STM32F103xx Reference Manual, Table 183:
	//  SYSCLK		I2S_DIV 		I2S_ODD		MCLK	Target 	   Real fS (KHz) 			 Error
	//  (MHz)	16-bit 32-bit	16-bit 32-b  			fS(Hz)	16-bit 		32-bit		16-bit 	32-bit
	//   72 	  2 	   2 		  0 	 0  	Yes 	96000 	70312.15 	70312.15 	26.76% 	26.76%
	//   72 	  3 	   3 		  0 	 0 		Yes  	48000 	46875 		46875 		2.34% 	2.34%
	//   72 	  3 	   3		  0		 0 		Yes 	44100 	46875 		46875 		6.29% 	6.29%
	//   72 	  4		   4		  1		 1		Yes 	32000 	31250 		31250 		2.34% 	2.34%
	//   72 	  6		   6		  1		 1		Yes		22050 	21634.61 	21634.61 	1.88% 	1.88%
	//   72 	  9		   9		  0		 0 		Yes 	16000 	15625 		15625 		2.34% 	2.34%
	//   72 	 13 	  13		  0		 0 		Yes 	11025 	10817.30 	10817.30 	1.88% 	1.88%
	//   72 	 17 	  17		  1		 1 		Yes 	 8000 	 8035.71 	 8035.71 	0.45% 	0.45%
	// FS = SysClk / ( 256*( 2* I2SDIV + ODD ) 
	int f = freq;
	int div = I2S_clk/(512*freq);
	int f1 = I2S_clk /(256 * (2*div+0));   // freq for div & odd=0
	int f2 = I2S_clk /(256 * (2*div+1));	 // freq for div & odd=1
	int f3 = I2S_clk /(256 * (2*div+2));   // freq for div+1 & odd=0
	// f1 > f2 > f3  -- which is closest to f?
	int e1 = abs(f-f1), e2 = abs(f-f2), e3 = abs(f-f3);
	int odd = 0;
	if ( e2 < e1 && e2 < e3 )
		odd = 1;		// f2 closest: DIV,   ODD=1
	else if ( e3 < e1 && e3 < e2 )
		div++;			// f3 closest: DIV+1, ODD=1
	// else 			// f1 closest: DIV,   ODD=0
			
// I2s config    SPI_I2SCFGR: I2SMOD, I2SE, I2SCFG, PCMSYNC, I2SSTD, CKPOL, DATLEN, CHLEN
	
	uint16_t i2s_cfg = 0;
	i2s_cfg |= I2S_MODE;   							// I2SMOD Selected
	i2s_cfg |= I2S_CFG_MASTER_TX;   		// I2SCFG = Master Transmit
	i2s->info->monoMode = monoMode;
	
	i2s_cfg |= I2S_STANDARD_PHILIPS;   	// I2SSTD = 00 = Phillips
	i2s_cfg |= I2S_CPOL_LOW;   					// CKPOL = 0
	i2s_cfg |= I2S_DATAFORMAT_16B;   		// DATLEN = 00, CHLEN = 0  16bits/channel

	// I2S pre-scaler  SPI_I2SPR: MCKOE, ODD, I2SDIV
	uint16_t i2s_pr = 0;
	i2s_pr |= I2S_MCLKOUTPUT_ENABLE;   			// MCKOE
	i2s_pr |= (odd? SPI_I2SPR_ODD : 0);   	// ODD
	i2s_pr |= div;   												// I2SDIV

  // set SPI2 I2SCFGR & I2SPR registers
	i2s->instance->I2SCFGR = I2S_MODE;  // set I2S Mode, but reset rest ( esp. I2SE = 0 )
	
	i2s->instance->I2SPR   = i2s_pr;		// store PreScaler  (while I2S disabled)
	i2s->instance->I2SCFGR = i2s_cfg;		// store I2S config (while disabled)
	
	// I2S_Send will enable:	i2s->instance->I2SCFGR |= I2S_MODE_ENAB;		// enable I2S device

	i2s->info->flags |= I2S_FLAG_CONFIGURED;
#endif
  return ARM_DRIVER_OK;
}
// 
// exported in Driver_SAI0 
static ARM_DRIVER_VERSION 		I2Sx_GetVersion (void) {
/**
  \fn          ARM_DRIVER_VERSION I2Sx_GetVersion (void)
  \brief       Get driver version.
  \return      \ref ARM_DRIVER_VERSION
*/
  return (DriverVersion);
}
static ARM_SAI_CAPABILITIES 	I2S_GetCapabilities( I2S_RESOURCES *i2s ) {
/**
  \fn          ARM_SAI_CAPABILITIES I2Sx_GetCapabilities (void)
  \brief       Get driver capabilities.
  \param[in]   i2s       Pointer to I2S resources
  \return      \ref ARM_SAI_CAPABILITIES
*/
  return (i2s->capabilities);
}


static int32_t 								I2S_Initialize( ARM_SAI_SignalEvent_t cb_event, I2S_RESOURCES *i2s ) {							// init I2S hardware resources
//   Initialize I2S Interface.
//   	cb_event  Pointer to ARM_SAI_SignalEvent function
//   	i2s       Pointer to I2S resources
//		return    execution_status
  if (i2s->info->flags & I2S_FLAG_INITIALIZED) return ARM_DRIVER_OK;  // Driver is already initialized

  // Initialize I2S Run-Time resources
  i2s->info->cb_event             = cb_event;
	reset_I2S_Info( i2s );

#ifdef STM3210E_EVAL
	// RCC clocks configuration ??
	// RCC_CFGR2 bit I2S2SRC -- should be configured to 0, to select SYSCLOCK as basis of I2S MCLK
	//   not done explicitly, as this is the default (and stm32f10x.h RCC_TypeDef doesn't include CFGR2)
	I2S_clk = SystemCoreClock;		// 72MHz on STM32F103xx of STM3210E_EVAL
#endif
	
#ifdef TBOOKREV2B
	// RM0402  STM32F412xx Reference Manual  6.3.23
  //  I2SCLK comes from PLLI2S = 96MHz from HSI by default
	//  RCC_PLLI2SCFGR configuration register:
	// 		PLLI2S_R  2..7  		( << 28)
	// 		PLLI2S_Q  2..15 		( << 24)
	// 		PLLI2_SRC 0..1   		0: same as PLL  1: CK_I2S_EXT	( << 22 )
	// 		PLLI2S_N  2..432 		( << 6 )
	// 		PLLI2S_M	2..63			( << 0 ) 
	//  VCO_in = SRC / M 			// ( should be 2MHz as recommended by 26.3.23 )
	//  VCO_clk = VCO_in * N 	
	//  USB/SDIO clk = VCO_clk / Q	
	// 	I2S_clk = VCO_clk / R		
	
  // I22_clk = (HSE * PLLI2S_N / PLLI2S_M) / PLLI2S_R 
	int I2S_N = 0x60;
	int I2S_M = 4;
	int I2S_R = 2;	
	int I2S_Q = 4;
	// configure to: 96MHz as expected by I2S3_ClockEnable() = (8000000 * 96 / 4)/ 2 = 96000000 
	int cfg = 0;																		// SRC=0 == input from same as PLL (HSE 8MHz crystal clock)
	cfg |= (I2S_R << RCC_PLLI2SCFGR_PLLI2SR_Pos);		// R == 2
	cfg |= (I2S_Q << RCC_PLLI2SCFGR_PLLI2SQ_Pos);		// Q == 4  // set to default for USB
	cfg |= (I2S_N << RCC_PLLI2SCFGR_PLLI2SN_Pos);		// N == 96
	cfg |= (I2S_M << RCC_PLLI2SCFGR_PLLI2SM_Pos);		// M == 16
	// PLLI2SCFGR: _RRR QQQQ _S__ ____ _NNN NNNN NNMM MMMM
	// 0x24001804  0010 1000 0000 0000 0001 1000 0000 0100 R=2 Q=4 S=0 N=0x60 M=4
	RCC->PLLI2SCFGR = cfg;

  // set APB1 peripherals to use PLLI2S_clock -- i.e. I2S3 == SPI3 
	RCC->DCKCFGR |= ( 0 << RCC_DCKCFGR_I2S1SRC_Pos );		// set to default: SPI2 & SPI3 I2S clock <= PLLI2S

	RCC->CR |= RCC_CR_PLLI2SON;					// enable the I2S_clk generator PLLI2S 
	//	I2S_clk = 192000000 / I2S_R;		// PLLI2S configuration -- default: 96MHz 
	
	RCC->APB1RSTR |= 	RCC_APB1RSTR_SPI2RST;		// reset spi2 
	RCC->APB1RSTR &= ~RCC_APB1RSTR_SPI2RST;	// un-reset spi2

	RCC->APB1RSTR |= 	RCC_APB1RSTR_SPI3RST;		// reset spi3
	RCC->APB1RSTR &= ~RCC_APB1RSTR_SPI3RST;	// un-reset spi3
#endif	

/* REMOVED: RTE_Device based configuration
	//GPIO_AFConfigure();	no alternate configurations are required-- using SPI2: SCK,NSS,MOSI & PC6
  //GPIO_PinConfigure(i2s->rxtx_pins.sck->port, i2s->rxtx_pins.sck->pin, GPIO_MODE_OUT, GPIO_OTYP_PUSHPULL, GPIO_SPD_FAST, GPIO_PUPD_NONE );
	//  GPIO_PinConfigure(i2s->rxtx_pins.sd->port, i2s->rxtx_pins.sd->pin, GPIO_MODE_OUT, GPIO_OTYP_PUSHPULL, GPIO_SPD_FAST, GPIO_PUPD_NONE );
	//  GPIO_PinConfigure(i2s->rxtx_pins.ws->port, i2s->rxtx_pins.ws->pin, GPIO_MODE_OUT, GPIO_OTYP_PUSHPULL, GPIO_SPD_FAST, GPIO_PUPD_NONE );
	//  GPIO_PinConfigure(i2s->rxtx_pins.mck->port, i2s->rxtx_pins.mck->pin, GPIO_MODE_OUT, GPIO_OTYP_PUSHPULL, GPIO_SPD_FAST, GPIO_PUPD_NONE );
*/

	// configure GPIOs using gpio_id's from main.h -- also specifies AFn for STM32F412
	// Configure SCK Pin (stm32F103zg: PB13 (SPI2_SCK / I2S2_CK ))  
	gConfigI2S( gI2S2_CK );  // TBookV2B 	{ gI2S2_CK,			"PB13|5"		},	// AK4637 BICK == CK == SCK 			(RTE_I2SDevice.h I2S0==SPI2 altFn=5) 
	// Configure SD Pin  (stm32F103zg: PB15 (SPI2_MOSI / I2S2_SD))
	gConfigI2S( gI2S2_SD );  // TBookV2B { gI2S2_SD,			"PB15|5"		},	// AK4637 SDTI == MOSI 						(RTE_I2SDevice.h I2S0==SPI2 altFn=5)
  // Configure WS Pin  (stm32F103zg: PB12 (SPI2_NSS / I2S2_WS))
	gConfigI2S( gI2S2_WS );  // TBookV2B { gI2S2_WS,			"PB12|5"		},	// AK4637 FCK  == NSS == WS 			(RTE_I2SDevice.h I2S0==SPI2 altFn=5)

	// Configure MCK Pin  (using I2S3_MCK on TBookRev2B)
	gConfigI2S( gI2S3_MCK ); // TBookV2B { gI2S3_MCK,		"PC7|6"			},	// AK4637 MCLK 	 									(RTE_I2SDevice.h I2S3==SPI2 altFn=6)
	//gConfigI2S( gI2S2_MCK ); // TBookV2B { gI2S2_MCK,		"PC6|5"			},	// AK4637 MCLK   									(RTE_I2SDevice.h I2S0==SPI2 altFn=5)

	i2s->info->flags = I2S_FLAG_INITIALIZED;
	
  return ARM_DRIVER_OK;
}

static int32_t 								I2S_Uninitialize( I2S_RESOURCES *i2s ) {																						// release I2S hardware resources
	I2S_PowerControl( ARM_POWER_OFF, i2s );		// shut down I2S & AK
	
/* REMOVED: RTE_Device based configuration
  // Unconfigure SCK Pin                
  GPIO_PinConfigure( i2s->rxtx_pins.sck->port, i2s->rxtx_pins.sck->pin, GPIO_MODE_ANALOG,  GPIO_OTYP_PUSHPULL,  GPIO_SPD_LOW,  GPIO_PUPD_NONE );												
  // Unconfigure WS Pin 
  GPIO_PinConfigure( i2s->rxtx_pins.ws->port, i2s->rxtx_pins.ws->pin, GPIO_MODE_ANALOG,  GPIO_OTYP_PUSHPULL,  GPIO_SPD_LOW,  GPIO_PUPD_NONE );
  // Unconfigure SD Pin 
  GPIO_PinConfigure( i2s->rxtx_pins.sd->port, i2s->rxtx_pins.sd->pin, GPIO_MODE_ANALOG,  GPIO_OTYP_PUSHPULL,  GPIO_SPD_LOW,  GPIO_PUPD_NONE );
  // Unconfigure MCK Pin 
*/

	RCC->CR &= ~RCC_CR_PLLI2SON;			// disable the I2S_clk generator PLLI2S 
	RCC->PLLI2SCFGR = 0x24003010;			// reset to default
	
	gUnconfig( gI2S2_CK );  	// Unconfigure SCK Pin 
	gUnconfig( gI2S2_WS );  	// Unconfigure WS Pin 
	gUnconfig( gI2S2_SD );  	// Unconfigure SD Pin 
	gUnconfig( gI2S2_MCK );  	// Unconfigure MCK Pin 

  i2s->info->flags = 0U;
  return ARM_DRIVER_OK;
}

static int32_t 								I2S_PowerControl( ARM_POWER_STATE state, I2S_RESOURCES *i2s ){											// power SAI up or down
/**
  Control I2S Interface Power.
		state  Power state
		i2s       Pointer to I2S resources
		return    execution_status
*/

  switch (state) {
    case ARM_POWER_OFF:				// power down when audio not in use
      NVIC_DisableIRQ( i2s->irq_num );      // Disable I2S IRQ
      NVIC_ClearPendingIRQ(i2s->irq_num);		// Clear pending I2S interrupts in NVIC

			ak_PowerDown();					// power down AK4637 & I2C1 device
 //     RCC->APB1ENR &= ~RCC_APB1ENR_I2C1EN; 	// disable I2C1 device

      RCC->APB1ENR &= ~RCC_APB1ENR_SPI2EN; 	// disable SPI2 device
      RCC->APB1ENR &= ~RCC_APB1ENR_SPI3EN; 	// disable SPI3 device (for codec clock)
			RCC->AHB1ENR &= ~RCC_AHB1ENR_DMA1EN;	// disable clock for DMA1
		
			
			reset_I2S_Info( i2s );      // Clear driver variables
      i2s->info->flags &= ~I2S_FLAG_POWERED;
      break;

    case ARM_POWER_LOW:
      return ARM_DRIVER_ERROR_UNSUPPORTED;

    case ARM_POWER_FULL:
      if ((i2s->info->flags & I2S_FLAG_INITIALIZED) == 0U) { return ARM_DRIVER_ERROR; }
      if ((i2s->info->flags & I2S_FLAG_POWERED)     != 0U) { return ARM_DRIVER_OK; }

			ak_Init();				// power up and initialize I2C & codec
			
      // Clear driver variables
			reset_I2S_Info( i2s );
      i2s->info->flags |= I2S_FLAG_POWERED;

      RCC->APB1ENR |= RCC_APB1ENR_SPI2EN; 	// start clocking SPI2 (==I2S)
      RCC->APB1ENR |= RCC_APB1ENR_SPI3EN; 	// enable SPI3 device -- for clock generation
			RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;		// enable clock for DMA1
			
      // Clear and Enable SAI IRQ
      NVIC_ClearPendingIRQ( i2s->irq_num );
      NVIC_EnableIRQ( i2s->irq_num );
      break;

    default: return ARM_DRIVER_ERROR_UNSUPPORTED;
  }
  return ARM_DRIVER_OK;
}
/*static void 									I2S_SendWord( I2S_RESOURCES *i2s ){																									// send next stereo sample-- duplicate if mono
	I2S_INFO *info = i2s->info;	
	if ( info->tx_cnt == 0 ){
		int sr = i2s->instance->SR;    // get status register-- left from end of previous buffer
		if (sr & SPI_SR_UDR) initialUnderRunCnt++;
dbgToggleVolume();
	}
	i2s->instance->DR = *info->dataPtr;		// clears TXE
//	info->firstChannel = !info->firstChannel;
//	if ( info->monoMode && info->firstChannel ) 
//		return;  // send same sample next time

 	if ( info->tx_cnt == info->tx_req ){		// buffer complete-- just stored 2nd copy of last word 
dbgToggleVolume();
		i2s->instance->CR2 &= ~I2S_CR2_TXEIE;									// disable TXE interrupt
		i2s->info->status.tx_busy = 0;
		i2s->info->cb_event( ARM_SAI_EVENT_SEND_COMPLETE );		// call buffer complete callback ( audio.c saiEvent )
	}
dbgToggleVolume();
	
	info->dataPtr++;							// move ptr to next sample-- always if stereo, every other if monoMode
	info->tx_cnt += 2;						// 2 more bytes (1 sample) consumed from buffer
	
	if ( info->tx_cnt >= BrkCnt && gGet( gMINUS ))		// DEBUG: if MINUS key -- halt when tx_cnt reaches BrkCnt
			tbErr( "I2S cnt >= %d \n", BrkCnt );
}
static void 									I2S_GetWord(I2S_RESOURCES *i2s ){																										// receive next stereo sample-- store left channel only
	I2S_INFO *info = i2s->info;	
	if ( info->rx_cnt == info->rx_req ){	// receive buffer full
		i2s->info->status.tx_busy = 0U;
		i2s->instance->I2SCFGR &= ~I2S_MODE_ENAB;		// disable I2S device
		i2s->instance->CR2 &= ~(I2S_CR2_RXNEIE | I2S_CR2_ERRIE);	// disable interrupt on receive data non-empty or error
		if (i2s->info->cb_event != NULL)
			i2s->info->cb_event( ARM_SAI_EVENT_RECEIVE_COMPLETE );		// call buffer complete callback
	}	else {
		int data = i2s->instance->DR;		// clears RXNE
//		info->firstChannel = !info->firstChannel;
//		if ( info->firstChannel ){
//			*info->dataPtr++ = data;		// store left channel sample
//			info->rx_cnt += 2;					// 2 more bytes (1 mono sample) received
//		}
	}
} */

static int32_t 								I2S2only_Send( const void *data, uint32_t nSamples, I2S_RESOURCES *i2s ){						// start data transmit
// ONLY for I2S2 (SPI2) using DMA1_Stream4
//   Start sending data to I2S transmitter.  
//	   data  Pointer to buffer with data to send to I2S transmitter
//	   num   Number of data items to send ( #stereo samples )
//	   i2s   Pointer to I2S resources
//		 return execution_status

  if ((data == NULL) || (nSamples == 0U))             { return ARM_DRIVER_ERROR_PARAMETER; }
	I2S_INFO *info = i2s->info;
	
  if ((info->flags & I2S_FLAG_CONFIGURED) == 0U) 		// I2S is not configured (mode not selected)
			return ARM_DRIVER_ERROR; 
	
	DMA_Stream_TypeDef * dma = DMA1_Stream4;				// DMA stream for TX
	
  if ( info->status.tx_busy ){ 										// FOR DMA Double Buffering:  allow multiple calls to Send, to keep M0AR & M1AR
				//	return ARM_DRIVER_ERROR_BUSY;	 // previous Send is not completed yet
		if ( (dma->CR & DMA_SxCR_CT)==0 )
			dma->M1AR = (uint32_t) data;		// Mem0 in progress, set up Mem1
		else 
			dma->M0AR = (uint32_t) data;
		return ARM_DRIVER_OK;
	}

	// First buffer of file
  info->status.tx_busy = 1U;  							// Set Send active flag
  info->status.tx_underflow = 0U;  					// Clear TX underflow flag

//	uint32_t nbytes = nSamples * info->data_bytes;  // Convert from number of samples to number of bytes
//  info->tx_req = nbytes;
	
	info->tx_cnt = 0;	// bytes transmitted
//	info->dataPtr = (uint16_t *) data;

	// set up DMA for Mem0 -> codec
	while ( (dma->CR & DMA_SxCR_EN) != 0 )		// reset DMA CR.EN to make sure it's disabled
		dma->CR &= 	~DMA_SxCR_EN;								// keep trying till its true
	
	// clear any leftover interrupt requests
	DMA1->HIFCR &= ~( DMA_HISR_TCIF4 |DMA_HISR_HTIF4 |DMA_HISR_TEIF4 |DMA_HISR_DMEIF4 | DMA_HISR_FEIF4  );

	dma->PAR = (uint32_t) &SPI2->DR;		// periph is SPI2 DataRegister
	dma->M0AR = (uint32_t) data;				// load M0AR -- 1st buff
	dma->NDTR = nSamples;
	
	// CHSEL, PFCTRL, PL, FIFO, BURST at defaults
	int cfg = DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0 | DMA_SxCR_MINC | DMA_SxCR_DIR_0;   // mem->codec, 16bit words
	cfg |= 0 << DMA_SxCR_CHSEL_Pos;							// SPI2_TX is in DMA1 (Table 30) row for Channel 0, as Stream4
	cfg |= DMA_SxCR_DBM;																						// double buffer, M0AR first
	cfg |= DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;					// interrupt on complete, error, direct mode error
	dma->CR = cfg;
	
	ak_SpeakerEnable( true );			// power up codec & amplifier (if not already)
	ak_SetMute( false );

	// clocks from codec are stable at selected frequency-- from ak_SetMasterFreq()
	i2s->instance->I2SCFGR |= I2S_MODE_ENAB;		// enable I2S device-- starts transmitting audio
	dma->CR |= DMA_SxCR_EN;											// enable DMA
  return ARM_DRIVER_OK;
}

static int32_t 								I2S_Receive( void *data, uint32_t num, I2S_RESOURCES *i2s){													// start data receive
/**
  \fn          int32_t I2S_Receive (void *data, uint32_t num, I2S_RESOURCES *i2s)
  \brief       Start receiving data from I2S receiver.
  \param[out]  data  Pointer to buffer for data to receive from I2S receiver
  \param[in]   num   Number of data items to receive ( #stereo samples )
  \param[in]   i2s   Pointer to I2S resources
  \return      \ref execution_status
*/
	return todo( "I2S_Receive" );
}

static uint32_t 							I2S_GetTxCount( I2S_RESOURCES *i2s ){																								// return nSamples transmitted so far
/**
  \fn          uint32_t I2S_GetTxCount (I2S_RESOURCES *i2s)
  \brief       Get transmitted data count.
  \param[in]   i2s       Pointer to I2S resources
  \return      number of data items transmitted
*/
  uint32_t cnt = i2s->info->tx_cnt / i2s->info->data_bytes;  // Convert count in bytes to count of samples
  return (cnt);
}

static uint32_t 							I2S_GetRxCount( I2S_RESOURCES *i2s ){ 																							// return nSamples received so far
  uint32_t cnt = i2s->info->rx_cnt / i2s->info->data_bytes;  // Convert count in bytes to count of samples
  return (cnt);
}

static int32_t 								I2S_Control( uint32_t control, uint32_t arg1, uint32_t arg2, I2S_RESOURCES *i2s ){	// control I2S Interface.
//   			control  Operation
//   			arg1     Argument 1 of operation (optional)
//   			arg2     Argument 2 of operation (optional)
//   			i2s      Pointer to I2S resources
//				return      common \ref execution_status and driver specific \ref sai_execution_status
  if ((i2s->info->flags & I2S_FLAG_POWERED) == 0U) { return ARM_DRIVER_ERROR; }
	
	uint32_t req = control & ARM_SAI_CONTROL_Msk;
	if ( req == ARM_SAI_ABORT_SEND ){
		  controlTXMT( i2s, (controlTyp) arg1 ); 
			return ARM_DRIVER_OK;
  }
	if ( req == ARM_SAI_ABORT_RECEIVE ){
			abortRCV( i2s ); 
			return ARM_DRIVER_OK;
	}
  if (i2s->info->status.tx_busy || i2s->info->status.rx_busy) { return ARM_DRIVER_ERROR_BUSY; }

  if ( req == ARM_SAI_CONFIGURE_TX ){
		// only configuration supported
		const uint32_t supported = ARM_SAI_CONFIGURE_TX | ARM_SAI_MODE_SLAVE  | ARM_SAI_ASYNCHRONOUS |
			ARM_SAI_PROTOCOL_I2S | ARM_SAI_DATA_SIZE(16);
		bool monoMode = (control & ARM_SAI_MONO_MODE) != 0;
		if ( (control & ~ARM_SAI_MONO_MODE) != supported )
				return ARM_DRIVER_ERROR_UNSUPPORTED;
		I2S_Configure( i2s, arg2, monoMode );		// configure audio freq
		return ARM_DRIVER_OK;

  } else
		return ARM_DRIVER_ERROR_UNSUPPORTED;    // no other options supported 
}

static ARM_SAI_STATUS 				I2S_GetStatus( I2S_RESOURCES *i2s ) {																								// return current status
  ARM_SAI_STATUS status;
  // only returns values updated by IRQ_Handler?
  status.frame_error  = i2s->info->status.frame_error;
  status.rx_busy      = i2s->info->status.rx_busy;
  status.rx_overflow  = i2s->info->status.rx_overflow;
  status.tx_busy      = i2s->info->status.tx_busy;
  status.tx_underflow = i2s->info->status.tx_underflow;

  return status;
}
/*static void 									I2S_IRQHandler( I2S_RESOURCES *i2s ){																								// I2S2 interrupts -- per data word events, reports completion to callback
  uint16_t sr;
  uint32_t event = 0;
	NVIC_ClearPendingIRQ( i2s->irq_num );		// reset IRQ

  sr = i2s->instance->SR;    // get status register
	const int SPI_ERRS = SPI_SR_OVR | SPI_SR_UDR;
	const int SPI_MSK = SPI_ERRS | SPI_SR_TXE | SPI_SR_RXNE;
	
	if ( (sr & SPI_MSK) == SPI_SR_TXE ){		// no error-- word transmitted 
		I2S_SendWord( i2s );
		return;
	}
	
	if ( (sr & SPI_MSK) == SPI_SR_RXNE ){		// no error-- word received 
		I2S_GetWord( i2s );
		return;
	}
	
	if ( (sr & SPI_ERRS) != 0 ){
		if ((sr & SPI_SR_OVR) != 0U) {
			i2s->info->status.rx_overflow = 1;
			uint16_t tmp = i2s->instance->DR;    // Clear Overrun flag by reading data register
			event |=	ARM_SAI_EVENT_RX_OVERFLOW;
		}
		if ((sr & SPI_SR_UDR) != 0U) {    // Underrun flag is set -- SLAVE mode only
			i2s->info->status.tx_underflow = 1;
			event |= ARM_SAI_EVENT_TX_UNDERFLOW;
		}
    i2s->info->cb_event( event );
	}
}
*/

void 													I2S_TX_DMA_Complete( uint32_t intReqs, const I2S_RESOURCES *i2s ) {									// handle TX complete
	// DMA1_Stream4_IRQ clears int req & passes reqBits (shifted to stream0) as intReqs
	// call cb_event to chain to next buffer
	const int DMA_COMPLETE = DMA_LISR_TCIF0;			// transfer complete flag (as Stream 0)
	const int DMA_ERROR_MSK = DMA_LISR_TEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_FEIF0;  // mask for transfer error, direct mode error & fifi error 
	
	if ( (intReqs & DMA_COMPLETE) != 0 )					// SEND complete first, if appropriate
		i2s->info->cb_event( ARM_SAI_EVENT_SEND_COMPLETE );
		
	if ( (intReqs & DMA_ERROR_MSK) != 0 )
		i2s->info->cb_event( intReqs << 3 );			// << so they don't look like ARM RX or TX complete
} 
void 													I2S_RX_DMA_Complete( uint32_t events, const I2S_RESOURCES *i2s ) {					// handle RX complete
/* TODO: Receive
  DMA1_Stream4->CR =  ;
	
  DMA_ChannelDisable( i2s->dma_rx->ptr_channel );
  int bytesLeft = DMA_ChannelTransferItemCount( i2s->dma_rx->ptr_channel );		// read bytes left in DMA channel
  if ( bytesLeft == 0 )  // transfer complete?
		i2s->info->rx_cnt = i2s->info->rx_req;
	else // RX DMA Complete caused by transfer abort-- no event
    return;

  i2s->info->status.rx_busy = 0U;
  if (i2s->info->cb_event != NULL)
    i2s->info->cb_event( ARM_SAI_EVENT_RECEIVE_COMPLETE );
*/
}

//------------------
// I2S0 Driver Wrapper functions -- pass resource ptr to general fn
static ARM_SAI_CAPABILITIES 	I2S2_GetCapabilities (void) {
  return I2S_GetCapabilities (&I2S2_Resources);
}
static int32_t 								I2S2_Initialize (ARM_SAI_SignalEvent_t cb_event) {
  return I2S_Initialize (cb_event, &I2S2_Resources);
}
static int32_t 								I2S2_Uninitialize (void) {
  return I2S_Uninitialize (&I2S2_Resources);
}
static int32_t 								I2S2_PowerControl (ARM_POWER_STATE state) {
  return I2S_PowerControl (state, &I2S2_Resources);
}
static int32_t 								I2S2_Send (const void *data, uint32_t num) {
  return I2S2only_Send( data, num, &I2S2_Resources );
}
static int32_t 								I2S2_Receive (void *data, uint32_t num) { 
  return I2S_Receive (data, num, &I2S2_Resources);
}
static uint32_t 							I2S2_GetTxCount (void) {
  return I2S_GetTxCount (&I2S2_Resources);
}
static uint32_t 							I2S2_GetRxCount (void) {
  return I2S_GetRxCount (&I2S2_Resources);
}
static int32_t 								I2S2_Control (uint32_t control, uint32_t arg1, uint32_t arg2) {
  return I2S_Control (control, arg1, arg2, &I2S2_Resources);
}
static ARM_SAI_STATUS 				I2S2_GetStatus (void) {
  return I2S_GetStatus (&I2S2_Resources);
}
/*void 													I2S2_IRQHandler (void) {				// SPI2 == I2S0
  I2S_IRQHandler (&I2S2_Resources);
} */
void 													DMA1_Stream4_IRQ( void ){				// I2S2 TX DMA interrupt
	// get values of Stream4 interrupt flags
	int events = DMA1->HISR & (DMA_HISR_TCIF4 | DMA_HISR_HTIF4 | DMA_HISR_TEIF4 | DMA_HISR_DMEIF4 | DMA_HISR_FEIF4 );
  DMA1->HIFCR |= events;		// clear all
	
	I2S_TX_DMA_Complete( events, &I2S2_Resources );		// pass flags shifted into Stream0 position
}
void 													DMA1_Stream3_IRQ( void ){				// I2S2 RX DMA interrupt
	// get values of Stream3 interrupt flags
	int events = DMA1->LISR & (DMA_LISR_TCIF3 | DMA_LISR_HTIF3 | DMA_LISR_TEIF3 | DMA_LISR_DMEIF3 | DMA_LISR_FEIF3 );
  DMA1->LIFCR |= events;		// clear all

	I2S_RX_DMA_Complete( events >> DMA_LISR_FEIF3_Pos, &I2S2_Resources );		// pass flags shifted into Stream0 position
}


//
// define externally referenced SAI0 Driver Control Block == I2S2
ARM_DRIVER_SAI Driver_SAI0 = {
    I2Sx_GetVersion,
    I2S2_GetCapabilities,
    I2S2_Initialize,
    I2S2_Uninitialize,
    I2S2_PowerControl,
    I2S2_Send,
    I2S2_Receive,
    I2S2_GetTxCount,
    I2S2_GetRxCount,
    I2S2_Control,
    I2S2_GetStatus
};

//*********** end I2S_stm32F10x.c 

