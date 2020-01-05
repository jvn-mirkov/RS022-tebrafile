#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include <QTreeView>
#include <QHeaderView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    fileList = ui->treeWidget;
    initTreeWidget();
    currentPath.clear();

    manager = new QNetworkAccessManager(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initTreeWidget()
{
    fileList->setEnabled(false);
    fileList->setRootIsDecorated(true);
    fileList->header()->setStretchLastSection(true);

    fileList->setColumnCount(5);
    fileList->setHeaderLabels(QStringList() << "Name"
                                    << "Size"
                                    << "Owner"
                                    << "Group"
                                    << "Last modified");
    headerView = fileList->header();
    headerView->setSectionsClickable(true);
    headerView->resizeSection(0, 180);

    restartTreeWidget();
    QObject::connect(headerView, SIGNAL( sectionClicked(int) ), this, SLOT( on_header_cliked(int) ) );
    QObject::connect(fileList, &QTreeWidget::itemDoubleClicked, this, &MainWindow::cdToFolder);
}

void MainWindow::restartTreeWidget()
{
    fileList->clear();
    isDir.clear();

    QTreeWidgetItem *widgetItem = new QTreeWidgetItem();
    widgetItem->setText(0, "..");
    fileList->addTopLevelItem(widgetItem);



    // ctrl+click for multi-select
    fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    manager = new QNetworkAccessManager(this);

    widgetItem->setDisabled(true);

}

void MainWindow::on_header_cliked(int logicalIndex)
{
    if(logicalIndex == 0) {
        headerView->setSortIndicatorShown(true);
        fileList ->takeTopLevelItem(0);
        fileList ->sortItems(logicalIndex, headerView->sortIndicatorOrder());

        QTreeWidgetItem *widgetItem = new QTreeWidgetItem();
        widgetItem->setText(0, "..");
        fileList->insertTopLevelItem(0, widgetItem);
    } else {
        headerView->setSortIndicatorShown(false);
    }
}

void MainWindow::ftpDone(bool error)
{
    if (error) {
        std::cerr << "Error: " << qPrintable(ftpClient->errorString()) << std::endl;
        ftpClient->disconnect();
    }
}

void MainWindow::on_connectButton_clicked()
{
    ftpAdrress = ui->serverNameField->text();
    connectToServer();
}

void MainWindow::login(InputDialog* diag)
{
    QStringList credentials = InputDialog::getStrings(diag);
    username = credentials.at(0);
    password = credentials.at(1);
    ftpClient->login(username, password);
    QObject::connect(ftpClient, &QFtp::stateChanged, this, &MainWindow::afterLogin);
}

void MainWindow::connectToServer()
{
        ftpClient = new QFtp(this);
        url = QUrl(ftpAdrress);
        if (url.isValid()) {
            ftpClient->connectToHost(url.host(), url.port(21));
            QObject::connect(ftpClient, &QFtp::done, this, &MainWindow::ftpDone);
        } else
            qDebug() << "greska ";

       QObject::connect(ftpClient, &QFtp::stateChanged, this, &MainWindow::showLoginDialog);
}

void MainWindow::on_disconnectButton_clicked()
{
    ftpClient->close();
    ftpClient->abort();

    currentPath.clear();
    restartTreeWidget();
}

void MainWindow::showLoginDialog(int state)
{
    if (state == QFtp::Connected and ftpClient->error() == QFtp::NoError) {
        InputDialog* diag = new InputDialog(this,
                                            QString("username"),
                                            QString("password")
                                            );
        QObject::connect(diag, &InputDialog::credentialsCaptured, this, &MainWindow::login);
        diag->exec();
    } else if (state == QFtp::Unconnected and ftpClient->currentCommand() != QFtp::Close){
        QMessageBox errBox;
        errBox.setWindowTitle("Connection error");
        errBox.setText(ftpClient->errorString());
        errBox.setIcon(QMessageBox::Critical);
        errBox.exec();
        ftpClient->close();
        ftpClient->disconnect();
    }

}

void MainWindow::afterLogin(int state)
{
    currentPath = QString("~");
    if (state == QFtp::LoggedIn and ftpClient->currentCommand() == QFtp::Login) {
        listFiles(currentPath);
        logged = true;
    }
}

void MainWindow::listFiles(const QString& fileName)
{
    QObject::connect(ftpClient, &QFtp::listInfo, this, &MainWindow::addToList);
    QObject::connect(ftpClient, &QFtp::done, this, &MainWindow::listDone);
    ftpClient->list(fileName);
}

void MainWindow::addToList(const QUrlInfo& file)
{
    QTreeWidgetItem *widgetItem = new QTreeWidgetItem();

    widgetItem->setText(0, file.name());
    widgetItem->setText(1, QString::number(file.size()));
    widgetItem->setText(2, file.owner());
    widgetItem->setText(3, file.group());
    widgetItem->setText(4, file.lastModified().toString("dd.MM.yyyy"));

    QIcon* folderIcon = new QIcon("../icons/directory.png");
    QIcon* fileIcon = new QIcon("../icons/file.png");
    QIcon* icon(file.isDir() ? folderIcon : fileIcon);
    widgetItem->setIcon(0, *icon);

    isDir.insert(file.name(), file.isDir());

    fileList->addTopLevelItem(widgetItem);

    // ako je item prvi postavi ga za trenutno selektovani
    if (!fileList->currentItem()) {
        fileList->setCurrentItem(fileList->topLevelItem(1));
        fileList->setEnabled(true);
    }
}

void MainWindow::cdToFolder(QTreeWidgetItem *widgetItem, int column)
{
    // ako je korisnik izabrao da ide nazad
    if(widgetItem == fileList->topLevelItem(0)) {
        leaveFolder();
    } else {
        QString name = widgetItem->text(0);
        if(isDir.value(name)) {
            currentPath += '/';
            currentPath += name;

            restartTreeWidget();

            ftpClient->cd(name);
            ftpClient->list();
        }
    }
    headerView->setSortIndicatorShown(false);
}

void MainWindow::leaveFolder()
{
      restartTreeWidget();
      currentPath = currentPath.left(currentPath.lastIndexOf('/'));
      if(currentPath.isEmpty()) {
          currentPath = "~";
          ftpClient->cd("~");
      } else {
          ftpClient->cd(currentPath);
      }
      ftpClient->list();
      headerView->setSortIndicatorShown(false);
}

void MainWindow::listDone(bool error)
{
    if (error) {
        std::cerr << "Error: " << qPrintable(ftpClient->errorString()) << std::endl;
    }
}



void MainWindow::on_openButton_clicked()
{
    const auto filenames = QFileDialog::getOpenFileNames(
                this,
                "Select files",
                QDir::homePath());
    ui->uploadFileInput->setText(filenames.join(';'));
}

void MainWindow::on_uploadButton_clicked()
{

    if (logged) {
        ftpClient->rawCommand("PWD");
        QObject::connect(ftpClient, &QFtp::rawCommandReply,
                         this, &MainWindow::pwdHandler);
    }
    if (ui->uploadFileInput->text().trimmed().length() == 0)
        QMessageBox::critical(this, "Alert", "Files did not selected.");
    else if (!logged)
        QMessageBox::critical(this, "Alert", "You are not connected.");
    else {
        const auto listOfFiles = ui->uploadFileInput->text().split(";");
        foreach (const auto file, listOfFiles) {
            const auto namesParts = file.split("/");
            QFile readFile(file);
            readFile.open(QIODevice::ReadOnly);
            const QByteArray buffer = readFile.readAll();
            int putID = ftpClient->put(buffer, namesParts.last(), QFtp::Binary);
            fileList->setEnabled(false);
            QObject::connect(ftpClient, &QFtp::dataTransferProgress,
                             this, &MainWindow::progressBarSlot);
            QObject::connect(ftpClient, &QFtp::commandFinished,
                             this, &MainWindow::uploadFinishHandler);

        }
    }
}



void MainWindow::on_downloadButton_clicked()
{

    if (ui->downloadFileInput->text().trimmed().length() == 0)
        QMessageBox::critical(this, "Alert", "Files did not selected.");
    else
    {

        fileList->setEnabled(false);
        ui->downloadButton->setEnabled(false);
        const auto downloadList = ui->downloadFileInput->text().split(";");


        //QString fileName = fileList->currentItem()->text(0);

        QString downloadsFolder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);


        for(auto fileName : downloadList)
        {
            file = new QFile(downloadsFolder + "/" + fileName);

            if (!file->open(QIODevice::WriteOnly)) {
             QMessageBox::information(this, tr("FTP"),
                                      tr("Unable to save the file %1: %2.")
                                      .arg(fileName).arg(file->errorString()));
             delete file;
             return;
            }

            ftpClient->get(fileName, file);
            QObject::connect(ftpClient, &QFtp::dataTransferProgress,
                             this, &MainWindow::downloadProgressBarSlot);



        }



    }
}

void MainWindow::downloadProgressBarSlot(qint64 done, qint64 total)
{
    ui->downloadProgressBar->setValue(100*done/total);
    if(done == total){
        fileList->setEnabled(true);
        ui->downloadButton->setEnabled(true);
    }
}


void MainWindow::on_treeWidget_clicked()
{
    const auto filenames = fileList->selectedItems();
    QStringList filenamesQ;

    //ruzno ali radi
    // fix sa refactorisanjem i std::transform
    QString temp;
    for(auto filename : filenames)
    {
        temp = filename->text(0);
        filenamesQ.push_back(temp);
    }




    ui->downloadFileInput->setText(filenamesQ.join(';'));

}

void MainWindow::progressBarSlot(qint64 done, qint64 total)
{
    ui->uploadProgressBar->setValue(100*done/total);
    if(done == total)
        fileList->setEnabled(true);
}

void MainWindow::uploadFinishHandler(int id, bool error)
{
    if (error) {
        fileList->setEnabled(true);
        std::cout << ftpClient->errorString().toUtf8().data() << std::endl;
        ftpClient->rawCommand("PWD");

    }
}

void MainWindow::pwdHandler(int replyCode, const QString& detail)
{
    qDebug() << "-----------------------";
    qDebug() << detail;
    qDebug() << "-----------------------";

}

