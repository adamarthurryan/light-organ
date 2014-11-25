light-organ
===========

The light organ is an audio visualizer or special effects controller. The device is an electronic a 3-channel dimmer for incandescent lights. Each channel interprets the amplitude of one frequency band from the input audio.

This device was used in the interactive sound ard installation "infinite space kaleidoscope" by Adam Brown. See http://madanworb.com/blog/2011/02/01/infinite-space-kaleidoscope/

# Hardware

The dimmer is implemented with TRIAC switches, signaled through optoisolators. The dimmer controller and sound analyzer is implemented with an Atmel ATXMega128A1 8-bit microcontroller. 

The dimmer circuitry also provides a rectified AC-sync signal at 2.5V. The controller analyzes this signal to determine when the TRIAC should be switched on at each point in the AC waveform. (Recall that TRIACs can only be switched off at zero-crossing, so dimmer PWM has to be syncronized to the AC carrier wave.)

Circuits were designed in the free edition of Eagle. Schematics and burnable board layouts are included in the "circuits/light organ base" and "circuits/light organ controller" folders.

# Software

The controller software is implemented for the Atmel ATXMega128A1 - an 8-bit, 32MHz microcontroller. The responsibilities of the controller software are to:
- analyze an input waveform to find amplitudes in the three target frequency bands
- analyze the AC-sync signal to determine the mains frequency and phase
- signal the amplitude for each channel by switching the TRIACs

The software is developed and compiled with Atmel AVR Studio and depends on the XMega appnote library.
