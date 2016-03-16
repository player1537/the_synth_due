/*
 * CFile1.c
 *
 * Created: 2/12/2016 10:27:58 PM
 *  Author: Michael Haines
 */ 
#include <Arduino.h>
#include "synth.h"
#include "Envelope.h"
#include "Osc.h"
//#include "tables_due.h"

static struct{
	
	struct envelope_struct amplitudeEnvs[SYNTH_VOICE_COUNT];
	struct oscillator_struct oscillators[SYNTH_VOICE_COUNT];
	
	} synthesizer;


volatile uint32_t phase_accumulators[SYNTH_VOICE_COUNT] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,};      //-Wave phase accumulators
volatile uint16_t frequancy_tuning_word[SYNTH_VOICE_COUNT] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,};           //-Wave frequency tuning words 200, 200, 300, 400, 200, 200, 300, 400
volatile uint16_t amplitude[SYNTH_VOICE_COUNT] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,};           //-Wave amplitudes [0-255]
volatile uint16_t pitch[SYNTH_VOICE_COUNT] = {
  100, 500, 500, 500, 100, 500, 500, 500, 0, 0, 0, 0, 0, 0, 0, 0,};          //-Voice pitch

volatile const uint8_t *wavs[SYNTH_VOICE_COUNT];                                   //-Wave table selector [address of wave in memory]

volatile uint32_t max_length[SYNTH_VOICE_COUNT] = {
  28890112, 28890112, 28890112, 28890112, 28890112, 28890112, 28890112, 28890112,
  28890112, 28890112, 28890112, 28890112, 28890112, 28890112, 28890112, 28890112, 
  };//1040384, //15043584
volatile uint32_t loop_point[SYNTH_VOICE_COUNT] = {
	2006528, 2006528, 2006528, 2006528, 2006528, 2006528, 2006528, 2006528, 
	2006528, 2006528, 2006528, 2006528, 2006528, 2006528, 2006528, 2006528,
};

volatile unsigned char divider = 0;//-Sample rate decimator for envelope
volatile uint16_t time_hz = 0;
volatile unsigned char tik = 0; 
volatile unsigned char output_mode;

volatile uint16_t wave_amplitude[SYNTH_VOICE_COUNT] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
volatile int16_t Pitch_bend[SYNTH_VOICE_COUNT] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,};
  
 volatile int noteTrigger[SYNTH_VOICE_COUNT] = {
 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,};
 
volatile int noteDeath[SYNTH_VOICE_COUNT] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
volatile int current_stage = 20;

volatile uint16_t test_variable = 0;

//*********************************************************************************************
//  Audio driver interrupt
//*********************************************************************************************

void TC5_Handler()
	{
	TC_GetStatus(TC1, 2);
	
	int i = 0;
	//-------------------------------
	// Time division
	//-------------------------------
	divider++;
	if(!(divider&=0x0f))
		tik=1;
	//-------------------------------
	// Volume envelope generator
	//-------------------------------
	if(noteTrigger[divider]){
		set_envelopes();
		envelope_trigger(&synthesizer.amplitudeEnvs[divider], wave_amplitude[divider]);
		noteTrigger[divider] = 0;
		//current_stage = 1;
	}
	
	else if (noteDeath[divider] == 1){
		envelope_setStage(&synthesizer.amplitudeEnvs[divider],RELEASE);
		noteDeath[divider] = 0;
	}
	envelope_update(&synthesizer.amplitudeEnvs[divider]);
	amplitude[divider] = env_getOutput(&synthesizer.amplitudeEnvs[divider]);//&(wave_amplitude[divider]);	
  
  
	//-------------------------------
	//  Synthesizer/audio mixer
	//-------------------------------

	for (i=0; i < SYNTH_VOICE_COUNT; i++){
		phase_accumulators[i] += frequancy_tuning_word[i] + Pitch_bend[i];
	}
	
	for(i=0; i < SYNTH_VOICE_COUNT; i++){
		if (phase_accumulators[i] >= max_length[i]) phase_accumulators[i] -= loop_point[i];
	}
	int16_t output_sum = 0;
	int16_t wave_temp = 0;
	for (i=0; i<SYNTH_VOICE_COUNT; i++){
		wave_temp = 127 - *(wavs[i] + ((phase_accumulators[i]) >> 9));
		output_sum += ((wave_temp * amplitude[i]) >> 8);
	}
	REG_PIOD_ODSR = 127 + ((output_sum) >> 2);

	if (current_stage > 12)
	{
		current_stage /= 2;
	}
	else if (current_stage > 11)
	{
		current_stage *= 2;
	}
	else if (current_stage >10)
	{
		current_stage *= 2;
	}
	else if (current_stage >9)
	{
		current_stage *= 2;
	}
	else if (current_stage >8)
	{
		current_stage *= 2;
	}
	else if (current_stage >7)
	{
		current_stage *= 2;
	}
	else if (current_stage >6)
	{
		current_stage *= 2;
	}
	else if (current_stage >5)
	{
		current_stage *= 2;
	}
	else if (current_stage > 1)
	{
		current_stage *= 2;
	}
	frequancy_tuning_word[divider] = pitch[divider];
	time_hz++;
}
void set_envelopes(){
	
	int i = 0;
	for(i = 0; i < SYNTH_VOICE_COUNT; i++){
		envelope_setup(&synthesizer.amplitudeEnvs[i], 65535,1,65535,27);
	}
	
}


void set_oscillators(){
	int i = 0;
	for(i = 0; i < SYNTH_VOICE_COUNT - 1; i++){
		//setVoices(&synthesizer.oscillators[i], &string_C6, 0, 127);
	}
	
	//setVoices(&synthesizer.oscillators[7], &snare,0,127);

}

void set_lfo(){
	
}