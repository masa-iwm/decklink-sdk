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

// CapturePreviewDlg.cpp : implementation file
//

#include "stdafx.h"
#include <vector>
#include "CapturePreview.h"
#include "CapturePreviewDlg.h"
#include "DeckLinkDevice.h"
#include "ProfileCallback.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


using namespace std;

static const vector<pair<BMDVideoConnection, CString>> kInputConnections =
{
	{ bmdVideoConnectionSDI,		_T("SDI") },
	{ bmdVideoConnectionHDMI,		_T("HDMI") },
	{ bmdVideoConnectionOpticalSDI,	_T("Optical SDI") },
	{ bmdVideoConnectionComponent,	_T("Component") },
	{ bmdVideoConnectionComposite,	_T("Composite") },
	{ bmdVideoConnectionSVideo,		_T("S-Video") },
};

CCapturePreviewDlg::CCapturePreviewDlg(CWnd* pParent)
: CDialog(CCapturePreviewDlg::IDD, pParent), m_deckLinkDiscovery(NULL),
m_selectedDevice(NULL)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);	
}

void CCapturePreviewDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_INPUT_DEVICE_COMBO, m_deviceListCombo);
	DDX_Control(pDX, IDC_INPUT_CONNECTION_COMBO, m_inputConnectionCombo);
	DDX_Control(pDX, IDC_AUTODETECT_FORMAT_CHECK, m_applyDetectedInputModeCheckbox);
	DDX_Control(pDX, IDC_INPUT_MODE_COMBO, m_modeListCombo);
	DDX_Control(pDX, IDC_START_STOP_BUTTON, m_startStopButton);
	DDX_Control(pDX, IDC_INVALID_INPUT_STATIC, m_invalidInputLabel);

	DDX_Control(pDX, IDC_VITC_TC_F1_STATIC, m_vitcTcF1);
	DDX_Control(pDX, IDC_VITC_UB_F1_STATIC, m_vitcUbF1);
	DDX_Control(pDX, IDC_VITC_TC_F2_STATIC, m_vitcTcF2);
	DDX_Control(pDX, IDC_VITC_UB_F2__STATIC, m_vitcUbF2);

	DDX_Control(pDX, IDC_RP188_VITC1_TC_STATIC, m_rp188Vitc1Tc);
	DDX_Control(pDX, IDC_RP188_VITC1_UB_STATIC, m_rp188Vitc1Ub);
	DDX_Control(pDX, IDC_RP188_VITC2_TC_STATIC, m_rp188Vitc2Tc);
	DDX_Control(pDX, IDC_RP188_VITC2_TC__STATIC, m_rp188Vitc2Ub);
	DDX_Control(pDX, IDC_RP188_LTC_TC_STATIC, m_rp188LtcTc);
	DDX_Control(pDX, IDC_RP188_LTC_UB_STATIC, m_rp188LtcUb);
	DDX_Control(pDX, IDC_RP188_HFRTC_TC_STATIC, m_rp188HfrtcTc);
	DDX_Control(pDX, IDC_RP188_HFRTC_UB_STATIC, m_rp188HfrtcUb);

	DDX_Control(pDX, IDC_HDR_EOTF_STATIC, m_hdrEotf);
	DDX_Control(pDX, IDC_HDR_DP_RED_X_STATIC, m_hdrDpRedX);
	DDX_Control(pDX, IDC_HDR_DP_RED_Y_STATIC, m_hdrDpRedY);
	DDX_Control(pDX, IDC_HDR_DP_GREEN_X_STATIC, m_hdrDpGreenX);
	DDX_Control(pDX, IDC_HDR_DP_GREEN_Y_STATIC, m_hdrDpGreenY);
	DDX_Control(pDX, IDC_HDR_DP_BLUE_X_STATIC, m_hdrDpBlueX);
	DDX_Control(pDX, IDC_HDR_DP_BLUE_Y_STATIC, m_hdrDpBlueY);
	DDX_Control(pDX, IDC_HDR_WHITE_POINT_X_STATIC, m_hdrWhitePointX);
	DDX_Control(pDX, IDC_HDR_WHITE_POINT_Y_STATIC, m_hdrWhitePointY);
	DDX_Control(pDX, IDC_HDR_MAX_DML_STATIC, m_hdrMaxDml);
	DDX_Control(pDX, IDC_HDR_MIN_DML_STATIC, m_hdrMinDml);
	DDX_Control(pDX, IDC_HDR_MAX_CLL_STATIC, m_hdrMaxCll);
	DDX_Control(pDX, IDC_HDR_MAX_FALL_STATIC, m_hdrMaxFall);
	DDX_Control(pDX, IDC_COLORSPACE_STATIC, m_colorspace);

	DDX_Control(pDX, IDC_PREVIEW_BOX, m_previewBox);
}

BEGIN_MESSAGE_MAP(CCapturePreviewDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CLOSE()

	// UI element messages
	ON_BN_CLICKED(IDC_START_STOP_BUTTON, &CCapturePreviewDlg::OnStartStopBnClicked)
	ON_CBN_SELCHANGE(IDC_INPUT_DEVICE_COMBO, &CCapturePreviewDlg::OnNewDeviceSelected)
	ON_CBN_SELCHANGE(IDC_INPUT_CONNECTION_COMBO, &CCapturePreviewDlg::OnInputConnectionSelected)

	// Custom messages
	ON_MESSAGE(WM_REFRESH_INPUT_STREAM_DATA_MESSAGE, &CCapturePreviewDlg::OnRefreshInputStreamData)
	ON_MESSAGE(WM_DETECT_VIDEO_MODE_MESSAGE, &CCapturePreviewDlg::OnDetectVideoMode)
	ON_MESSAGE(WM_ADD_DEVICE_MESSAGE, &CCapturePreviewDlg::OnAddDevice)
	ON_MESSAGE(WM_REMOVE_DEVICE_MESSAGE, &CCapturePreviewDlg::OnRemoveDevice)
	ON_MESSAGE(WM_ERROR_RESTARTING_CAPTURE_MESSAGE, &CCapturePreviewDlg::OnErrorRestartingCapture)
	ON_MESSAGE(WM_UPDATE_PROFILE_MESSAGE, &CCapturePreviewDlg::OnProfileUpdate)
END_MESSAGE_MAP()

void CCapturePreviewDlg::OnStartStopBnClicked()
{
	if (m_selectedDevice == NULL)
		return;

	if (m_selectedDevice->IsCapturing())
		StopCapture();
	else
		StartCapture();
}

void CCapturePreviewDlg::OnNewDeviceSelected()
{
	int		selectedDeviceIndex;

	selectedDeviceIndex = m_deviceListCombo.GetCurSel();
	if (selectedDeviceIndex < 0)
		return;

	// Release profile callback from existing selected device
	if (m_selectedDevice != NULL)
	{
		IDeckLinkProfileManager* profileManager = m_selectedDevice->GetDeviceProfileManager();
		if (profileManager != NULL)
			profileManager->SetCallback(NULL);
	}

	m_selectedDevice = (DeckLinkDevice*)m_deviceListCombo.GetItemDataPtr(selectedDeviceIndex);

	if (m_selectedDevice != NULL)
	{
		IDeckLinkProfileManager*	profileManager	= NULL;
		LONGLONG					duplexMode		= bmdDuplexInactive;

		// Register profile callback with newly selected device's profile manager
		profileManager = m_selectedDevice->GetDeviceProfileManager();
		if (profileManager != NULL)
			profileManager->SetCallback(m_profileCallback);

		// Query duplex attribute to determine whether sub-device is active
		if ((m_selectedDevice->GetDeckLinkAttributes()->GetInt(BMDDeckLinkDuplex, &duplexMode) == S_OK) && 
			(duplexMode != bmdDuplexInactive))
		{
			// Sub-device is active - update the input video connections combo
			RefreshInputConnectionList();

			if (m_selectedDevice->SupportsFormatDetection())
				m_applyDetectedInputModeCheckbox.SetCheck(BST_CHECKED);

		}
		else
		{
			// Sub-device inactive, reset interface and disable start button
			m_inputConnectionCombo.ResetContent();
			m_modeListCombo.ResetContent();
			m_applyDetectedInputModeCheckbox.SetCheck(BST_UNCHECKED);
			m_startStopButton.EnableWindow(FALSE);
		}
	}
}


void CCapturePreviewDlg::OnInputConnectionSelected()
{
	int selectedConnectionIndex;

	selectedConnectionIndex = m_inputConnectionCombo.GetCurSel();
	if (selectedConnectionIndex < 0)
		return;

	m_selectedInputConnection = (BMDVideoConnection)m_inputConnectionCombo.GetItemData(selectedConnectionIndex);

	// Configure input connection for selected device
	if (m_selectedDevice->GetDeckLinkConfiguration()->SetInt(bmdDeckLinkConfigVideoInputConnection, (int64_t)m_selectedInputConnection) != S_OK)
		return;

	// Updated video mode combo for selected input connection
	RefreshVideoModeList();
}

void CCapturePreviewDlg::OnClose()
{
	// Stop the capture
	StopCapture();

	// Disable profile callback
	if ((m_selectedDevice != NULL) && (m_selectedDevice->GetDeviceProfileManager() != NULL))
	{
		m_selectedDevice->GetDeviceProfileManager()->SetCallback(NULL);
	}

	// Release all DeckLinkDevice instances
	while(m_deviceListCombo.GetCount() > 0)
	{
		DeckLinkDevice* device = (DeckLinkDevice*)m_deviceListCombo.GetItemDataPtr(0);
		device->Release();
		m_deviceListCombo.DeleteString(0);
	}

	if (m_profileCallback != NULL)
	{
		m_profileCallback->Release();
		m_profileCallback = NULL;
	}

	if (m_previewWindow != NULL)
	{
		m_previewWindow->Release();
		m_previewWindow = NULL;
	}

	// Release DeckLink discovery instance
	if (m_deckLinkDiscovery != NULL)
	{
		m_deckLinkDiscovery->Disable();
		m_deckLinkDiscovery->Release();
		m_deckLinkDiscovery = NULL;
	}

	CDialog::OnClose();
}


void CCapturePreviewDlg::ShowErrorMessage(TCHAR* msg, TCHAR* title)
{
	MessageBox(msg, title);
}

void CCapturePreviewDlg::RefreshInputConnectionList()
{
	IDeckLinkProfileAttributes*	deckLinkAttributes = NULL;
	LONGLONG					availableInputConnections;
	LONGLONG					currentInputConnection;
	int							index;

	m_inputConnectionCombo.ResetContent();

	// Get the available input video connections for the device
	if (m_selectedDevice->GetDeckLinkAttributes()->GetInt(BMDDeckLinkVideoInputConnections, &availableInputConnections) != S_OK)
		availableInputConnections = bmdVideoConnectionUnspecified;

	// Get the current selected input connection
	if (m_selectedDevice->GetDeckLinkConfiguration()->GetInt(bmdDeckLinkConfigVideoInputConnection, &currentInputConnection) != S_OK)
	{
		currentInputConnection = bmdVideoConnectionUnspecified;
	}

	for (auto connection : kInputConnections)
	{
		if ((connection.first & availableInputConnections) != 0)
		{
			// Input video connection is supported by device, add to combo
			index = m_inputConnectionCombo.AddString(connection.second);
			m_inputConnectionCombo.SetItemData(index, connection.first);

			// If input connection is the active connection set combo to this item
			if (connection.first == (BMDVideoConnection)currentInputConnection)
			{
				m_inputConnectionCombo.SetCurSel(index);
				OnInputConnectionSelected();
			}
		}
	}

	// If no input connection has been selected, select first index
	index = m_inputConnectionCombo.GetCurSel();
	if ((index == CB_ERR) && (m_inputConnectionCombo.GetCount() > 0))
	{
		m_inputConnectionCombo.SetCurSel(0);
		OnInputConnectionSelected();
	}
}

void CCapturePreviewDlg::RefreshVideoModeList()
{
	// Populate the display mode menu with a list of display modes supported by the installed DeckLink card
	IDeckLinkDisplayModeIterator*		displayModeIterator;
	IDeckLinkDisplayMode*				deckLinkDisplayMode;

	// Clear the menu
	m_modeListCombo.ResetContent();

	if (m_selectedDevice->GetDeckLinkInput()->GetDisplayModeIterator(&displayModeIterator) != S_OK)
		return;

	while (displayModeIterator->Next(&deckLinkDisplayMode) == S_OK)
	{
		BSTR	modeName;
		int		newIndex;
		HRESULT hr			= E_FAIL;
		BOOL	supported;

		// Check that display mode is supported with the active profile
		hr = m_selectedDevice->GetDeckLinkInput()->DoesSupportVideoMode(m_selectedInputConnection, deckLinkDisplayMode->GetDisplayMode(), bmdFormatUnspecified, bmdSupportedVideoModeDefault, &supported);
		if (hr != S_OK || !supported)
			continue;

		if (deckLinkDisplayMode->GetName(&modeName) != S_OK)
		{
			deckLinkDisplayMode->Release();
			deckLinkDisplayMode = NULL;
			continue;
		}

		// Add this item to the video format popup menu
		newIndex = m_modeListCombo.AddString(modeName);

		// Save the BMDDisplayMode in the menu item's tag
		m_modeListCombo.SetItemData(newIndex, deckLinkDisplayMode->GetDisplayMode());

		if (m_modeListCombo.GetCount() == 1)
		{
			// We have added our first item, refresh pixel format menu
			m_modeListCombo.SetCurSel(0);
		}

		deckLinkDisplayMode->Release();
		SysFreeString(modeName);
	}

	displayModeIterator->Release();
	displayModeIterator = NULL;

	m_startStopButton.EnableWindow(m_modeListCombo.GetCount() > 0);
}

void CCapturePreviewDlg::StartCapture()
{
	bool	applyDetectedInputMode = (m_applyDetectedInputModeCheckbox.GetCheck() == BST_CHECKED);
	int		selectedVideoFormatIndex = m_modeListCombo.GetCurSel();

	if (selectedVideoFormatIndex < 0)
		return;

	if (m_selectedDevice && 
		m_selectedDevice->StartCapture((BMDDisplayMode)m_modeListCombo.GetItemData(selectedVideoFormatIndex), m_previewWindow, applyDetectedInputMode))
	{
		// Update UI
		m_startStopButton.SetWindowText(_T("Stop capture"));
		EnableInterface(false);
	}
}

void CCapturePreviewDlg::StopCapture()
{
	if (m_selectedDevice)
		m_selectedDevice->StopCapture();

	// Update UI
	m_startStopButton.SetWindowText(_T("Start capture"));
	EnableInterface(true);
	m_invalidInputLabel.ShowWindow(SW_HIDE);
}

void CCapturePreviewDlg::EnableInterface(bool enabled)
{
	m_deviceListCombo.EnableWindow((enabled) ? TRUE : FALSE);
	m_inputConnectionCombo.EnableWindow((enabled) ? TRUE : FALSE);
	m_modeListCombo.EnableWindow((enabled) ? TRUE : FALSE);

	if (enabled)
	{
		if (m_selectedDevice && m_selectedDevice->SupportsFormatDetection())
		{
			m_applyDetectedInputModeCheckbox.EnableWindow(TRUE);
		}
		else
		{
			m_applyDetectedInputModeCheckbox.EnableWindow(FALSE);
			m_applyDetectedInputModeCheckbox.SetCheck(BST_UNCHECKED);
		}
	}
	else
		m_applyDetectedInputModeCheckbox.EnableWindow(FALSE);
}

BOOL	CCapturePreviewDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	SetIcon(m_hIcon, FALSE);

	// Empty popup menus
	m_deviceListCombo.ResetContent();
	m_modeListCombo.ResetContent();

	// Disable the interface
	m_startStopButton.EnableWindow(FALSE);
	EnableInterface(false);

	// Create and initialise preview, profile callback and DeckLink device discovery objects 
	m_previewWindow = new PreviewWindow();
	if (m_previewWindow->init(&m_previewBox) == false)
	{
		ShowErrorMessage(_T("This application was unable to initialise the preview window"), _T("Error"));
		goto bail;
	}

	m_profileCallback = new ProfileCallback(this);

	m_deckLinkDiscovery = new DeckLinkDeviceDiscovery(this);
	if (! m_deckLinkDiscovery->Enable())
		ShowErrorMessage(_T("Please install the Blackmagic Desktop Video drivers to use the features of this application."), _T("This application requires the Desktop Video drivers installed."));

bail:
	return TRUE;
}

void CCapturePreviewDlg::AddDevice(IDeckLink* deckLink)
{	
	int deviceIndex;
	DeckLinkDevice* newDevice = new DeckLinkDevice(this, deckLink);

	// Initialise new DeckLinkDevice object
	if (!newDevice->Init())
	{
		newDevice->Release();
		return;
	}

	// Add this DeckLink device to the device list
	deviceIndex = m_deviceListCombo.AddString(newDevice->GetDeviceName());
	if (deviceIndex < 0)
		return;

	m_deviceListCombo.SetItemDataPtr(deviceIndex, newDevice);

	if (m_deviceListCombo.GetCount() == 1)
	{
		// We have added our first item, refresh and enable UI
		m_deviceListCombo.SetCurSel(0);
		OnNewDeviceSelected();

		m_startStopButton.EnableWindow(TRUE);
		EnableInterface(true);
	}
}

void CCapturePreviewDlg::RemoveDevice(IDeckLink* deckLink)
{
	int deviceIndex = -1;
	DeckLinkDevice* deviceToRemove  = NULL;

	// Find the combo box entry to remove (there may be multiple entries with the same name, but each
	// will have a different data pointer).
	for (deviceIndex = 0; deviceIndex < m_deviceListCombo.GetCount(); ++deviceIndex)
	{
		deviceToRemove = (DeckLinkDevice*)m_deviceListCombo.GetItemDataPtr(deviceIndex);
		if (deviceToRemove->DeckLinkInstance() == deckLink)
			break;
	}

	if (deviceToRemove == NULL)
		return;

	// Stop capturing before removal
	if (deviceToRemove->IsCapturing())
		deviceToRemove->StopCapture();

	// Remove device from list
	m_deviceListCombo.DeleteString(deviceIndex);

	// Refresh UI
	m_startStopButton.SetWindowText(_T("Start capture"));

	// Check how many devices are left
	if (m_deviceListCombo.GetCount() == 0)
	{
		// We have removed the last device, disable the interface.
		m_startStopButton.EnableWindow(FALSE);
		EnableInterface(false);
		m_selectedDevice = NULL;
	}
	else if (m_selectedDevice == deviceToRemove)
	{
		// The device that was removed was the one selected in the UI.
		// Select the first available device in the list and reset the UI.
		m_deviceListCombo.SetCurSel(0);
		OnNewDeviceSelected();

		m_invalidInputLabel.ShowWindow(SW_HIDE);
	}

	// Release DeckLinkDevice instance
	deviceToRemove->Release();
}

void	CCapturePreviewDlg::UpdateFrameData(AncillaryDataStruct& ancillaryData, MetadataStruct& metadata)
{
	// Copy ancillary data under protection of critsec object
	m_critSec.Lock();
		m_ancillaryData = ancillaryData;
		m_metadata = metadata;
	m_critSec.Unlock();
}

void    CCapturePreviewDlg::HaltStreams()
{
	// Profile is changing, stop playback if running
	if (m_selectedDevice->IsCapturing())
		StopCapture();
}

LRESULT  CCapturePreviewDlg::OnRefreshInputStreamData(WPARAM wParam, LPARAM lParam)
{
	// Update the UI under protection of critsec object
	m_critSec.Lock();

	m_vitcTcF1.SetWindowText(m_ancillaryData.vitcF1Timecode);
	m_vitcUbF1.SetWindowText(m_ancillaryData.vitcF1UserBits);
	m_vitcTcF2.SetWindowText(m_ancillaryData.vitcF2Timecode);
	m_vitcUbF2.SetWindowText(m_ancillaryData.vitcF2UserBits);

	m_rp188Vitc1Tc.SetWindowText(m_ancillaryData.rp188vitc1Timecode);
	m_rp188Vitc1Ub.SetWindowText(m_ancillaryData.rp188vitc1UserBits);
	m_rp188Vitc2Tc.SetWindowText(m_ancillaryData.rp188vitc2Timecode);
	m_rp188Vitc2Ub.SetWindowText(m_ancillaryData.rp188vitc2UserBits);
	m_rp188LtcTc.SetWindowText(m_ancillaryData.rp188ltcTimecode);
	m_rp188LtcUb.SetWindowText(m_ancillaryData.rp188ltcUserBits);
	m_rp188HfrtcTc.SetWindowText(m_ancillaryData.rp188hfrtcTimecode);
	m_rp188HfrtcUb.SetWindowText(m_ancillaryData.rp188hfrtcUserBits);

	m_hdrEotf.SetWindowText(m_metadata.electroOpticalTransferFunction);
	m_hdrDpRedX.SetWindowText(m_metadata.displayPrimariesRedX);
	m_hdrDpRedY.SetWindowText(m_metadata.displayPrimariesRedY);
	m_hdrDpGreenX.SetWindowText(m_metadata.displayPrimariesGreenX);
	m_hdrDpGreenY.SetWindowText(m_metadata.displayPrimariesGreenY);
	m_hdrDpBlueX.SetWindowText(m_metadata.displayPrimariesBlueX);
	m_hdrDpBlueY.SetWindowText(m_metadata.displayPrimariesBlueY);
	m_hdrWhitePointX.SetWindowText(m_metadata.whitePointX);
	m_hdrWhitePointY.SetWindowText(m_metadata.whitePointY);
	m_hdrMaxDml.SetWindowText(m_metadata.maxDisplayMasteringLuminance);
	m_hdrMinDml.SetWindowText(m_metadata.minDisplayMasteringLuminance);
	m_hdrMaxCll.SetWindowText(m_metadata.maximumContentLightLevel);
	m_hdrMaxFall.SetWindowText(m_metadata.maximumFrameAverageLightLevel);  
	m_colorspace.SetWindowText(m_metadata.colorspace);
	
	m_critSec.Unlock();

	m_invalidInputLabel.ShowWindow((wParam) ? SW_SHOW : SW_HIDE);

	return 0;
}

LRESULT CCapturePreviewDlg::OnDetectVideoMode(WPARAM wParam, LPARAM lParam)
{
	BMDDisplayMode	detectedVideoMode;
	int				modeIndex;
	
	// A new video mode was auto-detected, update the video mode combo box
	detectedVideoMode = (BMDDisplayMode)lParam;
	
	for (modeIndex = 0; modeIndex < m_modeListCombo.GetCount(); modeIndex++)
	{
		if (m_modeListCombo.GetItemData(modeIndex) == detectedVideoMode)
		{
			m_modeListCombo.SetCurSel(modeIndex);
			break;
		}
	}

	return 0;
}


LRESULT CCapturePreviewDlg::OnAddDevice(WPARAM wParam, LPARAM lParam)
{
	// A new device has been connected
	AddDevice((IDeckLink*)wParam);
	return 0;
}

LRESULT	CCapturePreviewDlg::OnRemoveDevice(WPARAM wParam, LPARAM lParam)
{
	// An existing device has been disconnected
	RemoveDevice((IDeckLink*)wParam);
	return 0;
}

LRESULT	CCapturePreviewDlg::OnErrorRestartingCapture(WPARAM wParam, LPARAM lParam)
{
	// A change in the input video mode was detected, but the capture could not be restarted.
	StopCapture();
	ShowErrorMessage(_T("This application was unable to apply the detected input video mode."), _T("Error restarting the capture."));
	return 0;
}

LRESULT CCapturePreviewDlg::OnProfileUpdate(WPARAM wParam, LPARAM lParam)
{
	// Action as if new device selected to check whether device is active/inactive
	// This will subsequently update input connections and video modes combo boxes
	OnNewDeviceSelected();

	return 0;
}

void CCapturePreviewDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CCapturePreviewDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

