#ifndef INFORMATIONWINDOW_H
#define INFORMATIONWINDOW_H

#include <QtGui/QWidget>
#include <QDateTime>
#include <QTimer>
#include <QSound>
#include <QCloseEvent>
#include "ui_informationwindow.h"
#include "miscclasses.h"
//#include "sessionconfig.h"

//Класс описывающий окно с информацией о смене статуса хоста или сервиса

enum InformationTypes
{
	hostUp,
	hostDown,
	serviceUp,
	serviceDown
};

class InformationWindow : public QWidget
{
    Q_OBJECT
public:
    InformationWindow(QWidget *parent, const NagiosObjectInfo & obj,
    		const SessionConfig & config, const QPoint & leftCorner);
    ~InformationWindow();
    static const int windowWidth = 280;
    static const int windowHeight = 121;
Q_SIGNALS:
	void deleteMe();

private Q_SLOTS:
    void processCbAutoClose(bool checked);

private:
    Ui::InformationWindowClass ui;
    QTimer	timerAutoClose;
    void closeEvent(QCloseEvent * event);
};

#endif // INFORMATIONWINDOW_H
