#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QSettings>
#include <QUrl>
#include <QFile>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTreeWidgetItem>
#include "qftp.h"

#include <QNetworkSession>
#include <QNetworkConfigurationManager>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();

private slots:
	void connectOrDisconnect();
	void downloadFile();
	void cancelDownload();
	void connectToFtp();

	void ftpCommandFinished(int commandId, bool error);
	void addToList(const QUrlInfo &urlInfo);
	void processItem(QTreeWidgetItem *item, int column);
	void cdToParent();
	void updateDataTransferProgress(qint64 readBytes,
	qint64 totalBytes);
	void enableDownloadButton();
	void enableConnectButton();

private:
	QFtp		*m_pFtp;
	QNetworkSession *m_pNetworkSession;
	QNetworkConfigurationManager m_Manager;

	QHash<QString, bool> m_bIsDirectory;
	QString m_strCurrentPath;
	QFile *m_pFile;
	QProgressDialog  *m_pProgressDialog;
private:	
	Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
