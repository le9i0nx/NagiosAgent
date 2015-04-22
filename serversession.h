/*
 * serversession.h
 *
 *  Created on: 27.06.2009
 *      Author: Chebotarev Roman
 */

#ifndef SERVERSESSION_H_
#define SERVERSESSION_H_

#include <QDebug>
#include <QObject>
#include <QHttp>
#include <QTcpSocket>
#include <QSslSocket>
#include <QStringList>
#include <QSslError>
#include <QTimer>
#include <QSettings>

#include "miscclasses.h"

class ServerSession : public QObject
{
	Q_OBJECT
public:
	ServerSession(const SessionConfig & config, bool launchNow = true);
	~ServerSession();

	static const int & activeSessionsCount() { return runningSessionsCount; }
	static const int & problemSessionsCount() { return problemSessCount; }

	const	SessionConfig & config() const { return runningConfig; }
	const	QString & name() const { return runningConfig.profileName; }
	const	bool & running() const { return sessionRunning; }
	const	bool & problem() const { return hasProblem; }
	//Возвращаем копию списка, а не ссылку, т.к. теоритически в ходе обработки списка функцией запросившей список -
	QList<NagiosObjectInfo> allProblems() const { return problemObjects; }	 //список может быть изменён
	QList<NagiosObjectInfo> downHosts() const;
	QList<NagiosObjectInfo> problemSrvcs() const;
	int	downHostsCount() const;
	int problemSrvcsCount() const;
	void	setConfig(const SessionConfig & newConfig);
	void	setName(const QString & newName) { runningConfig.profileName = newName; }

	enum HttpCodes
	{
		httpOk = 200,
		httpMovedPerm = 301,
		httpMovedTemp = 302,
		httpNotFound = 404
	};

	enum InfoMessageType
	{
		infoOk,
		infoWarn,
		infoError,
		infoCritError
	};

	static const int	defaultProfileEnabled = true;
	static const int 	defaultRequestPeriod = 15; 	//seconds
	static const bool	defaultEnableHostsMonitoring = true;
	static const bool	defaultEnableSrvcsMonitoring = true;
	static const bool	defaultShowHostsMessages = true;
	static const bool	defaultShowSrvcsMessages = true;
	static const bool	defaultUseUserDefHostsRegExp = false;
	static const bool	defaultUseUserDefSrvcsRegExp = false;
	static const bool 	defaultAutoClose = false;
	static const bool	defaultUseUserDefExecDir = false;
	static const bool	defaultUseUserDefHostsRequest = false;
	static const bool	defaultUseUserDefSrvcsRequest = false;
	static const bool	defaultUseHTTPAuth = false;
	static const bool	defaultAllExceptSpecifedHosts = true;
	static const bool	defaultOnlyListedHosts = !defaultAllExceptSpecifedHosts;
	static const bool	defaultAllExceptSpecifedSrvcs = true;
	static const bool	defaultOnlyListedSrvcs = !defaultAllExceptSpecifedSrvcs;
	static const bool	defaultPopupsStayOnTop = true;
	static const bool	defaultOpenUrlFromTray = true;
	static const bool	defaultUseTrayMessages = false;
	static const bool	defaultDisableSounds = false;
	static const bool	defaultUseHttps = false;
	static const bool	defaultDenyInvalidCert = true;		//Отвергаем SSL сертификаты с ошибками
	static const bool	defaultUseBasicRegExpConfig = true;	//Используем базовую конфигурацию фильтрации
	static const bool	defaultUseHostDownStat = true;
	static const bool	defaultUseHostUnreachStat = true;
	static const bool	defaultUseSrvCritStat = true;
	static const bool	defaultUseSrvWarnStat = true;
	static const bool	defaultUseSrvUnknStat = false;
	static const bool	defaultFlashesInTray = true;
	static const int	defaultHostStatRxPos = 1;
	static const int	defaultHostLinkRxPos = 2;
	static const int	defaultHostNameRxPos = 3;
	static const int	defaultSrvStatRxPos = 1;
	static const int	defaultSrvLinkRxPos = 2;
	static const int	defaultSrvNameRxPos = 3;
	static const bool	defaultUseFilterRegExpHosts = false;
	static const bool	defaultUseFilterRegExpSrvcs = false;
	static const QString	defaultHttpServerPort;			//Default HTTP port
	static const QString	defaultHttpsServerPort;
	static const QString	defaultAutoCloseTime;		//minutes
	static const QString	defaultExecDir;
	static const QString	defaultHostRequest;
	static const QString	defaultSrvcsRequest;
	static const QString	defaultBasicHostsRegExp;
	static const QString	defaultBasicSrvcsRegExp;
	static const QString	defaultExpertHostsRegExp;
	static const QString	defaultExpertSrvcsRegExp;
	static const QString	configDirName;
	static const int	maxHttpErrors = 3;			//Максимально допустимое число http ошибок подряд при выполнении запросов,
													//после превышения которого выдаёеся сообщения об ошибке
	static const int	maxHttpConnectingTime = 30;	//Максимальное время в течение которого допустимо производить попытки
													//подключения к серверу без вывода сообщений об ошибках (секунды)

Q_SIGNALS:
	void	newDataAvailable(const QList<NagiosObjectInfo> & infoList);
	void	sendInformation(const QString & errorDescr, ServerSession::InfoMessageType msgType = infoOk);

public	Q_SLOTS:
	void	start();
	void	stop();
	void	restart();

private Q_SLOTS:
    void	processHttpFinished(int id, bool error);
    void	processSslErrors(const QList<QSslError> & sslErrorsList);
    void	processTimer();
    void	httpGetInfo();
    void	timerStart();

private:

	static int runningSessionsCount;
	static int problemSessCount;
    void	connectSignals();
    void	disconnectSignals();
    bool	searchHost(const QString & host);
    void	processHostsInfo();
    void	processSrvcsInfo();
	void	processError(const QString & errorDescr, const bool isCritical = false);
	void	setProblem(const bool set = true);
	void	serverConnected();

	QHttp		* httpObj;
	QTimer 		timerRequests;
	QTcpSocket	* httpSocket;
	QSslSocket	* httpSslSocket;
	int			idGetHosts;
	int			idGetSrvcs;
	int			httpConnectingTime;
	bool		authReqError;
	QString		rawHostsInfo;
	QString		rawSrvcsInfo;
	bool 		connectOk;
	bool		hasProblem;
	bool		sessionRunning;
	bool		firstHostsUpdate;	//Переменная устанавливаемая в true до получения первых данных от сервера
	bool		firstSrvcsUpdate;	//Переменная устанавливаемая в true до получения первых данных от сервера
	int			httpErrorsCount;	//Число ошибок HTTP соединения с предыдущего момента установки соединения
	SessionConfig runningConfig;

	QList<NagiosObjectInfo> problemObjects;
};

#endif /* SERVERSESSION_H_ */
