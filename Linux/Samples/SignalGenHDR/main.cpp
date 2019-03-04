#include "SignalGenHDR.h"
#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	SignalGenHDR w;
	w.show();
	w.setup();

	return a.exec();
}
