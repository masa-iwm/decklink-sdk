/* -LICENSE-START-
 ** Copyright (c) 2016 Blackmagic Design
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
#include "DeckLinkAPI.h"
#include <cstring>
#include <cstdio>

// The DeckLinkAPI enables the insertion of arbitrary data into the vertical blanking of the SDI. This sample implements
// basic encoding of caption data using the CEA 708 spec, before passing that data to the DeckLinkAPI. Other caption
// encodings used by receiving equipment, or more advanced usage of CEA-708, are out of the scope of this sample.
#include "CEA708_Encoder.h"

// Video mode parameters
const BMDDisplayMode      kDisplayMode = bmdModeHD1080i50;
const BMDVideoOutputFlags kOutputFlag  = bmdVideoOutputVANC;
const BMDPixelFormat      kPixelFormat = bmdFormat10BitYUV;

// Frame parameters
const uint32_t kFrameDuration = 1000;
const uint32_t kTimeScale = 25000;
const uint32_t kFrameWidth = 1920;
const uint32_t kFrameHeight = 1080;

// 10-bit YUV row bytes, ref. SDK Manual "2.7.4 Pixel Formats" / bmdFormat10BitYUV
const uint32_t kRowBytes = ((kFrameWidth + 47) / 48) * 128;

// 10-bit YUV colour pixels
const uint32_t kBlueData[] = { 0x40aa298, 0x2a8a62a8, 0x298aa040, 0x2a8102a8 };

// IDs for CEA-708 captions from ITU-R BT.1364-3
const uint8_t kCaptionDistributionPacketDID = 0x61;
const uint8_t kCaptionDistributionPacketSDID = 0x1;

// Keep track of the number of scheduled frames
uint32_t gTotalFramesScheduled = 0;

static void FillBlue(IDeckLinkMutableVideoFrame* theFrame)
{
	uint32_t* nextWord;
	uint32_t  wordsRemaining;
	
	theFrame->GetBytes((void**)&nextWord);
	wordsRemaining = (kRowBytes * kFrameHeight) / 4;
	
	while (wordsRemaining > 0)
	{
		*(nextWord++) = kBlueData[0];
		*(nextWord++) = kBlueData[1];
		*(nextWord++) = kBlueData[2];
		*(nextWord++) = kBlueData[3];
		wordsRemaining = wordsRemaining - 4;
	}
}

class CaptionAncillaryPacket: public IDeckLinkAncillaryPacket
{
public:
	CaptionAncillaryPacket(const CEA708::EncodedCaptionDistributionPacket& userData)
	{
		m_refCount = 1;
		m_userData = userData;
	}
	
	// IDeckLinkAncillaryPacket
	HRESULT STDMETHODCALLTYPE GetBytes(BMDAncillaryPacketFormat format, const void** data, uint32_t* size)
	{
		if (format != bmdAncillaryPacketFormatUInt8)
		{
			// In this example we only implement our packets with 8-bit data. This is fine because DeckLink accepts
			// whatever format we can supply and, if required, converts it.
			return E_NOTIMPL;
		}
		if (size) // Optional
			*size = (uint32_t)m_userData.size();
		if (data) // Optional
			*data = m_userData.data();
		return S_OK;
	}
	
	// IDeckLinkAncillaryPacket
	uint8_t STDMETHODCALLTYPE GetDID(void)
	{
		return kCaptionDistributionPacketDID;
	}
	
	// IDeckLinkAncillaryPacket
	uint8_t STDMETHODCALLTYPE GetSDID(void)
	{
		return kCaptionDistributionPacketSDID;
	}
	
	// IDeckLinkAncillaryPacket
	uint32_t STDMETHODCALLTYPE GetLineNumber(void)
	{
		// Returning zero here tells DeckLink to attempt to assume the line for known DIDs/SDIDs, or otherwise place the
		// packet on the initial VANC lines of a frame
		return 0;
	}
	
	// IDeckLinkAncillaryPacket
	uint8_t STDMETHODCALLTYPE GetDataStreamIndex(void)
	{
		return 0;
	}
	
	// IUnknown
	HRESULT	STDMETHODCALLTYPE QueryInterface (REFIID iid, LPVOID *ppv)
	{
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	
	// IUnknown
	ULONG STDMETHODCALLTYPE AddRef()
	{
		// gcc atomic operation builtin
		return __sync_add_and_fetch(&m_refCount, 1);
	}
	
	// IUnknown
	ULONG STDMETHODCALLTYPE Release()
	{
		// gcc atomic operation builtin
		int32_t newRefValue = __sync_sub_and_fetch(&m_refCount, 1);
		
		if (newRefValue == 0)
			delete this;
		
		return newRefValue;
	}

private:
	int32_t m_refCount;
	CEA708::EncodedCaptionDistributionPacket m_userData;
};

class OutputCallback: public IDeckLinkVideoOutputCallback
{
private:
	int32_t				m_refCount;
	CEA708::Encoder		m_CC708Encoder;
	IDeckLinkOutput*	m_deckLinkOutput;
public:
	OutputCallback(IDeckLinkOutput* deckLinkOutput)
	: m_refCount(1), m_CC708Encoder(kFrameDuration, kTimeScale), m_deckLinkOutput(deckLinkOutput)
	{
		m_deckLinkOutput->AddRef();
	}
	virtual ~OutputCallback(void)
	{
		m_deckLinkOutput->Release();
	}
	
	HRESULT ScheduleNextFrame(IDeckLinkVideoFrame* videoFrame)
	{
		HRESULT										result = S_OK;
		IDeckLinkVideoFrameAncillaryPackets*		frameAncillaryPackets = NULL;
		IDeckLinkAncillaryPacket*					ancillaryPacket = NULL;
		CEA708::EncodedCaptionDistributionPacket	packet;
		
		// Resend the given caption data every second.
		unsigned fps = kTimeScale / kFrameDuration;
		if (gTotalFramesScheduled % fps == 0)
		{
			using namespace CEA708;
			Encoder& cc = m_CC708Encoder;
			
			// 8.11 Proper Order of Data
			cc << DeleteWindows();
			cc << DefineWindow(window_0, priority_Highest, anchor_BottomCenter, false, 27, 44, 2, 22, true, true, true, windowStyle_NTSCPopup, penStyle_NTSCProportionalSans);
			cc << SetWindowAttributes(justify_Left, printDirection_LeftToRight, scrollDirection_BottomToTop, false, displayEffect_Snap, effectDirection_LeftToRight, 0, colour_Black, opacity_Translucent, borderType_None, colour_Black);
			cc << SetPenLocation(0, 0) << "\r";
			
			cc << SetPenAttributes(penSize_Standard, font_ProportionalSans, textTag_Dialog, textOffset_Normal, false, false, edgeType_None);
			cc << SetPenLocation(0, 0) << "CEA-708 Closed Captions";
			cc << SetPenLocation(1, 0) << "Second line of text!";
			cc << EndOfText();
			
			cc << DisplayWindows(1 << window_0);
			cc.flush();
		}
		
		if (m_CC708Encoder.empty())
		{
			// If there is no caption data to be sent on this frame, generate a pad packet which includes the
			// headers which describe the available caption services.
			// Calling flush with an empty buffer will achieve this, by encoding a null service block.
			m_CC708Encoder.flush();
		}
		
		if (m_CC708Encoder.pop(&packet))
		{
		
			result = videoFrame->QueryInterface(IID_IDeckLinkVideoFrameAncillaryPackets, (void**)&frameAncillaryPackets);
			if (result != S_OK)
			{
				fprintf(stderr, "Could not get ancillary packet store = %08x\n", result);
				goto bail;
			}

			// DeckLink won't remove any pre-existing packets from a frame or replace those with a similar DID/SDID
			// We're recycling our frames via ScheduledFrameCompleted(), so ensure the last captions we added are removed
			result = frameAncillaryPackets->GetFirstPacketByID(kCaptionDistributionPacketDID, kCaptionDistributionPacketSDID, &ancillaryPacket);
			if (result == S_OK)
			{
				frameAncillaryPackets->DetachPacket(ancillaryPacket);
				ancillaryPacket->Release();
				ancillaryPacket = NULL;
			}
			
			result = frameAncillaryPackets->AttachPacket(new CaptionAncillaryPacket(packet));
			if (result != S_OK)
			{
				fprintf(stderr, "Could not attach packet = %08x\n", result);
				goto bail;
			}

		}

		// When a video frame completes,reschedule another frame
		result = m_deckLinkOutput->ScheduleVideoFrame(videoFrame, gTotalFramesScheduled*kFrameDuration, kFrameDuration, kTimeScale);
		if (result != S_OK)
		{
			fprintf(stderr, "Could not schedule video frame = %08x\n", result);
			goto bail;
		}
		
		gTotalFramesScheduled++;
		
	bail:
		
		if (frameAncillaryPackets != NULL)
		{
			frameAncillaryPackets->Release();
			frameAncillaryPackets = NULL;
		}
		
		return result;
	}
	
	HRESULT	STDMETHODCALLTYPE ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult)
	{
		HRESULT result = ScheduleNextFrame(completedFrame);
		
		if (result != S_OK)
		{
			completedFrame->Release();
			completedFrame = NULL;
		}
		
		return result;
	}
	
	HRESULT	STDMETHODCALLTYPE ScheduledPlaybackHasStopped(void)
	{
		return S_OK;
	}
	
	// IUnknown
	HRESULT	STDMETHODCALLTYPE QueryInterface (REFIID iid, LPVOID *ppv)
	{
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	
	ULONG STDMETHODCALLTYPE AddRef()
	{
		// gcc atomic operation builtin
		return __sync_add_and_fetch(&m_refCount, 1);
	}
	
	ULONG STDMETHODCALLTYPE Release()
	{
		// gcc atomic operation builtin
		int32_t newRefValue = __sync_sub_and_fetch(&m_refCount, 1);
		
		if (newRefValue == 0)
			delete this;
		
		return newRefValue;
	}
};

static IDeckLinkMutableVideoFrame* CreateFrame(IDeckLinkOutput* deckLinkOutput)
{
	HRESULT                         result;
	IDeckLinkMutableVideoFrame*     frame = NULL;
	
	result = deckLinkOutput->CreateVideoFrame(kFrameWidth, kFrameHeight, kRowBytes, kPixelFormat, bmdFrameFlagDefault, &frame);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not create a video frame - result = %08x\n", result);
		goto bail;
	}
	
	FillBlue(frame);
	
bail:
	
	return frame;
}

HRESULT GetDeckLinkIterator(IDeckLinkIterator **deckLinkIterator)
{
	HRESULT result = S_OK;
	
	// Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
	*deckLinkIterator = CreateDeckLinkIteratorInstance();
	if (*deckLinkIterator == NULL)
	{
		fprintf(stderr, "A DeckLink iterator could not be created.  The DeckLink drivers may not be installed.\n");
		result = E_FAIL;
	}
	
	return result;
}

int main()
{
	IDeckLinkIterator*      deckLinkIterator = NULL;
	IDeckLink*              deckLink         = NULL;
	IDeckLinkOutput*        deckLinkOutput   = NULL;
	OutputCallback*         outputCallback   = NULL;
	HRESULT                 result;
	
	// Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
	result = GetDeckLinkIterator(&deckLinkIterator);
	if (result != S_OK)
	{
		fprintf(stderr, "A DeckLink iterator could not be created.  The DeckLink drivers may not be installed.\n");
		goto bail;
	}
	
	// Obtain the first DeckLink device
	result = deckLinkIterator->Next(&deckLink);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not find DeckLink device - result = %08x\n", result);
		goto bail;
	}
	
	// Obtain the output interface for the DeckLink device
	result = deckLink->QueryInterface(IID_IDeckLinkOutput, (void**)&deckLinkOutput);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not obtain the IDeckLinkOutput interface - result = %08x\n", result);
		goto bail;
	}
	
	// Create an instance of output callback
	outputCallback = new OutputCallback(deckLinkOutput);
	if (outputCallback == NULL)
	{
		fprintf(stderr, "Could not create output callback object\n");
		goto bail;
	}
	
	// Set the callback object to the DeckLink device's output interface
	result = deckLinkOutput->SetScheduledFrameCompletionCallback(outputCallback);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not set callback - result = %08x\n", result);
		goto bail;
	}
	
	// Enable video output
	result = deckLinkOutput->EnableVideoOutput(kDisplayMode, kOutputFlag);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not enable video output - result = %08x\n", result);
		goto bail;
	}
	
	// Schedule a blue frame 3 times
	for (int i = 0; i < 3; i++)
	{
		IDeckLinkVideoFrame*    videoFrameBlue   = NULL;
		
		// Create a frame with defined format
		videoFrameBlue = CreateFrame(deckLinkOutput);
		if (!videoFrameBlue)
			goto bail;
		
		result = outputCallback->ScheduleNextFrame(videoFrameBlue);
		if (result != S_OK)
		{
			videoFrameBlue->Release();
			
			fprintf(stderr, "Could not schedule video frame - result = %08x\n", result);
			goto bail;
		}
		gTotalFramesScheduled ++;
	}
	
	// Start
	result = deckLinkOutput->StartScheduledPlayback(0, kTimeScale, 1.0);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not start - result = %08x\n", result);
		goto bail;
	}
	
	// Wait until user presses Enter
	printf("Playing... Press <RETURN> to exit\n");
	
	getchar();
	
	printf("Exiting.\n");
	
	// Stop playback
	result = deckLinkOutput->StopScheduledPlayback(0, NULL, 0);
	result = deckLinkOutput->SetScheduledFrameCompletionCallback(NULL);
	
	// Disable the video output interface
	result = deckLinkOutput->DisableVideoOutput();
	
	// Release resources
bail:
	
	// Release the video output interface
	if (deckLinkOutput != NULL)
	{
		deckLinkOutput->Release();
		deckLinkOutput = NULL;
	}
	
	// Release the Decklink object
	if (deckLink != NULL)
	{
		deckLink->Release();
		deckLink = NULL;
	}
	
	// Release the outputCallback callback object
	if (outputCallback != NULL)
		delete outputCallback;
	
	// Release the DeckLink iterator
	if (deckLinkIterator != NULL)
	{
		deckLinkIterator->Release();
		deckLinkIterator = NULL;
	}
	
	return(result == S_OK) ? 0 : 1;
}

