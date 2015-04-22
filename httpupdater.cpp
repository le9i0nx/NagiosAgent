/*
 * httpupdater.cpp
 *
 *  Created on: 08.06.2009
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

#include "httpupdater.h"

HttpUpdater::HttpUpdater(const QString & host, const quint16 port, const QString & path,
		const QString & updateInfo, const bool checkImmediate,
		const bool httpsMode, const bool denyInvalidCert)
{
	useHttps = httpsMode;
	denyInvalidCertificate = denyInvalidCert;
	updateServer = host;
	updatePort = port;
	updatePath = path;
	updateInfoFilename = updateInfo;
	checkFin = false;

	httpObj = NULL;
	httpSocket = NULL;
	httpSslSocket = NULL;

	if(checkImmediate)
		check();
}

/*
 * Формат строки содержащей информацию о последней доступной версии:
 * D.D.D.D http(s)://url-for-update-file
 * D - десятичные цифры в диапазоне 0 - 255.
 * В случае успешного анализа версия преобразуется к двоичному
 * 4хбайтовому числу каждый из байтов которого соответствует
 * одной из цифр в строковом представлении версии.
 */
bool HttpUpdater::parseUpdateInfo(const QString & updateInfoData)
{
	updateInfoContent = updateInfoData;
	QRegExp rx("^\\s*(\\d+)\\.(\\d+)\\.(\\d+)\\.(\\d+)\\s+(.+)$");
	if(rx.indexIn(updateInfoData) > -1)
	{
		latestBinVersion = rx.cap(1).toUInt() << 24 | rx.cap(2).toUInt() << 16 |
							rx.cap(3).toUInt() << 8 | rx.cap(4).toUInt();
		latestStrVersion = rx.cap(1) + "." + rx.cap(2) + "." + rx.cap(3) + "." + rx.cap(4);
		updateFileUrl = rx.cap(5);
	}
	else
		return false;
	return true;
}

void HttpUpdater::checkFinished(const bool error)
{
	updateError = error;
	if(updateError)
		errorDescr = httpObj->errorString();
	else
	{
		if(httpObj->lastResponse().statusCode() != httpOk)
		{
			updateError = true;
			errorDescr = tr("Can't get information about updates location. Server return HTTP code: ") +
				QString::number(httpObj->lastResponse().statusCode());

		}
		else
		{
			if(!parseUpdateInfo(httpObj->readAll()))
			{
				updateError = true;
				errorDescr = tr("Can't parse information about updates location.");
				MSG("Invalid update info");
			}
		}
	}

	disconnect(httpObj, SIGNAL(done(bool)), this, SLOT(checkFinished(bool)));
	clean();
	emit checkUpdateFinished();

}

void HttpUpdater::clean()
{
	if(httpObj)
	{
		httpObj->close();
		delete httpObj;
		httpObj = NULL;
	}
	if(useHttps && httpSslSocket)
	{
		delete httpSslSocket;
		httpSslSocket = NULL;
	}
	else if(httpSocket)
	{
		delete httpSocket;
		httpSocket = NULL;
	}
}

void HttpUpdater::check()
{
	if(updateServer.isEmpty() || updatePath.isEmpty())
	{
		updateError = true;
		errorDescr = tr("Invalid server address or path to update information.");
		emit checkUpdateFinished();
		return;
	}

	httpObj = new QHttp;

	connect(httpObj, SIGNAL(done(bool)), this, SLOT(checkFinished(bool)));

	if(useHttps)
	{
    	httpSslSocket = new QSslSocket;
    	if(denyInvalidCertificate == false)	//Разрешено принимать сертификаты с ошибками
			httpSslSocket->setPeerVerifyMode(QSslSocket::VerifyNone);
    	httpObj->setSocket(httpSslSocket);
    }
    else
    {
    	httpSocket = new QTcpSocket;
    	httpObj->setSocket(httpSocket);
    }

	if(!httpLogin.isEmpty())
		httpObj->setUser(httpLogin, httpPasswd);

	httpObj->setHost(updateServer, updatePort);
	httpObj->get(updatePath + updateInfoFilename);
}

void HttpUpdater::downloadUpdate(const QString & updateUrl)
{
	QUrl url(updateUrl);
	if(updateFileLocalName.isEmpty())
	{
		updateError = true;
		errorDescr = tr("Empty local filename for update file.");
		emit downloadUpdateFinished();
		return;
	}
	localUpdateFile = new QFile(updateFileLocalName);
	if(!localUpdateFile->open(QIODevice::WriteOnly))
	{
		updateError = true;
		errorDescr = tr("Can't create local file to store update.");
		emit downloadUpdateFinished();
		return;
	}

	httpObj = new QHttp;

	connect(httpObj, SIGNAL(done(bool)), this, SLOT(downloadFinished(bool)));

	if(updateUrl.indexOf(QRegExp("https:", Qt::CaseInsensitive)))
	{
    	httpSslSocket = new QSslSocket;
    	if(denyInvalidCertificate == false)	//Разрешено принимать сертификаты с ошибками
			httpSslSocket->setPeerVerifyMode(QSslSocket::VerifyNone);
    	httpObj->setSocket(httpSslSocket);
    }
    else
    {
    	httpSocket = new QTcpSocket;
    	httpObj->setSocket(httpSocket);
    }

	if(!httpLogin.isEmpty())
		httpObj->setUser(httpLogin, httpPasswd);
	httpObj->setHost(url.host(), url.port(80));
	httpObj->get(updateUrl, localUpdateFile);
}

void HttpUpdater::downloadFinished(const bool error)
{
	localUpdateFile->close();
	delete localUpdateFile;

	updateError = error;
	if(updateError)
		errorDescr = httpObj->errorString();
	else if(httpObj->lastResponse().statusCode() != httpOk)
	{
		updateError = true;
		errorDescr = tr("Server response code: ") +
				QString::number(httpObj->lastResponse().statusCode());
	}
	disconnect(httpObj, SIGNAL(done(bool)), this, SLOT(downloadFinished(bool)));
	clean();
	emit downloadUpdateFinished();
}

HttpUpdater::~HttpUpdater()
{
	if(httpSocket)
		delete httpSocket;
	if(httpSslSocket)
		delete httpSslSocket;
	if(httpObj)
		delete httpObj;
}
