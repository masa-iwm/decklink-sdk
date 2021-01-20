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
//  SignalGenHDR.mm
//  Signal Generator HDR
//

#include <atomic>
#include <utility>
#include <vector>

#import <CoreFoundation/CFString.h>
#import "ColorBars.h"
#import "DeckLinkDeviceDiscovery.h"
#import "SignalGenHDR.h"
#import "HDRVideoFrame.h"

// Define conventional display primaries and reference white for colorspace
static const ChromaticityCoordinates kDefaultRec2020Colorimetrics = { 0.708, 0.292, 0.170, 0.797, 0.131, 0.046, 0.3127, 0.3290 };

static const double kDefaultMaxDisplayMasteringLuminance	= 1000.0;
static const double kDefaultMinDisplayMasteringLuminance	= 0.0001;
static const double kDefaultMaxCLL							= 1000.0;
static const double kDefaultMaxFALL							= 50.0;

// Supported pixel formats map to string representation and boolean if RGB format
static const std::map<BMDPixelFormat, std::pair<NSString*, bool>> kPixelFormats = {
	std::make_pair(bmdFormat10BitYUV,	std::make_pair(@"10-bit YUV (Video-range)",	false)),
	std::make_pair(bmdFormat10BitRGB,	std::make_pair(@"10-bit RGB (Video-range)",	true)),
	std::make_pair(bmdFormat12BitRGBLE,	std::make_pair(@"12-bit RGB (Full-range)",	true)),
};

// Supported EOTFs
static const std::vector<std::pair<EOTF, NSString*>> kSupportedEOTF = {
	std::make_pair(EOTF::PQ,	@"PQ (ST 2084)"),
	std::make_pair(EOTF::HLG,	@"HLG"),
};

@implementation SignalGenHDRAppDelegate

@synthesize window;

- (void)addDevice:(com_ptr<IDeckLink>)deckLink
{
	CFStringRef							deviceNameStr;
	int64_t								intAttribute;
	bool								attributeFlag = false;
	com_ptr<IDeckLinkProfileAttributes>	deckLinkAttributes(IID_IDeckLinkProfileAttributes, deckLink);
	
	if (!deckLinkAttributes)
		return;
	
	// Check that device has playback interface
	
	if ((deckLinkAttributes->GetInt(BMDDeckLinkVideoIOSupport, &intAttribute) != S_OK)
		|| ((intAttribute & bmdDeviceSupportsPlayback) == 0))
		return;

	// Check that device supports HDR metadata and Rec2020
	if ((deckLinkAttributes->GetFlag(BMDDeckLinkSupportsHDRMetadata, &attributeFlag) != S_OK)
		|| !attributeFlag)
		return;
	if ((deckLinkAttributes->GetFlag(BMDDeckLinkSupportsColorspaceMetadata, &attributeFlag) != S_OK)
		|| !attributeFlag)
		return;
	
	// Get device name
	if (deckLink->GetDisplayName(&deviceNameStr) != S_OK)
		return;
	
	[[deviceListPopup menu] addItemWithTitle:(NSString*)deviceNameStr action:nil keyEquivalent:@""];
	[[deviceListPopup lastItem] setTag:(NSInteger)deckLink.get()];

	CFRelease(deviceNameStr);
	
	if ([deviceListPopup numberOfItems] == 1)
	{
		// We have added our first item, enable the interface
		[deviceListPopup selectItemAtIndex:0];
		[self newDeviceSelected:nil];
		
		[startButton setEnabled:YES];
		[self enableInterface:YES];
		[self enableHDRInterface:YES];
	}
}

- (void)removeDevice:(com_ptr<IDeckLink>)deckLink
{
	NSInteger	indexToRemove = 0;
	BOOL		removeSelectedDevice = NO;

	indexToRemove = [deviceListPopup indexOfItemWithTag:(NSInteger)deckLink.get()];
	if (indexToRemove < 0)
		return;
	
	removeSelectedDevice = (IDeckLink*)[[deviceListPopup selectedItem] tag] == deckLink.get();
	
	[deviceListPopup removeItemAtIndex:indexToRemove];
	
	if (removeSelectedDevice == YES)
	{
		// If playback is ongoing, stop it
		if (running == YES)
			[self stopRunning];
		
		if ([deviceListPopup numberOfItems] > 0)
		{
			// Select the first device in the list and enable the interface
			[deviceListPopup selectItemAtIndex:0];
			[self newDeviceSelected:nil];
			
			[startButton setEnabled:YES];
		}
	}
	
	if ([deviceListPopup numberOfItems] == 0)
	{
		// We have removed the last item, disable the interface
		[startButton setEnabled:NO];
		[self enableInterface:NO];
		[self enableHDRInterface:NO];
	}
}

- (void)refreshDisplayModeMenu
{
	// Populate the display mode menu with a list of display modes supported by the installed DeckLink card
	com_ptr<IDeckLinkDisplayModeIterator>	displayModeIterator;
	com_ptr<IDeckLinkDisplayMode>			deckLinkDisplayMode;

	// Release existing display mode references and clear popup
	supportedDisplayModeMap.clear();
	[videoFormatPopup removeAllItems];

	if (selectedDeckLinkOutput->GetDisplayModeIterator(displayModeIterator.releaseAndGetAddressOf()) != S_OK)
		return;

	while (displayModeIterator->Next(deckLinkDisplayMode.releaseAndGetAddressOf()) == S_OK)
	{
		CFStringRef				modeName;
		BMDDisplayMode			displayMode;

		if (deckLinkDisplayMode->GetName(&modeName) != S_OK)
			continue;

		// Ignore NTSC/PAL/720p/1080i display modes
		if ((deckLinkDisplayMode->GetWidth() < 1920) || (deckLinkDisplayMode->GetFieldDominance() != bmdProgressiveFrame))
			continue;
		
		displayMode = deckLinkDisplayMode->GetDisplayMode();
		supportedDisplayModeMap[displayMode] = deckLinkDisplayMode;
		
		// Add this item to the video format poup menu
		[videoFormatPopup addItemWithTitle:(NSString*)modeName];

		// Save the IDeckLinkDisplayMode in the menu item's tag
		[[videoFormatPopup lastItem] setTag:(NSInteger)displayMode];

		CFRelease(modeName);
	}

	if ([videoFormatPopup numberOfItems] == 0)
		[startButton setEnabled:false];
	else
	{
		[startButton setEnabled:true];
		[videoFormatPopup selectItemAtIndex:0];
		[self newVideoFormatSelected:nil];
	}
}

- (void)refreshPixelFormatMenu
{
	// Populate the pixel format mode combo with a list of pixel formats supported by the installed DeckLink card
	[pixelFormatPopup removeAllItems];
	
	for (auto& pixelFormat : kPixelFormats)
	{
		HRESULT		hr;
		bool		displayModeSupport = false;
		NSString*	pixelFormatString;
		
		std::tie(pixelFormatString, std::ignore) = pixelFormat.second;
		
		hr = selectedDeckLinkOutput->DoesSupportVideoMode(bmdVideoConnectionUnspecified, selectedDisplayMode->GetDisplayMode(), pixelFormat.first, bmdNoVideoOutputConversion, bmdSupportedVideoModeDefault, NULL, &displayModeSupport);
		if (hr != S_OK || !displayModeSupport)
			continue;
		
		// Add this item to the pixel format poup menu
		[pixelFormatPopup addItemWithTitle:pixelFormatString];
		[[pixelFormatPopup lastItem] setTag:(NSInteger)pixelFormat.first];
	}

	[pixelFormatPopup selectItemAtIndex:0];
	[self newPixelFormatSelected:nil];
}

- (void)refreshEOTFMenu
{
	[eotfPopup removeAllItems];
	
	for (auto& eotf : kSupportedEOTF)
	{
		// Full range not defined for HLG EOTF
		if ((eotf.first == EOTF::HLG) && (selectedPixelFormat == bmdFormat12BitRGBLE))
			continue;
		
		[eotfPopup addItemWithTitle:(NSString*)eotf.second];
		[[eotfPopup lastItem] setTag:(NSInteger)eotf.first];
	}
	
	[eotfPopup selectItemAtIndex:0];
	[self newEOTFSelected:nil];
}

- (IBAction)newDeviceSelected:(id)sender
{
	com_ptr<IDeckLink> selectedDeckLink;
	
	selectedDeckLink = (IDeckLink*)[[deviceListPopup selectedItem] tag];
	
	// Get output interface
	selectedDeckLinkOutput = com_ptr<IDeckLinkOutput>(IID_IDeckLinkOutput, selectedDeckLink);
	
	if (!selectedDeckLinkOutput)
		return;
	
	// Get configuration interface
	selectedDeckLinkConfiguration = com_ptr<IDeckLinkConfiguration>(IID_IDeckLinkConfiguration, selectedDeckLink);
	
	if (!selectedDeckLinkConfiguration)
		return;
	
	// Update the display mode popup menu
	[self refreshDisplayModeMenu];

	// Set Screen Preview callback for selected device
	selectedDeckLinkOutput->SetScreenPreviewCallback(CreateCocoaScreenPreview(previewView));
	
	// Enable the interface
	[self enableInterface:YES];
	[self enableHDRInterface:YES];
	
}

- (IBAction)newVideoFormatSelected:(id)sender
{
	auto iter = supportedDisplayModeMap.find((BMDDisplayMode)[[videoFormatPopup selectedItem] tag]);
	if (iter == supportedDisplayModeMap.end())
		return;
	
	// Matched display mode ID
	selectedDisplayMode = iter->second;
	
	[self refreshPixelFormatMenu];
}

- (IBAction)newPixelFormatSelected:(id)sender;
{
	selectedPixelFormat = (BMDPixelFormat)[[pixelFormatPopup selectedItem] tag];

	[self refreshEOTFMenu];
}

- (IBAction)newEOTFSelected:(id)sender
{
	selectedHDRParameters.EOTF = [[eotfPopup selectedItem] tag];
	[self enableHDRInterface:YES];
	[self updateOutputFrame];
}

- (com_ptr<HDRVideoFrame>) CreateColorbarsFrame
{
	com_ptr<IDeckLinkMutableVideoFrame>	referenceFrame = NULL;
	com_ptr<IDeckLinkMutableVideoFrame>	displayFrame = NULL;
	HRESULT								hr;
	BMDPixelFormat						referencePixelFormat;
	int									referenceFrameBytesPerRow;
	int									displayFrameBytesPerRow;
	com_ptr<IDeckLinkVideoConversion>	frameConverter = NULL;
	unsigned long						frameWidth;
	unsigned long						frameHeight;
	EOTFColorRange						colorRange;
	com_ptr<HDRVideoFrame>				ret = NULL;

	frameWidth = selectedDisplayMode->GetWidth();
	frameHeight = selectedDisplayMode->GetHeight();
	
	displayFrameBytesPerRow = GetBytesPerRow(selectedPixelFormat, frameWidth);
	
	if (selectedHDRParameters.EOTF == static_cast<int64_t>(EOTF::HLG))
		colorRange = EOTFColorRange::HLGVideoRange;
	else if (selectedPixelFormat == bmdFormat12BitRGBLE)
		colorRange = EOTFColorRange::PQFullRange;
	else
		colorRange = EOTFColorRange::PQVideoRange;
	
	hr = selectedDeckLinkOutput->CreateVideoFrame(frameWidth, frameHeight, displayFrameBytesPerRow, selectedPixelFormat, bmdFrameFlagDefault, displayFrame.releaseAndGetAddressOf());
	if (hr != S_OK)
		goto bail;

	referencePixelFormat = (selectedPixelFormat == bmdFormat12BitRGBLE) ? bmdFormat12BitRGBLE : bmdFormat10BitRGB;
	
	if (selectedPixelFormat == referencePixelFormat)
	{
		// output pixel format matches reference, so can be filled directly without conversion
		FillBT2111ColorBars(displayFrame, colorRange);
	}
	else
	{
		referenceFrameBytesPerRow = GetBytesPerRow(referencePixelFormat, frameWidth);
		
		// If the pixel formats are different create and fill reference frame
		hr = selectedDeckLinkOutput->CreateVideoFrame(frameWidth, frameHeight, referenceFrameBytesPerRow, referencePixelFormat, bmdFrameFlagDefault, referenceFrame.releaseAndGetAddressOf());
		if (hr != S_OK)
			goto bail;
		FillBT2111ColorBars(referenceFrame, colorRange);

		// Convert to required pixel format
		frameConverter = CreateVideoConversionInstance();

		hr = frameConverter->ConvertFrame(referenceFrame.get(), displayFrame.get());
		if (hr != S_OK)
			goto bail;
	}

	ret = new HDRVideoFrame(displayFrame, selectedHDRParameters);

bail:
	return ret;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
	//
	// Setup UI

	// Empty popup menus
	[deviceListPopup removeAllItems];
	[videoFormatPopup removeAllItems];
	[pixelFormatPopup removeAllItems];
	[eotfPopup removeAllItems];

	// Disable the interface
	[startButton setEnabled:NO];
	[self enableInterface:NO];
	[self enableHDRInterface:NO];

	//
	// Create and initialise DeckLink device discovery and preview objects
	deckLinkDiscovery = new DeckLinkDeviceDiscovery(self);
	if (deckLinkDiscovery)
	{
		deckLinkDiscovery->enable();
	}
	else
	{
		NSAlert* alert = [[NSAlert alloc] init];
		alert.messageText = @"Please install the Blackmagic Desktop Video drivers to use the features of this application.";
		alert.informativeText = @"This application requires the Desktop Video drivers installed.";
		[alert runModal];
		[alert release];
	}

	selectedHDRParameters = { static_cast<int64_t>(EOTF::PQ),
								kDefaultRec2020Colorimetrics,
								kDefaultMaxDisplayMasteringLuminance,
								kDefaultMinDisplayMasteringLuminance,
								kDefaultMaxCLL,
								kDefaultMaxFALL };

	frac3DigitNumberFormatter = [[NSNumberFormatter alloc] init];
	[frac3DigitNumberFormatter setPositiveFormat:@"0.000"];
	[frac3DigitNumberFormatter setMaximum:[NSNumber numberWithDouble:(1.000)]];
	[frac3DigitNumberFormatter setMinimum:[NSNumber numberWithDouble:(0.000)]];
	[[displayPrimaryRedXText cell] setFormatter:frac3DigitNumberFormatter];
	[[displayPrimaryRedYText cell] setFormatter:frac3DigitNumberFormatter];
	[[displayPrimaryGreenXText cell] setFormatter:frac3DigitNumberFormatter];
	[[displayPrimaryGreenYText cell] setFormatter:frac3DigitNumberFormatter];
	[[displayPrimaryBlueXText cell] setFormatter:frac3DigitNumberFormatter];
	[[displayPrimaryBlueYText cell] setFormatter:frac3DigitNumberFormatter];

	frac4DigitNumberFormatter = [[NSNumberFormatter alloc] init];
	[frac4DigitNumberFormatter setPositiveFormat:@"0.0000"];
	[frac4DigitNumberFormatter setMaximum:[NSNumber numberWithDouble:(1.0000)]];
	[frac4DigitNumberFormatter setMinimum:[NSNumber numberWithDouble:(0.0000)]];
	[[whitePointXText cell] setFormatter:frac4DigitNumberFormatter];
	[[whitePointYText cell] setFormatter:frac4DigitNumberFormatter];
	[[minDisplayMasteringLuminanceText cell] setFormatter:frac4DigitNumberFormatter];
	
	int5DigitNumberFormatter = [[NSNumberFormatter alloc] init];
	[int5DigitNumberFormatter setPositiveFormat:@"####0"];
	[int5DigitNumberFormatter setMaximum:[NSNumber numberWithInt:(10000)]];
	[int5DigitNumberFormatter setMinimum:[NSNumber numberWithInt:(0)]];
	[[maxDisplayMasteringLuminanceText cell] setFormatter:int5DigitNumberFormatter];
	[[maxCLLText cell] setFormatter:int5DigitNumberFormatter];
	[[maxFALLText cell] setFormatter:int5DigitNumberFormatter];
	
	[displayPrimaryRedXText setDoubleValue:selectedHDRParameters.referencePrimaries.RedX];
	[displayPrimaryRedXSlider setDoubleValue:selectedHDRParameters.referencePrimaries.RedX];
	[displayPrimaryRedYText setDoubleValue:selectedHDRParameters.referencePrimaries.RedY];
	[displayPrimaryRedYSlider setDoubleValue:selectedHDRParameters.referencePrimaries.RedY];
	[displayPrimaryGreenXText setDoubleValue:selectedHDRParameters.referencePrimaries.GreenX];
	[displayPrimaryGreenXSlider setDoubleValue:selectedHDRParameters.referencePrimaries.GreenX];
	[displayPrimaryGreenYText setDoubleValue:selectedHDRParameters.referencePrimaries.GreenY];
	[displayPrimaryGreenYSlider setDoubleValue:selectedHDRParameters.referencePrimaries.GreenY];
	[displayPrimaryBlueXText setDoubleValue:selectedHDRParameters.referencePrimaries.BlueX];
	[displayPrimaryBlueXSlider setDoubleValue:selectedHDRParameters.referencePrimaries.BlueX];
	[displayPrimaryBlueYText setDoubleValue:selectedHDRParameters.referencePrimaries.BlueY];
	[displayPrimaryBlueYSlider setDoubleValue:selectedHDRParameters.referencePrimaries.BlueY];
	[whitePointXText setDoubleValue:selectedHDRParameters.referencePrimaries.WhiteX];
	[whitePointXSlider setDoubleValue:selectedHDRParameters.referencePrimaries.WhiteX];
	[whitePointYText setDoubleValue:selectedHDRParameters.referencePrimaries.WhiteY];
	[whitePointYSlider setDoubleValue:selectedHDRParameters.referencePrimaries.WhiteY];

	[maxDisplayMasteringLuminanceText setIntValue:selectedHDRParameters.maxDisplayMasteringLuminance];
	[maxDisplayMasteringLuminanceSlider setDoubleValue:log10(selectedHDRParameters.maxDisplayMasteringLuminance)];
	[minDisplayMasteringLuminanceText setDoubleValue:selectedHDRParameters.minDisplayMasteringLuminance];
	[minDisplayMasteringLuminanceSlider setDoubleValue:log10(selectedHDRParameters.minDisplayMasteringLuminance)];
	[maxCLLText setIntValue:selectedHDRParameters.maxCLL];
	[maxCLLSlider setDoubleValue:log10(selectedHDRParameters.maxCLL)];
	[maxFALLText setIntValue:selectedHDRParameters.maxFALL];
	[maxFALLSlider setDoubleValue:log10(selectedHDRParameters.maxFALL)];
}

- (void)enableInterface:(BOOL)enable
{
	// Set the enable state of user interface elements
	[deviceListPopup setEnabled:enable];
	[videoFormatPopup setEnabled:enable];
	[pixelFormatPopup setEnabled:enable];
}

- (void)enableHDRInterface:(BOOL)enable
{
	BOOL enableMetadata = enable && ([[eotfPopup selectedItem] tag] != (NSInteger)(EOTF::HLG));
	
	// Set the enable state of HDR interface elements
	[eotfPopup setEnabled:enable];
	[displayPrimaryRedXSlider setEnabled:enableMetadata];
	[displayPrimaryRedXText setEnabled:enableMetadata];
	[displayPrimaryRedYSlider setEnabled:enableMetadata];
	[displayPrimaryRedYText setEnabled:enableMetadata];
	[displayPrimaryGreenXSlider setEnabled:enableMetadata];
	[displayPrimaryGreenXText setEnabled:enableMetadata];
	[displayPrimaryGreenYSlider setEnabled:enableMetadata];
	[displayPrimaryGreenYText setEnabled:enableMetadata];
	[displayPrimaryBlueXSlider setEnabled:enableMetadata];
	[displayPrimaryBlueXText setEnabled:enableMetadata];
	[displayPrimaryBlueYSlider setEnabled:enableMetadata];
	[displayPrimaryBlueYText setEnabled:enableMetadata];
	[whitePointXSlider setEnabled:enableMetadata];
	[whitePointXText setEnabled:enableMetadata];
	[whitePointYSlider setEnabled:enableMetadata];
	[whitePointYText setEnabled:enableMetadata];
	[maxDisplayMasteringLuminanceSlider setEnabled:enableMetadata];
	[maxDisplayMasteringLuminanceText setEnabled:enableMetadata];
	[minDisplayMasteringLuminanceSlider setEnabled:enableMetadata];
	[minDisplayMasteringLuminanceText setEnabled:enableMetadata];
	[maxCLLSlider setEnabled:enableMetadata];
	[maxCLLText setEnabled:enableMetadata];
	[maxFALLSlider setEnabled:enableMetadata];
	[maxFALLText setEnabled:enableMetadata];
}

- (IBAction)toggleStart:(id)sender
{
	if (running == NO)
		[self startRunning];
	else
		[self stopRunning];
}

- (IBAction)displayPrimaryRedXSliderChanged:(id)sender
{
	[displayPrimaryRedXText setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.RedX = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)displayPrimaryRedXTextChanged:(id)sender
{
	[displayPrimaryRedXSlider setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.RedX = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)displayPrimaryRedYSliderChanged:(id)sender
{
	[displayPrimaryRedYText setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.RedY = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)displayPrimaryRedYTextChanged:(id)sender
{
	[displayPrimaryRedYSlider setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.RedY = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)displayPrimaryGreenXSliderChanged:(id)sender
{
	[displayPrimaryGreenXText setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.GreenX = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)displayPrimaryGreenXTextChanged:(id)sender
{
	[displayPrimaryGreenXSlider setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.GreenX = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)displayPrimaryGreenYSliderChanged:(id)sender
{
	[displayPrimaryGreenYText setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.GreenY = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)displayPrimaryGreenYTextChanged:(id)sender
{
	[displayPrimaryGreenYSlider setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.GreenY = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)displayPrimaryBlueXSliderChanged:(id)sender
{
	[displayPrimaryBlueXText setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.BlueX = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)displayPrimaryBlueXTextChanged:(id)sender
{
	[displayPrimaryBlueXSlider setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.BlueX = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)displayPrimaryBlueYSliderChanged:(id)sender
{
	[displayPrimaryBlueYText setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.BlueY = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)displayPrimaryBlueYTextChanged:(id)sender
{
	[displayPrimaryBlueYSlider setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.BlueY = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)whitePointXSliderChanged:(id)sender
{
	[whitePointXText setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.WhiteX = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)whitePointXTextChanged:(id)sender
{
	[whitePointXSlider setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.WhiteX = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)whitePointYSliderChanged:(id)sender
{
	[whitePointYText setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.WhiteY = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)whitePointYTextChanged:(id)sender
{
	[whitePointYSlider setDoubleValue:[sender doubleValue]];
	selectedHDRParameters.referencePrimaries.WhiteY = [sender doubleValue];
	[self updateOutputFrame];
}

- (IBAction)maxDisplayMasteringLuminanceSliderChanged:(id)sender
{
	selectedHDRParameters.maxDisplayMasteringLuminance = pow(10.0,[sender doubleValue]);
	[maxDisplayMasteringLuminanceText setIntValue:selectedHDRParameters.maxDisplayMasteringLuminance];
	[self updateOutputFrame];
}

- (IBAction)maxDisplayMasteringLuminanceTextChanged:(id)sender
{
	selectedHDRParameters.maxDisplayMasteringLuminance = [sender doubleValue];
	[maxDisplayMasteringLuminanceSlider setDoubleValue:log10(selectedHDRParameters.maxDisplayMasteringLuminance)];
	[self updateOutputFrame];
}

- (IBAction)minDisplayMasteringLuminanceSliderChanged:(id)sender
{
	selectedHDRParameters.minDisplayMasteringLuminance = pow(10.0,[sender doubleValue]);
	[minDisplayMasteringLuminanceText setDoubleValue:selectedHDRParameters.minDisplayMasteringLuminance];
	[self updateOutputFrame];
}

- (IBAction)minDisplayMasteringLuminanceTextChanged:(id)sender
{
	selectedHDRParameters.minDisplayMasteringLuminance = [sender doubleValue];
	[minDisplayMasteringLuminanceSlider setDoubleValue:log10(selectedHDRParameters.minDisplayMasteringLuminance)];
	[self updateOutputFrame];
}

- (IBAction)maxCLLSliderChanged:(id)sender
{
	selectedHDRParameters.maxCLL = pow(10.0,[sender doubleValue]);
	[maxCLLText setIntValue:selectedHDRParameters.maxCLL];
	[self updateOutputFrame];
}

- (IBAction)maxCLLTextChanged:(id)sender
{
	selectedHDRParameters.maxCLL = [sender doubleValue];
	[maxCLLSlider setDoubleValue:log10(selectedHDRParameters.maxCLL)];
	[self updateOutputFrame];
}

- (IBAction)maxFALLSliderChanged:(id)sender
{
	selectedHDRParameters.maxFALL = pow(10.0,[sender doubleValue]);
	[maxFALLText setIntValue:selectedHDRParameters.maxFALL];
	[self updateOutputFrame];
}

- (IBAction)maxFALLTextChanged:(id)sender
{
	selectedHDRParameters.maxFALL = [sender doubleValue];
	[maxFALLSlider setDoubleValue:log10(selectedHDRParameters.maxFALL)];
	[self updateOutputFrame];
}

- (void)startRunning
{
	bool		output444;
	HRESULT		result;
	
	// Set the output to 444 if RGB mode is selected
	try
	{
		std::tie(std::ignore, output444) = kPixelFormats.at(selectedPixelFormat);
	}
	catch (std::out_of_range)
	{
		goto bail;
	}
	
	// If a device without SDI output is used, then SetFlags will return E_NOTIMPL
	result = selectedDeckLinkConfiguration->SetFlag(bmdDeckLinkConfig444SDIVideoOutput, output444);
	if ((result != S_OK) && (result != E_NOTIMPL))
		goto bail;
	
	// Set the video output mode
	if (selectedDeckLinkOutput->EnableVideoOutput(selectedDisplayMode->GetDisplayMode(), bmdVideoOutputFlagDefault) != S_OK)
		goto bail;
	
	videoFrameBars = [self CreateColorbarsFrame];
	if (! videoFrameBars)
		goto bail;

	if (selectedDeckLinkOutput->DisplayVideoFrameSync(videoFrameBars.get()) != S_OK)
		goto bail;
	
	// Success; update the UI
	running = YES;
	[startButton setTitle:@"Stop"];
	// Disable the user interface while running (prevent the user from making changes to the output signal)
	[self enableInterface:NO];
	
	return;
	
bail:
	NSAlert* alert = [[NSAlert alloc] init];
	alert.messageText = @"Unable to start playback.";
	alert.informativeText = @"Could not start playback, is the device already in use?";
	[alert runModal];
	[alert release];
	
	[self stopRunning];
}

- (void)stopRunning
{
	if (selectedDeckLinkOutput)
		selectedDeckLinkOutput->DisableVideoOutput();

	// Success; update the UI
	running = NO;
	[startButton setTitle:@"Start"];
	// Re-enable the user interface when stopped
	[self enableInterface:YES];
}

- (void)updateOutputFrame
{
	if (running == YES)
	{
		// Update the HDR metadata in frame and output
		// As these statements are sequential, they are thread safe.  It is not recommended
		// that an active frame is updated, and normally synchronization will be required
		videoFrameBars->UpdateHDRMetadata(selectedHDRParameters);
		selectedDeckLinkOutput->DisplayVideoFrameSync(videoFrameBars.get());
	}
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
	return YES;
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
	// Stop the output signal
	[self stopRunning];

	// Disable DeckLink device discovery
	deckLinkDiscovery->disable();

	// Release supported display modes
	supportedDisplayModeMap.clear();

	[frac3DigitNumberFormatter release];
	[frac4DigitNumberFormatter release];
	[int5DigitNumberFormatter release];
}

@end


int GetBytesPerRow(BMDPixelFormat pixelFormat, uint32_t frameWidth)
{
	int bytesPerRow;
	
	// Refer to DeckLink SDK Manual - 2.7.4 Pixel Formats
	switch (pixelFormat)
	{
		case bmdFormat12BitRGBLE:
			bytesPerRow = (frameWidth * 36) / 8;
			break;
			
		case bmdFormat10BitYUV:
			bytesPerRow = ((frameWidth + 47) / 48) * 128;
			break;

		case bmdFormat10BitRGB:
			bytesPerRow = ((frameWidth + 63) /64) * 256;
			break;
			
		default:
			bytesPerRow = frameWidth * 4;
			break;
	}
	
	return bytesPerRow;
}

