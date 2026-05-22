#include <QGuiApplication>
#include <QAccessible>
int main(int argc, char **argv) { 
    QGuiApplication app(argc, argv);
    QAccessible::setActive(false); 
    return 0; 
}
