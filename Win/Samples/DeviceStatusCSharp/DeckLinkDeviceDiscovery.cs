﻿/* -LICENSE-START-
** Copyright (c) 2020 Blackmagic Design
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

namespace DeviceStatusCSharp
{
	public class DeckLinkDiscoveryEventArgs : EventArgs
	{
		public readonly IDeckLink deckLink;

		public DeckLinkDiscoveryEventArgs(IDeckLink deckLink)
		{
			this.deckLink = deckLink;
		}
	}

	public class DeckLinkDeviceDiscovery : IDeckLinkDeviceNotificationCallback
	{
		private IDeckLinkDiscovery deckLinkDiscovery;
		private bool deckLinkDiscoveryEnabled = false;

		public event EventHandler<DeckLinkDiscoveryEventArgs> DeviceArrived;
		public event EventHandler<DeckLinkDiscoveryEventArgs> DeviceRemoved;

		public DeckLinkDeviceDiscovery()
		{
			deckLinkDiscovery = new CDeckLinkDiscovery();
		}

		~DeckLinkDeviceDiscovery()
		{
			Disable();
		}

		public void Enable()
		{
			deckLinkDiscovery.InstallDeviceNotifications(this);
			deckLinkDiscoveryEnabled = true;
		}

		public void Disable()
		{
			if (deckLinkDiscoveryEnabled)
			{
				deckLinkDiscovery.UninstallDeviceNotifications();
				deckLinkDiscoveryEnabled = false;
			}
		}

		#region callbacks
		void IDeckLinkDeviceNotificationCallback.DeckLinkDeviceArrived(IDeckLink deckLinkDevice)
		{
			DeviceArrived?.Invoke(this, new DeckLinkDiscoveryEventArgs(deckLinkDevice));
		}

		void IDeckLinkDeviceNotificationCallback.DeckLinkDeviceRemoved(IDeckLink deckLinkDevice)
		{
			DeviceRemoved?.Invoke(this, new DeckLinkDiscoveryEventArgs(deckLinkDevice));
		}
		#endregion
	}
}