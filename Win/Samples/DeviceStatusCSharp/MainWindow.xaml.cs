/* -LICENSE-START-
** Copyright (c) 2020 Blackmagic Design
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
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Threading;

using DeckLinkAPI;

namespace DeviceStatusCSharp
{
	/// <summary>
	/// Interaction logic for MainWindow.xaml
	/// </summary>
	public partial class MainWindow : Window
	{
		private Thread deckLinkMainThread;
		private readonly EventWaitHandle applicationCloseWaitHandle;

		private IDeckLink selectedDeckLink;
		private DeckLinkDeviceDiscovery deckLinkDeviceDiscovery;
		private ProfileCallback profileCallback;
		private NotificationCallback notificationCallback;

		private IReadOnlyList<IStatusItem> StatusItemList = new List<IStatusItem>
		{
			new StatusItemVideoInputMode("Detected video input display mode",         _BMDDeckLinkStatusID.bmdDeckLinkStatusDetectedVideoInputMode),
			new StatusItemVideoInputFormatFlags("Detected video input format flags",  _BMDDeckLinkStatusID.bmdDeckLinkStatusDetectedVideoInputFormatFlags),
			new StatusItemFieldDominance("Detected video input field dominance",      _BMDDeckLinkStatusID.bmdDeckLinkStatusDetectedVideoInputFieldDominance),
			new StatusItemColorspace("Detected video input colorspace",               _BMDDeckLinkStatusID.bmdDeckLinkStatusDetectedVideoInputColorspace),
			new StatusItemDynamicRange("Detected video input dynamic range",          _BMDDeckLinkStatusID.bmdDeckLinkStatusDetectedVideoInputDynamicRange),
			new StatusItemLinkConfiguration("Detected SDI video input link width",    _BMDDeckLinkStatusID.bmdDeckLinkStatusDetectedSDILinkConfiguration),
			new StatusItemVideoInputMode("Video input display mode",                  _BMDDeckLinkStatusID.bmdDeckLinkStatusCurrentVideoInputMode),
			new StatusItemPixelFormat("Video input pixel format",                     _BMDDeckLinkStatusID.bmdDeckLinkStatusCurrentVideoInputPixelFormat),
			new StatusItemVideoStatus("Video input flags",                            _BMDDeckLinkStatusID.bmdDeckLinkStatusCurrentVideoInputFlags),
			new StatusItemVideoOutputMode("Video output display mode",                _BMDDeckLinkStatusID.bmdDeckLinkStatusCurrentVideoOutputMode),
			new StatusItemVideoStatus("Video output flags",                           _BMDDeckLinkStatusID.bmdDeckLinkStatusCurrentVideoOutputFlags),
			new StatusItemInt("PCIe link width",                                      _BMDDeckLinkStatusID.bmdDeckLinkStatusPCIExpressLinkWidth),
			new StatusItemInt("PCIe link speed",                                      _BMDDeckLinkStatusID.bmdDeckLinkStatusPCIExpressLinkSpeed),
			new StatusItemPixelFormat("Video output pixel format",                    _BMDDeckLinkStatusID.bmdDeckLinkStatusLastVideoOutputPixelFormat),
			new StatusItemVideoOutputMode("Detected reference video mode",            _BMDDeckLinkStatusID.bmdDeckLinkStatusReferenceSignalMode),
			new StatusItemBusy("Busy state",                                          _BMDDeckLinkStatusID.bmdDeckLinkStatusBusy),
			new StatusItemBool("Video input locked",                                  _BMDDeckLinkStatusID.bmdDeckLinkStatusVideoInputSignalLocked),
			new StatusItemBool("Reference input locked",                              _BMDDeckLinkStatusID.bmdDeckLinkStatusReferenceSignalLocked),
			new StatusItemVideoStatus("Reference input video flags",                  _BMDDeckLinkStatusID.bmdDeckLinkStatusReferenceSignalFlags),
			new StatusItemPanel("Panel Installed",                                    _BMDDeckLinkStatusID.bmdDeckLinkStatusInterchangeablePanelType),
			new StatusItemBytes("Received EDID of connected HDMI sink",               _BMDDeckLinkStatusID.bmdDeckLinkStatusReceivedEDID),
			new StatusItemInt("On-board temperature (°C)",                            _BMDDeckLinkStatusID.bmdDeckLinkStatusDeviceTemperature),
		};

		private readonly IReadOnlyDictionary<_BMDDuplexMode, string> DuplexModeDictionary = new Dictionary<_BMDDuplexMode, string>
		{
			[_BMDDuplexMode.bmdDuplexFull] = "Full Duplex",
			[_BMDDuplexMode.bmdDuplexHalf] = "Half Duplex",
			[_BMDDuplexMode.bmdDuplexSimplex] = "Simplex",
			[_BMDDuplexMode.bmdDuplexInactive] = "Inactive",
		};

		public class StatusData
		{
			public _BMDDeckLinkStatusID ID { get; set; }
			public string Item { get; set; }
			public string Value { get; set; }
		}

		public ObservableCollection<StatusData> statusDataCollection;

		private static void UpdateUIElement(DispatcherObject element, Action action)
		{
			if (element == null)
				return;

			if (!element.Dispatcher.CheckAccess())
				element.Dispatcher.BeginInvoke(DispatcherPriority.Normal, action);
			else
				action();
		}

		public MainWindow()
		{
			InitializeComponent();
			applicationCloseWaitHandle = new EventWaitHandle(false, EventResetMode.AutoReset);
		}

		#region dl_events
		// All events occur in MTA threading context
		public void AddDevice(object sender, DeckLinkDiscoveryEventArgs e)
		{
			// Get device display name
			e.deckLink.GetDisplayName(out string displayName);

			// Update combo box with new device
			UpdateUIElement(comboBoxDevice, new Action(() => UpdateComboNewDevice(displayName, e.deckLink)));
		}

		public void RemoveDevice(object sender, DeckLinkDiscoveryEventArgs e)
		{
			// Remove device from combo box
			UpdateUIElement(comboBoxDevice, new Action(() => UpdateComboRemoveDevice(e.deckLink)));
		}

		public void ProfileActivated(object sender, DeckLinkProfileEventArgs e)
		{
			long duplexMode = (long)_BMDDuplexMode.bmdDuplexInactive;

			// Get duplex mode for the device
			var deckLinkAttributes = selectedDeckLink as IDeckLinkProfileAttributes;
			deckLinkAttributes.GetInt(_BMDDeckLinkAttributeID.BMDDeckLinkDuplex, out duplexMode);

			UpdateUIElement(comboBoxDevice, new Action(() => UpdateLabelDuplexMode((_BMDDuplexMode) duplexMode)));
	}

		public void StatusChanged(object sender, DeckLinkStatusChangedEventArgs e)
		{
			var statusItem = StatusItemList.Single(si => si.GetID() == e.statusID );
			var deckLinkStatus = selectedDeckLink as IDeckLinkStatus;
			var statusValue = statusItem.GetValue(deckLinkStatus);

			UpdateUIElement(dataGridStatus, new Action(() =>
			{
				var item = statusDataCollection.FirstOrDefault(entry => entry.ID == e.statusID);

				if (item != null)
				{
					if (String.IsNullOrEmpty(statusValue))
						// Remove status ID from collection
						statusDataCollection.Remove(item);
					else
						// Update status ID with new value
						item.Value = statusValue;
				}
				else if (!String.IsNullOrEmpty(statusValue))
				{
					// Add new status ID
					item = new StatusData()
					{
						ID = statusItem.GetID(),
						Item = statusItem.ToString(),
						Value = statusValue
					};
					statusDataCollection.Add(item);
				}

				dataGridStatus.ItemsSource = statusDataCollection;
				dataGridStatus.Items.Refresh();
			}));
		}
#endregion

#region uievents
		// All UI events are in STA apartment thread context, calls to DeckLinkAPI must be performed in MTA thread context
		private void Window_Loaded(object sender, RoutedEventArgs e)
		{
			deckLinkMainThread = new Thread(() => DeckLinkMainThread());
			deckLinkMainThread.SetApartmentState(ApartmentState.MTA);
			deckLinkMainThread.Start();
		}

		private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
		{
			applicationCloseWaitHandle.Set();
			deckLinkMainThread.Join();
		}

		private void ComboBoxDevice_SelectionChanged(object sender, SelectionChangedEventArgs e)
		{
			long duplexMode = (long)_BMDDuplexMode.bmdDuplexInactive;

			if (selectedDeckLink != null)
			{
				var mtaThread = new MTAAction(() =>
				{
					// Unsubscribe to profile callback
					var deckLinkProfileManager = selectedDeckLink as IDeckLinkProfileManager;
					if (deckLinkProfileManager != null)
					{
						deckLinkProfileManager.SetCallback(null);
					}

					// Unsubscribe to status changed callback
					var deckLinkNotification = selectedDeckLink as IDeckLinkNotification;
					deckLinkNotification.Unsubscribe(_BMDNotifications.bmdStatusChanged, notificationCallback);
				});
			}

			selectedDeckLink = null;

			if (comboBoxDevice.SelectedIndex < 0)
				return;

			selectedDeckLink = ((StringObjectPair<IDeckLink>)((ComboBoxItem)comboBoxDevice.SelectedItem).Content).Value;

			if (selectedDeckLink != null)
			{
				// Create new status data collection for selected device
				statusDataCollection = new ObservableCollection<StatusData>();

				var mtaThread = new MTAAction(() =>
				{
					// Subscribe the selected device to profile callback
					var deckLinkProfileManager = selectedDeckLink as IDeckLinkProfileManager;
					if (deckLinkProfileManager != null)
						deckLinkProfileManager.SetCallback(profileCallback);

					// Subscribe the selected device to status changed callback
					var deckLinkNotification = selectedDeckLink as IDeckLinkNotification;
					deckLinkNotification.Subscribe(_BMDNotifications.bmdStatusChanged, notificationCallback);

					// Get duplex mode for the device
					var deckLinkAttributes = selectedDeckLink as IDeckLinkProfileAttributes;
					deckLinkAttributes.GetInt(_BMDDeckLinkAttributeID.BMDDeckLinkDuplex, out duplexMode);

					var deckLinkStatus = selectedDeckLink as IDeckLinkStatus;
					foreach (var statusItem in StatusItemList)
					{
						var statusValue = statusItem.GetValue(deckLinkStatus);

						if (!String.IsNullOrEmpty(statusValue))
						{
							var statusData = new StatusData()
							{
								ID = statusItem.GetID(),
								Item = statusItem.ToString(),
								Value = statusValue
							};
							UpdateUIElement(dataGridStatus, new Action(() => statusDataCollection.Add(statusData)));
						}
					}
				});

				UpdateLabelDuplexMode((_BMDDuplexMode)duplexMode);
				dataGridStatus.ItemsSource = statusDataCollection;
			}
		}

		private void UpdateComboNewDevice(string displayName, IDeckLink deckLink)
		{
			ComboBoxItem newItem = new ComboBoxItem
			{
				Content = new StringObjectPair<IDeckLink>(displayName, deckLink),
			};
			comboBoxDevice.Items.Add(newItem);

			// If first device the select item
			if (comboBoxDevice.Items.Count == 1)
				comboBoxDevice.SelectedIndex = 0;
		}

		private void UpdateComboRemoveDevice(IDeckLink deckLink)
		{
			bool selectedDeviceRemoved = selectedDeckLink == deckLink;

			// Remove the device from the device dropdown
			foreach (ComboBoxItem item in comboBoxDevice.Items)
			{
				if (((StringObjectPair<IDeckLink>)item.Content).Value == deckLink)
				{
					comboBoxDevice.Items.Remove(item);
					break;
				}
			}

			if (comboBoxDevice.Items.Count == 0)
				selectedDeckLink = null;
			else if (selectedDeviceRemoved)
				comboBoxDevice.SelectedIndex = 0;
		}

		private void UpdateLabelDuplexMode(_BMDDuplexMode duplexMode)
		{
			try
			{
				textBlockDuplex.Text = DuplexModeDictionary[duplexMode];
			}
			catch (KeyNotFoundException)
			{
				textBlockDuplex.Text = "Unknown";
			}
		}
#endregion

		private void DeckLinkMainThread()
		{
			// Subscribe to events
			profileCallback = new ProfileCallback();
			profileCallback.ProfileActivated += ProfileActivated;

			notificationCallback = new NotificationCallback();
			notificationCallback.StatusChanged += StatusChanged;

			deckLinkDeviceDiscovery = new DeckLinkDeviceDiscovery();
			deckLinkDeviceDiscovery.DeviceArrived += AddDevice;
			deckLinkDeviceDiscovery.DeviceRemoved += RemoveDevice;
			deckLinkDeviceDiscovery.Enable();

			// Wait for application to close
			applicationCloseWaitHandle.WaitOne();

			// Unsubscribe from events
			profileCallback.ProfileActivated -= ProfileActivated;

			notificationCallback.StatusChanged -= StatusChanged;

			deckLinkDeviceDiscovery.Disable();
			deckLinkDeviceDiscovery.DeviceArrived -= AddDevice;
			deckLinkDeviceDiscovery.DeviceRemoved -= RemoveDevice;
		}
	}

	/// Used for putting object types into combo boxes.
	struct StringObjectPair<T>
	{
		public StringObjectPair(string name, T value)
		{
			Name = name;
			Value = value;
		}
		public string Name { get; }
		public T Value { get; set; }
		public override string ToString() => Name;
	}

	public interface IStatusItem
	{
		_BMDDeckLinkStatusID GetID();
		string GetValue(IDeckLinkStatus deckLinkStatus);
	}
	public abstract class StatusItem : IStatusItem
	/// Used to create mapping of status string to the get* function 
	{
		public StatusItem(string name, _BMDDeckLinkStatusID id)
		{
			Name = name;
			ID = id;
		}
		public string Name { get; }
		public _BMDDeckLinkStatusID ID { get; }
		protected abstract string Value(IDeckLinkStatus deckLinkStatus);
		public _BMDDeckLinkStatusID GetID() => ID;
		public string GetValue(IDeckLinkStatus deckLinkStatus) => Value(deckLinkStatus);
		public override string ToString() => Name;
	}

	internal class StatusItemBytes : StatusItem
	{
		public StatusItemBytes(string name, _BMDDeckLinkStatusID id) : base(name, id) { }

		protected override string Value(IDeckLinkStatus deckLinkStatus)
		{
			StringBuilder returnString;
			uint byteBufferSize = 0;
			IntPtr statusBytesPtr = IntPtr.Zero;

			try
			{
				// Get required size of buffer
				deckLinkStatus.GetBytes(ID, statusBytesPtr, ref byteBufferSize);

				statusBytesPtr = Marshal.AllocHGlobal((Int32)byteBufferSize);

				// Use unmanaged memory to access status byte data
				deckLinkStatus.GetBytes(ID, statusBytesPtr, ref byteBufferSize);

				byte[] byteBuffer = new byte[byteBufferSize];
				Marshal.Copy(statusBytesPtr, byteBuffer, 0, byteBuffer.Length);

				returnString = new StringBuilder(byteBuffer.Length * 3);
				for (int i = 0; i < byteBuffer.Length; ++i)
				{
					// Display bytes in groups of 16
					returnString.AppendFormat("{0:X2}", byteBuffer[i]);
					returnString.Append(i % 16 == 15 ? "\n" : " ");
				}
			}
			catch (NotImplementedException)
			{
				returnString = null;
			}
			finally
			{
				Marshal.FreeHGlobal(statusBytesPtr);
			}

			return returnString?.ToString() ?? String.Empty;
		}
	}

	internal class StatusItemPanel : StatusItem
	{
		private readonly IReadOnlyDictionary<_BMDPanelType, string> PanelDictionary = new Dictionary<_BMDPanelType, string>
		{
			[_BMDPanelType.bmdPanelNotDetected]           = "No panel detected",
			[_BMDPanelType.bmdPanelTeranexMiniSmartPanel] = "Teranex Mini panel detected",
		};

		public StatusItemPanel(string name, _BMDDeckLinkStatusID id) : base(name, id) { }

		protected override string Value(IDeckLinkStatus deckLinkStatus)
		{
			try
			{
				deckLinkStatus.GetInt(ID, out long intValue);
				return PanelDictionary[(_BMDPanelType)intValue];
			}
			catch (NotImplementedException)
			{
				return null;
			}
		}
	}

	internal class StatusItemBool : StatusItem
	{
		public StatusItemBool(string name, _BMDDeckLinkStatusID id) : base(name, id) { }

		protected override string Value(IDeckLinkStatus deckLinkStatus)
		{
			try
			{
				deckLinkStatus.GetFlag(ID, out int intValue);
				return Convert.ToBoolean(intValue).ToString();
			}
			catch (Exception)
			{
				return null;
			}
		}
	}

	internal class StatusItemBusy : StatusItem
	{
		private readonly IReadOnlyDictionary<_BMDDeviceBusyState, string> BusyStateDictionary = new Dictionary<_BMDDeviceBusyState, string>
		{
			[_BMDDeviceBusyState.bmdDeviceCaptureBusy] = "Capture active",
			[_BMDDeviceBusyState.bmdDevicePlaybackBusy] = "Playback active",
			[_BMDDeviceBusyState.bmdDeviceSerialPortBusy] = "Serial port active",
		};

		public StatusItemBusy(string name, _BMDDeckLinkStatusID id) : base(name, id) { }

		protected override string Value(IDeckLinkStatus deckLinkStatus)
		{
			string returnString;

			try
			{
				deckLinkStatus.GetInt(ID, out long intValue);
				returnString = string.Join("\n", BusyStateDictionary.Where(bs => ((_BMDDeviceBusyState)intValue).HasFlag(bs.Key)).Select(bs => bs.Value));
				return String.IsNullOrEmpty(returnString) ? "Inactive" : returnString;
			}
			catch (Exception)
			{
				return null;
			}
		}
	}

	internal class StatusItemInt : StatusItem
	{
		public StatusItemInt(string name, _BMDDeckLinkStatusID id) : base(name, id) { }

		protected override string Value(IDeckLinkStatus deckLinkStatus)
		{
			try
			{
				deckLinkStatus.GetInt(ID, out long intValue);
				return intValue.ToString();
			}
			catch (Exception)
			{
				return null;
			}
		}
	}

	internal class StatusItemVideoOutputMode : StatusItem
	{
		public StatusItemVideoOutputMode(string name, _BMDDeckLinkStatusID id) : base(name, id) { }

		protected override string Value(IDeckLinkStatus deckLinkStatus)
		{
			try
			{
				deckLinkStatus.GetInt(ID, out long intValue);

				if ((_BMDDisplayMode)intValue == _BMDDisplayMode.bmdModeUnknown)
				{
					return null;
				}

				var deckLinkOutput = deckLinkStatus as IDeckLinkOutput;
				deckLinkOutput.GetDisplayMode((_BMDDisplayMode)intValue, out IDeckLinkDisplayMode displayMode);

				displayMode.GetName(out string displayModeName);
				return displayModeName;
			}
			catch (Exception)
			{
				return null;
			}
		}
	}

	internal class StatusItemPixelFormat : StatusItem
	{
		private readonly IReadOnlyDictionary<_BMDPixelFormat, string> PixelFormatDictionary = new Dictionary<_BMDPixelFormat, string>
		{
			[_BMDPixelFormat.bmdFormat8BitYUV] = "8-bit YUV (UYVY)",
			[_BMDPixelFormat.bmdFormat10BitYUV] = "10-bit YUV (v210)",
			[_BMDPixelFormat.bmdFormat8BitARGB] = "8-bit ARGB",
			[_BMDPixelFormat.bmdFormat8BitBGRA] = "8-bit BGRA",
			[_BMDPixelFormat.bmdFormat10BitRGB] = "10-bit RGB (r210)",
			[_BMDPixelFormat.bmdFormat12BitRGB] = "12-bit RGB Big-Endian (R12B)",
			[_BMDPixelFormat.bmdFormat12BitRGBLE] = "12-bit RGB Little-Endian (R12L)",
			[_BMDPixelFormat.bmdFormat10BitRGBX] = "10-bit RGB Big-Endian (R10b)",
			[_BMDPixelFormat.bmdFormat10BitRGBXLE] = "10-bit RGB Little-Endian (R10l)",
			[_BMDPixelFormat.bmdFormatH265] = "H.265 Encoded Video Data",
			[_BMDPixelFormat.bmdFormatDNxHR] = "DNxHR Encoded Video Data",
			[_BMDPixelFormat.bmdFormat12BitRAWGRBG] = "12-Bit GRBG bayer",
			[_BMDPixelFormat.bmdFormat12BitRAWJPEG] = "12-Bit JPEG Compressed",

		};

		public StatusItemPixelFormat(string name, _BMDDeckLinkStatusID id) : base(name, id) { }

		protected override string Value(IDeckLinkStatus deckLinkStatus)
		{
			try
			{
				deckLinkStatus.GetInt(ID, out long intValue);
				return PixelFormatDictionary[(_BMDPixelFormat)intValue];
			}
			catch (Exception)
			{
				return null;
			}
		}
	}

	internal class StatusItemVideoStatus : StatusItem
	{
		private readonly IReadOnlyDictionary<_BMDDeckLinkVideoStatusFlags, string> VideoStatusDictionary = new Dictionary<_BMDDeckLinkVideoStatusFlags, string>
		{
			[_BMDDeckLinkVideoStatusFlags.bmdDeckLinkVideoStatusPsF] = "Progressive frames are PsF",
			[_BMDDeckLinkVideoStatusFlags.bmdDeckLinkVideoStatusDualStream3D] = "Dual-stream 3D video",
		};

		public StatusItemVideoStatus(string name, _BMDDeckLinkStatusID id) : base(name, id) { }

		protected override string Value(IDeckLinkStatus deckLinkStatus)
		{
			string returnString;

			try
			{
				deckLinkStatus.GetInt(ID, out long intValue);
				returnString = string.Join("\n", VideoStatusDictionary.Where(vs => ((_BMDDeckLinkVideoStatusFlags)intValue).HasFlag(vs.Key)).Select(vs => vs.Value));
				return String.IsNullOrEmpty(returnString) ? "None" : returnString;
			}
			catch (Exception)
			{
				return null;
			}
		}
	}

	internal class StatusItemVideoInputMode : StatusItem
	{
		public StatusItemVideoInputMode(string name, _BMDDeckLinkStatusID id) : base(name, id) { }

		protected override string Value(IDeckLinkStatus deckLinkStatus)
		{
			try
			{
				deckLinkStatus.GetInt(ID, out long intValue);

				if ((_BMDDisplayMode)intValue == _BMDDisplayMode.bmdModeUnknown)
				{
					return null;
				}

				var deckLinkInput = deckLinkStatus as IDeckLinkInput;
				deckLinkInput.GetDisplayMode((_BMDDisplayMode)intValue, out IDeckLinkDisplayMode displayMode);

				displayMode.GetName(out string displayModeName);
				return displayModeName;
			}
			catch (Exception)
			{
				return null;
			}
		}
	}

	internal class StatusItemVideoInputFormatFlags : StatusItem
	{
		private readonly IReadOnlyDictionary<_BMDDetectedVideoInputFormatFlags, string> VideoInputFormatFlagsDictionary = new Dictionary<_BMDDetectedVideoInputFormatFlags, string>
		{
			[_BMDDetectedVideoInputFormatFlags.bmdDetectedVideoInputYCbCr422] = "YCbCr 4:2:2",
			[_BMDDetectedVideoInputFormatFlags.bmdDetectedVideoInputRGB444] = "RGB 4:4:4",
			[_BMDDetectedVideoInputFormatFlags.bmdDetectedVideoInputDualStream3D] = "Dual-stream 3D",
			[_BMDDetectedVideoInputFormatFlags.bmdDetectedVideoInput12BitDepth] = "12-bit depth",
			[_BMDDetectedVideoInputFormatFlags.bmdDetectedVideoInput10BitDepth] = "10-bit depth",
			[_BMDDetectedVideoInputFormatFlags.bmdDetectedVideoInput8BitDepth] = "8-bit depth",
		};

		public StatusItemVideoInputFormatFlags(string name, _BMDDeckLinkStatusID id) : base(name, id) { }

		protected override string Value(IDeckLinkStatus deckLinkStatus)
		{
			string returnString;

			try
			{
				deckLinkStatus.GetInt(ID, out long intValue);
				returnString = string.Join("\n", VideoInputFormatFlagsDictionary.Where(vs => ((_BMDDetectedVideoInputFormatFlags)intValue).HasFlag(vs.Key)).Select(vs => vs.Value));
				return String.IsNullOrEmpty(returnString) ? "None" : returnString;
			}
			catch (Exception)
			{
				return null;
			}
		}
	}

	internal class StatusItemFieldDominance : StatusItem
	{
		private readonly IReadOnlyDictionary<_BMDFieldDominance, string> FieldDominanceDictionary = new Dictionary<_BMDFieldDominance, string>
		{
			[_BMDFieldDominance.bmdUpperFieldFirst] = "Upper field first",
			[_BMDFieldDominance.bmdLowerFieldFirst] = "Lower field first",
			[_BMDFieldDominance.bmdProgressiveFrame] = "Progressive frame",
			[_BMDFieldDominance.bmdProgressiveSegmentedFrame] = "Progressive segmented frame",
		};

		public StatusItemFieldDominance(string name, _BMDDeckLinkStatusID id) : base(name, id) { }

		protected override string Value(IDeckLinkStatus deckLinkStatus)
		{
			try
			{
				deckLinkStatus.GetInt(ID, out long intValue);
				return FieldDominanceDictionary[(_BMDFieldDominance)intValue];
			}
			catch (Exception)
			{
				return null;
			}
		}
	}

	internal class StatusItemColorspace : StatusItem
	{
		private readonly IReadOnlyDictionary<_BMDColorspace, string> ColorspaceDictionary = new Dictionary<_BMDColorspace, string>
		{
			[_BMDColorspace.bmdColorspaceRec601] = "Rec.601",
			[_BMDColorspace.bmdColorspaceRec709] = "Rec.709",
			[_BMDColorspace.bmdColorspaceRec2020] = "Rec.2020",
		};

		public StatusItemColorspace(string name, _BMDDeckLinkStatusID id) : base(name, id) { }

		protected override string Value(IDeckLinkStatus deckLinkStatus)
		{
			try
			{
				deckLinkStatus.GetInt(ID, out long intValue);
				return ColorspaceDictionary[(_BMDColorspace)intValue];
			}
			catch (Exception)
			{
				return null;
			}
		}
	}

	internal class StatusItemDynamicRange : StatusItem
	{
		private readonly IReadOnlyDictionary<_BMDDynamicRange, string> DynamicRangeDictionary = new Dictionary<_BMDDynamicRange, string>
		{
			[_BMDDynamicRange.bmdDynamicRangeSDR] = "SDR",
			[_BMDDynamicRange.bmdDynamicRangeHDRStaticPQ] = "PQ (ST 2084)",
			[_BMDDynamicRange.bmdDynamicRangeHDRStaticHLG] = "HLG",
		};

		public StatusItemDynamicRange(string name, _BMDDeckLinkStatusID id) : base(name, id) { }

		protected override string Value(IDeckLinkStatus deckLinkStatus)
		{
			try
			{
				deckLinkStatus.GetInt(ID, out long intValue);
				return DynamicRangeDictionary[(_BMDDynamicRange)intValue];
			}
			catch (Exception)
			{
				return null;
			}
		}
	}

	internal class StatusItemLinkConfiguration : StatusItem
	{
		private readonly IReadOnlyDictionary<_BMDLinkConfiguration, string> LinkConfigurationDictionary = new Dictionary<_BMDLinkConfiguration, string>
		{
			[_BMDLinkConfiguration.bmdLinkConfigurationSingleLink] = "Single-link",
			[_BMDLinkConfiguration.bmdLinkConfigurationDualLink] = "Dual-link",
			[_BMDLinkConfiguration.bmdLinkConfigurationQuadLink] = "Quad-link",
		};

		public StatusItemLinkConfiguration(string name, _BMDDeckLinkStatusID id) : base(name, id) { }

		protected override string Value(IDeckLinkStatus deckLinkStatus)
		{
			try
			{
				deckLinkStatus.GetInt(ID, out long intValue);
				return LinkConfigurationDictionary[(_BMDLinkConfiguration)intValue];
			}
			catch (Exception)
			{
				return null;
			}
		}
	}
}
