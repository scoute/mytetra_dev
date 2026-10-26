#include <QGridLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFont>

#include "EditorSpecialCharDialog.h"
#include "EditorConfig.h"
#include "libraries/helpers/ObjectHelper.h"


EditorSpecialCharDialog::EditorSpecialCharDialog(QWidget *parent) : QDialog(parent)
{
 setup_ui();
 assembly();
}


EditorSpecialCharDialog::~EditorSpecialCharDialog()
{

}


// Весь набор: типографика, латиница с диакритикой, греческий,
// стрелки, математика, валюты, фигуры, бокс-рисование
QStringList EditorSpecialCharDialog::allSymbols(void)
{
 return QStringList()
  // Кавычки, тире, типографика
  << QStringLiteral("«") << QStringLiteral("»") << QStringLiteral("„") << QStringLiteral("“") << QStringLiteral("”")
  << QStringLiteral("‘") << QStringLiteral("’") << QStringLiteral("‚") << QStringLiteral("‛") << QStringLiteral("–")
  << QStringLiteral("—") << QStringLiteral("―") << QStringLiteral("…") << QStringLiteral("•") << QStringLiteral("·")
  << QStringLiteral("°") << QStringLiteral("§") << QStringLiteral("¶") << QStringLiteral("†") << QStringLiteral("‡")
  << QStringLiteral("‹") << QStringLiteral("›") << QStringLiteral("❝") << QStringLiteral("❞") << QStringLiteral("‣")
  << QStringLiteral("◦") << QStringLiteral("¦") << QStringLiteral("¨") << QStringLiteral("´") << QStringLiteral("ˆ")
  << QStringLiteral("˜") << QStringLiteral("¯") << QStringLiteral("˘") << QStringLiteral("˙") << QStringLiteral("˚")
  << QStringLiteral("¸") << QStringLiteral("˛") << QStringLiteral("ˇ")
  // Латиница с диакритикой, нижний регистр
  << QStringLiteral("à") << QStringLiteral("á") << QStringLiteral("â") << QStringLiteral("ã") << QStringLiteral("ä")
  << QStringLiteral("å") << QStringLiteral("æ") << QStringLiteral("ç") << QStringLiteral("è") << QStringLiteral("é")
  << QStringLiteral("ê") << QStringLiteral("ë") << QStringLiteral("ì") << QStringLiteral("í") << QStringLiteral("î")
  << QStringLiteral("ï") << QStringLiteral("ð") << QStringLiteral("ñ") << QStringLiteral("ò") << QStringLiteral("ó")
  << QStringLiteral("ô") << QStringLiteral("õ") << QStringLiteral("ö") << QStringLiteral("ø") << QStringLiteral("ù")
  << QStringLiteral("ú") << QStringLiteral("û") << QStringLiteral("ü") << QStringLiteral("ý") << QStringLiteral("þ")
  << QStringLiteral("ÿ")
  // Латиница с диакритикой, верхний регистр
  << QStringLiteral("À") << QStringLiteral("Á") << QStringLiteral("Â") << QStringLiteral("Ã") << QStringLiteral("Ä")
  << QStringLiteral("Å") << QStringLiteral("Æ") << QStringLiteral("Ç") << QStringLiteral("È") << QStringLiteral("É")
  << QStringLiteral("Ê") << QStringLiteral("Ë") << QStringLiteral("Ì") << QStringLiteral("Í") << QStringLiteral("Î")
  << QStringLiteral("Ï") << QStringLiteral("Ð") << QStringLiteral("Ñ") << QStringLiteral("Ò") << QStringLiteral("Ó")
  << QStringLiteral("Ô") << QStringLiteral("Õ") << QStringLiteral("Ö") << QStringLiteral("Ø") << QStringLiteral("Ù")
  << QStringLiteral("Ú") << QStringLiteral("Û") << QStringLiteral("Ü") << QStringLiteral("Ý") << QStringLiteral("Þ")
  // Дроби и степени
  << QStringLiteral("½") << QStringLiteral("¼") << QStringLiteral("¾") << QStringLiteral("⅓") << QStringLiteral("⅔")
  << QStringLiteral("⅛") << QStringLiteral("⅜") << QStringLiteral("⅝") << QStringLiteral("⅞") << QStringLiteral("¹")
  << QStringLiteral("²") << QStringLiteral("³")
  // Греческий, нижний регистр
  << QStringLiteral("α") << QStringLiteral("β") << QStringLiteral("γ") << QStringLiteral("δ") << QStringLiteral("ε")
  << QStringLiteral("ζ") << QStringLiteral("η") << QStringLiteral("θ") << QStringLiteral("ι") << QStringLiteral("κ")
  << QStringLiteral("λ") << QStringLiteral("μ") << QStringLiteral("ν") << QStringLiteral("ξ") << QStringLiteral("ο")
  << QStringLiteral("π") << QStringLiteral("ρ") << QStringLiteral("ς") << QStringLiteral("σ") << QStringLiteral("τ")
  << QStringLiteral("υ") << QStringLiteral("φ") << QStringLiteral("χ") << QStringLiteral("ψ") << QStringLiteral("ω")
  // Греческий, верхний регистр
  << QStringLiteral("Α") << QStringLiteral("Β") << QStringLiteral("Γ") << QStringLiteral("Δ") << QStringLiteral("Ε")
  << QStringLiteral("Ζ") << QStringLiteral("Η") << QStringLiteral("Θ") << QStringLiteral("Ι") << QStringLiteral("Κ")
  << QStringLiteral("Λ") << QStringLiteral("Μ") << QStringLiteral("Ν") << QStringLiteral("Ξ") << QStringLiteral("Ο")
  << QStringLiteral("Π") << QStringLiteral("Ρ") << QStringLiteral("Σ") << QStringLiteral("Τ") << QStringLiteral("Υ")
  << QStringLiteral("Φ") << QStringLiteral("Χ") << QStringLiteral("Ψ") << QStringLiteral("Ω")
  // Математика
  << QStringLiteral("×") << QStringLiteral("÷") << QStringLiteral("±") << QStringLiteral("∓") << QStringLiteral("−")
  << QStringLiteral("≠") << QStringLiteral("≈") << QStringLiteral("∼") << QStringLiteral("≃") << QStringLiteral("≅")
  << QStringLiteral("≡") << QStringLiteral("≢") << QStringLiteral("≤") << QStringLiteral("≥") << QStringLiteral("≪")
  << QStringLiteral("≫") << QStringLiteral("∝") << QStringLiteral("∞") << QStringLiteral("√") << QStringLiteral("∛")
  << QStringLiteral("∑") << QStringLiteral("∏") << QStringLiteral("∐") << QStringLiteral("∫") << QStringLiteral("∬")
  << QStringLiteral("∂") << QStringLiteral("∆") << QStringLiteral("∇") << QStringLiteral("∈") << QStringLiteral("∋")
  << QStringLiteral("∉") << QStringLiteral("∅") << QStringLiteral("∪") << QStringLiteral("∩") << QStringLiteral("⊂")
  << QStringLiteral("⊃") << QStringLiteral("⊆") << QStringLiteral("⊇") << QStringLiteral("∧") << QStringLiteral("∨")
  << QStringLiteral("¬") << QStringLiteral("∀") << QStringLiteral("∃") << QStringLiteral("∴") << QStringLiteral("∵")
  << QStringLiteral("⊥") << QStringLiteral("∠") << QStringLiteral("⊙") << QStringLiteral("⊗") << QStringLiteral("⊕")
  << QStringLiteral("⊖")
  // Стрелки
  << QStringLiteral("←") << QStringLiteral("↑") << QStringLiteral("→") << QStringLiteral("↓") << QStringLiteral("↔")
  << QStringLiteral("↕") << QStringLiteral("↦") << QStringLiteral("↩") << QStringLiteral("↪") << QStringLiteral("⇒")
  << QStringLiteral("⇐") << QStringLiteral("⇑") << QStringLiteral("⇓") << QStringLiteral("⇔") << QStringLiteral("⤴")
  << QStringLiteral("➔") << QStringLiteral("⇧") << QStringLiteral("⇩")
  // Фигуры, метки
  << QStringLiteral("▲") << QStringLiteral("△") << QStringLiteral("▼") << QStringLiteral("►") << QStringLiteral("▷")
  << QStringLiteral("◄") << QStringLiteral("◁") << QStringLiteral("★") << QStringLiteral("☆") << QStringLiteral("✦")
  << QStringLiteral("✧") << QStringLiteral("♦") << QStringLiteral("♥") << QStringLiteral("♣") << QStringLiteral("♠")
  << QStringLiteral("●") << QStringLiteral("○") << QStringLiteral("◉") << QStringLiteral("◎") << QStringLiteral("◆")
  << QStringLiteral("◇") << QStringLiteral("■") << QStringLiteral("□") << QStringLiteral("✓") << QStringLiteral("✔")
  << QStringLiteral("✗") << QStringLiteral("✘") << QStringLiteral("☀") << QStringLiteral("☁") << QStringLiteral("☂")
  << QStringLiteral("☾")
  // Бокс-рисование
  << QStringLiteral("─") << QStringLiteral("│") << QStringLiteral("┌") << QStringLiteral("┐") << QStringLiteral("└")
  << QStringLiteral("┘") << QStringLiteral("├") << QStringLiteral("┤") << QStringLiteral("┬") << QStringLiteral("┴")
  << QStringLiteral("┼") << QStringLiteral("═") << QStringLiteral("║") << QStringLiteral("╔") << QStringLiteral("╗")
  << QStringLiteral("╚") << QStringLiteral("╝")
  // Валюты
  << QStringLiteral("$") << QStringLiteral("€") << QStringLiteral("£") << QStringLiteral("¥") << QStringLiteral("₽")
  << QStringLiteral("¢") << QStringLiteral("¤") << QStringLiteral("₹") << QStringLiteral("₩") << QStringLiteral("₴")
  << QStringLiteral("₸") << QStringLiteral("₺") << QStringLiteral("₪") << QStringLiteral("₫") << QStringLiteral("‰")
  << QStringLiteral("′") << QStringLiteral("″") << QStringLiteral("¿") << QStringLiteral("¡") << QStringLiteral("№")
  << QStringLiteral("©") << QStringLiteral("®") << QStringLiteral("™");
}


void EditorSpecialCharDialog::setup_ui(void)
{
 this->setWindowTitle(tr("Special character"));
}


void EditorSpecialCharDialog::assembly(void)
{
 QVBoxLayout *centralLayout=new QVBoxLayout(this);

 const int columns=10;
 const QFont buttonFont(QStringLiteral("DejaVu Sans"), 14);

 // Ряд недавних из конфига (если есть)
 EditorConfig *editorConfig=find_object<EditorConfig>("editorconfig");
 QStringList recent;

 if(editorConfig!=nullptr)
  recent=editorConfig->get_recentSymbols();

 if(!recent.isEmpty())
  {
   QLabel *recentLabel=new QLabel(tr("Recent:"), this);
   centralLayout->addWidget(recentLabel);

   // Недавние прижаты влево: иначе сетка тянется на всю ширину
   // и короткий ряд смотрится отцентрованным
   QHBoxLayout *recentRowLayout=new QHBoxLayout();
   recentRowLayout->setContentsMargins(0, 0, 0, 0);
   QGridLayout *recentLayout=new QGridLayout();
   int recentRow=0;
   fillGrid(recentLayout, recent, columns, recentRow);
   recentRowLayout->addLayout(recentLayout);
   recentRowLayout->addStretch();
   centralLayout->addLayout(recentRowLayout);
  }

 // Вся таблица — в прокрутке: три сотни кнопок в экран не влезают.
 // Минимум 8 рядов кнопок 36px, дальше растет вместе с окном:
 // у прокрутки вертикально Expanding, кнопка Cancel прижата к низу
 QScrollArea *scroll=new QScrollArea(this);
 scroll->setWidgetResizable(true);
 scroll->setMinimumHeight(360);
 scroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

 QWidget *gridHost=new QWidget(scroll);
 QGridLayout *gridLayout=new QGridLayout(gridHost);
 int row=0;
 fillGrid(gridLayout, allSymbols(), columns, row);

 scroll->setWidget(gridHost);
 centralLayout->addWidget(scroll);

 // Стартовой ширины хватает на все колонки без горизонтальной
 // прокрутки: минимум содержимого + вертикальный скроллбар
 // (таблица всегда выше вьюпорта, он виден сразу) + рамки
 scroll->setMinimumWidth(gridHost->minimumSizeHint().width()
                         + scroll->verticalScrollBar()->sizeHint().width()
                         + 2*scroll->frameWidth());

 // Кнопки шрифта покрупнее чтобы символы читались
 foreach(QPushButton *button, symbolButtons)
  button->setFont(buttonFont);

 // Только закрытие: выбор делается кликом по символу
 QDialogButtonBox *buttonBox=new QDialogButtonBox(QDialogButtonBox::Close, this);
 connect(buttonBox, &QDialogButtonBox::rejected, this, &EditorSpecialCharDialog::reject);
 centralLayout->addWidget(buttonBox);

 // Стартовый размер: ширина по содержимому, высота ужата чтобы
 // диалог открывался компактным (а не во всю таблицу) и рос за
 // счет прокрутки при растягивании. Без ужимки sizeHint прокрутки
 // с widgetResizable равен полной высоте таблицы
 adjustSize();

 const int maxH=QGuiApplication::primaryScreen()->availableGeometry().height()*2/3;

 if(height()>maxH)
  resize(width(), qMax(maxH, minimumHeight()));
}


void EditorSpecialCharDialog::fillGrid(QGridLayout *layout,
                                       const QStringList &symbols,
                                       int columns,
                                       int &row)
{
 int column=0;

 foreach(const QString &symbol, symbols)
  {
   QPushButton *button=new QPushButton(symbol, this);
   button->setFixedSize(36, 36);
   button->setToolTip(tr("Insert %1").arg(symbol));
   connect(button, &QPushButton::clicked,
           this,  &EditorSpecialCharDialog::on_symbol_button_clicked);
   layout->addWidget(button, row, column);
   symbolButtons.append(button);

   column++;

   if(column>=columns)
    {
     column=0;
     row++;
    }
  }

 if(column!=0)
  row++;
}


void EditorSpecialCharDialog::on_symbol_button_clicked(void)
{
 QPushButton *button=qobject_cast<QPushButton *>(sender());

 if(button==nullptr)
  return;

 pickedSymbol=button->text();

 // Диалог не закрывается: каждый клик сразу вставляет символ
 // в редактор через сигнал, можно набирать несколько подряд
 emit symbolPicked(pickedSymbol);
}


QString EditorSpecialCharDialog::selectedSymbol(void) const
{
 return pickedSymbol;
}
