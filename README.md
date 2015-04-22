# NagiosAgent
http://www.netpatch.ru/nagiosagent.html

build:

qmake ./NagiosAgent.pro

make

Программа для мониторинга сервера Nagios - Nagios Agent

Поддерживаемые платформы: Windows, Linux, FreeBSD, MacOS.
В данной статье представлена программа для отслеживания изменений статусов хостов и сервисов, мониторинг которых осуществляется Nagios. Программа обладает достаточно широкими возможностями конфигурирования, не требует инсталляции и административных прав для запуска и работы. Она написана с использованием библиотеки Qt, поэтому может использоваться в любой ОС для которой существуют сборки данной библиотеки. Для Nagios существует достаточно много плагинов и утилит (пример здесь) в той или иной мере реализующих подобный функцонал, но все они обладают различными недостатками (на мой взгляд).
Nagios Agent предназначен в первую очередь для сотрудников службы технической поддержки, имеющих склонность не замечать падения критических хостов/сервисов в сети, а так же для сетевых/системных администраторов.

Возможности программы

Nagios Agent может работать с серверами Nagios версий 2.X и 3.X (с версиями 1.X тестирование не проводилось). Поддерживаются два типа опроса статусов: опрос статуса хостов и опрос статуса сервисов (любой из типов опросов можно отключить), используемый протокол - HTTP/HTTPS *. В случае необходимости программой поддерживается фильтрация оповещений о смене статусов хостов/сервисов. В качестве сигнализации о смене статуса применяются всплывающие окна либо сообщения из трея + звуковое сопровождение (см. скриншоты). Текущие списки проблемных хостов и сервисов можно просмотреть щёлкнув левой кнопкой мыши по значку программы в трее. При выборе пункта всплывающего меню со списком проблемных хостов/сервисов внешнему браузеру передаётся ссылка на страничку Nagios содержащую детализированную информацию о данном хосте/сервисе.

Конфигурирование

Начиная с версии 1.2 программа поддерживает мониторинг нескольких серверов. Для каждого из серверов создаётся отдельный профиль описывающий индивидуальные настройки этого сервера.
При первом запуске программа сообщит что не найдено конфигураций профилей и откроет окно конфигурирования. В первую очередь следует задать имя профиля (не путать с именем сервера), в примере используется server1. Основная часть настроек находится на вкладке "Настройки сервера", в её подразделах:

Вкладка "Сервер":

Конфигурация - общие
Адрес сервера - доменное имя или IP адрес сервера Nagios, без указания типа протокола (т.е. без указания http://) и без указания номера порта. Например: 192.168.20.30 или nagios-server.local
Порт сервера - номер HTTP порта сервера Nagios
Период опроса - интервал через который производится посылка запросов, т.е. фактически это период через который обновляется информация о статусах хостов и сервисов. По умолчанию - 15 секунд, минимальное значение - 10 секунд.
Подключаться используя защищённое соединение (HTTPS) - для работы через HTTPS необходимо наличие в системе OpenSSL библиотек. В случае их отуствия при попытке включить использование HTTPS, программа выдаст окно с информацией об ошибке и ссылку откуда можно скачать OpenSSL. После установки OpenSSL программу необходимо перезапустить.
Автоматически закрывать сообщения - если данный флажок установлен, то сообщения в виде всплывающих окон будут автоматически закрываться спустя указанное число минут после появления. В таком случае флажок "Закрыть автоматически" на всплывающем окне (см. скриншоты) будет активен и установлен - если снять флажок то автоматическое закрытие окна (только этого окна - без влияния на общие настройки) отменяется.
Использовать следующую исполняемую директорию - по умолчанию программа считает, что исполняемые скрипты Nagios находятся в каталоге /cgi-bin/nagios/ , т.е. доступ к ним осуществляется по адресу вида http://server-name/cgi-bin/nagios/script-name. Если у вас другой путь к каталогу скриптов, вы можете указать его в данном пункте.
Использовать следующий запрос для хостов - можно изменить имя скрипта и список передаваемых ему параметров для запроса информации о хостах.
Использовать следующий запрос для сервисов - можно изменить имя скрипта и список передаваемых ему параметров для запроса информации о сервисах.
Использовать HTTP авторизацию - если ваш сервер Nagios использует HTTP авторизацию.
Получить путь к каталогу со скриптами Nagios можно скопировав любую ссылку в левом фрейме главной страницы из раздела "Monitoring", после чего выделить из неё путь находящийся между адресом сервера и именем исполняемого скрипта. Например ссылка для "Tactical Overview" (Nagios Version 2.11):
http://nagios-server/cgi-bin/nagios2/status.cgi?hostgroup=all&style=hostdetail&hoststatustypes=12
Синим цветом выделен путь к каталогу скриптов, зелёным - имя скрипта.

Запросы для хостов и сервисов можно получить аналогичным способом. Для хостов нужна ссылка "Host Problems":
http://nagios-server/cgi-bin/nagios2/status.cgi?hostgroup=all&style=hostdetail&hoststatustypes=12
Для сервисов ссылка "Service Problems":
http://nagios-server/cgi-bin/nagios2/status.cgi?host=all&servicestatustypes=28
Синим цветом выделены соответственно запрос для статуса хостов и запрос для статуса сервисов.


Вкладка "Фильтрация хостов":

Конфигурация - хосты
На данной вкладке настраивается фильтрация оповещений о смене статуса хостов. При помощи переключателей можно выбрать режим работы фильтра сообщений. В список нужно добавлять имена хостов в том виде, в котором они отображаются в Nagios, т.е. если у хостов при отображении не указан доменный суффикс, то вносить имена в список хостов нужно так же без доменного суффикса. Регистр букв в именах хостов не учитывается. Регулярные выражения используемые для фильтрации (в случае выбора этой опции) работают в стиле Perl.
HINT: Каждый раз нажимать мышкой на кнопку "Добавить" не обязательно. Вместо этого можно нажимать клавишу Enter (Return) на клавиатуре. Удалять можно клавишей Delete.

Вкладка "Фильтрация сервисов":

Конфигурация - сервисы
Назначение данной вкладки такое же как и у вкладки "Настройка хостов", но конфигурирование несколько отличается.
Общий формат строки для фильтрации оповещений об изменении статусов сервисов:
service name[:host1:host2:...:hostN]
Обязательно указывается только имя сервиса (может содержать пробелы). Если нужно отслеживать (или наоборот игнорировать) данный сервис только для некоторых хостов, то их имена необходимо указывать сразу за именем сервиса, отделяя их друг от друга двоеточиями.
В качестве примера возьмем скриншот окна с настройками приведенный выше: мы не хотим отслеживать изменения абсолютно всех сервисов, но желаем отслеживать изменения статуса всех сервисов хоста other-host, сервиса ping для всех хостов и сервиса load, но только для ряда особо важных (в примере обозначены как important-hostN). В таком случае нужно добавить три строчки:
.*:other-host
ping
load:important-host1:important-host2:important-host3
Для отслеживания всех сервисов конкретного хоста нам понадобилось простейшее регулярное выражение - .* фактически обозначающее любую строку, в нашем случае - любое имя сервиса. Регистр имён сервисов и хостов в данном случае так же не имеет значения.


Вкладка "Статусы":

Конфигурация - статусы
Базовая настройка - назначение данной опции достаточно очевидно если посмотреть на скриншот.
Расширенная настройка - опции "Регулярное выражение для анализа статуса хостов" и "Регулярное выражение для анализа статуса сервисов" рассмотрены ниже.
Опции задания регурярных выражений получения статуса хостов/сервисов сделаны для возможности настройки анализа HTML кода получаемого от сервера без перекомпиляции программы. Используется класс QRegExp поддерживающий регулярные выражения в стиле Perl.
По умолчанию анализ полученного от сервера HTML кода содержащего информацию о статусах хостов осуществляется следующим выражением:

^\s*<td .*\s?class=['"](statusHOSTDOWN|statusHOSTUNREACHABLE)['"]\s?.*><a href=['"](\S+)['"].*>(.+)</a>

Как можно заметить - отслеживаются строки содержащие подстроки statusHOSTDOWN или statusHOSTUNREACHABLE, что соответствует хостам в статусах HOSTDOWN или HOSTUNREACHEABLE.
В случае совпадения строки HTML кода с данным регулярным выражением мы получаем три подстроки (совпадения заключённые в круглых скобках): 1 - статус хоста; 2 - ссылка на детализированную информацию о проблемном хосте; 3 - собственно имя хоста.

Для анализа кода содержащего информацию о статусах сервисов применяется выражение:

^\s*<TD .*\s?class=['"](statusBGCRITICAL)['"]\s?.*><a href=['"](\S+)['"].*>(.+)</a>

В данном случае отслеживаются сервисы только со статусом CRITICAL.
В случае совпадения строки HTML кода с данным регулярным выражением мы получаем три подстроки (совпадения заключённые в круглых скобках): 1 - статус сервиса; 2 - ссылка на детализированную информацию о проблемном сервисе; 3 - имя сервиса.
Как можно заметить - в результате работы регулярного выражения получив имя сервиса мы не получили имя хоста к которому принадлежит данный сервис. Имя хоста в дальнейшем извлекается из 2й подстроки, т.е. из ссылки на детализированную информацию о проблемном сервисе.
В случае более глобального изменения регулярного выражения теоретически могут поменяться номера интересующих нас совпадений. Дабы это не привело к неработоспособности программы под каждым регулярным выражением указываются номера требуемых совпадений. При сохранении конфигурации производится проверка синтаксиса регулярного выражения с выдачей сообщения об ошибке, если регулярное выражение не верно.
Внимание! Синтаксически верное но при этом не верное семантически регулярное выражение может привести к фактической неработоспособности программы! Снятие флажка, задающего использование введенного пользователем регулярного выражения, и последующее сохранение настроек, возвращает данное регулярное выражения к значению по умолчанию. 

Пример: допустим у вас возникает необходимость отслеживать сервисы не только со статусом CRITICAL, но и со статусом WARNING. Для этого нужно изменить регулярное выражение анализирующее статус сервисов следующим образом:

^\s*<TD .*\s?class=['"](statusBGCRITICAL|statusBGWARNING)['"]\s?.*><a href=['"](\S+)['"].*>(.+)</a>

Т.е. добавить в первые круглые скобки оператор логического ИЛИ ("|") и строку statusBGWARNING. Получить соответствие имён классов (statusBGCRITICAL = CRITICAL, statusBGWARNING = WARNING и т.д.) вы можете просмотрев код HTML страницы проблемных сервисов.
Разумеется, что таким же образом можно настроить регулярное выражение для анализа статуса хостов.

Вкладка "Разное":

Конфигурация - разное
Мониторинг хостов - включение/выключение мониторинга хостов.
Показывать сообщения о смене статуса хостов - в случае снятия флажка программа не оповещает о смене статусов отслеживаемых хостов. При этом мониторинг хостов не отключается и можно просмотреть список упавших хостов щёлкнув в трее по иконке левой кнопкой мыши.
Мониторинг сервисов - включение/выключение мониторинга сервисов.
Показывать сообщения о смене статуса сервисов - в случае снятия флажка программа не оповещает о смене статусов отслеживаемых сервисов. При этом мониторинг сервисов не отключается и можно просмотреть список проблемных сервисов щёлкнув в трее по иконке средней кнопкой мыши.
Назначение остальных параметров на мой взгляд очевидно из их названий.

Часть общих для всех профилей настроек программы вынесена на вкладку "Общие":

Конфигурация - общие
В случае установки флажка "Избегать перекрытия всплывающих сообщений" программа располагает всплывающие сообщения последовательно, без перекрытия (см. скриншоты) - при появлении нескольких сообщений одновременно вам не нужно закрывать/передвигать последнее сообщение, что бы увидеть предыдущее.

Звуковые уведомления
Возможно вас не устроят звуковые уведомления встроенные в программу. В таком случае можете выбрать нужные звуки самостоятельно и поместить их в каталог программы заменив звуки по умолчанию. Для *nix систем это каталог:
~/.NetPatch/NagiosAgent/
для Windows: 
%USERDIR%\Application Data\NetPatch\NagiosAgent\
Названия файлов должны быть "HostDown.wav" - для сообщения о проблемных хостах/сервисах, "HostUp.wav" - для сообщения о поднявшихся хостах/сервисах, "trouble.wav" - для сообщения о возникновении проблем со связью с сервером Nagios либо проблем конфигурации. Более удобное конфигурирование звуковых уведомлений будет реализовано в будущей версии.
Гарантированно поддерживается только формат wav (PCM). Остальные форматы поддерживаются в зависимости от используемой ОС.
ВАЖНО: в системах использующих X11 (в частности - Linux, *BSD) для работы звука необходим пакет "Network Audio System". Обычно он называется nas и устанавливается одной командой при помощи стандартного менеджера пакетов.



Скриншоты

Всплывающие окна оповещений об изменении статуса сервисов (вариант оповещения по умолчанию):

 Всплывающие окна
При щелчке на имени объекта (хоста/сервиса) внешнему браузеру передаётся ссылка на страницу с информацией о состоянии этого объекта.

Если подобные всплывающие окна раздражают и мешают работать, то можно использовать оповещения через собщения трея (я сам использую именно этот режим). Падение хоста:

Host Down
Сообщение трея при поднятии хоста:

Host Up
Список упавших хостов и проблемных сервисов в виде единого меню (по умолчанию):

Hosts down and service problems
При желании можно настроить список хостов и список сервисов на разные кнопки меню (см. "Конфигурация -> Разное").
Меню-список упавших хостов - щелчёк левой кнопкой мыши (левая/средняя-колёсико кнопка мыши):

Host problems
Меню-список проблемных сервисов - щелчёк средней кнопкой мыши (средняя-колёсико/левая кнопка мыши):

Service problems
Все виды оповещений (включая мерцание иконки в трее) можно отключить через "Конфигурация -> Разное".

Upgrades

Планируемые улучшения функционала:

Отображение в уведомлениях статусной информации объекта.
Возможность группировки информации о нескольких проблемных объектах в одном всплывающем сообщении.
Возможность выбирать звуковые файлы для уведомлений непосредственно из окна конфигруации.
maintainer-wanted

К сожалению в на данный момент у меня практически нет свободного времени что бы заниматься сборкой и обслуживанием портов/пакетов для включения Nagios Agent в различные ОС. Потому - с благодарностью приму помощь в этом деле.
