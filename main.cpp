#include "amb_mon.h"
#include "fmc132p_mon.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

#include "brd.h"

U16 Dev_init()
{
	S32		status;
	BRD_displayMode(BRDdm_VISIBLE);
	int NumDev;
	status = BRD_init(_BRDC("brd.ini"), &NumDev); // инициализируем устройства
	if (!NumDev)
		return 0;

	BRD_LidList lidList;
	status = BRD_lidList(NULL, 0, &lidList.itemReal);
	lidList.item = lidList.itemReal;
	lidList.pLID = new U32[lidList.item];
	status = BRD_lidList(lidList.pLID, lidList.item, &lidList.itemReal);

	BRD_Info	info;
	info.size = sizeof(info);
	BRD_getInfo(lidList.pLID[0], &info); // получить информацию об устройстве

	status = BRD_cleanup();

	return (info.boardType >> 16);

}

int main(int argc, char *argv[])
{
    //Q_INIT_RESOURCE(amb_mon);
	//Q_INIT_RESOURCE(fmc132p_mon);
	QStringList paths = QCoreApplication::libraryPaths();
	paths.append(".");
	//paths.append("./plugins");
	QCoreApplication::setLibraryPaths(paths);

	QApplication a(argc, argv);

	//if (!QSystemTrayIcon::isSystemTrayAvailable()) {
 //       QMessageBox::critical(0, QObject::tr("Systray"),
 //                             QObject::tr("I couldn't detect any system tray "
 //                                         "on this system."));
 //       return 1;
 //   }
 //   QApplication::setQuitOnLastWindowClosed(false);

	fmc132p_mon* fmc132p_w;
	amb_mon* w;

	U16 deviceID = Dev_init();

	if (deviceID == 0x5522 || // FMC126P
		deviceID == 0x5523) // FMC132P
	{
		fmc132p_w = new fmc132p_mon;
		fmc132p_w->show();
	}
	else
	{
		w = new amb_mon;
		w->show();
	}
	int ret = a.exec();

	if (deviceID == 0x5522 || // FMC126P
		deviceID == 0x5523) // FMC132P
	{
		delete fmc132p_w;
	}
	else
	{
		delete w;
	}

	return ret;
}
