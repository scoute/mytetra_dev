#include <QAbstractButton>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSplitter>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

#include "TextDiffDialog.h"


TextDiffDialog::TextDiffDialog(const QString &recordTitle,
                               const QString &diffHtml,
                               QWidget *parent,
                               const QString &oldHtml,
                               const QString &newHtml,
                               const QPair<QString, QString> &baseDirs)
    : QDialog(parent)
{
    setModal(true);
    resize(1020, 700);
    setWindowTitle(tr("Diff: %1").arg(recordTitle));

    QLabel *infoLabel=new QLabel(this);
    infoLabel->setWordWrap(true);
    infoLabel->setText(tr("<b>%1</b> — сравнение текста.<br>"
                          "Красное — было (локальная копия), "
                          "зелёное — стало (версия владельца).")
                       .arg(recordTitle.toHtmlEscaped()));

    // Тройной вид: слева построчный diff, справа — рендер «Было / Стало»
    QLabel *diffLabel=new QLabel(tr("Построчный diff"), this);

    QTextEdit *diffView=new QTextEdit(this);
    diffView->setObjectName("textDiffView");
    diffView->setReadOnly(true);
    diffView->setHtml(diffHtml);

    QVBoxLayout *diffLayout=new QVBoxLayout();
    diffLayout->addWidget(diffLabel);
    diffLayout->addWidget(diffView, 1);
    QWidget *diffPage=new QWidget(this);
    diffPage->setLayout(diffLayout);

    QTextEdit *oldView=new QTextEdit(this);
    oldView->setObjectName("textDiffOld");
    oldView->setReadOnly(true);
    QTextEdit *newView=new QTextEdit(this);
    newView->setObjectName("textDiffNew");
    newView->setReadOnly(true);

    if(!baseDirs.first.isEmpty())
        oldView->document()->setBaseUrl(QUrl::fromLocalFile(baseDirs.first+"/"));
    if(!baseDirs.second.isEmpty())
        newView->document()->setBaseUrl(QUrl::fromLocalFile(baseDirs.second+"/"));

    oldView->setHtml(oldHtml.isEmpty()
                     ? tr("<p><i>Нет локальной копии для сравнения.</i></p>")
                     : oldHtml);
    newView->setHtml(newHtml.isEmpty()
                     ? tr("<p><i>Текст версии владельца недоступен.</i></p>")
                     : newHtml);

    QLabel *oldLabel=new QLabel(tr("Было (локальная копия)"), this);
    QLabel *newLabel=new QLabel(tr("Стало (версия владельца)"), this);

    QVBoxLayout *oldLayout=new QVBoxLayout();
    oldLayout->addWidget(oldLabel);
    oldLayout->addWidget(oldView, 1);
    QWidget *oldPage=new QWidget(this);
    oldPage->setLayout(oldLayout);

    QVBoxLayout *newLayout=new QVBoxLayout();
    newLayout->addWidget(newLabel);
    newLayout->addWidget(newView, 1);
    QWidget *newPage=new QWidget(this);
    newPage->setLayout(newLayout);

    QSplitter *renderSplitter=new QSplitter(Qt::Horizontal, this);
    renderSplitter->addWidget(oldPage);
    renderSplitter->addWidget(newPage);
    renderSplitter->setStretchFactor(0, 1);
    renderSplitter->setStretchFactor(1, 1);

    QSplitter *mainSplitter=new QSplitter(Qt::Horizontal, this);
    mainSplitter->addWidget(diffPage);
    mainSplitter->addWidget(renderSplitter);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 2);
    this->mainSplitter=mainSplitter;

    // Вид компоновки: diff и рендер в одном ряду (по умолчанию)
    // либо diff сверху, рендеры снизу
    viewModeSelector=new QComboBox(this);
    viewModeSelector->setObjectName("diffViewMode");
    viewModeSelector->addItem(tr("diff и рендер в одном ряду"));
    viewModeSelector->addItem(tr("diff сверху рендера"));
    connect(viewModeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,             &TextDiffDialog::onViewModeChanged);

    QHBoxLayout *topLayout=new QHBoxLayout();
    topLayout->addWidget(viewModeSelector);
    topLayout->addStretch(1);

    QDialogButtonBox *buttonBox=new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::clicked, this, &TextDiffDialog::reject);

    QVBoxLayout *layout=new QVBoxLayout(this);
    layout->addWidget(infoLabel);
    layout->addLayout(topLayout);
    layout->addWidget(mainSplitter, 1);
    layout->addWidget(buttonBox);
}


void TextDiffDialog::onViewModeChanged(int index)
{
    if(!mainSplitter)
        return;

    if(index==0)
    {
        // Diff и рендер в одном ряду
        mainSplitter->setOrientation(Qt::Horizontal);
        mainSplitter->setStretchFactor(0, 1);
        mainSplitter->setStretchFactor(1, 2);
    }
    else
    {
        // Diff сверху, рендеры снизу
        mainSplitter->setOrientation(Qt::Vertical);
        mainSplitter->setStretchFactor(0, 1);
        mainSplitter->setStretchFactor(1, 2);
    }
}
