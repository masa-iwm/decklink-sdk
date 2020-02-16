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
//  SyncController.h
//  Signal Generator
//

#include <atomic>
#include <vector>
#include <Cocoa/Cocoa.h>
#include "DeckLinkAPI.h"
#include "SignalGenerator3DVideoFrame.h"


enum OutputSignal
{
	kOutputSignalPip		= 0,
	kOutputSignalDrop		= 1
};

// Forward declarations
class DeckLinkDeviceDiscovery;
class PlaybackDelegate;
class ProfileCallback;

@interface SyncController : NSObject {
	NSWindow*					window;

	IBOutlet NSButton*			startButton;

	IBOutlet NSPopUpButton*		outputSignalPopup;
	IBOutlet NSPopUpButton*		audioChannelPopup;
	IBOutlet NSPopUpButton*		audioSampleDepthPopup;
	IBOutlet NSPopUpButton*		videoFormatPopup;
	IBOutlet NSPopUpButton*		pixelFormatPopup;
	IBOutlet NSPopUpButton*		deviceListPopup;

	IBOutlet NSView*			previewView;
	
	NSCondition*				stoppedCondition;
	BOOL						playbackStopped;
	
	DeckLinkDeviceDiscovery*	deckLinkDiscovery;
	PlaybackDelegate*			selectedDevice;
	ProfileCallback*			profileCallback;
	IDeckLinkDisplayMode*		selectedDisplayMode;
	BMDVideoOutputFlags			selectedVideoOutputFlags;
	
	BOOL						running;
	
	uint32_t					frameWidth;
	uint32_t					frameHeight;
	BMDTimeValue				frameDuration;
	BMDTimeScale				frameTimescale;
	uint32_t					framesPerSecond;
	SignalGenerator3DVideoFrame*	videoFrameBlack;
	SignalGenerator3DVideoFrame*	videoFrameBars;
	uint32_t					totalFramesScheduled;
	//
	OutputSignal				outputSignal;
	void*						audioBuffer;
	uint32_t					audioBufferSampleLength;
	uint32_t					audioSamplesPerFrame;
	uint32_t					audioChannelCount;
	BMDAudioSampleRate			audioSampleRate;
	uint32_t					audioSampleDepth;
	uint32_t					totalAudioSecondsScheduled;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification;
- (void)applicationWillTerminate:(NSNotification *)notification;

- (void)addDevice:(IDeckLink*)deckLink;
- (void)removeDevice:(IDeckLink*)deckLink;

- (void)haltStreams;
- (void)updateProfile:(IDeckLinkProfile*)newProfile;

- (void)refreshDisplayModeMenu;
- (void)refreshPixelFormatMenu;
- (void)refreshAudioChannelMenu;

- (IBAction)toggleStart:(id)sender;
- (IBAction)newDeviceSelected:(id)sender;
- (IBAction)newDisplayModeSelected:(id)sender;
- (void)enableInterface:(BOOL)enable;

- (void)startRunning;
- (void)stopRunning;
- (void)scheduledPlaybackStopped;
- (void)scheduleNextFrame:(BOOL)prerolling;
- (void)writeNextAudioSamples;

@property (assign) IBOutlet NSWindow *window;

@end

class PlaybackDelegate : public IDeckLinkVideoOutputCallback,
						 public IDeckLinkAudioOutputCallback
{
private:
	std::atomic<ULONG>			m_refCount;
	SyncController*				m_controller;
	IDeckLinkOutput*			m_deckLinkOutput;
	IDeckLinkConfiguration*		m_deckLinkConfiguration;
	IDeckLink*					m_deckLink;
	IDeckLinkProfileManager*	m_deckLinkProfileManager;
	CFStringRef					m_deviceName;

public:
	PlaybackDelegate (SyncController* owner, IDeckLink* deckLink);
	virtual ~PlaybackDelegate();
	
	bool				init();
	
	// IUnknown
	virtual HRESULT		QueryInterface (REFIID iid, LPVOID *ppv);
	virtual ULONG		AddRef ();
	virtual ULONG		Release ();
	
	// IDeckLinkVideoOutputCallback
	virtual HRESULT		ScheduledFrameCompleted (IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result);
	virtual HRESULT		ScheduledPlaybackHasStopped ();
	
	// IDeckLinkAudioOutputCallback
	virtual HRESULT		RenderAudioSamples (bool preroll);
	

	NSString*			getDeviceName() { return (NSString*)m_deviceName; }

	IDeckLinkOutput*			getDeviceOutput() { return m_deckLinkOutput; }
	IDeckLinkConfiguration*		getDeviceConfiguration() { return m_deckLinkConfiguration; }
	IDeckLink*					getDeckLinkDevice() { return m_deckLink; }
	IDeckLinkProfileManager*	getDeckLinkProfileManager() { return m_deckLinkProfileManager; }

};

class ProfileCallback : public IDeckLinkProfileCallback
{
private:
	SyncController*				m_controller;
	std::atomic<ULONG>			m_refCount;
	
public:
	ProfileCallback(SyncController* owner);
	virtual ~ProfileCallback() {}
	
	// IDeckLinkProfileCallback interface
	virtual HRESULT		ProfileChanging (IDeckLinkProfile *profileToBeActivated, bool streamsWillBeForcedToStop);
	virtual HRESULT		ProfileActivated (IDeckLinkProfile *activatedProfile);
	
	// IUnknown needs only a dummy implementation
	virtual HRESULT		QueryInterface (REFIID iid, LPVOID *ppv);
	virtual ULONG		AddRef ();
	virtual ULONG		Release ();
};

class DeckLinkDeviceDiscovery : public IDeckLinkDeviceNotificationCallback
{
private:
	IDeckLinkDiscovery*		m_deckLinkDiscovery;
	SyncController*			m_uiDelegate;
	std::atomic<ULONG>		m_refCount;

public:
	DeckLinkDeviceDiscovery(SyncController* uiDelegate);
	virtual ~DeckLinkDeviceDiscovery();

	bool				enable();
	void				disable();

	// IDeckLinkDeviceArrivalNotificationCallback interface
	virtual HRESULT		DeckLinkDeviceArrived (/* in */ IDeckLink* deckLinkDevice);
	virtual HRESULT		DeckLinkDeviceRemoved (/* in */ IDeckLink* deckLinkDevice);

	// IUnknown needs only a dummy implementation
	virtual HRESULT		QueryInterface (REFIID iid, LPVOID *ppv);
	virtual ULONG		AddRef ();
	virtual ULONG		Release ();
};

void	FillSine (void* audioBuffer, uint32_t samplesToWrite, uint32_t channels, uint32_t sampleDepth);
void	FillColourBars (IDeckLinkVideoFrame* theFrame, bool reversed);
void	FillBlack (IDeckLinkVideoFrame* theFrame);
void	ScheduleNextVideoFrame (void);
int		GetRowBytes(BMDPixelFormat pixelFormat, int frameWidth);
