/*
	Light Organ

	Improvements to be made:
		allow multiple input sources (multiple ADC channels)
		allow switches to configure input source, or filter bands
		incorporate beat detection
		increase the duty cycle setting resolution (to 11 bits, say)

	Problems:
		the low pass filter always seems to register a signal
		maybe this is 60 Hz hum?
*/

/* !!! try flattening dynamics of output:
	- clip output to 0 / 1 
 try other frequency bands*/

#include <avr/io.h>

//standard library
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <util/delay.h>


//peripheral drivers
#include "clksys_driver.h"
#include "event_system_driver.h"
#include "tc_driver.h"
#include "pmic_driver.h"
#include "ac_driver.h"
#include "adc_driver.h"
#include "dac_driver.h"

//device drivers
#include "display.h"


//helper
	#define max(a,b) ((a>b)?a:b)
	#define min(a,b) ((a<b)?a:b)

//filter options
	#define FILTERS_ENABLE
//	#define ANTIALIAS_ENABLE
	#define ANTIALIAS_RATE 2
	//the following is log(ANTIALIAS_RATE)
	#define ANTIALIAS_SHIFT 1

//input options
	#define INPUT_CLONE_SAMPLE0

//debug-mode options
//	#define DISPLAY_ENABLE
//	#define	FLASHER_ENABLE

//	#define PRINT_DEBUG
//	#define PRINT_DUTY_CYCLES

	#define DEBUG_FILTER_TEST
//	#define DEBUG_CLONE_DUTY0

//peripheral configuration
	#define UI_timer TCD0
	#define UI_vect TCD0_OVF_vect

	#define ms_UI_interval 1

	#define PHASE_timer TCE0
	#define PHASE_port PORTD

	#define EFFECTIVE_rate 11000
	#ifdef ANTIALIAS_ENABLE
		#define SAMPLE_rate (EFFECTIVE_rate * ANTIALIAS_RATE)
	#endif
	#ifndef ANTIALIAS_ENABLE
		#define SAMPLE_rate EFFECTIVE_rate
	#endif

	#define SAMPLE_timer TCD1
	#define SAMPLE_evsrc EVSYS_CHMUX_TCD1_OVF_gc

	#define CAPTURE_timer TCC1
	#define CAPTURE_vect TCC1_OVF_vect
	
	//the number of samples to collect in a bin for the beat detection algorithm
	//binsize must equal 2^binbits
	//note that the sound intensity will be sampled at rate/binsize, choose a binsize to make this around 40 Hz
	#define CAPTURE_binsize 256 //### 1024 //### 32 //###256
	#define CAPTURE_binbits 8 //### 10 //### 5 //###8
	
	#define HISTORY_size 43 //### 43

	#define AC_pin AC_MUXPOS_PIN2_gc

	//the gain for the ADC
	//this is configured for use with line level audio
	#define ADC_gain ADC_CH_GAIN_2X_gc

	//the pin for the AREF / 2 mid level reference voltage (for positive and negative ADC MUX)
	#define ADC_AREF_DIV_2_MUXPOS_pin ADC_CH_MUXPOS_PIN1_gc
	#define ADC_AREF_DIV_2_MUXNEG_pin ADC_CH_MUXNEG_PIN1_gc

//event related variables
	volatile uint32_t msClock=0;

	//these phase variables are expressed in terms of ticks of the phase clock
	uint16_t phasePeriod=0;
	volatile uint16_t phasePeriodLast=0;
	uint16_t phasePeriodMin=3200;

	volatile bool fPhaseBegin=false;
	volatile bool fPhaseEnd=false;
	volatile bool fSampleCaptured=false;
	volatile bool fCaptureBinFilled=false;

//	volatile uint16_t sampleLast=0;
//	volatile uint16_t sampleMax=0;
//	volatile uint16_t sampleMin=4096;

//	volatile uint32_t sampleSum=0;
//	volatile uint16_t sampleSumCount=0;

	volatile uint8_t dutyA, dutyB, dutyC;

//debugging variables
	volatile uint16_t debug1, debug2, debug3, debug4;

//a string for building output
	char string [17];

//the error offset of the adc
	int16_t ADC_offset=0;




//function prototypes
	void initializeMCU();
	void initializeUITimer();
	void initializePhaseTimer();
	void initializePhaseComparator();
	void initializeSampleTimer();
	void initializeCaptureTimer();
	void initializeADC();

	void initializeDAC();

	void enableInterrupts();

	void setDutyCycles(uint8_t, uint8_t, uint8_t);
	void findPhasePeriod();

	void error(char *);

	void printDutyCycles();
	void debugFilterTest();

int main(void) {

	//initialize the MCU clock
	initializeMCU();

	#ifdef DEBUG_FILTER_TEST
		debugFilterTest();
	#endif

	//initialize the display and display the title if the display is enabled
	#ifdef DISPLAY_ENABLE
		//initialize the display (the SPI port)
		display_init(16,2);	

		//display the boot screen
		display_returnHome();
		display_printString("Light Organ");
		
	#endif
	
	//initialize peripherals
	initializeUITimer();
	initializePhaseTimer();
	initializePhaseComparator();

	#ifndef FLASHER_ENABLE
		initializeSampleTimer();
		initializeCaptureTimer();
		initializeADC();
		initializeDAC();
	#endif

	enableInterrupts();
	
//	while (true) {
		//find the nominal phase period
		findPhasePeriod();	

		//### hack - because the phase period detection is not working
		//phasePeriod=1800;


		//display the period if the display is enabled
		#ifdef DISPLAY_ENABLE
			display_moveCursor(0x40);
			sprintf(string, "period: %i", phasePeriod);
			display_printString(string);
//			_delay_ms(1000);
			_delay_ms(300);
		#endif
//	}

	#ifdef DISPLAY_ENABLE
		volatile uint32_t msDisplayUpdate=0;
		#define DISPLAY_PERIOD 100
	#endif


	//some variables for generating fun duty cycle changes
	#ifdef FLASHER_ENABLE
		int value=1;
		int dir=1;
		int step=2;
		int stepdir=1;
		int stepstep=1;
	#endif

	volatile uint32_t msValueUpdate=0;
//	uint16_t energy=0;
	volatile uint32_t msStrobe=0;

	//enter the main event loop
	while (true) {
		//keep track of the minimum phase period
		if (fPhaseEnd) {

			//PHASE_port.OUTCLR = PIN0_bm;
			fPhaseEnd=false;
			phasePeriodMin=(phasePeriodLast<phasePeriodMin) ? phasePeriodLast : phasePeriodMin;
			//debug3=phasePeriodLast;
		}

//		if(msClock>=msStrobe) {
//			PHASE_port.OUTTGL=PIN1_bm;
//			msStrobe=msClock+2;
//		}


		#ifdef FLASHER_ENABLE
			//update the value to create a flashing light effect
			if (msClock>=msValueUpdate) {
				setDutyCycles(value, 255-value, 255-value);

				phasePeriodMin=30000;

				if (step<=1 || step>=128)
					stepdir=-stepdir;

				if (value<=0 || value>=255) {
					dir=-dir;
					step+=stepdir*stepstep;
				}
				value+=dir*step;

				if (value<0) value=0;
				if (value>255) value=255;
			
				msValueUpdate+=30;
			}
		#endif



		//update the display
		#ifdef DISPLAY_ENABLE
			if (msClock>=msDisplayUpdate) {
				display_moveCursor(0x0E);
				display_printString("  ");

				if (fSampleCaptured) {
					display_moveCursor(0x0E);
					display_printChar('S');
					fSampleCaptured=false;
				}

				if (fCaptureBinFilled) {
					display_moveCursor(0x0F);
					display_printChar('F');
					fCaptureBinFilled=false;
				}

				#ifdef PRINT_DEBUG
					sprintf(string, "%5i ", debug1);
					display_moveCursor(0x00);
					display_printString(string);

					sprintf(string, " %5i", debug2);
					display_moveCursor(0x06);
					display_printString(string);

					sprintf(string, "%5i ", debug3);
					display_moveCursor(0x40);
					display_printString(string);

					sprintf(string, " %5i", debug4);
					display_moveCursor(0x46);
					display_printString(string);
				#endif

				#ifdef PRINT_DUTY_CYCLES
					printDutyCycles();
				#endif

				msDisplayUpdate+=DISPLAY_PERIOD;
			}
		#endif

	}
}


//print the duty cycle to the display
void printDutyCycles() {
	sprintf(string, "a:%2X, b:%2X, c:%2X", dutyA, dutyB, dutyC);
	display_moveCursor(0x40);
	display_printString(string);
}

//set the duty cycle for each output channel
//duty cycle is expressed as a byte value
//0 is fully off, 255 is fully on
void setDutyCycles(uint8_t _dutyA, uint8_t _dutyB, uint8_t _dutyC) {
	//save the duty settings as global variables (for debugging)
	dutyA=_dutyA; dutyB=_dutyB; dutyC=_dutyC;

	//calculate the number of ticks given the duty cycle
	uint32_t prodA = (uint32_t) _dutyA * phasePeriod;
	uint16_t ticksA= phasePeriod - (prodA/255);

	//set the timer comparator value
	//the comparator output pins for the phase timer will be the duty cycle output
	TC_SetCompareA(&PHASE_timer, ticksA);

	uint32_t prodB = (uint32_t)_dutyB * phasePeriod;
	uint16_t ticksB=phasePeriod - (prodB/255);
	//note: comparator B didn't work, for some reason, so we use comparator D for output channel B instead
	TC_SetCompareD(&PHASE_timer, ticksB);

	uint32_t prodC = (uint32_t)_dutyC * phasePeriod;
	uint16_t ticksC=phasePeriod - (prodC/255);
	TC_SetCompareC(&PHASE_timer, ticksC);
	
//	debug1=(uint16_t) (prodA/255);
//	debug2=ticksA;
}

//find the noninal AC phase
void findPhasePeriod() {
	uint32_t sum=0;

	uint32_t msTimeout=msClock+10000;

	//average the phase period over one second
	for (int i=0;i<60;i++) {
		while (!fPhaseEnd) {
			if (msClock>=msTimeout)
				error("finding period");
		}
		fPhaseEnd=false;

		sum+=phasePeriodLast;
	}

	phasePeriod=sum/60;
}




//show an error and hang
void error(char * msg) {
	display_clear();
	_delay_ms(3);

	display_moveCursor(0x00);
	display_printString("ERROR");
	display_moveCursor(0x40);
	display_printString(msg);

	while (true) 
		nop();
}

//the UI timer interrupt
ISR(UI_vect)
{
	msClock+=ms_UI_interval;
}




//the analog comparator rising edge interrupt
//note this is mislabeled as "COMP0_vect" in the ATXMEGA datasheet!
ISR(ACA_AC0_vect)
{
	//turn on the counter
	TC0_ConfigClockSource(&PHASE_timer, TC_CLKSEL_DIV64_gc);   

	//flag that a zero-cross rising edge event has occured
	fPhaseBegin=true;

}

//the analog comparator falling edge interrupt
//note this is mislabeled as "COMP1_vect" in the ATXMEGA datasheet!
ISR(ACA_AC1_vect)
{
	//turn off phase timer
	TC0_ConfigClockSource(&PHASE_timer, TC_CLKSEL_OFF_gc);   

	//reset the output pins
	PHASE_port.OUTCLR = PIN0_bm | PIN1_bm | PIN2_bm;

	//force update of compare values
	//this would normally happen when the counter overflows
	TC_ForceUpdate(&PHASE_timer);

	//read the counter
	phasePeriodLast=PHASE_timer.CNT;

	//set the counter to 0 and clear the comparators
	TC_Restart(&PHASE_timer);

	//flag that a zero-cross falling edge event has occured
	fPhaseEnd=true;

}

//the interrupts for the comparators - when a comparator is triggered, turn on the cooresponding pin
//these could possibly be optimized by making them ISR_NAKED and doing the setup by hand
ISR(TCE0_CCA_vect) {
	PHASE_port.OUTSET = PIN0_bm;
}
ISR(TCE0_CCD_vect) {
	PHASE_port.OUTSET = PIN1_bm;
}
ISR(TCE0_CCC_vect) {
	PHASE_port.OUTSET = PIN2_bm;
}


//record the maximum sample level for debugging
uint16_t maxLevel=0;

//the number of input channels
#define INPUT_CHANNELS 3

//!!! These should be initialized to 0
//keeps a running tally of the energy for each sample
uint32_t energy_sum[INPUT_CHANNELS];


#ifdef FILTERS_ENABLE
	//the following variables are used for the three bandpass filters
	int32_t lowband_x0=0, lowband_x1=0, lowband_x2=0;
	int32_t lowband_y0=0, lowband_y1=0, lowband_y2=0;

	int32_t midband_x0=0, midband_x1=0, midband_x2=0;
	int32_t midband_y0=0, midband_y1=0, midband_y2=0;

	int32_t highband_x0=0, highband_x1=0, highband_x2=0;
	int32_t highband_y0=0, highband_y1=0, highband_y2=0;
 
 	#define FILTER_CONSTANT_SHIFT 12

//These were created using Interactive Digital Filter Design, http://www-users.cs.york.ac.uk/~fisher/mkfilter
// (and made fixed point with filters.php)
// they are shifted to have 12 fractional bits (ie. Q20.12 format)

// lowband_: 160-320 HZ
#define lowband_INVGAIN 386
#define lowband_B0 3549
// midband_: 2500-4000 HZ
#define midband_INVGAIN 1603
#define midband_B0 -891
#define midband_B1 -843
// highband_: 4000-5500 HZ
#define highband_INVGAIN 1572
#define highband_B0 -1139
#define highband_B1 -4864

	//compute a fast lowpass filter using the given parameters and sample history
	//the filter constants are assumed to have FILTER_CONSTANT_SHIFT fractional bits
	#define fastlowpass(sample, x0, x1, y0, y1, invgain, b0) \
		x0=x1; y0=y1;\
		x1=(sample*invgain);\
		y1=((x0+x1) + (b0 *y0)) >> FILTER_CONSTANT_SHIFT;\
		sample=y1;

	//compute a fast bandpass filter using the given parameters and sample history
	//the filter constants are assumed to have FILTER_CONSTANT_SHIFT fractional bits
	#define fastbandpass(sample, x0, x1, x2, y0, y1, y2, invgain, b0, b1) \
		x0=x1; x1=x2; y0=y1; y1=y2;\
		x2=(sample*invgain) ;\
		y2=((x2-x0) + (b0*y0) + (b1*y1)) >> FILTER_CONSTANT_SHIFT;\
		sample=y2;


	#ifdef DEBUG_FILTER_TEST
		#define TEST_SAMPLES 16
		#define TO_FIXED(x) ((int32_t)(x*(1<<12)))
		#define TO_FIXED_SMP(x) ((int32_t)(x*2048))
		void debugFilterTest() {
			int16_t results[TEST_SAMPLES];
		
			int32_t x0=0, x1=0, x2=0, y0=0, y1=0, y2=0;
			int32_t b0 = lowband_B0;
			int32_t b1 = 0;
			int32_t invgain = lowband_INVGAIN;
		

			int32_t x0_hist [TEST_SAMPLES];			
			int32_t x1_hist [TEST_SAMPLES];			
			int32_t x2_hist [TEST_SAMPLES];			
			int32_t y0_hist [TEST_SAMPLES];			
			int32_t y1_hist [TEST_SAMPLES];			
			int32_t y2_hist [TEST_SAMPLES];			

			int32_t sample=2048;
			fastlowpass(sample, x0, x1, y0, y1, invgain, b0);
			//for (int i=0; i<1000; i++) {
			//	sample=0;
			//	fastlowpass(sample, x0, x1, y0, y1, invgain, b0);
			//}
			

			for (int i=0; i<TEST_SAMPLES; i++) {
				sample=0;
				fastlowpass(sample, x0, x1, y0, y1, invgain, b0);

				x0_hist[i]=x0;
				x1_hist[i]=x1;
				x2_hist[i]=x2;
				y0_hist[i]=y0;
				y1_hist[i]=y1;
				y2_hist[i]=y2;
				results[i]=y2;
			}

			__builtin_avr_delay_cycles(4);
		}
	#endif


#endif

#ifdef ANTIALIAS_ENABLE
	int32_t antialias_sample[INPUT_CHANNELS];
	int8_t antialias_count=0;
#endif

//the sample capture interrupt from the ADC
//the ADC conversion is triggered by the sample timer and this ISR is called when the conversion is complete
ISR(ADCA_CH0_vect) {
	

	//int16_t sample[INPUT_CHANNELS];
	int32_t sample[INPUT_CHANNELS];

	//read the samples (signed)
	sample[0]=ADC_ResultCh_GetWord_Signed(&ADCA.CH0, ADC_offset);

	#ifndef INPUT_CLONE_SAMPLE0
		sample[1]=ADC_ResultCh_GetWord_Signed(&ADCA.CH1, ADC_offset);
		sample[2]=ADC_ResultCh_GetWord_Signed(&ADCA.CH2, ADC_offset);
	#endif




//FUDGE FACTOR!!!
	sample[0]-=84;


#ifdef ANTIALIAS_ENABLE
	//sample into the antialiasing filter
	antialias_sample[0]+=sample[0];

	//only process channels 1 and 2 if necessary
	#ifndef INPUT_CLONE_SAMPLE0
		antialias_sample[1]+=sample[1];
		antialias_sample[2]+=sample[2];
	#endif

	antialias_count++;

	//if the antialiasing filter is "full" output the signal
	if (antialias_count>=ANTIALIAS_RATE) {
		sample[0] = antialias_sample[0] >> ANTIALIAS_SHIFT;
		antialias_sample[0]=0;
		//only process channels 1 and 2 if necessary
		#ifndef INPUT_CLONE_SAMPLE0
			sample[1] = antialias_sample[1] >> ANTIALIAS_SHIFT;
			sample[2] = antialias_sample[2] >> ANTIALIAS_SHIFT;
			antialias_sample[1]=0;
			antialias_sample[2]=0;
		#endif
				
		
#endif

#ifndef ANTIALIAS_ENABLE
	if (true) {
#endif

		#ifdef INPUT_CLONE_SAMPLE0
			sample[1]=sample[0];
			sample[2]=sample[0];
		#endif

		#ifdef FILTERS_ENABLE
			debug1=sample[0];
			fastlowpass(sample[0], lowband_x0, lowband_x1, lowband_y0, lowband_y1, lowband_INVGAIN, lowband_B0);
		//	sample[0]=0;

		//more FUDGE!!!
			sample[0]-=35;

			debug2=sample[1];
			fastbandpass(sample[1], midband_x0, midband_x1, midband_x2, midband_y0, midband_y1, midband_y2, midband_INVGAIN, midband_B0, midband_B1);

			debug3=sample[2];
			fastbandpass(sample[2], highband_x0, highband_x1, highband_x2, highband_y0, highband_y1, highband_y2, highband_INVGAIN, highband_B0, highband_B1);

		#endif

		//###
		//	sample[0]=sample[0]>>6;
		//	sample[0]=sample[0]<<6;

		//this is only for sample 0?
	//	if (sample[0]>=0) 
	//		maxLevel=max(sample[0],maxLevel);
	//	else
	//		maxLevel=max(-sample[0], maxLevel);


		//take the energy of the samples and add them to the running total
		//we don't shift down by the full 16 (11?) bits, just to leave a little numerical resolution
		for (int i=0;i<INPUT_CHANNELS;i++)
			energy_sum[i]+=((int32_t)sample[i]*sample[i])>>2;

	
				// what about this instead of the square?
				//energy_sum_0+=(sample_0>=0)?sample_0:-sample_0;

		//put a sample to the DAC for debugging
		int16_t sampleOut=sample[0];
		DAC_Channel_Write(&DACB, (uint16_t) (sampleOut + 2048), CH0);//+ADC_offset/2, CH0);


		//increase the sample capture counter by sending event 4
		EVSYS_ManualTrigger(0, 0x10);


		//flag that the sample event occured
		fSampleCaptured=true;
	}
}



uint8_t historyIndex=0;
//should these arrays be initialized to 0?
uint16_t energy_history[INPUT_CHANNELS][HISTORY_size];

//uint32_t lowband_energy_historySum=0;
//uint32_t midband_energy_historySum=0;
//uint32_t highband_energy_historySum=0;

//this interrupt is called after every BINSIZE samples are filtered and summed into their three channels
//here we decide how to respond to the audio signal
ISR(CAPTURE_vect) {

	//###
	debug1=maxLevel;
	maxLevel=0;

	///
	// ** This algorithm is taken from http://www.gamedev.net/reference/articles/article1952.asp **
	///

	// * 1. Take the average input energy from the last bin of samples *
	uint16_t energy_inst[INPUT_CHANNELS];
	
	for (int i=0;i<INPUT_CHANNELS;i++) {
		//grab the sums lickety-quick so they don't change while we are processing
		//and divide them by BINSIZE to find their average
		energy_inst[i]=energy_sum[i] >> CAPTURE_binbits;

		//reset the sums
		energy_sum[i]=0;
	}

	
	// * 2.  Calculate the max and min history values for each input channel *

	uint16_t energy_max[INPUT_CHANNELS];
	uint16_t energy_min[INPUT_CHANNELS];
	
	uint16_t energy_all_max=0;
	uint16_t energy_all_min=65535;

	for (int i=0;i<INPUT_CHANNELS;i++) {
		//update the history array with the new values
		energy_history[i][historyIndex]=energy_inst[i];
		
		//now find maximum energy from the last HISTORY_size bin captures 
		//(this should be about a 1 second history)
		// the strength of the output channel will scale to this maximum

		energy_max[i]=0;
		energy_min[i]=65535;
		for (uint8_t j=0;j<HISTORY_size;j++) {
			energy_max[i]=max(energy_history[i][j],energy_max[i]);
			energy_min[i]=min(energy_history[i][j],energy_min[i]);
		}

		//find the max and min for all channels
		energy_all_max=max(energy_all_max,energy_max[i]);
		energy_all_min=min(energy_all_min,energy_min[i]);
	
	}

	// * 3. Fudge the max energy
		//this value is basically voodoo
		//it serves to ignore small variations in the energy though, basically requires a certain minimum intensity of sound to register a response
	for (int i=0;i<INPUT_CHANNELS;i++) {
		energy_max[i]=max(energy_max[i], 64);
	}
	energy_all_max=max(energy_all_max, 64); //was 16
	
	// * 3. Calculate the outputs *
		//for the time being, use the instantaneous energy, scaled to the maximum energy in the history
		// compare this to the instantaneous energy

		//other options are to use: 
		//	1) the rate of change of energy
		//	2) the difference of the energy from the 1 second background
		//	3) the simple instantaneous energy
		//  ...?

	uint8_t duty[INPUT_CHANNELS];
	for (int i=0;i<INPUT_CHANNELS;i++) {

/* this version uses just the max energy on the channel in question */
		if (energy_max[i]-energy_min[i]>1) {
			//scale the output to the maximum recent energy accross all channels 
				// (this removes all long-term dynamics)
			uint32_t scale = (((uint32_t)energy_inst[i] - energy_min[i]) *256) / (energy_max[i] - energy_min[i]);

			if (i==0) debug1=scale;

			duty[i] = min(scale,255);
		}

/* this version uses the max energy accross all channels
		if (energy_all_max-energy_all_min>1) {
			//scale the output to the maximum recent energy accross all channels 
				// (this removes all long-term dynamics)
			uint32_t scale = (((uint32_t)energy_inst[i] - energy_all_min) *256) / (energy_all_max - energy_all_min);

			if (i==0) debug1=scale;

			duty[i] = min(scale,255);
		}
*/

		//should we have a minimum duty value and scale relative to that?
	}	
	
	// * 4. Increment the history index *
	historyIndex++;
	if (historyIndex >= HISTORY_size)
		historyIndex = 0;

	
	debug2 = energy_inst[0];
	debug3 = energy_all_min;
	debug4 = energy_all_max;
	

	//copy the duty signal from channel 0 to the other channels for debugging
	#ifdef DEBUG_CLONE_DUTY0
		duty[1] = duty[0];
		duty[2] = duty[0];
	#endif


	#ifndef FLASHER_ENABLE
		//scale the duty cycle down to the range 0..255
		setDutyCycles(duty[0], duty[1], duty[2]);
	#endif


	fCaptureBinFilled=true;
}


//catch-all ISR - we shouldn't need this
//ISR(BADISR_vect)
//{
//    // user code here
//}

//set up the system clock
void initializeMCU() {
	//enable the 32MHz RC clock
	CLKSYS_Enable(OSC_RC32MEN_bm);
	//block till the clock is ready
	while (! CLKSYS_IsReady(OSC_RC32MRDY_bm)) {}
	//use the 32MHz RC clock as the main system clock
	CLKSYS_Main_ClockSource_Select(CLK_SCLKSEL_RC32M_gc);
}

//enable interrupts
void enableInterrupts() {
	PMIC_SetVectorLocationToApplication();
	PMIC_EnableLowLevel();
	PMIC_EnableMediumLevel();
	PMIC_EnableHighLevel();
	sei();
}

//set up a ui timer and enable its interrupt:
void initializeUITimer() {
	//set the mode
	TC0_ConfigWGM(&UI_timer, TC_WGMODE_NORMAL_gc);
	//set the clock source (prescale by 128)
	TC0_ConfigClockSource(&UI_timer, TC_CLKSEL_DIV256_gc);   
	//set the wave period (32 MHz / 256 = 125000 ticks / sec = 125 tics / ms)
	TC_SetPeriod( &UI_timer, 125*ms_UI_interval);
	//set the interupt
	TC0_SetOverflowIntLevel(&UI_timer, TC_OVFINTLVL_MED_gc);
}

//set up a timer for the ac phase / zero-crossing detector
void initializePhaseTimer() {

	//set the timer to normal mode (no waveform generation)
	TC0_ConfigWGM(&PHASE_timer, TC_WGMODE_NORMAL_gc);

		//for automatic comparator pin output, we set the mode to single-slope waveform generation
		//this puts the results of the enabled comparator channels on the coresponding output pins
		//TC0_ConfigWGM(&PHASE_timer, TC_WGMODE_SS_gc);

	//set the clock OFF
	TC0_ConfigClockSource(&PHASE_timer, TC_CLKSEL_OFF_gc);   

	//later we will configure this clock to a prescale of 64
	//this gives a resolution of 500 ticks per ms, ie. 2us period
	//we should find about 4167 ticks per half cycle at 60 Hz

	//the comparator pins must be enabled as output
	PHASE_port.DIRSET = PIN0_bm | PIN1_bm | PIN2_bm ; 
	PHASE_port.OUTCLR = PIN0_bm | PIN1_bm | PIN2_bm ; 

	//the comparator channels A, B and C will implement the duty cycle for the three output channels
	//note: comparator B didn't work, for some reason, so we use comparator D for output channel B instead
	TC0_EnableCCChannels(&PHASE_timer, (TC0_CCAEN_bm | TC0_CCDEN_bm | TC0_CCCEN_bm));

	TC0_SetCCAIntLevel(&PHASE_timer, TC_CCAINTLVL_HI_gc);
	TC0_SetCCDIntLevel(&PHASE_timer, TC_CCDINTLVL_HI_gc);
	TC0_SetCCCIntLevel(&PHASE_timer, TC_CCCINTLVL_HI_gc);


		//if we use the comparator pin output, the ports also have to be inverted, yes?
		//PHASE_port.PIN0CTRL = PORT_INVEN_bm;
		//PHASE_port.PIN1CTRL = PORT_INVEN_bm;
		//PHASE_port.PIN2CTRL = PORT_INVEN_bm;


}

//sets up rising and falling edge phase comparators
void initializePhaseComparator() {
	//configure the analog comparator (AC) voltage scaler
	AC_ConfigVoltageScaler(&ACA, 1);//1);//63); 
				//the voltage scaler was set to 1, once upon a time
				//does 63 work better? who the eff knows !!!
				
	//configure the AC inputs
	//AC 0 is the rising edge and AC 1 is the falling edge
	AC_ConfigMUX(&ACA, ANALOG_COMPARATOR0, AC_pin, AC_MUXNEG_SCALER_gc);
	AC_ConfigMUX(&ACA, ANALOG_COMPARATOR1, AC_pin, AC_MUXNEG_SCALER_gc);

	//configure the interrupts
	AC_ConfigInterrupt(&ACA, ANALOG_COMPARATOR0, AC_INTMODE_RISING_gc, AC_INTLVL_HI_gc);
	AC_ConfigInterrupt(&ACA, ANALOG_COMPARATOR1, AC_INTMODE_FALLING_gc, AC_INTLVL_HI_gc);

	//configure the hysteresis - this needs to be LARGE: small or no hysteresis causes glitches
	AC_ConfigHysteresis(&ACA, ANALOG_COMPARATOR0, AC_HYSMODE_LARGE_gc);
	AC_ConfigHysteresis(&ACA, ANALOG_COMPARATOR1, AC_HYSMODE_LARGE_gc);


	//enable the ACs
	//should the high-speed parameter (3rd argument) be true?
	AC_Enable(&ACA, ANALOG_COMPARATOR0, false);
	AC_Enable(&ACA, ANALOG_COMPARATOR1, false);
}

//initialize a one-channel analog to digital converter (ADC)
void initializeADC() {
	/* Move stored calibration values to ADC A. */
	ADC_CalibrationValues_Load(&ADCA);

	/*The ADC runs with 3 input channels:
		each is in differential mode, with the negative pin tied to AREF/2
		positive pins are AC coupled to the signal and biased to AREF/2
		the ADC reports a signed value, which will range from -1024 to 1024
			(we only have half precision on the ADC since the input is single ended)
			(maybe this could be corrected by using a balanced input topology)
	*/

	/* Set up ADC A to have signed conversion mode and 12 bit resolution. */
  	ADC_ConvMode_and_Resolution_Config(&ADCA, ADC_ConvMode_Signed, ADC_RESOLUTION_12BIT_gc);

	/* Set sample rate */
	ADC_Prescaler_Config(&ADCA, ADC_PRESCALER_DIV64_gc);

	/* Set referance voltage on ADC A to be 1 V.*/
	ADC_Reference_Config(&ADCA, ADC_REFSEL_AREFA_gc);

   	/* Get offset value for ADC A. */
	ADC_Ch_InputMode_and_Gain_Config(&ADCA.CH0,
	                                 ADC_CH_INPUTMODE_DIFFWGAIN_gc,
	                                 ADC_CH_GAIN_8X_gc);
   	ADC_Ch_InputMux_Config(&ADCA.CH0, ADC_AREF_DIV_2_MUXPOS_pin, ADC_AREF_DIV_2_MUXNEG_pin);

	ADC_Enable(&ADCA);

	/* Wait until common mode voltage is stable. This is configured to wait the minimum amount of time at 32MHz. */
	ADC_Wait_32MHz(&ADCA);
 	ADC_offset = ADC_Offset_Get_Signed(&ADCA, &ADCA.CH0, true);
    ADC_Disable(&ADCA);


	/*
	// Setup channel 0 to have differential input, with gain
	ADC_Ch_InputMode_and_Gain_Config(&ADCA.CH0,
	                                 ADC_CH_INPUTMODE_DIFFWGAIN_gc, 
									 ADC_gain);
	*/

	// Setup channel 0 to have differential input, with no gain
	ADC_Ch_InputMode_and_Gain_Config(&ADCA.CH0,
	                                 ADC_CH_INPUTMODE_DIFF_gc, 
									 ADC_DRIVER_CH_GAIN_NONE);

	// Set input to the channel 0 in ADC A to be PIN 4. The negative input is the AREF / 2 reference pin
	ADC_Ch_InputMux_Config(&ADCA.CH0, ADC_CH_MUXPOS_PIN4_gc, ADC_AREF_DIV_2_MUXNEG_pin);


	//!!! Here we should set up the other three ADC channels

	/* Setup only sweep channel 0.*/
	ADC_SweepChannels_Config(&ADCA, ADC_SWEEP_0_gc);

	/* Enable medium level interrupts on ADCA channel 0, on conversion complete. */
	ADC_Ch_Interrupts_Config(&ADCA.CH0, ADC_CH_INTMODE_COMPLETE_gc, ADC_CH_INTLVL_MED_gc);

	/* Perform automatic sample conversion on ADCA channel 0 when event 0 occurs. */
	ADC_Events_Config(&ADCA, ADC_EVSEL_0123_gc, ADC_EVACT_CH0_gc);

	/* Enable ADC A with VCC reference and signed conversion.*/
	ADC_Enable(&ADCA);

	/* Wait until common mode voltage is stable. This is configured to wait the minimum amount of time at 32MHz. */
	ADC_Wait_32MHz(&ADCA);
}

//set up a sample timer, let it be the source for event 0
void initializeSampleTimer() {
	//set the mode
	TC1_ConfigWGM( &SAMPLE_timer, TC_WGMODE_NORMAL_gc);
	//set the clock source
	TC1_ConfigClockSource(&SAMPLE_timer, TC_CLKSEL_DIV1_gc);
	//set the wave period (32MHz / 44.1KHz hZ = 725.62)
	TC_SetPeriod( &SAMPLE_timer, 32000000/SAMPLE_rate);
//	TC_SetPeriod( &SAMPLE_timer, 8000000/SAMPLE_rate);

			//a low sample rate: for testing
			//TC1_ConfigClockSource(&SAMPLE_timer, TC_CLKSEL_DIV256_gc);
			///TC_SetPeriod( &SAMPLE_timer, 65535/8);


	//send overflow events as SAMPLE
	EVSYS_SetEventSource(0, SAMPLE_evsrc);
}


//set up a sample capture timer and enable its interrupt:
void initializeCaptureTimer() {
	//set the mode
	TC1_ConfigWGM( &CAPTURE_timer, TC_WGMODE_NORMAL_gc);
	//set the clock source to event 4
	//event 4 will be triggered every time a sample is captured and filtered
	TC1_ConfigClockSource(&CAPTURE_timer, TC_CLKSEL_EVCH4_gc);
	//the timer overflows every BINSIZE captured samples
	TC_SetPeriod( &CAPTURE_timer, CAPTURE_binsize);
	//set the interupt
	TC1_SetOverflowIntLevel( &CAPTURE_timer, TC_OVFINTLVL_LO_gc );
}


//enable DACB and set it to output whenever the sample event (event 0) occurs
void initializeDAC() {
	DAC_SingleChannel_Enable(&DACB, DAC_REFSEL_AVCC_gc, false);
	DAC_EventAction_Set( &DACB, DAC_CH0TRIG_bm, 0 );
}
