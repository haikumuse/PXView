#include <QApplication>
#include <QToolBar>
#include <QGridLayout>
#include <QComboBox>
#include <QDebug>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QToolBar *tb = new QToolBar();
    QComboBox *cb = new QComboBox(tb);
    tb->addWidget(cb);
    qDebug() << "Parent after addWidget:" << cb->parent();
    
    QWidget *inner = new QWidget();
    QGridLayout *grid = new QGridLayout(inner);
    grid->addWidget(cb);
    qDebug() << "Parent after grid->addWidget:" << cb->parent();
    
    return 0;
}
