#include "fmc132p_mon.h"
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
	REAL64	Vrefp;			// Внешнее опорное напряжение (плюс)
	REAL64	Vrefn;			// Внешнее опорное напряжение  (минус)
} SYSMON_PARAM, *PSYSMON_PARAM;

static SYSMON_PARAM g_sysmon;
static U32 g_smonStatus;


static REAL64 g_fVCCintNominal = 0.0;	// Номинал питания ядра
static REAL64 g_fVCCauxNominal = 0.0;	// Номинал питания ПЛИС
//REAL64 g_fVrefpNominal = 0.0;	// Номинал внешнего опорного напряжения (плюс)
//REAL64 g_fVrefnNominal = 0.0;	// Номинал внешнего опорного напряжения (минус)

static BRD_Handle g_hDevice;
static BRD_Handle g_hSysMon;

typedef struct _INA219MON_PARAM
{
	REAL64	volt;		// Текущее напряжение напряжения питания
	REAL64	cur;		// Текущее напряжение тока питания
	REAL64	pow;		// Минимальное напряжение мощности питания
} INA219MON_PARAM, *PINA219MON_PARAM;

static INA219MON_PARAM g_val12;

static REAL64 g_voltage[8];
static REAL64 g_temp;
static REAL64 g_vcc;
bool g_pwm_en;
bool g_pwm_inv;
int g_pwm_threshold;


static QString g_brdInfo;

static U16 g_devid;

fmc132p_mon::fmc132p_mon(QWidget *parent)
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
		return;
	}
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
	BRD_VoltNominals voltNominals;
	BRD_ctrl(g_hSysMon, 0, BRDctrl_SYSMON_GETVNOMINALS, &voltNominals);
	g_fVCCintNominal = voltNominals.vccint;
	g_fVCCauxNominal = voltNominals.vccaux;
	//g_fVrefpNominal = voltNominals.vrefp;
	//g_fVrefnNominal = voltNominals.vrefn;

	g_pwm_en = false;
	g_pwm_inv = false;
	g_pwm_threshold = 0;

	getValSysMon();
	DisplayVoltTable();

	getValSensMon();
	DisplayIna219Table();
	DisplayLtc2991Table();

	smonTimer = startTimer(1000);

	labelCurTemp->setToolTip("Текущая температура кристалла");
	labelMinTemp->setToolTip("Минимальная температура кристалла");
	labelMaxTemp->setToolTip("Максимальная температура кристалла");

	// реализация сворачивания в Tray
	minimizeAction = new QAction(tr("Mi&nimize"), this);
    connect(minimizeAction, SIGNAL(triggered()), this, SLOT(hide()));
    restoreAction = new QAction(tr("&Restore"), this);
    connect(restoreAction, SIGNAL(triggered()), this, SLOT(showNormal()));
    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

	// создаем контекстное меню для Tray
	trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(minimizeAction);
    trayIconMenu->addAction(restoreAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu); //  Устанавливаем контекстное меню на иконку Tray

    //QIcon icon = QIcon(":/fmc132p_mon/images/pinion-icon.png");
    //QIcon icon = QIcon(":/fmc132p_mon/images/weather256.png");
	QIcon icon = QIcon(":/amb_mon/images/scale_ruler.png");
	trayIcon->setIcon(icon); // устанавливаем иконку Tray
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
	trayIcon->setToolTip(strTray); // задаем всплывающую подсказку для Tray 
    //trayIcon->setToolTip(tr("System Monitor"));

	// подключаем сигнал нажатия на иконку к обработчику данного нажатия
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

	minimizeAction->setEnabled(true);
	restoreAction->setEnabled(false);
	trayIcon->show(); // показываем иконку приложения в Tray

}

void fmc132p_mon::setVisible(bool visible)
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
void fmc132p_mon::closeEvent(QCloseEvent * event)
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
void fmc132p_mon::iconActivated(QSystemTrayIcon::ActivationReason reason)
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

void fmc132p_mon::showMessage()
{
    //QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::MessageIcon(0);
    //QIcon icon = QIcon(":/images/heart.svg"), tr("Heart");
    trayIcon->showMessage("Systray", "System Monitor", QSystemTrayIcon::Information, 100);
    //trayIcon->showMessage("Systray", "Register Tester");
}

fmc132p_mon::~fmc132p_mon()
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

void fmc132p_mon::timerEvent(QTimerEvent *event)
{
	if(event->timerId() == smonTimer)
	{
		getValSysMon();
		DisplayVoltTable();

		getValSensMon();
		DisplayIna219Table();
		DisplayLtc2991Table();

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

void fmc132p_mon::getValSysMon()
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
}

void fmc132p_mon::DisplayVoltTable()
{
	REAL64 err;
    //volt_table->setColumnCount(5);
    //volt_table->setRowCount(4);
//	volt_table->setColumnWidth(0, 50);
//	volt_table->setColumnWidth(1, 50);
//	volt_table->setColumnWidth(2, 50);
//	volt_table->setColumnWidth(3, 50);
//	QStringList RowHeaderLabels = (QStringList() << "VCCint" << "VCCaux" << "Vrefp" << "Vrefn");
	QStringList RowHeaderLabels = (QStringList() << "VCCint" << "VCCaux");
	volt_table->setVerticalHeaderLabels(RowHeaderLabels);
	QStringList ColHeaderLabels = (QStringList() << "Current, V" << "Error, %" << "Nominal, V" << "Maximum, V" << "Minimum, V");
	volt_table->setHorizontalHeaderLabels(ColHeaderLabels);

	char buf[64];
	int j = 0;
	QTableWidgetItem *numItem[4*4];
	QBrush itemBrushGreen(Qt::darkGreen);
	QBrush itemBrushRed(Qt::red);
	for(int iRow = 0; iRow < 2; iRow++)
	{
		//volt_table->insertRow(iRow);
		//volt_table->setRowHeight(iRow, 20);

		// current 
		if(iRow)
			sprintf(buf, "%.3f", g_sysmon.curVCCaux);
		else
			sprintf(buf, "%.3f", g_sysmon.curVCCint);

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
		if(iRow)
			sprintf(buf, "%.3f", g_fVCCauxNominal);
		else
			sprintf(buf, "%.3f", g_fVCCintNominal);

		numItem[j] = new QTableWidgetItem(buf);
		numItem[j]->setFont(itemFont);
		numItem[j]->setTextAlignment(Qt::AlignCenter);
		volt_table->setItem(iRow, 2, numItem[j++]);

		// max 
		if(iRow)
			sprintf(buf, "%.3f", g_sysmon.maxVCCaux);
		else
			sprintf(buf, "%.3f", g_sysmon.maxVCCint);

		numItem[j] = new QTableWidgetItem(buf);
		numItem[j]->setFont(itemFont);
		numItem[j]->setTextAlignment(Qt::AlignCenter);
		volt_table->setItem(iRow, 3, numItem[j++]);

		// min
		if(iRow)
			sprintf(buf, "%.3f", g_sysmon.minVCCaux);
		else
			sprintf(buf, "%.3f", g_sysmon.minVCCint);

		numItem[j] = new QTableWidgetItem(buf);
		numItem[j]->setFont(itemFont);
		numItem[j]->setTextAlignment(Qt::AlignCenter);
		volt_table->setItem(iRow, 4, numItem[j++]);

	}
}

void fmc132p_mon::getValSensMon()
{
	S32 status;
	QString strInfo;

	BRDextn_Fmc132PSensors mon_val;

	mon_val.pwmEnbl = g_pwm_en;
	mon_val.pwmInv = g_pwm_inv;
	mon_val.pwmThres = g_pwm_threshold;
		
	mon_val.mode = 0;
	status = BRD_extension(g_hDevice, 0, BRDextn_SENSORS, &mon_val);
	if (!BRD_errcmp(status, BRDerr_OK))
		BRDC_printf(_BRDC(" Error by reading chip INA219 or LTC2991 \n\n"));

	g_val12.volt = mon_val.voltage;
	g_val12.cur = mon_val.current;
	g_val12.pow = mon_val.power;

	for(int i = 0; i < 8; i++)
		g_voltage[i] = mon_val.inpv[i];

	g_temp = mon_val.tint;
	g_vcc = mon_val.vcc;

}

void fmc132p_mon::DisplayIna219Table()
{
	QStringList RowHeaderLabels = (QStringList() << "+12V");
	pow_table->setVerticalHeaderLabels(RowHeaderLabels);
	QStringList ColHeaderLabels = (QStringList() << "Voltage (V)" << "Current (A)" << "Power (W)");
	pow_table->setHorizontalHeaderLabels(ColHeaderLabels);

	char buf[64];
	int j = 0;
	QTableWidgetItem *numItem[4 * 4];
	QBrush itemBrushGreen(Qt::darkGreen);
	QBrush itemBrushRed(Qt::red);

	// voltage
	sprintf(buf, "%.3f", g_val12.volt);
	numItem[j] = new QTableWidgetItem(buf);
	QFont itemFont = numItem[j]->font();
	itemFont.setBold(1);
	numItem[j]->setFont(itemFont);
	//numItem[j]->setForeground(itemBrushGreen);
	numItem[j]->setTextAlignment(Qt::AlignCenter);
	pow_table->setItem(0, 0, numItem[j++]);

	// current 
	sprintf(buf, "%.3f", g_val12.cur);
	numItem[j] = new QTableWidgetItem(buf);
	itemFont = numItem[j]->font();
	itemFont.setBold(1);
	numItem[j]->setFont(itemFont);
	//numItem[j]->setForeground(itemBrushGreen);
	numItem[j]->setTextAlignment(Qt::AlignCenter);
	pow_table->setItem(0, 1, numItem[j++]);

	// power
	sprintf(buf, "%.3f", g_val12.pow);
	numItem[j] = new QTableWidgetItem(buf);
	itemFont = numItem[j]->font();
	itemFont.setBold(1);
	numItem[j]->setFont(itemFont);
	//numItem[j]->setForeground(itemBrushGreen);
	numItem[j]->setTextAlignment(Qt::AlignCenter);
	pow_table->setItem(0, 2, numItem[j++]);
}

//if (BRD_errcmp(status, BRDerr_OK))
//{
//	BRDC_printf(_BRDC(" 12V - %4.3f Volt     %4.3f A     %4.3f Vatt     \n"), mon_val.voltage, mon_val.current, mon_val.power);
//	BRDC_printf(_BRDC(" VIO_B_M2C - %4.3f Volt     VADJ - %4.3f Volt     3.3V_FMC - %4.3f Volt    1V_AVCC - %4.3f Volt \n"), mon_val.inpv[0], mon_val.inpv[1], mon_val.inpv[2], mon_val.inpv[3]);
//	BRDC_printf(_BRDC(" 1.2V_AVTT - %4.3f Volt     0.9V - %4.3f Volt     1.2V - %4.3f Volt   2.5V - %4.3f Volt \n"), mon_val.inpv[4], mon_val.inpv[5], mon_val.inpv[6], mon_val.inpv[7]);
//	BRDC_printf(_BRDC(" Chip Temperature: %7.3f C,    Vcc - %4.3f Volt\n"), mon_val.tint, mon_val.vcc);
//	BRDC_printf(_BRDC(" \n"));
//}
//else
//	BRDC_printf(_BRDC(" Error by reading chip0 INA219  \n"));
void fmc132p_mon::DisplayLtc2991Table()
{
	QStringList ColHeaderLabels = (QStringList() << "VIO_B_M2C" << "VADJ" << "3.3V_FMC" << "1V_AVCC" << "1.2V_AVTT" << "0.9V" << "1.2V" << "2.5V");
	//QStringList ColHeaderLabels = (QStringList() << "VIO" << "VADJ" << "3.3V" << "1V_" << "1.2V_" << "0.9V" << "1.2V" << "2.5V");
	volt_132_table->setHorizontalHeaderLabels(ColHeaderLabels);

	char buf[64];
	//int j = 0;
	QTableWidgetItem *numItem[8];
	QBrush itemBrushGreen(Qt::darkGreen);
	QBrush itemBrushRed(Qt::red);

	for (int iCol = 0; iCol < 8; iCol++)
	{
		sprintf(buf, "%.3f", g_voltage[iCol]);
		numItem[iCol] = new QTableWidgetItem(buf);
		QFont itemFont = numItem[iCol]->font();
		itemFont.setBold(1);
		numItem[iCol]->setFont(itemFont);
		//numItem[j]->setForeground(itemBrushGreen);
		numItem[iCol]->setTextAlignment(Qt::AlignCenter);
		volt_132_table->setItem(0, iCol, numItem[iCol]);
	}

	QString strInfo;
	strInfo.sprintf("Tint: <b>%.3f °C</b>", g_temp);
	labelIntTemp->setText(strInfo);

	strInfo.sprintf("Vcc: <b>%.3f V</b>", g_vcc);
	labelVcc->setText(strInfo);
}

/*virtual*/ void fmc132p_mon::slotNewConnection()
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
void fmc132p_mon::slotReadClient()
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
		str.sprintf("Temperature: <font color=green>%.2f°C (%.2f°C-%.2f°C)</font>|VCCint: <b>%.3fV (%.3fV-%.3fV)</b>|VCCaux: <b>%.3fV (%.3fV-%.3fV)</b>|"
			"+12V: <b>%.3fV %.3fA %.3fW</b>|", 
			g_sysmon.curTemp, g_sysmon.minTemp, g_sysmon.maxTemp,
			g_sysmon.curVCCint, g_sysmon.minVCCint, g_sysmon.maxVCCint,
			g_sysmon.curVCCaux, g_sysmon.minVCCaux, g_sysmon.maxVCCaux,
			//g_sysmon.Vrefp, g_sysmon.Vrefn, 
			//g_val3.volt, g_val3.cur, g_val3.pow,
			g_val12.volt, g_val12.cur, g_val12.pow);

		m_nNextBlockSize = 0;

		sendToClient(pClientSocket,
			"AMB Monitor: \"" + str + "\""
		);
	}
}

void fmc132p_mon::sendToClient(QTcpSocket* pSocket, const QString& str)
{
	QByteArray  arrBlock;
	QDataStream out(&arrBlock, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_5_7);
	out << quint16(0) << QTime::currentTime() << str;

	out.device()->seek(0);
	out << quint16(arrBlock.size() - sizeof(quint16));

	pSocket->write(arrBlock);
}

void fmc132p_mon::ClickedPwmEn()
{
	g_pwm_en = checkPwmEn->isChecked();
}

void fmc132p_mon::ClickedPwmInv()
{
	g_pwm_inv = checkPwmInv->isChecked();
}

void fmc132p_mon::EditPwmThreshold()
{
	g_pwm_threshold = spinPwm->value();
}

