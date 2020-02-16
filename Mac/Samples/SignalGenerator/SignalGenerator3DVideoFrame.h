/* -LICENSE-START-
** Copyright (c) 2013 Blackmagic Design
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
//  SignalGenerator3DVideoFrame.h
//  Signal Generator
//

#ifndef SignalGenerator_SignalGenerator3DVideoFrame_h
#define SignalGenerator_SignalGenerator3DVideoFrame_h

#include <atomic>
#include "DeckLinkAPI.h"

/*
 * An example class which may be used to output a frame or pair of frames to
 * a 3D capable output.
 *
 * This class implements the IDeckLinkVideoFrame interface which can
 * be used to operate on the left frame.
 *
 * Access to the right frame through the IDeckLinkVideoFrame3DExtensions
 * interface:
 *
 * 	IDeckLinkVideoFrame *rightEyeFrame;
 * 	hr = threeDimensionalFrame->GetFrameForRightEye(&rightEyeFrame);
 *
 * After which IDeckLinkVideoFrame operations are performed directly
 * on the rightEyeFrame object.
 */

class SignalGenerator3DVideoFrame : public IDeckLinkVideoFrame, public IDeckLinkVideoFrame3DExtensions
{
public:
	// IUnknown methods
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv);
	virtual ULONG STDMETHODCALLTYPE AddRef(void);
	virtual ULONG STDMETHODCALLTYPE Release(void);

	// IDeckLinkVideoFrame methods
	virtual long GetWidth(void);
	virtual long GetHeight(void);
	virtual long GetRowBytes(void);
	virtual BMDPixelFormat GetPixelFormat(void);
	virtual BMDFrameFlags GetFlags(void);
	virtual HRESULT GetBytes(/* out */ void **buffer);

	virtual HRESULT GetTimecode (/* in */ BMDTimecodeFormat format, /* out */ IDeckLinkTimecode **timecode) ;
	virtual HRESULT GetAncillaryData (/* out */ IDeckLinkVideoFrameAncillary **ancillary);

	// IDeckLinkVideoFrame3DExtensions methods
	virtual BMDVideo3DPackingFormat Get3DPackingFormat(void);
	virtual HRESULT GetFrameForRightEye(/* out */ IDeckLinkVideoFrame* *rightEyeFrame);

	SignalGenerator3DVideoFrame(IDeckLinkMutableVideoFrame *left,
									IDeckLinkMutableVideoFrame *right = NULL);
	virtual ~SignalGenerator3DVideoFrame();

protected:
	IDeckLinkMutableVideoFrame* m_frameLeft;
	IDeckLinkMutableVideoFrame* m_frameRight;
	std::atomic<ULONG>			m_refCount;
};

#endif
