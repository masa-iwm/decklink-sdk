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

#import "DeckLinkCoreMediaWriter.h"
#import "DeckLinkAPI.h"

// Audio configuration (must be compatible with DeckLink audio input)
// 2-channel 16-bit (DeckLink only supports 48kHz PCM)
static uint32_t kAudioChannelCount	= 2;
static uint32_t kAudioBitsPerSample	= bmdAudioSampleType16bitInteger;

static void ReleaseCapturedDeckLinkVideoFrame(void* releaseRefCon, const void* baseAddress)
{
	(void)baseAddress; // unused

	if (releaseRefCon)
		((IDeckLinkVideoInputFrame*)releaseRefCon)->Release();
}

static void ReleaseCapturedDeckLinkAudioPacket(void *releaseRefCon, void *baseAddress, size_t size)
{
	(void)baseAddress;	// unused
	(void)size;			// unused
	
	if (releaseRefCon)
		((IDeckLinkAudioInputPacket*)releaseRefCon)->Release();
}

static CMSampleBufferRef CMSampleBufferFromDeckLinkAudioPacket(BMDTimeValue streamTime, AudioStreamBasicDescription& audioFormatDesc,
														  com_ptr<IDeckLinkAudioInputPacket> audioPacket)
{
	bool							success				= false;
	CMAudioFormatDescriptionRef		formatDesc			= nullptr;
	CMBlockBufferRef				audioBlockBuffer	= nullptr;
	CMBlockBufferCustomBlockSource	blockSource;
	CMSampleBufferRef				audioSampleBuffer	= nullptr;
	char*							audioDataPtr		= nullptr;
	long							audioFrameCount		= audioPacket->GetSampleFrameCount();
	size_t							audioBufferLength	= audioFrameCount * audioFormatDesc.mBytesPerFrame;

	// Set DeckLinkAudioInputPacket audio buffer and deallocator
	blockSource.version			= kCMBlockBufferCustomBlockSourceVersion;
	blockSource.AllocateBlock	= nullptr;
	blockSource.FreeBlock		= &ReleaseCapturedDeckLinkAudioPacket;
	blockSource.refCon			= audioPacket.get();
	
	if (audioPacket->GetBytes((void**)&audioDataPtr) != S_OK)
		goto bail;
	
	// Create a DeckLinkAudioInputPacket backed CMBlockBuffer
	CMBlockBufferCreateWithMemoryBlock(nullptr, audioDataPtr, audioBufferLength, nullptr, &blockSource, 0, audioBufferLength, 0, &audioBlockBuffer);

	// Create a format description for the audio data
	if (CMAudioFormatDescriptionCreate(nullptr, &audioFormatDesc, 0, nullptr, 0, nullptr, nullptr, &formatDesc) != noErr)
		goto bail;
	
	// Create the final CMSampleBuffer
	if (CMAudioSampleBufferCreateReadyWithPacketDescriptions(nullptr, audioBlockBuffer, formatDesc, audioFrameCount,
															 CMTimeMake(streamTime, (int32_t)audioFormatDesc.mSampleRate),
															 nullptr, &audioSampleBuffer) != noErr)
		goto bail;
	
	// Manually AddRef audio packet, released by ReleaseCapturedDeckLinkAudioPacket when wrapping CMSampleBuffer is released
	audioPacket->AddRef();
	
	success = true;

bail:
	if (audioBlockBuffer)
		CFRelease(audioBlockBuffer);

	if (formatDesc)
		CFRelease(formatDesc);

	if (!success)
	{
		if (audioSampleBuffer)
		{
			CFRelease(audioSampleBuffer);
			audioSampleBuffer = nullptr;
		}
	}

	return audioSampleBuffer;
}

DeckLinkCoreMediaWriter::DeckLinkCoreMediaWriter() :
m_avAsset(nullptr),
m_avAssetWriter(nullptr),
m_audioInput(nullptr),
m_videoInput(nullptr),
m_videoInputPixelAdaptor(nullptr),
m_writerState(kWriterIdle),
m_filePath(""),
m_frameWidth(0),
m_frameHeight(0),
m_frameDuration(0),
m_timeScale(0),
m_videoOffset(kInvalidVideoOffset),
m_audioStreamTime(0),
m_audioOffset(kInvalidAudioOffset)
{
}

DeckLinkCoreMediaWriter::~DeckLinkCoreMediaWriter()
{
	complete(false);
}

bool DeckLinkCoreMediaWriter::init(const std::string& filePath, long frameWidth, long frameHeight, BMDPixelFormat pixelFormat, BMDTimeValue duration, BMDTimeScale timeScale)
{
	m_filePath		= filePath;
	m_frameDuration	= duration;
	m_timeScale		= timeScale;

	bool		success		= false;
	NSError*	error		= nullptr;
	NSString*	filename 	= [[NSString alloc] initWithUTF8String:m_filePath.c_str()];

	m_avAssetWriter = [[AVAssetWriter alloc]
			initWithURL:[NSURL fileURLWithPath:filename]
			fileType:AVFileTypeQuickTimeMovie error:&error];

	// Setup Audio - 2 channel, 48Khz, 16-bit linear PCM
	m_audioStreamDescription.mSampleRate		= bmdAudioSampleRate48kHz;
	m_audioStreamDescription.mFormatID			= kAudioFormatLinearPCM;
	m_audioStreamDescription.mFormatFlags 		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	m_audioStreamDescription.mBytesPerPacket	= kAudioChannelCount * (kAudioBitsPerSample / 8);
	m_audioStreamDescription.mFramesPerPacket	= 1;
	m_audioStreamDescription.mBytesPerFrame		= m_audioStreamDescription.mBytesPerPacket;
	m_audioStreamDescription.mChannelsPerFrame	= kAudioChannelCount;
	m_audioStreamDescription.mBitsPerChannel	= kAudioBitsPerSample;

	AudioChannelLayout stereoChannelLayout =
	{
			.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo,
			.mChannelBitmap = 0,
			.mNumberChannelDescriptions = 0
	};

	NSData *channelLayoutAsData = [NSData dataWithBytes:&stereoChannelLayout length:offsetof(AudioChannelLayout, mChannelDescriptions)];

	// Compress to 128kbps AAC
	NSDictionary *audioCompressionSettings = @{AVFormatIDKey: @(kAudioFormatMPEG4AAC),
											   AVEncoderBitRateKey: @128000,
											   AVSampleRateKey: @(bmdAudioSampleRate48kHz),
											   AVChannelLayoutKey: channelLayoutAsData,
											   AVNumberOfChannelsKey: @(kAudioChannelCount)};

	// Setup Video - H.264
	NSDictionary *videoCompressionSettings = nil;
	NSDictionary *videoCleanApertureSettings = @{AVVideoCleanApertureWidthKey: @(frameWidth),
												 AVVideoCleanApertureHeightKey: @(frameHeight),
												 AVVideoCleanApertureHorizontalOffsetKey: @0,
												 AVVideoCleanApertureVerticalOffsetKey: @0};

	NSDictionary *videoAspectRatioSettings = @{AVVideoPixelAspectRatioHorizontalSpacingKey: @3,
											   AVVideoPixelAspectRatioVerticalSpacingKey: @3};

	auto iter = kBMDCMPixelFormatMap.find(pixelFormat);
	if (iter == kBMDCMPixelFormatMap.end())
		return false;
	
	NSDictionary *bufferAttributes = @{(NSString*)kCVPixelBufferPixelFormatTypeKey:@(iter->second)};

	if (videoCleanApertureSettings || videoAspectRatioSettings)
	{
		NSMutableDictionary *mutableVideoCompressionSettings = [NSMutableDictionary dictionary];
		if (videoCleanApertureSettings)
			mutableVideoCompressionSettings[AVVideoCleanApertureKey] = videoCleanApertureSettings;
		if (videoAspectRatioSettings)
			mutableVideoCompressionSettings[AVVideoPixelAspectRatioKey] = videoAspectRatioSettings;
		videoCompressionSettings = mutableVideoCompressionSettings;
	}

	NSMutableDictionary *videoSettings = [@{AVVideoCodecKey: AVVideoCodecH264,
											AVVideoWidthKey: @(frameWidth),
											AVVideoHeightKey: @(frameHeight)} mutableCopy];
	if (videoCompressionSettings)
		videoSettings[AVVideoCompressionPropertiesKey] = videoCompressionSettings;

	if (!m_avAssetWriter || ![m_avAssetWriter canApplyOutputSettings:audioCompressionSettings forMediaType:AVMediaTypeAudio])
		goto bail;

	m_audioInput = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeAudio outputSettings:audioCompressionSettings];
	[m_avAssetWriter addInput:m_audioInput];

	if (![m_avAssetWriter canApplyOutputSettings:videoSettings forMediaType:AVMediaTypeVideo])
		goto bail;

	m_videoInput = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo outputSettings:videoSettings];
	
	m_videoInput.expectsMediaDataInRealTime	= true;
	m_videoInput.mediaTimeScale				= (int32_t)m_timeScale;
	m_avAssetWriter.movieTimeScale			= (int32_t)m_timeScale;

	m_videoInputPixelAdaptor = [AVAssetWriterInputPixelBufferAdaptor assetWriterInputPixelBufferAdaptorWithAssetWriterInput:m_videoInput
																								sourcePixelBufferAttributes:bufferAttributes];
	
	[m_avAssetWriter addInput:m_videoInput];

	m_videoOffset = kInvalidVideoOffset;
	m_audioOffset = kInvalidAudioOffset;

	// Start writing the file
	if ([m_avAssetWriter startWriting])
	{
		[m_avAssetWriter startSessionAtSourceTime:kCMTimeZero];
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_writerState = kWriterReady;
		}
		success = true;
	}

bail:
	if (!success)
		m_filePath = "";

	return success;
}

uint32_t DeckLinkCoreMediaWriter::audioBitsPerSample() const
{
	return kAudioBitsPerSample;
}

uint32_t DeckLinkCoreMediaWriter::audioChannelCount() const
{
	return kAudioChannelCount;
}

WriteResult DeckLinkCoreMediaWriter::writeVideoFrame(com_ptr<IDeckLinkVideoFrame>& deckLinkFrame, BMDTimeValue streamTime)
{
	void*						deckLinkFrameBuffer	= nullptr;
	CVPixelBufferRef			cvPixelBuffer		= nullptr;
	CMSampleBufferRef			videoSampleBuffer 	= nullptr;
	CMVideoFormatDescriptionRef	videoInfo 			= nullptr;

	if (!deckLinkFrame || deckLinkFrame->GetBytes(&deckLinkFrameBuffer) != S_OK)
		return kFrameError;

	// Get the CoreMedia pixel format for this frame
	auto iter = kBMDCMPixelFormatMap.find(deckLinkFrame->GetPixelFormat());
	if (iter == kBMDCMPixelFormatMap.end())
		return kFrameError;

	CMPixelFormatType pixelFormat = iter->second;

	if (m_videoOffset < 0)
		m_videoOffset = streamTime;

	WriteResult result = kFrameError;

	if (CVPixelBufferCreateWithBytes(kCFAllocatorSystemDefault,
									 (size_t)deckLinkFrame->GetWidth(),
									 (size_t)deckLinkFrame->GetHeight(),
									 pixelFormat,
									 deckLinkFrameBuffer,
									 (size_t)deckLinkFrame->GetRowBytes(),
									 ReleaseCapturedDeckLinkVideoFrame,
									 deckLinkFrame.get(),
									 nullptr,
									 &cvPixelBuffer) != kIOReturnSuccess)
		goto bail;

	// Manually AddRef video frame, released by ReleaseCapturedDeckLinkVideoFrame when wrapping CVPixelBuffer is released
	deckLinkFrame->AddRef();
	
	// A queue of frames could be used here to extend the time the system is temporarily unable to write frames,
	// however, if the system is too slow to compress frames in realtime frames will be dropped.
	if (![m_videoInput isReadyForMoreMediaData])
	{
		result = kFrameDropped;
		goto bail;
	}
	
	if ([m_videoInputPixelAdaptor appendPixelBuffer:cvPixelBuffer withPresentationTime:CMTimeMake(streamTime - m_videoOffset, (int32_t)m_timeScale)])
		result = kFrameComplete;

bail:
	if (cvPixelBuffer)
		CVPixelBufferRelease(cvPixelBuffer);

	if (videoSampleBuffer)
		CFRelease(videoSampleBuffer);

	if (videoInfo)
		CFRelease(videoInfo);

	// deckLinkFrame will be cleaned up by ReleaseCapturedDeckLinkVideoFrame
	
	return result;
}

WriteResult DeckLinkCoreMediaWriter::writeAudioPacket(com_ptr<IDeckLinkAudioInputPacket>& deckLinkAudioPacket)
{
	void*				audioBuffer 		= nullptr;
	BMDTimeValue		audioStreamTime		= 0;
	CMSampleBufferRef	audioSampleBuffer	= nullptr;
	
	if (!deckLinkAudioPacket || deckLinkAudioPacket->GetBytes(&audioBuffer) != S_OK)
		return kFrameError;

	if (deckLinkAudioPacket->GetPacketTime(&audioStreamTime, (BMDTimeScale)m_audioStreamDescription.mSampleRate) != S_OK)
		return kFrameError;

	if (m_audioOffset < 0)
		m_audioOffset = audioStreamTime;

	WriteResult	result = kFrameError;
	
	if (m_audioInput)
	{
		audioSampleBuffer = CMSampleBufferFromDeckLinkAudioPacket(audioStreamTime - m_audioOffset, m_audioStreamDescription, deckLinkAudioPacket);
		if (audioBuffer)
		{
			// Write the audio to the file
			if (![m_audioInput isReadyForMoreMediaData])
				return kFrameDropped;
			
			if ([m_audioInput appendSampleBuffer:audioSampleBuffer])
				result = kFrameComplete;

			CFRelease(audioSampleBuffer);
		}
	}
	
	return result;
}

void DeckLinkCoreMediaWriter::writeFrame(com_ptr<IDeckLinkVideoFrame>& deckLinkFrame, BMDTimeValue streamTime, com_ptr<IDeckLinkAudioInputPacket>& deckLinkAudioPacket, const WriteResultHandlerFunc& resultHandler)
{
	
		WriteResult result = kFrameError;

		std::lock_guard<std::mutex> lock(m_mutex);
	
		if (m_writerState == kWriterIdle)
		{
			// Recording has stopped and the file is being finalized
			// Discard this frame
			result = kFrameFlushed;
			goto bail;
		}
		else if (m_writerState != kWriterReady)
		{
			// The writer has not been initialized
			result = kFrameError;
			goto bail;
		}

		result = writeVideoFrame(deckLinkFrame, streamTime);

		// Don't write audio for video frames that were dropped
		if (result != kFrameComplete)
			goto bail;

		if (deckLinkAudioPacket)
			result = writeAudioPacket(deckLinkAudioPacket);
	
bail:
	if (resultHandler)
		resultHandler(result);
}

bool DeckLinkCoreMediaWriter::complete(bool save)
{
	std::unique_lock<std::mutex> lock(m_mutex);
	
	m_writerState = kWriterIdle;
	m_writingComplete = false;

	if (!m_avAssetWriter)
		return false;

	// Wait for the AVAssetWriter to complete
	[m_avAssetWriter finishWritingWithCompletionHandler:^{
		std::lock_guard<std::mutex> guard(m_mutex);
		m_writingComplete = true;
		m_avAssetWriter = nullptr;
		m_condition.notify_all();
	}];

	do
	{
		m_condition.wait(lock);
	} while (!m_writingComplete);

	if (m_avAsset)
		m_avAsset = nullptr;

	if (!save && m_filePath.length() > 0)
	{
		NSURL* tempURL = [NSURL fileURLWithPath:[[NSString alloc] initWithUTF8String:m_filePath.c_str()]];
		[[NSFileManager defaultManager] removeItemAtURL:[tempURL URLByDeletingLastPathComponent] error:nil];
	}

	return true;
}

std::string DeckLinkCoreMediaWriter::filePath() const
{
	return m_filePath;
}
