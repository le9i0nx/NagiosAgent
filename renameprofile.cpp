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
#include "renameprofile.h"

RenameProfile::RenameProfile(QWidget *parent)
    : QWidget(parent)
{
	ui.setupUi(this);
	connect(ui.bboxOkCancel, SIGNAL(accepted()), this, SLOT(acceptNewName()));
	connect(ui.bboxOkCancel, SIGNAL(rejected()), this, SLOT(rejectNewName()));
	connect(ui.edNewName, SIGNAL(returnPressed()), this, SLOT(acceptNewName()));
}

void RenameProfile::acceptNewName()
{
	if(ui.edNewName->text().isEmpty())
	{
		QMessageBox::warning(this->parentWidget(), tr("Nagios Agent: configuration error"),
				tr("Profile name can't be empty."));
		ui.edNewName->setFocus();
		return;
	}
	emit emitNewName(ui.edNewName->text());
	close();
}

void RenameProfile::closeEvent(QCloseEvent * event)
{
	//Сообщаем что по завершении обработки события данный объект нужно удалить
	deleteLater();	// т.к. setAttribute(Qt::WA_DeleteOnClose) почему-то не работает
	event->accept();
}


void RenameProfile::rejectNewName()
{
	emit emitNewName("");
}

RenameProfile::~RenameProfile()
{

}
