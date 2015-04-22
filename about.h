#ifndef ABOUT_H
#define ABOUT_H

#define VERSION 		"1.2.0.1"
#define CURRENT_VERSION	0x01020001
#define BUILD_DATE		"09.08.2009"

#include <QtGui/QWidget>
#include <QDesktopWidget>
#include <QRect>
#include <QCloseEvent>
#include "ui_about.h"

class About : public QWidget
{
    Q_OBJECT

public:
    About(QWidget *parent = 0);
    ~About();
private:
    void closeEvent(QCloseEvent * event);
    Ui::AboutClass ui;
};

#endif // ABOUT_H
