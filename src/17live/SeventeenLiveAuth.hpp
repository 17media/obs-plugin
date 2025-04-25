#pragma once

#include <QObject>
// #include <memory>
#include <string>

namespace seventeenlive {

/**
 * @brief 17Live 认证类，处理用户登录和认证相关功能
 */
class SeventeenLiveAuth : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit SeventeenLiveAuth(QObject* parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~SeventeenLiveAuth();

    /**
     * @brief 显示登录对话框
     * @return 登录是否成功
     */
    static bool Login(QWidget *parent);

signals:
    /**
     * @brief 登录成功信号
     * @param token 登录令牌
     */
    void loginSuccess(const std::string& token);
    
    /**
     * @brief 登录失败信号
     * @param errorMessage 错误信息
     */
    void loginFailed(const std::string& errorMessage);
};

} // namespace seventeenlive
