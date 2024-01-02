#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_semaphor_tcp_server.h"

class semaphor_tcp_server : public QMainWindow
{
    Q_OBJECT

public:
    semaphor_tcp_server(QWidget *parent = nullptr);
    ~semaphor_tcp_server();

private:
    Ui::semaphor_tcp_serverClass ui;
};
