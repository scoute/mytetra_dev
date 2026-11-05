#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QGroupBox>
#include <QRadioButton>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QApplication>
#include <QFileInfo>
#include <QDir>

#include "AppConfigPage_Search.h"
#include "models/appConfig/AppConfig.h"
#include "libraries/GlobalParameters.h"
#include "libraries/AppContext.h"
#include "libraries/FtsTextIndex.h"
#include "models/appConfig/AppConfigUi.h"

AppConfigPage_Search::AppConfigPage_Search(QWidget *parent) : ConfigPage(parent)
{
    setupUi();
    setupSignals();
    assembly();
}

AppConfigPage_Search::~AppConfigPage_Search()
{

}

void AppConfigPage_Search::setupUi()
{
    qDebug() << "Create search config page";

    // Поиск внутри заметки: строкой-полоской или отдельным окном.
    // По умолчанию полоска, окно выбирается откреплением или здесь
    findBarRadio=new QRadioButton(this);
    findBarRadio->setText(tr("Embedded bar in note"));

    findWindowRadio=new QRadioButton(this);
    findWindowRadio->setText(tr("Separate window"));

    if(AppContext::instance().appConfig()->uiSection()->get_editorFindEmbedded())
      findBarRadio->setChecked(true);
    else
      findWindowRadio->setChecked(true);

    // Ручное обслуживание FTS-индекса текстов. Тексты задаются
    // сразу здесь (а не только в retranslateUi): иначе до первой
    // смены языка виджеты пустые (см. 504)
    ftsIndexBox=new QGroupBox(this);
    ftsIndexBox->setTitle(tr("Text index (FTS5)"));

    ftsIndexSizeLabel=new QLabel(this);
    updateFtsSizeLabel();

    ftsVacuumButton=new QPushButton(this);
    ftsVacuumButton->setText(tr("Compact index file"));
    ftsVacuumButton->setToolTip(tr("Reclaim free space in the text index file (about 40 MB per 1000 notes). May take minutes on a large base."));
    ftsVacuumButton->setWhatsThis(tr("The text index is built on the first text search and lives next to conf.ini (mytetra-fts5.db, about 40 MB per 1000 notes). It grows on saves and shrinks logically on deletions, but the file itself only grows. Compacting (VACUUM) rewrites the file and returns free space to the disk."));
}

void AppConfigPage_Search::setupSignals()
{
    connect(ftsVacuumButton, &QPushButton::clicked,
            this,              &AppConfigPage_Search::onFtsVacuumClicked);
}

void AppConfigPage_Search::assembly()
{
    // Группировщик виджетов для поиска внутри заметки
    findInNoteBox=new QGroupBox(this);
    findInNoteBox->setTitle(tr("Find in note"));

    // Виджеты вставляются в группировщик поиска внутри заметки
    QVBoxLayout *findInNoteLayout = new QVBoxLayout;
    findInNoteLayout->addWidget(findBarRadio);
    findInNoteLayout->addWidget(findWindowRadio);
    findInNoteBox->setLayout(findInNoteLayout);

    // Группировщик виджетов для FTS-индекса: размер файла и кнопка уплотнения
    QVBoxLayout *ftsIndexLayout = new QVBoxLayout;
    ftsIndexLayout->addWidget(ftsIndexSizeLabel);
    ftsIndexLayout->addWidget(ftsVacuumButton);
    ftsIndexBox->setLayout(ftsIndexLayout);

    // Собирается основной слой
    QVBoxLayout *centralLayout=new QVBoxLayout();
    centralLayout->addWidget(findInNoteBox);
    centralLayout->addWidget(ftsIndexBox);
    centralLayout->addStretch();

    // Основной слой устанавливается
    setLayout(centralLayout);
}

void AppConfigPage_Search::retranslateUi(void)
{
  findBarRadio->setText(tr("Embedded bar in note"));
  findWindowRadio->setText(tr("Separate window"));
  findInNoteBox->setTitle(tr("Find in note"));

  ftsIndexBox->setTitle(tr("Text index (FTS5)"));
  updateFtsSizeLabel();
  ftsVacuumButton->setText(tr("Compact index file"));
  ftsVacuumButton->setToolTip(tr("Reclaim free space in the text index file (about 40 MB per 1000 notes). May take minutes on a large base."));
  ftsVacuumButton->setWhatsThis(tr("The text index is built on the first text search and lives next to conf.ini (mytetra-fts5.db, about 40 MB per 1000 notes). It grows on saves and shrinks logically on deletions, but the file itself only grows. Compacting (VACUUM) rewrites the file and returns free space to the disk."));
}

// Текст размера файла индекса: обновляется при показе и после уплотнения
void AppConfigPage_Search::updateFtsSizeLabel(void)
{
    const qint64 size=QFileInfo(FtsTextIndex::indexFilePath()).size();

    if(size<=0)
        ftsIndexSizeLabel->setText(tr("No index file yet. It is created on the first text search."));
    else
        ftsIndexSizeLabel->setText(tr("Index file: %1 MB").arg(QString::number(size/1048576.0, 'f', 1)));
}

// Ручное уплотнение файла индекса
void AppConfigPage_Search::onFtsVacuumClicked(void)
{
    const QString dbPath=FtsTextIndex::indexFilePath();
    const qint64 before=QFileInfo(dbPath).size();

    if(before<=0)
    {
        QMessageBox::information(this,
                                tr("Text index"),
                                tr("No index file yet. It is created on the first text search."));
        return;
    }

    if(QMessageBox::question(this,
                             tr("Text index"),
                             tr("Compact the text index file now? It may take minutes on a large base."),
                             QMessageBox::Yes | QMessageBox::No,
                             QMessageBox::No)!=QMessageBox::Yes)
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    QString error;
    const bool ok=AppContext::instance().ftsTextIndex()->vacuum(&error);

    QApplication::restoreOverrideCursor();

    if(!ok)
    {
        QMessageBox::warning(this,
                            tr("Text index"),
                            tr("Cannot compact index: %1").arg(error));
        return;
    }

    const qint64 after=QFileInfo(dbPath).size();

    updateFtsSizeLabel();

    QMessageBox::information(this,
                             tr("Text index"),
                             tr("Index compacted: %1 MB → %2 MB").arg(QString::number(before/1048576.0, 'f', 1),
                                                                      QString::number(after/1048576.0, 'f', 1)));
}

// Метод должен возвращать уровень сложности сделанных изменений
// 0 - изменения не требуют перезапуска программы
// 1 - изменения требуют перезапуска программы
int AppConfigPage_Search::applyChanges()
{
    qDebug() << "Apply changes search";

    int result=0;

    // Сохраняется выбранный вариант поиска в заметке: полоска или окно
    bool embedded=findBarRadio->isChecked();
    if(AppContext::instance().appConfig()->uiSection()->get_editorFindEmbedded()!=embedded)
      AppContext::instance().appConfig()->uiSection()->set_editorFindEmbedded(embedded);

    return result;
}
