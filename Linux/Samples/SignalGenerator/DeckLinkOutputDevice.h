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
//  DeckLinkOutputDevice.h
//  DeckLink Output device callback
//

#pragma once

#include <QString>
#include "DeckLinkAPI.h"
#include "SignalGenerator.h"

class DeckLinkOutputDevice : public IDeckLinkVideoOutputCallback, public IDeckLinkAudioOutputCallback
{
private:
	QAtomicInt					m_refCount;
	SignalGenerator*			m_uiDelegate;
	IDeckLinkOutput*			m_deckLinkOutput;
	IDeckLink*					m_deckLink;
	QString                 	m_deviceName;

public:
	DeckLinkOutputDevice(SignalGenerator* owner, IDeckLink* deckLink);
	virtual ~DeckLinkOutputDevice();

	bool                Init();

	// IUnknown
	virtual HRESULT		QueryInterface(REFIID iid, LPVOID *ppv);
	virtual ULONG		AddRef();
	virtual ULONG		Release();

	// IDeckLinkVideoOutputCallback
	virtual HRESULT		ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result);
	virtual HRESULT		ScheduledPlaybackHasStopped();

	// IDeckLinkAudioOutputCallback
	virtual HRESULT		RenderAudioSamples(bool preroll);

	const QString		GetDeviceName() { return m_deviceName; };

	IDeckLinkOutput*    GetDeviceOutput() { return m_deckLinkOutput; };
	IDeckLink*			GetDeckLinkInstance() { return m_deckLink; };
};

