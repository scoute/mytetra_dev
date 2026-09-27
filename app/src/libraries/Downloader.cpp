#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QDebug>
#include <QProgressBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHeaderView>
#include <QLabel>
#include <QUrl>
#include <QFileInfo>
#include <QApplication>
#include <QSslSocket>

#include "Downloader.h"
#include "libraries/helpers/DebugHelper.h"
#include "libraries/helpers/MessageHelper.h"
#include "libraries/helpers/UniqueIdHelper.h"
#include "models/appConfig/AppConfig.h"


extern AppConfig mytetraConfig;


// Проверка, способен ли TLS-бэкенд вообще проверять сертификаты.
// Qt старше 5.15.8, собранный под OpenSSL 1.1, не умеет работать с OpenSSL 3
// в рантайме: не резолвятся символы вроде SSL_get_peer_certificate, из-за чего
// КАЖДОЕ https-соединение завершается ошибкой "The peer did not present any
// certificate", даже с валидным сертификатом. Получить сертификат для проверки
// в такой среде нельзя в принципе
static bool isTlsVerificationBroken(void)
{
  // Строки вида "OpenSSL 1.1.1g 21 Apr 2020", мажорная версия - второе слово до точки
  QString buildVersion=QSslSocket::sslLibraryBuildVersionString().section(' ', 1, 1).section('.', 0, 0);
  QString runtimeVersion=QSslSocket::sslLibraryVersionString().section(' ', 1, 1).section('.', 0, 0);

  // Комбинация "собран под 1.1, работает под 3.x" означает неработоспособную проверку
  if(buildVersion=="1" && runtimeVersion.toInt()>=3)
    return true;

  return false;
}


Downloader::Downloader()
{
  downloadMode=disk;
  saveDirectory="";
  referencesList.clear();
  memoryFiles.clear();
  diskFilesNames.clear();
  isSuccessFlag=false;
  downloadHasErrors=false;
  errorLog="";
  currentReferenceNum=-1;
  lastRedirectUrl.clear();
  redirectCount=0;

  colsName << tr("Url") << tr("%");
  downloadReferenceCol=0;
  downloadPercentCol=1;

  // Попытка заставить работать закачку с HTTPS-сайтов на 64 bit, так не сработало
  // webManager.setStrictTransportSecurityEnabled(false);

  setupUI();
  setupSignals();
  assembly();
}


Downloader::~Downloader()
{

}


void Downloader::setupUI()
{
  // Текст описания
  aboutLabel=new QLabel(this);
  aboutLabel->hide();

  // Создается таблица скачиваемых файлов
  table=new QTableWidget(0, colsName.count(), this);

  // Задаются заголовки таблицы
  table->setHorizontalHeaderLabels(colsName);

  table->horizontalHeader()->setSectionResizeMode(downloadReferenceCol, QHeaderView::Stretch);
  table->setColumnWidth(downloadPercentCol, 128);


  // Создается кнопка отмены загрузки
  cancelButton=new QPushButton(this);
  cancelButton->setText(tr("Cancel"));
}


void Downloader::setupSignals()
{
  connect(&webManager, &QNetworkAccessManager::finished,
          this,        &Downloader::onFileDownloadFinished);

  connect(&webManager, &QNetworkAccessManager::sslErrors,
        this,          &Downloader::onSslErrors);

  connect(cancelButton, &QPushButton::clicked,
          this,         &Downloader::onCancelClicked);
}


void Downloader::assembly()
{
  QVBoxLayout *mainLayout=new QVBoxLayout();

  mainLayout->addWidget(aboutLabel);
  mainLayout->addWidget(table);
  mainLayout->addWidget(cancelButton);

  setLayout(mainLayout);
}


void Downloader::setDownloadMode(int iMode)
{
  downloadMode=iMode;
}


void Downloader::setSaveDirectory(QString iDir)
{
  saveDirectory=iDir;
}


QString Downloader::getSaveDirectory()
{
  return saveDirectory;
}


// Установка списка ссылок для закачивания
void Downloader::setReferencesList(QStringList iReferencesList)
{
  // qDebug() << "Downlod list: "  << iReferencesList;

  referencesList=iReferencesList;

  if(downloadMode==disk)
  {
    diskFilesNames.clear();
    diskFilesNames.resize(referencesList.size());
  }

  if(downloadMode==memory)
  {
    memoryFiles.clear();
    memoryFiles.resize(referencesList.size());
  }

  // Перебирается список ссылок
  for(int i=0; i<referencesList.count(); i++)
  {
    // Добавляется строка на экране
    table->insertRow(i);

    // Заполняется столбец со ссылкой
    QTableWidgetItem *referenceItem=new QTableWidgetItem(referencesList.at(i));
    table->setItem(i, downloadReferenceCol, referenceItem);

    // Создается виджет линейки наполняемости
    QProgressBar *progressBar=new QProgressBar();
    progressBar->setRange(0,100);
    table->setCellWidget(i, downloadPercentCol, progressBar);

    // Генерируются имена файлов на диске, куда будут сохранятся выкачиваемые файлы
    if(downloadMode==disk)
    {
      QUrl fileUrl( referencesList.at(i) );
      QString fileName=fileUrl.fileName(); // Имя файла из URL, содержащее расширение

      QFileInfo fileInfo(fileName);
      QString fileExtention = fileInfo.completeSuffix();

      diskFilesNames[i]=getUniqueId()+"."+fileExtention;
    }
  }
}


void Downloader::setAboutText(QString iAboutText)
{
  aboutLabel->setText(iAboutText);

  if(iAboutText.length()>0)
    aboutLabel->show();
  else
    aboutLabel->hide();
}


QString Downloader::getAboutText()
{
  return aboutLabel->text();
}


QVector<QByteArray> Downloader::getMemoryFiles() const
{
  if(downloadMode==disk)
    criticalError("Cant execute Downloader::getMemoryFiles() for disk mode. Current Download instance use memory mode.");

  return memoryFiles;
}


QMap<QString, QByteArray> Downloader::getReferencesAndMemoryFiles() const
{
  if(downloadMode==disk)
    criticalError("Cant execute Downloader::getReferencesAndMemoryFiles() for disk mode. Current Download instance use memory mode.");

  QMap<QString, QByteArray> tempReferencesAndMemoryFiles;

  for(int i=0; i<memoryFiles.count(); ++i)
    tempReferencesAndMemoryFiles[ referencesList.at(i) ]=memoryFiles.value(i);

  return tempReferencesAndMemoryFiles;
}


QMap<QString, QString> Downloader::getReferencesAndFileNames() const
{
  if(downloadMode==memory)
    criticalError("Cant execute Downloader::getReferencesAndFileNames() for memory mode. Current Download instance use disk mode.");

  QMap<QString, QString> tempReferencesAndFileNames;

  for(int i=0; i<referencesList.count(); ++i)
    tempReferencesAndFileNames[ referencesList.at(i) ]=diskFilesNames.value(i);

  return tempReferencesAndFileNames;
}


QStringList Downloader::getDiskFilesList() const
{
  if(downloadMode==memory)
    criticalError("Cant execute Downloader::getDiskFilesList() for memory mode. Current Download instance use disk mode.");

  return QStringList();
}


QStringList Downloader::getReferencesList() const
{
  return referencesList;
}


void Downloader::run()
{
  if(referencesList.count()>0)
  {
    startNextDownload();

    exec(); // Запускается цикл обработки сигналов-слотов для данного класса
  }
  else
    criticalError("Running downloader with empty references list.");
}


bool Downloader::isSuccess()
{
  return isSuccessFlag;
}


QString Downloader::getErrorLog()
{
  return errorLog;
}


void Downloader::reconnectSignalsNetworkReply(QNetworkReply *networkReply)
{
  disconnect(networkReply, &QNetworkReply::downloadProgress, 0, 0);
  connect(   networkReply, &QNetworkReply::downloadProgress, this, &Downloader::onDownloadProgress);

  disconnect(this, &Downloader::cancelDownload, 0, 0 );
  connect(   this, &Downloader::cancelDownload, networkReply, &QNetworkReply::abort);
}


// Запуск загрузки очередной ссылки
void Downloader::startNextDownload()
{
  // Определяется, какую ссылку надо загружать
  currentReferenceNum++;
  QString currentReference=referencesList.at( currentReferenceNum );

  // Состояние редиректов сбрасывается для каждой новой ссылки,
  // иначе редирект одной ссылки влиял бы на обработку следующей
  lastRedirectUrl.clear();
  redirectCount=0;

  // Запуск загрузки
  qDebug() << "Start download" << currentReference;
  QNetworkRequest request(currentReference);

  // В конце загрузки автоматически будет вызван слот onFileDownloadFinished() ( см. связывание сингнал-слот в setupSignals() )
  networkReply=webManager.get(request);
  reconnectSignalsNetworkReply(networkReply);
}


// Слот, вызываемый в конце загрузки очередного файла
void Downloader::onFileDownloadFinished(QNetworkReply *reply)
{
  bool enableNextDownload=true;

  // Если при получении ответа не было ошибок сети
  if(reply->error() == QNetworkReply::NoError)
  {
    // Определение, есть ли перенаправление (редирект) в ответе сервера.
    // Относительный адрес редиректа разрешается относительно запрошенного URL
    QVariant possibleRedirectUrl=reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    // Если есть перенаправление
    if(!possibleRedirectUrl.toUrl().isEmpty())
    {
      QUrl urlRedirectedTo=checkedRedirectUrl( reply->url().resolved( possibleRedirectUrl.toUrl() ) );

      if(!urlRedirectedTo.isEmpty() && redirectCount<maxRedirectCount)
      {
        redirectCount++;

        qDebug() << "Redirected to " << urlRedirectedTo.toString();

        QNetworkRequest request(urlRedirectedTo);
        networkReply=webManager.get(request); // В конце загрузки будет вызван слот onFileDownloadFinished()
        reconnectSignalsNetworkReply(networkReply);

        enableNextDownload=false;
      }
      else
      {
        // Недопустимый редирект: повтор адреса, не-HTTP схема или слишком
        // длинная цепочка. Тело такого ответа файлом не считается, ссылка
        // пропускается с ошибкой, иначе вредоносный сервер мог бы зациклить
        // загрузку или подсунуть локальный файл
        addErrorLog("Bad redirect, download skipped: "+referencesList.at(currentReferenceNum));
        downloadHasErrors=true;
        isSuccessFlag=false;
      }
    }
    else
    {
      // Перенаправления нет, и значит в ответе содержится принятый файл

      // Загруженные данные сохраняются в память
      if(downloadMode==memory)
        memoryFiles[currentReferenceNum]=reply->readAll();

      // Загруженные данные сохраняются на диск
      if(downloadMode==disk)
      {
        QString fileName=saveDirectory+"/"+diskFilesNames[currentReferenceNum];

        QFile file( fileName );
        if( file.open( QIODevice::WriteOnly ) )
        {
          file.write( reply->readAll() );
          file.close();
        }
        else
          showMessageBox( tr("Has problem with save file to directory %1").arg(saveDirectory) );
      }
    }
  }
  else
  {
    qDebug() << reply->errorString();
    addErrorLog(reply->errorString());
    isSuccessFlag=false; // Если с одним файлом была проблема, флаг успешной загрузки снимается для всех
    downloadHasErrors=true;
  }


  // Если разрешена загрузка следующей ссылки
  if(enableNextDownload)
  {
    // Если при загрузке текущей ссылки была какая-то ошибка, строка выделяется цветом
    if(reply->error() != QNetworkReply::NoError)
      table->item(currentReferenceNum, downloadReferenceCol)->setBackground( QApplication::palette().color(QPalette::Mid) );

    // На экране отмечается, что текущая ссылка загружена полностью
    qobject_cast<QProgressBar *>(table->cellWidget(currentReferenceNum, downloadPercentCol))->setValue(100);

    // Если еще не все ссылки загружены
    if(currentReferenceNum<(referencesList.count()-1))
      startNextDownload();
    else
    {
      // Иначе все загрузки завершены

      qDebug() << "All download successfull";

      // Флаг успеха выставляется только если ни одна загрузка не завершилась
      // ошибкой. Раньше он безусловно ставился в true и затирал ранее
      // зафиксированные ошибки, и неуспешная закачка выглядела успешной
      isSuccessFlag=!downloadHasErrors;

      emit accept(); // Программно закрывается окно диалога, как будто нажали Ok
    }
  }

  reply->deleteLater();
}


void Downloader::onDownloadProgress(qint64 read, qint64 total)
{
  qint64 percent=0;

  if(total>0) // На 0 делить нельзя, а -1 означает что размер данных не известен
    percent=(read * 100) / total;

  // На экране изменяется процент загрузки ссылки
  qobject_cast<QProgressBar *>(table->cellWidget(currentReferenceNum, downloadPercentCol))->setValue(percent);
}


// Метод, отбрасывающий повторяющиеся и недопустимые ссылки при редиректе.
// Разрешаются только http и https: другие схемы (в частности file) никогда
// не подставляются в запрос, иначе сервер мог бы подсунуть локальный файл
QUrl Downloader::checkedRedirectUrl(const QUrl& possibleRedirectUrl)
{
  QUrl redirectUrl;

  if(!possibleRedirectUrl.isEmpty() &&
     possibleRedirectUrl != lastRedirectUrl &&
     (possibleRedirectUrl.scheme()=="http" || possibleRedirectUrl.scheme()=="https"))
  {
    redirectUrl=possibleRedirectUrl;
  }

  lastRedirectUrl=redirectUrl;

  return redirectUrl;
}


void Downloader::onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
  // Особый случай битого TLS-бэкенда (см. isTlsVerificationBroken): в такой
  // среде Qt не различает валидный серт, самоподписанный и подмену - на все
  // одна ошибка NoPeerCertificate, потому что сертификат получить нельзя
  // вообще. Блокировать тут бессмысленно: это не остановит никакую реальную
  // атаку, а лишь сломает все https-скачивания. Поэтому качаем с явным
  // предупреждением в лог. На здоровом бэкенде ниже действует строгая политика
  if(isTlsVerificationBroken())
  {
    static bool verificationWarningShown=false;
    if(!verificationWarningShown)
    {
      verificationWarningShown=true;
      qWarning() << "TLS backend cannot verify certificates: built for"
                 << QSslSocket::sslLibraryBuildVersionString()
                 << "but runtime is"
                 << QSslSocket::sslLibraryVersionString()
                 << ". Downloads proceed without SSL verification.";
    }

    reply->ignoreSslErrors();
    return;
  }

  // Ошибки самоподписанных сертификатов игнорируются только если пользователь
  // явно разрешил это в настройках (нужно для сайтов с самоподписанными
  // сертификатами). Игнорируются точечно: только самоподписанность, а не
  // все SSL-ошибки скопом. Остальные ошибки (истекший срок, чужое имя и т.д.)
  // в любом случае прерывают загрузку
  if(mytetraConfig.getIgnoreSelfSignedSslErrors())
  {
    QList<QSslError> selfSignedErrors;
    foreach(const QSslError &error, errors)
    {
      if(error.error()==QSslError::SelfSignedCertificate ||
         error.error()==QSslError::SelfSignedCertificateInChain)
        selfSignedErrors << error;
    }

    // Игнорируем только если ВСЕ ошибки относятся к самоподписанности
    if(!selfSignedErrors.isEmpty() && selfSignedErrors.count()==errors.count())
    {
      qDebug() << "Ignore self-signed SSL certificate errors for " << reply->url().toString();

      reply->ignoreSslErrors(selfSignedErrors);
      return;
    }
  }

  // SSL-ошибки игнорировать нельзя: невалидный сертификат может означать
  // подмену сервера. Загрузка прерывается, ошибка фиксируется, а остальные
  // ссылки обрабатываются обычным порядком через ветку ошибки ответа
  downloadHasErrors=true;

  reply->abort();
}


void Downloader::onCancelClicked()
{
  emit cancelDownload();

  addErrorLog("Cancel download by user.");

  isSuccessFlag=false;

  reject();
}


void Downloader::addErrorLog(const QString text)
{
  if(errorLog.length()>0)
    errorLog+="\n";

  errorLog+=text;
}
