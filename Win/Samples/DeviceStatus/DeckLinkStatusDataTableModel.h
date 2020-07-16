/* -LICENSE-START-
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

#pragma once

#include <QAbstractTableModel>

#include <atomic>
#include <vector>

#include "com_ptr.h"
#include "platform.h"
#include "DeckLinkAPI.h"

class DeckLinkNotificationCallback : public QObject, public IDeckLinkNotificationCallback
{
	Q_OBJECT

public:
	DeckLinkNotificationCallback();
	virtual ~DeckLinkNotificationCallback() = default;

	// IUnknown interface
	HRESULT	QueryInterface(REFIID iid, LPVOID *ppv) override;
	ULONG	AddRef() override;
	ULONG	Release() override;

	// IDeckLinkNotificationCallback interface
	HRESULT Notify(BMDNotifications topic, uint64_t param1, uint64_t param2) override;

signals:
	void	statusChanged(BMDDeckLinkStatusID statusID);
	void	preferencesChanged(void);

private:
	std::atomic<ULONG>	m_refCount;
};

enum class StatusDataTableHeader : int { Item, Value, Count };

class DeckLinkStatusDataTableModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	DeckLinkStatusDataTableModel(QObject* parent);

	IDeckLinkNotificationCallback*		delegate();
	void								reset(com_ptr<IDeckLink>& deckLink);

	// QAbstractTableModel methods
	int									rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int									columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant							data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant							headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private slots:
	void								statusChanged(BMDDeckLinkStatusID statusID);

private:
	com_ptr<IDeckLinkStatus>								m_deckLinkStatus;
	com_ptr<DeckLinkNotificationCallback>					m_delegate;

	std::vector<std::pair<BMDDeckLinkStatusID, QString>>	m_statusData;
};

// Status item getter functions
QString		getStatusFlag(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id);
QString		getStatusInt(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id);
QString		getStatusBytes(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id);

QString		getStatusPanelType(com_ptr<IDeckLinkStatus>& deckLinkStatus);
QString		getStatusBusy(com_ptr<IDeckLinkStatus>& deckLinkStatus);
QString		getStatusVideoOutputMode(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id);
QString		getStatusPixelFormat(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id);
QString		getStatusVideoFlags(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id);
QString		getStatusDetectedVideoInputFormatFlags(com_ptr<IDeckLinkStatus>& deckLinkStatus);
QString		getStatusVideoInputMode(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id);
QString		getStatusDynamicRange(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id);
QString		getStatusFieldDominance(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id);
QString		getStatusColorspace(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id);
QString		getStatusLinkConfiguration(com_ptr<IDeckLinkStatus>& deckLinkStatus, const BMDDeckLinkStatusID id);

