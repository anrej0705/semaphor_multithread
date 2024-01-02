#include "semaphor_tcp_server.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    semaphor_tcp_server w;
    w.show();
    return a.exec();
}
