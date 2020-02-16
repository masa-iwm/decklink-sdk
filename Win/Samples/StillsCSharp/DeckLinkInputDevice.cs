﻿/* -LICENSE-START-
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
using System;
using DeckLinkAPI;
using System.Collections.Generic;

namespace StillsCSharp
{
    class DeckLinkInputInvalidException : Exception { }
    
    #region eventargs
    public class DeckLinkInputFormatChangedEventArgs : EventArgs
    {
        public readonly _BMDVideoInputFormatChangedEvents notificationEvents;
        public readonly _BMDDisplayMode displayMode;
        public readonly _BMDPixelFormat pixelFormat;

        public DeckLinkInputFormatChangedEventArgs(_BMDVideoInputFormatChangedEvents notificationEvents, _BMDDisplayMode displayMode, _BMDPixelFormat pixelFormat)
        {
            this.notificationEvents = notificationEvents;
            this.displayMode = displayMode;
            this.pixelFormat = pixelFormat;
        }

    }

    public class DeckLinkAudioPacketArrivedEventArgs : EventArgs
    {
        public readonly IDeckLinkAudioInputPacket audioPacket;

        public DeckLinkAudioPacketArrivedEventArgs(IDeckLinkAudioInputPacket audioPacket)
        {
            this.audioPacket = audioPacket;
        }
    }

    public class DeckLinkVideoFrameArrivedEventArgs : EventArgs
    {
        public readonly IDeckLinkVideoInputFrame videoFrame;
        public readonly bool inputInvalid;

        public DeckLinkVideoFrameArrivedEventArgs(IDeckLinkVideoInputFrame videoFrame, bool inputInvalid)
        {
            this.videoFrame = videoFrame;
            this.inputInvalid = inputInvalid;
        }
    }
    #endregion

    public class DeckLinkInputDevice : DeckLinkDevice, IDeckLinkInputCallback, IEnumerable<IDeckLinkDisplayMode>
    {
        private IDeckLinkInput      m_deckLinkInput;
        private bool                m_applyDetectedInputMode = true;
        private bool                m_currentlyCapturing = false;
        private bool                m_prevInputSignalAbsent = true;

        public DeckLinkInputDevice(IDeckLink deckLink) : base(deckLink)
        {
            if (!CaptureDevice)
                throw new DeckLinkInputInvalidException();

            // Query input interface
            m_deckLinkInput = (IDeckLinkInput)deckLink;
        }

        public event EventHandler<DeckLinkAudioPacketArrivedEventArgs> AudioPacketArrivedHandler;
        public event EventHandler<DeckLinkInputFormatChangedEventArgs> InputFormatChangedHandler;
        public event EventHandler<DeckLinkVideoFrameArrivedEventArgs> VideoFrameArrivedHandler;

        public IDeckLinkInput deckLinkInput
        {
            get { return m_deckLinkInput; }
        }

        public bool isCapturing
        {
            get { return m_currentlyCapturing; }
        }

        private _BMDVideoInputFlags InputFlags
        {
            get { return m_applyDetectedInputMode ? _BMDVideoInputFlags.bmdVideoInputEnableFormatDetection : _BMDVideoInputFlags.bmdVideoInputFlagDefault; }
        }

        public bool IsVideoModeSupported(IDeckLinkDisplayMode displayMode, _BMDPixelFormat pixelFormat)
        {
            int supported = 0;

            try
            {
                m_deckLinkInput.DoesSupportVideoMode((_BMDVideoConnection)0, displayMode.GetDisplayMode(), pixelFormat, _BMDVideoInputConversionMode.bmdNoVideoInputConversion, _BMDSupportedVideoModeFlags.bmdSupportedVideoModeDefault, null, out supported);
            }
            catch (Exception)
            {
                supported = 0;
            }

            return (supported != 0);
        }

        void IDeckLinkInputCallback.VideoInputFormatChanged(_BMDVideoInputFormatChangedEvents notificationEvents, IDeckLinkDisplayMode newDisplayMode, _BMDDetectedVideoInputFormatFlags detectedSignalFlags)
        {
            // Restart capture with the new video mode if told to
            if (! m_applyDetectedInputMode)
                return;

            var pixelFormat = _BMDPixelFormat.bmdFormat8BitYUV;
            if (detectedSignalFlags.HasFlag(_BMDDetectedVideoInputFormatFlags.bmdDetectedVideoInputRGB444))
                pixelFormat = _BMDPixelFormat.bmdFormat8BitBGRA;

            // Stop the capture
            m_deckLinkInput.StopStreams();

            var displayMode = newDisplayMode.GetDisplayMode();

            // Set the video input mode
            m_deckLinkInput.EnableVideoInput(displayMode, pixelFormat, _BMDVideoInputFlags.bmdVideoInputEnableFormatDetection);

            // Start the capture
            m_deckLinkInput.StartStreams();

            // Register input format changed event
            var handler = InputFormatChangedHandler;

            // Check whether there are any subscribers to InputFormatChangedHandler
            if (handler != null)
            {
                handler(this, new DeckLinkInputFormatChangedEventArgs(notificationEvents, displayMode, pixelFormat));
            } 
        }

        void IDeckLinkInputCallback.VideoInputFrameArrived(IDeckLinkVideoInputFrame videoFrame, IDeckLinkAudioInputPacket audioPacket)
        {
            if (videoFrame != null)
            {
                bool inputSignalAbsent = videoFrame.GetFlags().HasFlag(_BMDFrameFlags.bmdFrameHasNoInputSource);

                // Detect change in input signal, restart stream when valid stream detected 
                if (!inputSignalAbsent && m_prevInputSignalAbsent)
                {
                    m_deckLinkInput.StopStreams();
                    m_deckLinkInput.FlushStreams();
                    m_deckLinkInput.StartStreams();
                }
                m_prevInputSignalAbsent = inputSignalAbsent;

                // Register video frame received event
                var handler = VideoFrameArrivedHandler;

                // Check whether there are any subscribers to VideoFrameArrivedHandler
                if (handler != null)
                {
                    handler(this, new DeckLinkVideoFrameArrivedEventArgs(videoFrame, inputSignalAbsent));
                }
            }

            if (audioPacket != null)
            {
                // Register audio packet received event
                var handler = AudioPacketArrivedHandler;

                // Check whether there are any subscribers to AudioPacketArrivedHandler
                if (handler != null)
                {
                    handler(this, new DeckLinkAudioPacketArrivedEventArgs(audioPacket));
                }
            }

            System.Runtime.InteropServices.Marshal.ReleaseComObject(videoFrame);
        }

        IEnumerator<IDeckLinkDisplayMode> IEnumerable<IDeckLinkDisplayMode>.GetEnumerator()
        {
            IDeckLinkDisplayModeIterator displayModeIterator;
            m_deckLinkInput.GetDisplayModeIterator(out displayModeIterator);
            return new DisplayModeEnum(displayModeIterator);
        }

        System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
        {
            throw new InvalidOperationException();
        }

        public void StartCapture(IDeckLinkDisplayMode displayMode, IDeckLinkScreenPreviewCallback screenPreviewCallback, bool applyDetectedInputMode)
        {
            if (m_currentlyCapturing)
                return;

            var videoInputFlags = _BMDVideoInputFlags.bmdVideoInputFlagDefault;

            m_applyDetectedInputMode = applyDetectedInputMode;
            m_prevInputSignalAbsent = true;

            // Enable input video mode detection if the device supports it
            if (SupportsFormatDetection && m_applyDetectedInputMode)
                videoInputFlags |= _BMDVideoInputFlags.bmdVideoInputEnableFormatDetection;

            // Set the screen preview
            if (screenPreviewCallback != null)
                m_deckLinkInput.SetScreenPreviewCallback(screenPreviewCallback);

            // Set capture callback
            m_deckLinkInput.SetCallback(this);

            // Set the video input mode
            m_deckLinkInput.EnableVideoInput(displayMode.GetDisplayMode(), _BMDPixelFormat.bmdFormat8BitYUV, videoInputFlags);

            // Start the capture
            m_deckLinkInput.StartStreams();

            m_currentlyCapturing = true;
        }

        public void StopCapture()
        {
            if (!m_currentlyCapturing)
                return;

            RemoveAllListeners();

            // Stop the capture
            m_deckLinkInput.StopStreams();

            // Disable video input
            m_deckLinkInput.DisableVideoInput();

            // Disable callbacks
            m_deckLinkInput.SetScreenPreviewCallback(null);
            m_deckLinkInput.SetCallback(null);

            m_currentlyCapturing = false;
        }

        void RemoveAllListeners()
        {
            AudioPacketArrivedHandler = null;
            InputFormatChangedHandler = null;
            VideoFrameArrivedHandler = null;
        }
    }
}
