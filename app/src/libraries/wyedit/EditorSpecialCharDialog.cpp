#include <QGridLayout>
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


// Весь набор: типографика, стрелки, математика, валюты, фигуры
QStringList EditorSpecialCharDialog::allSymbols(void)
{
 return QStringList()
  << QStringLiteral("«") << QStringLiteral("»") << QStringLiteral("„") << QStringLiteral("“") << QStringLiteral("”")
  << QStringLiteral("‘") << QStringLiteral("’") << QStringLiteral("‚") << QStringLiteral("–") << QStringLiteral("—")
  << QStringLiteral("−") << QStringLiteral("…") << QStringLiteral("•") << QStringLiteral("·") << QStringLiteral("°")
  << QStringLiteral("§") << QStringLiteral("¶") << QStringLiteral("©") << QStringLiteral("®") << QStringLiteral("™")
  << QStringLiteral("№") << QStringLiteral("‰") << QStringLiteral("′") << QStringLiteral("″") << QStringLiteral("¿")
  << QStringLiteral("¡") << QStringLiteral("×") << QStringLiteral("÷") << QStringLiteral("±") << QStringLiteral("≠")
  << QStringLiteral("≈") << QStringLiteral("∞") << QStringLiteral("√") << QStringLiteral("∑") << QStringLiteral("∏")
  << QStringLiteral("∫") << QStringLiteral("∂") << QStringLiteral("∆") << QStringLiteral("π") << QStringLiteral("µ")
  << QStringLiteral("Ω") << QStringLiteral("α") << QStringLiteral("β") << QStringLiteral("γ") << QStringLiteral("λ")
  << QStringLiteral("Δ") << QStringLiteral("Σ") << QStringLiteral("≤") << QStringLiteral("≥") << QStringLiteral("∈")
  << QStringLiteral("∪") << QStringLiteral("∩") << QStringLiteral("∧") << QStringLiteral("∨") << QStringLiteral("¬")
  << QStringLiteral("∅") << QStringLiteral("∀") << QStringLiteral("∃") << QStringLiteral("←") << QStringLiteral("↑")
  << QStringLiteral("→") << QStringLiteral("↓") << QStringLiteral("↔") << QStringLiteral("↕") << QStringLiteral("⇒")
  << QStringLiteral("⇐") << QStringLiteral("⇔") << QStringLiteral("▲") << QStringLiteral("▼") << QStringLiteral("►")
  << QStringLiteral("◄") << QStringLiteral("★") << QStringLiteral("☆") << QStringLiteral("♦") << QStringLiteral("♥")
  << QStringLiteral("♣") << QStringLiteral("♠") << QStringLiteral("●") << QStringLiteral("○") << QStringLiteral("◆")
  << QStringLiteral("◇") << QStringLiteral("■") << QStringLiteral("□") << QStringLiteral("✓") << QStringLiteral("✗")
  << QStringLiteral("$") << QStringLiteral("€") << QStringLiteral("£") << QStringLiteral("¥") << QStringLiteral("₽")
  << QStringLiteral("¢");
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

   QGridLayout *recentLayout=new QGridLayout();
   int recentRow=0;
   fillGrid(recentLayout, recent, columns, recentRow);
   centralLayout->addLayout(recentLayout);
  }

 // Вся таблица
 QLabel *allLabel=new QLabel(tr("All symbols:"), this);
 centralLayout->addWidget(allLabel);

 QGridLayout *gridLayout=new QGridLayout();
 int row=0;
 fillGrid(gridLayout, allSymbols(), columns, row);
 centralLayout->addLayout(gridLayout);

 // Кнопки шрифта покрупнее чтобы символы читались
 foreach(QPushButton *button, symbolButtons)
  button->setFont(buttonFont);

 // Только отмена: выбор делается кликом по символу
 QDialogButtonBox *buttonBox=new QDialogButtonBox(QDialogButtonBox::Cancel, this);
 connect(buttonBox, &QDialogButtonBox::rejected, this, &EditorSpecialCharDialog::reject);
 centralLayout->addWidget(buttonBox);
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
