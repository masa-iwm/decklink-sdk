/* -LICENSE-START-
 ** Copyright (c) 2011 Blackmagic Design
 **
 ** Permission is hereby granted, free of charge, to any person or organization
 ** obtaining a copy of the software and accompanying documentation covered by
 ** this license (the "Software") to use, reproduce, display, distribute,
 ** execute, and transmit the Software, and to prepare derivative works of the
 ** Software, and to permit third-parties to whom the Software is furnished to
 ** do so, all subject to the following:
 ** 
 ** The copyright notices in the Software and this entire statement, including
 ** the above license grant, this restriction and the following disclaimer,
 ** must be included in all copies of the Software, in whole or in part, and
 ** all derivative works of the Software, unless such copies or derivative
 ** works are solely in the form of machine-executable object code generated by
 ** a source language processor.
 ** 
 ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 ** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 ** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 ** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 ** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 ** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 ** DEALINGS IN THE SOFTWARE.
 ** -LICENSE-END-
 */

#ifndef __playback_helper_h__
#define __playback_helper_h__


#include "DeckLinkAPI.h"
#include <pthread.h>

/*
 * Playback helper
 * This class creates an array of coloured frames and schedules them for output.
 */
class PlaybackHelper : public IDeckLinkVideoOutputCallback
{
private:
	int32_t						m_refCount;
	IDeckLink *					m_deckLink;
	IDeckLinkOutput *			m_deckLinkOutput;
	IDeckLinkConfiguration *	m_configuration;
	
	pthread_t					m_watchdogThread;

	bool						m_playbackStarted;
	
	// array of coloured frames
	IDeckLinkMutableVideoFrame** m_videoFrames;
	uint32_t					m_nextFrameIndex;
	uint32_t					m_totalFrameScheduled;
	
	// video mode
	long						m_width;
	long						m_height;
	BMDTimeScale				m_timeScale;
	BMDTimeValue				m_frameDuration;
	
	bool			fillFrame(int index);
	void			releaseFrames();
	bool			createFrames();
	
	bool			setupDeckLinkOutput();	
	bool			scheduleNextFrame(bool preroll);	
	void			cleanupDeckLinkOutput();
	
	void			pingWatchdog();
	
	
public:
	PlaybackHelper(IDeckLink *deckLink);
	virtual			~PlaybackHelper();
	
	// init() must be called after the constructor.
	// if init() fails, call the destructor
	bool			init();
	
	bool			startPlayback();
	void			stopPlayback();
	
	// Watchdog thread function
	static void*	pingWatchdogFunc(void *me);
		
	// IDeckLinkVideoOutputCallback
	virtual HRESULT	ScheduledFrameCompleted (IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result);
	virtual HRESULT	ScheduledPlaybackHasStopped () {return S_OK;};
	
	// IUnknown
	HRESULT			QueryInterface (REFIID iid, LPVOID *ppv);
	ULONG			AddRef ();
	ULONG			Release ();
};


#endif	//__playback_helper_h__