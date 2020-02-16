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
//
//  DeckLinkOutputDevice.cpp
//  Decklink output device callback
//

#include "DeckLinkOutputDevice.h"

DeckLinkOutputDevice::DeckLinkOutputDevice(SignalGenerator* owner, IDeckLink* deckLink) : 
	m_refCount(1), 
	m_uiDelegate(owner), 
	m_deckLink(deckLink), 
	m_deckLinkOutput(NULL),
	m_deckLinkConfiguration(NULL),
	m_deckLinkProfileManager(NULL)
{
	m_deckLink->AddRef();
}

DeckLinkOutputDevice::~DeckLinkOutputDevice()
{
	if (m_deckLinkProfileManager)
	{
		m_deckLinkProfileManager->Release();
		m_deckLinkProfileManager = NULL;
	}

	if (m_deckLinkConfiguration)
	{
		m_deckLinkConfiguration->Release();
		m_deckLinkConfiguration = NULL;
	}

	if (m_deckLinkOutput)
	{
		m_deckLinkOutput->SetScheduledFrameCompletionCallback(NULL);
		m_deckLinkOutput->SetAudioCallback(NULL);

		m_deckLinkOutput->Release();
		m_deckLinkOutput = NULL;
	}

	if (m_deckLink)
	{
		m_deckLink->Release();
		m_deckLink = NULL;
	}
}

bool DeckLinkOutputDevice::Init()
{
	const char*						deviceNameCStr = NULL;

	// Get output interface
	if (m_deckLink->QueryInterface(IID_IDeckLinkOutput, (void**)&m_deckLinkOutput) != S_OK)
		return false;

	// Get configuration interface
	if (m_deckLink->QueryInterface(IID_IDeckLinkConfiguration, (void**)&m_deckLinkConfiguration) != S_OK)
		return false;

	// Get device name
	if (m_deckLink->GetDisplayName(&deviceNameCStr) == S_OK)
	{
		m_deviceName = QString(deviceNameCStr);
	}
	else
	{
		m_deviceName = QString("DeckLink");
	}
	// Provide the delegate to the audio and video output interfaces
	m_deckLinkOutput->SetScheduledFrameCompletionCallback(this);
	m_deckLinkOutput->SetAudioCallback(this);

	// Get the profile manager interface
	// Will return S_OK when the device has > 1 profiles
	if (m_deckLink->QueryInterface(IID_IDeckLinkProfileManager, (void**) &m_deckLinkProfileManager) != S_OK)
	{
		m_deckLinkProfileManager = NULL;
	}

	return true;
}

HRESULT	DeckLinkOutputDevice::QueryInterface(REFIID iid, LPVOID *ppv)
{
	CFUUIDBytes		iunknown;
	HRESULT			result = E_NOINTERFACE;

	if (ppv == NULL)
		return E_INVALIDARG;

	// Initialise the return result
	*ppv = NULL;

	// Obtain the IUnknown interface and compare it the provided REFIID
	iunknown = CFUUIDGetUUIDBytes(IUnknownUUID);
	if (memcmp(&iid, &iunknown, sizeof(REFIID)) == 0)
	{
		*ppv = this;
		AddRef();
		result = S_OK;
	}
	else if (memcmp(&iid, &IID_IDeckLinkDeviceNotificationCallback, sizeof(REFIID)) == 0)
	{
		*ppv = (IDeckLinkDeviceNotificationCallback*)this;
		AddRef();
		result = S_OK;
	}

	return result;
}

ULONG DeckLinkOutputDevice::AddRef(void)
{
	int		oldValue;

	oldValue = m_refCount.fetchAndAddAcquire(1);
	return (ULONG)(oldValue + 1);
}

ULONG DeckLinkOutputDevice::Release(void)
{
	int		oldValue;

	oldValue = m_refCount.fetchAndAddAcquire(-1);
	if (oldValue == 1)
	{
		delete this;
	}

	return (ULONG)(oldValue - 1);
}

HRESULT	DeckLinkOutputDevice::ScheduledFrameCompleted(IDeckLinkVideoFrame* /* completedFrame */, BMDOutputFrameCompletionResult /* result */)
{
	// When a scheduled video frame is complete, schedule next frame
	m_uiDelegate->scheduleNextFrame(false);
	return S_OK;
}

HRESULT	DeckLinkOutputDevice::ScheduledPlaybackHasStopped()
{
	// Notify delegate that playback has stopped, so it can disable output
	m_uiDelegate->playbackStopped();
	return S_OK;
}

HRESULT	DeckLinkOutputDevice::RenderAudioSamples(bool preroll)
{
	// Provide further audio samples to the DeckLink API until our preferred buffer waterlevel is reached
	m_uiDelegate->writeNextAudioSamples();

	if (preroll)
	{
		// Start audio and video output
		m_deckLinkOutput->StartScheduledPlayback(0, 100, 1.0);
	}

	return S_OK;
}

