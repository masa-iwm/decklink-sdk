/* -LICENSE-START-
** Copyright (c) 2019 Blackmagic Design
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

#include "DeckLinkOutputDevice.h"

DeckLinkOutputDevice::DeckLinkOutputDevice(com_ptr<IDeckLink>& device, int videoPrerollSize, BMDTimeScale hardwareTimescale) :
	m_refCount(1),
	m_state(PlaybackState::Idle),
	m_deckLink(device),
	m_deckLinkOutput(IID_IDeckLinkOutput, device),
	m_hardwareTimescale(hardwareTimescale),
	m_videoPrerollSize(videoPrerollSize),
	m_seenFirstVideoFrame(false),
	m_seenFirstAudioPacket(false),
	m_startPlaybackTime(0),
	m_scheduledFrameCompletedCallback(nullptr)
{
	// Check that device has an output interface, this will throw an error if using a capture-only device such as DeckLink Mini Recorder
	if (!m_deckLinkOutput)
		throw std::runtime_error("DeckLink device does not have an output interface");
}

// IUnknown methods

HRESULT	DeckLinkOutputDevice::QueryInterface(REFIID iid, LPVOID *ppv)
{
	HRESULT result = S_OK;

	if (ppv == nullptr)
		return E_INVALIDARG;

	// Obtain the IUnknown interface and compare it the provided REFIID
	if (iid == IID_IUnknown)
	{
		*ppv = this;
		AddRef();
	}
	else if (iid == IID_IDeckLinkVideoOutputCallback)
	{
		*ppv = (IDeckLinkVideoOutputCallback*)this;
		AddRef();
	}
	else if (iid == IID_IDeckLinkAudioOutputCallback)
	{
		*ppv = (IDeckLinkAudioOutputCallback*)this;
		AddRef();
	}
	else
	{
		*ppv = nullptr;
		result = E_NOINTERFACE;
	}

	return result;
}

ULONG DeckLinkOutputDevice::AddRef(void)
{
	return ++m_refCount;
}

ULONG DeckLinkOutputDevice::Release(void)
{
	ULONG newRefValue = --m_refCount;

	if (newRefValue == 0)
		delete this;

	return newRefValue;
}

// IDeckLinkVideoOutputCallback interface

HRESULT	DeckLinkOutputDevice::ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result)
{
	BMDTimeValue frameCompletionTimestamp;
	
	// Get frame completion timestamp
	if (completedFrame)
	{
		// Get the time that scheduled frame was completely transmitted by the device
		if (m_deckLinkOutput->GetFrameCompletionReferenceTimestamp(completedFrame, m_hardwareTimescale, &frameCompletionTimestamp) == S_OK)
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			for (auto iter = m_scheduledFramesList.rbegin(); iter != m_scheduledFramesList.rend(); iter++)
			{
				if (iter->get()->getVideoFramePtr() == completedFrame)
				{
					if (m_scheduledFrameCompletedCallback != nullptr)
					{
						iter->get()->setOutputCompletionResult(result);
						iter->get()->setOutputFrameCompletedHardwareTime(frameCompletionTimestamp - m_hardwareFrameDuration);
						m_scheduledFrameCompletedCallback(std::move(*iter));
					}
					// Erase item from reverse_iterator
					m_scheduledFramesList.erase(std::next(iter).base());
					break;
				}
			}
		}
	}

	return S_OK;
}

HRESULT	DeckLinkOutputDevice::ScheduledPlaybackHasStopped()
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_state = PlaybackState::Stopped;
	}
	m_playbackStoppedCondition.notify_one();

	return S_OK;
}

// IDeckLinkAudioOutputCallback interface

HRESULT	DeckLinkOutputDevice::RenderAudioSamples(dlbool_t preroll)
{
	return S_OK;
}

// Other methods

bool DeckLinkOutputDevice::startPlayback(BMDDisplayMode displayMode, bool enable3D, BMDPixelFormat pixelFormat, BMDAudioSampleType audioSampleType, uint32_t audioChannelCount, bool requireReferenceLocked)
{
	// Pass through RP188 timecode and VANC from input frame.  VITC timecode is forwarded with VANC
	BMDVideoOutputFlags				outputFlags = (BMDVideoOutputFlags)(bmdVideoOutputRP188 | bmdVideoOutputVANC);
	com_ptr<IDeckLinkDisplayMode>	deckLinkDisplayMode;
	dlbool_t						displayModeSupported;
	BMDSupportedVideoModeFlags		supportedVideoModeFlags = enable3D ? bmdSupportedVideoModeDualStream3D : bmdSupportedVideoModeDefault;

	m_seenFirstVideoFrame = false;
	m_seenFirstAudioPacket = false;
	m_startPlaybackTime = 0;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_state = PlaybackState::Starting;
	}

	if ((m_deckLinkOutput->DoesSupportVideoMode(bmdVideoConnectionUnspecified, displayMode, pixelFormat, bmdNoVideoOutputConversion, supportedVideoModeFlags, nullptr, &displayModeSupported) != S_OK) ||
		!displayModeSupported)
		return false;

	if (m_deckLinkOutput->GetDisplayMode(displayMode, deckLinkDisplayMode.releaseAndGetAddressOf()) != S_OK)
		return false;

	if (deckLinkDisplayMode->GetFrameRate(&m_frameDuration, &m_frameTimescale) != S_OK)
		return false;

	// Get audio water level, based on video preroll size
	m_audioWaterLevel = (uint32_t)(((int64_t)(m_videoPrerollSize * m_frameDuration) * bmdAudioSampleRate48kHz) / m_frameTimescale);

	if (enable3D)
		outputFlags = (BMDVideoOutputFlags)(outputFlags | bmdVideoOutputDualStream3D);

	// Reference DeckLinkOutputDevice delegate callbacks
	if (m_deckLinkOutput->SetScheduledFrameCompletionCallback(this) != S_OK)
		return false;
	
	if (m_deckLinkOutput->SetAudioCallback(this) != S_OK)
		return false;
	
	if (m_deckLinkOutput->EnableVideoOutput(displayMode, outputFlags) != S_OK)
		return false;

	if (m_deckLinkOutput->EnableAudioOutput(bmdAudioSampleRate48kHz, audioSampleType, audioChannelCount, bmdAudioOutputStreamTimestamped) != S_OK)
		return false;

	if (requireReferenceLocked)
	{
		if (!waitForReferenceSignalToLock())
			return false;
	}

	if (m_deckLinkOutput->BeginAudioPreroll() != S_OK)
		return false;

	m_outputVideoFrameQueue.reset();
	m_outputAudioPacketQueue.reset();
	
	// Start scheduling threads
	m_scheduleVideoFramesThread = std::thread(&DeckLinkOutputDevice::scheduleVideoFramesThread, this);
	m_scheduleAudioPacketsThread = std::thread(&DeckLinkOutputDevice::scheduleAudioPacketsThread, this);

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_state = PlaybackState::Prerolling;
	}

	return true;
}

void DeckLinkOutputDevice::stopPlayback()
{
	PlaybackState 	currentState;
	dlbool_t		scheduledPlaybackRunning = false;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		currentState = m_state;
	}

	// Cancel scheduling thread
	if ((currentState == PlaybackState::Starting) || (currentState == PlaybackState::Prerolling) || (currentState == PlaybackState::Running))
	{
		// Terminate scheduling threads 
		{
			// signal cancel flag to terminate wait condition
			std::lock_guard<std::mutex> lock(m_mutex);
			m_state = PlaybackState::Stopping;
		}
		
		m_outputVideoFrameQueue.cancelWaiters();
		m_outputAudioPacketQueue.cancelWaiters();
		
		if (m_scheduleVideoFramesThread.joinable())
			m_scheduleVideoFramesThread.join();

		if (m_scheduleAudioPacketsThread.joinable())
			m_scheduleAudioPacketsThread.join();
	}

	// In scheduled playback is running, stop video and audio streams immediately
	if ((m_deckLinkOutput->IsScheduledPlaybackRunning(&scheduledPlaybackRunning) == S_OK) && scheduledPlaybackRunning)
	{
		m_deckLinkOutput->StopScheduledPlayback(0, nullptr, 0);
		{
			// Wait for scheduled playback to complete
			std::unique_lock<std::mutex> lock(m_mutex);
			m_playbackStoppedCondition.wait(lock, [this] { return m_state == PlaybackState::Stopped; });
		}
	}

	// Disable video and audio outputs
	m_deckLinkOutput->DisableAudioOutput();
	m_deckLinkOutput->DisableVideoOutput();

	// Dereference DeckLinkOutputDevice delegate from callbacks
	m_deckLinkOutput->SetScheduledFrameCompletionCallback(nullptr);
	m_deckLinkOutput->SetAudioCallback(nullptr);
	
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_scheduledFramesList.clear();
		m_state = PlaybackState::Idle;
	}
}

void DeckLinkOutputDevice::cancelWaitForReference()
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_state == PlaybackState::Starting)
			m_state = PlaybackState::Prerolling;
	}
}

bool DeckLinkOutputDevice::getReferenceSignalMode(BMDDisplayMode* mode)
{
	com_ptr<IDeckLinkStatus> deckLinkStatus(IID_IDeckLinkStatus, m_deckLink);
	
	if (mode == nullptr)
		return false;
	
	*mode = bmdModeUnknown;
	
	if (deckLinkStatus)
	{
		dlbool_t	referenceSignalLocked;
		int64_t		referenceSignalMode;
		
		if ((deckLinkStatus->GetFlag(bmdDeckLinkStatusReferenceSignalLocked, &referenceSignalLocked) == S_OK) && referenceSignalLocked)
		{
			if (deckLinkStatus->GetInt(bmdDeckLinkStatusReferenceSignalMode, &referenceSignalMode) == S_OK)
			{
				*mode = static_cast<BMDDisplayMode>(referenceSignalMode);
			}
			return true;
		}
	}
	
	return false;
}


bool DeckLinkOutputDevice::isPlaybackActive()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_state != PlaybackState::Idle;
}

void DeckLinkOutputDevice::scheduleVideoFramesThread()
{
	while (true)
	{
		std::shared_ptr<LoopThroughVideoFrame> outputFrame;
	
		if (m_outputVideoFrameQueue.waitForSample(outputFrame))
		{
			BMDTimeValue scheduleHardwareTimestamp;
			BMDTimeValue hardwareFrameDuration;

			std::lock_guard<std::mutex> lock(m_mutex);
			// Get the hardware time when video frame was scheduled
			if (m_deckLinkOutput->GetHardwareReferenceClock(m_hardwareTimescale, &scheduleHardwareTimestamp, nullptr, &hardwareFrameDuration) != S_OK)
			{
				fprintf(stderr, "Unable to get output hardware reference clock\n");
				break;
			}

			// Record the stream time of the first frame, so we can start playing from that point
			if (!m_seenFirstVideoFrame)
			{
				m_startPlaybackTime = std::max(m_startPlaybackTime, outputFrame->getVideoStreamTime());
				m_hardwareFrameDuration = hardwareFrameDuration;
				m_seenFirstVideoFrame = true;
			}
			
			if (m_deckLinkOutput->ScheduleVideoFrame(outputFrame->getVideoFramePtr(), outputFrame->getVideoStreamTime(), m_frameDuration, m_frameTimescale) != S_OK)
			{
				fprintf(stderr, "Unable to schedule output video frame\n");
				break;
			}
			
			outputFrame->setOutputFrameScheduledHardwareTime(scheduleHardwareTimestamp);
			m_scheduledFramesList.push_back(outputFrame);

			checkEndOfPreroll();
		}
		else
		{
			// Wait for sample was cancelled
			break;
		}
	}
}

void DeckLinkOutputDevice::scheduleAudioPacketsThread()
{
	while (true)
	{
		std::shared_ptr<LoopThroughAudioPacket> outputPacket;
		
		if (m_outputAudioPacketQueue.waitForSample(outputPacket))
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			BMDTimeValue scheduleHardwareTimestamp;
			
			// Get the hardware time when audio packet was scheduled
			if (m_deckLinkOutput->GetHardwareReferenceClock(m_hardwareTimescale, &scheduleHardwareTimestamp, nullptr, nullptr) != S_OK)
			{
				fprintf(stderr, "Unable to get output hardware reference clock\n");
				break;
			}

			// Record the stream time of the first frame, so we can start playing from that point
			if (!m_seenFirstAudioPacket)
			{
				m_startPlaybackTime = std::max(m_startPlaybackTime, outputPacket->getAudioStreamTime());
				m_seenFirstAudioPacket = true;
			}

			if (m_deckLinkOutput->ScheduleAudioSamples(outputPacket->getBuffer(), (uint32_t)outputPacket->getSampleFrameCount(), outputPacket->getAudioStreamTime(), m_frameTimescale, nullptr) != S_OK)
			{
				fprintf(stderr, "Unable to schedule output audio packet\n");
				break;
			}
			
			if (m_scheduledAudioPacketCallback)
			{
				outputPacket->setOutputPacketScheduledHardwareTime(scheduleHardwareTimestamp);
				m_scheduledAudioPacketCallback(std::move(outputPacket));
			}

			checkEndOfPreroll();
		}
		else
		{
			// Wait for sample was cancelled
			break;
		}
	}
}

bool DeckLinkOutputDevice::waitForReferenceSignalToLock()
{
	com_ptr<IDeckLinkStatus>	deckLinkStatus(IID_IDeckLinkStatus, m_deckLink);
	dlbool_t					referenceSignalLocked;
	auto						isStarting = [this] { std::lock_guard<std::mutex> lock(m_mutex); return m_state == PlaybackState::Starting; };

	while (isStarting())
	{
		if ((deckLinkStatus->GetFlag(bmdDeckLinkStatusReferenceSignalLocked, &referenceSignalLocked) == S_OK) && referenceSignalLocked)
			return true;

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	return false;
}

void DeckLinkOutputDevice::checkEndOfPreroll()
{
	uint32_t prerollAudioSampleCount;
	
	// Ensure that both audio and video preroll have sufficient samples, then commence scheduled playback
	if (m_state == PlaybackState::Prerolling)
	{
		// If prerolling, check whether sufficent audio and video samples have been scheduled
		if (m_deckLinkOutput->GetBufferedAudioSampleFrameCount(&prerollAudioSampleCount) != S_OK)
		{
			fprintf(stderr, "Unable to get audio sample count\n");
			return;
		}

		if ((prerollAudioSampleCount >= m_audioWaterLevel) && (m_scheduledFramesList.size() >= m_videoPrerollSize))
		{
			m_deckLinkOutput->EndAudioPreroll();
			if (m_deckLinkOutput->StartScheduledPlayback(m_startPlaybackTime, m_frameTimescale, 1.0) != S_OK)
			{
				fprintf(stderr, "Unable to start scheduled playback\n");
				return;
			}
			
			m_state = PlaybackState::Running;
		}
	}
}