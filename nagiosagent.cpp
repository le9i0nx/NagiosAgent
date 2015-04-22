/*
 *	Copyright (C) 2009 by Chebotarev Roman <roma@ultranet.ru>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "nagiosagent.h"

const QString NagiosAgent::configDirName = "NetPatch";
const QString NagiosAgent::programmName = "NagiosAgent";
const QString NagiosAgent::commonSettings = "CommonSettings";
const QString NagiosAgent::configServersArray = "NagiosServers";
const QString NagiosAgent::trayToolTipHeader = "Nagios Agent";
const QString NagiosAgent::hostUpSnd = "HostDown.wav";
const QString NagiosAgent::hostDownSnd = "HostUp.wav";
const QString NagiosAgent::troubleSnd = "trouble.wav";
const QString NagiosAgent::programmTranslatorName = "NagiosAgentLang_";
const QString NagiosAgent::commonTranslatorName = "qt_";
const QString NagiosAgent::hostUpIcon1 = ":/icons/icon16x16-host-up-1.png";
const QString NagiosAgent::hostUpIcon2 = ":/icons/icon16x16-host-up-2.png";
const QString NagiosAgent::hostDownIcon1 = ":/icons/icon16x16-host-down-1.png";
const QString NagiosAgent::hostDownIcon2 = ":/icons/icon16x16-host-down-2.png";
const QString NagiosAgent::normalTrayIcon = ":/icons/icon16x16.png";
const QString NagiosAgent::problemTrayIcon = ":/icons/icon16x16-problem.png";
const QString NagiosAgent::serverProblemTrayIcon = ":/icons/icon16x16-server-problem.png";

NagiosAgent::NagiosAgent(QWidget *parent)
    : QWidget(parent,
    		Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint |
				Qt::WindowMaximizeButtonHint | Qt::WindowTitleHint )
{
    ui.setupUi(this);

	//Позиционируем окно в центре экрана
	QRect screenRect = QApplication::desktop()->screenGeometry(this);
	setGeometry(screenRect.width()/2 - defaultWidth/2, screenRect.height()/2 - defaultHeight/2,
				defaultWidth, defaultHeight);

	//Добавляем иконку в заголовок
	setWindowIcon(QIcon(normalTrayIcon));

    //Создаём меню профилей
	createProfilesMenu();
	quietUpdate = true;

	QList<SessionConfig> sessionsConfigs;	//Список конфигураций профилей

	if(!readConfig(sessionsConfigs))
    {
		makeDefaultConfig();
		if(!readConfig(sessionsConfigs))
		{
			QMessageBox::critical(this, "Nagios Agent",
					tr("Default configuration is created but can't be read."));
			exit(-1);
			return;
		}
    }

	//Задаём стартовую позицию для popup окон
	informWinPt.rx() = constStartX;
    informWinPt.ry() = constStartY;

    //Создаём объект проверяющий наличие обновлений
    updater = new HttpUpdater(UPDATE_SERVER, UPDATE_PORT, UPDATE_PATH, UPDATE_INFO);

    //Создаём набор серверных сессий
    Q_FOREACH(SessionConfig sConfig, sessionsConfigs)
    {
    	ServerSession * sess = new ServerSession(sConfig);
   		serversSessions.append(sess);
    }

    createTrayIcon();	//Создаём иконку и контекстные меню для трея
    connectSignals();	//Подключаем все необходимы Qt-сигналы

    //Если в ходе выполнения функции readConfig() не найдено конфигураций для серверов
    //и соответственно не было созданно ни одной сессии предлагаем сконфигурировать программу.
    if(serversSessions.count() < 1)
	{
		QMessageBox::warning(this, tr("Nagios Agent: configuration error."),
				tr("No profile configurations can be found. "
				"Please configure programm before using."));
    	showAndReadConfig();
    	configChanged = false;
    	profileNew();
		return;
	}

    if(ServerSession::activeSessionsCount() < 1)	//Если число запущенных серверных сессий меньше одного -
    {												//значит у нас нет правильных активных конфигураций.
    	MSG("Can't launch any server profile.");
    	trouble(tr("Can't launch any server profile."));
    	setSuitableIcon();
    	return;
    }

    //Если не отключено автоматическое обновление - запускаем процесс проверки наличия обновлений
    if(!commonConfig.disableAutoUpdate)
    {
    	ui.actionCheckUpdates->setEnabled(false);
    	updater->check();
    	timerUpdateChecker.setSingleShot(true);
    	timerUpdateChecker.setInterval(defaultUpdateCheckInterval);
		timerUpdateChecker.start();
    }
}

NagiosAgent::~NagiosAgent()
{
	qDeleteAll(serversSessions);
}


//Внутренние функции-утилиты

//Служебные функции

void NagiosAgent::initAppResources()
{
	//Проверяем наличие домашнего каталога директории, если такового нет - создаём.
#ifdef	Q_OS_WIN32
#define SETTINGS_DIR	"Application Data/"
#else
#define	SETTINGS_DIR	"."
#endif
	appDir.setPath(QDir::homePath() + "/" + SETTINGS_DIR + configDirName + "/" + programmName);
	if(!appDir.exists())
	{
		MSG("Creating application directory.");
		if(!appDir.mkpath(appDir.path()))
		{
			MSG("Can't create application directory. Using system TEMP directory as application directory");
			appDir.setPath(QDir::tempPath());
		}
	}

	//Загружаем кодеки и переводчики
	QTextCodec * codec = QTextCodec::codecForName("UTF-8");
	if(codec)
	{
		QTextCodec::setCodecForTr(codec);
		QTextCodec::setCodecForCStrings(codec);
		QTextCodec::setCodecForLocale(codec);
		QTextCodec::setCodecForTr(QTextCodec::codecForName("utf8"));
		installTranslators(!commonConfig.disableLocalization);
	}

	//Проверяем наличие звуков для сигнализации событий,
    //если их нет - выгружаем из ресурсов в файловую систему потому что:
    //"Note that QSound does not support resources. This might be fixed in a future Qt version."
    //( http://doc.trolltech.com/4.5/qsound.html )
	if(! QFile::exists(appDir.path() + "/" + hostUpSnd))
		QFile::copy(":/sounds/" + hostUpSnd, appDir.path() + "/" + hostUpSnd);
	if(! QFile::exists(appDir.path() + "/" + hostDownSnd))
		QFile::copy(":/sounds/" + hostDownSnd, appDir.path() + "/" + hostDownSnd);
	if(! QFile::exists(appDir.path() + "/" + troubleSnd))
		QFile::copy(":/sounds/" + troubleSnd, appDir.path() + "/" + troubleSnd);
}

void NagiosAgent::installTranslators(const bool install)
{
	if(install)
	{
		//Загружаем файлы переводов. Сперва пробуем загрузить из файловой системы.
		//Если не получается - пробуем загрузить из ресурсов.
		QString locale = QLocale::system().name();
		MSG("Installing translators. System locale is:" << locale);
		if(!programmTranslator.load((appDir.path() + "/" + programmTranslatorName) + locale))
		{
			MSG("Can't load programm translator '" + programmTranslatorName + locale +
					+ "' from filesystem. Loading from internal programm resources.");
			MSG("Loading " << ((":/translators/NagiosAgentLang_") + locale));
			programmTranslator.load((":/translators/NagiosAgentLang_") + locale);
		}
		if(!commonTranslator.load((appDir.path() + "/" + commonTranslatorName) + locale))
		{
			MSG("Can't load common translator '" + commonTranslatorName + locale +
					"' from filesystem. Loading from internal programm resources.");
			MSG("Loading " << ((":/translators/qt_") + locale));
			commonTranslator.load((":/translators/qt_") + locale);
		}

		if(programmTranslator.isEmpty())
			MSG("Error: empty programmTranslator! Can't install.");
		else
			qApp->installTranslator(&programmTranslator);
		if(commonTranslator.isEmpty())
			MSG("Error: empty commonTranslator! Cant't install.");
		else
			qApp->installTranslator(&commonTranslator);
	}
	else
	{
		MSG("Uninstall translators.");
		if(!programmTranslator.isEmpty())
			qApp->removeTranslator(&programmTranslator);
		if(!commonTranslator.isEmpty())
			qApp->removeTranslator(&commonTranslator);
	}
}

void NagiosAgent::connectSignals()
{
	connect(ui.btnAddHost, SIGNAL(clicked()), this, SLOT(addHost()));
	connect(ui.btnDelHost, SIGNAL(clicked()), this, SLOT(delHost()));
	connect(ui.btnAddService, SIGNAL(clicked()), this, SLOT(addService()));
	connect(ui.btnDelService, SIGNAL(clicked()), this, SLOT(delService()));
	connect(ui.btnOk, SIGNAL(clicked()), this, SLOT(configSaveAndClose()));
    connect(ui.actionQuit, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(ui.actionConfig, SIGNAL(triggered()), this, SLOT(showAndReadConfig()));
    connect(ui.actionCloseAll, SIGNAL(triggered()), this, SLOT(closeAllPopups()));
    connect(ui.actionAbout, SIGNAL(triggered()), this, SLOT(showAbout()));
    connect(&trayIcon, SIGNAL(messageClicked()), this, SLOT(trayMessageClicked()));
    connect(&timerFlash, SIGNAL(timeout()), this, SLOT(trayFlashed()));
    connect(ui.cbUseTrayMessages, SIGNAL(toggled(bool)), this, SLOT(useTrayMsgsClicked(bool)));
    connect(updater, SIGNAL(checkUpdateFinished()), this, SLOT(checkUpdateFinished()));
    connect(ui.actionCheckUpdates, SIGNAL(triggered()), this, SLOT(checkUpdates()));
    connect(&timerUpdateChecker, SIGNAL(timeout()), this, SLOT(checkUpdatesQuiet()));
    connect(ui.cboxProfileName, SIGNAL(currentIndexChanged(QString)), this, SLOT(serverProfileChanged(QString)));
    connect(ui.cbUseHttps, SIGNAL(toggled(bool)), this, SLOT(useHttpsClicked(bool)));

	connect(ui.actionSaveProfile, SIGNAL(triggered()), this, SLOT(profileSave()));
	connect(ui.actionSetDefaultSettings, SIGNAL(triggered()), this, SLOT(profileSetDefault()));
	connect(ui.actionNewProfile, SIGNAL(triggered()), this, SLOT(profileNew()));
	connect(ui.actionDeleteProfile, SIGNAL(triggered()), this, SLOT(profileDelete()));
	connect(ui.actionRenameProfile, SIGNAL(triggered()), this, SLOT(profileRename()));

	connectConfModifSignals(ui.tabServersConfig);
	Q_FOREACH(ServerSession * sess, serversSessions)
	{
	    connect(sess, SIGNAL(newDataAvailable(const QList<NagiosObjectInfo> & )),
				this, SLOT(newStatusData(const QList<NagiosObjectInfo> & )));
	    connect(sess, SIGNAL(sendInformation(const QString & , ServerSession::InfoMessageType )),
				this, SLOT(reciveInfoMessage(const QString & , ServerSession::InfoMessageType )));
	}

}

void NagiosAgent::connectConfModifSignals(QObject * parentObj)
{
	QObjectList controls = parentObj->children();
	Q_FOREACH(QObject *obj, controls)
	{
		if(qobject_cast<QAbstractButton *>(obj))
			connect(obj, SIGNAL(clicked()), this, SLOT(onConfigModify()));
		else if(qobject_cast<QLineEdit *>(obj))
			connect(obj, SIGNAL(textEdited(const QString &)), this, SLOT(onConfigModify(const QString&)));
		else if(qobject_cast<QAbstractSpinBox *>(obj))
			connect(obj, SIGNAL(valueChanged(const QString &)), this, SLOT(onConfigModify(const QString&)));
		else if(obj->children().count())
			connectConfModifSignals(obj);
	}
}

//Функция обновляющая координаты левого верхнего угла для следующего
//информационного окна - сдвигает вправо/вниз, либо возвращает
//значение координаты к исходному значению
void NagiosAgent::updateInformWinPt()
{
	QRect screenRect = QApplication::desktop()->screenGeometry(this);

	if(commonConfig.avoidOverlapMsgs)
	{
		int titleHeight = frameGeometry().height() - height();
		int	borderWidth = (frameGeometry().width() - width());

		if((informWinPt.rx() + InformationWindow::windowWidth)
				> (screenRect.width() - InformationWindow::windowWidth))
		{	//Стартовое значение
			informWinPt.rx() = constStartX;
			if((informWinPt.ry() + InformationWindow::windowHeight + titleHeight) >
				(screenRect.height() - InformationWindow::windowHeight + frameGeometry().height() - height()))
			{
				informWinPt.ry() = constStartY;
				informWinPt.rx() = constStartX;
			}
			else
				informWinPt.ry() += InformationWindow::windowHeight + titleHeight;
			return;
		}
		else
		{
			informWinPt.rx() += InformationWindow::windowWidth + borderWidth;
			return;
		}

	}
	else
	{
		if(informWinPt.rx() > screenRect.width() - 400)
		{	//Стартовое значение
			informWinPt.rx() = constStartX;
		}
		else
			informWinPt.rx() += 15;

		if(informWinPt.ry() > screenRect.height() - 220)
		{	//Стартовое значение
			informWinPt.ry() = constStartY;
		}
		else
			informWinPt.ry() += 15;
	}
}

void NagiosAgent::saveCommonConfig()
{
	QSettings config(configDirName, programmName);

	config.beginGroup(commonSettings);

	commonConfig.avoidOverlapMsgs =	ui.cbAvoidOverlapMsgs->isChecked();
	config.setValue("AvoidOverlappingMessages", commonConfig.avoidOverlapMsgs);

	if(commonConfig.disableLocalization != ui.cbDisableLocalization->isChecked())
	{
		commonConfig.disableLocalization = ui.cbDisableLocalization->isChecked();
		installTranslators(!commonConfig.disableLocalization);
		ui.retranslateUi(this);
		loadServerConfigToGui();
		config.setValue("DisableLocalization", commonConfig.disableLocalization);
	}

	config.setValue("DisableAutoUpdate", ui.cbDisableAutoUpdate->isChecked());

	if(commonConfig.disableAutoUpdate != ui.cbDisableAutoUpdate->isChecked())
	{
		commonConfig.disableAutoUpdate = ui.cbDisableAutoUpdate->isChecked();
	    if(commonConfig.disableAutoUpdate)
	    	timerUpdateChecker.stop();
	    else
	    {
	    	ui.actionCheckUpdates->setEnabled(false);
	    	updater->check();
	    	timerUpdateChecker.setSingleShot(true);
	    	timerUpdateChecker.setInterval(defaultUpdateCheckInterval);
			timerUpdateChecker.start();
	    }
	}

	if(ui.rbHostOnLMK->isChecked())
	{
		config.setValue("NumMenuOnLMK", hostsMenu);
		commonConfig.menuNumOnLMK = hostsMenu;
	}
	else if(ui.rbSrvcsOnLMK->isChecked())
	{
		config.setValue("NumMenuOnLMK", srvcsMenu);
		commonConfig.menuNumOnLMK = srvcsMenu;
	}
	else
	{
		config.setValue("NumMenuOnLMK", hostsSrvcsMenu);
		commonConfig.menuNumOnLMK = hostsSrvcsMenu;
	}
	config.endGroup();
}

inline bool NagiosAgent::needSave()
{
	//Проверяем не изменились ли настройки текущего профиля
	if(configChanged && serversSessions.count() &&
		QMessageBox::information(this,
				tr("Nagios Agent: configuration changed"),
				tr("Configuration of profile '") +
				currentLoadedProfile +
				tr("' was changed. Save changes?"),
				QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) ==
			QMessageBox::Yes)
		return true;
	return false;
}

void NagiosAgent::closeEvent(QCloseEvent * event)
{
	hide();
	event->ignore();
}

QString NagiosAgent::makeProblemLink(const SessionConfig & config, MenuTypes type)
{
	QString port;
	if ((config.useHttps && (config.serverPort != ServerSession::defaultHttpsServerPort ))
			||
		!(config.useHttps && (config.serverPort != ServerSession::defaultHttpServerPort))
	)
		port = ":" + config.serverPort;

	return (config.useHttps ? "https://" : "http://") +
			config.serverAddress + port +
			config.execDir +
			( (type == hostsMenu) ? config.hostsRequest : config.servicesRequest);
}

//*******************************************
//Функции работы с конфигурацией

bool NagiosAgent::readConfig(QList<SessionConfig> & sessionsConfigs)
{
	QSettings config(configDirName, programmName);

	//Проверка на возможность работы с конфигом
	if(config.status() != QSettings::NoError)
		return false;

	//Проверка наличия группы общих настроек
	config.beginGroup(commonSettings);
	if(config.value("DisableLocalization").isNull())	//Если не найден этот параметр, то нет и самой
	{													//группы.
		config.endGroup();
		//Возможно есть конфиг старой версии, пытаемся запустить его
		if(!convertFromOldConfig(config, sessionsConfigs))
			return false;
		else	//Старый конфиг прочтён успешно - выходим т.к. версии ниже 1.2
			return true;	//поддерживали только один сервер
	}

	//Читаем общие настройки
	commonConfig.avoidOverlapMsgs = config.value("AvoidOverlappingMessages", defaultAvoidOverlapMsgs).toBool();
	commonConfig.disableLocalization = config.value("DisableLocalization", defaultDisableLocalization).toBool();
	commonConfig.disableAutoUpdate = config.value("DisableAutoUpdate", defaultDisableAutoUpdate).toBool();
	commonConfig.menuNumOnLMK = config.value("NumMenuOnLMK", defaultMenuNumOnLMK).toInt();

	config.endGroup();

	initAppResources();		//Создаём необходимые для работы каталоги, выгружаем звуки, загружаем переводчики
	ui.retranslateUi(this);	//Переводим интерфейс под текущую локаль
	loadCommonConfigToGui();	//Применяем общие настройки. Вкладка "General settings"

	//Проверка наличия конфигураций серверов
	int count = config.beginReadArray(configServersArray);
	if(count < 1)		//Успешно создан дефолтный конфиг нового формата,
		return true;	//но в нём не найдено ни одного сервера


	//Производим чтение списка конфигов серверов и добавление их в sessionsConfigs
	for(int i = 0; i < count; ++i)
	{
		config.setArrayIndex(i);
		sessionsConfigs.append(readServerConfig(config));
	}

	config.endArray();

	return true;
}

bool NagiosAgent::convertFromOldConfig(QSettings & config, QList<SessionConfig> & sessionsConfigs)
{

	QString serverAddress = config.value("ServerAddress").toString();
	if(serverAddress.isEmpty())
		return false;

	//Создаём дефолтное имя профиля
	config.setValue("ProfileName", tr("Server <") + serverAddress + ">");

	//Не начинаем открывать массив серверных конфигураций т.к. в версиях ниже 1.2
	//конфигурация хранилась непосредственно в корневом разделе
	SessionConfig sConfig = readServerConfig(config);
	//Конвертируем списки хостов и сервисов из старого формата в новый
	if(sConfig.hostsList.count())
	{
		sConfig.hostsList = sConfig.hostsList.first().split("|", QString::SkipEmptyParts);
		sConfig.hostsList.erase(std::unique(sConfig.hostsList.begin(), sConfig.hostsList.end()),
				sConfig.hostsList.end());
	}
	if(sConfig.srvcsList.count())
	{
		sConfig.srvcsList = sConfig.srvcsList.first().split("|", QString::SkipEmptyParts);
		sConfig.srvcsList.erase(std::unique(sConfig.srvcsList.begin(), sConfig.srvcsList.end()),
				sConfig.srvcsList.end());
	}
	//Возможно старый конфиг был не верен
	if(!sConfig.validConfig)
	{
		QMessageBox::warning(this, tr("Nagios Agent: warning"),
				tr("Old configuration is found but is incorrect. Please reconfigure programm."));
		sConfig.profileEnabled = false;
	}

	//Читаем общие настройки из старого конфига
	commonConfig.avoidOverlapMsgs = defaultAvoidOverlapMsgs;
	commonConfig.disableLocalization = config.value("DisableLocalization", defaultDisableLocalization).toBool();
	commonConfig.disableAutoUpdate = config.value("DisableAutoUpdate", defaultDisableAutoUpdate).toBool();
	commonConfig.menuNumOnLMK = config.value("NumMenuOnLMK", defaultMenuNumOnLMK).toInt();

	//Записываем их в новый конфиг
	config.beginGroup(commonSettings);
	config.setValue("AvoidOverlappingMessages", defaultAvoidOverlapMsgs);
	config.setValue("DisableLocalization", commonConfig.disableLocalization);
	config.setValue("DisableAutoUpdate", commonConfig.disableAutoUpdate);
	config.setValue("NumMenuOnLMK", commonConfig.menuNumOnLMK);
	config.endGroup();

	//Добавляем в набор конфигураций
	sessionsConfigs.append(sConfig);
	//Сохраняем конфигурацию нового формата
	config.beginWriteArray(configServersArray);
	config.setArrayIndex(0);
	saveServerValues(config, sConfig);
	config.endArray();

	initAppResources();		//Создаём необходимые для работы каталоги, выгружаем звуки, загружаем переводчики
	ui.retranslateUi(this);	//Переводим интерфейс под текущую локаль
	loadCommonConfigToGui();	//Применяем общие настройки. Вкладка "General settings"

	return true;
}

void NagiosAgent::makeDefaultConfig()
{
	QSettings config(configDirName, programmName);

	//Создаём группу общих настроек
	config.beginGroup(commonSettings);
	config.setValue("AvoidOverlappingMessages", defaultAvoidOverlapMsgs);
	config.setValue("DisableLocalization", defaultDisableLocalization);
	config.setValue("DisableAutoUpdate", defaultDisableAutoUpdate);
	config.setValue("NumMenuOnLMK", defaultMenuNumOnLMK);
	config.endGroup();
	//Создаём массив настроек для серверов. Устанавливаем нулевой размер.
	config.beginWriteArray(configServersArray, 0);
	config.setArrayIndex(0);
	config.endArray();
}

void NagiosAgent::setDefaultConfigValues(const bool resetAll)
{
	//Вкладка "server settings"
	if(resetAll)
		ui.cboxProfileName->clear();
	ui.cbProfileEnabled->setChecked(ServerSession::defaultProfileEnabled);
	//Вкладка "Server"
	if(resetAll)
		ui.edServerAddress->clear();
	ui.edServerPort->setText(ServerSession::defaultHttpServerPort);
	ui.spinBoxPeriod->setValue(ServerSession::defaultRequestPeriod);
	ui.cbUseHttps->setChecked(ServerSession::defaultUseHttps);
	ui.cbAutoClose->setChecked(ServerSession::defaultAutoClose);
	ui.spinBoxClose->setValue(ServerSession::defaultAutoCloseTime.toInt());
	ui.cbExecDir->setChecked(ServerSession::defaultUseUserDefExecDir);
	ui.edExecDir->setText(ServerSession::defaultExecDir);
	ui.cbHostsRequest->setChecked(ServerSession::defaultUseUserDefHostsRequest);
	ui.edHostsRequest->setText(ServerSession::defaultHostRequest);
	ui.cbServicesRequest->setChecked(ServerSession::defaultUseUserDefSrvcsRequest);
	ui.edServicesRequest->setText(ServerSession::defaultSrvcsRequest);
	ui.cbUseHTTPAuth->setChecked(ServerSession::defaultUseHTTPAuth);
	ui.edLogin->clear();
	ui.edPassword->clear();
	//Вкладка "Host filtering"
	ui.rbAllExceptSpecifedHosts->setChecked(ServerSession::defaultAllExceptSpecifedHosts);
	ui.listHosts->clear();
	ui.edAddHost->clear();
	ui.cbUseFilterRegExpHosts->setChecked(ServerSession::defaultUseFilterRegExpHosts);
	//Вкладка "Filtering service"
	ui.rbAllExceptSpecifedSrvcs->setChecked(ServerSession::defaultAllExceptSpecifedSrvcs);
	ui.listServices->clear();
	ui.edAddService->clear();
	ui.cbUseFilterRegExpSrvcs->setChecked(ServerSession::defaultUseFilterRegExpSrvcs);
	//Вкладка "Statuses"
	ui.rbBaseRegExpConfig->setChecked(ServerSession::defaultUseBasicRegExpConfig);
	ui.cbHostDownStat->setChecked(ServerSession::defaultUseHostDownStat);
	ui.cbHostUnreachStat->setChecked(ServerSession::defaultUseHostUnreachStat);
	ui.cbServiceCritStat->setChecked(ServerSession::defaultUseSrvCritStat);
	ui.cbServiceWarnStat->setChecked(ServerSession::defaultUseSrvWarnStat);
	ui.cbServiceUnknownStat->setChecked(ServerSession::defaultUseSrvUnknStat);
	ui.cbHostsRegExp->setChecked(ServerSession::defaultUseUserDefHostsRegExp);
	ui.edHostsRegExp->setText(ServerSession::defaultExpertHostsRegExp);
	ui.edHostStatRxPos->setText(QString::number(ServerSession::defaultHostStatRxPos));
	ui.edHostLinkRxPos->setText(QString::number(ServerSession::defaultHostLinkRxPos));
	ui.edHostNameRxPos->setText(QString::number(ServerSession::defaultHostNameRxPos));
	ui.cbSrvcsRegExp->setChecked(ServerSession::defaultUseUserDefSrvcsRegExp);
	ui.edSrvcsRegExp->setText(ServerSession::defaultExpertSrvcsRegExp);
	ui.edSrvStatRxPos->setText(QString::number(ServerSession::defaultSrvStatRxPos));
	ui.edSrvLinkRxPos->setText(QString::number(ServerSession::defaultSrvLinkRxPos));
	ui.edSrvNameRxPos->setText(QString::number(ServerSession::defaultSrvNameRxPos));

	//Вкладка "Misc"
	ui.cbEnableHosts->setChecked(ServerSession::defaultEnableHostsMonitoring);
	ui.cbShowHosts->setChecked(ServerSession::defaultShowHostsMessages);
	ui.cbEnableSrvcs->setChecked(ServerSession::defaultEnableSrvcsMonitoring);
	ui.cbShowSrvcs->setChecked(ServerSession::defaultShowSrvcsMessages);
	ui.cbPopupsStayOnTop->setChecked(ServerSession::defaultPopupsStayOnTop);
	ui.cbUseTrayMessages->setChecked(ServerSession::defaultUseTrayMessages);
	ui.cbOpenUrlFromTray->setChecked(ServerSession::defaultOpenUrlFromTray);
	ui.cbFlashInTray->setChecked(ServerSession::defaultFlashesInTray);
	ui.cbDisableSounds->setChecked(ServerSession::defaultDisableSounds);
	ui.rbDenyInvalidCert->setChecked(ServerSession::defaultDenyInvalidCert);
}

int	NagiosAgent::profileNameIndex(const QString & name)
{
	//Открываем статичный конфиг
	QSettings config(configDirName, programmName);
	int profileIndex;
	int count = config.beginReadArray(configServersArray);
	//Проверка на дублирование имени профиля
	for(profileIndex = 0; profileIndex < count; ++profileIndex)
	{
		config.setArrayIndex(profileIndex);
		if(config.value("ProfileName").toString() == name)
			return profileIndex;
	}
	config.endArray();
	return -1;
}

//Читаем конфигурацию для отдельного профиля
SessionConfig NagiosAgent::readServerConfig(const QString profileName, const bool showError)
{
	QSettings config(configDirName, programmName);
	SessionConfig sConfig;

	if(profileName.isEmpty())	//Если не указано имя профиля - возвращаем первый попавшийся
		sConfig = readServerConfig(config, showError);
	else
	{
		config.beginReadArray(configServersArray);
		int serverIndex = profileNameIndex(profileName);
		if(serverIndex < 0)
		{
			MSG("Can't found such server configuration");
			return sConfig;
		}
		config.setArrayIndex(serverIndex);
		sConfig = readServerConfig(config, showError);
		config.endArray();
	}
	return sConfig;
}

//Читаем конфигурацию для отдельного профиля
SessionConfig NagiosAgent::readServerConfig(const QSettings & config, const bool showError)
{
	SessionConfig sConfig;

	sConfig.profileName = config.value("ProfileName").toString();
	sConfig.profileEnabled = config.value("ProfileEnabled", ServerSession::defaultProfileEnabled).toBool();

	//sConfig- = config.value("", );
	//Вкладка "Server"
	sConfig.serverAddress = config.value("ServerAddress").toString();
	sConfig.useHttps = config.value("UseHTTPS", ServerSession::defaultUseHttps).toBool();
	if(sConfig.useHttps)
		sConfig.serverPort = config.value("HTTPSPort", ServerSession::defaultHttpsServerPort).toString();
	else
		sConfig.serverPort = config.value("HTTPPort", ServerSession::defaultHttpServerPort).toString();
	sConfig.requestPeriod = config.value("RequestPeriod", ServerSession::defaultRequestPeriod).toInt();
	sConfig.autoClose = config.value("AutoClose", ServerSession::defaultAutoClose).toBool();
	sConfig.autoCloseTime = config.value("AutoCloseTime", ServerSession::defaultAutoCloseTime).toInt();
	sConfig.useUserDefExecDir = config.value("UseUserDefExecDir", ServerSession::defaultUseUserDefExecDir).toBool();
	if(sConfig.useUserDefExecDir)
	{
		sConfig.execDir = config.value("UserDefNagiosExecDir", ServerSession::defaultExecDir).toString();
		//Добавляем лидирующий слеш абсолютного пути (/cgi-bin...) если необходим
		if(sConfig.execDir.indexOf(QRegExp("^/")) < 0)
			sConfig.execDir.insert(0, "/");
		//Добавляем завершающий слеш пути исполняемого каталога (...cgi-bin/) если необходим
		if(sConfig.execDir.indexOf(QRegExp("/$")) < 0)
			sConfig.execDir.append("/");
	}
	else
		sConfig.execDir = ServerSession::defaultExecDir;

	sConfig.useUserDefHostRequest = config.value("UseUserDefHostsRequest", ServerSession::defaultUseUserDefHostsRequest).toBool();
	sConfig.hostsRequest = sConfig.useUserDefHostRequest ?
		config.value("UserDefHostsRequest", ServerSession::defaultHostRequest).toString() :
		ServerSession::defaultHostRequest;
	sConfig.useUserDefSrvcsRequest = config.value("UseUserDefSrvcsRequest", ServerSession::defaultUseUserDefSrvcsRequest).toBool();
	sConfig.servicesRequest = sConfig.useUserDefSrvcsRequest?
		config.value("UserDefServicesRequest", ServerSession::defaultSrvcsRequest).toString() :
		ServerSession::defaultSrvcsRequest;
	sConfig.useHTTPAuth = config.value("UseHTTPAuth", ServerSession::defaultUseHTTPAuth).toBool();
	sConfig.httpLogin = config.value("Login").toString();
	sConfig.httpPassword = config.value("Password").toString();
	//Вкладка "Filtering hosts"
	sConfig.allExceptSpecifedHosts = config.value("AllExceptSpecifedHosts", ServerSession::defaultAllExceptSpecifedHosts).toBool();
	sConfig.onlyListedHosts = config.value("OnlyListedHosts", ServerSession::defaultOnlyListedHosts).toBool();
	sConfig.hostsList = config.value("HostsList").toStringList();
	sConfig.hostsList.erase(std::unique(sConfig.hostsList.begin(), sConfig.hostsList.end()),
			sConfig.hostsList.end());
	sConfig.useFilterRegExpHosts = config.value("UseFilterRegExpHosts", ServerSession::defaultUseFilterRegExpHosts).toBool();
	//Вкладка "Filtering service"
	sConfig.allExceptSpecifedSrvcs = config.value("AllExceptSpecifedSrvcs", ServerSession::defaultAllExceptSpecifedSrvcs).toBool();
	sConfig.onlyListedSrvcs = config.value("OnlyListedSrvcs", ServerSession::defaultOnlyListedSrvcs).toBool();
	sConfig.srvcsList = config.value("ServicesList").toStringList();
	sConfig.srvcsList.erase(std::unique(sConfig.srvcsList.begin(), sConfig.srvcsList.end()),
			sConfig.srvcsList.end());
	sConfig.useFilterRegExpSrvcs = config.value("UseFilterRegExpSrvcs", ServerSession::defaultUseFilterRegExpSrvcs).toBool();
	//Вкладка "Statuses"
	sConfig.useBasicRegExpConfig = config.value("UseBasicRegExpConfig", ServerSession::defaultUseBasicRegExpConfig).toBool();
	sConfig.useHostDownStat = config.value("UseHostDownStat", ServerSession::defaultUseHostDownStat).toBool();
	sConfig.useHostUnreachStat = config.value("UseHostUnreachStat", ServerSession::defaultUseHostUnreachStat).toBool();
	sConfig.useSrvCritStat = config.value("UseSrvCritStat", ServerSession::defaultUseSrvCritStat).toBool();
	sConfig.useSrvWarnStat = config.value("UseSrvWarnStat", ServerSession::defaultUseSrvWarnStat).toBool();
	sConfig.useSrvUnknStat = config.value("UseSrvUnknStat", ServerSession::defaultUseSrvUnknStat).toBool();
	sConfig.useUserDefHostsRegExp = config.value("UseUserDefHostsRegExp",
			ServerSession::defaultUseUserDefHostsRegExp).toBool();
	sConfig.hostStatRxPos = config.value("HostStatRxPos", ServerSession::defaultHostStatRxPos).toInt();
	sConfig.hostLinkRxPos = config.value("HostLinkRxPos", ServerSession::defaultHostLinkRxPos).toInt();
	sConfig.hostNameRxPos = config.value("HostNameRxPos", ServerSession::defaultHostNameRxPos).toInt();
	sConfig.useUserDefSrvcsRegExp = config.value("UseUserDefSrvcsRegExp", ServerSession::defaultUseUserDefSrvcsRegExp).toBool();
	sConfig.srvStatRxPos = config.value("SrvStatRxPos", ServerSession::defaultSrvStatRxPos).toInt();
	sConfig.srvLinkRxPos = config.value("SrvLinkRxPos", ServerSession::defaultSrvLinkRxPos).toInt();
	sConfig.srvNameRxPos = config.value("SrvNameRxPos", ServerSession::defaultSrvNameRxPos).toInt();

	if(sConfig.useBasicRegExpConfig)
	{
		makeBasicRegExp(sConfig);
	}
	else
	{
		//Определяем регулярное выражение для хостов
		sConfig.hostsRegExp = sConfig.useUserDefHostsRegExp ?
			config.value("UserDefHostsRegExp", ServerSession::defaultExpertHostsRegExp).toString() :
			ServerSession::defaultExpertHostsRegExp;
		//Определяем регулярное выражение для сервисов
		sConfig.srvcsRegExp = sConfig.useUserDefSrvcsRegExp ?
			config.value("UserDefSrvcsRegExp", ServerSession::defaultExpertSrvcsRegExp).toString() :
			ServerSession::defaultExpertSrvcsRegExp;
	}
	//Вкладка "Misc"
	sConfig.enableHosts = config.value("EnableHostsMonitoring", ServerSession::defaultEnableHostsMonitoring).toBool();
	sConfig.showHostsStateChanged = config.value("ShowHostsMessages", ServerSession::defaultShowHostsMessages).toBool();
	sConfig.enableSrvcs = config.value("EnableSrvcsMonitoring", ServerSession::defaultEnableSrvcsMonitoring).toBool();
	sConfig.showSrvcsStateChanged = config.value("ShowSrvcsMessages", ServerSession::defaultShowSrvcsMessages).toBool();
	sConfig.popupsAlwaysStayOnTop = config.value("PopupsStayOnTop", ServerSession::defaultPopupsStayOnTop).toBool();
	sConfig.useTrayMessages = config.value("UseTrayMessages", ServerSession::defaultUseTrayMessages).toBool();
	sConfig.openUrlFromTray = config.value("OpenUrlFromTray", ServerSession::defaultOpenUrlFromTray).toBool();
	sConfig.disableSounds = config.value("DisableSounds", ServerSession::defaultDisableSounds).toBool();
	sConfig.flashesInTray = config.value("FlashesInTray", ServerSession::defaultFlashesInTray).toBool();
	sConfig.denyInvalidCert = config.value("DenySSLCert", ServerSession::defaultDenyInvalidCert).toBool();

	if(!sConfig.profileEnabled || isConfigOk(sConfig, showError))	//Проверяем конфиг только если он активен
		sConfig.validConfig = true;
	else
		sConfig.validConfig = false;
	return sConfig;
}

void NagiosAgent::makeBasicRegExp(SessionConfig & config)
{
	//Обработка хостов
	QString resultExpr = "";
	QString resultRegExp = ServerSession::defaultBasicHostsRegExp;
	if(config.useHostDownStat)
		resultExpr = "statusHOSTDOWN";
	if(config.useHostUnreachStat)
		resultExpr += QString(resultExpr.isEmpty()? "" : "|") + "statusHOSTUNREACHABLE";
	resultRegExp.replace("status_list", resultExpr);
	config.hostsRegExp = resultRegExp;
	config.hostStatRxPos = ServerSession::defaultHostStatRxPos;
	config.hostLinkRxPos = ServerSession::defaultHostLinkRxPos;
	config.hostNameRxPos = ServerSession::defaultHostNameRxPos;
	//обработка сервисов
	resultExpr.clear();
	resultRegExp = ServerSession::defaultBasicSrvcsRegExp;
	if(config.useSrvCritStat)
		resultExpr = "statusBGCRITICAL";
	if(config.useSrvWarnStat)
		resultExpr += QString(resultExpr.isEmpty() ? "" : "|") + "statusBGWARNING";
	if(config.useSrvUnknStat)
		resultExpr += QString(resultExpr.isEmpty()? "" : "|") + "statusBGUNKNOWN";
	resultRegExp.replace("status_list", resultExpr);
	config.srvcsRegExp = resultRegExp;
	config.srvStatRxPos = ServerSession::defaultSrvStatRxPos;
	config.srvLinkRxPos = ServerSession::defaultSrvLinkRxPos;
	config.srvNameRxPos = ServerSession::defaultSrvNameRxPos;
}

bool NagiosAgent::isConfigOk(SessionConfig & config, bool showError)
{
#define CONF_ERR(msg) { \
			if(showError) \
				QMessageBox::warning(this, tr("Nagios Agent: configuration error"), \
						tr("Profile '") + \
						((config.profileName.isEmpty())? tr("<empty name>") : config.profileName) + \
						"': " + msg); \
		return false; \
	}
#define CONF_WARN(msg)	\
			if(showError) \
				QMessageBox::information(this, tr("Nagios Agent: warning"), msg);

	if(config.profileName.isEmpty())
		CONF_ERR(tr("Profile name can't be empty."));

	if(config.serverAddress.isEmpty())
		CONF_ERR(tr("No server address of Nagios found."));

	if(config.serverPort.isEmpty())
		config.serverPort = ((config.useHttps)?
				ServerSession::defaultHttpsServerPort : ServerSession::defaultHttpServerPort);
	else
	{
		if(config.serverPort.toInt(0, 10) < 1 ||
				config.serverPort.toInt(0, 10) > 65535)
			CONF_ERR(tr("Invalid server port number Nagios: ") + config.serverPort);
	}

	if(config.useUserDefHostRequest && config.hostsRequest.isEmpty())
		CONF_ERR(tr("No request for the status of hosts."));

	if(config.useUserDefSrvcsRequest && config.servicesRequest.isEmpty())
		CONF_ERR(tr("No request for the status of services."));

	if(config.useHTTPAuth)
	{
		if(config.httpLogin.isEmpty())
			CONF_ERR(tr("Not specified HTTP username."));
		if(config.httpPassword.isEmpty())
			CONF_ERR(tr("Not specified HTTP password."));
	}

	if(config.useUserDefHostsRegExp)
	{
		QRegExp rx(config.hostsRegExp);
		if(!rx.isValid() || config.hostsRegExp.isEmpty())
			CONF_ERR(tr("Not a valid regular expression to analyze the status of hosts."));
	}

	if(config.useUserDefSrvcsRegExp)
	{
		QRegExp rx(config.srvcsRegExp);
		if(!rx.isValid() || config.srvcsRegExp.isEmpty())
			CONF_ERR(tr("Not a valid regular expression to analyze the status of services."));
	}

	if(config.useBasicRegExpConfig)
	{
		if(!config.useHostDownStat && !config.useHostUnreachStat)
			CONF_ERR(tr("Not specified no status for the host!"));
		if(!config.useSrvCritStat && !config.useSrvWarnStat &&
				!config.useSrvUnknStat)
			CONF_ERR(tr("Not specified no status for the service!"));
	}

	if(config.useHttps)
	{
	    if (!QSslSocket::supportsSsl())
	    	CONF_ERR(tr("<font>You can obtain the necessary libraries for work using HTTPS can "
	    			"<a href='http://www.openssl.org/related/binaries.html'>here</a>."
	    			"The work using HTTPS is impossible now.</font>"));
	}

	if(config.useFilterRegExpHosts)
	{
		QRegExp rx;
		QString invalidRx;
		int irxc = 0;
		Q_FOREACH(QString host, config.hostsList)
		{
			rx.setPattern(host);
			if(!rx.isValid())
			{
				++irxc;
				invalidRx += "'" + host + "' ";
			}
		}
		if(irxc)
			CONF_WARN(tr("Incorrect regular expression(s) "
					"in the list of hosts: ", "", irxc) +
					invalidRx);
	}

	if(config.useFilterRegExpSrvcs)
	{
		QRegExp rx;
		QString invalidRx;
		int irxc = 0;
		Q_FOREACH(QString srv, config.hostsList)
		{
			rx.setPattern(srv);
			if(!rx.isValid())
			{
				++irxc;
				invalidRx += "'" + srv + "' ";
			}
		}
		if(irxc)
			CONF_WARN(tr("Incorrect regular expression(s) "
					"in the list of services: ", "", irxc) +
					invalidRx);
	}

#undef CONF_ERR
#undef CONF_WARN
	return true;
}

void NagiosAgent::loadServerConfigToGui(const QString & profileName)
{
	if(serversSessions.count() < 1)
		return;

	SessionConfig * sConfig;

	Q_FOREACH(ServerSession *sess, serversSessions)
	{
		if(profileName.isEmpty() ||	//Если не указано имя профиля - загружаем первый попавшийся профиль
				sess->config().profileName == profileName)
		{
			sConfig = (SessionConfig*) &sess->config();
			goto profile_found;
		}
	}
	MSG("Can't found session configuration for loading to GUI");
	return;	//По какой-то причине не найдено подходящего профиля
profile_found:
	//Вкладка "server settings"
	ui.cboxProfileName->setCurrentIndex(ui.cboxProfileName->findText(sConfig->profileName, Qt::MatchFixedString));
	ui.cbProfileEnabled->setChecked(sConfig->profileEnabled);
	//Вкладка "Server"
	ui.edServerAddress->setText(sConfig->serverAddress);
	//WARN!!! Сперва нужно устанавливать этот флажёк, и только потом - значение порта,
	ui.cbUseHttps->setChecked(sConfig->useHttps);	 //потому что если значение порта задано пользователем
	ui.edServerPort->setText(sConfig->serverPort);	//то при обработке сигнала установки флажка оно сбросится в дефолтное
	ui.spinBoxPeriod->setValue(sConfig->requestPeriod);
	ui.cbAutoClose->setChecked(sConfig->autoClose);
	ui.spinBoxClose->setValue(sConfig->autoCloseTime);
	ui.cbExecDir->setChecked(sConfig->useUserDefExecDir);
	ui.edExecDir->setText(sConfig->execDir);
	ui.cbHostsRequest->setChecked(sConfig->useUserDefHostRequest);
	ui.edHostsRequest->setText(sConfig->hostsRequest);
	ui.cbServicesRequest->setChecked(sConfig->useUserDefSrvcsRequest);
	ui.edServicesRequest->setText(sConfig->servicesRequest);
	ui.cbUseHTTPAuth->setChecked(sConfig->useHTTPAuth);
	ui.edLogin->setText(sConfig->httpLogin);
	ui.edPassword->setText(sConfig->httpPassword);
	//Вкладка "Host filtering"
	ui.rbAllExceptSpecifedHosts->setChecked(sConfig->allExceptSpecifedHosts);
	ui.rbOnlyListedHosts->setChecked(sConfig->onlyListedHosts);
	ui.listHosts->clear();
	ui.listHosts->addItems(sConfig->hostsList);
	ui.edAddHost->clear();
	ui.cbUseFilterRegExpHosts->setChecked(sConfig->useFilterRegExpHosts);
	//Вкладка "Filtering service"
	ui.rbAllExceptSpecifedSrvcs->setChecked(sConfig->allExceptSpecifedSrvcs);
	ui.rbOnlyListedSrvcs->setChecked(sConfig->onlyListedSrvcs);
	ui.listServices->clear();
	ui.listServices->addItems(sConfig->srvcsList);
	ui.edAddService->clear();
	ui.cbUseFilterRegExpSrvcs->setChecked(sConfig->useFilterRegExpSrvcs);
	//Вкладка "Statuses"
	if(sConfig->useBasicRegExpConfig)
	{
		ui.rbBaseRegExpConfig->setChecked(true);
		ui.frameBasicRegExpConf->setEnabled(true);
		ui.frameExpertRegExpConf->setDisabled(true);
	}
	else
	{
		ui.rbExpertRegExpConfig->setChecked(true);
		ui.frameExpertRegExpConf->setEnabled(true);
		ui.frameBasicRegExpConf->setDisabled(true);
	}
	ui.cbHostDownStat->setChecked(sConfig->useHostDownStat);
	ui.cbHostUnreachStat->setChecked(sConfig->useHostUnreachStat);
	ui.cbServiceCritStat->setChecked(sConfig->useSrvCritStat);
	ui.cbServiceWarnStat->setChecked(sConfig->useSrvWarnStat);
	ui.cbServiceUnknownStat->setChecked(sConfig->useSrvUnknStat);
	ui.cbHostsRegExp->setChecked(sConfig->useUserDefHostsRegExp);
	ui.edHostsRegExp->setText(sConfig->hostsRegExp);
	ui.edHostStatRxPos->setText(QString::number(sConfig->hostStatRxPos));
	ui.edHostLinkRxPos->setText(QString::number(sConfig->hostLinkRxPos));
	ui.edHostNameRxPos->setText(QString::number(sConfig->hostNameRxPos));
	ui.cbSrvcsRegExp->setChecked(sConfig->useUserDefSrvcsRegExp);
	ui.edSrvcsRegExp->setText(sConfig->srvcsRegExp);
	ui.edSrvStatRxPos->setText(QString::number(sConfig->srvStatRxPos));
	ui.edSrvLinkRxPos->setText(QString::number(sConfig->srvLinkRxPos));
	ui.edSrvNameRxPos->setText(QString::number(sConfig->srvNameRxPos));

	//Вкладка "Misc"
	ui.cbEnableHosts->setChecked(sConfig->enableHosts);
	ui.cbShowHosts->setChecked(sConfig->showHostsStateChanged);
	ui.cbEnableSrvcs->setChecked(sConfig->enableSrvcs);
	ui.cbShowSrvcs->setChecked(sConfig->showSrvcsStateChanged);
	ui.cbPopupsStayOnTop->setChecked(sConfig->popupsAlwaysStayOnTop);
	ui.cbUseTrayMessages->setChecked(sConfig->useTrayMessages);
	ui.cbOpenUrlFromTray->setChecked(sConfig->openUrlFromTray);
	ui.cbFlashInTray->setChecked(sConfig->flashesInTray);
	ui.cbDisableSounds->setChecked(sConfig->disableSounds);
	ui.rbDenyInvalidCert->setChecked(sConfig->denyInvalidCert);
	ui.rbAllowInvalidCert->setChecked(!sConfig->denyInvalidCert);

	currentLoadedProfile = ui.cboxProfileName->currentText();
}

void NagiosAgent::loadCommonConfigToGui()
{
	ui.cbAvoidOverlapMsgs->setChecked(commonConfig.avoidOverlapMsgs);
	ui.cbDisableAutoUpdate->setChecked(commonConfig.disableAutoUpdate);
	ui.cbDisableLocalization->setChecked(commonConfig.disableLocalization);
	switch(commonConfig.menuNumOnLMK)
	{
	case hostsMenu:
		ui.rbHostOnLMK->setChecked(true);
		break;
	case srvcsMenu:
		ui.rbSrvcsOnLMK->setChecked(true);
		break;
	case hostsSrvcsMenu:
	default:
		ui.rbHostsAndSrvcsOnLMK->setChecked(true);
		break;
	}
}

SessionConfig NagiosAgent::makeConfigFromGui()
{
	SessionConfig config;
	//Вкладка "Servers settings" главного TabWidget
	config.profileName = currentLoadedProfile;
	config.profileEnabled = ui.cbProfileEnabled->isChecked();
	//Вкладка "Server"
	config.serverAddress = ui.edServerAddress->text();
	config.serverPort = ui.edServerPort->text();
	config.requestPeriod = ui.spinBoxPeriod->value();
	config.useHttps = ui.cbUseHttps->isChecked();
	config.autoClose = ui.cbAutoClose->isChecked();
	config.autoCloseTime = ui.spinBoxClose->value();
	config.useUserDefExecDir = ui.cbExecDir->isChecked();
	if(config.useUserDefExecDir)
	{
		config.execDir = ui.edExecDir->text();
		//Добавляем лидирующий слеш абсолютного пути (/cgi-bin...) если необходим
		if(config.execDir.indexOf(QRegExp("^/")) < 0)
			config.execDir.insert(0, "/");
		//Добавляем завершающий слеш пути исполняемого каталога (...cgi-bin/) если необходим
		if(config.execDir.indexOf(QRegExp("/$")) < 0)
			config.execDir.append("/");
	}
	else
		config.execDir = ServerSession::defaultExecDir;
	config.useUserDefHostRequest = ui.cbHostsRequest->isChecked();
	config.hostsRequest = config.useUserDefHostRequest ?
			ui.edHostsRequest->text() : ServerSession::defaultHostRequest;
	config.useUserDefSrvcsRequest = ui.cbServicesRequest->isChecked();
	config.servicesRequest = config.useUserDefSrvcsRequest ?
			ui.edServicesRequest->text() : ServerSession::defaultSrvcsRequest;
	config.useHTTPAuth = ui.cbUseHTTPAuth->isChecked();
	config.httpLogin = ui.edLogin->text();
	config.httpPassword = ui.edPassword->text();
	//Вкладка "Filtering hosts"
	config.allExceptSpecifedHosts = ui.rbAllExceptSpecifedHosts->isChecked();
	config.onlyListedHosts = ui.rbOnlyListedHosts->isChecked();
	for(int i = 0; i < ui.listHosts->count(); ++i)
		config.hostsList.append(ui.listHosts->item(i)->text());
	config.useFilterRegExpHosts = ui.cbUseFilterRegExpHosts->isChecked();
	//Вкладка "Filtering service"
	config.allExceptSpecifedSrvcs = ui.rbAllExceptSpecifedSrvcs->isChecked();
	config.onlyListedSrvcs = ui.rbOnlyListedSrvcs->isChecked();
	for(int i = 0; i < ui.listServices->count(); ++i)
		config.srvcsList.append(ui.listServices->item(i)->text());
	config.useFilterRegExpSrvcs = ui.cbUseFilterRegExpSrvcs->isChecked();
	//Вкладка "Statuses"
	config.useBasicRegExpConfig = ui.rbBaseRegExpConfig->isChecked();
	config.useHostDownStat = ui.cbHostDownStat->isChecked();
	config.useHostUnreachStat = ui.cbHostUnreachStat->isChecked();
	config.useSrvCritStat = ui.cbServiceCritStat->isChecked();
	config.useSrvWarnStat = ui.cbServiceWarnStat->isChecked();
	config.useSrvUnknStat = ui.cbServiceUnknownStat->isChecked();
	config.useUserDefHostsRegExp = ui.cbHostsRegExp->isChecked();
	config.hostStatRxPos = ui.edHostStatRxPos->text().toInt();
	config.hostLinkRxPos = ui.edHostLinkRxPos->text().toInt();
	config.hostNameRxPos = ui.edHostNameRxPos->text().toInt();
	config.useUserDefSrvcsRegExp = ui.cbSrvcsRegExp->isChecked();
	config.srvStatRxPos = ui.edSrvStatRxPos->text().toInt();
	config.srvLinkRxPos = ui.edSrvLinkRxPos->text().toInt();
	config.srvNameRxPos = ui.edSrvNameRxPos->text().toInt();

	if(config.useBasicRegExpConfig)
		makeBasicRegExp(config);
	else
	{
		config.hostsRegExp = config.useUserDefHostsRegExp ?
				ui.edHostsRegExp->text() : ServerSession::defaultExpertHostsRegExp;
		config.srvcsRegExp = config.useUserDefSrvcsRegExp ?
				ui.edSrvcsRegExp->text() : ServerSession::defaultExpertSrvcsRegExp;
	}
	//Вкладка "Misc"
	config.enableHosts = ui.cbEnableHosts->isChecked();
	config.showHostsStateChanged = ui.cbShowHosts->isChecked();
	config.enableSrvcs = ui.cbEnableSrvcs->isChecked();
	config.showSrvcsStateChanged = ui.cbShowSrvcs->isChecked();
	config.popupsAlwaysStayOnTop = ui.cbPopupsStayOnTop->isChecked();
	config.useTrayMessages = ui.cbUseTrayMessages->isChecked();
	config.openUrlFromTray = ui.cbOpenUrlFromTray->isChecked();
	config.disableSounds =  ui.cbDisableSounds->isChecked();
	config.flashesInTray = ui.cbFlashInTray->isChecked();
	config.denyInvalidCert = ui.rbDenyInvalidCert->isChecked();

	config.validConfig = true;

	return config;
}

void NagiosAgent::saveServersConfig(const QString & profileName, const bool rewriteAll)
{
	QSettings config(configDirName, programmName);

	if(rewriteAll)
	{
		//Удаляем весь старый конфиг
		config.beginWriteArray(configServersArray);
		config.remove("");
		config.endArray();
		//Перезаписываем все конфигурации
		config.beginWriteArray(configServersArray);
		for(int i = 0; i < serversSessions.count(); ++i)
		{
			config.setArrayIndex(i);
			saveServerValues(config, serversSessions.at(i)->config());
		}
		config.endArray();
	}
	else
	{
		ServerSession * sess;
		SessionConfig sConfig;

		if(serversSessions.count() > 0)
		{
			Q_FOREACH(sess, serversSessions)
				if(sess->config().profileName == profileName)
					goto configFound;
		}
		return;
configFound:
		//Сохраняем конфигурацию
		int count = config.beginReadArray(configServersArray);
		config.endArray();
		int serverIndex = profileNameIndex(profileName);
		if(serverIndex < 0)
		{
			MSG("Can't found suitable configuration!");
			return;
		}

		sConfig = makeConfigFromGui();
		//Открываем массив конфигураций на запись
		config.beginWriteArray(configServersArray);
		config.setArrayIndex(serverIndex);
		//Собственно производим запись
		saveServerValues(config, sConfig);
		//Устанавливаем индекс массива равный числу элементов - 1 что бы не происходило
		//усекание длины массива по предыдущему установленному индексу
		config.setArrayIndex(count - 1);
		config.endArray();
		//Задаём новую конфигурацию серверной сессии
		sess->setConfig(sConfig);
	}
}

void NagiosAgent::saveServerValues(QSettings & config, const SessionConfig & sConfig)
{
	//Вкладка "Servers settings" главного TabWidget
	config.setValue("ProfileName", sConfig.profileName);	//Не используем sConfig т.к. там может оказаться уже изменённая
													//информация. Например в случае смены индекса ComboBox
													//без предварительного сохранения конфигурации
	config.setValue("ProfileEnabled", sConfig.profileEnabled);
	//Вкладка "Server"
	config.setValue("ServerAddress", sConfig.serverAddress);
	if(ui.cbUseHttps->isChecked())
		config.setValue("HTTPSPort", sConfig.serverPort);
	else
		config.setValue("HTTPPort", sConfig.serverPort);
	config.setValue("RequestPeriod", sConfig.requestPeriod);
	config.setValue("UseHTTPS", sConfig.useHttps);
	config.setValue("AutoClose", sConfig.autoClose);
	config.setValue("AutoCloseTime", sConfig.autoCloseTime);
	config.setValue("UseUserDefExecDir", sConfig.useUserDefExecDir);
	config.setValue("UserDefNagiosExecDir", sConfig.execDir);
	config.setValue("UseUserDefHostsRequest", sConfig.useUserDefHostRequest);
	config.setValue("UserDefHostsRequest", sConfig.hostsRequest);
	config.setValue("UseUserDefSrvcsRequest", sConfig.useUserDefSrvcsRequest);
	config.setValue("UserDefServicesRequest", sConfig.servicesRequest);
	config.setValue("UseHTTPAuth", sConfig.useHTTPAuth);
	config.setValue("Login", sConfig.httpLogin);
	config.setValue("Password", sConfig.httpPassword);
	//Вкладка "Filtering hosts"
	config.setValue("AllExceptSpecifedHosts", sConfig.allExceptSpecifedHosts);
	config.setValue("OnlyListedHosts", sConfig.onlyListedHosts);
	config.setValue("HostsList", sConfig.hostsList);
	config.setValue("UseFilterRegExpHosts", sConfig.useFilterRegExpHosts);
	//Вкладка "Filtering service"
	config.setValue("AllExceptSpecifedSrvcs", sConfig.allExceptSpecifedSrvcs);
	config.setValue("OnlyListedSrvcs", sConfig.onlyListedSrvcs);
	config.setValue("ServicesList", sConfig.srvcsList);
	config.setValue("UseFilterRegExpSrvcs", sConfig.useFilterRegExpSrvcs);
	//Вкладка "Statuses"
	config.setValue("UseBasicRegExpConfig", sConfig.useBasicRegExpConfig);
	config.setValue("UseHostDownStat", sConfig.useHostDownStat);
	config.setValue("UseHostUnreachStat", sConfig.useHostUnreachStat);
	config.setValue("UseSrvCritStat", sConfig.useSrvCritStat);
	config.setValue("UseSrvWarnStat", sConfig.useSrvWarnStat);
	config.setValue("UseSrvUnknStat", sConfig.useSrvUnknStat);
	config.setValue("UseUserDefHostsRegExp", sConfig.useUserDefHostsRegExp);
	config.setValue("UserDefHostsRegExp", sConfig.hostsRegExp);
	config.setValue("HostStatRxPos", sConfig.hostStatRxPos);
	config.setValue("HostLinkRxPos", sConfig.hostLinkRxPos);
	config.setValue("HostNameRxPos", sConfig.hostNameRxPos);
	config.setValue("UseUserDefSrvcsRegExp", sConfig.useUserDefSrvcsRegExp);
	config.setValue("UserDefSrvcsRegExp", sConfig.srvcsRegExp);
	config.setValue("SrvStatRxPos", sConfig.srvStatRxPos);
	config.setValue("SrvLinkRxPos", sConfig.srvLinkRxPos);
	config.setValue("SrvNameRxPos", sConfig.srvNameRxPos);
	//Вкладка "Misc"
	config.setValue("EnableHostsMonitoring", sConfig.enableHosts);
	config.setValue("ShowHostsMessages", sConfig.showHostsStateChanged);
	config.setValue("EnableSrvcsMonitoring", sConfig.enableSrvcs);
	config.setValue("ShowSrvcsMessages", sConfig.showSrvcsStateChanged);
	config.setValue("PopupsStayOnTop", sConfig.popupsAlwaysStayOnTop);
	config.setValue("UseTrayMessages", sConfig.useTrayMessages);
	config.setValue("OpenUrlFromTray", sConfig.openUrlFromTray);
	config.setValue("DisableSounds", sConfig.disableSounds);
	config.setValue("FlashesInTray", sConfig.flashesInTray);
	config.setValue("DenySSLCert", sConfig.denyInvalidCert);
}

//*******************************************
//Функции работы с GUI

void NagiosAgent::createTrayIcon()
{
    //Create main menu
    trayMainMenu.addAction(ui.actionConfig);
    trayMainMenu.addSeparator();
    trayMainMenu.addAction(ui.actionCheckUpdates);
    trayMainMenu.addAction(ui.actionAbout);
    trayMainMenu.addSeparator();
    trayMainMenu.addAction(ui.actionCloseAll);
    trayMainMenu.addSeparator();
    trayMainMenu.addAction(ui.actionQuit);

    ui.actionEmpty->setDisabled(true);
    ui.actionUnmonitored->setDisabled(true);

    //Создаём иконку в трее без проверки доступности трея:
    //If the system tray is currently unavailable but becomes available later,
    //QSystemTrayIcon will automatically add an entry in the system tray if it is visible.
    //Источник: http://doc.trolltech.com/4.5/qsystemtrayicon.html#isSystemTrayAvailable
	/*if (!QSystemTrayIcon::isSystemTrayAvailable())
	{
        QMessageBox::critical(this, QObject::tr("Systray"),
                              QObject::tr("I couldn't detect any system tray on this system."));
        exit(-1);
    }*/
    //Да, я знаю что плохо пытаться создать иконку в системном трее не проверив его доступность
    //т.к. может случиться что системный трей не создастся никогда, но если выполнять проверку
    //перед запуском, то в ОС отличных от Windows приложение не запускается если оно находится в автостарте
    //по причине того что запуск приложения выполняется раньше чем запуск системного трея.

    trayIcon.setIcon(QIcon(normalTrayIcon));
	trayIcon.setToolTip("NagiosAgent");
	trayIcon.show();
	trayIcon.setContextMenu(&trayMainMenu);
	connect(&trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
	            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
}

void NagiosAgent::updateTrayToolTip()
{
	QString toolTip = trayToolTipHeader;
	if(ServerSession::activeSessionsCount())
	{
		toolTip += tr("\nActive profiles: ") + QString::number(ServerSession::activeSessionsCount());
		int totalHosts = 0;
		int totalSrvcs = 0;
		Q_FOREACH(ServerSession * sess, serversSessions)
		{
			totalHosts += sess->downHostsCount();
			totalSrvcs += sess->problemSrvcsCount();
		}
		toolTip += tr("\nHosts down count: ") + QString::number(totalHosts);
		toolTip += tr("\nProblem services: ") + QString::number(totalSrvcs);
		toolTip += tr("\nLast update: ") + QDateTime::currentDateTime().toString("hh:mm:ss dd.MM.yyyy");
	}
	else
		toolTip += tr("\nNo active profiles found.");
	trayIcon.setToolTip(toolTip);
}

void NagiosAgent::showObjStatChangeMsg(const SessionConfig & config, const NagiosObjectInfo & obj)
{
	if(config.useTrayMessages)
	{
		if(config.openUrlFromTray)
			sysTrayBufferStr = obj.url;
		else
			sysTrayBufferStr.clear();
		QString data = (ServerSession::activeSessionsCount() > 1 ?
						tr("Profile '") + config.profileName + "'\n" : "" ) +
				tr("Name: ") + obj.name + "\n" +
				tr("Status: ");
		if(obj.state == NagiosObjectInfo::objectDown)
			data += obj.strState;
		else
			data += (obj.type == NagiosObjectInfo::host ? "Host UP" : "Service UP");

		trayIcon.showMessage(tr("Change status of ") +
				(obj.type == NagiosObjectInfo::host ? tr("host") : tr("service")), data,
				obj.state == NagiosObjectInfo::objectDown ?
						QSystemTrayIcon::Critical : QSystemTrayIcon::Information
				);

	}
	else
	{
		InformationWindow *b = new InformationWindow(0, obj, config, informWinPt);
		updateInformWinPt();
		connect(this, SIGNAL(closeAllInformWin()), b, SLOT(close()));
	}

	if(config.flashesInTray)
		startFlashingTray(obj.state == NagiosObjectInfo::objectUp);

	if(!config.disableSounds)
		QSound::play(appDir.path() + "/" +
				(obj.state == NagiosObjectInfo::objectUp ? hostDownSnd : hostUpSnd));
}

inline void NagiosAgent::trouble(QString reason, const bool showMessage)
{
	sysTrayBufferStr.clear();
	updateTrayToolTip();
	if(showMessage)
		trayIcon.showMessage(tr("Nagios Agent: error"), reason, QSystemTrayIcon::Critical);
	QSound::play(appDir.path() + "/" + troubleSnd);
}

void NagiosAgent::startFlashingTray(const bool up)
{
	flashingCounter = 12;
	if(timerFlash.isActive())
		timerFlash.stop();
	stateUp = up;
	currentIcon = (up)? &hostUpIcon1 : &hostDownIcon1;
	trayIcon.setIcon(QIcon(*currentIcon));
	timerFlash.start(flashingTimeout);
}

void NagiosAgent::createProfilesMenu()
{
    profileMenu.addAction(ui.actionSaveProfile);
    profileMenu.addAction(ui.actionSetDefaultSettings);
    profileMenu.addAction(ui.actionRenameProfile);
    profileMenu.addSeparator();
    profileMenu.addAction(ui.actionNewProfile);
    profileMenu.addAction(ui.actionDeleteProfile);
    ui.tbActionsProfiles->setMenu(&profileMenu);
    ui.tbActionsProfiles->setPopupMode(QToolButton::InstantPopup);
}

void NagiosAgent::setDisableControls(const bool disable)
{
	ui.tabConfig->setDisabled(disable);
	ui.cboxProfileName->setDisabled(disable);
	ui.cbProfileEnabled->setDisabled(disable);
	ui.actionSaveProfile->setDisabled(disable);
	ui.actionSetDefaultSettings->setDisabled(disable);
	ui.actionRenameProfile->setDisabled(disable);
	ui.actionDeleteProfile->setDisabled(disable);
}

void NagiosAgent::showObjectsMenu(MenuTypes type)
{
	ObjectsMenu menu;
	QList<NagiosObjectInfo> infoList;
	switch(type)
	{
	case hostsSrvcsMenu:
		Q_FOREACH(ServerSession *sess, serversSessions)
		{
			if(!sess->config().profileEnabled)
				continue;
			//Добавляем проблемные хосты
			menu.addSeparator();
			menu.addAction(tr("Down hosts: ") + sess->config().profileName,
					makeProblemLink(sess->config(), hostsMenu) );
			menu.addSeparator();
			if(sess->config().enableHosts)
			{
				if(sess->problem())
					menu.addAction(tr("   < monitoring is impossible >"), "", true);
				else
				{
					infoList = sess->downHosts();
					if(infoList.isEmpty())
						menu.addAction(tr("      < empty >"), "", true);
					else
						Q_FOREACH(NagiosObjectInfo obj, infoList)
							menu.addAction(obj.name, obj.url);
				}
			}
			else
				menu.addAction(tr("      < monitoring disabled >"), "", true);
			//Добавляем проблемные сервисы
			menu.addSeparator();
			menu.addAction(tr("Problem services: ") + sess->config().profileName,
					makeProblemLink(sess->config(), srvcsMenu) );
			menu.addSeparator();
			if(sess->config().enableSrvcs)
			{
				if(sess->problem())
					menu.addAction(tr("   < monitoring is impossible >"), "", true);
				else
				{
					infoList = sess->problemSrvcs();
					if(infoList.isEmpty())
						menu.addAction(tr("      < empty >"), "", true);
					else
						Q_FOREACH(NagiosObjectInfo obj, infoList)
							menu.addAction(obj.name, obj.url);
				}
			}
			else
				menu.addAction(tr("      < monitoring disabled >"), "", true);
		}
		break;
	case hostsMenu:
		Q_FOREACH(ServerSession *sess, serversSessions)
		{
			if(!sess->config().profileEnabled)
				continue;
			//Добавляем проблемные хосты
			menu.addSeparator();
			menu.addAction(tr("Down hosts: ") + sess->config().profileName,
					makeProblemLink(sess->config(), hostsMenu) );
			menu.addSeparator();
			if(sess->config().enableHosts)
			{
				if(sess->problem())
					menu.addAction(tr("   < monitoring is impossible >"), "", true);
				else
				{
					infoList = sess->downHosts();
					if(infoList.isEmpty())
						menu.addAction(tr("      < empty >"), "", true);
					else
						Q_FOREACH(NagiosObjectInfo obj, infoList)
							menu.addAction(obj.name, obj.url);
				}
			}
			else
				menu.addAction(tr("      < monitoring disabled >"), "", true);
		}
		break;
	case srvcsMenu:
		Q_FOREACH(ServerSession *sess, serversSessions)
		{
			if(!sess->config().profileEnabled)
				continue;
			//Добавляем проблемные сервисы
			menu.addSeparator();
			menu.addAction(tr("Problem services: ") + sess->config().profileName,
					makeProblemLink(sess->config(), srvcsMenu) );
			menu.addSeparator();
			if(sess->config().enableSrvcs)
			{
				if(sess->problem())	//Если есть проблема с этой сессией - добавляем только один пункт
					menu.addAction(tr("   < monitoring is impossible >"), "", true);
				else	//Иначе получаем список проблемных сервисов
				{
					infoList = sess->problemSrvcs();
					if(infoList.isEmpty())
						menu.addAction(tr("      < empty >"), "", true);
					else
						Q_FOREACH(NagiosObjectInfo obj, infoList)
							menu.addAction(obj.name, obj.url);
				}
			}
			else
				menu.addAction(tr("      < monitoring disabled >"), "", true);
		}
		break;
	case mainMenu:
	default:
		break;
	}

	if(!menu.actions().count())
		menu.addAction(tr("No active profiles found. Click here to open configuration."));

	QPoint pt = QCursor::pos();
	if(pt.x() + menu.sizeHint().width() > QApplication::desktop()->screenGeometry(this).width())
	{
		if(pt.x() - menu.sizeHint().width() < 0)
			pt.rx() = 0;
		else
			pt.rx() -= menu.sizeHint().width();
	}

	if(pt.x() < 0)
		pt.setX(0);

	if(pt.y() + menu.sizeHint().height() > QApplication::desktop()->screenGeometry(this).height())
		pt.ry() -= menu.sizeHint().height();

	if(pt.y() < 0)
		pt.setY(0);

	activateWindow();

	connect(&menu, SIGNAL(triggered(QAction *)), this, SLOT(hostsSrvcsMenuHandler(QAction *)));

	menu.exec(pt);
}

void NagiosAgent::setSuitableIcon()
{
	if(ServerSession::activeSessionsCount() && !ServerSession::problemSessionsCount())
		trayIcon.setIcon(QIcon(normalTrayIcon));	//Если нет проблем и есть активные сессии
	else if(!ServerSession::activeSessionsCount())
		trayIcon.setIcon(QIcon(problemTrayIcon));	//Если нет активных сессий
	else if(ServerSession::problemSessionsCount() == ServerSession::activeSessionsCount())
		trayIcon.setIcon(QIcon(problemTrayIcon));	//Если проблемы со всеми профилями
	else
		trayIcon.setIcon(QIcon(serverProblemTrayIcon));	//Если есть проблемы с каким-либо из профилей серверов
}

//*******************************************
//СЛОТЫ

//Непосредственная работа с GUI

void NagiosAgent::showAndReadConfig(const QString & profileName)
{
	if(isVisible())
	{
		activateWindow();
		return;
	}
	else
	{	//Позиционируем окно настроек в центре экрана
		QRect screenRect = QApplication::desktop()->screenGeometry(this);
		setGeometry(screenRect.width()/2 - defaultWidth/2, screenRect.height()/2 - defaultHeight/2,
					defaultWidth, defaultHeight);
	}
	ui.tabMain->setCurrentIndex(0);
	ui.tabConfig->setCurrentIndex(serverTab);

	configChanged = false;

	loadCommonConfigToGui();

	//Загружаем список серверов в cboxProfileName
	ui.cboxProfileName->clear();
	Q_FOREACH(ServerSession * sess, serversSessions)
		ui.cboxProfileName->addItem(sess->config().profileName);

	if(serversSessions.count() < 1)
	{
		setDefaultConfigValues();
		setDisableControls();
	}
	else
		loadServerConfigToGui(profileName);
	show();
}

void NagiosAgent::useHttpsClicked(const bool use)
{
	if(use)
		ui.edServerPort->setText(ServerSession::defaultHttpsServerPort);
	else
		ui.edServerPort->setText(ServerSession::defaultHttpServerPort);
	ui.grBoxSsl->setEnabled(use);
}

void NagiosAgent::useTrayMsgsClicked(const bool use)
{
	ui.cbAutoClose->setDisabled(use);
	if(!use)
	{
		if(ui.cbAutoClose->isChecked())
		{
			ui.spinBoxClose->setEnabled(true);
			ui.lbMinutes->setEnabled(true);
		}
	}
	else
	{
		ui.spinBoxClose->setEnabled(false);
		ui.lbMinutes->setEnabled(false);
	}
}

void NagiosAgent::addHost()
{
	if(ui.edAddHost->text().isEmpty())
		return;

	for(int i = 0; i < ui.listHosts->count(); ++i)
		if(!ui.listHosts->item(i)->text().compare(ui.edAddHost->text(), Qt::CaseInsensitive))
		{
			QMessageBox::warning(this,
					tr("Nagios Agent: warning"),
					tr("Host with name \"") + ui.edAddHost->text() +
					tr("\" was already added to the list."));
			return;
		}

	ui.listHosts->addItem(ui.edAddHost->text());
	ui.edAddHost->clear();
}

void NagiosAgent::delHost()
{
	qDeleteAll(ui.listHosts->selectedItems());
}

void NagiosAgent::addService()
{
	if(ui.edAddService->text().isEmpty())
		return;

	for(int i = 0; i < ui.listServices->count(); ++i)
		if(!ui.listServices->item(i)->text().compare(ui.edAddService->text(), Qt::CaseInsensitive))
		{
			QMessageBox::warning(this,
					tr("Nagios Agent: warning"),
					tr("Service \"") + ui.edAddService->text() +
					tr("\" was already added to the list."));
			return;
		}

	ui.listServices->addItem(ui.edAddService->text());
	ui.edAddService->clear();
}

void NagiosAgent::delService()
{
	qDeleteAll(ui.listServices->selectedItems());
}

bool NagiosAgent::isSavingConfigOk()
{
#define CONF_ERR(msg, widget, tab) { \
		ui.tabMain->setCurrentIndex(0); \
		ui.tabConfig->setCurrentIndex(tab); \
		widget->setFocus(); \
		QMessageBox::warning(this, tr("Nagios Agent: configuration error"), msg); \
		return false; \
	}
#define CONF_WARN(msg)	QMessageBox::information(this, tr("Nagios Agent: warning"), msg);

	if(ui.cboxProfileName->currentText().isEmpty())
		CONF_ERR(tr("Profile name can't be empty."), ui.cboxProfileName, serverTab);

	if(ui.edServerAddress->text().isEmpty())
		CONF_ERR(tr("No server address of Nagios found."), ui.edServerAddress, serverTab);

	if(ui.edServerPort->text().isEmpty())
	{
		CONF_WARN(tr("Server port is not specified and will be installed in the default."));
		ui.edServerPort->setText((ui.cbUseHttps->isChecked())?
				ServerSession::defaultHttpsServerPort : ServerSession::defaultHttpServerPort);
	}
	else
	{
		if(ui.edServerPort->text().toInt(0, 10) < 1 ||
				ui.edServerPort->text().toInt(0, 10) > 65535)
			CONF_ERR(tr("Invalid server port number Nagios."), ui.edServerPort, serverTab);
	}

	if(ui.cbHostsRequest->isChecked())
	{
		if(ui.edHostsRequest->text().isEmpty())
			CONF_ERR(tr("No request for the status of hosts."), ui.edHostsRequest, serverTab);
	}

	if(ui.cbServicesRequest->isChecked())
	{
		if(ui.edServicesRequest->text().isEmpty())
			CONF_ERR(tr("No request for the status of services."), ui.edServicesRequest, serverTab);
	}

	if(ui.cbUseHTTPAuth->isChecked())
	{
		if(ui.edLogin->text().isEmpty())
			CONF_ERR(tr("Not specified HTTP username."), ui.edLogin, serverTab);
		if(ui.edPassword->text().isEmpty())
			CONF_ERR(tr("Not specified HTTP password."), ui.edPassword, serverTab);
	}

	if(ui.cbHostsRegExp->isChecked())
	{
		QRegExp rx(ui.edHostsRegExp->text());
		if(!rx.isValid() || ui.edHostsRegExp->text().isEmpty())
		{
			CONF_ERR(tr("Not a valid regular expression to analyze the status of hosts."),
					ui.edHostsRegExp, statusTab);
		}
	}

	if(ui.cbSrvcsRegExp->isChecked())
	{
		QRegExp rx(ui.edSrvcsRegExp->text());
		if(!rx.isValid() || ui.edSrvcsRegExp->text().isEmpty())
			CONF_ERR(tr("Not a valid regular expression to analyze the status of services."),
					ui.edSrvcsRegExp, statusTab);
	}

	if(ui.rbBaseRegExpConfig->isChecked())
	{
		if(!ui.cbHostDownStat->isChecked() && !ui.cbHostUnreachStat->isChecked())
			CONF_ERR(tr("Not specified no status for the host!"), ui.frameBasicRegExpConf, statusTab);
		if(!ui.cbServiceCritStat->isChecked() && !ui.cbServiceWarnStat->isChecked() &&
				!ui.cbServiceUnknownStat->isChecked())
			CONF_ERR(tr("Not specified no status for the service!"), ui.frameBasicRegExpConf, statusTab);
	}

	if(ui.cbUseHttps->isChecked())
	{
	    if (!QSslSocket::supportsSsl())
	    	CONF_ERR(tr("<font>You can obtain the necessary libraries for work using HTTPS can "
	    			"<a href='http://www.openssl.org/related/binaries.html'>here</a>."
	    			"The work using HTTPS is impossible now.</font>"),
	    			ui.cbUseHttps, serverTab);
	}

	if(ui.cbUseFilterRegExpHosts->isChecked())
	{
		QRegExp rx;
		QString invalidRx;
		int irxc = 0;
		for(int i = 0; i < ui.listHosts->count(); ++i)
		{
			rx.setPattern(ui.listHosts->item(i)->text());
			if(!rx.isValid())
			{
				++irxc;
				invalidRx += "'" + ui.listHosts->item(i)->text() + "' ";
			}
		}
		if(irxc)
			CONF_ERR(tr("Incorrect regular expression(s) "
					"in the list of hosts: ", "", irxc) + invalidRx,
				ui.listHosts, filterHostsTab);
	}

	if(ui.cbUseFilterRegExpSrvcs->isChecked())
	{
		QRegExp rx;
		QString invalidRx;
		int irxc = 0;
		for(int i = 0; i < ui.listServices->count(); ++i)
		{
			rx.setPattern(ui.listServices->item(i)->text());
			if(!rx.isValid())
			{
				++irxc;
				invalidRx += "'" + ui.listServices->item(i)->text() + "' ";
			}
		}
		if(!invalidRx.isEmpty())
			CONF_ERR(tr("Incorrect regular expression(s) "
					"in the list of services: ", "", irxc) + invalidRx,
					ui.listServices, filterServicesTab);
	}
#undef CONF_ERR
#undef CONF_WARN
	return true;
}

void NagiosAgent::configSaveAndClose()
{
	saveCommonConfig();
	if(profileSave())
	{
		hide();
		updateTrayToolTip();
	}
}

void NagiosAgent::trayFlashed()
{
	if(--flashingCounter < 1)
	{
		timerFlash.stop();
		setSuitableIcon();
		return;
	}
	if(!currentIcon)
		return;
	if(stateUp)
		currentIcon = (currentIcon != &hostUpIcon1)? &hostUpIcon1 : &hostUpIcon2;
	else
		currentIcon = (currentIcon != &hostDownIcon1)? &hostDownIcon1 : &hostDownIcon2;
	trayIcon.setIcon(QIcon(*currentIcon));
}

void NagiosAgent::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
	QPoint pt = QCursor::pos();

	switch (reason)
	{
	case QSystemTrayIcon::Trigger:
		if(commonConfig.menuNumOnLMK == hostsSrvcsMenu)
			showObjectsMenu(hostsSrvcsMenu);
		else if(commonConfig.menuNumOnLMK == hostsMenu)
			showObjectsMenu(hostsMenu);
		else
			showObjectsMenu(srvcsMenu);
		break;
	case QSystemTrayIcon::MiddleClick:
		if(commonConfig.menuNumOnLMK == srvcsMenu)
			showObjectsMenu(hostsMenu);
		else if(commonConfig.menuNumOnLMK == hostsMenu)
			showObjectsMenu(srvcsMenu);
		break;
	case QSystemTrayIcon::Unknown:
	default:
		return;
	}

	return;
}

void NagiosAgent::closeAllPopups()
{
	//Даём сигнал всем порождённым всплывающим окнам закрыться.
	closeAllInformWin();
}

void NagiosAgent::showAbout()
{
	(new About())->show();
}

//Проверка наличия обновлений

void NagiosAgent::checkUpdates()
{
	ui.actionCheckUpdates->setEnabled(false);
	quietUpdate = false;
	updater->check();
}

void NagiosAgent::checkUpdatesQuiet()
{
	ui.actionCheckUpdates->setEnabled(false);
	quietUpdate = true;
	updater->check();
}

void NagiosAgent::checkUpdateFinished()
{
	if(updater->isErrorOccured())
	{
		if(!quietUpdate)
		{
			sysTrayBufferStr.clear();
			trayIcon.showMessage("Nagios Agent",
					tr("Can't get information about latest version: '") +
					updater->errorDescription() + "'",
					QSystemTrayIcon::Warning);
		}
	} else if(updater->binVersion() > CURRENT_VERSION)
	{
		QMessageBox::information(this, "Nagios Agent",
				tr("<font>New version of programm is available: ") + updater->strVersion() +
				tr("<br>Current version: ") + VERSION +
				tr("<br>Click <a href='") + updater->updateUrl() + tr("'>here</a> for download.</font>"),
				QMessageBox::Information);
	}
	else if(!quietUpdate)
		QMessageBox::information(this, "Nagios Agent",
				tr("You are using the latest stable version of the program."),
				QMessageBox::Information);
	quietUpdate = true;
	if(!commonConfig.disableAutoUpdate)
	{
    	timerUpdateChecker.setSingleShot(true);
    	timerUpdateChecker.setInterval(defaultUpdateCheckInterval);
		timerUpdateChecker.start();
	}
	ui.actionCheckUpdates->setEnabled(true);
}

//Слоты вызывающиеся сигналами от GUI

//Выполняется при смене пункта выбранного в cboxProfileName
void NagiosAgent::serverProfileChanged(QString profileName)
{
	if(needSave() && !profileSave())	//Если профиль не удалось сохранить (ошибка конфирурирования) - возвращаем
	{	//cboxProfileName в предыдущую позицию. Отключаем сигнал currentIndexChanged() от класса
		//для того что бы при смене индекса не произошла перезагрузка конфигуации и не были
		//потеряны изменения.
		disconnect(ui.cboxProfileName, SIGNAL(currentIndexChanged(QString)), this, SLOT(serverProfileChanged(QString)));
		ui.cboxProfileName->setCurrentIndex(ui.cboxProfileName->findText(currentLoadedProfile));
		connect(ui.cboxProfileName, SIGNAL(currentIndexChanged(QString)), this, SLOT(serverProfileChanged(QString)));
		return;
	}
	loadServerConfigToGui(profileName);
	configChanged = false;
}

bool NagiosAgent::profileSave()
{
	if(!isSavingConfigOk())
		return false;
	if(!configChanged)
		return true;
	saveServersConfig(currentLoadedProfile);
	configChanged = false;
	//Если после сохранения число запущенных серверных сессий меньше одного -
    if(ServerSession::activeSessionsCount() < 1)	//значит у нас нет правильных активных конфигураций.
    {
    	MSG("Active profile is not found.");
    	trouble(tr("Active profiles is not found."));
    }
    setSuitableIcon();
    updateTrayToolTip();
    return true;
}

void NagiosAgent::profileNew()
{
	//Проверяем не изменились ли настройки текущего профиля
	if(needSave() && !profileSave())
			return;
	//Показываем окно предлагающее ввести имя нового профиля
	RenameProfile * renProf = new RenameProfile;
	renProf->setWindowTitle(tr("Enter new profile name"));
	renProf->setGeometry(geometry().left() + width()/2 - renProf->width()/2,
			geometry().top() + this->geometry().height()/2 - renProf->height()/2,
			renProf->width(), renProf->height());
	connect(renProf, SIGNAL(emitNewName(const QString)), this, SLOT(newProfileName(const QString)));
	renProf->show();
	updateTrayToolTip();
	setSuitableIcon();
}

void NagiosAgent::profileSetDefault()
{
	if(ui.cboxProfileName->currentText().isEmpty())
		return;

	if(QMessageBox::warning(this, tr("Nagios Agent: changing configuration"),
			tr("This action will set default configuration for profile '") +
			currentLoadedProfile +
			tr("'. Confirm?"),
			QMessageBox::Yes | QMessageBox::No,
			QMessageBox::No) == QMessageBox::Yes)
	{
		setDefaultConfigValues(false);
		configChanged = true;
		updateTrayToolTip();
		setSuitableIcon();
	}
}

void NagiosAgent::profileDelete()
{
	if(ui.cboxProfileName->currentText().isEmpty())
		return;

	if(QMessageBox::warning(this, tr("Nagios Agent: warning"),
			tr("Really delete profile '") + currentLoadedProfile +
			tr("'? This operation can't be undone."),
			QMessageBox::Yes | QMessageBox::No,
			QMessageBox::No) == QMessageBox::No
			)
		return;
	configChanged = false;
	MSG("DELETE" << currentLoadedProfile);
	//Останавливаем сессию
	ServerSession * sess = NULL;
	Q_FOREACH(sess, serversSessions)
	{
		if(sess->name() == currentLoadedProfile)
		{
			if(sess->running())
				sess->stop();
			break;
		}
	}
	//Удаляем сессию
	if(sess)
	{
		serversSessions.removeOne(sess);
		delete sess;
		sess = NULL;
	}
	//Полностью перезаписываем конфиг т.к. необходимо пересоздать конфигурационный массив
	saveServersConfig("", true);
	//Обновляем GUI
	ui.cboxProfileName->clear();
	Q_FOREACH(ServerSession * sess, serversSessions)
		ui.cboxProfileName->addItem(sess->config().profileName);

	currentLoadedProfile =  ui.cboxProfileName->currentText();

	if(serversSessions.count() < 1)
	{
		setDefaultConfigValues();
		configChanged = false;
		QMessageBox::warning(this, tr("Nagios Agent"),
				tr("No profiles of Nagios servers found. Please add a profile."));
		setDisableControls();	//Деактивируем контролы т.к. невозможно что-либо редактировать
		ui.tabConfig->setCurrentIndex(serverTab);
		profileNew();
	}
	else
		loadServerConfigToGui();
	updateTrayToolTip();
	setSuitableIcon();
}

void NagiosAgent::profileRename()
{
	if(ui.cboxProfileName->currentText().isEmpty())
		return;

	RenameProfile * renProf = new RenameProfile;
	renProf->setWindowTitle(tr("Enter new name for profile '") + currentLoadedProfile + "'");
	renProf->setGeometry(geometry().left() + width()/2 - renProf->width()/2,
			geometry().top() + this->geometry().height()/2 - renProf->height()/2,
			renProf->width(), renProf->height());
	connect(renProf, SIGNAL(emitNewName(const QString)), this, SLOT(profileRenamed(const QString)));
	renProf->show();
}

//Прочие слоты для работы с профилями

void NagiosAgent::profileRenamed(const QString newName)
{
	if(newName.isEmpty())	//Пустое имя обозначет что пользователь нажал "Отмена"
		return;

	//Проверка на дублирование имени профиля
	if( profileNameIndex(newName) != -1)
	{
			QMessageBox::warning(this, tr("Nagios Agent: configuration error"),
					tr("Profile name '") + newName + tr("' is already exist.\n"
						"Please enter another name."));
			profileRename();
			return;
	}
	//Поиск текущего имени профиля в конфиге
	int profileIndex = profileNameIndex(currentLoadedProfile);
	if(profileIndex < 0)
	{
		QMessageBox::critical(this, tr("Nagios Agent: configuration error"),
				tr("Can't find profile with name '") + currentLoadedProfile + tr("' in configuration."));
		return;
	}
	//Запись нового имени профиля
	QSettings config(configDirName, programmName);
	int count = config.beginReadArray(configServersArray);	//Считываем общее число профилей в конфиге
	config.endArray();
	//Переходим в режим записи
	config.beginWriteArray(configServersArray);
	config.setArrayIndex(profileIndex);
	config.setValue("ProfileName", newName);
	config.setArrayIndex(count - 1);	//Устанавливаем правильный размер массива конфигураций
	config.endArray();
	//Обновление имени профиля в текущей конфигурации сервера
	Q_FOREACH(ServerSession * sess, serversSessions)
		if(sess->config().profileName == currentLoadedProfile)
		{
			MSG("Rename profile" << sess->config().profileName << "to" << newName);
			sess->setName(newName);
			break;
		}

	currentLoadedProfile = newName;
	ui.cboxProfileName->setItemText(ui.cboxProfileName->currentIndex(), newName);
}

//Слот принимающий сигнал содержащий имя нового профиля
//Сигнал посылается от окна-объекта RenameProfile
void NagiosAgent::newProfileName(const QString newName)
{
	if(newName.isEmpty())	//Пустое имя обозначет что пользователь нажал "Отмена"
		return;
	//Провереям не дублируются-ли имя профиля
	if(profileNameIndex(newName) != -1)
	{
		QMessageBox::warning(this, tr("Nagios Agent: configuration error"),
				tr("Profile name '") + newName + tr("' is already exist.\n"
					"Please enter another name."));
		profileNew();
		return;
	}

	//Создаём новый профиль в конфигурации
	QSettings config(configDirName, programmName);
	//Считываем общее количество профилей
	int profileIndex = config.beginReadArray(configServersArray);
	config.endArray();

	//Открываем конфиг в режиме записи
	config.beginWriteArray(configServersArray);
	config.setArrayIndex(profileIndex);
	config.setValue("ProfileName", newName);	//Записываем только имя профиля
	//Временно отключаем сигнал выпадающего списка что бы не запустился обработчик
	//смены выбранного пункта в cboxProfileName
    disconnect(ui.cboxProfileName, SIGNAL(currentIndexChanged(QString)), this, SLOT(serverProfileChanged(QString)));
	//Добавляем новый профиль в выпадающий список профилей
	ui.cboxProfileName->addItem(newName);
	ui.cboxProfileName->setCurrentIndex(ui.cboxProfileName->count() - 1);
    connect(ui.cboxProfileName, SIGNAL(currentIndexChanged(QString)), this, SLOT(serverProfileChanged(QString)));
	//Очищаем поле "адрес сервера"
	ui.edServerAddress->clear();
	//Сбрасываем в дефолтные значения GUI конфигурацию
	setDefaultConfigValues(false);
	//Устанавливаем новое имя для текущего профиля
	currentLoadedProfile = newName;
	//Добавляем новый серверный конфиг в список конфигураций
	SessionConfig sConfig = readServerConfig(config, false);
	//Создаём новую серверную сессию
	ServerSession * sess = new ServerSession(sConfig, false);
	serversSessions.append(sess);
	//Закрываем конфигурацию для данного сервера
	config.endArray();
	//Ставим флаг изменения профиля
	configChanged = true;
	//Активируем контролы окна если они были не активны
	if(!ui.tabConfig->isEnabled())
		setDisableControls(false);
	//Подключаем её к слоту
    connect(sess, SIGNAL(newDataAvailable(const QList<NagiosObjectInfo> & )),
    		this, SLOT(newStatusData(const QList<NagiosObjectInfo> & )));
    connect(sess, SIGNAL(sendInformation(const QString & , ServerSession::InfoMessageType )),
			this, SLOT(reciveInfoMessage(const QString & , ServerSession::InfoMessageType )));
    //Ставим фокус на поле адреса сервера - нуждается в обязательном заполнении
    ui.edServerAddress->setFocus();

    updateTrayToolTip();
	setSuitableIcon();
}

void NagiosAgent::onConfigModify()
{
	configChanged = true;
}

void NagiosAgent::onConfigModify(const QString &)
{
	configChanged = true;
}

//Слоты-обработчики данных о смене статусов

void NagiosAgent::newStatusData(const QList<NagiosObjectInfo> & infoList)
{
	ServerSession * senderSession = static_cast<ServerSession*>(sender());

	if(!senderSession)
	{
		MSG("Unknown sender! Exit from function.");
		return;
	}

	if(!infoList.isEmpty())
	{
		Q_FOREACH(NagiosObjectInfo obj, infoList)
			showObjStatChangeMsg(senderSession->config(), obj);
	}

	updateTrayToolTip();
}

void NagiosAgent::reciveInfoMessage(const QString & message, ServerSession::InfoMessageType msgType)
{
	//showDialog установлена в случае возникновения критических ошибок требующих
	//обязательного вмешательства пользователя

	sysTrayBufferStr.clear();
	setSuitableIcon();

	switch(msgType)
	{
	case ServerSession::infoOk:
		trayIcon.showMessage(tr("Nagios Agent: information"), message, QSystemTrayIcon::Information);
		break;
	case ServerSession::infoWarn:

		break;
	case ServerSession::infoError:
		trouble(message);
		break;
	case ServerSession::infoCritError:
		{
			trouble(message, false);
			ServerSession * sess = (ServerSession *) sender();
			switch(QMessageBox::critical(this, tr("Nagios Agent: error"),
					message +
					tr("\n\"Yes\" - terminate programm's execution.\n"
					"\"No\" - continue programm's execution without changing configuration."
					"\n\"Cancel\" - open settings."),
					QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
					QMessageBox::Cancel))
			{
			case QMessageBox::Yes:
				qApp->quit();
				break;
			case QMessageBox::No:
				sess->start();	//Вновь запускаем сессию
				break;
			case QMessageBox::Cancel:
			default:
					showAndReadConfig(sess->config().profileName);
				break;
			}
		}
		break;
	default:
		break;
	}
}

//Слоты обрабатывающие запросы на открытие дополнительной информации о хостах/сервисах

void NagiosAgent::hostsSrvcsMenuHandler(QAction * action)
{
	if(action->data().toString().isEmpty())
		showAndReadConfig();
	else
		QDesktopServices::openUrl(action->data().toString());
}

void NagiosAgent::trayMessageClicked()
{
	if(!sysTrayBufferStr.isEmpty() &&
			(sysTrayBufferStr.indexOf(QRegExp("^https?://"), Qt::CaseInsensitive) > -1))
		QDesktopServices::openUrl(sysTrayBufferStr);
}


//*******************************************
