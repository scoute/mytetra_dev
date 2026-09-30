#include <QtTest>

#include "libraries/helpers/HtmlHelper.h"

// Тестовый класс для проверки экранирования HTML-спецсимволов.

class TestHtmlHelper : public QObject
{
    Q_OBJECT

private slots:

    // Проверка создания объекта (методы статические,
    // но конструктор должен существовать и работать)
    void constructorDestructor()
    {
        HtmlHelper helper;
        Q_UNUSED(helper);
    }

    // Проверка экранирования кавычек и угловых скобок
    void escapeSpecialChars()
    {
        QCOMPARE(HtmlHelper::htmlSpecialChars("\""), QString("&quot;"));
        QCOMPARE(HtmlHelper::htmlSpecialChars("<"), QString("&lt;"));
        QCOMPARE(HtmlHelper::htmlSpecialChars(">"), QString("&gt;"));
        QCOMPARE(HtmlHelper::htmlSpecialChars("<a href=\"x\">y</a>"),
                 QString("&lt;a href=&quot;x&quot;&gt;y&lt;/a&gt;"));
    }

    // Проверка строк без спецсимволов и пустой строки
    void escapePlainText()
    {
        QCOMPARE(HtmlHelper::htmlSpecialChars(""), QString(""));
        QCOMPARE(HtmlHelper::htmlSpecialChars("plain text 123"), QString("plain text 123"));
        // Амперсанд не экранируется текущей реализацией
        QCOMPARE(HtmlHelper::htmlSpecialChars("a&b"), QString("a&b"));
    }

    // Проверка обратного декодирования
    void decodeSpecialChars()
    {
        QCOMPARE(HtmlHelper::htmlSpecialCharsDecode("&quot;"), QString("\""));
        QCOMPARE(HtmlHelper::htmlSpecialCharsDecode("&lt;"), QString("<"));
        QCOMPARE(HtmlHelper::htmlSpecialCharsDecode("&gt;"), QString(">"));
        QCOMPARE(HtmlHelper::htmlSpecialCharsDecode(""), QString(""));
        QCOMPARE(HtmlHelper::htmlSpecialCharsDecode("plain"), QString("plain"));
    }

    // Проверка кругового преобразования: encode -> decode
    void roundTrip()
    {
        QStringList samples;
        samples << "" << "hello" << "say \"hi\"" << "<tag>text</tag>"
                << "a < b && c > d \"quoted\"";

        foreach (const QString &sample, samples)
        {
            QCOMPARE(HtmlHelper::htmlSpecialCharsDecode(HtmlHelper::htmlSpecialChars(sample)), sample);
        }
    }
};

QTEST_APPLESS_MAIN(TestHtmlHelper)

#include "tst_htmlhelper.moc"
