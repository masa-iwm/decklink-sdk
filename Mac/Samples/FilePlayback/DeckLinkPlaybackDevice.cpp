/* -LICENSE-START-
 ** Copyright (c) 2018 Blackmagic Design
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

#include <chrono>
#include <vector>
#include <set>
#include "DeckLinkPlaybackDevice.h"
#include "DeckLinkCoreMediaVideoFrame.h"

static const BMDPixelFormat kDevicePixelFormat	= bmdFormat10BitYUV;
static const BMDPixelFormat kDecoderPixelFormat	= bmdFormat8BitBGRA;

static const uint32_t	kBufferedAudioLevel	= (bmdAudioSampleRate48kHz / 2); // 0.5 seconds

static bool IsPlaying(com_ptr<IDeckLinkOutput> deckLinkOutput)
{
	bool isPlaying = false;
	deckLinkOutput->IsScheduledPlaybackRunning(&isPlaying);
	return isPlaying;
}

// DeckLinkPlaybackDevice
DeckLinkPlaybackDevice::DeckLinkPlaybackDevice(com_ptr<IDeckLink>& deckLink) :
m_refCount(1),
m_init(false),
m_statusListener(nullptr),
m_errorListener(nullptr),
m_deviceState(kDeviceIOIdle),
m_deckLink(deckLink),
m_deckLinkOutput(nullptr),
m_deckLinkVideoConversion(nullptr),
m_frameRate(0.0),
m_frameDuration(0),
m_videoOutputDisplayMode(bmdModeUnknown),
m_audioStreamTime(0),
m_streamTime(0),
m_streamTimeOffset(0),
m_streamDuration(0),
m_timeScale(0),
m_mediaReader(nullptr),
m_playbackStopping(false),
m_playbackStopped(true),
m_lastScheduledFrame(0),
m_isAvailable(true),
m_convertToDevicePixelFormat(true)
{
	com_ptr<IDeckLinkProfileAttributes>	attributes(IID_IDeckLinkProfileAttributes, m_deckLink);
	if (!m_deckLink || !attributes)
		return;

	attributes->GetString(BMDDeckLinkDisplayName, &m_displayName);
}

DeckLinkPlaybackDevice::~DeckLinkPlaybackDevice()
{
}

HRESULT DeckLinkPlaybackDevice::QueryInterface(REFIID iid, LPVOID *ppv)
{
	CFUUIDBytes	iunknown;
	HRESULT		result = E_NOINTERFACE;

	*ppv = NULL;

	iunknown = CFUUIDGetUUIDBytes(IUnknownUUID);
	if (memcmp(&iid, &iunknown, sizeof(REFIID)) == 0)
	{
		*ppv = this;
		AddRef();
		result = S_OK;
	}
	else if (memcmp(&iid, &IID_IDeckLinkVideoOutputCallback, sizeof(REFIID)) == 0)
	{
		*ppv = (IDeckLinkVideoOutputCallback*)this;
		AddRef();
		result = S_OK;
	}
	else if (memcmp(&iid, &IID_IDeckLinkAudioOutputCallback, sizeof(REFIID)) == 0)
	{
		*ppv = (IDeckLinkAudioOutputCallback*)this;
		AddRef();
		result = S_OK;
	}
	else if (memcmp(&iid, &IID_IDeckLinkProfileCallback, sizeof(REFIID)) == 0)
	{
		*ppv = (IDeckLinkProfileCallback*)this;
		AddRef();
		result = S_OK;
	}

	return result;
}

ULONG DeckLinkPlaybackDevice::AddRef()
{
	return ++m_refCount;
}

ULONG DeckLinkPlaybackDevice::Release()
{
	ULONG newRefValue = --m_refCount;
	if (newRefValue == 0)
	{
		delete this;
		return 0;
	}

	return newRefValue;
}

HRESULT DeckLinkPlaybackDevice::ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result)
{
	BMDTimeValue frameTime	= 0;

	if (completedFrame)
	{
		if (m_deckLinkOutput->GetScheduledStreamTime(m_timeScale, &frameTime, nullptr) != S_OK)
			goto bail;

		if (result == bmdOutputFrameDisplayedLate)
		{
			if (!m_playbackStopping)
			{
				m_playbackStopping = true;
				notifyError(kFrameDisplayedLate);
				dispatch_async(dispatch_get_main_queue(), ^{ stop(0, m_timeScale); });
			}
			goto bail;
		}
	
		if (result != bmdOutputFrameFlushed)
			m_streamTime = frameTime + m_streamTimeOffset;

		notifyStatus(kPlaybackStreamTimeUpdated);
	}

	if (m_playbackStopping)
	{
		if (m_streamTime >= m_lastScheduledFrame - m_frameDuration)
			dispatch_async(dispatch_get_main_queue(), ^{ stop(0, m_timeScale); });

		goto bail;
	}

	if (!scheduleVideo())
		m_playbackStopping = true;

bail:
	return S_OK;
}

HRESULT DeckLinkPlaybackDevice::ScheduledPlaybackHasStopped()
{
	std::lock_guard<std::mutex> guard(m_mutex);
	m_playbackStopped = true;
	m_condition.notify_all();

	return S_OK;
}

HRESULT DeckLinkPlaybackDevice::RenderAudioSamples(bool preroll)
{
	using namespace std::placeholders;

	std::unique_ptr<PCMAudioBuffer> deckLinkAudioBuffer;

	uint32_t buffered = 0;
	if (m_deckLinkOutput->GetBufferedAudioSampleFrameCount(&buffered) != S_OK)
		return E_FAIL;
	
	if (buffered > kBufferedAudioLevel)
	{
		if (m_prerollingAudio)
		{
			m_deckLinkOutput->EndAudioPreroll();
			m_prerollingAudio = false;
		}
		return S_OK;
	}

	if (!m_mediaReader || !m_mediaReader->readAudio(deckLinkAudioBuffer))
		return E_FAIL;

	deckLinkAudioBuffer->read(std::bind(&DeckLinkPlaybackDevice::scheduleAudio, this, _1, _2));
	
	return S_OK;
}


HRESULT DeckLinkPlaybackDevice::ProfileChanging(IDeckLinkProfile *profileToBeActivated, bool streamsWillBeForcedToStop)
{
	if (!profileToBeActivated)
		return E_FAIL;
	
	if (streamsWillBeForcedToStop)
		disable();
	
	return S_OK;
}

HRESULT DeckLinkPlaybackDevice::ProfileActivated(IDeckLinkProfile *activatedProfile)
{
	update();
	return S_OK;
}

bool DeckLinkPlaybackDevice::init(const std::shared_ptr<DeckLinkMediaReader>& mediaReader)
{
	com_ptr<IDeckLinkDisplayModeIterator>	displayModeIterator;
	com_ptr<IDeckLinkOutput>				deckLinkOutput(IID_IDeckLinkOutput, m_deckLink);

	if (!deckLinkOutput)
		return false;
	
	m_deckLinkOutput	= std::move(deckLinkOutput);
	m_mediaReader		= mediaReader;
	
	com_ptr<IDeckLinkProfileManager> profileManager(IID_IDeckLinkProfileManager, m_deckLink);
	if (profileManager)
		profileManager->SetCallback(this);
	
	m_deckLinkVideoConversion = CreateVideoConversionInstance();
	if (!m_deckLinkVideoConversion)
		return false;
	
	update();
	
	m_init = true;

	return m_init;
}

bool DeckLinkPlaybackDevice::update()
{
	com_ptr<IDeckLinkProfileAttributes>	deckLinkAttributes(IID_IDeckLinkProfileAttributes, m_deckLink);
	
	if (!deckLinkAttributes)
		return false;
	
	int64_t intVal = 0;
	if (deckLinkAttributes->GetInt(BMDDeckLinkDuplex, &intVal) != S_OK)
		return E_FAIL;
	
	m_isAvailable = (intVal != bmdDuplexInactive);
	
	notifyStatus(kDeviceProfileChanged);
	return true;
}

CFStringRef DeckLinkPlaybackDevice::displayName() const
{
	return m_displayName;
}

intptr_t DeckLinkPlaybackDevice::getDeviceID() const
{
	return (intptr_t)m_deckLink.get();
}

DeviceIOState DeckLinkPlaybackDevice::getDeviceIOState() const
{
	return m_deviceState;
}

void DeckLinkPlaybackDevice::setErrorListener(const DeviceErrorOccurredFunc& func)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_errorListener = func;
}

void DeckLinkPlaybackDevice::setStatusListener(const DeviceStatusUpdateFunc& func)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_statusListener = func;
}

void DeckLinkPlaybackDevice::notifyStatus(DeviceStatus status)
{
	if (m_statusListener)
		m_statusListener(com_ptr<DeckLinkPlaybackDevice>(this), status);
}

void DeckLinkPlaybackDevice::notifyError(DeviceError error)
{
	if (m_errorListener)
		m_errorListener(com_ptr<DeckLinkPlaybackDevice>(this), error);
}

void DeckLinkPlaybackDevice::enableOutput(com_ptr<IDeckLinkScreenPreviewCallback> previewCallback, BMDDisplayMode displayMode)
{
	DeviceError		error				= kNoError;
	bool			displayModeSupport	= false;
	
	if (m_deviceState == kDeviceIOEnabled && m_videoOutputDisplayMode == displayMode)
	{
		setState(kDeviceIOEnabled);
		return;
	}

	com_ptr<IDeckLinkDisplayMode> deckLinkDisplayMode;
	if (m_deckLinkOutput->GetDisplayMode(displayMode, deckLinkDisplayMode.releaseAndGetAddressOf()) != S_OK || !deckLinkDisplayMode)
		return;
	
	if (!m_mediaReader)
	{
		error = kMediaFileReadFailed;
		goto bail;
	}

	if (deckLinkDisplayMode->GetFrameRate(&m_frameDuration, &m_timeScale) != S_OK)
	{
		error = kVideoDisplayModeNotSupported;
		goto bail;
	}

	m_convertToDevicePixelFormat = false;
	// Check if the device supports this video mode in the decoder pixel format, 8-bit BGRA
	if ((m_deckLinkOutput->DoesSupportVideoMode(bmdVideoConnectionUnspecified, displayMode, kDecoderPixelFormat, bmdSupportedVideoModeDefault, nullptr, &displayModeSupport) != S_OK)
		|| !displayModeSupport)
	{
		// If the decoder pixel format is not supported, check the device pixel format, 10-bit YUV
		if ((m_deckLinkOutput->DoesSupportVideoMode(bmdVideoConnectionUnspecified, displayMode, kDevicePixelFormat, bmdSupportedVideoModeDefault, nullptr, &displayModeSupport) != S_OK)
			|| !displayModeSupport)
		{
			error = kVideoDisplayModeNotSupported;
			goto bail;
		}
		// The device pixel format is supported, but pixel conversion is required
		m_convertToDevicePixelFormat = true;
	}
	
	m_deckLinkOutput->DisableVideoOutput();

	if (m_deckLinkOutput->EnableVideoOutput(displayMode, bmdVideoOutputFlagDefault) != S_OK)
	{
		error = kEnableVideoOutputFailed;
		goto bail;
	}

	m_videoOutputDisplayMode = displayMode;

	m_deckLinkOutput->DisableAudioOutput();
	
	if (m_deckLinkOutput->EnableAudioOutput(bmdAudioSampleRate48kHz, m_mediaReader->audioBitsPerSample(), m_mediaReader->audioChannelCount(), bmdAudioOutputStreamTimestamped) != S_OK)
	{
		error = kEnableAudioOutputFailed;
		goto bail;
	}

	m_deckLinkOutput->SetScreenPreviewCallback(previewCallback.get());

	setState(kDeviceIOEnabled);
	
bail:
	if (error)
	{
		disableOutput();
		setState(kDeviceIOIdle);
		notifyError(error);
	}
}

void DeckLinkPlaybackDevice::disableOutput()
{
	m_deckLinkOutput->DisableVideoOutput();
	m_deckLinkOutput->SetScheduledFrameCompletionCallback(nullptr);

	m_deckLinkOutput->DisableAudioOutput();
	m_deckLinkOutput->SetAudioCallback(nullptr);

}

void DeckLinkPlaybackDevice::preview(com_ptr<IDeckLinkScreenPreviewCallback> previewCallback, const std::string& filePath, BMDTimeValue streamTime, BMDTimeScale streamTimeScale)
{
	com_ptr<IDeckLinkVideoFrame>	previewFrame;
	com_ptr<IDeckLinkVideoFrame>	convertedFrame;
	std::set<BMDDisplayMode>		validDisplayModes;

	DeviceIOState	deviceState				= kFileIOError;
	DeviceError		deviceError				= kMediaFileReadFailed;
	BMDTimeValue	streamDuration			= 0;
	BMDDisplayMode	displayMode				= bmdModeUnknown;

	if (!m_mediaReader)
		return;

	m_frameRate = 0.0;

	// Seek the required frame time
	if (!m_mediaReader->init(filePath, streamTime, streamTimeScale))
		goto bail;

	// Get video frame and its natural time scale
	if (!m_mediaReader->previewFrame(previewFrame, &m_frameRate))
		goto bail;

	// Find a display mode that matches the video frame
	displayMode = getOutputDisplayMode(m_frameRate, previewFrame->GetWidth(), previewFrame->GetHeight());
	if (displayMode == bmdModeUnknown)
		goto bail;

	enableOutput(previewCallback, displayMode);
	if (m_deviceState != kDeviceIOEnabled)
		return;

	// Get media length in terms of the video output time scale
	if (!m_mediaReader->duration(streamDuration, m_timeScale))
		goto bail;

	// Update stream information
	m_streamTime		= streamTime;
	m_streamTimeOffset	= streamTime;

	// For simplicity, adjust to start of last frame
	m_streamDuration	= streamDuration - m_frameDuration;

	// Convert the frame if required
	convertVideoFrame(previewFrame, convertedFrame);
	
	// Send the frame out
	if (m_deckLinkOutput->DisplayVideoFrameSync(convertedFrame.get()) != S_OK)
	{
		deviceState = kDeviceIOError;
		deviceError = kFrameOutputFailed;
		goto bail;
	}

	notifyStatus(kPlaybackStreamTimeUpdated);

	return;

bail:
	setState(deviceState);

	if (m_errorListener)
			m_errorListener(com_ptr<DeckLinkPlaybackDevice>(this), deviceError);
}

void DeckLinkPlaybackDevice::play(BMDTimeValue streamTime, BMDTimeScale timeScale)
{
	// Preroll 1/2 second of frames
	auto framesToPreroll = (uint32_t)round(m_frameRate / 2.0);

	if (m_deviceState != kDeviceIOEnabled || !m_mediaReader)
		return;

	// For convenience, if the play head is at the end of the clip, play from the start
	if (m_streamTime >= m_streamDuration && timeScale == m_timeScale)
		streamTime = 0;

	m_streamTimeOffset = streamTime;
	m_mediaReader->reset(streamTime, timeScale);

	m_streamTime			= streamTime;
	m_audioStreamTime		= 0;
	m_lastScheduledFrame	= 0;

	m_deckLinkOutput->SetScheduledFrameCompletionCallback(this);
	m_deckLinkOutput->SetAudioCallback(this);

	m_playbackStopping = false;

	m_deckLinkOutput->BeginAudioPreroll();
	m_prerollingAudio = true;
	for (auto i = 0; i < framesToPreroll; ++i)
	{
		if (!scheduleVideo())
			break;
	}
	
	// If the audio buffer level has not been reached, end audio preroll here
	if (m_prerollingAudio)
	{
		m_deckLinkOutput->EndAudioPreroll();
		m_prerollingAudio = false;
	}

	if (m_deckLinkOutput->StartScheduledPlayback(0, m_timeScale, 1.0) != S_OK)
	{
		setState(kDeviceIOError);
		return;
	}

	setState(kFileIORunning);
	m_playbackStopped = false;
}

void DeckLinkPlaybackDevice::scrub(BMDTimeValue streamTime, BMDTimeScale timeScale)
{
	com_ptr<IDeckLinkVideoFrame>	previewFrame;

	if (!m_mediaReader || !m_mediaReader->reset(streamTime, timeScale))
		return;

	stopScheduledPlayback(0, nullptr, 0);

	if (!m_mediaReader->previewFrame(previewFrame, nullptr))
	{
		setState(kFileIOError);
		return;
	}

	if (m_deckLinkOutput->DisplayVideoFrameSync(previewFrame.get()) != S_OK)
	{
		setState(kDeviceIOError);
		return;
	}

	m_streamTimeOffset = streamTime;
	m_streamTime = m_streamTimeOffset;

	notifyStatus(kPlaybackStreamTimeUpdated);
}

void DeckLinkPlaybackDevice::stopScheduledPlayback(BMDTimeValue stopPlaybackAtTime, BMDTimeValue* actualStopTime, BMDTimeScale timeScale)
{
	using std::chrono::milliseconds;
	
	BMDTimeValue stopTime = 0;

	if (!IsPlaying(m_deckLinkOutput) || timeScale == 0)
		return;

	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_playbackStopping = true;
		m_deckLinkOutput->StopScheduledPlayback(stopPlaybackAtTime, &stopTime, timeScale);
		m_deckLinkOutput->FlushBufferedAudioSamples();

		// Wait for playback to stop
		do
		{
			m_condition.wait(lock);
		} while (!m_playbackStopped);
	}

	// Update the stream time to the the (start of frame) time playback actually stopped
	m_streamTime = ((stopTime * m_timeScale) / timeScale) + m_streamTimeOffset - m_frameDuration;

	notifyStatus(kPlaybackStreamTimeUpdated);

	if (actualStopTime)
		*actualStopTime = stopTime;
}

void DeckLinkPlaybackDevice::stop(BMDTimeValue streamTime, BMDTimeScale timeScale)
{
	if (m_deviceState != kFileIORunning || !m_deckLinkOutput || !IsPlaying(m_deckLinkOutput))
		return;

	BMDTimeValue actualStopTime = 0;
	stopScheduledPlayback(streamTime, &actualStopTime, timeScale);

	if (!m_mediaReader)
		m_mediaReader->complete();

	setState(kDeviceIOEnabled);
}

void DeckLinkPlaybackDevice::disable()
{
	if (m_deviceState == kDeviceIOIdle)
		return;

	stopScheduledPlayback(0, nullptr, 0);

	m_deckLinkOutput->SetScreenPreviewCallback(nullptr);

	disableOutput();

	if (m_mediaReader)
		m_mediaReader->reset(0, m_timeScale);

	m_streamTime = 0;

	setState(kDeviceIOIdle);
}

bool DeckLinkPlaybackDevice::convertVideoFrame(com_ptr<IDeckLinkVideoFrame> sourceFrame, com_ptr<IDeckLinkVideoFrame>& targetFrame)
{
	if (!sourceFrame)
		return false;

	// If there is no need to convert the pixel format, reuse the source frame instead
	if (!m_convertToDevicePixelFormat || sourceFrame->GetPixelFormat() == kDevicePixelFormat)
	{
		// Add a reference to the source frame (during construction QueryInterface adds a reference)
		targetFrame = com_ptr<IDeckLinkVideoFrame>(IID_IDeckLinkVideoFrame, sourceFrame);
		return true;
	}
	
	// Convert to device pixel format, 10-bit YUV
	com_ptr<IDeckLinkMutableVideoFrame> tempFrame;
	int32_t width 		= (int32_t)sourceFrame->GetWidth();
	int32_t height		= (int32_t)sourceFrame->GetHeight();
	int32_t rowBytes	= ((width + 47) / 48) * 128; // 10-bit YUV, refer to SDK manual for other pixel formats
	
	if (m_deckLinkOutput->CreateVideoFrame(width, height, rowBytes, kDevicePixelFormat, sourceFrame->GetFlags(), tempFrame.releaseAndGetAddressOf()) != S_OK)
		return false;
	
	if (!m_deckLinkVideoConversion || m_deckLinkVideoConversion->ConvertFrame(sourceFrame.get(), tempFrame.get()) != S_OK)
		return false;
	
	// Set the target frame
	targetFrame = com_ptr<IDeckLinkVideoFrame>(IID_IDeckLinkVideoFrame, tempFrame);
	
	return true;
}

bool DeckLinkPlaybackDevice::scheduleVideo()
{
	bool			success		= false;
	BMDTimeValue	frameTime	= 0;

	com_ptr<IDeckLinkVideoFrame> deckLinkVideoFrame;
	com_ptr<IDeckLinkVideoFrame> convertedVideoFrame;

	if (!m_mediaReader || !m_mediaReader->readVideo(deckLinkVideoFrame) || !deckLinkVideoFrame)
		goto bail;

	if (!((DeckLinkCoreMediaVideoFrame*)deckLinkVideoFrame.get())->getStreamTime(&frameTime, nullptr, m_timeScale))
		goto bail;

	if (!convertVideoFrame(deckLinkVideoFrame, convertedVideoFrame))
		goto bail;
	
	if (m_deckLinkOutput->ScheduleVideoFrame(convertedVideoFrame.get(), frameTime - m_streamTimeOffset, m_frameDuration, m_timeScale) != S_OK)
		goto bail;

	m_lastScheduledFrame = frameTime;
	success = true;

bail:
	return success;
}

bool DeckLinkPlaybackDevice::scheduleAudio(void* data, uint32_t frameCount)
{
	uint32_t sampleFramesWritten = 0;
	HRESULT result = m_deckLinkOutput->ScheduleAudioSamples(data, frameCount, m_audioStreamTime, bmdAudioSampleRate48kHz, &sampleFramesWritten);

	if (result == S_OK)
		m_audioStreamTime += frameCount;

	return result == S_OK;
}

std::string DeckLinkPlaybackDevice::filePath()
{
	if (!m_mediaReader)
		return std::string();

	return m_mediaReader->filePath();
}

BMDDisplayMode	DeckLinkPlaybackDevice::getOutputDisplayMode(float frameRate, long frameWidth, long frameHeight)
{
	BMDDisplayMode								bmdDisplayMode = bmdModeUnknown;
	std::vector<com_ptr<IDeckLinkDisplayMode> > candidateModes;

	// Create a list of all supported display modes that have a frame rate that is acceptably close to the target frame rate
	queryDisplayModes([frameRate, &candidateModes](com_ptr<IDeckLinkDisplayMode>& deckLinkDisplayMode)
	{
		BMDTimeValue frameDuration;
		BMDTimeScale timeScale;
		if (deckLinkDisplayMode->GetFrameRate(&frameDuration, &timeScale) != S_OK)
			return;

		auto modeFrameRate = (float)timeScale / (float)frameDuration;
		if ((modeFrameRate - 0.01 < frameRate) && (modeFrameRate + 0.01 > frameRate))
		{
			// For simplicity, assume SD modes are interlaced and HD modes are progressive
			auto fieldDominance = deckLinkDisplayMode->GetFieldDominance();
			if ((deckLinkDisplayMode->GetWidth() > 720 && fieldDominance != bmdProgressiveFrame) ||
				(deckLinkDisplayMode->GetWidth() <= 720 && fieldDominance != bmdLowerFieldFirst && fieldDominance != bmdUpperFieldFirst))
				return;

			candidateModes.push_back(deckLinkDisplayMode);
		}
	});

	// Sort candidate modes in ascending order by frame size
	std::sort(candidateModes.begin(), candidateModes.end(),
		[](com_ptr<IDeckLinkDisplayMode> &t1, com_ptr<IDeckLinkDisplayMode> &t2) {
			return (
				((t1->GetWidth() < t2->GetWidth()) && (t1->GetHeight() <= t2->GetHeight())) ||
				((t1->GetHeight() < t2->GetHeight()) && (t1->GetWidth() <= t2->GetWidth()))
			);
		});

	// Try to find the best fit (first mode where frame size >= target frame size)
	for (auto candidateMode : candidateModes)
	{
		if (candidateMode->GetWidth() >= frameWidth && candidateMode->GetHeight() >= frameHeight)
		{
			bmdDisplayMode = candidateMode->GetDisplayMode();
			break;
		}
	}
	return bmdDisplayMode;
}

void DeckLinkPlaybackDevice::queryDisplayModes(DeckLinkDisplayModeQueryFunc func, bool active)
{
	com_ptr<IDeckLinkDisplayModeIterator>	displayModeIterator;

	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_deckLinkOutput->GetDisplayModeIterator(displayModeIterator.releaseAndGetAddressOf()) != S_OK)
		return;

	com_ptr<IDeckLinkDisplayMode> displayMode;
	while (displayModeIterator->Next(displayMode.releaseAndGetAddressOf()) == S_OK)
	{
		if (!active || displayMode->GetDisplayMode() == m_videoOutputDisplayMode)
			func(displayMode);
	}
}

bool DeckLinkPlaybackDevice::streamTime(BMDTimeValue& streamTime, BMDTimeValue& streamDuration, BMDTimeScale& timeScale)
{
	streamTime		= m_streamTime;
	streamDuration	= m_streamDuration;
	timeScale		= m_timeScale;

	return true;
}

void DeckLinkPlaybackDevice::setState(DeviceIOState state)
{
	m_deviceState = state;
	notifyStatus(kDeviceStateChanged);
}

bool DeckLinkPlaybackDevice::isAvailable() const
{
	return m_isAvailable;
}

