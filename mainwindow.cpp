#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
QMainWindow(parent),
ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	m_pFtp = nullptr;
	m_pProgressDialog = nullptr;

	ui->fileList->setEnabled(false);
	ui->fileList->setRootIsDecorated(false);
	ui->fileList->setHeaderLabels(QStringList() << tr("Name") << tr("Size") << tr("Owner") << tr("Group") << tr("Time"));
	ui->fileList->header()->setStretchLastSection(false);
	ui->connectButton->setDefault(true);
	ui->cdToParentButton->setIcon(QPixmap(":/images/cdtoparent.png"));
	ui->cdToParentButton->setEnabled(false);
	ui->downloadButton->setEnabled(false);
	ui->buttonBox->addButton(ui->downloadButton, QDialogButtonBox::ActionRole);
	ui->buttonBox->addButton(ui->quitButton, QDialogButtonBox::RejectRole);

	ui->ftpServerLineEdit->setText("192.168.0.139");
	ui->edtUser->setText("patri");
	ui->edtPassword->setText("patori");

	connect(ui->fileList, SIGNAL(itemActivated(QTreeWidgetItem*,int)), this, SLOT(processItem(QTreeWidgetItem*, int)));
	connect(ui->fileList, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), this, SLOT(enableDownloadButton()));
	connect(m_pProgressDialog, SIGNAL(canceled()), this, SLOT(cancelDownload()));
	connect(ui->connectButton, SIGNAL(clicked()), this, SLOT(connectOrDisconnect()));
	connect(ui->cdToParentButton, SIGNAL(clicked()), this, SLOT(cdToParent()));
	connect(ui->downloadButton, SIGNAL(clicked()), this, SLOT(downloadFile()));
	connect(ui->quitButton, SIGNAL(clicked()), this, SLOT(close()));

	setWindowTitle(tr("FTP"));
}

MainWindow::~MainWindow()
{
	if(m_pProgressDialog != nullptr) {
		delete m_pProgressDialog;
	}

	delete ui;
}

void MainWindow::connectOrDisconnect()
{
	if (m_pFtp) {
		m_pFtp->abort();
		m_pFtp->deleteLater();
		m_pFtp = nullptr;

		ui->fileList->setEnabled(false);
		ui->cdToParentButton->setEnabled(false);
		ui->downloadButton->setEnabled(false);
		ui->connectButton->setEnabled(true);
		ui->connectButton->setText(tr("Connect"));
		setCursor(Qt::ArrowCursor);
		ui->statusLabel->setText(tr("Please enter the name of an FTP server."));
		return;
	}

	if (!m_pNetworkSession || !m_pNetworkSession->isOpen()) {
		if (m_Manager.capabilities() & QNetworkConfigurationManager::NetworkSessionRequired) {
			if (!m_pNetworkSession) {
				// Get saved network configuration
				QSettings settings(QSettings::UserScope, QLatin1String("Trolltech"));
				settings.beginGroup(QLatin1String("QtNetwork"));
				const QString id = settings.value(QLatin1String("DefaultNetworkConfiguration")).toString();
				settings.endGroup();

				// If the saved network configuration is not currently discovered use the system default
				QNetworkConfiguration config = m_Manager.configurationFromIdentifier(id);
				if ((config.state() & QNetworkConfiguration::Discovered) !=
				QNetworkConfiguration::Discovered) {
					config = m_Manager.defaultConfiguration();
				}

				m_pNetworkSession = new QNetworkSession(config, this);
				connect(m_pNetworkSession, SIGNAL(opened()), this, SLOT(connectToFtp()));
				connect(m_pNetworkSession, SIGNAL(error(Qm_pNetworkSession::SessionError)), this, SLOT(enableConnectButton()));
			}

			m_pNetworkSession->open();

			return;
		}
	}
	connectToFtp();
}

void MainWindow::connectToFtp()
{
	m_pFtp = new QFtp(this);

	connect(m_pFtp, SIGNAL(commandFinished(int,bool)), this, SLOT(ftpCommandFinished(int,bool)));
	connect(m_pFtp, SIGNAL(listInfo(QUrlInfo)), this, SLOT(addToList(QUrlInfo)));
	connect(m_pFtp, SIGNAL(dataTransferProgress(qint64,qint64)), this, SLOT(updateDataTransferProgress(qint64,qint64)));

	ui->fileList->clear();
	m_strCurrentPath.clear();
	m_bIsDirectory.clear();

	QUrl url(ui->ftpServerLineEdit->text());
	if (!url.isValid() || url.scheme().toLower() != QLatin1String("ftp")) {
		m_pFtp->setTransferMode(QFtp::Passive);
		m_pFtp->connectToHost(ui->ftpServerLineEdit->text(), 21);
		m_pFtp->login(ui->edtUser->text(), ui->edtPassword->text());
	} else {
		m_pFtp->connectToHost(url.host(), (quint16)url.port(21));

		if (!url.userName().isEmpty())
			m_pFtp->login(QUrl::fromPercentEncoding(url.userName().toLatin1()), url.password());
		else
			m_pFtp->login();
		if (!url.path().isEmpty())
			m_pFtp->cd(url.path());
	}

	ui->fileList->setEnabled(true);
	ui->connectButton->setEnabled(false);
	ui->connectButton->setText(tr("Disconnect"));
	ui->statusLabel->setText(tr("Connecting to FTP server %1...").arg(ui->ftpServerLineEdit->text()));
}

void MainWindow::downloadFile()
{
	QString fileName = ui->fileList->currentItem()->text(0);
	if (QFile::exists(fileName)) {
		QMessageBox::information(this, tr("FTP"), tr("There already exists a file called %1 in the current directory.").arg(fileName));
		return;
	}

	m_pFile = new QFile(fileName);
	if (!m_pFile->open(QIODevice::WriteOnly)) {
		QMessageBox::information(this, tr("FTP"),
		tr("Unable to save the file %1: %2.")
		.arg(fileName).arg(m_pFile->errorString()));
		delete m_pFile;
		return;
	}

	m_pFtp->get(ui->fileList->currentItem()->text(0), m_pFile);

	if(m_pProgressDialog == nullptr) {
		m_pProgressDialog = new QProgressDialog;
	}
	m_pProgressDialog->setLabelText(tr("Downloading %1...").arg(fileName));
	ui->downloadButton->setEnabled(false);
	m_pProgressDialog->exec();
}

void MainWindow::cancelDownload()
{
	if(m_pFtp != nullptr) {
		m_pFtp->abort();

		if (m_pFile->exists()) {
			m_pFile->close();
			m_pFile->remove();
		}
		delete m_pFtp;
	}
}

void MainWindow::ftpCommandFinished(int, bool error)
{
	setCursor(Qt::ArrowCursor);

	if (m_pFtp->currentCommand() == QFtp::ConnectToHost) {
		if (error) {
			QMessageBox::information(this, tr("FTP"),
			tr("Unable to connect to the FTP server "
			"at %1. Please check that the host "
			"name is correct.")
			.arg(ui->ftpServerLineEdit->text()));
			connectOrDisconnect();
			return;
		}
		ui->statusLabel->setText(tr("Logged onto %1.").arg(ui->ftpServerLineEdit->text()));
		ui->fileList->setFocus();
		ui->downloadButton->setDefault(true);
		ui->connectButton->setEnabled(true);
		return;
	}
	if (m_pFtp->currentCommand() == QFtp::Login)
		m_pFtp->list();

	if (m_pFtp->currentCommand() == QFtp::Get) {
		if (error) {
			ui->statusLabel->setText(tr("Canceled download of %1.").arg(m_pFile->fileName()));
			m_pFile->close();
			m_pFile->remove();
		} else {
			ui->statusLabel->setText(tr("Downloaded %1 to current directory.").arg(m_pFile->fileName()));
			m_pFile->close();
		}
		delete m_pFile;
		enableDownloadButton();
		m_pProgressDialog->hide();

	} else if (m_pFtp->currentCommand() == QFtp::List) {
		if (m_bIsDirectory.isEmpty()) {
			ui->fileList->addTopLevelItem(new QTreeWidgetItem(QStringList() << tr("<empty>")));
			ui->fileList->setEnabled(false);
		}
	}
}

void MainWindow::addToList(const QUrlInfo &urlInfo)
{
	QTreeWidgetItem *item = new QTreeWidgetItem;
	item->setText(0, urlInfo.name());
	item->setText(1, QString::number(urlInfo.size()));
	item->setText(2, urlInfo.owner());
	item->setText(3, urlInfo.group());
	item->setText(4, urlInfo.lastModified().toString("MMM dd yyyy"));

	QPixmap pixmap(urlInfo.isDir() ? ":/images/dir.png" : ":/images/file.png");
	item->setIcon(0, pixmap);

	m_bIsDirectory[urlInfo.name()] = urlInfo.isDir();
	ui->fileList->addTopLevelItem(item);
	if (!ui->fileList->currentItem()) {
		ui->fileList->setCurrentItem(ui->fileList->topLevelItem(0));
		ui->fileList->setEnabled(true);
	}
}
//![10]

//![11]
void MainWindow::processItem(QTreeWidgetItem *item, int /*column*/)
{
	QString name = item->text(0);
	if (m_bIsDirectory.value(name)) {
		ui->fileList->clear();
		m_bIsDirectory.clear();
		m_strCurrentPath += '/';
		m_strCurrentPath += name;
		m_pFtp->cd(name);
		m_pFtp->list();
		ui->cdToParentButton->setEnabled(true);
		setCursor(Qt::WaitCursor);
		return;
	}
}

void MainWindow::cdToParent()
{
	setCursor(Qt::WaitCursor);
	ui->fileList->clear();
	m_bIsDirectory.clear();
	m_strCurrentPath = m_strCurrentPath.left(m_strCurrentPath.lastIndexOf('/'));
	if (m_strCurrentPath.isEmpty()) {
		ui->cdToParentButton->setEnabled(false);
		m_pFtp->cd("/");
	} else {
		m_pFtp->cd(m_strCurrentPath);
	}
	m_pFtp->list();
}

void MainWindow::updateDataTransferProgress(qint64 readBytes, qint64 totalBytes)
{
	m_pProgressDialog->setMaximum(totalBytes);
	m_pProgressDialog->setValue(readBytes);
}

void MainWindow::enableDownloadButton()
{
	QTreeWidgetItem *current = ui->fileList->currentItem();
	if (current) {
		QString currentFile = current->text(0);
		ui->downloadButton->setEnabled(!m_bIsDirectory.value(currentFile));
	} else {
		ui->downloadButton->setEnabled(false);
	}
}

void MainWindow::enableConnectButton()
{
	// Save the used configuration
	QNetworkConfiguration config = m_pNetworkSession->configuration();
	QString id;
	if (config.type() == QNetworkConfiguration::UserChoice)
		id = m_pNetworkSession->sessionProperty(QLatin1String("UserChoiceConfiguration")).toString();
	else
		id = config.identifier();

	QSettings settings(QSettings::UserScope, QLatin1String("Trolltech"));
	settings.beginGroup(QLatin1String("QtNetwork"));
	settings.setValue(QLatin1String("DefaultNetworkConfiguration"), id);
	settings.endGroup();

	ui->connectButton->setEnabled(true);
	ui->statusLabel->setText(tr("Please enter the name of an FTP server."));
}
