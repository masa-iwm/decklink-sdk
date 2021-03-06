/* -LICENSE-START-
 ** Copyright (c) 2010 Blackmagic Design
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


// translate a BMDDeckControlError to a string
#define ERR_TO_STR(err) ((err)==bmdDeckControlNoError) ? "No error" :\
							((err)==bmdDeckControlModeError) ? "Mode error":\
							((err)==bmdDeckControlMissedInPointError) ? "Missed InPoint error":\
							((err)==bmdDeckControlDeckTimeoutError) ? "DeckTimeout error":\
							((err)==bmdDeckControlCommandFailedError) ? "Cmd failed error":\
							((err)==bmdDeckControlDeviceAlreadyOpenedError) ? "Device already open":\
							((err)==bmdDeckControlFailedToOpenDeviceError) ? "Failed to open device error":\
							((err)==bmdDeckControlInLocalModeError) ? "InLocal mode error":\
							((err)==bmdDeckControlEndOfTapeError) ? "EOT error":\
							((err)==bmdDeckControlUserAbortError) ? "UserAbort error":\
							((err)==bmdDeckControlNoTapeInDeckError) ? "NoTape error":\
							((err)==bmdDeckControlNoVideoFromCardError) ? "No video from card error":\
							((err)==bmdDeckControlNoCommunicationError) ? "No communication error":"Unknown error"

// translate a BMDDeckControlStatusFlags to a group of 4 strings
#define FLAGS_TO_STRS(flags) ((flags) & bmdDeckControlStatusDeckConnected) ? "Deck connected" : "Deck disconnected",\
							((flags) & bmdDeckControlStatusRemoteMode) ? "Remote mode" : "Local mode",\
							((flags) & bmdDeckControlStatusRecordInhibited) ? "Rec. disabled" : "Rec. enabled",\
							((flags) & bmdDeckControlStatusCassetteOut) ? "Cassette out" : "Cassette in"

// translate a BMDDeckControlEvent to a string
#define EVT_TO_STR(evt) ((evt)==bmdDeckControlPrepareForExportEvent) ? "Prepare for export" :\
							((evt)==bmdDeckControlPrepareForCaptureEvent) ? "Prepare for capture" :\
							((evt)==bmdDeckControlExportCompleteEvent) ? "Export complete" :\
							((evt)==bmdDeckControlCaptureCompleteEvent) ? "Capture complete" : "Abort"

// translate a BMDDeckControlVTRControlState to a string
#define STATE_TO_STR(state) (state==bmdDeckControlNotInVTRControlMode) ? "N/A" :\
							(state==bmdDeckControlVTRControlPlaying) ? "Play" :\
							(state==bmdDeckControlVTRControlRecording) ? "Record" :\
							(state==bmdDeckControlVTRControlStill) ? "Still" :\
							(state==bmdDeckControlVTRControlShuttleForward) ? "Shuttle forward" :\
							(state==bmdDeckControlVTRControlShuttleReverse) ? "Shuttle reverse" :\
							(state==bmdDeckControlVTRControlJogForward) ? "Jog forward" :\
							(state==bmdDeckControlVTRControlJogReverse) ? "Jog reverse" : "Stop"

// make a BCD timecode given hour, min, sec and frame values
#define MAKE_TC_BCD(h1,h2,m1,m2,s1,s2,f1,f2) ( \
							(((uint8_t) h1)<<28) +\
							(((uint8_t) h2)<<24) +\
							(((uint8_t) m1)<<20) +\
							(((uint8_t) m2)<<16) +\
							(((uint8_t) s1)<<12) +\
							(((uint8_t) s2)<<8) +\
							(((uint8_t) f1)<<4) +\
							((uint8_t) f2) )
