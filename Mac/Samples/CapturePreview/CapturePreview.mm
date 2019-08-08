/* -LICENSE-START-
 ** Copyright (c) 2011 Blackmagic Design
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

#import "CapturePreview.h"
#import "DeckLinkController.h"

using namespace std;

static const NSDictionary* kInputConnections = @{
												 [NSNumber numberWithInteger:bmdVideoConnectionSDI]			: @"SDI",
												 [NSNumber numberWithInteger:bmdVideoConnectionHDMI]		: @"HDMI",
												 [NSNumber numberWithInteger:bmdVideoConnectionOpticalSDI]	: @"Optical SDI",
												 [NSNumber numberWithInteger:bmdVideoConnectionComponent]	: @"Component",
												 [NSNumber numberWithInteger:bmdVideoConnectionComposite]	: @"Composite",
												 [NSNumber numberWithInteger:bmdVideoConnectionSVideo]		: @"S-Video",
												};

@implementation TimecodeStruct
@synthesize timecode;
@synthesize userBits;

- (void)dealloc;
{
	[timecode release];
	[userBits release];
	
	[super dealloc];
}

-(id) copyWithZone: (NSZone *) zone
{
	TimecodeStruct *timecodeCopy = [[[self class] allocWithZone: zone] init];
	
	timecodeCopy.timecode = [NSString stringWithString:self.timecode];
	timecodeCopy.userBits = [NSString stringWithString:self.userBits];

	return timecodeCopy;
}

@end

@implementation AncillaryDataStruct

@synthesize vitcF1;
@synthesize vitcF2;
@synthesize rp188vitc1;
@synthesize rp188vitc2;
@synthesize rp188ltc;
@synthesize rp188hfrtc;
@synthesize metadata;

- (void)dealloc
{
	[vitcF1 dealloc];
	[vitcF2 dealloc];
	[rp188vitc1 dealloc];
	[rp188vitc2 dealloc];
	[rp188ltc dealloc];
	[rp188hfrtc dealloc];
	[metadata dealloc];
	
	[super dealloc];
}
@end

@implementation MetadataStruct
@synthesize electroOpticalTransferFunction;
@synthesize displayPrimariesRedX;
@synthesize displayPrimariesRedY;
@synthesize displayPrimariesGreenX;
@synthesize displayPrimariesGreenY;
@synthesize displayPrimariesBlueX;
@synthesize displayPrimariesBlueY;
@synthesize whitePointX;
@synthesize whitePointY;
@synthesize maxDisplayMasteringLuminance;
@synthesize minDisplayMasteringLuminance;
@synthesize maximumContentLightLevel;
@synthesize maximumFrameAverageLightLevel;
@synthesize colorspace;

- (void)dealloc;
{
	[electroOpticalTransferFunction release];
	[displayPrimariesRedX release];
	[displayPrimariesRedY release];
	[displayPrimariesGreenX release];
	[displayPrimariesGreenY release];
	[displayPrimariesBlueX release];
	[displayPrimariesBlueY release];
	[whitePointX release];
	[whitePointY release];
	[maxDisplayMasteringLuminance release];
	[minDisplayMasteringLuminance release];
	[maximumContentLightLevel release];
	[maximumFrameAverageLightLevel release];
	[colorspace release];

	[super dealloc];
}

-(id) copyWithZone: (NSZone *) zone
{
	MetadataStruct *metadataCopy = [[[self class] allocWithZone: zone] init];

	metadataCopy.electroOpticalTransferFunction = [NSString stringWithString:self.electroOpticalTransferFunction];
	metadataCopy.displayPrimariesRedX = [NSString stringWithString:self.displayPrimariesRedX];
	metadataCopy.displayPrimariesRedY = [NSString stringWithString:self.displayPrimariesRedY];
	metadataCopy.displayPrimariesGreenX = [NSString stringWithString:self.displayPrimariesGreenX];
	metadataCopy.displayPrimariesGreenY = [NSString stringWithString:self.displayPrimariesGreenY];
	metadataCopy.displayPrimariesBlueX = [NSString stringWithString:self.displayPrimariesBlueX];
	metadataCopy.displayPrimariesBlueY = [NSString stringWithString:self.displayPrimariesBlueY];
	metadataCopy.whitePointX = [NSString stringWithString:self.whitePointX];
	metadataCopy.whitePointY = [NSString stringWithString:self.whitePointY];
	metadataCopy.maxDisplayMasteringLuminance = [NSString stringWithString:self.maxDisplayMasteringLuminance];
	metadataCopy.minDisplayMasteringLuminance = [NSString stringWithString:self.minDisplayMasteringLuminance];
	metadataCopy.maximumContentLightLevel = [NSString stringWithString:self.maximumContentLightLevel];
	metadataCopy.maximumFrameAverageLightLevel = [NSString stringWithString:self.maximumFrameAverageLightLevel];
	metadataCopy.colorspace = [NSString stringWithString:self.colorspace];

	return metadataCopy;
}
@end


@implementation CapturePreviewAppDelegate

@synthesize window;

- (id)init
{
	self = [super init];
	if (self)
	{
		ancillaryDataValues = [[NSMutableArray arrayWithObjects:@"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", @"", nil] retain];
		ancillaryDataTypes = [[NSMutableArray arrayWithObjects:
							@"VITC Timecode field 1", 
							@"VITC User bits field 1", 
							@"VITC Timecode field 2", 
							@"VITC User bits field 2", 
							@"RP188 VITC1 Timecode", 
							@"RP188 VITC1 User bits",
							@"RP188 LTC Timecode", 
							@"RP188 LTC User bits",
							@"RP188 VITC2 Timecode", 
							@"RP188 VITC2 User bits",
							@"RP188 HFRTC Timecode",
							@"RP188 HFRTC User bits",
							@"Static HDR Electro-optical Transfer Function",
							@"Static HDR Display Primaries Red X",
							@"Static HDR Display Primaries Red Y",
							@"Static HDR Display Primaries Green X",
							@"Static HDR Display Primaries Green Y",
							@"Static HDR Display Primaries Blue X",
							@"Static HDR Display Primaries Blue Y",
							@"Static HDR White Point X",
							@"Static HDR White Point Y",
							@"Static HDR Max Display Mastering Luminance",
							@"Static HDR Min Display Mastering Luminance",
							@"Static HDR Max Content Light Level",
							@"Static HDR Max Frame Average Light Level",
							@"Colorspace",
							nil] retain];
	}
	return self;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	//
	// Setup UI
	
	// Empty popup menus
	[deviceListPopup removeAllItems];
	[modeListPopup removeAllItems];
	[ancillaryDataTable reloadData];
	
	// Disable the interface
	[startStopButton setEnabled:NO];
	[self enableInterface:NO];
	
	//
	// Create and initialise DeckLink device discovery, profile manager and preview objects
	screenPreviewCallback = CreateCocoaScreenPreview(previewView);
	deckLinkDiscovery = new DeckLinkDeviceDiscovery(self);
	profileCallback = new ProfileCallback(self);
	if ((screenPreviewCallback != NULL) && (deckLinkDiscovery != NULL) && (profileCallback != NULL))
	{
		deckLinkDiscovery->Enable();
	}
	else
	{
		[self showErrorMessage:@"This application requires the Desktop Video drivers installed." title:@"Please install the Blackmagic Desktop Video drivers to use the features of this application."];
	}
	
}

- (void)addDevice:(IDeckLink*)deckLink
{
	// Create new DeckLinkDevice object to wrap around new IDeckLink instance
	DeckLinkDevice* device = new DeckLinkDevice(self, deckLink);
	
	// Initialise new DeckLinkDevice object
	if (! device->init())
	{
		[self showErrorMessage:@"Error initialising the new device" title:@"This application is unable to initialise the new device"];
		device->Release();
		return;
	}

	[[deviceListPopup menu] addItemWithTitle:(NSString*)device->getDeviceName() action:nil keyEquivalent:@""];
	[[deviceListPopup lastItem] setTag:(NSInteger)device];
	
	if ([deviceListPopup numberOfItems] == 1)
	{
		// We have added our first item, enable the interface
		[self enableInterface:YES];
		[deviceListPopup selectItemAtIndex:0];
		[self newDeviceSelected:nil];
	}
	
}

- (void)removeDevice:(IDeckLink*)deckLink
{
	DeckLinkDevice* deviceToRemove = NULL;
	DeckLinkDevice* removalCandidate = NULL;
	NSInteger index = 0;
	
	// Find the DeckLinkDevice that wraps the IDeckLink being removed
	for (NSMenuItem* item in [deviceListPopup itemArray])
	{
		removalCandidate = (DeckLinkDevice*)[item tag];

		if (removalCandidate->deckLink == deckLink)
		{
			deviceToRemove = removalCandidate;
			break;
		}
		++index;
	}
	
	if (deviceToRemove == NULL)
		return;
	
	// If capture is ongoing, stop it
	if (deviceToRemove->isCapturing())
		deviceToRemove->stopCapture();
	
	[deviceListPopup removeItemAtIndex:index];
	
	[startStopButton setTitle:@"Start Capture"];
	
	if ([deviceListPopup numberOfItems] == 0)
	{
		// We have removed the last item, disable the interface
		[startStopButton setEnabled:NO];
		[self enableInterface:NO];
		selectedDevice = NULL;
	}
	else if (selectedDevice == deviceToRemove)
	{
		// Select the first device in the list and enable the interface
		[deviceListPopup selectItemAtIndex:0];
		[self newDeviceSelected:nil];
	}
	
	// Release DeckLinkDevice instance
	deviceToRemove->Release();
}

- (void)haltStreams
{
	// Profile is changing, stop capture if active
	if (selectedDevice->isCapturing())
		[self stopCapture];
}

- (void)updateProfile:(IDeckLinkProfile*)newProfile
{
	// Action as if new device selected to check whether device is active/inactive
	// This call will subsequently updated input connections and video mode popups if active
	[self newDeviceSelected:nil];
	
	// A reference was added in IDeckLinkProfileCallback::ProfileActivated callback
	newProfile->Release();
}

- (void)showErrorMessage:(NSString*)message title:(NSString*)title
{
	NSAlert* alert = [[NSAlert alloc] init];
	alert.messageText = title;
	alert.informativeText = message;
	[alert runModal];
	[alert release];
}


- (void)refreshInputConnectionList
{
	BMDVideoConnection availableInputConnections = selectedDevice->getInputConnections();
	
	[inputConnectionPopup removeAllItems];
	
	for (NSNumber* key in kInputConnections)
	{
		BMDVideoConnection inputConnection = (BMDVideoConnection)[key intValue];
		
		if ((inputConnection & availableInputConnections) != 0)
		{
			[inputConnectionPopup addItemWithTitle:kInputConnections[key]];
			[[inputConnectionPopup lastItem] setTag:(NSInteger)inputConnection];
		}
	}

	if ([inputConnectionPopup numberOfItems] > 0)
	{
		int64_t currentInputConnection;
		
		// Get the current selected input connection
		if (selectedDevice->deckLinkConfig->GetInt(bmdDeckLinkConfigVideoInputConnection, &currentInputConnection) == S_OK)
		{
			[inputConnectionPopup selectItemWithTag:(NSInteger)currentInputConnection];
			[self newConnectionSelected:nil];
		}
	}
}


- (void)refreshVideoModeList
{
	// Populate the display mode menu with a list of display modes supported by the installed DeckLink card
	IDeckLinkDisplayModeIterator*		displayModeIterator;
	IDeckLinkDisplayMode*				deckLinkDisplayMode;
	IDeckLinkInput*						deckLinkInput;
	
	// Clear the menu
	[modeListPopup removeAllItems];
	
	deckLinkInput = selectedDevice->deckLinkInput;
	
	if (deckLinkInput->GetDisplayModeIterator(&displayModeIterator) != S_OK)
		return;
	
	while (displayModeIterator->Next(&deckLinkDisplayMode) == S_OK)
	{
		CFStringRef				modeName;
		bool					supported;
		HRESULT					hr;
		
		hr = deckLinkInput->DoesSupportVideoMode(selectedInputConnection, deckLinkDisplayMode->GetDisplayMode(), bmdFormatUnspecified, bmdSupportedVideoModeDefault, &supported);
		if (hr != S_OK || !supported)
			continue;
		
		if (deckLinkDisplayMode->GetName(&modeName) != S_OK)
		{
			deckLinkDisplayMode->Release();
			deckLinkDisplayMode = NULL;
			continue;
		}
		
		// Add this item to the video format poup menu
		[modeListPopup addItemWithTitle:(NSString*)modeName];
		
		// Save the IDeckLinkDisplayMode in the menu item's tag
		[[modeListPopup lastItem] setTag:(NSInteger)deckLinkDisplayMode->GetDisplayMode()];
		
		deckLinkDisplayMode->Release();
		CFRelease(modeName);
	}
	
	displayModeIterator->Release();
	displayModeIterator = NULL;

	[startStopButton setEnabled:([modeListPopup numberOfItems] != 0)];
}


- (IBAction)newDeviceSelected:(id)sender
{
	// Release profile callback from existing selected device
	if ((selectedDevice != NULL) && (selectedDevice->deckLinkProfileManager != NULL))
		selectedDevice->deckLinkProfileManager->SetCallback(NULL);
	
	// Get the DeckLinkDevice object for the selected menu item.
	selectedDevice = (DeckLinkDevice*)[[deviceListPopup selectedItem] tag];
	
	if (selectedDevice != NULL)
	{
		IDeckLinkProfileAttributes*	deckLinkAttributes = NULL;
		int64_t						duplexMode;
		
		// Register profile callback with newly selected device's profile manager
		if (selectedDevice->deckLinkProfileManager != NULL)
			selectedDevice->deckLinkProfileManager->SetCallback(profileCallback);
		
		// Query Duplex mode attribute to check whether sub-device is active
		if (selectedDevice->deckLink->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&deckLinkAttributes) == S_OK)
		{
			if ((deckLinkAttributes->GetInt(BMDDeckLinkDuplex, &duplexMode) == S_OK)
				&& (duplexMode != bmdDuplexInactive))
			{
				// Update the input connections popup menu
				[self refreshInputConnectionList];
				
				// Enable the interface
				[inputConnectionPopup setEnabled:YES];
				[modeListPopup setEnabled:YES];
				[applyDetectedVideoMode setEnabled:YES];
				
				// Set default state for apply detected video mode checkbox
				[applyDetectedVideoMode setState:(selectedDevice->deviceSupportsFormatDetection() ? NSOnState : NSOffState)];
				[self toggleApplyDetectionVideoMode:nil];
			}
			else
			{
				[inputConnectionPopup removeAllItems];
				[inputConnectionPopup setEnabled:NO];
				[modeListPopup removeAllItems];
				[modeListPopup setEnabled:NO];
				[applyDetectedVideoMode setState:NSOffState];
				[startStopButton setEnabled:NO];
			}
			
			deckLinkAttributes->Release();
		}
	}
}


- (IBAction)newConnectionSelected:(id)sender
{
	selectedInputConnection = (BMDVideoConnection)[[inputConnectionPopup selectedItem] tag];
	
	// Configure input connection for selected device
	if (selectedDevice->deckLinkConfig->SetInt(bmdDeckLinkConfigVideoInputConnection, (int64_t)selectedInputConnection) != S_OK)
		return;
	
	// Updated video mode popup for selected input connection
	[self refreshVideoModeList];
}


- (IBAction)toggleStart:(id)sender
{
	if (selectedDevice == NULL)
		return;
	
	if (selectedDevice->isCapturing())
		[self stopCapture];
	else
		[self startCapture];
}

- (IBAction)toggleApplyDetectionVideoMode:(id)sender
{
	[modeListPopup setEnabled:([applyDetectedVideoMode state] == NSOffState)];
}


- (void)startCapture
{
	if (selectedDevice && selectedDevice->startCapture([[modeListPopup selectedItem] tag], screenPreviewCallback, ([applyDetectedVideoMode state] == NSOnState)))
	{
		// Update UI
		[startStopButton setTitle:@"Stop"];
		[self enableInterface: NO];
	}
}


- (void)stopCapture
{
	if (selectedDevice)
		selectedDevice->stopCapture();
	
	// Update UI
	[startStopButton setTitle:@"Start Capture"];
	[self enableInterface:YES];
	[noValidSource setHidden:YES];
	
}


- (void)enableInterface:(BOOL)enabled
{
	[deviceListPopup setEnabled:enabled];
	[inputConnectionPopup setEnabled:enabled];
	[noValidSource setHidden:YES];
	
	if (enabled == TRUE)
	{
		[applyDetectedVideoMode setEnabled:(selectedDevice && selectedDevice->deviceSupportsFormatDetection())];
		[modeListPopup setEnabled:([applyDetectedVideoMode state] == NSOffState)];
	}
	else
	{
		[applyDetectedVideoMode setEnabled:FALSE];
		[modeListPopup setEnabled:enabled];
	}
}


- (BOOL)shouldRestartCaptureWithNewVideoMode
{
	return ([applyDetectedVideoMode state] == NSOnState) ? YES : NO;
}


- (void)updateInputSourceState:(BOOL)state
{
	// Check if the state has changed
	if ([noValidSource isHidden] != state)
	{
		[noValidSource setHidden:state];
	}
}


- (void)selectDetectedVideoMode:(BMDDisplayMode)newVideoMode
{
	[modeListPopup selectItemWithTag:(NSInteger)newVideoMode];
}

- (void)setAncillaryData:(AncillaryDataStruct *)latestAncillaryDataValues
{
	// VITC
	[ancillaryDataValues replaceObjectAtIndex:0 withObject:latestAncillaryDataValues.vitcF1.timecode];
	[ancillaryDataValues replaceObjectAtIndex:1 withObject:latestAncillaryDataValues.vitcF1.userBits];
	[ancillaryDataValues replaceObjectAtIndex:2 withObject:latestAncillaryDataValues.vitcF2.timecode];
	[ancillaryDataValues replaceObjectAtIndex:3 withObject:latestAncillaryDataValues.vitcF2.userBits];
	
	// RP188
	[ancillaryDataValues replaceObjectAtIndex:4 withObject:latestAncillaryDataValues.rp188vitc1.timecode];
	[ancillaryDataValues replaceObjectAtIndex:5 withObject:latestAncillaryDataValues.rp188vitc1.userBits];
	[ancillaryDataValues replaceObjectAtIndex:6 withObject:latestAncillaryDataValues.rp188ltc.timecode];
	[ancillaryDataValues replaceObjectAtIndex:7 withObject:latestAncillaryDataValues.rp188ltc.userBits];
	[ancillaryDataValues replaceObjectAtIndex:8 withObject:latestAncillaryDataValues.rp188vitc2.timecode];
	[ancillaryDataValues replaceObjectAtIndex:9 withObject:latestAncillaryDataValues.rp188vitc2.userBits];
	[ancillaryDataValues replaceObjectAtIndex:10 withObject:latestAncillaryDataValues.rp188hfrtc.timecode];
	[ancillaryDataValues replaceObjectAtIndex:11 withObject:latestAncillaryDataValues.rp188hfrtc.userBits];

	// HDR metadata
	[ancillaryDataValues replaceObjectAtIndex:12 withObject:latestAncillaryDataValues.metadata.electroOpticalTransferFunction];
	[ancillaryDataValues replaceObjectAtIndex:13 withObject:latestAncillaryDataValues.metadata.displayPrimariesRedX];
	[ancillaryDataValues replaceObjectAtIndex:14 withObject:latestAncillaryDataValues.metadata.displayPrimariesRedY];
	[ancillaryDataValues replaceObjectAtIndex:15 withObject:latestAncillaryDataValues.metadata.displayPrimariesGreenX];
	[ancillaryDataValues replaceObjectAtIndex:16 withObject:latestAncillaryDataValues.metadata.displayPrimariesGreenY];
	[ancillaryDataValues replaceObjectAtIndex:17 withObject:latestAncillaryDataValues.metadata.displayPrimariesBlueX];
	[ancillaryDataValues replaceObjectAtIndex:18 withObject:latestAncillaryDataValues.metadata.displayPrimariesBlueY];
	[ancillaryDataValues replaceObjectAtIndex:19 withObject:latestAncillaryDataValues.metadata.whitePointX];
	[ancillaryDataValues replaceObjectAtIndex:20 withObject:latestAncillaryDataValues.metadata.whitePointY];
	[ancillaryDataValues replaceObjectAtIndex:21 withObject:latestAncillaryDataValues.metadata.maxDisplayMasteringLuminance];
	[ancillaryDataValues replaceObjectAtIndex:22 withObject:latestAncillaryDataValues.metadata.minDisplayMasteringLuminance];
	[ancillaryDataValues replaceObjectAtIndex:23 withObject:latestAncillaryDataValues.metadata.maximumContentLightLevel];
	[ancillaryDataValues replaceObjectAtIndex:24 withObject:latestAncillaryDataValues.metadata.maximumFrameAverageLightLevel];
	[ancillaryDataValues replaceObjectAtIndex:25 withObject:latestAncillaryDataValues.metadata.colorspace];
}

- (void)reloadAncillaryTable;
{
	[ancillaryDataTable reloadData];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
	if (([aTableColumn identifier] != nil) && [[aTableColumn identifier] isEqualToString:@"Type"])
	{
		if (rowIndex >= [ancillaryDataTypes count])
			return @"unknown row";
		
		// return ancillary data labels
		return [ancillaryDataTypes objectAtIndex:rowIndex];
	}
	
	if (([aTableColumn identifier] != nil) && [[aTableColumn identifier] isEqualToString:@"Value"])
	{
		if (rowIndex >= [ancillaryDataValues count])
			return @"unknown row";
		
		// return ancillary data values
		return [ancillaryDataValues objectAtIndex:rowIndex];
	}
	
	return @"unknown column";
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
{
	return [ancillaryDataValues count];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
	return YES;
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
	// Stop the capture
	[self stopCapture];

	// Disable profile callback
	if ((selectedDevice != NULL) && (selectedDevice->deckLinkProfileManager != NULL))
	{
		selectedDevice->deckLinkProfileManager->SetCallback(NULL);
	}

	// Disable DeckLink device discovery
	deckLinkDiscovery->Disable();

	// Release all DeckLinkDevice instances
	while([deviceListPopup numberOfItems] > 0)
	{
		DeckLinkDevice* device = (DeckLinkDevice*)[[deviceListPopup itemAtIndex:0] tag];
		device->Release();
		[deviceListPopup removeItemAtIndex:0];
	}

	// Release profile callback interface
	if (profileCallback != NULL)
	{
		profileCallback->Release();
		profileCallback = NULL;
	}
	
	// Release DeckLink device discovery interface
	if (deckLinkDiscovery != NULL)
	{
		deckLinkDiscovery->Release();
		deckLinkDiscovery = NULL;
	}
	
	// Release screen preview callback interface
	if (screenPreviewCallback)
	{
		screenPreviewCallback->Release();
		screenPreviewCallback = NULL;
	}

	[ancillaryDataValues release];
	[ancillaryDataTypes release];
}

@end

