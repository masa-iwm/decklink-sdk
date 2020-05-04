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


#include <QMessageBox>
#include <cmath>
#include <utility>
#include <map>

#include "ColorBars.h"
#include "HDRVideoFrame.h"
#include "SignalGenHDR.h"
#include "ui_SignalGenHDR.h"

// Define conventional display primaries and reference white for colorspace
static const ChromaticityCoordinates kDefaultRec2020Colorimetrics		= { 0.708, 0.292, 0.170, 0.797, 0.131, 0.046, 0.3127, 0.3290 };
static const double kDefaultMaxDisplayMasteringLuminance	= 1000.0;
static const double kDefaultMinDisplayMasteringLuminance	= 0.0001;
static const double kDefaultMaxCLL							= 1000.0;
static const double kDefaultMaxFALL							= 50.0;

// Supported pixel formats map to string representation and boolean if RGB format
static const std::map<BMDPixelFormat, std::pair<QString, bool>> kPixelFormats = {
	std::make_pair(bmdFormat10BitYUV,	std::make_pair(QString("10-bit YUV (Video-range)"), false)),
	std::make_pair(bmdFormat10BitRGB,	std::make_pair(QString("10-bit RGB (Video-range)"), true)),
	std::make_pair(bmdFormat12BitRGBLE, std::make_pair(QString("12-bit RGB (Full-range)"), true)),
};

// Supported EOTFs
static const std::vector<std::pair<EOTF, QString>> kSupportedEOTF = {
	std::make_pair(EOTF::PQ,	QString("PQ (ST 2084)")),
	std::make_pair(EOTF::HLG,	QString("HLG")),
};

static int GetBytesPerRow(BMDPixelFormat pixelFormat, ULONG frameWidth)
{
	int bytesPerRow;

	// Refer to DeckLink SDK Manual - 2.7.4 Pixel Formats
	switch (pixelFormat)
	{
	case bmdFormat12BitRGBLE:
		bytesPerRow = (frameWidth * 36) / 8;
		break;

	case bmdFormat10BitYUV:
		bytesPerRow = ((frameWidth + 47) / 48) * 128;
		break;

	case bmdFormat10BitRGB:
		bytesPerRow = ((frameWidth + 63) / 64) * 256;
		break;

	default:
		bytesPerRow = frameWidth * 4;
		break;
	}

	return bytesPerRow;
}

SignalGenHDR::SignalGenHDR(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::SignalGenHDR),
	m_running(false)
{
	ui->setupUi(this);

	layout = new QGridLayout(ui->previewWidget);
	layout->setMargin(0);

	m_previewView = new DeckLinkOpenGLWidget(dynamic_cast<QWidget*>(this));
	m_previewView->resize(ui->previewWidget->size());
	m_previewView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	layout->addWidget(m_previewView, 0, 0, 0, 0);
	m_previewView->clear();

	connect(ui->startButton, SIGNAL(clicked()), this, SLOT(ToggleStart()));

	connect(ui->ouputDeviceComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(OutputDeviceChanged(int)));
	connect(ui->videoFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(VideoFormatChanged(int)));
	connect(ui->pixelFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(PixelFormatChanged(int)));
	connect(ui->eotfComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(EOTFChanged(int)));

	connect(ui->displayPrimaryRedXSlider, SIGNAL(valueChanged(int)), this, SLOT(DisplayPrimaryRedXSliderChanged(int)));
	connect(ui->displayPrimaryRedYSlider, SIGNAL(valueChanged(int)), this, SLOT(DisplayPrimaryRedYSliderChanged(int)));
	connect(ui->displayPrimaryGreenXSlider, SIGNAL(valueChanged(int)), this, SLOT(DisplayPrimaryGreenXSliderChanged(int)));
	connect(ui->displayPrimaryGreenYSlider, SIGNAL(valueChanged(int)), this, SLOT(DisplayPrimaryGreenYSliderChanged(int)));
	connect(ui->displayPrimaryBlueXSlider, SIGNAL(valueChanged(int)), this, SLOT(DisplayPrimaryBlueXSliderChanged(int)));
	connect(ui->displayPrimaryBlueYSlider, SIGNAL(valueChanged(int)), this, SLOT(DisplayPrimaryBlueYSliderChanged(int)));
	connect(ui->whitePointXSlider, SIGNAL(valueChanged(int)), this, SLOT(WhitePointXSliderChanged(int)));
	connect(ui->whitePointYSlider, SIGNAL(valueChanged(int)), this, SLOT(WhitePointYSliderChanged(int)));
	connect(ui->maxDisplayMasteringLuminanceSlider, SIGNAL(valueChanged(int)), this, SLOT(MaxDisplayMasteringLuminanceSliderChanged(int)));
	connect(ui->minDisplayMasteringLuminanceSlider, SIGNAL(valueChanged(int)), this, SLOT(MinDisplayMasteringLuminanceSliderChanged(int)));
	connect(ui->maxCLLSlider, SIGNAL(valueChanged(int)), this, SLOT(MaxCLLSliderChanged(int)));
	connect(ui->maxFALLSlider, SIGNAL(valueChanged(int)), this, SLOT(MaxFALLSliderChanged(int)));

	m_selectedHDRParameters = { static_cast<int64_t>(EOTF::PQ),
								kDefaultRec2020Colorimetrics,
								kDefaultMaxDisplayMasteringLuminance,
								kDefaultMinDisplayMasteringLuminance,
								kDefaultMaxCLL,
								kDefaultMaxFALL };

	ui->displayPrimaryRedXSlider->setSliderPosition((int)(m_selectedHDRParameters.referencePrimaries.RedX * 1000));
	ui->displayPrimaryRedYSlider->setSliderPosition((int)(m_selectedHDRParameters.referencePrimaries.RedY * 1000));
	ui->displayPrimaryGreenXSlider->setSliderPosition((int)(m_selectedHDRParameters.referencePrimaries.GreenX * 1000));
	ui->displayPrimaryGreenYSlider->setSliderPosition((int)(m_selectedHDRParameters.referencePrimaries.GreenY * 1000));
	ui->displayPrimaryBlueXSlider->setSliderPosition((int)(m_selectedHDRParameters.referencePrimaries.BlueX * 1000));
	ui->displayPrimaryBlueYSlider->setSliderPosition((int)(m_selectedHDRParameters.referencePrimaries.BlueY * 1000));
	ui->whitePointXSlider->setSliderPosition((int)(m_selectedHDRParameters.referencePrimaries.WhiteX * 10000));
	ui->whitePointYSlider->setSliderPosition((int)(m_selectedHDRParameters.referencePrimaries.WhiteY * 10000));
	ui->maxDisplayMasteringLuminanceSlider->setSliderPosition((int)(std::log10(m_selectedHDRParameters.maxDisplayMasteringLuminance) * 10000));
	ui->minDisplayMasteringLuminanceSlider->setSliderPosition((int)(std::log10(m_selectedHDRParameters.minDisplayMasteringLuminance) * 10000));
	ui->maxCLLSlider->setSliderPosition((int)(std::log10(m_selectedHDRParameters.maxCLL) * 10000));
	ui->maxFALLSlider->setSliderPosition((int)(std::log10(m_selectedHDRParameters.maxFALL) * 10000));

	// Disable interface on load
	EnableInterface(false);
	EnableHDRInterface(false);
	ui->startButton->setEnabled(false);

	show();
}

SignalGenHDR::~SignalGenHDR()
{
	delete ui;
}

void SignalGenHDR::setup()
{
	// Create and initialise DeckLink device discovery
	m_deckLinkDiscovery = make_com_ptr<DeckLinkDeviceDiscovery>(this);
	if (m_deckLinkDiscovery)
	{
		if (!m_deckLinkDiscovery->enable())
		{
			QMessageBox::critical(this, "This application requires the DeckLink drivers installed.", "Please install the Blackmagic DeckLink drivers to use the features of this application.");
		}
	}
}

void SignalGenHDR::customEvent(QEvent *event)
{
	if (event->type() == kAddDeviceEvent)
	{
		DeckLinkDeviceDiscoveryEvent* discoveryEvent = dynamic_cast<DeckLinkDeviceDiscoveryEvent*>(event);
		AddDevice(discoveryEvent->DeckLink());
	}
	else if (event->type() == kRemoveDeviceEvent)
	{
		DeckLinkDeviceDiscoveryEvent* discoveryEvent = dynamic_cast<DeckLinkDeviceDiscoveryEvent*>(event);
		RemoveDevice(discoveryEvent->DeckLink());
	}
}

void SignalGenHDR::closeEvent(QCloseEvent *)
{
	// Stop output signal
	if (m_selectedDeckLinkOutput && m_running)
		StopRunning();

	// Disable DeckLink device discovery
	m_deckLinkDiscovery->disable();

	// Release supported display modes
	m_supportedDisplayModeMap.clear();
}

void SignalGenHDR::ToggleStart()
{
	if (!m_selectedDeckLinkOutput)
		return;

	if (!m_running)
		StartRunning();
	else
		StopRunning();
}

void SignalGenHDR::StartRunning()
{
	bool		output444;
	HRESULT		result;

	// Set the output to 444 if RGB mode is selected
	try
	{
		std::tie(std::ignore, output444) = kPixelFormats.at(m_selectedPixelFormat);
	}
	catch (std::out_of_range)
	{
		goto bail;
	}
	
	result = m_selectedDeckLinkConfiguration->SetFlag(bmdDeckLinkConfig444SDIVideoOutput, output444);
	// If a device without SDI output is used (eg Intensity Pro 4K), then SetFlags will return E_NOTIMPL
	if ((result != S_OK) && (result != E_NOTIMPL))
		goto bail;

	// Set the video output mode
	if (m_selectedDeckLinkOutput->EnableVideoOutput(m_selectedDisplayMode->GetDisplayMode(), bmdVideoOutputFlagDefault) != S_OK)
		goto bail;

	m_videoFrameBars = CreateColorbarsFrame();
	if (!m_videoFrameBars)
		goto bail;

	if (m_selectedDeckLinkOutput->DisplayVideoFrameSync(m_videoFrameBars.get()) != S_OK)
		goto bail;

	// Success, update the UI
	m_running = true;
	ui->startButton->setText("Stop");
	// Disable the user interface while running (prevent the user from making changes to the output signal)
	EnableInterface(false);

	return;

bail:
	QMessageBox::critical(this, "Unable to start playback.", "Could not start playback, is the device already in use?");

	StopRunning();
}

void SignalGenHDR::StopRunning()
{
	m_selectedDeckLinkOutput->DisableVideoOutput();

	// Update UI
	m_running = false;
	ui->startButton->setText("Start");
	EnableInterface(true);
}

void SignalGenHDR::EnableInterface(bool enable)
{
	// Set the enable state of combo boxes in properties group box
	for (auto& combobox : ui->propertiesGroupBox->findChildren<QComboBox*>())
	{
		combobox->setEnabled(enable);
	}
}

void SignalGenHDR::EnableHDRInterface(bool enable)
{
	bool enableMetadata = enable && (ui->eotfComboBox->itemData(ui->eotfComboBox->currentIndex()) != (int)EOTF::HLG);

	ui->eotfComboBox->setEnabled(enable);

	// Set the enable state of sliders in HDR group box
	for (auto& combobox : ui->hdrGroupBox->findChildren<QSlider*>())
	{
		combobox->setEnabled(enableMetadata);
	}
}

void SignalGenHDR::RefreshDisplayModeMenu(void)
{
	com_ptr<IDeckLinkDisplayModeIterator>	displayModeIterator;
	com_ptr<IDeckLinkDisplayMode>			deckLinkDisplayMode;

	m_supportedDisplayModeMap.clear();
	ui->videoFormatComboBox->clear();

	if (m_selectedDeckLinkOutput->GetDisplayModeIterator(displayModeIterator.releaseAndGetAddressOf()) != S_OK)
		return;

	// Populate the display mode menu with a list of display modes supported by the installed DeckLink card
	while (displayModeIterator->Next(deckLinkDisplayMode.releaseAndGetAddressOf()) == S_OK)
	{
		char*					modeName;
		BMDDisplayMode			displayMode;

		if (deckLinkDisplayMode->GetName(const_cast<const char**>(&modeName)) != S_OK)
			continue;

		// Ignore NTSC/PAL/720p/1080i display modes
		if ((deckLinkDisplayMode->GetWidth() < 1920) || (deckLinkDisplayMode->GetFieldDominance() != bmdProgressiveFrame))
			continue;

		displayMode = deckLinkDisplayMode->GetDisplayMode();
		m_supportedDisplayModeMap[displayMode] = deckLinkDisplayMode;

		ui->videoFormatComboBox->addItem(QString(modeName), QVariant::fromValue((uint64_t)displayMode));

		free(modeName);
	}

	if (ui->videoFormatComboBox->count() == 0)
	{
		ui->startButton->setEnabled(false);
	}
	else
	{
		ui->startButton->setEnabled(true);
		ui->videoFormatComboBox->setCurrentIndex(0);
	}
}

void SignalGenHDR::RefreshPixelFormatMenu(void)
{
	ui->pixelFormatComboBox->clear();

	for (auto& pixelFormat : kPixelFormats)
	{
		HRESULT		hr;
		bool		displayModeSupport = false;
		QString		pixelFormatString;
		
		std::tie(pixelFormatString, std::ignore) = pixelFormat.second;

		hr = m_selectedDeckLinkOutput->DoesSupportVideoMode(bmdVideoConnectionUnspecified, m_selectedDisplayMode->GetDisplayMode(), pixelFormat.first, bmdNoVideoOutputConversion, bmdSupportedVideoModeDefault, nullptr, &displayModeSupport);
		if (hr != S_OK || !displayModeSupport)
			continue;

		ui->pixelFormatComboBox->addItem(pixelFormatString, QVariant::fromValue(pixelFormat.first));
	}

	ui->pixelFormatComboBox->setCurrentIndex(0);
}

void SignalGenHDR::RefreshEOTFMenu(void)
{
	ui->eotfComboBox->clear();

	for (auto& eotf : kSupportedEOTF)
	{
		// Full-range not defined for HLG EOTF
		if ((eotf.first == EOTF::HLG) && (m_selectedPixelFormat == bmdFormat12BitRGBLE))
			continue;

		ui->eotfComboBox->addItem(eotf.second, QVariant::fromValue(static_cast<int64_t>(eotf.first)));
	}

	ui->eotfComboBox->setCurrentIndex(0);
}

void SignalGenHDR::AddDevice(com_ptr<IDeckLink> deckLink)
{
	char*								deviceNameStr;
	int64_t								intAttribute;
	bool								attributeFlag = false;
	com_ptr<IDeckLinkProfileAttributes>	deckLinkAttributes(IID_IDeckLinkProfileAttributes, deckLink);

	if (!deckLinkAttributes)
		return;

	// Check that device has playback interface
	if ((deckLinkAttributes->GetInt(BMDDeckLinkVideoIOSupport, &intAttribute) != S_OK)
		|| ((intAttribute & bmdDeviceSupportsPlayback) == 0))
		return;

	// Check that device supports HDR metadata and Rec.2020
	if ((deckLinkAttributes->GetFlag(BMDDeckLinkSupportsHDRMetadata, &attributeFlag) != S_OK)
		|| !attributeFlag)
		return;

	if ((deckLinkAttributes->GetFlag(BMDDeckLinkSupportsColorspaceMetadata, &attributeFlag) != S_OK)
		|| !attributeFlag)
		return;

	// Get device name
	if (deckLink->GetDisplayName(const_cast<const char**>(&deviceNameStr)) != S_OK)
		return;

	// Add to output device list popup
	ui->ouputDeviceComboBox->addItem(QString(deviceNameStr), QVariant::fromValue((void*)deckLink.get()));

	free(deviceNameStr);

	if (ui->ouputDeviceComboBox->count() == 1)
	{
		// We have added our first item, enable the interface
		ui->ouputDeviceComboBox->setCurrentIndex(0);

		ui->startButton->setEnabled(true);
		EnableInterface(true);
		EnableHDRInterface(true);
	}
}

void SignalGenHDR::RemoveDevice(com_ptr<IDeckLink> deckLink)
{
	int		indexToRemove;
	bool	removeSelectedDevice = false;

	// Find the combo box entry to remove
	indexToRemove = ui->ouputDeviceComboBox->findData(QVariant::fromValue((void*)deckLink.get()));
	if (indexToRemove < 0)
		return;

	removeSelectedDevice = (IDeckLink*)(((QVariant)ui->ouputDeviceComboBox->itemData(ui->ouputDeviceComboBox->currentIndex())).value<void*>()) == deckLink.get();

	ui->ouputDeviceComboBox->removeItem(indexToRemove);

	if (removeSelectedDevice)
	{
		// If playback is ongoing, stop it
		if (m_running)
			StopRunning();

		if (ui->ouputDeviceComboBox->count() > 0)
		{
			// Select the first device in the list and enable the interface
			ui->ouputDeviceComboBox->setCurrentIndex(0);
			ui->startButton->setEnabled(true);
		}
	}

	if (ui->ouputDeviceComboBox->count() == 0)
	{
		// We have removed the last item, disable the interface
		ui->startButton->setEnabled(false);
		EnableInterface(false);
		EnableHDRInterface(false);

		m_selectedDeckLinkOutput = nullptr;
	}
}

void SignalGenHDR::OutputDeviceChanged(int selectedDeviceIndex)
{
	com_ptr<IDeckLink> selectedDeckLink;

	if (selectedDeviceIndex == -1)
		return;

	selectedDeckLink = (IDeckLink*)(((QVariant)ui->ouputDeviceComboBox->itemData(selectedDeviceIndex)).value<void*>());

	// Get output interface
	m_selectedDeckLinkOutput = com_ptr<IDeckLinkOutput>(IID_IDeckLinkOutput, selectedDeckLink);

	if (!m_selectedDeckLinkOutput)
		return;

	// Get configuration interface
	m_selectedDeckLinkConfiguration = com_ptr<IDeckLinkConfiguration>(IID_IDeckLinkConfiguration, selectedDeckLink);

	if (!m_selectedDeckLinkConfiguration)
		return;

	// Update the  display mode popup menu
	RefreshDisplayModeMenu();

	// Set Screen Preview callback for selected device
	m_selectedDeckLinkOutput->SetScreenPreviewCallback(m_previewView->delegate());

	// Enable the interface
	EnableInterface(true);
	EnableHDRInterface(true);
}

void SignalGenHDR::VideoFormatChanged(int selectedVideoFormatIndex)
{
	if (selectedVideoFormatIndex == -1)
		return;

	auto iter = m_supportedDisplayModeMap.find(((QVariant)ui->videoFormatComboBox->itemData(selectedVideoFormatIndex)).value<BMDDisplayMode>());
	if (iter == m_supportedDisplayModeMap.end())
		return;

	m_selectedDisplayMode = iter->second;

	RefreshPixelFormatMenu();
}

void SignalGenHDR::PixelFormatChanged(int selectedPixelFormatIndex)
{
	if (selectedPixelFormatIndex == -1)
		return;

	m_selectedPixelFormat = (((QVariant)ui->pixelFormatComboBox->itemData(selectedPixelFormatIndex)).value<BMDPixelFormat>());
	RefreshEOTFMenu();
}

void SignalGenHDR::EOTFChanged(int selectedEOTFIndex)
{
	if (selectedEOTFIndex == -1)
		return;

	m_selectedHDRParameters.EOTF = (((QVariant)ui->eotfComboBox->itemData(selectedEOTFIndex)).value<int64_t>());
	EnableHDRInterface(true);
	UpdateOutputFrame();
}

void SignalGenHDR::DisplayPrimaryRedXSliderChanged(int displayPrimaryRedXValue)
{
	m_selectedHDRParameters.referencePrimaries.RedX = (double)displayPrimaryRedXValue / 1000;
	ui->displayPrimaryRedXLineEdit->setText(QString::number(m_selectedHDRParameters.referencePrimaries.RedX, 'f', 3));
	UpdateOutputFrame();
}

void SignalGenHDR::DisplayPrimaryRedYSliderChanged(int displayPrimaryRedYValue)
{
	m_selectedHDRParameters.referencePrimaries.RedY = (double)displayPrimaryRedYValue / 1000;
	ui->displayPrimaryRedYLineEdit->setText(QString::number(m_selectedHDRParameters.referencePrimaries.RedY, 'f', 3));
	UpdateOutputFrame();
}

void SignalGenHDR::DisplayPrimaryGreenXSliderChanged(int displayPrimaryGreenXValue)
{
	m_selectedHDRParameters.referencePrimaries.GreenX = (double)displayPrimaryGreenXValue / 1000;
	ui->displayPrimaryGreenXLineEdit->setText(QString::number(m_selectedHDRParameters.referencePrimaries.GreenX, 'f', 3));
	UpdateOutputFrame();
}

void SignalGenHDR::DisplayPrimaryGreenYSliderChanged(int displayPrimaryGreenYValue)
{
	m_selectedHDRParameters.referencePrimaries.GreenY = (double)displayPrimaryGreenYValue / 1000;
	ui->displayPrimaryGreenYLineEdit->setText(QString::number(m_selectedHDRParameters.referencePrimaries.GreenY, 'f', 3));
	UpdateOutputFrame();
}

void SignalGenHDR::DisplayPrimaryBlueXSliderChanged(int displayPrimaryBlueXValue)
{
	m_selectedHDRParameters.referencePrimaries.BlueX = (double)displayPrimaryBlueXValue / 1000;
	ui->displayPrimaryBlueXLineEdit->setText(QString::number(m_selectedHDRParameters.referencePrimaries.BlueX, 'f', 3));
	UpdateOutputFrame();
}

void SignalGenHDR::DisplayPrimaryBlueYSliderChanged(int displayPrimaryBlueYValue)
{
	m_selectedHDRParameters.referencePrimaries.BlueY = (double)displayPrimaryBlueYValue / 1000;
	ui->displayPrimaryBlueYLineEdit->setText(QString::number(m_selectedHDRParameters.referencePrimaries.BlueY, 'f', 3));
	UpdateOutputFrame();
}

void SignalGenHDR::WhitePointXSliderChanged(int whitePointXValue)
{
	m_selectedHDRParameters.referencePrimaries.WhiteX = (double)whitePointXValue / 10000;
	ui->whitePointXLineEdit->setText(QString::number(m_selectedHDRParameters.referencePrimaries.WhiteX, 'f', 4));
	UpdateOutputFrame();
}

void SignalGenHDR::WhitePointYSliderChanged(int whitePointYValue)
{
	m_selectedHDRParameters.referencePrimaries.WhiteY = (double)whitePointYValue / 10000;
	ui->whitePointYLineEdit->setText(QString::number(m_selectedHDRParameters.referencePrimaries.WhiteY, 'f', 4));
	UpdateOutputFrame();
}

void SignalGenHDR::MaxDisplayMasteringLuminanceSliderChanged(int maxDisplayMasteringLuminanceValue)
{
	m_selectedHDRParameters.maxDisplayMasteringLuminance = std::pow(10.0, (double)maxDisplayMasteringLuminanceValue / 10000);
	ui->maxDisplayMasteringLuminanceLineEdit->setText(QString::number(m_selectedHDRParameters.maxDisplayMasteringLuminance, 'f', 0));
	UpdateOutputFrame();
}

void SignalGenHDR::MinDisplayMasteringLuminanceSliderChanged(int minDisplayMasteringLuminanceValue)
{
	m_selectedHDRParameters.minDisplayMasteringLuminance = std::pow(10.0, (double)minDisplayMasteringLuminanceValue / 10000);
	ui->minDisplayMasteringLuminanceLineEdit->setText(QString::number(m_selectedHDRParameters.minDisplayMasteringLuminance, 'f', 4));
	UpdateOutputFrame();
}

void SignalGenHDR::MaxCLLSliderChanged(int maxCLLValue)
{
	m_selectedHDRParameters.maxCLL = std::pow(10.0, (double)maxCLLValue / 10000);
	ui->maxCLLLineEdit->setText(QString::number(m_selectedHDRParameters.maxCLL, 'f', 0));
	UpdateOutputFrame();
}

void SignalGenHDR::MaxFALLSliderChanged(int maxFALLValue)
{
	m_selectedHDRParameters.maxFALL = std::pow(10.0, (double)maxFALLValue / 10000);
	ui->maxFALLLineEdit->setText(QString::number(m_selectedHDRParameters.maxFALL, 'f', 0));
	UpdateOutputFrame();
}

void SignalGenHDR::UpdateOutputFrame()
{
	if (m_running)
	{
		// Update the HDR metadata in frame and output
		// As these statements are sequential, they are thread safe.  It is not recommended
		// that an active frame is updated, and normally synchronization will be required
		m_videoFrameBars->UpdateHDRMetadata(m_selectedHDRParameters);
		m_selectedDeckLinkOutput->DisplayVideoFrameSync(m_videoFrameBars.get());
	}
}

com_ptr<HDRVideoFrame> SignalGenHDR::CreateColorbarsFrame()
{
	com_ptr<IDeckLinkMutableVideoFrame>	referenceFrame;
	com_ptr<IDeckLinkMutableVideoFrame>	displayFrame;
	HRESULT								hr;
	BMDPixelFormat						referencePixelFormat;
	int									referenceFrameBytesPerRow;
	int									displayFrameBytesPerRow;
	com_ptr<IDeckLinkVideoConversion>	frameConverter;
	unsigned long						frameWidth;
	unsigned long						frameHeight;
	EOTFColorRange						colorRange;
	com_ptr<HDRVideoFrame>				ret;

	frameWidth = m_selectedDisplayMode->GetWidth();
	frameHeight = m_selectedDisplayMode->GetHeight();

	displayFrameBytesPerRow = GetBytesPerRow(m_selectedPixelFormat, frameWidth);

	if (m_selectedHDRParameters.EOTF == static_cast<int64_t>(EOTF::HLG))
		colorRange = EOTFColorRange::HLGVideoRange;
	else if (m_selectedPixelFormat == bmdFormat12BitRGBLE)
		colorRange = EOTFColorRange::PQFullRange;
	else
		colorRange = EOTFColorRange::PQVideoRange;

	hr = m_selectedDeckLinkOutput->CreateVideoFrame(frameWidth, frameHeight, displayFrameBytesPerRow, m_selectedPixelFormat, bmdFrameFlagDefault, displayFrame.releaseAndGetAddressOf());
	if (hr != S_OK)
		goto bail;

	referencePixelFormat = (m_selectedPixelFormat == bmdFormat12BitRGBLE) ? bmdFormat12BitRGBLE : bmdFormat10BitRGB;

	if (m_selectedPixelFormat == referencePixelFormat)
	{
		// output pixel format matches reference, so can be filled directly without conversion
		FillBT2111ColorBars(displayFrame, colorRange);
	}
	else
	{
		referenceFrameBytesPerRow = GetBytesPerRow(referencePixelFormat, frameWidth);

		// If the pixel formats are different create and fill reference frame
		hr = m_selectedDeckLinkOutput->CreateVideoFrame(frameWidth, frameHeight, referenceFrameBytesPerRow, referencePixelFormat, bmdFrameFlagDefault, referenceFrame.releaseAndGetAddressOf());
		if (hr != S_OK)
			goto bail;
		FillBT2111ColorBars(referenceFrame, colorRange);

		// Convert to required pixel format
		frameConverter = CreateVideoConversionInstance();

		hr = frameConverter->ConvertFrame(referenceFrame.get(), displayFrame.get());
		if (hr != S_OK)
			goto bail;
	}

	ret = new HDRVideoFrame(displayFrame, m_selectedHDRParameters);

bail:
	return ret;
}
