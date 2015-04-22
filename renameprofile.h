#ifndef RENAMEPROFILE_H
#define RENAMEPROFILE_H

#include <QtGui/QWidget>
#include <QMessageBox>
#include <QCloseEvent>
#include "ui_renameprofile.h"

class RenameProfile : public QWidget
{
    Q_OBJECT

public:
    RenameProfile(QWidget *parent = 0);
    ~RenameProfile();

Q_SIGNALS:
	void	emitNewName(const QString newName);
private Q_SLOTS:
	void	acceptNewName();
	void	rejectNewName();
private:
	void 	closeEvent(QCloseEvent * event);

    Ui::RenameProfileClass ui;
};

#endif // RENAMEPROFILE_H
