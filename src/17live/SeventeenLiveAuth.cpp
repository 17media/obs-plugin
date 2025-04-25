#include "SeventeenLiveAuth.hpp"
#include <QVBoxLayout>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <obs-module.h>

#include "LoginDialog.hpp"

// 包含 moc 生成的代码
#include "moc_SeventeenLiveAuth.cpp"
// 为 LoginHandler 类生成 moc 代码
#include "SeventeenLiveAuth.moc"

namespace seventeenlive {

SeventeenLiveAuth::SeventeenLiveAuth(QObject* parent)
    : QObject(parent)
{
}

SeventeenLiveAuth::~SeventeenLiveAuth()
{
}

bool SeventeenLiveAuth::Login(QWidget *parent)
{
    LoginDialog dialog(parent);
    return dialog.exec() == QDialog::Accepted;
}


} // namespace seventeenlive
