#ifndef MISCCLASSES_H
#define MISCCLASSES_H

#include <QString>
#include <QStringList>
#include <QAction>
#include <QMenu>
#include <QRegExp>
#include <QDebug>


#define QT_NO_DEBUG_OUTPUT

#ifndef QT_NO_DEBUG_OUTPUT
#define MSG(s)	qDebug() << s
#else
#define	MSG(s)
#endif

struct SessionConfig
{
	//Вкладка "Servers settings" главного TabWidget
	QString	profileName;
	bool	profileEnabled;
	//Вкладка "Server"
	QString serverAddress;
	QString	serverPort;
	int 	requestPeriod;
	bool	useHttps;
	bool 	autoClose;
	int 	autoCloseTime;
	bool	useUserDefExecDir;
	QString execDir;
	bool 	useUserDefHostRequest;
	QString	hostsRequest;		//Запрос исключая путь к исполняемому каталогу (/cgi-bin/nagios например)
	bool	useUserDefSrvcsRequest;
	QString	servicesRequest;
	bool	useHTTPAuth;
	QString	httpLogin;
	QString httpPassword;
	//Вкладка "Filtering hosts"
	bool	allExceptSpecifedHosts;
	bool	onlyListedHosts;
	QStringList hostsList;
	bool	useFilterRegExpHosts;
	//Вкладка "Filtering service"
	bool	allExceptSpecifedSrvcs;
	bool	onlyListedSrvcs;
	QStringList	srvcsList;
	bool	useFilterRegExpSrvcs;
	//Вкладка "Statuses"
	bool	useBasicRegExpConfig;
	bool	useHostDownStat;
	bool	useHostUnreachStat;
	bool	useSrvCritStat;
	bool	useSrvWarnStat;
	bool	useSrvUnknStat;
	bool	useUserDefHostsRegExp;
	QString hostsRegExp;
	int		hostStatRxPos;
	int		hostLinkRxPos;
	int		hostNameRxPos;
	bool	useUserDefSrvcsRegExp;
	QString srvcsRegExp;
	int		srvStatRxPos;
	int		srvLinkRxPos;
	int		srvNameRxPos;
	//Вкладка "Misc"
	bool	enableHosts;
	bool	showHostsStateChanged;
	bool	enableSrvcs;
	bool	showSrvcsStateChanged;
	bool	popupsAlwaysStayOnTop;
	bool	useTrayMessages;
	bool	openUrlFromTray;
	bool	disableSounds;
	bool	flashesInTray;
	bool	denyInvalidCert;

	bool	validConfig;
};

struct NagiosObjectInfo
{
	enum ObjectType
	{
		host,
		service
	};

	enum ObjectState
	{
		objectDown,
		objectUp
	};

	ObjectType	type;
	ObjectState	state;
	QString	name;
	QString url;
	QString	strState;
	QString description;

	bool operator== (const NagiosObjectInfo & other)
	{
		return (name == other.name) && (type == other.type) && (state == other.state);
	}

	bool operator!= (const NagiosObjectInfo & other)
	{
		return !(*this == other);
	}
};


//Меню с автоматическим созданием QAction содержащих userData
class ObjectsMenu : public QMenu
{
	Q_OBJECT
public:
	ObjectsMenu(QWidget * parent = NULL);
	~ObjectsMenu();

	void addAction(const QString & text, const QString & link = "",
			const bool disabled = false);
private:
	QList<QAction *> actionsList;
};

//Класс облегчающий поиск в списке проблемных сервисов, получает строки вида
//service_name:host1:host2:host3:...:hostN
//в качестве параметра конструктора и разделяет их на имя сервиса (переменная srvcName)
//и список интересующих хостов (переменная hostsList). Если в конструкторе
//указано только имя сервиса - список хостов будет пустым.

class ServiceClass
{
public:
	ServiceClass(const QString & srvcDef);
	const 	QString		& service() const;
			QString		allHostsAsString() const;
	const 	QStringList	& allHostsAsList() const;
			bool		isHostFound(const QString & srvc, const QString & hostName,
					const bool useRegExp = false);
private:
	QString		srvcName;
	QStringList	hostsList;
};

#endif
