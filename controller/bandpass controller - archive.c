

//the following variables are used for the three bandpass filters
	int16_t lowband_x0=0, lowband_x1=0, lowband_x2=0;
	int16_t lowband_y0=0, lowband_y1=0, lowband_y2=0;
	volatile uint32_t lowband_energy_sum=0;

	int16_t midband_x0=0, midband_x1=0, midband_x2=0;
	int16_t midband_y0=0, midband_y1=0, midband_y2=0;
	volatile uint32_t midband_energy_sum=0;

	int16_t highband_x0=0, highband_x1=0, highband_x2=0;
	int16_t highband_y0=0, highband_y1=0, highband_y2=0;
	volatile uint32_t highband_energy_sum=0;

	volatile uint16_t lowband_energy_inst=0;
	volatile uint16_t midband_energy_inst=0;
	volatile uint16_t highband_energy_inst=0;

//following are the filter parameters for the three bandpass filters
//the constants are scaled to 10 binary digits of fractional and 5 binary digits of whole numbers (along with a sign bit)
//ie. they are shifted 10 binary digits to the left
//These were created using Interactive Digital Filter Design, http://www-users.cs.york.ac.uk/~fisher/mkfilter
	
/*
	//low band: 40-320 Hz  (11 KHz sample rate)
	#define lowband_INVGAIN 81		// 1/1.256683577e+01
	#define lowband_B0 -872			// -0.8516140379  
	#define lowband_B1 1892			// 1.8477407416   

	//mid band: 320-1280 Hz
	#define midband_INVGAIN 233		// 1/4.386068625e+00
	#define midband_B0 -832			// -0.8129414462 
	#define midband_B1 1778			// 1.7365521893 
	
	//high band: 1280-5120 Hz
	#define highband_INVGAIN 685	// 1/1.493993194e+00
	#define highband_B0 329			// 0.3217651065 
	#define highband_B1 -387		// -0.3776717375 
*/

/*
	//low band: 20-200 Hz  (11 KHz sample rate)
	#define lowband_INVGAIN 55		// 1/1.891664577e+01
	#define lowband_B0 -934			// -0.9021293322    
	#define lowband_B1 1947			// 1.9008871652     
	//low band: 200 Hz lowpass 2nd order (11 KHz sample rate)
	#define lowband_INVGAIN 3		// 1/3.315618977e+02
	#define lowband_B0 -871			// -0.8508165960     
	#define lowband_B1 1883			// 1.8387524836      
*/
//colin HZ
	//20-100? 20-80?

	//150-300
	//400-600

	//1000-40000

	//low band: 20-200 Hz  (11 KHz sample rate)
	#define lowband_INVGAIN 55		// 1/1.891664577e+01
	#define lowband_B0 -934			// -0.9021293322    
	#define lowband_B1 1947			// 1.9008871652     

	//mid band: 300-600 Hz bandpass 1st order
	#define midband_INVGAIN 82		// 1/1.247795193e+01
	#define midband_B0 -817			// -0.8418070516  
	#define midband_B1 1831			// 1.7878562348   
	
	//high band: 800-3200 Hz bandpass 1st order
	#define highband_INVGAIN 467	// 1/2.194012665e+00
	#define highband_B0 -103		// -0.1002940335  
	#define highband_B1 604			// 0.5904328794  


//record the maximum sample level for debugging
	uint16_t maxLevel=0;

//the sample capture interrupt from the ADC
//the ADC conversion is triggered by the sample timer and this ISR is called when the conversion is complete
ISR(ADCA_CH0_vect) {
	
	//read the sample (unsigned)
//	uint16_t sampleRaw=ADC_ResultCh_GetWord_Unsigned(&ADCA.CH0, ADC_offset);
//	int16_t sample = sampleRaw-2048;

	//read the sample (signed)
	int16_t sample=ADC_ResultCh_GetWord_Signed(&ADCA.CH0, ADC_offset);

//	//discard 5 least significant bits
//	sample=sample>>5;
//	sample=sample<<5;

	if (sample>=0)
		maxLevel=max(sample,maxLevel);
	else
		maxLevel=max(-sample, maxLevel);

	//calculate the lowband filter:
	//shift the values (a circular buffer is probably not worth implementing)
	lowband_x0=lowband_x1;
	lowband_x1=lowband_x2;
	lowband_y0=lowband_y1;
	lowband_y1=lowband_y2;	

	//note that since the INVGAIN, B0 and B1 values are left shifted 10 bits,
	//the product must be right shifted 10 bits
	lowband_x2=((int32_t)sample*lowband_INVGAIN) >> 10;
	
	lowband_y2=(lowband_x2-lowband_x0) 
					+ ((lowband_B0*(int32_t)lowband_y0)>>10) + ((lowband_B1*(int32_t)lowband_y1)>>10);


	//y2 is the filtered result
	//since y2 is effectively left shifted 11 bits
	// (we should think of it as a floating point number between -1 and 1)
	// we have to right shift its square 11 bits
//	lowband_energy_sum+=((int32_t)lowband_y2*lowband_y2)>>11;

//give a little bit more resolution at the low end:
lowband_energy_sum+=((int32_t)lowband_y2*lowband_y2)>>7;


			// what about this instead of the square?
			//lowband_energy_sum+=(lowband_y2>=0)?lowband_y2:-lowband_y2;

	//next the midband filter
	midband_x0=midband_x1;
	midband_x1=midband_x2;
	midband_y0=midband_y1;
	midband_y1=midband_y2;	


	midband_x2=((int32_t)sample*midband_INVGAIN) >> 10;



	midband_y2=(midband_x2-midband_x0) 
					+ (((int32_t)midband_B0*midband_y0)>>10) + (((int32_t)midband_B1*midband_y1)>>10);


//	midband_energy_sum+=((int32_t)midband_y2*midband_y2)>>11;
midband_energy_sum+=((int32_t)midband_y2*midband_y2)>>7;
				//midband_energy_sum+=(midband_y2>=0)?midband_y2:-midband_y2;


	//finally the highband filter
	highband_x0=highband_x1;
	highband_x1=highband_x2;
	highband_y0=highband_y1;
	highband_y1=highband_y2;	

	highband_x2=((int32_t)sample*highband_INVGAIN) >> 10;
	highband_y2=(highband_x2-highband_x0) 
					+ ((highband_B0*(int32_t)highband_y0)>>10) + ((highband_B1*(int32_t)highband_y1)>>10);

	
//	highband_energy_sum+=((int32_t)highband_y2*highband_y2)>>11;
highband_energy_sum+=((int32_t)highband_y2*highband_y2)>>7;
				//highband_energy_sum+=(highband_y2>=0)?highband_y2:-highband_y2;

	//put a sample to the DAC for debugging
	int16_t sampleOut=lowband_y2;
	DAC_Channel_Write(&DACB, (uint16_t) (sampleOut + 2048), CH0);//+ADC_offset/2, CH0);


	//increase the sample capture counter by sending event 4
	EVSYS_ManualTrigger(0, 0x10);

debug2=((int32_t)midband_y2*midband_y2)>>7;

	//flag that the sample event occured
	fSampleCaptured=true;
}

uint8_t historyIndex=0;
//should these arrays be initialized to 0?
uint16_t lowband_energy_history[HISTORY_size];
uint16_t midband_energy_history[HISTORY_size];
uint16_t highband_energy_history[HISTORY_size];

//uint32_t lowband_energy_historySum=0;
//uint32_t midband_energy_historySum=0;
//uint32_t highband_energy_historySum=0;

//this interrupt is called after every BINSIZE samples are filtered and summed into their three channels
//here we decide how to 
ISR(CAPTURE_vect) {
	//this algorithm is taken from http://www.gamedev.net/reference/articles/article1952.asp

	//grab the sums lickety-quick so they don't change while we are processing
	//and divide them by BINSIZE to find their average
	lowband_energy_inst=lowband_energy_sum >> CAPTURE_binbits;

//###
	//drop the lowband energy by a fudge factor, because it never seems to be flat
	if (lowband_energy_inst>51)
		lowband_energy_inst-=51;
	else 
		lowband_energy_inst=0;

	lowband_energy_history[historyIndex]=lowband_energy_inst;

	midband_energy_inst=midband_energy_sum >> CAPTURE_binbits;
	midband_energy_history[historyIndex]=midband_energy_inst;

	highband_energy_inst=highband_energy_sum >> CAPTURE_binbits;
	highband_energy_history[historyIndex]=highband_energy_inst;

	//reset the sums
	lowband_energy_sum=0;
	midband_energy_sum=0;
	highband_energy_sum=0;


	//increment the history index
	historyIndex++;
	if (historyIndex>=HISTORY_size)
		historyIndex=0;

	debug1=maxLevel;
	maxLevel=0;
	
	//now find maximum energy from the last HISTORY_size bin captures 
	//(this should be about a 1 second history)
	// the strength of the output channel will scale to this maximum

	uint16_t lowband_energy_max=0, lowband_energy_min=65535;

	for (uint8_t i=0;i<HISTORY_size;i++) {
		lowband_energy_max=max(lowband_energy_history[i],lowband_energy_max);
		lowband_energy_min=min(lowband_energy_history[i],lowband_energy_min);
	}
	
	uint16_t midband_energy_max=0, midband_energy_min=65535;
	for (uint8_t i=0;i<HISTORY_size;i++) {
		midband_energy_max=max(midband_energy_history[i],midband_energy_max);
		midband_energy_min=min(midband_energy_history[i],midband_energy_min);
	}

	uint16_t highband_energy_max=0, highband_energy_min=65535;
	for (uint8_t i=0;i<HISTORY_size;i++) {
		highband_energy_max=max(highband_energy_history[i],highband_energy_max);
		highband_energy_min=min(highband_energy_history[i],highband_energy_min);
	}



	//for the time being, use the instantaneous energy, scaled to the maximum energy in the history
	// compare this to the instantaneous energy

	//other options are to use: 
	//	1) the rate of change of energy
	//	2) the difference of the energy from the 1 second background
	//	3) the simple instantaneous energy
	//  ...?

	//try fudging the max energy a little bit
	//these values are basically voodoo
	//it serves to ignore small variations in the energy though, basically requires a certain minimum intensity of sound to register a response
//###	lowband_energy_max+=32;
//###	midband_energy_max+=32;
//###	highband_energy_max+=32;
//###
lowband_energy_max=max(32,lowband_energy_max);
midband_energy_max=max(32,midband_energy_max);
highband_energy_max=max(32,highband_energy_max);



	uint16_t energy_min=min(lowband_energy_min, min(midband_energy_min, highband_energy_min));
	uint16_t energy_max=max(lowband_energy_max, max(midband_energy_max, highband_energy_max));

	//should we use the max of these maximums for scaling?

	//???
	//lowband_energy_max = (lowband_energy_max+energy+max)>>1;
	//etc...

	//should we have a minimum duty value and scale relative to that?

//!!! the following lines are wrong
	// they don't deal with minium energy in the right way
	uint8_t lowband_duty=0, midband_duty=0, highband_duty=0;
	if (lowband_energy_max-lowband_energy_min>1) {
		uint32_t lowband_scale=((uint32_t)lowband_energy_inst*256)/(lowband_energy_max-lowband_energy_min);
		lowband_duty=min(lowband_scale,255);
	}
	if (midband_energy_max-midband_energy_min>1) {
		uint32_t midband_scale=((uint32_t)midband_energy_inst*256)/(midband_energy_max-midband_energy_min);
		midband_duty=min(midband_scale,255);
debug2=midband_scale;
debug4=midband_scale;
	}
	if (highband_energy_max-highband_energy_min>1) {
		uint32_t highband_scale=((uint32_t)highband_energy_inst*256)/(highband_energy_max-highband_energy_min);
		highband_duty=min(highband_scale,255);
	}



//###
//if (lowband_duty<=192) lowband_duty=0;
//if (midband_duty<=192) midband_duty=0;
//if (highband_duty<=192) highband_duty=0;

//	lowband_duty=0;

	#ifndef FLASHER_ENABLE
		//scale the duty cycle down to the range 0..255
		setDutyCycles(lowband_duty, midband_duty, highband_duty);
	#endif


	fCaptureBinFilled=true;
}


