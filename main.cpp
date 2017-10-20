#include "amb_mon.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(amb_mon);

	QApplication a(argc, argv);

	if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(0, QObject::tr("Systray"),
                              QObject::tr("I couldn't detect any system tray "
                                          "on this system."));
        return 1;
    }
    QApplication::setQuitOnLastWindowClosed(false);
	amb_mon w;
	w.show();
	return a.exec();
}
