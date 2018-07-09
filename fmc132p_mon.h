#ifndef FMC132P_MON_H
#define FMC132P_MON_H

#include <QSystemTrayIcon>
#include <QtGui>
#include <QtWidgets/QDialog>
#include <QtNetwork/QTcpServer> 
#include <QtNetwork/QTcpSocket> 
#include <QtNetwork/QNetworkInterface> 
#include "ui_fmc132p_mon.h"

class fmc132p_mon : public QDialog, public Ui::fmc132p_monClass
{
	Q_OBJECT

public:
	fmc132p_mon(int& isValid, QWidget *parent = 0);
	~fmc132p_mon();
	void timerEvent(QTimerEvent *event);
	void setVisible(bool visible); // for tray

	int smonTimer;
	void getValSysMon();
	void DisplayVoltTable();

	void getValSensMon();
	void DisplayIna219Table();
	void DisplayLtc2991Table();
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

	virtual void closeEvent(QCloseEvent * event);

	QTcpServer *tcpServer;
	quint16     m_nNextBlockSize;
	void sendToClient(QTcpSocket* pSocket, const QString& str);

public slots:
	virtual void slotNewConnection();
			void slotReadClient();
			void ClickedPwmEn();
			void ClickedPwmInv();
			void EditPwmThreshold();

};

#endif // FMC132P_MON_H
