/* -LICENSE-START-
 ** Copyright (c) 2010 Blackmagic Design
 **  
 ** Permission is hereby granted, free of charge, to any person or organization 
 ** obtaining a copy of the software and accompanying documentation (the 
 ** "Software") to use, reproduce, display, distribute, sub-license, execute, 
 ** and transmit the Software, and to prepare derivative works of the Software, 
 ** and to permit third-parties to whom the Software is furnished to do so, in 
 ** accordance with:
 ** 
 ** (1) if the Software is obtained from Blackmagic Design, the End User License 
 ** Agreement for the Software Development Kit (“EULA”) available at 
 ** https://www.blackmagicdesign.com/EULA/DeckLinkSDK; or
 ** 
 ** (2) if the Software is obtained from any third party, such licensing terms 
 ** as notified by that third party,
 ** 
 ** and all subject to the following:
 ** 
 ** (3) the copyright notices in the Software and this entire statement, 
 ** including the above license grant, this restriction and the following 
 ** disclaimer, must be included in all copies of the Software, in whole or in 
 ** part, and all derivative works of the Software, unless such copies or 
 ** derivative works are solely in the form of machine-executable object code 
 ** generated by a source language processor.
 ** 
 ** (4) THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
 ** OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 ** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT 
 ** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE 
 ** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, 
 ** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 ** DEALINGS IN THE SOFTWARE.
 ** 
 ** A copy of the Software is available free of charge at 
 ** https://www.blackmagicdesign.com/desktopvideo_sdk under the EULA.
 ** 
 ** -LICENSE-END-
 */
#ifndef __export_h__

#include "DeckLinkAPI.h"
#include <pthread.h>


/*
 * ETTHelper
 * This class creates an array of coloured frames which are exported to tape in a round robin fashion.
 * The video mode, in- and out-timecodes are specified as macros at the top of
 * the implementation file.
 */
class ETTHelper : public IDeckLinkDeckControlStatusCallback , public IDeckLinkVideoOutputCallback
{
private:
	int32_t						m_refCount;
	IDeckLink *					m_deckLink;
	IDeckLinkDeckControl *		m_deckControl;
	IDeckLinkOutput *			m_deckLinkOutput;
	
	// The mutex and condition variable are used to wait for
	// - a deck to be connected
	// - the export to complete
	pthread_mutex_t				m_mutex ;
	pthread_cond_t				m_condition;
	bool						m_waitingForDeckConnected;
	bool						m_waitingForExportEnd;
	bool						m_exportStarted;

	// array of coloured frames
	IDeckLinkMutableVideoFrame** m_videoFrames;
	uint32_t					m_nextFrameIndex;
	uint32_t					m_totalFrameScheduled;
	
	// video mode
	long						m_width;
	long						m_height;
	BMDTimeScale				m_timeScale;
	BMDTimeValue				m_frameDuration;
	
	virtual			~ETTHelper();
	
	bool			fillFrame(int index);
	void			releaseFrames();
	bool			createFrames();
	
	bool			setupDeckLinkOutput();
	bool			setupDeckControl();
	
	bool			scheduleNextFrame(bool preroll);
	
	void			cleanupDeckControl();
	void			cleanupDeckLinkOutput();
	
	
public:
	ETTHelper(IDeckLink *deckLink);

	// init() must be called after the constructor.
	// if init() fails, call the destructor
	bool			init();
	
	// start the export-to-tape operation. returns when the operation has completed
	void			doExport();
	
	// IDeckLinkDeckControlStatusCallback
	virtual HRESULT TimecodeUpdate (/* in */ BMDTimecodeBCD currentTimecode) {return S_OK;};
    virtual HRESULT VTRControlStateChanged (/* in */ BMDDeckControlVTRControlState newState, /* in */ BMDDeckControlError error) {return S_OK;};
    virtual HRESULT DeckControlEventReceived (/* in */ BMDDeckControlEvent event, /* in */ BMDDeckControlError error);
    virtual HRESULT DeckControlStatusChanged (/* in */ BMDDeckControlStatusFlags flags, /* in */ uint32_t mask);
	
	// IDeckLinkVideoOutputCallback
	virtual HRESULT	ScheduledFrameCompleted (IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result);
	virtual HRESULT	ScheduledPlaybackHasStopped () {return S_OK;};
	
	// IUnknown
	HRESULT			QueryInterface (REFIID iid, LPVOID *ppv);
	ULONG			AddRef ();
	ULONG			Release ();
};

#endif		// __export_h__

