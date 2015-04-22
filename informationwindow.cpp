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

#include "informationwindow.h"

InformationWindow::InformationWindow(QWidget *parent, const NagiosObjectInfo & obj,
		const SessionConfig & config, const QPoint & leftCorner)
    : QWidget(parent,
    		Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::SubWindow | Qt::MSWindowsFixedSizeDialogHint |
    		    		((config.popupsAlwaysStayOnTop)? Qt::WindowStaysOnTopHint : (Qt::WindowFlags) 0) )
{
	//setAttribute(Qt::WA_DeleteOnClose); 		// - почему-то не работает так как нужно: при закрытии окна
												// память не освобождается. Вместо этого используется
												// deleteLater(); в closeEvent()
	ui.setupUi(this);

	setWindowIcon(QIcon(":/icons/icon16x16.png"));

	ui.lbHostLink->setText("<a href=\"" + obj.url + "\">" + obj.name + "</a");
	if(obj.state == NagiosObjectInfo::objectDown)
		ui.lbStatus->setText(obj.strState);
	else
		ui.lbStatus->setText(obj.type == NagiosObjectInfo::host ? "Host UP" : "Service UP");
	if(obj.type == NagiosObjectInfo::host)
		ui.lbObjectName->setText(tr(" Hostname:"));
	else
		ui.lbObjectName->setText(tr(" Name of service:"));
	ui.lbEventTime->setText(QDateTime::currentDateTime().toString("hh:mm:ss dd.MM.yyyy"));
	//Определяем цвет окна
	if(obj.state  == NagiosObjectInfo::objectUp)
		setStyleSheet("QWidget { background: LimeGreen ; font-size: 10pt; font-weight: bold }");
	else
		setStyleSheet("QWidget { background: red ; font-size: 10pt; font-weight: bold; }");
	ui.lbHostLink->setStyleSheet("QLabel { font-size: 12pt; font-weight: bold;}");
	ui.cbAutoClose->setStyleSheet("QCheckBox { font-size: 8pt; font-weight: normal }");
	if(config.autoClose)	//Если указан интервал спустя который окно с сообщением автоматически закрывается
	{	//Запускаем таймер автозакрытия окна
		timerAutoClose.setSingleShot(true);
		timerAutoClose.start(config.autoCloseTime * 1000 * 60); //Переводим минуты в миллисекунды
		ui.cbAutoClose->setEnabled(true);
		ui.cbAutoClose->setChecked(true);
		//При таймауте таймера вызывается close для данного окна
		connect(&timerAutoClose, SIGNAL(timeout()), this, SLOT(close()));
		//В случае снятия флажка "Закрыть автоматически" деактивируем этот флажок и останавливаем таймер
		connect(ui.cbAutoClose, SIGNAL(toggled(bool)), this, SLOT(processCbAutoClose(bool)));
	}

	setWindowTitle(tr("Nagios Agent: change of status"));
	setGeometry(leftCorner.x(), leftCorner.y(), windowWidth, windowHeight);
	ui.lbHostLink->setFocus();	//Переводим фокус на метку, во избежания случаного
								//закрытия окна при нажатии пробела/Return
	show();
}

void InformationWindow::processCbAutoClose(bool checked)
{
	timerAutoClose.stop();
	ui.cbAutoClose->setEnabled(checked);
}

InformationWindow::~InformationWindow()
{

}

void InformationWindow::closeEvent(QCloseEvent * event)
{
	//Сообщаем что по завершении обработки события данный объект нужно удалить
	deleteLater();	// т.к. setAttribute(Qt::WA_DeleteOnClose) почему-то не работает
	event->accept();
}
