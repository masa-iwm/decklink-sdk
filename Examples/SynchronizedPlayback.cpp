/* -LICENSE-START-
 ** Copyright (c) 2018 Blackmagic Design
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

#include "platform.h"
#include <array>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#define kDeviceCount 4

// Video mode parameters
const BMDDisplayMode      kDisplayMode = bmdModeHD1080p25;
const BMDVideoOutputFlags kOutputFlag  = bmdVideoOutputSynchronizeToPlaybackGroup;
const BMDPixelFormat      kPixelFormat = bmdFormat10BitYUV;

// Frame parameters
const INT32_UNSIGNED kFrameDuration = 1000;
const INT32_UNSIGNED kTimeScale = 25000;
const INT32_UNSIGNED kFrameWidth = 1920;
const INT32_UNSIGNED kFrameHeight = 1080;
const INT32_UNSIGNED kRowBytes = 5120;
const INT32_UNSIGNED kSynchronizedPlaybackGroup = 2;

// 10-bit YUV pixels
static const size_t kFrameCount = 4;

const std::array<INT32_UNSIGNED[4], kFrameCount> kFrameData{{
	{ 0x20010200, 0x04080040, 0x20010200, 0x04080040 }, // Black
	{ 0x1d71ffc0, 0x07ff007f, 0x3c01fdd7, 0x07f75c7f }, // Blue
	{ 0x3c03e999, 0x0fa664fa, 0x1993ebc0, 0x0faf00fa }, // Red
	{ 0x069acca7, 0x2b329eb3, 0x0a7acc69, 0x2b31a6b3 }  // Green
}};

static void FillFrame(IDeckLinkMutableVideoFrame* theFrame, const INT32_UNSIGNED frameData[4])
{
	INT32_UNSIGNED* nextWord;
	INT32_UNSIGNED  wordsRemaining;

	theFrame->GetBytes((void**)&nextWord);
	wordsRemaining = (kRowBytes * kFrameHeight) / 4;

	while (wordsRemaining > 0)
	{
		*(nextWord++) = frameData[0];
		*(nextWord++) = frameData[1];
		*(nextWord++) = frameData[2];
		*(nextWord++) = frameData[3];
		wordsRemaining = wordsRemaining - 4;
	}
}

static IDeckLinkMutableVideoFrame* CreateFrame(IDeckLinkOutput* deckLinkOutput, const INT32_UNSIGNED frameData[4])
{
	HRESULT									result;
	IDeckLinkMutableVideoFrame*				frame = nullptr;

	result = deckLinkOutput->CreateVideoFrame(kFrameWidth, kFrameHeight, kRowBytes, kPixelFormat, bmdFrameFlagDefault, &frame);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not create a video frame - result = %08x\n", result);
		goto bail;
	}
	FillFrame(frame, frameData);

bail:
	return frame;
}


class DeckLinkDevice;

class OutputCallback: public IDeckLinkVideoOutputCallback
{
public:
	OutputCallback(DeckLinkDevice* deckLinkDevice) :
		m_deckLinkDevice(deckLinkDevice),
		m_refCount(1)
	{
	}

	HRESULT	STDMETHODCALLTYPE ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result) override;
	HRESULT	STDMETHODCALLTYPE ScheduledPlaybackHasStopped(void) override;

	// IUnknown needs only a dummy implementation
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override
	{
		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return ++m_refCount;
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		INT32_UNSIGNED newRefValue = --m_refCount;

		if (newRefValue == 0)
			delete this;

		return newRefValue;
	}

private:
	DeckLinkDevice*  m_deckLinkDevice;
	std::atomic<INT32_SIGNED> m_refCount;
};

class NotificationCallback : public IDeckLinkNotificationCallback
{
public:
	NotificationCallback(DeckLinkDevice* deckLinkDevice) :
		m_deckLinkDevice(deckLinkDevice),
		m_refCount(1)
	{
	}

	HRESULT STDMETHODCALLTYPE Notify(BMDNotifications topic, uint64_t param1, uint64_t param2) override;

	// IUnknown needs only a dummy implementation
	HRESULT	STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv) override
	{
		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return AtomicIncrement(&m_refCount);
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		INT32_UNSIGNED newRefValue = AtomicDecrement(&m_refCount);

		if (newRefValue == 0)
			delete this;

		return newRefValue;
	}
private:
	DeckLinkDevice*  m_deckLinkDevice;
	INT32_SIGNED m_refCount;
};

class DeckLinkDevice
{
public:
	DeckLinkDevice() :
		m_deckLink(nullptr),
		m_deckLinkConfig(nullptr),
		m_deckLinkStatus(nullptr),
		m_deckLinkNotification(nullptr),
		m_notificationCallback(nullptr),
		m_deckLinkOutput(nullptr),
		m_outputCallback(nullptr),
		m_totalFramesScheduled(0),
		m_nextFrameToSchedule(0),
		m_stopped(false)
	{
	}

	HRESULT setup(IDeckLink* deckLink)
	{
		m_deckLink = deckLink;

		// Obtain the configuration interface for the DeckLink device
		HRESULT result = m_deckLink->QueryInterface(IID_IDeckLinkConfiguration, (void**)&m_deckLinkConfig);
		if (result != S_OK)
		{
			fprintf(stderr, "Could not obtain the IDeckLinkConfiguration interface - result = %08x\n", result);
			goto bail;
		}

		// Set the synchronized playback group number. This can be any 32-bit number
		// All devices enabled for synchronized playback with the same group number are started together
		result = m_deckLinkConfig->SetInt(bmdDeckLinkConfigPlaybackGroup, kSynchronizedPlaybackGroup);
		if (result != S_OK)
		{
			fprintf(stderr, "Could not set playback group - result = %08x\n", result);
			goto bail;
		}

		// Obtain the status interface for the DeckLink device
		result = m_deckLink->QueryInterface(IID_IDeckLinkStatus, (void**)&m_deckLinkStatus);
		if (result != S_OK)
		{
			fprintf(stderr, "Could not obtain the IDeckLinkStatus interface - result = %08x\n", result);
			goto bail;
		}

		// Obtain the notification interface for the DeckLink device
		result = m_deckLink->QueryInterface(IID_IDeckLinkNotification, (void**)&m_deckLinkNotification);
		if (result != S_OK)
		{
			fprintf(stderr, "Could not obtain the IDeckLinkNotification interface - result = %08x\n", result);
			goto bail;
		}

		m_notificationCallback = new NotificationCallback(this);
		if (m_notificationCallback == nullptr)
		{
			fprintf(stderr, "Could not create notification callback object\n");
			goto bail;
		}

		// Set the callback object to the DeckLink device's notification interface
		result = m_deckLinkNotification->Subscribe(bmdStatusChanged, m_notificationCallback);
		if (result != S_OK)
		{
			fprintf(stderr, "Could not set notification callback - result = %08x\n", result);
			goto bail;
		}

		// Obtain the output interface for the DeckLink device
		result = m_deckLink->QueryInterface(IID_IDeckLinkOutput, (void**)&m_deckLinkOutput);
		if (result != S_OK)
		{
			fprintf(stderr, "Could not obtain the IDeckLinkOutput interface - result = %08x\n", result);
			goto bail;
		}

		// Create an instance of output callback
		m_outputCallback = new OutputCallback(this);
		if (m_outputCallback == nullptr)
		{
			fprintf(stderr, "Could not create output callback object\n");
			goto bail;
		}

		// Set the callback object to the DeckLink device's output interface
		result = m_deckLinkOutput->SetScheduledFrameCompletionCallback(m_outputCallback);
		if (result != S_OK)
		{
			fprintf(stderr, "Could not set output callback - result = %08x\n", result);
			goto bail;
		}

		for (size_t i = 0; i < kFrameCount; ++i)
		{
			m_frames[i] = CreateFrame(m_deckLinkOutput, kFrameData[i]);
			if (!m_frames[i])
			{
				fprintf(stderr, "Could not create frame\n");
				result = E_FAIL;
				goto bail;
			}
		}

	bail:
		return result;
	}

	HRESULT waitForReferenceLock()
	{
		/*
		 * When performing synchronized playback, all participating devices need to have their clocks locked together.
		 * This happens automatically for subdevices on the same device, but must be achieved using the reference signal
		 * for other devices.
		 */
		std::unique_lock<std::mutex> guard(m_mutex);

		HRESULT result = S_OK;

		m_referenceCondition.wait(guard, [this, &result]() -> BOOL
		{
			BOOL locked;
			result = m_deckLinkStatus->GetFlag(bmdDeckLinkStatusReferenceSignalLocked, &locked);
			if (result != S_OK)
			{
				fprintf(stderr, "Could not query reference status - result = %08x\n", result);
				return true;
			}
			return locked;
		});

		return result;
	}

	void notifyReferenceInputChanged()
	{
		m_referenceCondition.notify_all();
	}

	HRESULT prepareForPlayback()
	{
		// Enable video output
		HRESULT result = m_deckLinkOutput->EnableVideoOutput(kDisplayMode, kOutputFlag);
		if (result != S_OK)
		{
			fprintf(stderr, "Could not enable video output - result = %08x\n", result);
			goto bail;
		}

		// Schedule 3 frames
		for (int i = 0; i < 3; i++)
			scheduleNextFrame();

	bail:
		return result;
	}

	HRESULT startPlayback()
	{
		{
			std::unique_lock<std::mutex> guard(m_mutex);
			m_stopped = false;
		}

		HRESULT result = m_deckLinkOutput->StartScheduledPlayback(0, kTimeScale, 1.0);
		if (result != S_OK)
		{
			fprintf(stderr, "Could not start - result = %08x\n", result);
			goto bail;
		}

	bail:
		return result;
	}

	HRESULT stopPlayback()
	{
		HRESULT result = m_deckLinkOutput->StopScheduledPlayback(0, nullptr, kTimeScale);
		if (result != S_OK)
		{
			fprintf(stderr, "Could not stop - result = %08x\n", result);
			goto bail;
		}

	bail:
		return result;
	}

	HRESULT waitForPlaybackStop()
	{
		std::unique_lock<std::mutex> guard(m_mutex);
		if (!stopped())
			m_stopCondition.wait(guard);
		return S_OK;
	}

	void notifyDeviceStopped()
	{
		std::unique_lock<std::mutex> guard(m_mutex);
		m_stopped = true;
		m_stopCondition.notify_all();
	}

	bool stopped() const
	{
		return m_stopped;
	}

	HRESULT cleanUpFromPlayback()
	{
		HRESULT result = m_deckLinkOutput->DisableVideoOutput();
		if (result != S_OK)
		{
			fprintf(stderr, "Could not disable - result = %08x\n", result);
			goto bail;
		}

	bail:
		return result;
	}

	HRESULT scheduleNextFrame()
	{
		IDeckLinkVideoFrame* frameToSchedule = m_frames[m_nextFrameToSchedule];
		m_nextFrameToSchedule = ++m_nextFrameToSchedule % kFrameCount;

		HRESULT result = m_deckLinkOutput->ScheduleVideoFrame(frameToSchedule, m_totalFramesScheduled*kFrameDuration, kFrameDuration, kTimeScale);
		if (result != S_OK)
		{
			fprintf(stderr, "Could not schedule video frame - result = %08x\n", result);
			goto bail;
		}

		++m_totalFramesScheduled;

	bail:
		return result;
	}

	~DeckLinkDevice()
	{
		if (m_outputCallback)
		{
			m_deckLinkOutput->SetScheduledFrameCompletionCallback(nullptr);
			m_outputCallback->Release();
		}

		if (m_notificationCallback)
		{
			m_deckLinkNotification->Unsubscribe(bmdStatusChanged, m_notificationCallback);
			m_notificationCallback->Release();
		}

		if (m_deckLink)
			m_deckLink->Release();

		if (m_deckLinkConfig)
			m_deckLinkConfig->Release();

		if (m_deckLinkStatus)
			m_deckLinkStatus->Release();

		if (m_deckLinkOutput)
			m_deckLinkOutput->Release();

		if (m_deckLinkNotification)
			m_deckLinkNotification->Release();

		for (auto frame : m_frames)
		{
			if (frame)
				frame->Release();
		}
	}

private:
	IDeckLink*										m_deckLink;
	IDeckLinkConfiguration*							m_deckLinkConfig;
	IDeckLinkStatus*								m_deckLinkStatus;
	IDeckLinkNotification*							m_deckLinkNotification;
	NotificationCallback*							m_notificationCallback;
	IDeckLinkOutput*								m_deckLinkOutput;
	OutputCallback*									m_outputCallback;
	std::array<IDeckLinkVideoFrame*, kFrameCount>	m_frames;
	INT32_UNSIGNED									m_totalFramesScheduled;
	INT32_UNSIGNED									m_nextFrameToSchedule;
	std::mutex										m_mutex;
	bool											m_stopped;
	std::condition_variable							m_stopCondition;
	std::condition_variable							m_referenceCondition;
};

HRESULT	OutputCallback::ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result)
{
	// When a video frame completes, reschedule another frame
	if (!m_deckLinkDevice->stopped())
		m_deckLinkDevice->scheduleNextFrame();
	return S_OK;
}

HRESULT	OutputCallback::ScheduledPlaybackHasStopped(void)
{
	m_deckLinkDevice->notifyDeviceStopped();
	return S_OK;
}

HRESULT NotificationCallback::Notify(BMDNotifications topic, uint64_t param1, uint64_t param2)
{
	if (topic != bmdStatusChanged)
		return S_OK;

	if ((BMDDeckLinkStatusID)param1 != bmdDeckLinkStatusReferenceSignalLocked)
		return S_OK;

	m_deckLinkDevice->notifyReferenceInputChanged();
	return S_OK;
}

static BOOL supportsSynchronizedPlayback(IDeckLink* deckLink)
{
	IDeckLinkProfileAttributes* attributes = nullptr;
	HRESULT result = E_FAIL;

	result = deckLink->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&attributes);
	if (result != S_OK)
		return false;

	BOOL supported = false;
	result = attributes->GetFlag(BMDDeckLinkSupportsSynchronizeToPlaybackGroup, &supported);
	if (result != S_OK)
		supported = false;

	attributes->Release();

	return supported;
}

int main(int argc, const char * argv[])
{

	IDeckLinkIterator*      deckLinkIterator = nullptr;
	IDeckLink*				deckLink = nullptr;
	std::array<DeckLinkDevice, kDeviceCount> deckLinkDevices;
	HRESULT                 result;

	Initialize();

	// Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
	result = GetDeckLinkIterator(&deckLinkIterator);
	if (result != S_OK)
	{
		fprintf(stderr, "A DeckLink iterator could not be created.  The DeckLink drivers may not be installed.\n");
		goto bail;
	}

	for (auto& device : deckLinkDevices)
	{
		// Obtain the next DeckLink device
		while (true)
		{
			result = deckLinkIterator->Next(&deckLink);
			if (result != S_OK)
			{
				fprintf(stderr, "Could not find DeckLink device - result = %08x\n", result);
				goto bail;
			}

			if (supportsSynchronizedPlayback(deckLink))
				break;

			deckLink->Release();
		}

		result = device.setup(deckLink);
		if (result != S_OK)
			goto bail;

		deckLink = nullptr;
	}

	for (auto& device : deckLinkDevices)
	{
		result = device.prepareForPlayback();
		if (result != S_OK)
			goto bail;
	}

	// Wait for devices to lock to the reference signal
	printf("Waiting for reference lock...\n");

	for (auto& device : deckLinkDevices)
	{
		result = device.waitForReferenceLock();
		if (result != S_OK)
			goto bail;
	}

	// Start playback - This only needs to be performed on one device in the group
	result = deckLinkDevices[0].startPlayback();
	if (result != S_OK)
		goto bail;

	// Wait until user presses Enter
	printf("Monitoring... Press <RETURN> to exit\n");

	getchar();

	printf("Exiting.\n");

	// Stop capture - This only needs to be performed on one device in the group
	result = deckLinkDevices[0].stopPlayback();

	// If we're stopping in the future, we need to wait until the devices have stopped before we can clean up
	for (auto& device : deckLinkDevices)
	{
		result = device.waitForPlaybackStop();
		if (result != S_OK)
			goto bail;
	}

	// Disable the video input interface
	for (auto& device : deckLinkDevices)
		result = device.cleanUpFromPlayback();

	// Release resources
bail:

	// Release the Decklink object
	if (deckLink != nullptr)
		deckLink->Release();

	// Release the DeckLink iterator
	if (deckLinkIterator != nullptr)
		deckLinkIterator->Release();

	return (result == S_OK) ? 0 : 1;
}

