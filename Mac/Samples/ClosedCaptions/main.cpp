/* -LICENSE-START-
 ** Copyright (c) 2016 Blackmagic Design
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
#include "DeckLinkAPI.h"
#include <cstring>
#include <cstdio>
#include <libkern/OSAtomic.h>

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
const uint32_t kBlackData[] = { 0x20010200, 0x04080040 };

// Data Identifier 
const uint8_t kCaptionDistributionPacketDID = 0x61;

// Secondary Data Identifier
const uint8_t kCaptionDistributionPacketSDID = 0x1;

// Define VANC line for CEA-708 captions
const uint32_t kCaptionDistributionPacketLine = 15;

// Keep track of the number of scheduled frames
uint32_t gTotalFramesScheduled = 0;

//  This function translates a byte into a 10-bit sample
//   x x x x x x x x x x x x
//       -------------------
//   | | |  0-7 raw data   |
//   | |
//   | even parity bit
//   inverse of bit 8
static inline uint32_t EncodeByte(uint32_t byte)
{
	uint32_t temp = byte;
	// Calculate the even parity bit of bits 0-7 by XOR every individual bits
	temp ^= temp >> 4;
	temp ^= temp >> 2;
	temp ^= temp >> 1;
	// Use lsb as parity bit
	temp &= 1;
	// Put even parity bit on bit 8
	byte |= temp << 8;
	// Bit 9 is inverse of bit 8
	byte |= ((~temp) & 1) << 9;
	return byte;
}

// This function writes 10bit ancillary data to 10bit luma value in YUV 10bit structure
static void WriteAncDataToLuma(uint32_t*& sdiStreamPosition, uint32_t value, uint32_t dataPosition)
{
	switch (dataPosition % 3)
	{
		case 0:
			*sdiStreamPosition++  = (value) << 10;
			break;
		case 1:
			*sdiStreamPosition = (value);
			break;
		case 2:
			*sdiStreamPosition++ |= (value) << 20;
			break;
		default:
			break;
	}
}

static void WriteAncillaryDataPacket(uint32_t* line, const uint8_t did, const uint8_t sdid, const uint8_t* data, uint32_t length)
{
	// Sanity check
	if (length == 0 || length > 255)
		return;
	
	const uint32_t encodedDID  = EncodeByte(did);
	const uint32_t encodedSDID = EncodeByte(sdid);
	const uint32_t encodedDC   = EncodeByte(length);
	
	// Start sequence
	*line++ = 0;
	*line++ = 0x3ff003ff;
	
	// DID
	*line++ = encodedDID << 10;
	
	// SDID and DC
	*line++ = encodedSDID | (encodedDC << 20);
	
	// Checksum does not include the start sequence
	uint32_t sum = encodedDID + encodedSDID + encodedDC;
	// Write the payload
	for (uint32_t i = 0; i < length; ++i)
	{
		const uint32_t encoded = EncodeByte(data[i]);
		WriteAncDataToLuma(line, encoded, i);
		sum += encoded & 0x1ff;
	}
	
	// Checksum % 512 then copy inverse of bit 8 to bit 9
	sum &= 0x1ff;
	sum |= ((~(sum << 1)) & 0x200);
	WriteAncDataToLuma(line, sum, length);
}

static void SetVancData(IDeckLinkVideoFrameAncillary* ancillary, const uint8_t* data, uint32_t length)
{
	HRESULT   result;
	uint32_t* buffer;
	
	result = ancillary->GetBufferForVerticalBlankingLine(kCaptionDistributionPacketLine, (void **)&buffer);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not get buffer for Vertical blanking line - result = %08x\n", result);
		return;
	}
	// Write caption data to buffer
	WriteAncillaryDataPacket(buffer, kCaptionDistributionPacketDID, kCaptionDistributionPacketSDID, data, length);
}

static void ClearVancData(IDeckLinkVideoFrameAncillary* ancillary)
{
	HRESULT   result;
	uint32_t* nextWord;
	uint32_t  wordsRemaining;
	
	result = ancillary->GetBufferForVerticalBlankingLine(kCaptionDistributionPacketLine, (void **)&nextWord);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not get buffer for Vertical blanking line - result = %08x\n", result);
		return;
	}
	
	wordsRemaining = kRowBytes / 4;
	
	while (wordsRemaining > 0)
	{
		*(nextWord++) = kBlackData[0];
		*(nextWord++) = kBlackData[1];
		wordsRemaining = wordsRemaining - 2;
	}
}

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
		IDeckLinkVideoFrameAncillary*				ancillaryData = NULL;
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
		
		result = videoFrame->GetAncillaryData(&ancillaryData);
		if (result != S_OK)
		{
			fprintf(stderr, "Could not get ancillary data = %08x\n", result);
			goto bail;
		}

		ClearVancData(ancillaryData);
		
		if (m_CC708Encoder.pop(&packet))
		{
			SetVancData(ancillaryData, packet.data(), static_cast<uint32_t>(packet.size()));
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
		
		if (ancillaryData != NULL)
		{
			ancillaryData->Release();
			ancillaryData = NULL;
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
		return OSAtomicIncrement32(&m_refCount);
	}
	
	ULONG STDMETHODCALLTYPE Release()
	{
		int32_t newRefValue = OSAtomicDecrement32(&m_refCount);
		
		if (newRefValue == 0)
			delete this;
		
		return newRefValue;
	}
};

static IDeckLinkMutableVideoFrame* CreateFrame(IDeckLinkOutput* deckLinkOutput)
{
	HRESULT                         result;
	IDeckLinkMutableVideoFrame*     frame = NULL;
	IDeckLinkVideoFrameAncillary*	ancillaryData = NULL;
	
	result = deckLinkOutput->CreateVideoFrame(kFrameWidth, kFrameHeight, kRowBytes, kPixelFormat, bmdFrameFlagDefault, &frame);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not create a video frame - result = %08x\n", result);
		goto bail;
	}
	
	FillBlue(frame);
	
	result = deckLinkOutput->CreateAncillaryData(kPixelFormat, &ancillaryData);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not create Ancillary data - result = %08x\n", result);
		goto bail;
	}

	// Ancillary data filled in callback
	result = frame->SetAncillaryData(ancillaryData);
	if (result != S_OK)
	{
		fprintf(stderr, "Fail to set ancillary data to the frame - result = %08x\n", result);
		goto bail;
	}
	
bail:
	// Release the Ancillary object
	if (ancillaryData != NULL)
		ancillaryData->Release();
	
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

