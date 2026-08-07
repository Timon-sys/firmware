#pragma once

void playBeep();
void playLongBeep();
void playPingSentBeep(); // medium: our ping went out
void playAckBeep();      // high: an ACK came back
void playAckFailBeep();  // low: no ACK, max retransmissions reached
void playStartMelody();
void playShutdownMelody();
void playGPSEnableBeep();
void playGPSDisableBeep();
void playComboTune();
void play4ClickDown();
void play4ClickUp();
void playBoop();
void playChirp();
void playClick();
bool playNextLeadUpNote();  // Play the next note in the lead-up sequence
void resetLeadUpSequence(); // Reset the lead-up sequence to start from beginning