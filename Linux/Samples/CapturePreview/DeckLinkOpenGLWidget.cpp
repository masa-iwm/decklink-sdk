/* -LICENSE-START-
** Copyright (c) 2019 Blackmagic Design
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

#include "DeckLinkOpenGLWidget.h"
#include <QOpenGLFunctions>

///
/// DeckLinkOpenGLDelegate
///

DeckLinkOpenGLDelegate::DeckLinkOpenGLDelegate() : 
	m_refCount(1)
{
}

/// IUnknown methods

HRESULT DeckLinkOpenGLDelegate::QueryInterface(REFIID iid, LPVOID *ppv)
{
	CFUUIDBytes		iunknown;
	HRESULT			result = S_OK;

	if (ppv == nullptr)
		return E_INVALIDARG;

	// Obtain the IUnknown interface and compare it the provided REFIID
	iunknown = CFUUIDGetUUIDBytes(IUnknownUUID);
	if (memcmp(&iid, &iunknown, sizeof(REFIID)) == 0)
	{
		*ppv = this;
		AddRef();
	}
	else if (memcmp(&iid, &IID_IDeckLinkScreenPreviewCallback, sizeof(REFIID)) == 0)
	{
		*ppv = static_cast<IDeckLinkScreenPreviewCallback*>(this);
		AddRef();
	}
	else
	{
		*ppv = nullptr;
		result = E_NOINTERFACE;
	}

	return result;
}

ULONG DeckLinkOpenGLDelegate::AddRef ()
{
	return ++m_refCount;
}

ULONG DeckLinkOpenGLDelegate::Release()
{
	ULONG newRefValue = --m_refCount;
	if (newRefValue == 0)
		delete this;

	return newRefValue;
}

/// IDeckLinkScreenPreviewCallback methods

HRESULT DeckLinkOpenGLDelegate::DrawFrame(IDeckLinkVideoFrame* frame)
{
	emit frameArrived(com_ptr<IDeckLinkVideoFrame>(frame));
	return S_OK;
}

///
/// DeckLinkOpenGLWidget
///

DeckLinkOpenGLWidget::DeckLinkOpenGLWidget(QWidget* parent) : 
	QOpenGLWidget(parent)
{
	m_deckLinkScreenPreviewHelper = CreateOpenGLScreenPreviewHelper();
	m_delegate = make_com_ptr<DeckLinkOpenGLDelegate>();

	connect(m_delegate.get(), &DeckLinkOpenGLDelegate::frameArrived, this, &DeckLinkOpenGLWidget::setFrame, Qt::QueuedConnection);
}

void DeckLinkOpenGLWidget::clear()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_delegate)
		m_delegate->DrawFrame(nullptr);
}

/// QOpenGLWidget methods

void DeckLinkOpenGLWidget::initializeGL()
{
	if (m_deckLinkScreenPreviewHelper)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_deckLinkScreenPreviewHelper->InitializeGL();
	}
}

void DeckLinkOpenGLWidget::paintGL()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_deckLinkScreenPreviewHelper->PaintGL();
}

void DeckLinkOpenGLWidget::resizeGL(int width, int height)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	QOpenGLFunctions* f = context()->functions();
	f->glViewport(0, 0, width, height);
}

/// DeckLinkOpenGLWidget slots 

void DeckLinkOpenGLWidget::setFrame(com_ptr<IDeckLinkVideoFrame> frame)
{
	if (m_deckLinkScreenPreviewHelper)
	{
		m_deckLinkScreenPreviewHelper->SetFrame(frame.get());
		update();
	}
}
