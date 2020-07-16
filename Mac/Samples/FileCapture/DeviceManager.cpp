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
#include "DeckLinkCaptureDevice.h"
#include "DeviceManager.h"
#include "com_ptr.h"

DeviceManager::DeviceManager() :
m_device(nullptr)
{
}

DeviceManager::~DeviceManager()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	m_devices.clear();
	m_device = nullptr;
}

void DeviceManager::deckLinkArrived(com_ptr<IDeckLink>&& deckLink)
{
	com_ptr<IDeckLinkInput>			deckLinkInput(IID_IDeckLinkInput, deckLink);
	std::lock_guard<std::mutex>		lock(m_mutex);

	if (!deckLinkInput)
		return;

	com_ptr<DeckLinkCaptureDevice>	device = make_com_ptr<DeckLinkCaptureDevice>(deckLink);
	if (!device->init(m_mediaWriter))
		return;

	m_devices[(intptr_t)deckLink.get()] = device;

	if (m_statusListener)
		m_statusListener(device, kDeviceAdded);
}

void DeviceManager::deckLinkRemoved(com_ptr<IDeckLink>&& deckLink)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	auto iter = m_devices.find((intptr_t)deckLink.get());

	if (iter != m_devices.end())
	{
		iter->second->disable();

		if(m_statusListener)
			m_statusListener(iter->second, kDeviceRemoved);

		m_devices.erase(iter);

		if (m_device && m_device->getDeviceID() == (intptr_t)deckLink.get())
			m_device = nullptr;
	}
}

void DeviceManager::setMediaWriter(const std::shared_ptr<DeckLinkMediaWriter>& mediaWriter)
{
	m_mediaWriter = mediaWriter;
}

void DeviceManager::setStatusListener(const DeviceManagerStatusUpdateFunc& func)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_statusListener = func;
}

void DeviceManager::clearListener()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_statusListener = nullptr;
}

com_ptr<DeckLinkCaptureDevice> DeviceManager::findDevice(intptr_t deviceID)
{
	auto iter = m_devices.find(deviceID);

	if (iter != m_devices.end())
		return iter->second;

	return nullptr;
}

void DeviceManager::setDevice(com_ptr<DeckLinkCaptureDevice>& device)
{
	m_device = device;
}

com_ptr<DeckLinkCaptureDevice> DeviceManager::device()
{
	return m_device;
}