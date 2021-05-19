/* -LICENSE-START-
** Copyright (c) 2011 Blackmagic Design
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

#ifndef BMD_COM_OBJECT_H__
#define BMD_COM_OBJECT_H__

#include <CoreFoundation/CFPluginCOM.h>
#include <libkern/OSAtomic.h>
#include <string.h>
#include "Debug.h"

template <class T, const REFIID* I> class COMObject : public T
{
public:
	COMObject() : mRefCount(1) {}

	virtual SInt32 QueryInterface(REFIID iid, LPVOID* ppv)
	{
		if (!ppv)
			return E_POINTER;

		if (!memcmp(&iid, I, sizeof(iid)))
		{
			*ppv = static_cast<T*>(this);
			AddRef();
			return S_OK;
		}

		CFUUIDBytes iunknown = CFUUIDGetUUIDBytes(IUnknownUUID);
		if (!memcmp(&iid, &iunknown, sizeof(iid)))
		{
			*ppv = static_cast<IUnknown*>(this);
			AddRef();
			return S_OK;
		}

		*ppv = NULL;

		return E_NOINTERFACE;
	}

	virtual UInt32 AddRef()
	{
		return ::OSAtomicIncrement32(&mRefCount);
	}

	virtual UInt32 Release()
	{
		int newCount = ::OSAtomicDecrement32(&mRefCount);
		BAIL_IF(newCount < 0, "Negative reference count: %d\n", newCount);

		if (newCount == 0)
			delete this;

	bail:
		return newCount;
	}
			
protected:
	virtual ~COMObject() {}

	int mRefCount;
};

#endif // BMD_COM_OBJECT_H__
