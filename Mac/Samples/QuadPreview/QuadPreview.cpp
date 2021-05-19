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

#include "QuadPreview.h"
#include "ui_QuadPreview.h"

#include <QMessageBox>
#include <qglobal.h>
#include <vector>
#include "platform.h"
#include "QuadPreviewEvents.h"
#include "DeckLinkAPI.h"

QuadPreview::QuadPreview(QWidget *parent) :
	QDialog(parent),
	m_ui(new Ui::QuadPreview)
{
	setWindowFlags(Qt::Window
		| Qt::WindowMinimizeButtonHint
		| Qt::WindowMaximizeButtonHint
		| Qt::WindowCloseButtonHint);

	m_ui->setupUi(this);

	m_devicePages[0] = m_ui->devicePage1;
	m_devicePages[1] = m_ui->devicePage2;
	m_devicePages[2] = m_ui->devicePage3;
	m_devicePages[3] = m_ui->devicePage4;

	QSize previewViewSize = m_ui->previewContainer->size();
	previewViewSize /= 2.0;

	m_previewLayout = new QGridLayout(m_ui->previewContainer);
	m_previewLayout->setMargin(0);

	for (size_t i = 0; i < m_devicePages.size(); i++)
	{
		m_devicePages[i]->setPreviewSize(previewViewSize);
		m_previewLayout->addWidget(m_devicePages[i]->getPreviewView(), /* row */ (int)i / 2, /* column */ (int)i % 2);

		connect(m_devicePages[i], &DeckLinkInputPage::requestDeckLink, this, std::bind(&QuadPreview::requestDevice, this, m_devicePages[i], std::placeholders::_1));
		connect(m_devicePages[i], &DeckLinkInputPage::requestDeckLinkIfAvailable, std::bind(&QuadPreview::requestDeviceIfAvailable, this, m_devicePages[i], std::placeholders::_1));
		connect(m_devicePages[i], &DeckLinkInputPage::relinquishDeckLink, this, &QuadPreview::relinquishDevice);
	}

	connect(m_ui->deviceLabelCheckBox, &QCheckBox::stateChanged, this, &QuadPreview::deviceLabelEnableChanged);
	connect(m_ui->timecodeCheckBox, &QCheckBox::stateChanged, this, &QuadPreview::timecodeEnableChanged);

	show();

	setup();
}

QuadPreview::~QuadPreview()
{
}

void QuadPreview::setup()
{
	// Create and initialise DeckLink device discovery and profile objects
	m_deckLinkDiscovery = make_com_ptr<DeckLinkDeviceDiscovery>(this);

	if (m_deckLinkDiscovery)
	{
		if (!m_deckLinkDiscovery->enable())
		{
			QMessageBox::critical(this, "This application requires the DeckLink drivers installed.", "Please install the Blackmagic DeckLink drivers to use the features of this application.");
		}
	}

	m_profileCallback = new ProfileCallback(this);
	m_profileCallback->onProfileChanging(std::bind(&QuadPreview::haltStreams, this, std::placeholders::_1));
}

void QuadPreview::customEvent(QEvent *event)
{
	switch (event->type())
	{
		case kAddDeviceEvent:
		{
			DeckLinkDeviceDiscoveryEvent* discoveryEvent = dynamic_cast<DeckLinkDeviceDiscoveryEvent*>(event);
			com_ptr<IDeckLink> deckLink(discoveryEvent->DeckLink());
			addDevice(deckLink);
		}
		break;

		case kRemoveDeviceEvent:
		{
			DeckLinkDeviceDiscoveryEvent* discoveryEvent = dynamic_cast<DeckLinkDeviceDiscoveryEvent*>(event);
			com_ptr<IDeckLink> deckLink(discoveryEvent->DeckLink());
			removeDevice(deckLink);
		}
		break;

		case kProfileActivatedEvent:
		{
			ProfileActivatedEvent* profileEvent = dynamic_cast<ProfileActivatedEvent*>(event);
			com_ptr<IDeckLinkProfile> deckLinkProfile(profileEvent->deckLinkProfile());
			updateProfile(deckLinkProfile);
		}
		break;

		case kErrorRestartingCaptureEvent:
		{

		}
			break;

		default:
			break;
	}
}

void QuadPreview::closeEvent(QCloseEvent *)
{
	for (auto& devicePage : m_devicePages)
	{
		com_ptr<DeckLinkInputDevice> selectedDevice = devicePage->getSelectedDevice();
		if (selectedDevice)
		{
			// Stop capture
			if (selectedDevice->isCapturing())
				selectedDevice->stopCapture();

			// Deregister profile callback
			com_ptr<IDeckLinkProfileManager> profileManager(IID_IDeckLinkProfileManager, selectedDevice->getDeckLinkInstance());
			if (profileManager)
				profileManager->SetCallback(nullptr);
		}
	}

	m_inputDevices.clear();

	if (m_deckLinkDiscovery)
		m_deckLinkDiscovery->disable();
}

void QuadPreview::addDevice(com_ptr<IDeckLink>& deckLink)
{
	com_ptr<IDeckLinkProfileAttributes>	deckLinkAttributes(IID_IDeckLinkProfileAttributes, deckLink);
	com_ptr<IDeckLinkProfileManager>	profileManager(IID_IDeckLinkProfileManager, deckLink);
	int64_t								videoIOSupport;

	// First check that device has an input interface
	if (!deckLinkAttributes)
		return;

	if ((deckLinkAttributes->GetInt(BMDDeckLinkVideoIOSupport, &videoIOSupport) != S_OK) ||
			((videoIOSupport & bmdDeviceSupportsCapture) == 0))
		// Device does not support capture, eg DeckLink Mini Monitor
		return;

	bool active = isDeviceActive(deckLink);

	m_inputDevices[deckLink] = (active ? DeviceState::Available : DeviceState::Inactive);

	for (auto& devicePage : m_devicePages)
		devicePage->addDevice(deckLink, active);

	// Get the profile manager interface
	// Will return S_OK when the device has > 1 profiles
	if (profileManager)
		profileManager->SetCallback(m_profileCallback);
}

void QuadPreview::removeDevice(com_ptr<IDeckLink>& deckLink)
{
	// Find input device to remove
	auto deviceIter = m_inputDevices.find(deckLink);
	if (deviceIter != m_inputDevices.end())
		deviceIter = m_inputDevices.erase(deviceIter);
	else
		return;

	// Check whether device to remove was selected device, if so then stop capture
	for (auto& devicePage : m_devicePages)
	{
		com_ptr<DeckLinkInputDevice> selectedDevice = devicePage->getSelectedDevice();
		if (selectedDevice == nullptr)
			continue;

		if (selectedDevice->getDeckLinkInstance().get() == deckLink.get())
		{
			selectedDevice->stopCapture();
			break;
		}
	}

	// Remove the device from each page
	for (auto& devicePage : m_devicePages)
		devicePage->removeDevice(deckLink);
}

void QuadPreview::haltStreams(com_ptr<IDeckLinkProfile> profile)
{
	com_ptr<IDeckLink> deviceToStop;

	// Profile is changing, stop capture if running
	if (profile->GetDevice(deviceToStop.releaseAndGetAddressOf()) != S_OK)
		return;

	for (auto& devicePage : m_devicePages)
	{
		com_ptr<DeckLinkInputDevice> selectedDevice = devicePage->getSelectedDevice();
		if (selectedDevice && (selectedDevice->getDeckLinkInstance() == deviceToStop) && selectedDevice->isCapturing())
		{
			selectedDevice->stopCapture();
			break;
		}
	}
}

void QuadPreview::updateProfile(com_ptr<IDeckLinkProfile>& newProfile)
{
	// Action as if new device selected to check whether device is active/inactive
	// This will subsequently update input connections and video modes combo boxes
	com_ptr<IDeckLink>	deckLink;
	bool				deviceActive;

	if (newProfile->GetDevice(deckLink.releaseAndGetAddressOf()) != S_OK)
		return;

	auto iter = m_inputDevices.find(deckLink);
	if (iter == m_inputDevices.end())
		return;

	deviceActive = isDeviceActive(deckLink);

	iter->second = deviceActive ? DeviceState::Available : DeviceState::Inactive;
	for (auto& devicePage : m_devicePages)
	{
		com_ptr<DeckLinkInputDevice> selectedDevice = devicePage->getSelectedDevice();
		if (selectedDevice && (selectedDevice->getDeckLinkInstance() == deckLink) && !selectedDevice->isCapturing())
		{
			// Restart capture if previously halted
			devicePage->startCapture();
		}

		devicePage->enableDevice(deckLink, deviceActive);
	}
}

bool QuadPreview::isDeviceAvailable(com_ptr<IDeckLink>& device)
{
	auto iter = m_inputDevices.find(device);
	if (iter == m_inputDevices.end())
		return false;

	return (iter->second != DeviceState::Inactive);
}

void QuadPreview::requestDevice(DeckLinkInputPage* page, com_ptr<IDeckLink>& deckLink)
{
	for (auto& devicePage : m_devicePages)
	{
		// Check each page (except itself) and stop/release device if already selected
		if (devicePage == page)
			continue;

		if (devicePage->releaseDeviceIfSelected(deckLink))
			break;
	}

	page->requestedDeviceGranted(deckLink);
}

void QuadPreview::requestDeviceIfAvailable(DeckLinkInputPage* page, com_ptr<IDeckLink>& deckLink)
{
	for (auto& devicePage : m_devicePages)
	{
		// Check each page (except itself) to see if device is already selected
		if (devicePage == page)
			continue;

		com_ptr<DeckLinkInputDevice> selectedDevice = devicePage->getSelectedDevice();
		if (selectedDevice && selectedDevice->getDeckLinkInstance() == deckLink)
			return;
	}

	page->requestedDeviceGranted(deckLink);
}

void QuadPreview::relinquishDevice(com_ptr<IDeckLink>& deckLink)
{
	auto iter = m_inputDevices.find(deckLink);
	if (iter == m_inputDevices.end())
		return;

	iter->second = DeviceState::Available;
}

void QuadPreview::deviceLabelEnableChanged(bool enabled)
{
	for (int i = 0; i < m_previewLayout->count(); ++i)
	{
		DeckLinkOpenGLWidget* widget = qobject_cast<DeckLinkOpenGLWidget*>(m_previewLayout->itemAt(i)->widget());
		if (widget)
			widget->enableDeviceLabel(enabled);
	}
}

void QuadPreview::timecodeEnableChanged(bool enabled)
{
	for (int i = 0; i < m_previewLayout->count(); ++i)
	{
		DeckLinkOpenGLWidget* widget = qobject_cast<DeckLinkOpenGLWidget*>(m_previewLayout->itemAt(i)->widget());
		if (widget)
			widget->enableTimecode(enabled);
	}
}
