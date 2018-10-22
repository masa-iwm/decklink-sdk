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

#include <atomic>
#include <vector>
#include "DeckLinkAPI.h"

class Bgra32VideoFrame : public IDeckLinkVideoFrame
{
private:
	long					m_width;
	long					m_height;
	BMDFrameFlags			m_flags;
	std::vector<uint8_t>	m_pixelBuffer;

	std::atomic<uint32_t>	m_refCount;

public:
	Bgra32VideoFrame(long width, long height, BMDFrameFlags flags);
	virtual ~Bgra32VideoFrame() {};

	// IDeckLinkVideoFrame interface
	virtual long			STDMETHODCALLTYPE	GetWidth(void)			{ return m_width; };
	virtual long			STDMETHODCALLTYPE	GetHeight(void)			{ return m_height; };
	virtual long			STDMETHODCALLTYPE	GetRowBytes(void)		{ return m_width * 4; };
	virtual HRESULT			STDMETHODCALLTYPE	GetBytes(void** buffer);
	virtual BMDFrameFlags	STDMETHODCALLTYPE	GetFlags(void)			{ return m_flags; };
	virtual BMDPixelFormat	STDMETHODCALLTYPE	GetPixelFormat(void)	{ return bmdFormat8BitBGRA; };
	
	// Dummy implementations of remaining methods in IDeckLinkVideoFrame
	virtual HRESULT			STDMETHODCALLTYPE	GetAncillaryData(IDeckLinkVideoFrameAncillary** ancillary) { return E_NOTIMPL; };
	virtual HRESULT			STDMETHODCALLTYPE	GetTimecode(BMDTimecodeFormat format, IDeckLinkTimecode** timecode) { return E_NOTIMPL;	};

	// IUnknown interface
	virtual HRESULT			STDMETHODCALLTYPE	QueryInterface(REFIID iid, LPVOID *ppv);
	virtual ULONG			STDMETHODCALLTYPE	AddRef();
	virtual ULONG			STDMETHODCALLTYPE	Release();
};

