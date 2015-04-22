/*
 * serversession.cpp
 *
 *  Created on: 27.06.2009
 *      Author: Chebotarev Roman
 */

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

#include "serversession.h"

int ServerSession::runningSessionsCount = 0;
int ServerSession::problemSessCount = 0;

const QString ServerSession::defaultExecDir = "/nagios/cgi-bin/";
const QString ServerSession::defaultHostRequest = "status.cgi?hostgroup=all&style=hostdetail&hoststatustypes=12";
const QString ServerSession::defaultSrvcsRequest = "status.cgi?host=all&servicestatustypes=28";
const QString ServerSession::defaultHttpServerPort = "80";
const QString ServerSession::defaultHttpsServerPort = "443";
const QString ServerSession::defaultAutoCloseTime = "10";
const QString ServerSession::defaultBasicHostsRegExp =
	"^\\s*<td .*\\s?class=['\"](status_list)['\"]\\s?.*><a href=['\"](\\S+)['\"].*>(.+)</a>";
const QString ServerSession::defaultBasicSrvcsRegExp =
	"^\\s*<TD .*\\s?class=['\"](status_list)['\"]\\s?.*><a href=['\"](\\S+)['\"].*>(.+)</a>";
const QString ServerSession::defaultExpertHostsRegExp =
	"^\\s*<td .*\\s?class=['\"](statusHOSTDOWN|statusHOSTUNREACHABLE)['\"]\\s?.*><a href=['\"](\\S+)['\"].*>(.+)</a>";
const QString ServerSession::defaultExpertSrvcsRegExp =
	"^\\s*<TD .*\\s?class=['\"](statusBGCRITICAL)['\"]\\s?.*><a href=['\"](\\S+)['\"].*>(.+)</a>";


ServerSession::ServerSession(const SessionConfig & config, bool launchNow)
	: runningConfig(config)
{
	httpConnectingTime = 0;
	httpErrorsCount = 0;

	sessionRunning = false;
	authReqError = false;
	connectOk = true;
	hasProblem = false;
	firstSrvcsUpdate = true;
	firstHostsUpdate = true;
	httpObj = NULL;
	httpSocket = NULL;
	httpSslSocket = NULL;

	if(runningConfig.profileEnabled && runningConfig.validConfig && launchNow)
		start();
}

void ServerSession::setConfig(const SessionConfig & newConfig)
{
	if(sessionRunning)
		stop();

	//Если мы отключили профиль - очищаем список проблемных объектов
	if(!newConfig.profileEnabled ||
			(newConfig.serverAddress != runningConfig.serverAddress) ||
			(newConfig.serverPort != runningConfig.serverPort) ||
			(newConfig.execDir != runningConfig.execDir)
	)
	{
		problemObjects.clear();
	}

	setProblem(false);
	authReqError = false;
	connectOk = true;
	hasProblem = false;

	runningConfig = newConfig;

	//Если отключен мониторинг хостов/сервисов - очищаем список от объектов соответствующего типа
	if(!runningConfig.enableHosts && ! problemObjects.isEmpty())
	{
		Q_FOREACH(NagiosObjectInfo obj, problemObjects)
		{
			if(obj.type == NagiosObjectInfo::host)
				problemObjects.removeOne(obj);
		}
	}

	if(!runningConfig.enableSrvcs && ! problemObjects.isEmpty())
	{
		Q_FOREACH(NagiosObjectInfo obj, problemObjects)
		{
			if(obj.type == NagiosObjectInfo::service)
				problemObjects.removeOne(obj);
		}
	}

	if(runningConfig.profileEnabled)
		start();
}

ServerSession::~ServerSession()
{
	stop();
}

//Вспомогательная функция для упорядочевания списка информации по именам
bool lessNameNO(const NagiosObjectInfo & obj1, const NagiosObjectInfo & obj2)
{
    return  obj1.name.toLower() < obj2.name.toLower();
}

QList<NagiosObjectInfo> ServerSession::downHosts() const
{
	QList<NagiosObjectInfo> hostsList;
	Q_FOREACH(NagiosObjectInfo obj, problemObjects)
		if(obj.type == NagiosObjectInfo::host)
			hostsList << obj;
	qSort(hostsList.begin(), hostsList.end(), lessNameNO);
	return hostsList;
}

QList<NagiosObjectInfo> ServerSession::problemSrvcs() const
{
	QList<NagiosObjectInfo> srvcsList;
	Q_FOREACH(NagiosObjectInfo obj, problemObjects)
		if(obj.type == NagiosObjectInfo::service)
			srvcsList << obj;
	qSort(srvcsList.begin(), srvcsList.end(), lessNameNO);
	return srvcsList;
}

int ServerSession::downHostsCount() const
{
	int count = 0;
	Q_FOREACH(NagiosObjectInfo obj, problemObjects)
		if(obj.type == NagiosObjectInfo::host)
			++count;
	return count;
}

int ServerSession::problemSrvcsCount() const
{
	int count = 0;
	Q_FOREACH(NagiosObjectInfo obj, problemObjects)
		if(obj.type == NagiosObjectInfo::service)
			++count;
	return count;
}

void ServerSession::connectSignals()
{
    connect(httpObj, SIGNAL(requestFinished ( int , bool )), this, SLOT(processHttpFinished(int, bool)));
    connect(httpObj, SIGNAL(sslErrors (const QList<QSslError> &)), this, SLOT(processSslErrors(const QList<QSslError>&)));
    connect(&timerRequests, SIGNAL(timeout()), this, SLOT(processTimer()));
}

void ServerSession::disconnectSignals()
{
    disconnect(httpObj, SIGNAL(requestFinished ( int , bool )), this, SLOT(processHttpFinished(int, bool)));
    disconnect(httpObj, SIGNAL(sslErrors (const QList<QSslError> &)), this, SLOT(processSslErrors(const QList<QSslError>&)));
    disconnect(&timerRequests, SIGNAL(timeout()), this, SLOT(processTimer()));
}

void ServerSession::processError(const QString & errorDescr, const bool critical)
{
	if((httpErrorsCount < maxHttpErrors) && !critical)
	{	//О не критических ошибках HTTP сообщаем только в случае превышения лимита.
		MSG("Inc httpErrorsCount");
		++httpErrorsCount;
		return;
	}

	MSG("Processing error");

	setProblem(true);

	if(critical)	//Если нужно показать диалог пользователю - прерываем выполнение запросов
	{
		timerRequests.stop();
		httpObj->abort();
	}

	if(!problemObjects.isEmpty())
		problemObjects.clear();

	if(critical || connectOk)
	{
		connectOk = false;
		emit sendInformation(
				(runningSessionsCount > 1 ? tr("Profile '") + runningConfig.profileName + "'\n" : "" ) +
				errorDescr, critical ? infoCritError : infoError);
	}
}

void ServerSession::setProblem(const bool set)
{
	//Проблема с сервером уже зафиксирована
	if(hasProblem && set)
		return;
	//Проблема не зафиксированна и производится попытка ещё раз "удалить" проблему
	if(!hasProblem && !set)
		return;

	if(hasProblem && !set)	//Проблема уже зафиксированна, но нужно снять указать что проблема теперь отсутствует
	{
		--problemSessCount;
		hasProblem = false;
	}
	else //if(!hasProblem && set) - для наглядности, другого варианта тут быть не может
	{
		++problemSessCount;
		hasProblem = true;
	}
}

void ServerSession::serverConnected()
{
	connectOk = true;
	httpErrorsCount = 0;
	setProblem(false);
	emit sendInformation(
			(runningSessionsCount > 1 ? tr("Profile '") + runningConfig.profileName + "'\n" : "" ) +
		tr("Connection with the server is established."));
}

void ServerSession::start()
{
	if(sessionRunning)
	{
		if(timerRequests.isActive())
			MSG("Session " << runningConfig.profileName << "already running.");
		else
		{
			authReqError = false;
			firstHostsUpdate = true;
			firstSrvcsUpdate = true;
			timerStart();
		}
		return;
	}

	if(!runningConfig.validConfig)
	{
		MSG("Can't start session for" << runningConfig.profileName << "- configuration is invalid.");
		return;
	}

	sessionRunning = true;
	firstSrvcsUpdate = true;
	firstHostsUpdate = true;
	++runningSessionsCount;

	MSG("START session for" << runningConfig.profileName);

	httpObj = new QHttp;
	if(runningConfig.useHttps)
	{
		httpSslSocket = new QSslSocket;
		if(runningConfig.denyInvalidCert == false)	//Разрешено принимать сертификаты с ошибками
			httpSslSocket->setPeerVerifyMode(QSslSocket::VerifyNone);
		httpObj->setSocket(httpSslSocket);
	}
	else
	{
		httpSocket = new QTcpSocket;
		httpObj->setSocket(httpSocket);
	}

	connectSignals();

	httpGetInfo();
	timerStart();
}

void ServerSession::stop()
{
	if(!sessionRunning)
	{
		MSG("Session" << runningConfig.profileName << "already stopped");
		return;
	}
	MSG("STOP session for" << runningConfig.profileName);

	httpConnectingTime = 0;

	sessionRunning = false;
	authReqError = false;

	--runningSessionsCount;

	disconnectSignals();
	timerRequests.stop();

	if(httpObj)
	{
		httpObj->abort();
		delete httpObj;
		httpObj = NULL;
	}

	if(httpSocket)
	{
		delete httpSocket;
		httpSocket = NULL;
	}

	if(httpSslSocket)
	{
		delete httpSslSocket;
		httpSslSocket = NULL;
	}
}

void ServerSession::restart()
{
	if(sessionRunning)
		stop();
	start();
}

void ServerSession::processHttpFinished(int id, bool error)
{
	QString httpData;

	int httpStatus = httpObj->lastResponse().statusCode();

	if(id == idGetHosts)
	{
		if(!runningConfig.enableHosts)
			return;
		rawHostsInfo = httpObj->readAll();
	}
	else if(id == idGetSrvcs)
	{
		if(!runningConfig.enableSrvcs)
			return;
		rawSrvcsInfo = httpObj->readAll();
	}
	else
		return;	//Invalid request id

	if(error)
	{
		switch(httpObj->error())
		{
		case QHttp::HostNotFound:
		case QHttp::ConnectionRefused:
		case QHttp::UnexpectedClose:
		case QHttp::InvalidResponseHeader:
		case QHttp::WrongContentLength:
		case QHttp::ProxyAuthenticationRequiredError:
			processError(tr("During the execution of the query\n"
					"an error occurred:\n") +
					"'" + httpObj->errorString() + "'");
		    break;
		case QHttp::AuthenticationRequiredError:
			if(authReqError)
				return;
			authReqError = true;
			processError(tr("HTTP error ocurred: invalid password or username.\n"
								"Monitoring for this profile is impossible."), true);
			authReqError = false;
			break;
		case QHttp::Aborted:
		    	break;
		case QHttp::UnknownError:
		default:
			processError(tr("HTTP error ocurred: unidentified error.\n"
					"Monitoring for this profile is impossible."));
			break;
		}
		return;
	}

	if( httpStatus != httpOk)
	{
		MSG("HTTP code:" << httpStatus);
		processError(tr("HTTP error ocurred: server '") +
						runningConfig.serverAddress + tr("' return error ")
						+ QString::number(httpStatus) + "\n" +
						tr("Monitoring for this profile is impossible."));
		return;
	}

	if(!connectOk|| (firstHostsUpdate && firstSrvcsUpdate))
		serverConnected();

	if(id == idGetHosts)
		processHostsInfo();
	else if(id == idGetSrvcs)
		processSrvcsInfo();
}

void ServerSession::processSslErrors(const QList<QSslError> & sslErrorsList)
{
	if(runningConfig.denyInvalidCert == false)
		return;

	httpObj->close();

	QString errors;
	int num = 0;
	Q_FOREACH(QSslError error, sslErrorsList)
		errors += QString::number(++num) + ") " + error.errorString() + "\n";
	processError(tr("Server certificate ") + runningConfig.serverAddress + ":" +
			runningConfig.serverPort +
			tr(" contains the following errors:\n") + errors +
			tr("On the tab \"Misc\" in configuration window, you can allow SSL certificates which contain errors.\n"),
			true);
}

void ServerSession::processTimer()
{
	timerRequests.setInterval(runningConfig.requestPeriod * 1000); //Передаём значение переведённое из секунд в миллисекунды
	httpGetInfo();
}

void ServerSession::httpGetInfo()
{
	if(runningConfig.useHTTPAuth)
		httpObj->setUser(runningConfig.httpLogin, runningConfig.httpPassword);
	else
		httpObj->setUser("", "");
	if(httpObj->state() == QHttp::Connecting)
	{	//Если QHttp объект нахдится в состоянии Connecting на момент наступления
		//времени выполнения следующего запроса, то производим ряд проверок
		//что бы вычислить наступление таймаута.
		httpConnectingTime += runningConfig.requestPeriod;
		if(httpConnectingTime >= maxHttpConnectingTime)	//Время ожидания установления соединения
		{												//превысило максимально допустимое.
			processError(tr("HTTP error ocurred: connection is timed out.\n"
					"Monitoring for this profile is impossible."));
			httpConnectingTime = 0;
		}
	}
	else
	{	//Объект QHttp не находится в состоянии Connecting - следовательно подключение уже совершено
		//информация предыдущего запроса обработана.
		httpConnectingTime = 0;
		httpObj->setHost(runningConfig.serverAddress,
						(runningConfig.useHttps)? QHttp::ConnectionModeHttps : QHttp::ConnectionModeHttp,
						runningConfig.serverPort.toInt(0, 10));
		if(runningConfig.enableHosts)
			idGetHosts = httpObj->get(runningConfig.execDir + runningConfig.hostsRequest);
		if(runningConfig.enableSrvcs)
			idGetSrvcs = httpObj->get(runningConfig.execDir + runningConfig.servicesRequest);
	}
}

void ServerSession::timerStart()
{
	timerRequests.start(runningConfig.requestPeriod * 1000); //Передаём значение переведённое из секунд в миллисекунды
}

bool ServerSession::searchHost(const QString & host)
{
	if(runningConfig.useFilterRegExpHosts)
	{
		QRegExp rx;
		rx.setCaseSensitivity(Qt::CaseInsensitive);
		Q_FOREACH(QString hostFromList, runningConfig.hostsList)
		{
			rx.setPattern(hostFromList);
			if(!rx.isValid())
				continue;
			if(rx.indexIn(host) > -1)
				return true;
		}
	}
	else
		if(runningConfig.hostsList.indexOf(
				QRegExp(host, Qt::CaseInsensitive, QRegExp::FixedString))
				> -1)
			return true;

	return false;
}

void ServerSession::processHostsInfo()
{
	if(rawHostsInfo.isEmpty())
	{
		MSG("Empty data from hosts!");
		return;
	}

	QList<NagiosObjectInfo> objList;
	bool foundInHostsList;
	QString port;
	QStringList newHosts;	//Временный список хранящий новую информацию о упавших хостах,
							//используется для проверки текущего списка упавших хостов с новым
							//с целью определения поднявшихся хостов.
	QStringList httpStrings = rawHostsInfo.split("\n", QString::SkipEmptyParts);	//Разделяем полученный HTML текст на список строк
	QRegExp rx(runningConfig.hostsRegExp);
	rx.setCaseSensitivity(Qt::CaseInsensitive);
	//Анализируем полученные HTTP данные, в случае совпадения с регулярным выражением
    Q_FOREACH(QString str,  httpStrings)
    {
    	if(rx.indexIn(str) > -1)	//Совпадение регулярного выражения
    	{
    		NagiosObjectInfo nObj;
    		//Проверяем не находится ли данный хост в списке исключений
    		QString host = rx.cap(runningConfig.hostNameRxPos);

    		foundInHostsList = searchHost(host);

    		if(foundInHostsList && runningConfig.allExceptSpecifedHosts)
    			continue;
    		else if(!foundInHostsList && runningConfig.onlyListedHosts)
    				continue;
			//Добавляем в временный список хостов для последующей проверки на поднявшиеся хосты
    		newHosts << host;
    		//Проверяем не добавлен ли уже данный хост в список
    		Q_FOREACH(NagiosObjectInfo obj, problemObjects)
    		{
    			if((!obj.name.compare(host, Qt::CaseInsensitive))
    					&& (obj.type == NagiosObjectInfo::host))
    				goto nextString;
    		}

			//Заполняем поля нового объекта
			nObj.name = host;
			nObj.type = NagiosObjectInfo::host;
			nObj.state = NagiosObjectInfo::objectDown;
			nObj.strState = rx.cap(runningConfig.hostStatRxPos).remove("status");

			//Проверка - если порт не дефолтный для выбранного протокола (80/HTTP или 443/HTTP),
			//то добавляем его в номер в ссылку для меню трея
			port.clear();
			if( (runningConfig.useHttps && (runningConfig.serverPort != defaultHttpsServerPort) )
					||
				(!runningConfig.useHttps && (runningConfig.serverPort != defaultHttpServerPort) )
				)
				port = ":" + runningConfig.serverPort;

			nObj.url = (runningConfig.useHttps ? "https://" : "http://") + runningConfig.serverAddress +
				port + runningConfig.execDir + rx.cap(runningConfig.hostLinkRxPos);
			//Добавляем объект в выходной список
			if(runningConfig.showHostsStateChanged)
				objList << nObj;
			//Добавляем объект во внутренний список проблемных объектов
			problemObjects << nObj;
nextString:;
	    }
    }
    //Сравниваем текущий список упавших хостов (в problemObjects) с новым
	//списком полученном с сервера. В случае отсутствия какого-либо сервера
	//в новом списке - удаляем его из текущего списка упавших хостов
	NagiosObjectInfo nObj;
    Q_FOREACH(NagiosObjectInfo obj, problemObjects)
	{
    	if(obj.type != NagiosObjectInfo::host)
    		goto nextHost;
		if(newHosts.indexOf(QRegExp(obj.name, Qt::CaseInsensitive, QRegExp::FixedString)) > -1)
			goto nextHost;

		//Удаляем хост из списка упавших
		problemObjects.removeOne(obj);

		//Проверяем - не находится-ли данный хост в списке исключкний
		foundInHostsList = searchHost(obj.name);

		if(foundInHostsList && runningConfig.allExceptSpecifedHosts)
			continue;
		else if(!foundInHostsList && runningConfig.onlyListedHosts)
				continue;

		//Добавляем его в выходной список.
		if(runningConfig.showHostsStateChanged)
		{
			nObj = obj;
			nObj.state = NagiosObjectInfo::objectUp;
			objList << nObj;
		}
nextHost:;
	}

    if(!objList.isEmpty() || firstHostsUpdate)
    {
    	emit newDataAvailable(objList);
    	firstHostsUpdate = false;
    }
}

void ServerSession::processSrvcsInfo()
{
	if(rawSrvcsInfo.isEmpty())
	{
		MSG("Empty data from services!");
		return;
	}

	QList<NagiosObjectInfo> objList;
	bool foundInSrvcsList = false;
	QStringList newSrvcs;	//Временный список хранящий новую информацию о упавших хостах,
							//используется для проверки текущего списка упавших хостов с новым
							//с целью определения поднявшихся хостов.
	QString port;
	QString hostSrvc;
	QStringList httpStrings = rawSrvcsInfo.split("\n", QString::SkipEmptyParts);
	QRegExp rx(runningConfig.srvcsRegExp);	//Регулярное выражение для получения информации о сервисах
	QRegExp rxHostName(	//Регулярное выражение для получения имени хоста из ссылки на проблемный сервис
			"host=(\\S+)[&$]"
	);

	rx.setCaseSensitivity(Qt::CaseInsensitive);
	//Анализируем полученные HTTP данные, в случае совпадения с регулярным выражением
	//добавляем к traySrvcsMenu новый Action в поле text - имя проблемного сервиса,
	//в поле data ссылка для открытия этого сервиса в браузере
    Q_FOREACH(QString str,  httpStrings)
    {
    	if(rx.indexIn(str) > -1)
    	{
    		NagiosObjectInfo nObj;
    		if(rxHostName.indexIn(rx.cap(runningConfig.srvLinkRxPos)) < 0)	//Извлекаем из ссылки имя хоста с проблемным сервисом
    			continue;											//если не удалось извлечь - обрабатываем следующую строку
    		//Проверяем не находится ли данный хост в списке исключений
    		QString srvName = rx.cap(runningConfig.srvNameRxPos);
    		QString hostName = rxHostName.cap(1);

    		foundInSrvcsList = false;

			Q_FOREACH(ServiceClass service, runningConfig.srvcsList)
				if(service.isHostFound(srvName, hostName, runningConfig.useFilterRegExpSrvcs))
				{
					foundInSrvcsList = true;
					break;
				}

    		if(foundInSrvcsList && runningConfig.allExceptSpecifedSrvcs)
    			continue;
    		else if(!foundInSrvcsList && runningConfig.onlyListedSrvcs)
    				continue;
			//Добавляем в временный список хостов для последующей проверки на поднявшиеся хосты
    		hostSrvc = srvName + ":" + hostName;
			newSrvcs << hostSrvc;

			//Проверяем не добавлен ли уже данный хост в список
    		Q_FOREACH(NagiosObjectInfo obj, problemObjects)
    		{
    			if((!obj.name.compare(hostSrvc, Qt::CaseInsensitive))
    					&& (obj.type == NagiosObjectInfo::service))
    				goto nextString;
    		}

			//Заполняем поля нового объекта
			nObj.name = hostSrvc;
			nObj.type = NagiosObjectInfo::service;
			nObj.state = NagiosObjectInfo::objectDown;
			nObj.strState = rx.cap(runningConfig.srvStatRxPos).remove("statusBG");

			//Проверка - если порт не дефолтный для выбранного протокола (80/HTTP или 443/HTTP),
			//то добавляем его в номер в ссылку для меню трея
			port.clear();
			if( (runningConfig.useHttps && (runningConfig.serverPort != defaultHttpsServerPort) )
					||
				(!runningConfig.useHttps && (runningConfig.serverPort != defaultHttpServerPort) )
				)
				port = ":" + runningConfig.serverPort;

			nObj.url = (runningConfig.useHttps ? "https://" : "http://") + runningConfig.serverAddress +
				port + runningConfig.execDir + rx.cap(runningConfig.srvLinkRxPos);
			//Добавляем объект в выходной список
			if(runningConfig.showSrvcsStateChanged)
				objList << nObj;
			//Добавляем объект во внутренний список проблемных объектов
			problemObjects << nObj;
	    }
nextString:;
    }
    //Сравниваем текущий список проблемных сервисов (в problemObjects) с новым
	//списком полученном с сервера. В случае отсутствия какого-либо сервиса
	//в новом списке - удаляем его из текущего списка проблемных сервисов
	QString hostName;
	QString currSrvc;
	NagiosObjectInfo nObj;
    Q_FOREACH(NagiosObjectInfo obj, problemObjects)
	{
    	if(obj.type != NagiosObjectInfo::service)
    		goto nextSrvc;
		if(newSrvcs.indexOf(QRegExp(obj.name, Qt::CaseInsensitive, QRegExp::FixedString)) > -1)
			goto nextSrvc;

		//Удаляем хост из списка упавших
		problemObjects.removeOne(obj);

		//Проверяем - не находится-ли данный сервис в списке исключений
		currSrvc = obj.name.split(":").first();
		hostName = obj.name.split(":").last();

		foundInSrvcsList = false;

		Q_FOREACH(ServiceClass service, runningConfig.srvcsList)
			if(service.isHostFound(currSrvc, hostName, runningConfig.useFilterRegExpSrvcs))
			{
				foundInSrvcsList = true;
				break;
			}

		if(foundInSrvcsList && runningConfig.allExceptSpecifedSrvcs)
			continue;
		else if(!foundInSrvcsList && runningConfig.onlyListedSrvcs)
				continue;

		if(runningConfig.showSrvcsStateChanged)
		{
			//Добавляем его в выходной список.
			nObj = obj;
			nObj.state = NagiosObjectInfo::objectUp;
			objList << nObj;
		}
nextSrvc:;
	}

    if(!objList.isEmpty() || firstSrvcsUpdate)
    {
    	emit newDataAvailable(objList);
    	firstSrvcsUpdate = false;
    }
}

