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

#include "miscclasses.h"

//ObjectsMenu implementation
//Меню с автоматическим созданием QAction содержащих userData
ObjectsMenu::ObjectsMenu(QWidget * parent)
	: QMenu(parent)
{

}

ObjectsMenu::~ObjectsMenu()
{
	//В принципе это происходит автоматически при уничтожении родителя.
	qDeleteAll(actionsList);	 //Но на всякий случай, делаем вручную.
}

void ObjectsMenu::addAction(const QString & text, const QString & link, const bool disabled)
{
	QAction * newAction = new QAction(this);
	newAction->setText(text);
	newAction->setData(link);
	newAction->setDisabled(disabled);
	actionsList << newAction;
	QMenu::addAction(newAction);
}



//Класс облегчающий поиск в списке проблемных сервисов, получает строки вида
//service_name:host1:host2:host3:...:hostN
//в качестве параметра конструктора и разделяет их на имя сервиса (переменная srvcName)
//и список интересующих хостов (переменная hostsList). Если в конструкторе
//указано только имя сервиса - список хостов будет пустым.

ServiceClass::ServiceClass(const QString & srvcDef)
{
	hostsList = srvcDef.split(":", QString::SkipEmptyParts);
	srvcName = hostsList.first();	//получаем имя сервиса
	hostsList.removeFirst();		//удаляем имя сервиса из списка хостов
}

const QString & ServiceClass::service() const
{
	return srvcName;
}

const QStringList & ServiceClass::allHostsAsList() const
{
	return hostsList;
}

QString ServiceClass::allHostsAsString() const
{
	QString hosts;
	Q_FOREACH(QString host, hostsList)
	{
		hosts += host + ":";
	}
	return hosts;
}

bool ServiceClass::isHostFound(const QString & srvc, const QString & hostName,
		const bool useRegExp)
{
	if(useRegExp)
	{
		//Настройка регулярного выражения
		QRegExp rx(srvcName);
		if(!rx.isValid())	//Не верное регулярное выражение
			return false;
		rx.setCaseSensitivity(Qt::CaseInsensitive);
		//Сперва проверяем совпадение сервиса
		if(rx.indexIn(srvc) < 0)	//Не совпадает
			return false;
		//Проверяем совпадение имени хоста
		if(hostsList.isEmpty())
			return true;
		else
		{
			Q_FOREACH(QString host, hostsList)
			{
				rx.setPattern(host);
				if(!rx.isValid())	//Пропускаем не правильный шаблон
					continue;
				if(rx.indexIn(hostName) > -1)	//Найдено совпадение
					return true;
			}
		}
		return false;
	}
	else
	{
		if(srvc.toLower() != srvcName.toLower())
			return false;
		if(hostsList.isEmpty())
			return true;
		else
		{
			Q_FOREACH(QString host, hostsList)
			{
				if(!host.compare(hostName, Qt::CaseInsensitive))
					return true;
			}
		}
		return false;
	}
}
