#ifndef MEDIAPLYR_H
#define MEDIAPLYR_H

#include "tbook.h"
#include "audio.h"			// interface definitions for audio.c

extern 	osEventFlagsId_t			mMediaEventId;		// event channel for signals to mediaThread

extern void			initMediaPlayer( void );			// init mediaPlayer & spawn thread to handle  playback & record requests
extern void			mediaPowerDown( void );
extern void 		playAudio( const char *fileName, MsgStats *stats );
extern void 		pauseResume( void );
extern void 		stopPlayback( void );
extern int			playPosition( void );
extern void 		adjPlayPosition( int sec );
extern void			setVolume( int vol );								// set initial volume
extern void 		adjVolume( int adj );
extern void 		adjVolume( int adj );
extern void 		adjSpeed( int adj );
extern int			recordPosition( void );
extern MediaState	getStatus( void );
extern void			recordAudio( const char *fileName, MsgStats *stats );
extern void			stopRecording( void );
extern void 		resetAudio( void ); 								// stop any playback/recording in progress
#endif
