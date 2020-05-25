// TBookV2  mediaplayer.c		play/record audio
//  Apr2018

#include "mediaPlyr.h"

const int 									CODEC_DATA_TX_DN   =	0x01; 			// signal sent by SAI callback when an buffer completes
const int 									CODEC_PLAYBACK_DN	 =  0x02;				// signal from SAI on playback done
const int 									CODEC_DATA_RX_DN   =	0x04; 			// signal sent by SAI callback when a buffer has been filled
const int 									CODEC_RECORD_DN    =	0x08; 			// signal sent by SAI callback when recording stops
const int 									MEDIA_PLAY_EVENT	 =	0x10;
const int 									MEDIA_RECORD_START =	0x20;

static 	osThreadAttr_t 				thread_attr;
static 	osThreadId_t					mMediaThreadId;

osEventFlagsId_t							mMediaEventId;			// for signals to mediaThread

static int audioVolume				= DEFAULT_VOLUME;

// communication variables shared with mediaThread
static volatile char					mPlaybackFilePath[ MAX_PATH ];
static volatile MsgStats *		mPlaybackStats;
static volatile char 					mRecordFilePath[ MAX_PATH ];
static volatile MsgStats *		mRecordStats;
static volatile MsgStats *    sysStats;						// subst buffer for system files 

static void 	mediaThread( void *arg );						// forward 
	
void					initMediaPlayer( void ){						// init mediaPlayer & spawn thread to handle  playback & record requests
	mMediaEventId = osEventFlagsNew(NULL);					// osEvent channel for communication with mediaThread
	if ( mMediaEventId == NULL)
		tbErr( "mMediaEventId alloc failed" );	
	
	sysStats = tbAlloc( sizeof( MsgStats ), "sysStats" );

	mPlaybackFilePath[0] = 0;			
	mRecordFilePath[0] = 0;			
	
	thread_attr.name = "Media";
	thread_attr.stack_size = MEDIA_STACK_SIZE; 
	mMediaThreadId = osThreadNew( mediaThread, NULL, &thread_attr );
	if ( mMediaThreadId == NULL )
		tbErr( "mediaThread spawn failed" );	
	
	//registerPowerEventHandler( handlePowerEvent );
	audInitialize( );
}
void					mediaPowerDown( void ){
	audStopAudio();
}

// external methods for  controlManager executeActions
int 					playAudio( const char * fileName, MsgStats *stats ){ // start playback from fileName
	while ( audGetState()!=Ready ){ 
		audStopAudio();
		osDelay( 5 );
	}
	strcpy( (char *)mPlaybackFilePath, fileName );
	mPlaybackStats = stats==NULL? sysStats : stats;
	osEventFlagsSet( mMediaEventId, MEDIA_PLAY_EVENT );
	return TB_SUCCESS;
}
void 					pauseResume( void ){								// pause (or resume) playback or recording
	audPauseResumeAudio( );
}
void 					stop( void ){												// stop playback
	audStopAudio( );
}

int						playPosition( void ){								// => current playback position in sec
	return audPlayPct();
}
void 					adjPlayPosition( int sec ){					// skip playback forward/back 'sec' seconds
	dbgEvt( TB_audAdjPos, sec, 0, 0,0);
	audAdjPlayPos( sec );
}
void 					adjVolume( int adj ){								// adjust playback volume by 'adj'
	audioVolume += adj;
	if ( audioVolume > MAX_VOLUME ) audioVolume = MAX_VOLUME;
	if ( audioVolume < MIN_VOLUME ) audioVolume = MIN_VOLUME;
	dbgEvt( TB_audAdjVol, adj, audioVolume, 0,0);
	ak_SetVolume( audioVolume );
	dbgLog( "SetVol:%d  [%d]", audioVolume, audPlayPct() );
}
void 					adjSpeed( int adj ){								// adjust playback speed by 'adj'
	audAdjPlaySpeed( adj );
	dbgEvt( TB_audAdjSpd, adj, 0, 0,0);
}
MediaState		getStatus( void ){									// => Ready / Playing / Recording
	return audGetState();
}
int						recordAudio( const char * fileName, MsgStats *stats ){	// start recording into fileName
	while ( audGetState()!=Ready ){ 
		audStopAudio();
		osDelay( 5 );
	}
	strcpy( (char *)mRecordFilePath, fileName );
	mRecordStats = stats==NULL? sysStats : stats;
	osEventFlagsSet( mMediaEventId, MEDIA_RECORD_START );
	return TB_SUCCESS;
}
void					stopRecording( void ){  						// stop recording
	audStopRecording( ); 
}

/* **************  mediaThread -- start audio play & record operations, handle completion signal
// 		MEDIA_PLAY_EVENT 	=> play mPlaybackFilename
//		MEDIA_RECORD_START 	=> record into mRecordFilePath
//		CODEC_PLAY_DONE 	=> send AudioDone CSM event
***************/
static void 	mediaThread( void *arg ){						// communicates with audio codec for playback & recording		
	const int MEDIA_EVENTS = MEDIA_PLAY_EVENT | MEDIA_RECORD_START | CODEC_DATA_TX_DN | CODEC_DATA_RX_DN | CODEC_PLAYBACK_DN | CODEC_RECORD_DN;
	while (true){		
		uint32_t flags = osEventFlagsWait( mMediaEventId, MEDIA_EVENTS,  osFlagsWaitAny, osWaitForever );
		
		dbgEvt( TB_mediaEvt, flags, 0,0,0);
		if ( (flags & CODEC_DATA_TX_DN) != 0 ){								// buffer transmission complete from SAI_event
			audLoadBuffs();		// preload any empty audio buffers
			
		} else if ( (flags & CODEC_DATA_RX_DN) != 0 ){				// buffer reception complete from SAI_event
			audSaveBuffs();
			
		} else if ( (flags & CODEC_PLAYBACK_DN) != 0 ){				// playback complete
			audPlaybackComplete();
			
		} else if ( (flags & CODEC_RECORD_DN) != 0 ){				// recording complete
			audRecordComplete();

		} else if ( (flags & MEDIA_PLAY_EVENT) != 0 ){				// request to start playback
			if ( mPlaybackFilePath[0] == 0 ) continue;
			audPlayAudio( (const char *)mPlaybackFilePath, (MsgStats *) mPlaybackStats );
			
		} else if ( (flags & MEDIA_RECORD_START) != 0 ){			// request to start recording
			FILE* outFP = fopen( (const char *)mRecordFilePath, "wb" );
			if ( outFP != NULL ){
				audStartRecording( outFP, (MsgStats *) mRecordStats );
			} else {
				printf ("Cannot open record file to write\n\r");
			}
		}
	}
}
//end  mediaplayer.c

