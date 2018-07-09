#include "amb_mon.h"
//#include <QTextCodec>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QMenu>

#include "brd.h"
#include "extn.h"
#include "ctrlsysmon.h"

typedef struct _SYSMON_PARAM
{
	REAL64	curTemp;		// Текущая температура кристалла
	REAL64	minTemp;		// Минимальная температура кристалла
	REAL64	maxTemp;		// Максимальная температура кристалла
	REAL64	curVCCint;		// Текущее напряжение питания ядра
	REAL64	minVCCint;		// Минимальное напряжение питания ядра
	REAL64	maxVCCint;		// Максимальное напряжение питания ядра
	REAL64	curVCCaux;		// Текущее напряжение питания ПЛИС
	REAL64	minVCCaux;		// Минимальное напряжение питания ПЛИС
	REAL64	maxVCCaux;		// Максимальное напряжение питания ПЛИС
	REAL64	curVCCbram;		// Текущее напряжение питания блока RAM
	REAL64	minVCCbram;		// Минимальное питание блока RAM
	REAL64	maxVCCbram;		// Максимальное питание блока RAM
	REAL64	Vrefp;			// Внешнее опорное напряжение (плюс)
	REAL64	Vrefn;			// Внешнее опорное напряжение  (минус)
} SYSMON_PARAM, *PSYSMON_PARAM;

SYSMON_PARAM g_sysmon;
U32 g_smonStatus; 

REAL64 g_fVCCintNominal = 0.0;	// Номинал питания ядра
REAL64 g_fVCCauxNominal = 0.0;	// Номинал питания ПЛИС
REAL64 g_fVrefpNominal = 0.0;	// Номинал внешнего опорного напряжения (плюс)
REAL64 g_fVrefnNominal = 0.0;	// Номинал внешнего опорного напряжения (минус)
REAL64 g_fVCCbramNominal = 0.0;	// Номинал питания блока RAM

U08 g_isVCCbram = 1;	// 0 - нет напряжения питания блока RAM

static BRD_Handle g_hDevice;
static BRD_Handle g_hSysMon;

typedef struct _INA219MON_PARAM
{
	REAL64	volt;		// Текущее напряжение напряжения питания
	REAL64	cur;		// Текущее напряжение тока питания
	REAL64	pow;		// Минимальное напряжение мощности питания
} INA219MON_PARAM, *PINA219MON_PARAM;

INA219MON_PARAM g_val3;
INA219MON_PARAM g_val12;

QString g_brdInfo;

U16 g_devid;

amb_mon::amb_mon(int& isValid, QWidget *parent)
	: QDialog(parent)
{
	//QTextCodec::setCodecForCStrings(QTextCodec::codecForName("CP1251"));
	setupUi(this);

	//*******************************
	// network
	m_nNextBlockSize = 0;
	tcpServer = new QTcpServer(this);
	if (!tcpServer->listen(QHostAddress::Any, 2323))
	{
		QMessageBox::critical( 0, "Server Error", "Unable to start the server:" + tcpServer->errorString() );
		tcpServer->close();
		//return;
	}
	else
		connect( tcpServer, SIGNAL(newConnection()), this, SLOT(slotNewConnection()) );
	//*******************************

	//quint16 port = tcpServer->serverPort();

	//QString locIP;
	//QHostAddress host_adr = tcpServer->serverAddress();
	//locIP = host_adr.toString();

	//QList<QHostAddress> myIpAddresses = QNetworkInterface::allAddresses();

	////locIP = myIpAddresses.first().toString();
	////locIP = myIpAddresses.last().toString();
	//int adr_cnt = myIpAddresses.count();

	//for(int i = 0; i < adr_cnt; i++)
	//{
	//	locIP = myIpAddresses.at(i).toString();
	//}

	S32		status;
	BRD_displayMode(BRDdm_VISIBLE);
	int NumDev;
    status = BRD_init(_BRDC("brd.ini"), &NumDev); // инициализируем устройства
    if(!NumDev)
	{
		labelBrdInfo->setText("<b><font color=red>Devices not found!</font></b>");
		return;
	}
	BRD_LidList lidList;
	status = BRD_lidList(NULL, 0, &lidList.itemReal);
	lidList.item = lidList.itemReal;
	lidList.pLID = new U32[lidList.item];
	status = BRD_lidList(lidList.pLID, lidList.item, &lidList.itemReal);

	BRD_Info	info;
	info.size = sizeof(info);
	BRD_getInfo(lidList.pLID[0], &info); // получить информацию об устройстве
	QString strInfo; 
#ifdef _WIN64
//	strInfo.sprintf("<font color=blue><b>%ls</b>(0x%X): RevID = 0x%X, PID = %lu, Bus = %d, Dev = %d.</font>", 
//			info.name, info.boardType >> 16, info.boardType & 0xff, info.pid, info.bus, info.dev);
	strInfo.sprintf("<font color=blue><b>%ls ver %d.%d: s/n = %lu (Bus = %d, Dev = %d).</b></font>", 
			info.name, (info.boardType>>4)&0xf, info.boardType&0xf, info.pid, info.bus, info.dev);
	g_brdInfo.sprintf("%ls ver %d.%d: s/n = %lu",
		info.name, (info.boardType >> 4) & 0xf, info.boardType & 0xf, info.pid);
#else 
//	strInfo.sprintf("<font color=blue><b>%s</b>(0x%X): RevID = 0x%X, PID = %lu, Bus = %d, Dev = %d.</font>", 
//			info.name, info.boardType >> 16, info.boardType & 0xff, info.pid, info.bus, info.dev);
	strInfo.sprintf("<font color=blue><b>%s ver %d.%d: s/n = %lu (Bus = %d, Dev = %d).</b></font>", 
			info.name, (info.boardType>>4)&0xf, info.boardType&0xf, info.pid, info.bus, info.dev);
	g_brdInfo.sprintf("%s ver %d.%d: s/n = %lu",
		info.name, (info.boardType >> 4) & 0xf, info.boardType & 0xf, info.pid);
#endif
	labelBrdInfo->setText(strInfo);
	g_devid = info.boardType >> 16;

	g_hDevice = BRD_open(lidList.pLID[0], BRDopen_SHARED, NULL); // открываем первое устройство

	U32 ItemReal;
	status = BRD_serviceList(g_hDevice, 0, NULL, 0, &ItemReal);
	PBRD_ServList pSrvList = new BRD_ServList[ItemReal];
	status = BRD_serviceList(g_hDevice, 0, pSrvList, ItemReal, &ItemReal);
	U32 mode = BRDcapt_EXCLUSIVE;
	//U32 mode = BRDcapt_SHARED;
	g_hSysMon = 0;
	for(U32 iSrv = 0; iSrv < ItemReal; iSrv++)
	{
		if(!BRDC_strcmp(pSrvList[iSrv].name, _BRDC("SYSMON0")))
		{
			g_hSysMon = BRD_capture(g_hDevice, 0, &mode, _BRDC("SYSMON0"), 10000); // захватываем службу доступа к регистрам устройства
			break;
		}
	}

	delete lidList.pLID;
	delete pSrvList;

	if(!g_hSysMon)
	{
		QMessageBox::about(this, "System Monitor", "<font color=red>Service SYSMON not capture!</font>");
		return;
	}

	BRD_VoltNominals7s voltNominals;
	S32	err = BRD_ctrl(g_hSysMon, 0, BRDctrl_SYSMON_GETVN7S, &voltNominals);
	if (BRD_errcmp(err, BRDerr_OK))
	{
		g_fVCCbramNominal = voltNominals.vccbram;
	}
	else
	{
		BRD_ctrl(g_hSysMon, 0, BRDctrl_SYSMON_GETVNOMINALS, &voltNominals);
		g_isVCCbram = 0;
	}

	//BRD_VoltNominals voltNominals;
	//BRD_ctrl(g_hSysMon, 0, BRDctrl_SYSMON_GETVNOMINALS, &voltNominals);
	g_fVCCintNominal = voltNominals.vccint;
	g_fVCCauxNominal = voltNominals.vccaux;
	g_fVrefpNominal = voltNominals.vrefp;
	g_fVrefnNominal = voltNominals.vrefn;

/*	if (g_devid == 0x5523) //FMC132P
	{
		pow_table->setRowCount(1);
		//pow_table->resizeRowsToContents();
		QSize pow_size = pow_table->size();
		pow_size.setHeight(pow_size.height() - 25);
		pow_table->setFixedSize(pow_size);
	}
	else
	{
		//volt_132_table->setVisible(false);
		//volt_132_table->hide();
	}
	*/
	getValSysMon();
	DisplayVoltTable();

	getValPowMon();
	DisplayPowTable();

	smonTimer = startTimer(1000);

	labelCurTemp->setToolTip("Текущая температура кристалла");
	labelMinTemp->setToolTip("Минимальная температура кристалла");
	labelMaxTemp->setToolTip("Максимальная температура кристалла");
//	labelCurVCCint->setToolTip("Текущее напряжение питания ядра");
//	labelMinVCCint->setToolTip("Минимальное напряжение питания ядра");
//	labelMaxVCCint->setToolTip("Максимальное напряжение питания ядра");
//	labelCurVCCaux->setToolTip("Текущее напряжение питания ПЛИС");
//	labelMinVCCaux->setToolTip("Минимальное напряжение питания ПЛИС");
//	labelMaxVCCaux->setToolTip("Максимальное напряжение питания ПЛИС");
//	labelVRefp->setToolTip("Внешнее опорное напряжение (плюс)");
//	labelVRefn->setToolTip("Внешнее опорное напряжение (минус)");
	//labelCurTemp->setToolTip("Current temperature of PLD chip");
	//labelMinTemp->setToolTip(tr("Minimum temperature of PLD chip"));
	//labelMaxTemp->setToolTip(tr("Maximum temperature of PLD chip"));
	//labelCurVCCint->setToolTip(tr("Current voltage of PLD core"));
	//labelMinVCCint->setToolTip(tr("Minimum voltage of PLD core"));
	//labelMaxVCCint->setToolTip(tr("Maximum voltage of PLD core"));
	//labelCurVCCaux->setToolTip(tr("Current voltage of PLD chip"));
	//labelMinVCCaux->setToolTip(tr("Minimum voltage of PLD chip"));
	//labelMaxVCCaux->setToolTip(tr("Maximum voltage of PLD chip"));
	//labelVRefp->setToolTip(tr("External reference voltage (plus)"));
	//labelVRefn->setToolTip(tr("External reference voltage (minus)"));
	// реализация сворачивания в Tray
	minimizeAction = new QAction(tr("Mi&nimize"), this);
    connect(minimizeAction, SIGNAL(triggered()), this, SLOT(hide()));
    restoreAction = new QAction(tr("&Restore"), this);
    connect(restoreAction, SIGNAL(triggered()), this, SLOT(showNormal()));
    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

	trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(minimizeAction);
    trayIconMenu->addAction(restoreAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);

    //QIcon icon = QIcon(":/amb_mon/images/pinion-icon.png");
    //QIcon icon = QIcon(":/amb_mon/images/weather256.png");
	QIcon icon = QIcon(":/amb_mon/images/scale_ruler.png");
	trayIcon->setIcon(icon);
    setWindowIcon(icon);
	QString strTray;
	//strTray.sprintf("%.3fC (%.3fC-%.3fC)|%.3fV (%.3fV-%.3fV)|%.3fV (%.3fV-%.3fV)|%.3fV|%.3fV", 
	//	g_sysmon.curTemp, g_sysmon.minTemp, g_sysmon.maxTemp,
	//	g_sysmon.curVCCint, g_sysmon.minVCCint, g_sysmon.maxVCCint,
	//	g_sysmon.curVCCaux, g_sysmon.minVCCaux, g_sysmon.maxVCCaux,
	//	g_sysmon.Vrefp, g_sysmon.Vrefn);
		strTray.sprintf("%.3f°C | %.3fV | %.3fV", 
			g_sysmon.curTemp,
			g_sysmon.curVCCint,
			g_sysmon.curVCCaux);
	trayIcon->setToolTip(strTray);
    //trayIcon->setToolTip(tr("System Monitor"));

	//connect(trayIcon, SIGNAL(messageClicked()), this, SLOT(messageClicked()));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

	minimizeAction->setEnabled(true);
	restoreAction->setEnabled(false);
	trayIcon->show();

	isValid = 1; // конструктор выполнился без ошибок
}

void amb_mon::setVisible(bool visible)
{
	if(g_hSysMon)
	{
	    minimizeAction->setEnabled(visible);
		restoreAction->setEnabled(!visible);
		QDialog::setVisible(visible);
	}
}

// Виртуальная функция родительского класса,
// в нашем классе переопределяется для изменения поведения приложения,
// чтобы оно сворачивалось в трей, когда мы этого хотим
void amb_mon::closeEvent(QCloseEvent * event)
{
	/* Если окно видимо и чекбокс отмечен, то завершение приложения
	* игнорируется, а окно просто скрывается, что сопровождается
	* соответствующим всплывающим сообщением
	*/
	if (this->isVisible() && trayCheckBox->isChecked()) {
		//	if (this->isVisible()) {
		event->ignore();
		this->hide();
		QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::MessageIcon(QSystemTrayIcon::Information);

		//trayIcon->showMessage("Senmsors Monitor",
		//	trUtf8("Приложение свернуто в трей. Для того чтобы, "
		//		"развернуть окно приложения, щелкните по иконке приложения в трее"),
		//	icon,
		//	2000);
	}
}

// Слот, который будет принимать сигнал от события нажатия на иконку приложения в трее
void amb_mon::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::MiddleClick:
        showMessage();
        break;
    case QSystemTrayIcon::DoubleClick:
		//minimizeAction->setEnabled(true);
		//restoreAction->setEnabled(false);
        showNormal();
        break;
    default:
        ;
    }
}

void amb_mon::showMessage()
{
    //QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::MessageIcon(0);
    //QIcon icon = QIcon(":/images/heart.svg"), tr("Heart");
    trayIcon->showMessage("Systray", "System Monitor", QSystemTrayIcon::Information, 100);
    //trayIcon->showMessage("Systray", "Register Tester");
}

amb_mon::~amb_mon()
{
	S32	status;

	tcpServer->close();

	if(g_hSysMon)
	{
		killTimer(smonTimer);
		status = BRD_release(g_hSysMon, 0);	// освобождаем службу
	}
	status = BRD_close(g_hDevice);	// закрываем устройство
	status = BRD_cleanup();
}

void amb_mon::timerEvent(QTimerEvent *event)
{
	if(event->timerId() == smonTimer)
	{
		getValSysMon();
		DisplayVoltTable();

		getValPowMon();
		DisplayPowTable();

		QString strTray;
		//strTray.sprintf("%.3fC (%.3fC-%.3fC)|%.3fV (%.3fV-%.3fV)|%.3fV (%.3fV-%.3fV)|%.3fV|%.3fV", 
		//	g_sysmon.curTemp, g_sysmon.minTemp, g_sysmon.maxTemp,
		//	g_sysmon.curVCCint, g_sysmon.minVCCint, g_sysmon.maxVCCint,
		//	g_sysmon.curVCCaux, g_sysmon.minVCCaux, g_sysmon.maxVCCaux,
		//	g_sysmon.Vrefp, g_sysmon.Vrefn);
		strTray.sprintf("%.3f°C | %.3fV | %.3fV", 
			g_sysmon.curTemp,
			g_sysmon.curVCCint,
			g_sysmon.curVCCaux);
		trayIcon->setToolTip(strTray);
	}
	else
		QWidget::timerEvent(event);
}

void amb_mon::getValSysMon()
{
	QString strInfo; 
	BRD_SysMonVal sysmon_data;
	BRD_ctrl(g_hSysMon, 0, BRDctrl_SYSMON_GETSTATUS, &g_smonStatus);
	if(g_smonStatus & 0x10)
	{
//		QString msgErr;
//		msgErr.sprintf("<font color=red>WARNING: TEMPERATURE OF PLD > 125 С !!!</font>", regdata.tetr, regdata.reg);
		QMessageBox::warning(this, "System Monitor", "<font color=red>WARNING: TEMPERATURE OF PLD > 125 °С !!!</font>");
	}
	//Снятие значения температуры
	//if(g_isTemp)	
	{
		BRD_ctrl(g_hSysMon, 0, BRDctrl_SYSMON_GETTEMP, &sysmon_data);
		//strInfo.sprintf("<b><font color=green>%.3f C (min: %.3f C - max: %.3f C)</font></b>", 
		//	sysmon_data.curv, sysmon_data.minv, sysmon_data.maxv);
		//labelTemp->setText(strInfo);
		g_sysmon.curTemp = sysmon_data.curv;
		g_sysmon.minTemp = sysmon_data.minv;
		g_sysmon.maxTemp = sysmon_data.maxv;

        if(g_smonStatus & 2)
			strInfo.sprintf("Current: <b><font color=red>%.3f °C</font></b>", 
							sysmon_data.curv);
		else
			strInfo.sprintf("Current: <b><font color=green>%.3f °C</font></b>", 
							sysmon_data.curv);
		labelCurTemp->setText(strInfo);
		strInfo.sprintf("Minimum: <b>%.3f °C</b>", 
				sysmon_data.minv);
		labelMinTemp->setText(strInfo);
		strInfo.sprintf("Maximum: <b>%.3f °C</b>", 
				sysmon_data.maxv);
        labelMaxTemp->setText(strInfo);

	}
	//Снятие значения Vccint
	//if(g_isVCCint)
	{
		BRD_ctrl(g_hSysMon, 0, BRDctrl_SYSMON_GETVCCINT, &sysmon_data);
		g_sysmon.curVCCint = sysmon_data.curv;
		g_sysmon.minVCCint = sysmon_data.minv;
		g_sysmon.maxVCCint = sysmon_data.maxv;
/*		if(g_smonStatus & 4)
			strInfo.sprintf("Current: <b><font color=red>%.3f V</font></b>", 
							sysmon_data.curv);
		else
			strInfo.sprintf("Current: <b><font color=green>%.3f V</font></b>", 
							sysmon_data.curv);
		labelCurVCCint->setText(strInfo);
		strInfo.sprintf("Minimum: <b>%.3f V</b>", 
				sysmon_data.minv);
		labelMinVCCint->setText(strInfo);
		strInfo.sprintf("Maximum: <b>%.3f V</b>", 
				sysmon_data.maxv);
		labelMaxVCCint->setText(strInfo);*/
	}
	//Снятие значения Vccaux
//	if(g_isVCCaux)
	{
		BRD_ctrl(g_hSysMon, 0, BRDctrl_SYSMON_GETVCCAUX, &sysmon_data);
		//strInfo.sprintf("<b><font color=green>VCCaux: %.3f V (min: %.3f V - max: %.3f V)</font></b>", 
		//	sysmon_data.curv, sysmon_data.minv, sysmon_data.maxv);
		//labelVCCaux->setText(strInfo);
		g_sysmon.curVCCaux = sysmon_data.curv;
		g_sysmon.minVCCaux = sysmon_data.minv;
		g_sysmon.maxVCCaux = sysmon_data.maxv;
/*		if(g_smonStatus & 8)
			strInfo.sprintf("Current: <b><font color=red>%.3f V</font></b>", 
							sysmon_data.curv);
		else
			strInfo.sprintf("Current: <b><font color=green>%.3f V</font></b>", 
							sysmon_data.curv);
		labelCurVCCaux->setText(strInfo);
		strInfo.sprintf("Minimum: <b>%.3f V</b>", 
				sysmon_data.minv);
		labelMinVCCaux->setText(strInfo);
		strInfo.sprintf("Maximum: <b>%.3f V</b>", 
				sysmon_data.maxv);
		labelMaxVCCaux->setText(strInfo);*/
	}
	//Снятие значения Vrefp
	//if (g_isVrefp)
	{
		REAL64 val;
		BRD_ctrl(g_hSysMon, 0, BRDctrl_SYSMON_GETVREFP, &val);
		g_sysmon.Vrefp = val;
/*		strInfo.sprintf("Vrefp: <b>%.3f V</b>", val);
		labelVRefp->setText(strInfo);*/
	}
	//Снятие значения Vrefn
	//if(g_isVrefn)
	{
		REAL64 val;
		BRD_ctrl(g_hSysMon, 0, BRDctrl_SYSMON_GETVREFN, &val);
		g_sysmon.Vrefn = val;
		/*strInfo.sprintf("Vrefn: <b>%.3f V</b>", val);
		labelVRefn->setText(strInfo);*/
	}
	//Снятие значения Vccint
	if (g_isVCCbram)
	{
		BRD_ctrl(g_hSysMon, 0, BRDctrl_SYSMON_GETVCCBRAM, &sysmon_data);
		g_sysmon.curVCCbram = sysmon_data.curv;
		g_sysmon.maxVCCbram = sysmon_data.maxv;
		g_sysmon.minVCCbram = sysmon_data.minv;
	}
}

void amb_mon::DisplayVoltTable()
{
	REAL64 err;
    //volt_table->setColumnCount(5);
    //volt_table->setRowCount(4);
//	volt_table->setColumnWidth(0, 50);
//	volt_table->setColumnWidth(1, 50);
//	volt_table->setColumnWidth(2, 50);
//	volt_table->setColumnWidth(3, 50);
	QStringList RowHeaderLabels = (QStringList() << "VCCint" << "VCCaux" << "Vrefp" << "Vrefn" << "VCCbram");
	volt_table->setVerticalHeaderLabels(RowHeaderLabels);
	QStringList ColHeaderLabels = (QStringList() << "Current, V" << "Error, %" << "Nominal, V" << "Maximum, V" << "Minimum, V");
	volt_table->setHorizontalHeaderLabels(ColHeaderLabels);

	char buf[64];
	int j = 0;
	QTableWidgetItem *numItem[5*5];
	QBrush itemBrushGreen(Qt::darkGreen);
	QBrush itemBrushRed(Qt::red);
	for(int iRow = 0; iRow < 5; iRow++)
	{
		//volt_table->insertRow(iRow);
		//volt_table->setRowHeight(iRow, 20);

		// current 
		switch(iRow)
		{
		case 0:
			sprintf(buf, "%.3f", g_sysmon.curVCCint);
			break;
		case 1:
			sprintf(buf, "%.3f", g_sysmon.curVCCaux);
			break;
		case 2:
			sprintf(buf, "%.3f", g_sysmon.Vrefp);
			break;
		case 3:
			sprintf(buf, "%.3f", g_sysmon.Vrefn);
			break;
		case 4:
			if(g_isVCCbram)
				sprintf(buf, "%.3f", g_sysmon.curVCCbram);
			else
				sprintf(buf, "None");
			break;
		}
		numItem[j] = new QTableWidgetItem(buf);
		QFont itemFont = numItem[j]->font();
		itemFont.setBold(1);
		numItem[j]->setFont(itemFont);
		if(((iRow == 0) && (g_smonStatus & 4)) || ((iRow == 1) && (g_smonStatus & 8)))
			numItem[j]->setForeground(itemBrushRed);
		else
			numItem[j]->setForeground(itemBrushGreen);
		numItem[j]->setTextAlignment(Qt::AlignCenter);
		volt_table->setItem(iRow, 0, numItem[j++]);

		// error
		int fl_disp = 1;
		switch(iRow)
		{
		case 0:
			if(g_fVCCintNominal)
                err = fabs(g_sysmon.curVCCint - g_fVCCintNominal) / g_fVCCintNominal * 100.;
			else
				fl_disp = 0;
			break;
		case 1:
			if(g_fVCCauxNominal)
                err = fabs(g_sysmon.curVCCaux - g_fVCCauxNominal) / g_fVCCauxNominal * 100.;
			else
				fl_disp = 0;
			break;
		case 2:
			if(g_fVrefpNominal)
                err = fabs(g_sysmon.Vrefp - g_fVrefpNominal) / g_fVrefpNominal * 100.;
			else
				fl_disp = 0;
			break;
		case 3:
			if(g_fVrefnNominal)
                err = fabs(g_sysmon.Vrefn - g_fVrefnNominal) / g_fVrefnNominal * 100.;
			else
				fl_disp = 0;
			break;
		case 4:
			if (g_isVCCbram && g_fVCCbramNominal)
				err = fabs(g_sysmon.curVCCint - g_fVCCbramNominal) / g_fVCCbramNominal * 100.;
			else
				fl_disp = 0;
			break;
		}
		if(fl_disp)
		{
			sprintf(buf, "%.1f", err);
			numItem[j] = new QTableWidgetItem(buf);
			numItem[j]->setFont(itemFont);
			QBrush itemBrush1(Qt::magenta);
			numItem[j]->setForeground(itemBrush1);
			numItem[j]->setTextAlignment(Qt::AlignCenter);
			volt_table->setItem(iRow, 1, numItem[j++]);
		}

		// nominal
		switch(iRow)
		{
		case 0:
			sprintf(buf, "%.3f", g_fVCCintNominal);
			break;
		case 1:
			sprintf(buf, "%.3f", g_fVCCauxNominal);
			break;
		case 2:
			sprintf(buf, "%.3f", g_fVrefpNominal);
			break;
		case 3:
			sprintf(buf, "%.3f", g_fVrefnNominal);
			break;
		case 4:
			sprintf(buf, "%.3f", g_fVCCbramNominal);
			break;
		}
		numItem[j] = new QTableWidgetItem(buf);
		numItem[j]->setFont(itemFont);
		numItem[j]->setTextAlignment(Qt::AlignCenter);
		volt_table->setItem(iRow, 2, numItem[j++]);

		// max 
		switch (iRow)
		{
		case 0:
			sprintf(buf, "%.3f", g_sysmon.maxVCCint);
			break;
		case 1:
			sprintf(buf, "%.3f", g_sysmon.maxVCCaux);
			break;
		case 4:
			sprintf(buf, "%.3f", g_sysmon.maxVCCbram);
			break;
		}
		if(iRow == 0 || iRow == 1 || iRow == 4)
		{
			numItem[j] = new QTableWidgetItem(buf);
			numItem[j]->setFont(itemFont);
			numItem[j]->setTextAlignment(Qt::AlignCenter);
			volt_table->setItem(iRow, 3, numItem[j++]);
		}

		// min
		switch (iRow)
		{
		case 0:
			sprintf(buf, "%.3f", g_sysmon.minVCCint);
			break;
		case 1:
			sprintf(buf, "%.3f", g_sysmon.minVCCaux);
			break;
		case 4:
			sprintf(buf, "%.3f", g_sysmon.minVCCbram);
			break;
		}
		if (iRow == 0 || iRow == 1 || iRow == 4)
		{
			numItem[j] = new QTableWidgetItem(buf);
			numItem[j]->setFont(itemFont);
			numItem[j]->setTextAlignment(Qt::AlignCenter);
			volt_table->setItem(iRow, 4, numItem[j++]);
		}

	}
}

void amb_mon::getValPowMon()
{
	S32 status;
	QString strInfo;

	BRDextn_Sensors sval;

	sval.chip = 0;
	status = BRD_extension(g_hDevice, 0, BRDextn_SENSORS, &sval);
	if (!BRD_errcmp(status, BRDerr_OK))
		BRDC_printf(_BRDC(" Error by reading chip0 INA219  \n"));

	g_val3.volt = sval.voltage;
	g_val3.cur = sval.current;
	g_val3.pow = sval.power;

	sval.chip = 1;
	status = BRD_extension(g_hDevice, 0, BRDextn_SENSORS, &sval);
	if (!BRD_errcmp(status, BRDerr_OK))
		BRDC_printf(_BRDC(" Error by reading chip1 INA219  \n\n"));

	g_val12.volt = sval.voltage;
	g_val12.cur = sval.current;
	g_val12.pow = sval.power;
}

void amb_mon::DisplayPowTable()
{
	QStringList RowHeaderLabels = (QStringList() << "+3.3V" << "+12V");
	pow_table->setVerticalHeaderLabels(RowHeaderLabels);
	QStringList ColHeaderLabels = (QStringList() << "Voltage (V)" << "Current (A)" << "Power (W)");
	pow_table->setHorizontalHeaderLabels(ColHeaderLabels);

	char buf[64];
	int j = 0;
	QTableWidgetItem *numItem[4 * 4];
	QBrush itemBrushGreen(Qt::darkGreen);
	QBrush itemBrushRed(Qt::red);
	for (int iRow = 0; iRow < 2; iRow++)
	{
		// voltage
		switch (iRow)
		{
		case 0:
			sprintf(buf, "%.3f", g_val3.volt);
			break;
		case 1:
			sprintf(buf, "%.3f", g_val12.volt);
			break;
		}
		numItem[j] = new QTableWidgetItem(buf);
		QFont itemFont = numItem[j]->font();
		itemFont.setBold(1);
		numItem[j]->setFont(itemFont);
		//numItem[j]->setForeground(itemBrushGreen);
		numItem[j]->setTextAlignment(Qt::AlignCenter);
		pow_table->setItem(iRow, 0, numItem[j++]);

		// current 
		switch (iRow)
		{
		case 0:
			sprintf(buf, "%.3f", g_val3.cur);
			break;
		case 1:
			sprintf(buf, "%.3f", g_val12.cur);
			break;
		}
		numItem[j] = new QTableWidgetItem(buf);
		itemFont = numItem[j]->font();
		itemFont.setBold(1);
		numItem[j]->setFont(itemFont);
		//numItem[j]->setForeground(itemBrushGreen);
		numItem[j]->setTextAlignment(Qt::AlignCenter);
		pow_table->setItem(iRow, 1, numItem[j++]);

		// power
		switch (iRow)
		{
		case 0:
			sprintf(buf, "%.3f", g_val3.pow);
			break;
		case 1:
			sprintf(buf, "%.3f", g_val12.pow);
			break;
		}
		numItem[j] = new QTableWidgetItem(buf);
		itemFont = numItem[j]->font();
		itemFont.setBold(1);
		numItem[j]->setFont(itemFont);
		//numItem[j]->setForeground(itemBrushGreen);
		numItem[j]->setTextAlignment(Qt::AlignCenter);
		pow_table->setItem(iRow, 2, numItem[j++]);

	}
}

/*virtual*/ void amb_mon::slotNewConnection()
{
	QTcpSocket* pClientSocket = tcpServer->nextPendingConnection();
	connect(pClientSocket, SIGNAL(disconnected()),
		pClientSocket, SLOT(deleteLater())
	);
	connect(pClientSocket, SIGNAL(readyRead()),
		this, SLOT(slotReadClient())
	);

	//QString str;
	//str.sprintf("Temperature: <font color=blue>%.2f°C (%.2f°C-%.2f°C)</font>|VCCint: <b>%.3fV (%.3fV-%.3fV)</b>|VCCaux: <b>%.3fV (%.3fV-%.3fV)</b>|Vrefp: <b>%.3fV</b>|Vrefn: <b>%.3fV|</b>"
	//	"+3.3V: <b>%.3fV %.3fA %.3fW</b>|+12V: <b>%.3fV %.3fA %.3fW</b>|",
	//	g_sysmon.curTemp, g_sysmon.minTemp, g_sysmon.maxTemp,
	//	g_sysmon.curVCCint, g_sysmon.minVCCint, g_sysmon.maxVCCint,
	//	g_sysmon.curVCCaux, g_sysmon.minVCCaux, g_sysmon.maxVCCaux,
	//	g_sysmon.Vrefp, g_sysmon.Vrefn,
	//	g_val3.volt, g_val3.cur, g_val3.pow,
	//	g_val12.volt, g_val12.cur, g_val12.pow);

	//sendToClient(pClientSocket, "Server Response: Connected!");
	sendToClient(pClientSocket,
		"Server Response: Connected! " + g_brdInfo + "|"
	);
}

// ответ на запрос клиента
void amb_mon::slotReadClient()
{
	QTcpSocket* pClientSocket = (QTcpSocket*)sender();
	QDataStream in(pClientSocket);
	in.setVersion(QDataStream::Qt_5_7);
	for (;;)
	{
		QString str;
		if (!m_nNextBlockSize) {
			if (pClientSocket->bytesAvailable() < sizeof(quint16)) {
				break;
			}
			in >> m_nNextBlockSize;
		}

		if (pClientSocket->bytesAvailable() < m_nNextBlockSize) {
			break;
		}
		QTime   time;
		in >> time >> str;

		//QString strMessage =
		//	time.toString() + " " + "Client has sended - " + str;
		//m_ptxt->append(strMessage);

		//str.sprintf("%.3f°C | %.3fV | %.3fV", g_sysmon.curTemp, g_sysmon.curVCCint, g_sysmon.curVCCaux);
		str.sprintf("Temperature: <font color=green>%.2f°C (%.2f°C-%.2f°C)</font>|VCCint: <b>%.3fV (%.3fV-%.3fV)</b>|VCCaux: <b>%.3fV (%.3fV-%.3fV)</b>|Vrefp: <b>%.3fV</b>|Vrefn: <b>%.3fV</b>|"
			"+3.3V: <b>%.3fV %.3fA %.3fW</b>|+12V: <b>%.3fV %.3fA %.3fW</b>|", 
			g_sysmon.curTemp, g_sysmon.minTemp, g_sysmon.maxTemp,
			g_sysmon.curVCCint, g_sysmon.minVCCint, g_sysmon.maxVCCint,
			g_sysmon.curVCCaux, g_sysmon.minVCCaux, g_sysmon.maxVCCaux,
			g_sysmon.Vrefp, g_sysmon.Vrefn, 
			g_val3.volt, g_val3.cur, g_val3.pow,
			g_val12.volt, g_val12.cur, g_val12.pow);

		m_nNextBlockSize = 0;

		sendToClient(pClientSocket,
			"AMB Monitor: \"" + str + "\""
		);
	}
}

void amb_mon::sendToClient(QTcpSocket* pSocket, const QString& str)
{
	QByteArray  arrBlock;
	QDataStream out(&arrBlock, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_5_7);
	out << quint16(0) << QTime::currentTime() << str;

	out.device()->seek(0);
	out << quint16(arrBlock.size() - sizeof(quint16));

	pSocket->write(arrBlock);
}