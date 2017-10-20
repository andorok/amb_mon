#ifndef AMB_MON_H
#define AMB_MON_H

#include <QSystemTrayIcon>
#include <QtGui>
#include <QtWidgets/QDialog>
#include "ui_amb_mon.h"

class amb_mon : public QDialog, public Ui::amb_monClass
{
	Q_OBJECT

public:
	amb_mon(QWidget *parent = 0);
	~amb_mon();
	void timerEvent(QTimerEvent *event);
	void setVisible(bool visible); // for tray

	int smonTimer;
	void getValSysMon();
	void DisplayVoltTable();

	void getValPowMon();
	void DisplayPowTable();

	// for tray
private slots:
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    void showMessage();

private:
    QAction *minimizeAction;
    QAction *restoreAction;
    QAction *quitAction;
    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;

};

#endif // AMB_MON_H
