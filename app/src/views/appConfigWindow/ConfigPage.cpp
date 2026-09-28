#include <QWidget>
#include <QDebug>

#include "ConfigPage.h"


ConfigPage::ConfigPage(QWidget *parent) : QWidget(parent)
{

}


ConfigPage::~ConfigPage()
{
    qDebug() << Q_FUNC_INFO;
}


int ConfigPage::applyChanges(void)
{
    return 0;
}


void ConfigPage::cancelChanges(void)
{
    return;
}

