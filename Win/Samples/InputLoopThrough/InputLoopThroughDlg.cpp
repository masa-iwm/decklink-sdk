/* -LICENSE-START-
** Copyright (c) 2009 Blackmagic Design
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
#include "stdafx.h"
#include "InputLoopThrough.h"
#include "InputLoopThroughDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CInputLoopThroughDlg dialog



CInputLoopThroughDlg::CInputLoopThroughDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CInputLoopThroughDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pDelegate = new CVideoDelegate(this);
}

CInputLoopThroughDlg::~CInputLoopThroughDlg()
{
	delete m_pDelegate;
}

void CInputLoopThroughDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_INPUT_CARD_COMBO, m_InputCardCombo);
	DDX_Control(pDX, IDC_OUTPUT_CARD_COMBO, m_OutputCardCombo);
	DDX_Control(pDX, IDC_VIDEO_FORMAT_COMBO, m_VideoFormatCombo);
	DDX_Control(pDX, IDC_CAPTURE_TIME_LABEL, m_CaptureTimeLabel);
	DDX_Control(pDX, IDC_CAPTURE_TIME, m_CaptureTime);
	DDX_Control(pDX, IDC_START_BUTTON, m_StartButton);
}

BEGIN_MESSAGE_MAP(CInputLoopThroughDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_START_BUTTON, OnBnClickedStartButton)
	ON_CBN_SELCHANGE(IDC_VIDEO_FORMAT_COMBO, OnCbnSelchangeVideoFormatCombo)
END_MESSAGE_MAP()


// CInputLoopThroughDlg message handlers

BOOL CInputLoopThroughDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

		// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// Setup video format combo
	m_VideoFormatCombo.SetItemData(m_VideoFormatCombo.AddString("525i59.94 NTSC"),bmdModeNTSC);
	m_VideoFormatCombo.SetItemData(m_VideoFormatCombo.AddString("625i50 PAL"),bmdModePAL);
	m_VideoFormatCombo.SetItemData(m_VideoFormatCombo.AddString("1080PsF23.98"),bmdModeHD1080p2398);
	m_VideoFormatCombo.SetItemData(m_VideoFormatCombo.AddString("1080PsF24"),bmdModeHD1080p24);
	m_VideoFormatCombo.SetItemData(m_VideoFormatCombo.AddString("1080i50"),bmdModeHD1080i50);
	m_VideoFormatCombo.SetItemData(m_VideoFormatCombo.AddString("1080i59.94"),bmdModeHD1080i5994);
	m_VideoFormatCombo.SetItemData(m_VideoFormatCombo.AddString("720p50"),bmdModeHD720p50);
	m_VideoFormatCombo.SetItemData(m_VideoFormatCombo.AddString("720p59.94"),bmdModeHD720p5994);
	m_VideoFormatCombo.SetItemData(m_VideoFormatCombo.AddString("720p60"),bmdModeHD720p60);
	m_VideoFormatCombo.SetItemData(m_VideoFormatCombo.AddString("2Kp23.98"),bmdMode2k2398);
	m_VideoFormatCombo.SetItemData(m_VideoFormatCombo.AddString("2Kp24"),bmdMode2k24);
	m_VideoFormatCombo.SetCurSel(0);

	CComPtr<IDeckLinkIterator>	pIterator = NULL;
	int							i = 0;
	CComBSTR					cardNameBSTR;
	CString						cardName;

	if (CoCreateInstance(CLSID_CDeckLinkIterator,NULL,CLSCTX_ALL,IID_IDeckLinkIterator,(void**)&pIterator) == S_OK && pIterator)
	{
		for (;i<MAX_DECKLINK;i++)
		{
			if (pIterator->Next(&m_pDeckLink[i]) != S_OK)
				break;
			
			// Add this deckLink instance to the popup menus
			
			if (m_pDeckLink[i]->GetDisplayName(&cardNameBSTR) != S_OK)
				cardNameBSTR = L"Unknown DeckLink";

			cardName = cardNameBSTR;
			
			m_InputCardCombo.SetItemDataPtr(m_InputCardCombo.AddString(cardName),m_pDeckLink[i]);
			m_OutputCardCombo.SetItemDataPtr(m_OutputCardCombo.AddString(cardName),m_pDeckLink[i]);
		}
	}

	if (!i)
	{
		m_InputCardCombo.AddString("No DeckLink Cards Found");
		m_OutputCardCombo.AddString("No DeckLink Cards Found");
		m_StartButton.EnableWindow(FALSE);
	}

	m_InputCardCombo.SetCurSel(0);
	m_OutputCardCombo.SetCurSel(0);
	
	// Hide the timecode
	m_CaptureTimeLabel.ShowWindow(SW_HIDE);
	m_CaptureTime.ShowWindow(SW_HIDE);
	
	m_bRunning = FALSE;

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CInputLoopThroughDlg::OnPaint() 
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
HCURSOR CInputLoopThroughDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CInputLoopThroughDlg::OnBnClickedStartButton()
{
		HRESULT		theResult;

	if (!m_bRunning)
	{
		// Obtain the input and output interfaces
		if (m_pDeckLink[m_InputCardCombo.GetCurSel()]->QueryInterface(IID_IDeckLinkInput, (void**)&m_pInputCard) != S_OK)
			goto bail;
		if (m_pDeckLink[m_OutputCardCombo.GetCurSel()]->QueryInterface(IID_IDeckLinkOutput, (void**)&m_pOutputCard) != S_OK)
		{
			m_pInputCard->Release();
			goto bail;
		}
		
		BMDDisplayMode displayMode = (BMDDisplayMode)m_VideoFormatCombo.GetItemData(m_VideoFormatCombo.GetCurSel());

		// Turn on video input
		theResult = m_pInputCard->SetCallback(m_pDelegate);
		if (theResult != S_OK)
			TRACE("SetDelegate failed with result 0x%08x\n", theResult);
		//
		theResult = m_pInputCard->EnableVideoInput(displayMode, bmdFormat8BitYUV, 0);
		if (theResult != S_OK)
			TRACE("EnableVideoInput failed with result 0x%08x\n", theResult);

		// Turn on video output
		theResult = m_pOutputCard->EnableVideoOutput(displayMode, bmdVideoOutputFlagDefault);
		if (theResult != S_OK)
			TRACE("EnableVideoOutput failed with result 0x%08x\n", theResult);
		// 
		theResult = m_pOutputCard->StartScheduledPlayback(0, 600, 1.0);
		if (theResult != S_OK)
			TRACE("StartScheduledPlayback failed with result 0x%08x\n", theResult);
		
		// Sart the input stream running
		theResult = m_pInputCard->StartStreams();
		if (theResult != S_OK)
			TRACE("Input StartStreams failed with result %08x\n", theResult);
		
		m_bRunning = TRUE;
		m_CaptureTimeLabel.ShowWindow(SW_SHOW);
		m_CaptureTime.ShowWindow(SW_SHOW);
		m_CaptureTime.SetWindowText("");
		m_InputCardCombo.EnableWindow(FALSE);
		m_OutputCardCombo.EnableWindow(FALSE);
		m_StartButton.SetWindowText("Stop");
	}
	else
	{
		m_bRunning = FALSE;
		
		m_pInputCard->StopStreams();
		m_pOutputCard->StopScheduledPlayback(0, NULL, 600);
		
		m_pOutputCard->DisableVideoOutput();
		m_pInputCard->DisableVideoInput();

		m_CaptureTimeLabel.ShowWindow(SW_HIDE);
		m_CaptureTime.ShowWindow(SW_HIDE);
		m_InputCardCombo.EnableWindow(TRUE);
		m_OutputCardCombo.EnableWindow(TRUE);
		m_StartButton.SetWindowText("Start");
	}
	
bail:
	return;
}

void CInputLoopThroughDlg::OnCbnSelchangeVideoFormatCombo()
{
	if (m_bRunning)
	{
		BMDDisplayMode displayMode = (BMDDisplayMode)m_VideoFormatCombo.GetItemData(m_VideoFormatCombo.GetCurSel());

		m_pOutputCard->StopScheduledPlayback(0, NULL, 600);
		m_pOutputCard->DisableVideoOutput();
		
		m_pInputCard->StopStreams();
		m_pInputCard->EnableVideoInput(displayMode, bmdFormat8BitYUV, 0);
		m_pOutputCard->EnableVideoOutput(displayMode, bmdVideoOutputFlagDefault);
		m_pOutputCard->StartScheduledPlayback(0, 600, 1.0);
		m_pInputCard->StartStreams();
	}
}

		/// <summary> CVideoDelegate Constructor ! </summary>

CVideoDelegate::CVideoDelegate(CInputLoopThroughDlg* pController)
{
	m_pController = pController;
	m_RefCount = 1;
}

HRESULT	STDMETHODCALLTYPE CVideoDelegate::QueryInterface(REFIID iid, LPVOID *ppv)
{
	HRESULT			result = E_NOINTERFACE;
	
	// Initialise the return result
	*ppv = NULL;
	
	// Obtain the IUnknown interface and compare it the provided REFIID
	if (iid == IID_IUnknown)
	{
		*ppv = this;
		AddRef();
		result = S_OK;
	}
	else if (iid == IID_IDeckLinkInputCallback)
	{
		*ppv = (IDeckLinkInputCallback*)this;
		AddRef();
		result = S_OK;
	}
	
	return result;
}

ULONG STDMETHODCALLTYPE CVideoDelegate::AddRef(void)
{
	return InterlockedIncrement((LONG*)&m_RefCount);
}

ULONG STDMETHODCALLTYPE CVideoDelegate::Release(void)
{
	int		newRefValue;
	
	newRefValue = InterlockedDecrement((LONG*)&m_RefCount);
	if (newRefValue == 0)
	{
		delete this;
		return 0;
	}
	
	return newRefValue;
}

HRESULT STDMETHODCALLTYPE CVideoDelegate::VideoInputFormatChanged(BMDVideoInputFormatChangedEvents notificationEvents, IDeckLinkDisplayMode* newDisplayMode, BMDDetectedVideoInputFormatFlags detectedSignalFlags)
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CVideoDelegate::VideoInputFrameArrived(IDeckLinkVideoInputFrame* pArrivedFrame, IDeckLinkAudioInputPacket*)
{
	if (m_pController->m_bRunning)
	{
		BMDTimeValue	frameTime, frameDuration;
		int				hours, minutes, seconds, frames;
		HRESULT			theResult;
		CString			captureString;

		pArrivedFrame->GetStreamTime(&frameTime, &frameDuration, 600);
		theResult = m_pController->m_pOutputCard->ScheduleVideoFrame(pArrivedFrame, frameTime, frameDuration, 600);
		if (theResult != S_OK)
			TRACE("Scheduling failed with error 0x%08x\n", theResult);
		
		hours = (int)(frameTime / (600 * 60*60));
		minutes = (int)((frameTime / (600 * 60)) % 60);
		seconds = (int)((frameTime / 600) % 60);
		frames = (int)((frameTime / 6) % 100);

		captureString.Format("%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
		m_pController->m_CaptureTime.SetWindowText(captureString);
	}
	
	return S_OK;
}
