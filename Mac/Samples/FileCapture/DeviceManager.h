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
#pragma once

#include <map>
#include <memory>
#include <mutex>
#include "DeckLinkAPI.h"
#include "DeckLinkCaptureDevice.h"
#include "com_ptr.h"

class DeviceManager
{
public:
	DeviceManager();
	virtual ~DeviceManager();

	void deckLinkArrived(com_ptr<IDeckLink>&& deckLink);
	void deckLinkRemoved(com_ptr<IDeckLink>&& deckLink);

	void setMediaWriter(const std::shared_ptr<DeckLinkMediaWriter>& mediaWriter);

	void setStatusListener(const DeviceManagerStatusUpdateFunc& func);
	void clearListener();

	com_ptr<DeckLinkCaptureDevice>	findDevice(intptr_t deviceID);
	com_ptr<DeckLinkCaptureDevice>	device();
	void							setDevice(com_ptr<DeckLinkCaptureDevice>& device);

private:
	std::mutex											m_mutex;
	std::map<intptr_t, com_ptr<DeckLinkCaptureDevice>>	m_devices;
	com_ptr<DeckLinkCaptureDevice>						m_device;
	std::shared_ptr<DeckLinkMediaWriter>				m_mediaWriter;
	DeviceManagerStatusUpdateFunc						m_statusListener;
};