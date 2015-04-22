#ifndef NAGIOSAGENT_H
#define NAGIOSAGENT_H

#include <QtDebug>
#include <QtGui>
#include <QKeySequence>
#include <QApplication>
#include <QThread>
#include <QSettings>
#include <QStringList>
#include <QHttp>
#include <QRegExp>
#include <QList>
#include <QSslError>
#include <QSslSocket>
#include <QtEndian>

#include <algorithm>	//Необходим для удаления возможных дублей из списков фильтраций

#include "ui_nagiosagent.h"
#include "informationwindow.h"
#include "httpupdater.h"
#include "serversession.h"
#include "miscclasses.h"
#include "about.h"
#include "renameprofile.h"


#define UPDATE_SERVER	"www.netpatch.ru"
#define UPDATE_PORT		80
#define	UPDATE_PATH		"/projects/nagiosagent/"

#if defined (Q_OS_WIN32)
#define UPDATE_INFO		"latest-version-win32.txt"
#elif defined (Q_OS_LINUX)
#define UPDATE_INFO		"latest-version-linux.txt"
#elif defined(Q_OS_FREEBSD)
#define UPDATE_INFO		"latest-version-fbsd.txt"
#elif defined(Q_OS_MAC)
#define UPDATE_INFO		"latest-version-mac.txt"
#else
#error Unsupported operating system!
#endif

class NagiosAgent : public QWidget
{
    Q_OBJECT
public:
    NagiosAgent(QWidget *parent = 0);
    ~NagiosAgent();

private:

	enum HttpCodes
	{
		httpOk = 200,
		httpMovedPerm = 301,
		httpMovedTemp = 302,
		httpNotFound = 404
	};

	enum TabNums
	{
		serverTab,
		filterHostsTab,
		filterServicesTab,
		statusTab,
		miscTab
	};

	enum MenuTypes
	{
		hostsSrvcsMenu,
		hostsMenu,
		srvcsMenu,
		mainMenu
	};

Q_SIGNALS:
	void closeAllInformWin();

private Q_SLOTS:
    //Непосредственная работа с GUI

	//Открытие конфигурационного окна и загрузка туда одного из серверных профилей
	void	showAndReadConfig(const QString & profileName = "");
    void	useHttpsClicked(const bool use);		//Обработчик щелчка на "использовать HTTPS"
    void	useTrayMsgsClicked(const bool use);	//Обработчик щелчка на "использовать сообщения трея"
    void	addHost();			//Добавление в список игнорируемых хостов
    void	delHost();			//Удаление из списка игнорируемых хостов
    void	addService();		//Добавление в список игнорируемых сервисов
    void	delService();		//Удаление из списка игнорируемых сервисов
    bool	isSavingConfigOk();	//Проверка на правильность сохраняемой из GUI конфигурации
    //Сохранение конфигурации и закрытие конфигурационного окна
    void	configSaveAndClose();
	//Срабатывает при активизации иконки в трее (щелчёк мышью)
    void	trayFlashed();		//Мерцание иконки в трее в случае смены статуса хоста/сервиса
	void	trayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void	closeAllPopups();	//Закрыть все всплывающие окна
    void	showAbout();		//Показать "О программе"

    //Проверка наличия обновлений

    //Запуск проверки обновлений при выборе пункта меню "проверить обновления"
    void	checkUpdates();
    //Запуск проверки обновления по сигналу таймера
    void	checkUpdatesQuiet();
    //Обработчик сигнала checkUpdateFinished() от объекта HttpUpdater
    void	checkUpdateFinished();

    //Работа с профилями

    //Слоты вызывающиеся сигналами от GUI
    void	serverProfileChanged(QString profileName);	//Срабатывает при смене значения ui.cboxProfileName
    bool	profileSave();
    void	profileNew();
    void	profileSetDefault();
    void	profileDelete();
    //Слот вызывающийся при выборе пункта меню переименования профиля (tool-button Actions). Открывает окно
    void	profileRename();	//класса RenameProfile и подключает его сигнал emitNewName() к слоту profileRenamed()

    //Прочие слоты для работы с профилями

    //Слот срабатывающий при закрытии окна RenameProfile в случае переименования профиля
    void	profileRenamed(const QString newName);
    //Слот срабатывающий при закрытии окна RenameProfile в случае создания нового профиля
    void	newProfileName(const QString newName);

    //Слоты срабатывающие при модификации значения контролов (кнопок, edit's т.д.)
    void	onConfigModify();	//Устанавливают переменню configChanged в значение true.
    void	onConfigModify(const QString & text);	//Это обозначает что конфигурация была изменена


    //Слот-обработчик данных о смене статусов
    void	newStatusData(const QList<NagiosObjectInfo> & infoList);
    //Слоты-обработчики служебной информации от серверных сессий
    void	reciveInfoMessage(const QString & errorDescr, ServerSession::InfoMessageType msgType);

    //Слоты обрабатывающие запросы на открытие дополнительной информации о хостах/сервисах

    //Обработчик щелчка на меню трея
    void	hostsSrvcsMenuHandler(QAction * action);
    //Обработчик щелчка на сообщении трея
    void	trayMessageClicked();

private:
	//Внутренние функции-утилиты

    //Служебные функции

	void	initAppResources();	//Инициализирует программные ресурсы: звуки, трансляторы и т.д.
    void	installTranslators(const bool install);	//Установка трансляторов - перевод программы
    void	connectSignals();	//Подключение всех сигналов к слотам
    //Рекурсивное подключение сигналов контролов окна конфигурации к слотам onConfigModify()
    void	connectConfModifSignals(QObject * parentObj);	 //для отслеживания модификации конфигурации
    void	updateInformWinPt();	//Обновление координат всплывающих окон изменения статуса
    void	saveCommonConfig();		//Сохранение общего для всех профилей конфига
    //Проверяет необходимость сохранения конфигурации. Возвращает true если
    inline bool	needSave();	//конфигурация в GUI была изменена
    void	closeEvent(QCloseEvent * event);	//Обработчик закрытия основного окна (конфигурации).
    QString	makeProblemLink(const SessionConfig & config, MenuTypes type = hostsMenu);

	//Функции работы с конфигурацией

	//Читает всю конфигурационную информацию программы. В случае её отсутствия - создаёт
	//конфигурацию по умолчанию
    bool	readConfig(QList<SessionConfig> & sessionsConfigs);
    //Создаёт конфигурацию по умолчанию. Вызывается предыдущей функцией
    void	makeDefaultConfig();
    //Если старый конфиг существует (для версий меньше 1.2) - конвертирует его в новый
    bool	convertFromOldConfig(QSettings & config, QList<SessionConfig> & sessionsConfigs);
    //Устанавливает значения по умолчанию для элементов GUI. В случае resetAll = true
    //очищается список имён профилей и адрес текущего сервера
    void	setDefaultConfigValues(const bool resetAll = true);
	//Возвращает индекс профиля в массиве конфигураций QSettings либо -1
	int		profileNameIndex(const QString & name);	//если профиля с заданным именем не найдено
	//Читает конфигурацию профиля указанного первым параметром, либо если имя профиля пустое -
	//первую попавшуюся конфигурацию
    SessionConfig readServerConfig(const QString profileName = "", bool showError = true);
    //То же что и предыдущий вариант, но в качестве параметра используется раздел QSettings
    SessionConfig readServerConfig(const QSettings & config, const bool showError = true);
    //Функция преобразующая комбинацию из чекбоксов для настройки регулярных выражений в
    //действующее регулярное выражение
    void	makeBasicRegExp(SessionConfig & config);
    //Проверяет конфигурацию сервера переданную первым параметром на правильность
    bool	isConfigOk(SessionConfig & config, bool showError = false);
    //Загружает конфигурацию серверного профиля указанную первым параметром в GUI
    void	loadServerConfigToGui(const QString & profileName = "");
    //Загружает общую конфигурацию в GUI
    void	loadCommonConfigToGui();
    //Создаёт конфигурационную структуру из GUI
    SessionConfig makeConfigFromGui();
    //Сохраняет конфигурацию указанного первым параметром профиля. Если второй параметр true -
    //выполняется полная перезапись массива конфигураций профилей (используется при удалении
    //одного из профилей)
    void	saveServersConfig(const QString & profileName, const bool rewriteAll = false);
    //Сохраняет конфигурационные ключи для отдельного массива профилей. Используется
    //внутри предыдущей функции
    void	saveServerValues(QSettings & config, const SessionConfig & sConfig);

    //Функции работы с GUI

    void	createTrayIcon();	//Создание иконки в трее и меню для неё
    //Обновление всплывающей подсказки для иконки в трее
    void	updateTrayToolTip();
    //Показывает сообщение о смене статуса хоста/сервиса в зависимости от настроек конкретного профиля
    void	showObjStatChangeMsg(const SessionConfig & config, const NagiosObjectInfo & obj);
    //Сообщает о возникновении проблемы возникшей при работе программы - проблемы с сервером,
    inline void trouble(QString reason, const bool showMessage = true);	//конфигурацией и т.д.
    void	startFlashingTray(const bool up);	//Мерцание иконки в трее
    void	createProfilesMenu();	//Создание меню Actions для редактирования профилей
    //Деактивирует контролы в случае отсутствия любых конфигураций которые можно изменить
    void	setDisableControls(const bool disable = true);
    void	showObjectsMenu(MenuTypes type);	//Создаёт popup меню трея в зависимости от заданного типа
    void	setSuitableIcon();	//Устанавливает подходящую к текущему времени иконку в трее

	Ui::NagiosAgentClass ui;

	QList<ServerSession *> 	serversSessions;	//Список серверных сессий

    QTranslator commonTranslator;
    QTranslator programmTranslator;
    QDir		appDir;
	QSystemTrayIcon trayIcon;
	QMenu 		trayMainMenu;
	QMenu		profileMenu;
	QTimer		timerFlash;
	QTimer		timerUpdateChecker;
	QPoint		informWinPt;
	bool		stateUp;
	bool		quietUpdate;			//Если true, то не сообщать об ошибках в процессе проверки обновлений
	const QString * currentIcon;
	int			flashingCounter;
	QString		sysTrayBufferStr;
	HttpUpdater *updater;
	QString		currentLoadedProfile;	//Имя текущего профиля загруженного в GUI
	bool		configChanged;

	static const int	defaultAvoidOverlapMsgs = true;
	static const int	defaultWidth = 505;
	static const int	defaultHeight = 440;
	static const int	defaultMenuNumOnLMK = hostsSrvcsMenu;
	static const int	defaultUpdateCheckInterval = 1000 * 60 * 60 * 24; //Одни сутки
	static const bool	defaultDisableLocalization = false;
	static const bool	defaultDisableAutoUpdate = false;
	static const int	flashingTimeout = 400;	//Таймаут мигания иконки в трее
	static const int	constStartX = 100;
	static const int	constStartY = 100;
	static const QString	configDirName;
	static const QString	programmName;
	static const QString	commonSettings;
	static const QString	configServersArray;
	static const QString	trayToolTipHeader;
	static const QString	hostUpSnd;
	static const QString	hostDownSnd;
	static const QString	troubleSnd;
	static const QString	programmTranslatorName;
	static const QString	commonTranslatorName;
	static const QString	normalTrayIcon;
	static const QString	problemTrayIcon;
	static const QString	serverProblemTrayIcon;
	static const QString	hostUpIcon1;
	static const QString	hostUpIcon2;
	static const QString	hostDownIcon1;
	static const QString	hostDownIcon2;
	//Структура в которой хранится активная конфигурация времени выполнения
	struct
	{
		bool	avoidOverlapMsgs;
		bool	disableLocalization;
		bool	disableAutoUpdate;
		int		menuNumOnLMK;
	}commonConfig;
};

#endif // NAGIOSAGENT_H
