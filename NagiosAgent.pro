TEMPLATE = app
TARGET = NagiosAgent
QT += core \
    gui \
    network
HEADERS += renameprofile.h \ 
	about.h \
    miscclasses.h \
    informationwindow.h \
    nagiosagent.h \
    httpupdater.h \
    serversession.h 
SOURCES += renameprofile.cpp \
	about.cpp \
    miscclasses.cpp \
    informationwindow.cpp \
    main.cpp \
    nagiosagent.cpp \
	httpupdater.cpp \
	serversession.cpp
FORMS += renameprofile.ui \
	about.ui \
    informationwindow.ui \
    nagiosagent.ui
RESOURCES += resources/nagiosagent.qrc
RC_FILE = resources/appico.rc
TRANSLATIONS = resources/NagiosAgentLang_ru.ts
#CONFIG += console
CODECFORTR = 
