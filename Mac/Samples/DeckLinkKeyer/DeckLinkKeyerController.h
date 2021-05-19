/* -LICENSE-START-
 ** Copyright (c) 2018 Blackmagic Design
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

/* DeckLinkKeyerController.h */

#import <Cocoa/Cocoa.h>
#import "DeckLinkAPI.h"
#import "DeckLinkKeyerMovieView.h"


@interface DeckLinkKeyerController : NSObject {
	BOOL								videoOutputOn;
	IDeckLink*							deckLink;
	IDeckLinkOutput*					deckLinkOutput;
	IDeckLinkKeyer*						deckLinkKeyer;
	IDeckLinkDisplayMode*				currentDisplayMode;
	IDeckLinkMutableVideoFrame*			keyFrameBuffer;
	//
	NSUserDefaults*						userDefaults;
	
	IBOutlet NSButtonCell*				keyOffRadio;
	IBOutlet NSButtonCell*				internalKeyRadio;
	IBOutlet NSButtonCell*				externalKeyRadio;
	IBOutlet NSMatrix*					radioMatrix;
	
	IBOutlet NSTextField*				modeLabel;
	IBOutlet NSPopUpButton*				outputModeMenu;
	
	IBOutlet NSTextField*				quickKeyLabel;	
	IBOutlet DeckLinkKeyerMovieView*	movieView;
	
	IBOutlet NSTextField*				keyTransitionLabel;
	IBOutlet NSTextField*				durationText;	
	IBOutlet NSTextField*				durationLabel;
	IBOutlet NSButton*					blendOnButton;
	IBOutlet NSButton*					blendOffButton;

	IBOutlet NSTextField*				alphaLabel;
	IBOutlet NSSlider*					alphaSlider;
	IBOutlet NSTextField*				alphaText;
}

- (void)awakeFromNib;
- (void)disableControls;
- (void)enableControls;
- (BOOL)setMovieFile:(NSURL*)fileURL;
- (void)outputCurrentFrame;
- (void)videoOutputOff;
- (void)videoOutputOn;
- (void)applicationWillTerminate:(NSNotification*)notification;
- (void*)getFrameBuffer:(uint32_t)width height:(uint32_t)height;

- (IBAction)performOpen:(id)sender;
- (IBAction)doKeyExternal:(id)sender;
- (IBAction)doKeyInternal:(id)sender;
- (IBAction)doKeyOff:(id)sender;
- (IBAction)doBlendOn:(id)sender;
- (IBAction)doBlendOff:(id)sender;
- (IBAction)outputModeChanged:(id)sender;
- (IBAction)alphaSliderChanged:(id)sender;
- (IBAction)alphaTextChanged:(id)sender;

@end
