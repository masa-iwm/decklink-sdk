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
// HDRVideoFrame.h
//

#pragma once

#include <atomic>
#include "com_ptr.h"
#include "DeckLinkAPI.h"

struct ChromaticityCoordinates
{
	double RedX;
	double RedY;
	double GreenX;
	double GreenY;
	double BlueX;
	double BlueY;
	double WhiteX;
	double WhiteY;
};

struct HDRMetadata
{
	int64_t					EOTF;
	ChromaticityCoordinates	referencePrimaries;
	double					maxDisplayMasteringLuminance;
	double					minDisplayMasteringLuminance;
	double					maxCLL;
	double					maxFALL;
};

class HDRVideoFrame : public IDeckLinkVideoFrame, public IDeckLinkVideoFrameMetadataExtensions
{
public:
	HDRVideoFrame(com_ptr<IDeckLinkMutableVideoFrame> frame, const HDRMetadata& metadata);
	virtual ~HDRVideoFrame() {}
	
	// IUnknown interface
	virtual HRESULT			QueryInterface(REFIID iid, LPVOID *ppv);
	virtual ULONG			AddRef(void);
	virtual ULONG			Release(void);

	// IDeckLinkVideoFrame interface
	virtual long			GetWidth(void)			{ return m_videoFrame->GetWidth(); }
	virtual long			GetHeight(void)			{ return m_videoFrame->GetHeight(); }
	virtual long			GetRowBytes(void)		{ return m_videoFrame->GetRowBytes(); }
	virtual BMDPixelFormat	GetPixelFormat(void)	{ return m_videoFrame->GetPixelFormat(); }
	virtual BMDFrameFlags	GetFlags(void)			{ return m_videoFrame->GetFlags() | bmdFrameContainsHDRMetadata; }
	virtual HRESULT			GetBytes(void **buffer)	{ return m_videoFrame->GetBytes(buffer); }
	virtual HRESULT			GetTimecode(BMDTimecodeFormat format, IDeckLinkTimecode **timecode)	{ return m_videoFrame->GetTimecode(format, timecode); }
	virtual HRESULT			GetAncillaryData(IDeckLinkVideoFrameAncillary **ancillary)			{ return m_videoFrame->GetAncillaryData(ancillary); }

	// IDeckLinkVideoFrameMetadataExtensions interface
	virtual HRESULT			GetInt(BMDDeckLinkFrameMetadataID metadataID, int64_t *value);
	virtual HRESULT			GetFloat(BMDDeckLinkFrameMetadataID metadataID, double* value);
	virtual HRESULT			GetFlag(BMDDeckLinkFrameMetadataID metadataID, bool* value);
	virtual HRESULT			GetString(BMDDeckLinkFrameMetadataID metadataID, CFStringRef* value);

	void UpdateHDRMetadata(const HDRMetadata& metadata) { m_metadata = metadata; }
	
private:
	com_ptr<IDeckLinkMutableVideoFrame>	m_videoFrame;
	HDRMetadata							m_metadata;
	std::atomic<ULONG>					m_refCount;
};
