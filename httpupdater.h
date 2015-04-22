/*
 * httpupdater.h
 *
 *  Created on: 08.06.2009
 *      Author: Chebotarev Roman
 */

#ifndef HTTPUPDATER_H_
#define HTTPUPDATER_H_

#include <QtDebug>
#include <QFile>
#include <QObject>
#include <QStringList>
#include <QHttp>
#include <QUrl>
#include <QList>
#include <QSslError>
#include <QSslSocket>

#include "miscclasses.h"

class HttpUpdater : public QObject
{
	Q_OBJECT
public:
	HttpUpdater(const QString & host = "", const quint16 port = 80, const QString & path = "",
			const QString & updateInfo = "", const bool checkImmediate = false,
			const bool httpsMode = false, const bool denyInvalidCert = true);

	void 	setHost(const QString & host) { updateServer = host; }
	void	setPort(const quint32 & port) { updatePort = port; }
	void 	setPath(const QString & path) { updatePath = path; }
	void 	setUpdateInfoFilename(const QString & updateInfo) { updateInfoFilename = updateInfo; }
	void 	setHttpsMode(const bool use) { useHttps = use; }
	void 	setDenyInvalidCert(const bool deny) { denyInvalidCertificate = deny; }
	void 	setUser(const QString & login = "", const QString & passwd = "") { httpLogin = login; httpPasswd = passwd; }

	void	setUpdateFileLocalName(const QString & name) { updateFileLocalName = name; }
	void	downloadUpdate(const QString & updateUrl);

	bool	isCheckFinished() const { return checkFin; }
	bool	isErrorOccured() const { return updateError; }
	const 	QString & errorDescription() const { return errorDescr; }
			quint32	binVersion() const { return latestBinVersion; }
	const 	QString & strVersion() const { return latestStrVersion; }
	const 	QString & updateUrl() const { return updateFileUrl; }
	const 	QString & infoContent() const { return updateInfoContent; }

	~HttpUpdater();

Q_SIGNALS:
	void		checkUpdateFinished();
	void		downloadUpdateFinished();

public Q_SLOTS:
	void	check();

private Q_SLOTS:
	void		checkFinished(const bool error);
	void		downloadFinished(const bool error);

private:
	bool	parseUpdateInfo(const QString & updateInfoData);
	void	clean();

	enum	httpResponseCodes
	{
		httpOk			= 200,
		httpNotFound	= 404
	};
	QHttp		*httpObj;
	QTcpSocket	*httpSocket;
	QSslSocket	*httpSslSocket;
	bool		useHttps;
	bool		denyInvalidCertificate;
	QString		updateServer;
	quint32		updatePort;
	QString		updatePath;
	QString		updateInfoFilename;
	QString		httpLogin;
	QString		httpPasswd;
	bool		checkFin;
	bool		updateError;
	QString		errorDescr;
	QString		latestStrVersion;
	quint32		latestBinVersion;
	QString		updateFileUrl;
	QString		updateInfoContent;
	QString		updateFileLocalName;
	QFile		*localUpdateFile;
};

#endif /* HTTPUPDATER_H_ */
