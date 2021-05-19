/* -LICENSE-START-
 ** Copyright (c) 2015 Blackmagic Design
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
#include "MainWindow.h"

#include <QPushButton>
#include <QVBoxLayout>
#include <QPainter>
#include <QFontDatabase>
#include <QFrame>
#include <QLabel>

#include "ColourPalette.h"
#include "CommonGui.h"
#include "CommonWidgets.h"

#include "ControllerWidget.h"

#define PSS(x) ColourPalette::processStyleSheet(x)

class WindowHeader: public QWidget
{
public:
	WindowHeader(QWidget* parent = NULL):
	QWidget(parent),
	m_text("H.265 Encoder"),
	m_logo(":/Logo")
	{
		setFixedHeight(76);
		const int kLabelBaseline = 48;
		const int kLabelXPos = 29;
		setFont(CommonGui::font(CommonGui::kSentinelLight));
		QFontMetrics qf = fontMetrics();
		int labelWidth = qf.width(m_text) + 4;	// Add a few pixels for safety
		int labelHeight = qf.height();
		int labelYPos = kLabelBaseline + qf.descent() - labelHeight;
		m_textRect = QRect(kLabelXPos, labelYPos, labelWidth, labelHeight);
	}

protected:
	QString m_text;
	QRect m_textRect;
	QPixmap m_logo;

	virtual void paintEvent(QPaintEvent*)
	{
		QPainter painter(this);

		QLinearGradient gradient(0, 0, 0, height());
		gradient.setColorAt(0.0, ColourPalette::kColour6);
		gradient.setColorAt(0.5, ColourPalette::kColour7);
		gradient.setColorAt(1.0, ColourPalette::kColour8);
		QBrush backgroundBrush(gradient);

		painter.fillRect(rect(), backgroundBrush);

		painter.setPen(ColourPalette::kColour4);
		painter.drawText(m_textRect, Qt::AlignVCenter | Qt::AlignLeft, m_text);

		painter.drawPixmap(width() - m_logo.width() - 24, (height() - m_logo.height()) / 2, m_logo);
	}
};

MainWindow::MainWindow()
{
	setMinimumSize(682, 538 - 50 + 76);
	setMaximumSize(682, 538 - 50 + 76);
	QFontDatabase::addApplicationFont(":/gotham-xlight");
	QFontDatabase::addApplicationFont(":/OpenSans-Light");
	QFontDatabase::addApplicationFont(":/OpenSans-Semibold");
	setWindowTitle(tr("H.265 Encoder"));
	createWidgets();
}

void MainWindow::init(QObject* controller)
{
	bool connected = connect(m_controlsWidget->m_recordButton, SIGNAL(clicked()), controller, SLOT(startStopCapture()));
	Q_ASSERT(connected);
	connected = connect(m_controlsWidget, SIGNAL(speedChanged(int)), controller, SLOT(changeTargetRate(int)));
	Q_ASSERT(connected);
}

void MainWindow::createWidgets()
{
	QFrame* frame = new QFrame();
	frame->setStyleSheet(PSS("QWidget { background: kColour1; border-color: kColour2; }"));
	frame->setFrameStyle(QFrame::Box);

	QVBoxLayout *layout = new TightVBoxLayout;
	frame->setLayout(layout);

	layout->addWidget(new WindowHeader);

	QWidget* blackSep = new QWidget();
	blackSep->setFixedHeight(1);
	blackSep->setStyleSheet(PSS("QWidget { background: kColour2; }"));
	layout->addWidget(blackSep);

	layout->addSpacing(50);
	{
		QLabel* deviceImage = new QLabel();
		deviceImage->setFixedSize(600, 136);
		deviceImage->setStyleSheet(QString("QWidget { background-image: url(:/Device); }"));
		layout->addWidget(deviceImage, 0, Qt::AlignHCenter);
	}
	
	layout->addSpacing(50);

	m_controlsWidget = new ControlsWidget();
	layout->addWidget(m_controlsWidget, 0, Qt::AlignHCenter);

	layout->addStretch();

	setCentralWidget(frame);
}

void MainWindow::recordingStarted(QString displayMode, uint32_t frameRate)
{
	m_controlsWidget->onRecordingStarted(displayMode, frameRate);
}

void MainWindow::recordingFinished()
{
	m_controlsWidget->onRecordingStopped();
}


