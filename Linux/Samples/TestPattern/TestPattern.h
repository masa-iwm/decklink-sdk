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

#include <mutex>
#include <condition_variable>

#include "DeckLinkAPI.h"
#include "Config.h"

enum OutputSignal
{
	kOutputSignalPip		= 0,
	kOutputSignalDrop		= 1
};


class TestPattern : public IDeckLinkVideoOutputCallback, public IDeckLinkAudioOutputCallback
{
private:
	int32_t					m_refCount;
	BMDConfig*				m_config;
	bool					m_running;
	IDeckLink*				m_deckLink;
	IDeckLinkOutput*		m_deckLinkOutput;
	IDeckLinkDisplayMode*	m_displayMode;

	unsigned long			m_frameWidth;
	unsigned long			m_frameHeight;
	BMDTimeValue			m_frameDuration;
	BMDTimeScale			m_frameTimescale;
	unsigned long			m_framesPerSecond;
	IDeckLinkVideoFrame*	m_videoFrameBlack;
	IDeckLinkVideoFrame*	m_videoFrameBars;
	unsigned long			m_totalFramesScheduled;
	unsigned long			m_totalFramesDropped;
	unsigned long			m_totalFramesCompleted;

	OutputSignal			m_outputSignal;
	void*					m_audioBuffer;
	unsigned long			m_audioBufferSampleLength;
	unsigned long			m_audioBufferOffset;
	BMDAudioSampleRate		m_audioSampleRate;

	std::mutex				m_mutex;
	std::condition_variable	m_stoppedCondition;

	~TestPattern();

	// Signal Generator Implementation
	void			StartRunning();
	void			StopRunning();
	void			ScheduleNextFrame(bool prerolling);
	void			WriteNextAudioSamples();

	void			PrintStatusLine();

public:
	TestPattern(BMDConfig *config);
	bool Run();

	// *** DeckLink API implementation of IDeckLinkVideoOutputCallback IDeckLinkAudioOutputCallback *** //
	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv);
	virtual ULONG STDMETHODCALLTYPE AddRef();
	virtual ULONG STDMETHODCALLTYPE Release();

	virtual HRESULT STDMETHODCALLTYPE ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result);
	virtual HRESULT STDMETHODCALLTYPE ScheduledPlaybackHasStopped();

	virtual HRESULT STDMETHODCALLTYPE RenderAudioSamples(bool preroll);

	HRESULT CreateFrame(IDeckLinkVideoFrame** theFrame, void (*fillFunc)(IDeckLinkVideoFrame*));
};

void FillSine(void* audioBuffer, unsigned long samplesToWrite, unsigned long channels, unsigned long sampleDepth);
void FillColourBars(IDeckLinkVideoFrame* theFrame, bool reverse);
static inline void FillForwardColourBars(IDeckLinkVideoFrame* theFrame)
{
	FillColourBars(theFrame, false);
}
static inline void FillReverseColourBars(IDeckLinkVideoFrame* theFrame)
{
	FillColourBars(theFrame, true);
}
void FillBlack(IDeckLinkVideoFrame* theFrame);
int GetRowBytes(BMDPixelFormat pixelFormat, int frameWidth);
