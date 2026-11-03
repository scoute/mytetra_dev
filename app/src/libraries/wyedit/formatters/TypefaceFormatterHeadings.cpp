#include <QTextBlock>
#include <QTextBlockUserData>
#include <QTextDocument>
#include <QTextLayout>
#include <QFont>
#include <QRegularExpression>
#include <QFile>
#include <QMap>
#include <QClipboard>
#include <QToolTip>
#include <QDebug>
#include <QColorDialog>
#include <QColor>
#include <QDomNode>
#include <QUrl>
#include <QTextTable>
#include <QApplication>

#include <QCXXHighlighter>
#include <QBashHighlighter>
#include <QCLikeHighlighter>
#include <QCssHighlighter>
#include <QGLSLHighlighter>
#include <QIniHighlighter>
#include <QPhpHighlighter>
#include <QSqlHighlighter>
#include <QJSONHighlighter>
#include <QLuaHighlighter>
#include <QPythonHighlighter>
#include <QStyleSyntaxHighlighter>
#include <QSyntaxStyle>
#include <QXMLHighlighter>


#include "../Editor.h"
#include "../EditorConfig.h"
#include "../EditorTextArea.h"


// Масштабный коэффициент размера заголовка: во сколько раз h1/h2/h3
// крупнее обычного текста. Qt хранит крупность в свойстве
// FontSizeAdjustment, а оно не откатывается undo и плодит мусор,
// поэтому в работу берутся абсолютные размеры. Коэффициент замеряется
// один раз за сессию рендером эталона: допущений о внутренностях Qt нет

// P3: хелперы заголовков (были file-static в общем файле)
// Запомненное оформление абзаца до первого превращения в заголовок:
// размер, жирность и отступы. Нужно для точного отката: снятие уровня
// возвращает свое, а не дефолт. Запоминается один раз: H1->H2 хранит
// исходное обычное оформление. Только стандартные свойства: все
// откатывается штатным undo и не оставляет мусора значений
struct HeadingStash : public QTextBlockUserData
{
    int stashedLevel=0;
    double pointSize=0.0;
    int weight=QFont::Normal;
    QVariant sizeAdjustment;
    double topMargin=0.0;
    double bottomMargin=0.0;
    bool hasData=false;
};


// Масштаб кегля заголовков относительно базового: фиксированная
// лесенка 48/32/24/20/16/12 при базе 12 (факторы 4, 8/3, 2, 5/3,
// 4/3, 1). Замер рендера Qt тут не годится: Qt рисует h6 кеглем
// обычного текста, а h5 - small, выходила инверсия H6 крупнее H5
static double headingScaleFactor(int level)
{
    static const double factors[7]={1.0, 4.0, 8.0/3.0, 2.0, 5.0/3.0, 4.0/3.0, 1.0};

    if(level<0 || level>6)
        return 1.0;

    return factors[level];
}


// Эталон блочных отступов уровня: снимается с временного документа


// Эталон блочных отступов уровня: снимается с временного документа
static void headingProbeMargins(int level, QTextBlockFormat &blockFormat)
{
    QTextDocument probe;
    if(level==1)
        probe.setHtml(QStringLiteral("<h1>x</h1>"));
    else if(level==2)
        probe.setHtml(QStringLiteral("<h2>x</h2>"));
    else if(level==3)
        probe.setHtml(QStringLiteral("<h3>x</h3>"));
    else if(level==4)
        probe.setHtml(QStringLiteral("<h4>x</h4>"));
    else if(level==5)
        probe.setHtml(QStringLiteral("<h5>x</h5>"));
    else if(level==6)
        probe.setHtml(QStringLiteral("<h6>x</h6>"));
    else
        probe.setHtml(QStringLiteral("<p>x</p>"));

    QTextBlock block=probe.begin();
    if(block.isValid())
    {
        blockFormat.setTopMargin(block.blockFormat().topMargin());
        blockFormat.setBottomMargin(block.blockFormat().bottomMargin());
    }
}


// Установка уровня заголовка для абзацев в выделении (или текущего
// абзаца без выделения). Повторный клик по своему уровню снимает
// его в обычный текст. Символьное форматирование не трогается:
// уровень заголовка живёт в блочном формате и в HTML едет как <h1>


// Установка уровня заголовка для абзацев в выделении (или текущего
// абзаца без выделения). Повторный клик по своему уровню снимает
// его в обычный текст. Символьное форматирование не трогается:
// уровень заголовка живёт в блочном формате и в HTML едет как <h1>
void TypefaceFormatter::applyHeadingLevel(int level)
{
    QTextCursor cursor=textArea->textCursor();

    // Границы прохода по абзацам: без выделения — текущий абзац
    const int from=cursor.hasSelection() ? cursor.selectionStart() : cursor.position();
    const int to=cursor.hasSelection() ? cursor.selectionEnd() : cursor.position();

    // Конец выделения ровно на границе абзаца этот абзац не включает:
    // иначе выделение строки вместе с переводом цепляет следующую,
    // та заранее становится заголовком, а повторный клик ее снимает
    int endBound=to;
    if(cursor.hasSelection() && to>from &&
       textArea->document()->findBlock(to).position()==to)
        endBound=to-1;

    // Тумблер: все затронутые абзацы уже этого уровня — снять в обычный
    bool allSame=true;

    for(QTextBlock block=textArea->document()->findBlock(from);
        block.isValid() && block.position()<=endBound;
        block=block.next())
    {
        if(block.blockFormat().headingLevel()!=level)
        {
            allSame=false;
            break;
        }
    }

    const int target=allSame ? 0 : level;

    // Эталон блочных отступов целевого уровня
    QTextBlockFormat probeBlockFormat;
    headingProbeMargins(target, probeBlockFormat);

    const double baseNormalSize=normalPointSize();

    cursor.beginEditBlock();

    // Разбить абзацы по внутристрочным разрывам строк (U+2028/U+2029
    // из вставок с веба) - но только когда заголовки ставятся, а не
    // снимаются. Иначе несколько зрительных строк остаются одним
    // блоком и уходят в один пункт оглавления. Справа налево, чтобы
    // позиции левее не ехали; курсор-выделение само едет за правками,
    // границы прохода перечитываются ниже
    int realFrom=from;
    int realTo=endBound;
    if(target>0)
    {
        QList<int> separators;
        for(QTextBlock block=textArea->document()->findBlock(from);
            block.isValid() && block.position()<=endBound;
            block=block.next())
        {
            const QString text=block.text();
            for(int i=0; i<text.length(); ++i)
                if(text[i]==QChar(0x2028) || text[i]==QChar(0x2029))
                    separators << block.position()+i;
        }

        for(int i=separators.size()-1; i>=0; --i)
        {
            QTextCursor splitCursor(textArea->document());
            splitCursor.setPosition(separators[i]);
            splitCursor.insertBlock();

            // Курсор встает в начало нового абзаца - прямо на
            // символ-разрыв: удаляется он сам, а не перенос
            // (удаление по старой позиции стирало перенос и
            // склеивало абзацы обратно)
            splitCursor.deleteChar();
        }

        realFrom=cursor.hasSelection() ? cursor.selectionStart() : cursor.position();
        realTo=cursor.hasSelection() ? cursor.selectionEnd() : cursor.position();

        // То же правило границы после сплита: конец ровно на границе
        // абзаца этот абзац не включает
        if(cursor.hasSelection() && realTo>realFrom &&
           textArea->document()->findBlock(realTo).position()==realTo)
            realTo=realTo-1;
    }

    for(QTextBlock block=textArea->document()->findBlock(realFrom);
        block.isValid() && block.position()<=realTo;
        block=block.next())
    {
        const int oldLevel=block.blockFormat().headingLevel();

        HeadingStash *stash=static_cast<HeadingStash *>(block.userData());

        // Обычный абзац без stash трогать нечем: снимать нечего,
        // иначе сброс в обычный текст затирал бы размер начертания
        if(target==0 && oldLevel==0 && (stash==nullptr || !stash->hasData))
            continue;

        // Пустые абзацы заголовками не делаются: иначе плодятся
        // пустые H1/H2/H3. Снятие уровня с пустых работает как обычно
        if(target>0 && block.text().simplified().isEmpty())
            continue;

        // В заголовок: исходное запомнить один раз для точного отката.
        // Незаданный размер резолвится в базовый сразу: дальше везде
        // только явные значения, никаких снятий и нулей
        if(target>0 && stash==nullptr)
        {
            stash=new HeadingStash();
            QTextBlock::iterator it=block.begin();
            if(!it.atEnd())
            {
                const QTextCharFormat src=it.fragment().charFormat();
                stash->pointSize=src.fontPointSize()>0.0 ? src.fontPointSize() : baseNormalSize;
                stash->weight=src.fontWeight();
                stash->sizeAdjustment=src.property(QTextFormat::FontSizeAdjustment);
            }
            else
            {
                stash->pointSize=baseNormalSize;
            }
            stash->topMargin=block.blockFormat().topMargin();
            stash->bottomMargin=block.blockFormat().bottomMargin();
            stash->stashedLevel=oldLevel;
            stash->hasData=true;
            block.setUserData(stash);
        }

        QTextBlockFormat format=block.blockFormat();
        format.setHeadingLevel(target);
        format.setTopMargin(probeBlockFormat.topMargin());
        format.setBottomMargin(probeBlockFormat.bottomMargin());

        QTextCursor edit(block);
        edit.setBlockFormat(format);

        // Снапшот фрагментов абзаца: позиции и форматы. Применяется
        // вторым проходом, так как setCharFormat дробит фрагменты
        // и инвалидирует итератор. Позиции не едут: длина не меняется
        struct FragmentSnapshot
        {
            int position;
            int length;
            QTextCharFormat format;
        };
        QList<FragmentSnapshot> snapshots;
        for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
        {
            FragmentSnapshot snap;
            snap.position=it.fragment().position();
            snap.length=it.fragment().length();
            snap.format=it.fragment().charFormat();
            snapshots << snap;
        }

        // Размер заголовка - абсолютный: масштаб от базового. Свойство
        // FontSizeAdjustment не трогается выставлением, только сбросом
        // в нейтральный ноль: оно не откатывается undo и плодит мусор
        if(target>0)
        {
            double baseSize=baseNormalSize;
            if(stash!=nullptr && stash->hasData && stash->stashedLevel==0 && stash->pointSize>0.0)
                baseSize=stash->pointSize;
            else if(oldLevel==0 && !snapshots.isEmpty() &&
                    snapshots.first().format.fontPointSize()>0.0)
                baseSize=snapshots.first().format.fontPointSize();
            // Блок уже заголовок без stash (загруженный документ):
            // база - шрифт документа. Текущий кегль тут след прошлого
            // применения, брать его базой - множить при каждом клике

            const double absoluteSize=baseSize*headingScaleFactor(target);

            for(const FragmentSnapshot &snap : snapshots)
            {
                QTextCursor fragmentCursor(textArea->document());
                fragmentCursor.setPosition(snap.position);
                fragmentCursor.setPosition(snap.position+snap.length, QTextCursor::KeepAnchor);

                // Только merge явными значениями: судьба каждого
                // свойства предсказуема (ничего лишнего не задевается),
                // а полный setCharFormat на выделении в этой версии Qt
                // замечен в непересчете вида. Ступень FontSizeAdjustment
                // только снимается где есть: само ее присутствие
                // переключает раскладку на свой путь и глушит явный
                // размер, а выставление не откатывается undo.
                // Свежим фрагментам она не ставится
                QTextCharFormat fragmentFormat;
                fragmentFormat.setFontPointSize(absoluteSize);
                fragmentFormat.setFontWeight(QFont::Bold);
                if(snap.format.hasProperty(QTextFormat::FontSizeAdjustment))
                    fragmentFormat.clearProperty(QTextFormat::FontSizeAdjustment);
                fragmentCursor.mergeCharFormat(fragmentFormat);
            }
        }
        else
        {
            // Назад в обычный. Был обычным - вернуть свое из stash,
            // был загруженным заголовком - нормализовать: свой размер
            // (если задан), обычная жирность, ступень снимается.
            // Везде только merge явными значениями: снятий свойств,
            // кроме чужой ступени, нет, нейтральный ноль не пишется
            if(stash!=nullptr && stash->hasData && stash->stashedLevel==0)
            {
                format.setTopMargin(stash->topMargin);
                format.setBottomMargin(stash->bottomMargin);
                edit.setBlockFormat(format);

                for(const FragmentSnapshot &snap : snapshots)
                {
                    QTextCursor fragmentCursor(textArea->document());
                    fragmentCursor.setPosition(snap.position);
                    fragmentCursor.setPosition(snap.position+snap.length, QTextCursor::KeepAnchor);

                    QTextCharFormat fragmentFormat;
                    fragmentFormat.setFontPointSize(stash->pointSize);
                    fragmentFormat.setFontWeight(stash->weight);
                    if(stash->sizeAdjustment.isValid())
                        fragmentFormat.setProperty(QTextFormat::FontSizeAdjustment, stash->sizeAdjustment);
                    else
                        fragmentFormat.clearProperty(QTextFormat::FontSizeAdjustment);
                    fragmentCursor.mergeCharFormat(fragmentFormat);
                }
            }
            else
            {
                // Без stash с исходным обычным уровнем размер - след
                // прошлого заголовка (загруженный документ): сбросить
                // к базовому. Оставить как был - следующее применение
                // снова умножит (кегль "улетает в космос" при вкл/выкл)
                for(const FragmentSnapshot &snap : snapshots)
                {
                    QTextCursor fragmentCursor(textArea->document());
                    fragmentCursor.setPosition(snap.position);
                    fragmentCursor.setPosition(snap.position+snap.length, QTextCursor::KeepAnchor);

                    QTextCharFormat fragmentFormat;
                    fragmentFormat.setFontPointSize(baseNormalSize);
                    fragmentFormat.setFontWeight(QFont::Normal);
                    fragmentFormat.clearProperty(QTextFormat::FontSizeAdjustment);
                    fragmentCursor.mergeCharFormat(fragmentFormat);
                }
            }

            block.setUserData(nullptr);
        }
    }

    cursor.endEditBlock();

    // Подсветка актуального уровня в меню заголовков и на кнопках
    // обновляется сразу: курсор при смене уровня не двигается,
    // без этого галка висела бы до первого движения курсора
    emit updateOutlineButtonHiglight();
}


void TypefaceFormatter::onHeadingClicked(int index)
{
    // Пункты меню: 0-H1, 1-H2, 2-H3, 3-H4, 4-H5, 5-H6, 6-обычный текст
    if(index==0)
        applyHeadingLevel(1);
    else if(index==1)
        applyHeadingLevel(2);
    else if(index==2)
        applyHeadingLevel(3);
    else if(index==3)
        applyHeadingLevel(4);
    else if(index==4)
        applyHeadingLevel(5);
    else if(index==5)
        applyHeadingLevel(6);
    else if(index==6)
        applyHeadingLevel(0);
}


double TypefaceFormatter::normalPointSize(void) const
{
    double size=textArea->document()->defaultFont().pointSizeF();
    if(!(size>0.0))
        size=QApplication::font().pointSizeF();
    if(!(size>0.0))
        size=10.0;

    return size;
}


// Enter в заголовке. В конце непустого заголовка новый
// абзац сразу обычный (иначе каждый Enter плодит пустые H1/H2/H3),
// на пустом заголовке уровень снимается на месте без новой строки.
// Выделение строго внутри одного заголовка вырезается как штатно,
// затем работают те же правила. В середине абзаца, через абзацы
// и вне заголовков - false, работает штатный путь


// Enter в заголовке. В конце непустого заголовка новый
// абзац сразу обычный (иначе каждый Enter плодит пустые H1/H2/H3),
// на пустом заголовке уровень снимается на месте без новой строки.
// Выделение строго внутри одного заголовка вырезается как штатно,
// затем работают те же правила. В середине абзаца, через абзацы
// и вне заголовков - false, работает штатный путь
bool TypefaceFormatter::handleHeadingEnter(void)
{
    if(textArea->isReadOnly())
        return false;

    QTextCursor cursor=textArea->textCursor();

    bool eraseSelection=false;
    if(cursor.hasSelection())
    {
        // После выбора уровня из меню выделение обычно остается,
        // и Enter с ним уходил в штатный путь с наследованием уровня.
        // Выделение строго внутри одного заголовка вырезается здесь,
        // через абзацы - штатный путь
        QTextBlock startBlock=textArea->document()->findBlock(cursor.selectionStart());
        QTextBlock endBlock=textArea->document()->findBlock(cursor.selectionEnd());
        if(startBlock!=endBlock || startBlock.blockFormat().headingLevel()<1)
            return false;

        eraseSelection=true;
    }
    else if(cursor.block().blockFormat().headingLevel()<1)
        return false;

    QTextBlockFormat probeBlockFormat;
    headingProbeMargins(0, probeBlockFormat);

    const double baseSize=normalPointSize();

    cursor.beginEditBlock();

    if(eraseSelection)
        cursor.removeSelectedText();

    QTextBlock block=cursor.block();

    bool consumed=false;
    int headBlockNumber=-1;
    if(block.text().isEmpty())
    {
        // Пустой заголовок: уровень и вид снимаются на месте,
        // новой строки нет. Метка стирается: абзац снова обычный
        QTextBlockFormat format=block.blockFormat();
        format.setHeadingLevel(0);
        format.setTopMargin(probeBlockFormat.topMargin());
        format.setBottomMargin(probeBlockFormat.bottomMargin());

        QTextCursor edit(block);
        edit.setBlockFormat(format);
        block.setUserData(nullptr);

        textArea->setTextCursor(edit);
        QTextCharFormat current=textArea->currentCharFormat();
        current.setFontPointSize(baseSize);
        current.setFontWeight(QFont::Normal);
        current.clearProperty(QTextFormat::FontSizeAdjustment);
        textArea->setCurrentCharFormat(current);
        consumed=true;
    }
    else if(cursor.atBlockEnd())
    {
        // В конце заголовка: штатный сплит, затем новый абзац
        // приводится к обычному виду. В середине абзаца сплит
        // штатный: обе половины остаются заголовком
        cursor.insertBlock();

        QTextBlock newBlock=cursor.block();
        QTextBlockFormat format=newBlock.blockFormat();
        format.setHeadingLevel(0);
        format.setTopMargin(probeBlockFormat.topMargin());
        format.setBottomMargin(probeBlockFormat.bottomMargin());

        QTextCursor edit(newBlock);
        edit.setBlockFormat(format);

        textArea->setTextCursor(cursor);
        QTextCharFormat current=textArea->currentCharFormat();
        current.setFontPointSize(baseSize);
        current.setFontWeight(QFont::Normal);
        current.clearProperty(QTextFormat::FontSizeAdjustment);
        textArea->setCurrentCharFormat(current);
        consumed=true;
    }
    else if(cursor.atBlockStart() && !block.text().isEmpty())
    {
        // В начале заголовка: штатный сплит, затем ПЕРВЫЙ (пустой)
        // абзац приводится к обычному виду, курсор остается в нем.
        // Заголовок ниже сохраняет текст и уровень.
        // Состояние курсора выставляется ПОСЛЕ endEditBlock: внутри
        // макроса оно разбивало отмену на два шага вместо одного
        cursor.insertBlock();

        QTextBlock newFirst=cursor.block().previous();
        if(newFirst.isValid())
        {
            QTextBlockFormat format=newFirst.blockFormat();
            format.setHeadingLevel(0);
            format.setTopMargin(probeBlockFormat.topMargin());
            format.setBottomMargin(probeBlockFormat.bottomMargin());

            QTextCursor editFirst(newFirst);
            editFirst.setBlockFormat(format);
            headBlockNumber=newFirst.blockNumber();
        }
        consumed=true;
    }

    cursor.endEditBlock();

    if(consumed && headBlockNumber>=0)
    {
        QTextBlock headBlock=textArea->document()->findBlockByNumber(headBlockNumber);
        if(headBlock.isValid())
        {
            QTextCursor headCursor(headBlock);
            textArea->setTextCursor(headCursor);

            QTextCharFormat current=textArea->currentCharFormat();
            current.setFontPointSize(baseSize);
            current.setFontWeight(QFont::Normal);
            current.clearProperty(QTextFormat::FontSizeAdjustment);
            textArea->setCurrentCharFormat(current);
        }
    }

    return consumed;
}


// Префикс ссылок-переходов оглавления. Дальше через слэш:
// уровень, порядковый номер заголовка, кодированный текст.
// Номер - запасной ключ, если текст заголовка поменяли после вставки


// Префикс ссылок-переходов оглавления. Дальше через слэш:
// уровень, порядковый номер заголовка, кодированный текст.
// Номер - запасной ключ, если текст заголовка поменяли после вставки
QString TypefaceFormatter::tocGoPrefix(void)
{
    return QStringLiteral("mytetra://toc-go/");
}


// Адрес якоря оглавления: невидимая метка (символ нулевой ширины
// со ссылкой) в начале первой строки. Переживает сохранение,
// так как это обычная ссылка с односимвольным текстом


// Адрес якоря оглавления: невидимая метка (символ нулевой ширины
// со ссылкой) в начале первой строки. Переживает сохранение,
// так как это обычная ссылка с односимвольным текстом
QString TypefaceFormatter::tocAnchorHref(void)
{
    return QStringLiteral("mytetra://toc");
}


// Первый абзац с якорем оглавления. Невалидный блок - якоря в заметке нет


// Первый абзац с якорем оглавления. Невалидный блок - якоря в заметке нет
QTextBlock TypefaceFormatter::findTocAnchor(void) const
{
    for(QTextBlock block=textArea->document()->begin(); block.isValid(); block=block.next())
        for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
            if(it.fragment().charFormat().anchorHref()==tocAnchorHref())
                return block;

    return QTextBlock();
}


QList<TypefaceFormatter::TocEntry> TypefaceFormatter::collectTocEntries(void) const
{
    QList<TocEntry> entries;

    for(QTextBlock block=textArea->document()->begin(); block.isValid(); block=block.next())
    {
        const int level=block.blockFormat().headingLevel();
        if(level<1 || level>6)
            continue;

        QString text=block.text().simplified();
        if(text.isEmpty())
            continue;
        if(text.length()>120)
            text=text.left(120)+QStringLiteral("…");

        TocEntry entry;
        entry.level=level;
        entry.text=text;
        entry.position=block.position();
        entries << entry;
    }

    return entries;
}


bool TypefaceFormatter::blockHasTocLink(const QTextBlock &block) const
{
    if(!block.isValid())
        return false;

    for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
        if(it.fragment().charFormat().anchorHref().startsWith(tocGoPrefix()))
            return true;

    return false;
}


// Вставка снимка оглавления: каждый заголовок - абзац с отступом
// уровня и ссылкой-переходом. Первая строка несет невидимый якорь,
// по которому кнопка находит каноническое оглавление: повторный вызов
// обновляет его на месте куда бы курсор ни стоял, дубликаты не плодятся


// Вставка снимка оглавления: каждый заголовок - абзац с отступом
// уровня и ссылкой-переходом. Первая строка несет невидимый якорь,
// по которому кнопка находит каноническое оглавление: повторный вызов
// обновляет его на месте куда бы курсор ни стоял, дубликаты не плодятся
void TypefaceFormatter::insertTocEntries(QTextCursor &edit, const QList<TocEntry> &entries, bool withAnchor)
{
    for(int i=0; i<entries.size(); ++i)
    {
        if(i>0)
            edit.insertBlock();

        QTextBlockFormat blockFormat;
        blockFormat.setIndent(entries[i].level-1);
        edit.setBlockFormat(blockFormat);

        edit.setCharFormat(QTextCharFormat());

        if(i==0 && withAnchor)
        {
            QTextCharFormat anchorFormat;
            anchorFormat.setAnchor(true);
            anchorFormat.setAnchorHref(tocAnchorHref());
            edit.insertText(QString(QChar(0x200B)), anchorFormat);
        }

        const QString href=tocGoPrefix()
            +QString::number(entries[i].level)+QStringLiteral("/")
            +QString::number(i)+QStringLiteral("/")
            +QString::fromLatin1(QUrl::toPercentEncoding(entries[i].text));

        edit.insertText(entries[i].text);

        // Вставленный текст оформляется ссылкой-переходом
        QTextCursor linkCursor(edit.document());
        const int linkEnd=edit.position();
        linkCursor.setPosition(linkEnd-entries[i].text.length());
        linkCursor.setPosition(linkEnd, QTextCursor::KeepAnchor);

        QTextCharFormat linkFormat;
        linkFormat.setAnchor(true);
        linkFormat.setAnchorHref(href);
        linkFormat.setForeground(editorConfig->linkColorForHref(href));
        linkFormat.setFontUnderline(true);
        linkCursor.mergeCharFormat(linkFormat);
    }
}


// Вставка снимка оглавления: повторный вызов не плодит дубликат,
// а обновляет существующие снимки на месте (первому выставляется
// якорь) и ставит курсор в начало канонического оглавления.
// Новое оглавление вставляется в курсор, только если снимков нет


// Вставка снимка оглавления: повторный вызов не плодит дубликат,
// а обновляет существующие снимки на месте (первому выставляется
// якорь) и ставит курсор в начало канонического оглавления.
// Новое оглавление вставляется в курсор, только если снимков нет
void TypefaceFormatter::onTocClicked(void)
{
    const QList<TocEntry> entries=collectTocEntries();

    // Все существующие оглавления: непрерывные куски абзацев-ссылок
    QList<int> regionStarts;
    for(QTextBlock block=textArea->document()->begin(); block.isValid(); )
    {
        if(!blockHasTocLink(block))
        {
            block=block.next();
            continue;
        }
        regionStarts << block.position();
        do { block=block.next(); } while(block.isValid() && blockHasTocLink(block));
    }

    QTextCursor edit(textArea->document());
    edit.beginEditBlock();

    // Снимков нет - вставить новый в курсор (как раньше).
    // Без заголовков вообще - ничего не делать
    if(regionStarts.isEmpty())
    {
        if(entries.isEmpty())
        {
            edit.endEditBlock();
            return;
        }

        QTextCursor cursor=textArea->textCursor();

        // Новое оглавление пишется строго в пустой абзац, текст
        // существующих абзацев не разрезается и не затирается
        edit.setPosition(cursor.position());
        if(edit.block().text().isEmpty())
        {
            // Уже пустой абзац: пишем прямо сюда
        }
        else if(edit.atBlockStart())
        {
            // В начале абзаца: пустой абзац появляется перед ним
            edit.insertBlock();
            edit.movePosition(QTextCursor::PreviousBlock);
        }
        else
        {
            // Внутри или в конце абзаца: пустой абзац появляется после него
            edit.movePosition(QTextCursor::EndOfBlock);
            edit.insertBlock();
        }

        const int tocStart=edit.position();
        insertTocEntries(edit, entries, true);

        edit.endEditBlock();

        // Курсор - в начало оглавления: повторный хоткей обновит на месте
        QTextCursor result(textArea->document());
        result.setPosition(tocStart);
        textArea->setTextCursor(result);
        return;
    }

    // Снимки есть - обновить все на месте с конца к началу,
    // чтобы позиции ранних не ехали. Новых не создавать.
    // Пустой список заголовков просто стирает строки
    for(int i=regionStarts.size()-1; i>=0; --i)
    {
        QTextBlock first=textArea->document()->findBlock(regionStarts[i]);
        if(!first.isValid() || !blockHasTocLink(first))
            continue;

        QTextBlock last=first;
        while(last.next().isValid() && blockHasTocLink(last.next()))
            last=last.next();

        // Конец выделения - именно конец текста последнего абзаца
        // (EndOfBlock), его перенос строки остается: иначе строка
        // оглавления склеится со следующим. Позиции - целыми числами
        // до правок: хендлы QTextBlock после removeSelectedText
        // протухают и врут про position()
        const int firstPos=first.position();
        edit.setPosition(firstPos);
        edit.setPosition(last.position(), QTextCursor::KeepAnchor);
        edit.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        edit.removeSelectedText();
        edit.setPosition(firstPos);

        // Якорь - только первому (каноническому) оглавлению
        insertTocEntries(edit, entries, i==0);
    }

    edit.endEditBlock();

    // Курсор - в начало канонического оглавления
    QTextCursor result(textArea->document());
    result.setPosition(regionStarts.first());
    textArea->setTextCursor(result);
}


// Снять подчеркивание со ссылок-кнопок [copy]: HTML-импортер ставит
// его всем ссылкам по умолчанию, а свежие ссылки идут без него.
// Заодно чинятся старые целые ссылки-строки: insertText при замене
// маркера наследовал ссылку конца выделения, такие строки уже
// сохранены - якорь остается только на суффиксе [copy].
// Снимается merge с явным false (setCharFormat на выделении в этой
// версии Qt замечен в непересчете раскладки)


// Снять подчеркивание со ссылок-кнопок [copy]: HTML-импортер ставит
// его всем ссылкам по умолчанию, а свежие ссылки идут без него.
// Заодно чинятся старые целые ссылки-строки: insertText при замене
// маркера наследовал ссылку конца выделения, такие строки уже
// сохранены - якорь остается только на суффиксе [copy].
// Снимается merge с явным false (setCharFormat на выделении в этой
// версии Qt замечен в непересчете раскладки)
void TypefaceFormatter::normalizeCopyLinkUnderline(void)
{
    QTextDocument *doc=textArea->document();

    // Сначала позиции: merge дробит фрагменты и инвалидирует итератор.
    // Позиции не едут: длина текста не меняется
    QList<QPair<int, int> > linkRanges;

    for(QTextBlock block=doc->begin(); block.isValid(); block=block.next())
        for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
            if(it.fragment().charFormat().anchorHref()==copyCodeHref())
                linkRanges << qMakePair(it.fragment().position(), it.fragment().length());

    // Диапазоны снятия якоря: текст маркера и пробел после него,
    // сам суффикс [copy] ссылку сохраняет. Без суффикса в строке
    // якорю делать нечего - снимается со всей строки
    QList<QPair<int, int> > stripRanges;

    for(QTextBlock block=doc->begin(); block.isValid(); block=block.next())
    {
        if(codeMarkerLanguage(block.text())<0)
            continue;

        const QString text=block.text();
        const int copyAt=text.lastIndexOf(copyCodeSuffix());

        const int keepStart=copyAt>=0 ? block.position()+copyAt : -1;
        const int keepEnd=keepStart>=0 ?
                          keepStart+copyCodeSuffix().length() : -1;

        for(QTextBlock::iterator it=block.begin(); !it.atEnd(); ++it)
        {
            QTextFragment fragment=it.fragment();

            if(!fragment.isValid() || fragment.length()==0)
                continue;

            if(fragment.charFormat().anchorHref()!=copyCodeHref())
                continue;

            const int fragStart=fragment.position();
            const int fragEnd=fragStart+fragment.length();

            if(keepStart>=0)
            {
                // Кусок суффикса [copy] живет дальше со ссылкой
                if(fragStart>=keepStart && fragEnd<=keepEnd)
                    continue;

                if(fragStart<keepStart)
                    stripRanges << qMakePair(fragStart,
                                             qMin(fragEnd, keepStart)-fragStart);

                if(fragEnd>keepEnd)
                    stripRanges << qMakePair(qMax(fragStart, keepEnd),
                                             fragEnd-qMax(fragStart, keepEnd));
            }
            else
                stripRanges << qMakePair(fragStart, fragment.length());
        }
    }

    if(linkRanges.isEmpty() && stripRanges.isEmpty())
        return;

    QTextCursor edit(doc);
    edit.beginEditBlock();

    for(int i=0; i<linkRanges.size(); ++i)
    {
        edit.setPosition(linkRanges[i].first);
        edit.setPosition(linkRanges[i].first+linkRanges[i].second, QTextCursor::KeepAnchor);

        QTextCharFormat format;
        format.setFontUnderline(false);
        edit.mergeCharFormat(format);
    }

    for(int i=0; i<stripRanges.size(); ++i)
    {
        edit.setPosition(stripRanges[i].first);
        edit.setPosition(stripRanges[i].first+stripRanges[i].second, QTextCursor::KeepAnchor);

        QTextCharFormat format;
        format.setAnchor(false);
        format.setAnchorHref(QString());
        edit.mergeCharFormat(format);
    }

    edit.endEditBlock();
}


// Клик по пункту оглавления: прыжок в раздел. Сначала ищется
// заголовок с тем же уровнем и текстом (пережил правки вокруг),
// запасной вариант - тот же порядковый номер того же уровня.
// Ничего не нашлось - молча игнорируется (оглавление устарело)


// Клик по пункту оглавления: прыжок в раздел. Сначала ищется
// заголовок с тем же уровнем и текстом (пережил правки вокруг),
// запасной вариант - тот же порядковый номер того же уровня.
// Ничего не нашлось - молча игнорируется (оглавление устарело)
void TypefaceFormatter::onTocGoClicked(const QString &href)
{
    // Клик по самому якорю: прыжок в начало оглавления
    if(href==tocAnchorHref())
    {
        QTextBlock anchor=findTocAnchor();
        if(anchor.isValid())
        {
            QTextCursor cursor(textArea->document());
            cursor.setPosition(anchor.position());
            textArea->setTextCursor(cursor);
            textArea->ensureCursorVisible();
        }
        return;
    }

    if(!href.startsWith(tocGoPrefix()))
        return;

    const QStringList parts=href.mid(tocGoPrefix().length()).split(QStringLiteral("/"));
    if(parts.size()<3)
        return;

    bool okLevel=false;
    bool okIndex=false;
    const int level=parts[0].toInt(&okLevel);
    const int index=parts[1].toInt(&okIndex);
    const QString text=QString::fromUtf8(QByteArray::fromPercentEncoding(parts[2].toLatin1()));
    if(!okLevel || !okIndex)
        return;

    const QList<TocEntry> entries=collectTocEntries();

    int target=-1;
    for(int i=0; i<entries.size(); ++i)
        if(entries[i].level==level && entries[i].text==text)
        {
            target=i;
            break;
        }
    if(target<0)
        for(int i=0; i<entries.size(); ++i)
            if(entries[i].level==level && i==index)
            {
                target=i;
                break;
            }
    if(target<0)
        return;

    QTextCursor cursor(textArea->document());
    cursor.setPosition(entries[target].position);
    textArea->setTextCursor(cursor);
    textArea->ensureCursorVisible();
}


// Очистка форматирования, т.е. установка стандартного шрифта,
// размера и убирание утолщения, наклона, подчеркивания
// Картинки и таблицы не удаляются
