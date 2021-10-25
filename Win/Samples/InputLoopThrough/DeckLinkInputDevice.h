/* -LICENSE-START-
** Copyright (c) 2019 Blackmagic Design
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

#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include "LoopThroughAudioPacket.h"
#include "LoopThroughVideoFrame.h"
#include "DeckLinkAPI.h"
#include "com_ptr.h"


class DeckLinkInputDevice : public IDeckLinkInputCallback
{
public:
	using VideoFormatChangedCallback		= std::function<void(BMDDisplayMode, bool, BMDPixelFormat)>;
	using VideoInputArrivedCallback			= std::function<void(std::shared_ptr<LoopThroughVideoFrame>)>;
	using AudioInputArrivedCallback			= std::function<void(std::shared_ptr<LoopThroughAudioPacket>)>;
	using VideoInputFrameDroppedCallback	= std::function<void(BMDTimeValue, BMDTimeValue, BMDTimeScale)>;

	DeckLinkInputDevice(com_ptr<IDeckLink>& deckLink);
	virtual ~DeckLinkInputDevice() = default;

	// IUnknown interface
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override;
	ULONG	STDMETHODCALLTYPE AddRef() override;
	ULONG	STDMETHODCALLTYPE Release() override;

	// IDeckLinkInputCallback interface
	HRESULT	STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents notificationEvents, IDeckLinkDisplayMode *newDisplayMode, BMDDetectedVideoInputFormatFlags detectedSignalFlags) override;
	HRESULT	STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket) override;

	// Other methods
	bool	startCapture(BMDDisplayMode displayMode, bool enable3D, BMDPixelFormat pixelFormat, BMDAudioSampleType audioSampleType, uint32_t audioChannelCount);
	void	stopCapture(void);
	void	setReadyForCapture(void);

	void	onVideoFormatChange(const VideoFormatChangedCallback& callback) { m_videoFormatChangedCallback = callback; }
	void	onVideoInputArrived(const VideoInputArrivedCallback& callback) { m_videoInputArrivedCallback = callback; }
	void	onAudioInputArrived(const AudioInputArrivedCallback& callback) { m_audioInputArrivedCallback = callback; }
	void	onVideoInputFrameDropped(const VideoInputFrameDroppedCallback& callback) { m_videoInputFrameDroppedCallback = callback; }

private:
	std::atomic<ULONG>				m_refCount;
	//
	com_ptr<IDeckLink>				m_deckLink;
	com_ptr<IDeckLinkInput>			m_deckLinkInput;
	BMDTimeValue					m_frameDuration;
	BMDTimeValue					m_lastStreamTime;
	BMDTimeScale					m_frameTimescale;
	bool							m_seenValidSignal;
	bool							m_readyForCapture;
	//
	VideoFormatChangedCallback		m_videoFormatChangedCallback;
	VideoInputArrivedCallback		m_videoInputArrivedCallback;
	AudioInputArrivedCallback		m_audioInputArrivedCallback;
	VideoInputFrameDroppedCallback	m_videoInputFrameDroppedCallback;
};
