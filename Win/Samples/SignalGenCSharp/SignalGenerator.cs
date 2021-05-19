﻿/* -LICENSE-START-
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
using System;
using System.Windows.Forms;

using System.Runtime.InteropServices;
using DeckLinkAPI;
using System.Collections.Generic;
using System.Linq;

namespace SignalGenCSharp
{
    public partial class SignalGenerator : Form
    {
        enum OutputSignal
        {
            kOutputSignalPip = 0,
            kOutputSignalDrop = 1
        };
        const uint kAudioWaterlevel = 48000;
        private IReadOnlyList<int> kAudioChannels = new List<int> {2, 8, 16};

        private IReadOnlyList<StringObjectPair<_BMDPixelFormat>> kPixelFormatList = new List<StringObjectPair<_BMDPixelFormat>>
        {
            new StringObjectPair<_BMDPixelFormat>("8-Bit YUV", _BMDPixelFormat.bmdFormat8BitYUV),
            new StringObjectPair<_BMDPixelFormat>("10-Bit YUV", _BMDPixelFormat.bmdFormat10BitYUV),
            new StringObjectPair<_BMDPixelFormat>("8-Bit RGB", _BMDPixelFormat.bmdFormat8BitARGB),
            new StringObjectPair<_BMDPixelFormat>("10-Bit RGB", _BMDPixelFormat.bmdFormat10BitRGB),
        };
    
        private bool m_running;

        private DeckLinkDeviceDiscovery m_deckLinkDiscovery;
        private DeckLinkOutputDevice m_selectedDevice;
        private IDeckLinkDisplayMode m_selectedDisplayMode;
        //
        private int m_frameWidth;
        private int m_frameHeight;
        private long m_frameDuration;
        private long m_frameTimescale;
        private uint m_framesPerSecond;
        private IDeckLinkMutableVideoFrame m_videoFrameBlack;
        private IDeckLinkMutableVideoFrame m_videoFrameBars;
        private uint m_totalFramesScheduled;
        //
        private OutputSignal m_outputSignal;
        private IntPtr m_audioBuffer;
        private uint m_audioBufferOffset;
        private uint m_audioBufferSampleLength;
        private uint m_audioChannelCount;
        private _BMDAudioSampleRate m_audioSampleRate;
        private _BMDAudioSampleType m_audioSampleDepth;
        private _BMDPixelFormat m_selectedPixelFormat;

        public SignalGenerator()
        {
            InitializeComponent();

            m_running = false;

            m_deckLinkDiscovery = new DeckLinkDeviceDiscovery();

            m_deckLinkDiscovery.DeviceArrived += new DeckLinkDiscoveryHandler((d) => this.Invoke((Action)(() => AddDevice(d))));
            m_deckLinkDiscovery.DeviceRemoved += new DeckLinkDiscoveryHandler((d) => this.Invoke((Action)(() => RemoveDevice(d))));

            InitDialog();

            previewWindow.InitD3D();
        }

        private void InitDialog()
        {
            // Output signal combo box
            comboBoxOutputSignal.BeginUpdate();
            comboBoxOutputSignal.Items.Clear();
            comboBoxOutputSignal.Items.Add(new StringObjectPair<OutputSignal>("Pip", OutputSignal.kOutputSignalPip));
            comboBoxOutputSignal.Items.Add(new StringObjectPair<OutputSignal>("Drop", OutputSignal.kOutputSignalDrop));
            comboBoxOutputSignal.EndUpdate();

            // Audio depth combo box
            comboBoxAudioDepth.BeginUpdate();
            comboBoxAudioDepth.Items.Clear();
            comboBoxAudioDepth.Items.Add(new StringObjectPair<_BMDAudioSampleType>("16 Bit", _BMDAudioSampleType.bmdAudioSampleType16bitInteger));
            comboBoxAudioDepth.Items.Add(new StringObjectPair<_BMDAudioSampleType>("32 Bit", _BMDAudioSampleType.bmdAudioSampleType32bitInteger));
            comboBoxAudioDepth.EndUpdate();

            comboBoxOutputSignal.SelectedIndex = 0;
            comboBoxAudioDepth.SelectedIndex = 0;
        }

        void AddDevice(IDeckLink decklinkDevice)
        {
            DeckLinkOutputDevice deckLink = new DeckLinkOutputDevice(decklinkDevice);

            if (deckLink.deckLinkOutput != null)
            {
                comboBoxOutputDevice.BeginUpdate();
                comboBoxOutputDevice.Items.Add(new StringObjectPair<DeckLinkOutputDevice>(deckLink.deviceName, deckLink));
                comboBoxOutputDevice.EndUpdate();

                if (comboBoxOutputDevice.Items.Count == 1)
                {
                    comboBoxOutputDevice.SelectedIndex = 0;

                    EnableInterface(true);
                }
            }
        }

        void RemoveDevice(IDeckLink decklinkDevice)
        {
            // Stop capture if the selected device was removed
            if (m_selectedDevice != null && m_selectedDevice.deckLink == decklinkDevice && m_running)
            {
                // Stop running and disable output, we will not receive ScheduledPlaybackHasStopped callback
                StopRunning();
                DisableOutput();
            }

            // Remove the device from the dropdown
            comboBoxOutputDevice.BeginUpdate();
            foreach (StringObjectPair<DeckLinkOutputDevice> item in comboBoxOutputDevice.Items)
            {
                if (item.value.deckLink == decklinkDevice)
                {
                    comboBoxOutputDevice.Items.Remove(item);
                    break;
                }
            }
            comboBoxOutputDevice.EndUpdate();

            if (comboBoxOutputDevice.Items.Count == 0)
            {
                EnableInterface(false);
                m_selectedDevice = null;
            }
            else if (m_selectedDevice.deckLink == decklinkDevice)
            {
                comboBoxOutputDevice.SelectedIndex = 0;
                buttonStartStop.Enabled = true;
            }
        }


        private void SignalGenerator_Load(object sender, EventArgs e)
        {
            EnableInterface(false);

            m_deckLinkDiscovery.Enable();
        }

        private void buttonStartStop_Click(object sender, EventArgs e)
        {
            if (m_running)
                StopRunning();
            else
                StartRunning();
        }

        private void StartRunning()
        {
            m_selectedDevice.VideoFrameCompleted += new DeckLinkVideoOutputHandler((b) => this.BeginInvoke((Action)(() => { ScheduleNextFrame(b); })));
            m_selectedDevice.AudioOutputRequested += new DeckLinkAudioOutputHandler(() => this.BeginInvoke((Action)(() => { WriteNextAudioSamples(); })));
            m_selectedDevice.PlaybackStopped += new DeckLinkPlaybackStoppedHandler(() => this.BeginInvoke((Action)(() => { DisableOutput(); })));

            m_outputSignal = ((StringObjectPair<OutputSignal>)comboBoxOutputSignal.SelectedItem).value;
            m_audioChannelCount = (uint)((int)comboBoxAudioChannels.SelectedItem);
            m_audioSampleDepth = ((StringObjectPair<_BMDAudioSampleType>)comboBoxAudioDepth.SelectedItem).value;
            m_audioSampleRate = _BMDAudioSampleRate.bmdAudioSampleRate48kHz;
            //
            //- Extract the IDeckLinkDisplayMode from the display mode popup menu
            m_frameWidth = m_selectedDisplayMode.GetWidth();
            m_frameHeight = m_selectedDisplayMode.GetHeight();
            m_selectedDisplayMode.GetFrameRate(out m_frameDuration, out m_frameTimescale);
            // Calculate the number of frames per second, rounded up to the nearest integer.  For example, for NTSC (29.97 FPS), framesPerSecond == 30.
            m_framesPerSecond = (uint)((m_frameTimescale + (m_frameDuration-1))  /  m_frameDuration);

            // Set the video output mode
            m_selectedDevice.deckLinkOutput.EnableVideoOutput(m_selectedDisplayMode.GetDisplayMode(), _BMDVideoOutputFlags.bmdVideoOutputFlagDefault);

            // Set the audio output mode
            m_selectedDevice.deckLinkOutput.EnableAudioOutput(m_audioSampleRate, m_audioSampleDepth, m_audioChannelCount, _BMDAudioOutputStreamType.bmdAudioOutputStreamContinuous);

            // Set screen preview callback
            m_selectedDevice.deckLinkOutput.SetScreenPreviewCallback(previewWindow);

            // Generate one second of audio
            m_audioBufferSampleLength = (uint)((m_framesPerSecond * (uint)m_audioSampleRate * m_frameDuration) / m_frameTimescale);
            m_audioBuffer = Marshal.AllocCoTaskMem((int)(m_audioBufferSampleLength * m_audioChannelCount * ((uint)m_audioSampleDepth / 8)));

	        // Zero the buffer (interpreted as audio silence)
            for (int i = 0; i < (m_audioBufferSampleLength * m_audioChannelCount * (uint)m_audioSampleDepth / 8); i++)
                Marshal.WriteInt32(m_audioBuffer, i, 0);
	        uint audioSamplesPerFrame = (uint)(((uint)m_audioSampleRate * m_frameDuration) / m_frameTimescale);

            if (m_outputSignal == OutputSignal.kOutputSignalPip)
                FillSine(m_audioBuffer, audioSamplesPerFrame, m_audioChannelCount, m_audioSampleDepth);
            else
                FillSine(new IntPtr(m_audioBuffer.ToInt64() + (audioSamplesPerFrame * m_audioChannelCount * (uint)m_audioSampleDepth / 8)), (m_audioBufferSampleLength - audioSamplesPerFrame), m_audioChannelCount, m_audioSampleDepth);

            // Generate a frame of black
            m_videoFrameBlack = CreateOutputVideoFrame(FillBlack);

            // Generate a frame of colour bars
            m_videoFrameBars = CreateOutputVideoFrame(FillColourBars);

            // Begin video preroll by scheduling a second of frames in hardware
            m_totalFramesScheduled = 0;
            for (uint i = 0; i < m_framesPerSecond; i++)
                ScheduleNextFrame(true);

            // Begin audio preroll.  This will begin calling our audio callback, which will start the DeckLink output stream.
            m_audioBufferOffset = 0;
            m_selectedDevice.deckLinkOutput.BeginAudioPreroll();

            m_running = true;
            buttonStartStop.Text = "Stop";
        }

        private IDeckLinkMutableVideoFrame CreateOutputVideoFrame(Action<IDeckLinkVideoFrame> fillFrame)
        {
            IDeckLinkMutableVideoFrame  referenceFrame = null;
            IDeckLinkMutableVideoFrame  scheduleFrame = null;
            IDeckLinkVideoConversion    frameConverter = new CDeckLinkVideoConversion();

            m_selectedDevice.deckLinkOutput.CreateVideoFrame(m_frameWidth, m_frameHeight, BytesPerRow, m_selectedPixelFormat, _BMDFrameFlags.bmdFrameFlagDefault, out scheduleFrame);
            if (m_selectedPixelFormat == _BMDPixelFormat.bmdFormat8BitYUV)
            {
                // Fill 8-bit YUV directly without conversion
                fillFrame(scheduleFrame);
            }
            else
            {
                // Pixel formats are different, first generate 8-bit YUV bars frame and convert to required format
                m_selectedDevice.deckLinkOutput.CreateVideoFrame(m_frameWidth, m_frameHeight, m_frameWidth * 2, _BMDPixelFormat.bmdFormat8BitYUV, _BMDFrameFlags.bmdFrameFlagDefault, out referenceFrame);
                fillFrame(referenceFrame);
                frameConverter.ConvertFrame(referenceFrame, scheduleFrame);
            }

            return scheduleFrame;
        }

        private void StopRunning()
        {
            long unused;
            m_selectedDevice.deckLinkOutput.StopScheduledPlayback(0, out unused, 100);

            m_running = false;
        }

        private void DisableOutput()
        {
            m_selectedDevice.deckLinkOutput.SetScreenPreviewCallback(null);

            m_selectedDevice.deckLinkOutput.DisableAudioOutput();
            m_selectedDevice.deckLinkOutput.DisableVideoOutput();
            m_selectedDevice.RemoveAllListeners();

            // free audio buffer
            Marshal.FreeCoTaskMem(m_audioBuffer);

            buttonStartStop.Text = "Start";
        }

        private void ScheduleNextFrame(bool prerolling)
        {
            if (prerolling == false)
            {
                // If not prerolling, make sure that playback is still active
                if (m_running == false)
                    return;
            }

            if (m_outputSignal == OutputSignal.kOutputSignalPip)
            {
                if ((m_totalFramesScheduled % m_framesPerSecond) == 0)
                {
                    // On each second, schedule a frame of bars
                    m_selectedDevice.deckLinkOutput.ScheduleVideoFrame(m_videoFrameBars, (m_totalFramesScheduled * m_frameDuration), m_frameDuration, m_frameTimescale);
                }
                else
                {
                    // Schedue frames of black
                    m_selectedDevice.deckLinkOutput.ScheduleVideoFrame(m_videoFrameBlack, (m_totalFramesScheduled * m_frameDuration), m_frameDuration, m_frameTimescale);
                }
            }
            else
            {
                if ((m_totalFramesScheduled % m_framesPerSecond) == 0)
                {
                    // On each second, schedule a frame of black
                    m_selectedDevice.deckLinkOutput.ScheduleVideoFrame(m_videoFrameBlack, (m_totalFramesScheduled * m_frameDuration), m_frameDuration, m_frameTimescale);
                }
                else
                {
                    // Schedue frames of color bars
                    m_selectedDevice.deckLinkOutput.ScheduleVideoFrame(m_videoFrameBars, (m_totalFramesScheduled * m_frameDuration), m_frameDuration, m_frameTimescale);
                }
            }

            m_totalFramesScheduled += 1;
        }

        void WriteNextAudioSamples()
        {
            // Write one second of audio to the DeckLink API.
            uint bufferedSamples;

            // Make sure that playback is still active
            if (m_running == false)
                return;

	        // Try to maintain the number of audio samples buffered in the API at a specified waterlevel
            m_selectedDevice.deckLinkOutput.GetBufferedAudioSampleFrameCount(out bufferedSamples);
            if (bufferedSamples < kAudioWaterlevel)
            {
                uint samplesToEndOfBuffer;
                uint samplesToWrite;
                uint samplesWritten;

                samplesToEndOfBuffer = (m_audioBufferSampleLength - m_audioBufferOffset);
                samplesToWrite = (kAudioWaterlevel - bufferedSamples);
                if (samplesToWrite > samplesToEndOfBuffer)
                    samplesToWrite = samplesToEndOfBuffer;

                m_selectedDevice.deckLinkOutput.ScheduleAudioSamples(new IntPtr(m_audioBuffer.ToInt64() + (m_audioBufferOffset * m_audioChannelCount * (uint)m_audioSampleDepth / 8)),
                    samplesToWrite, 0, 0, out samplesWritten);
                m_audioBufferOffset = ((m_audioBufferOffset + samplesWritten) % m_audioBufferSampleLength);
            }
        }

        private void DisplayModeChanged(IDeckLinkDisplayMode newDisplayMode)
        {
            foreach (DisplayModeEntry item in comboBoxVideoFormat.Items)
            {
                if (item.displayMode.GetDisplayMode() == newDisplayMode.GetDisplayMode())
                    comboBoxVideoFormat.SelectedItem = item;
            }
        }

        private void comboBoxOutputDevice_SelectedValueChanged(object sender, EventArgs e)
        {
            m_selectedDevice = null;

            if (comboBoxOutputDevice.SelectedIndex < 0)
                return;

            m_selectedDevice = ((StringObjectPair<DeckLinkOutputDevice>)comboBoxOutputDevice.SelectedItem).value;

            // Update the video mode popup menu
            RefreshVideoModeList();

            // Update the audio channels popup menu
            RefreshAudioChannelList();

            // Enable the interface
            EnableInterface(true);
        }

        private void comboBoxVideoFormat_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBoxVideoFormat.SelectedIndex < 0)
                return;

            m_selectedDisplayMode = ((DisplayModeEntry)comboBoxVideoFormat.SelectedItem).displayMode;

            RefreshPixelFormatList();
        }


        private void comboBoxPixelFormat_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBoxPixelFormat.SelectedIndex < 0)
                return;

            m_selectedPixelFormat = ((StringObjectPair<_BMDPixelFormat>)comboBoxPixelFormat.SelectedItem).value;
        }

        private void EnableInterface(bool enabled)
        {
            comboBoxOutputDevice.Enabled = enabled;
            comboBoxVideoFormat.Enabled = enabled;
            buttonStartStop.Enabled = enabled;
       }

        private void RefreshVideoModeList()
        {
            if (m_selectedDevice != null)
            {
                comboBoxVideoFormat.BeginUpdate();
                comboBoxVideoFormat.Items.Clear();

                foreach (IDeckLinkDisplayMode displayMode in m_selectedDevice)
                    comboBoxVideoFormat.Items.Add(new DisplayModeEntry(displayMode));

                comboBoxVideoFormat.SelectedIndex = 0;
                comboBoxVideoFormat.EndUpdate();
            }

            // Refresh pixel format list
            RefreshPixelFormatList();
        }

        private void RefreshPixelFormatList()
        {
            comboBoxPixelFormat.BeginUpdate();
            comboBoxPixelFormat.Items.Clear();

            foreach (StringObjectPair<_BMDPixelFormat> pixelFormat in kPixelFormatList.Where((pf, ret) => { return (m_selectedDevice.IsVideoModeSupported(((DisplayModeEntry)comboBoxVideoFormat.SelectedItem).displayMode, pf.value)); }))
                comboBoxPixelFormat.Items.Add(pixelFormat);

            comboBoxPixelFormat.SelectedIndex = 0;
            comboBoxPixelFormat.EndUpdate();
        }

        private void RefreshAudioChannelList()
        {
            if (m_selectedDevice != null)
            {
                long maxAudioChannels;

                var deckLinkAttributes = (IDeckLinkProfileAttributes)m_selectedDevice.deckLink;
                deckLinkAttributes.GetInt(_BMDDeckLinkAttributeID.BMDDeckLinkMaximumAudioChannels, out maxAudioChannels);

                comboBoxAudioChannels.BeginUpdate();
                comboBoxAudioChannels.Items.Clear();

                foreach (int channels in kAudioChannels)
                {
                    if (channels <= maxAudioChannels)
                    {
                        comboBoxAudioChannels.Items.Add(channels);
                    }
                }

                comboBoxAudioChannels.SelectedIndex = comboBoxAudioChannels.Items.Count - 1;
                comboBoxAudioChannels.EndUpdate();
            
            }
        }

        private int BytesPerRow
        {
            get
            {
                int bytesPerRow;

                // Refer to DeckLink SDK Manual - 2.7.4 Pixel Formats
                switch (m_selectedPixelFormat)
                {
                    case _BMDPixelFormat.bmdFormat8BitYUV:
                        bytesPerRow = m_frameWidth * 2;
                        break;

                    case _BMDPixelFormat.bmdFormat10BitYUV:
                        bytesPerRow = ((m_frameWidth + 47) / 48) * 128;
                        break;

                    case _BMDPixelFormat.bmdFormat10BitRGB:
                        bytesPerRow = ((m_frameWidth + 63) / 64) * 256;
                        break;

                    case _BMDPixelFormat.bmdFormat8BitARGB:
                    case _BMDPixelFormat.bmdFormat8BitBGRA:
                    default:
                        bytesPerRow = m_frameWidth * 4;
                        break;
                }

                return bytesPerRow; 
            }
        }


        #region buffer filling
        /*****************************************/

        void FillSine(IntPtr audioBuffer, uint samplesToWrite, uint channels, _BMDAudioSampleType sampleDepth)
        {
            if ((uint)sampleDepth == 16)
            {
                Int16[] buffer = new Int16[channels * samplesToWrite];

                for (uint i = 0; i < samplesToWrite; i++)
                {
                    Int16 sample = (Int16)(24576.0 * Math.Sin((i * 2.0 * Math.PI) / 48.0));
                    for (uint ch = 0; ch < channels; ch++)
                    {
                        buffer[i * channels + ch] = sample;
                    }
                }
                // Copy it into unmanaged buffer
                Marshal.Copy(buffer, 0, audioBuffer, (int)(channels * samplesToWrite));
            }
            else if ((uint)sampleDepth == 32)
            {
                Int32[] buffer = new Int32[channels * samplesToWrite];

                for (uint i = 0; i < samplesToWrite; i++)
                {
                    Int32 sample = (Int32)(1610612736.0 * Math.Sin((i * 2.0 * Math.PI) / 48.0));
                    for (uint ch = 0; ch < channels; ch++)
                    {
                        buffer[i * channels + ch] = sample;
                    }
                }
                // Copy it into unmanaged buffer
                Marshal.Copy(buffer, 0, audioBuffer, (int)(channels * samplesToWrite));

            }
        }

        void FillColourBars(IDeckLinkVideoFrame theFrame)
        {
            IntPtr          buffer;
            int             width, height;
            UInt32[]        bars = {0xEA80EA80, 0xD292D210, 0xA910A9A5, 0x90229035, 0x6ADD6ACA, 0x51EF515A, 0x286D28EF, 0x10801080};
            int             index = 0;

            theFrame.GetBytes(out buffer);
            width = theFrame.GetWidth();
            height = theFrame.GetHeight();

            for (uint y = 0; y < height; y++)
            {
                for (uint x = 0; x < width; x += 2)
                {
                    // Write directly into unmanaged buffer
                    Marshal.WriteInt32(buffer, index * 4, (Int32)bars[(x * 8) / width]);
                    index++;
                }
            }
        }

        void FillBlack(IDeckLinkVideoFrame theFrame)
        {
            IntPtr buffer;
            int             width, height;
            int             wordsRemaining;
            UInt32          black = 0x10801080;
            int             index = 0;

            theFrame.GetBytes(out buffer);
            width = theFrame.GetWidth();
            height = theFrame.GetHeight();

            wordsRemaining = (width * 2 * height) / 4;

            while (wordsRemaining-- > 0)
            {
                Marshal.WriteInt32(buffer, index*4, (Int32)black);
                index++;
            }
        }

        #endregion

        /// <summary>
        /// Used for putting the IDeckLinkDisplayMode objects into the video format
        /// combo box.
        /// </summary>
        struct DisplayModeEntry
        {
            public IDeckLinkDisplayMode displayMode;

            public DisplayModeEntry(IDeckLinkDisplayMode displayMode)
            {
                this.displayMode = displayMode;
            }

            public override string ToString()
            {
                string str;

                displayMode.GetName(out str);

                return str;
            }
        }

        /// <summary>
        /// Used for putting other object types into combo boxes.
        /// </summary>
        struct StringObjectPair<T>
        {
            public string name;
            public T value;

            public StringObjectPair(string name, T value)
            {
                this.name = name;
                this.value = value;
            }

            public override string ToString()
            {
                return name;
            }
        }
    }
}
