#pragma once

#include <QObject>
#include <memory>
#include <string>
#include <map>
#include <mutex>

// 前向声明
class QMainWindow;

namespace seventeenlive {

// 前向声明 SeventeenLiveMenuManager 类
class SeventeenLiveMenuManager;

class SeventeenLiveApiWrappers;

struct SeventeenLiveLoginData;

/**
 * @brief SeventeenLiveCoreManager 类是17live插件的核心管理类
 * 
 * 该类采用单例模式设计，作为管理全部17live插件的控制中心。
 * 负责插件的初始化、配置管理、资源分配等核心功能。
 */
class SeventeenLiveCoreManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取SeventeenLiveCoreManager的单例实例
     * 
     * @param mainWindow OBS主窗体，仅在首次调用时需要提供
     * @return SeventeenLiveCoreManager& 单例实例的引用
     */
    static SeventeenLiveCoreManager& getInstance(QMainWindow* mainWindow = nullptr);

    /**
     * @brief 初始化核心管理器
     * 
     * @return bool 初始化是否成功
     */
    bool initialize();

    /**
     * @brief 关闭并清理资源
     */
    void shutdown();

    /**
     * @brief 获取OBS主窗体
     * 
     * @return QMainWindow* OBS主窗体指针
     */
    QMainWindow* getMainWindow() const;

    /**
     * @brief 设置配置项
     * 
     * @param key 配置键
     * @param value 配置值
     */
    void setConfig(const std::string& key, const std::string& value);

    /**
     * @brief 获取配置项
     * 
     * @param key 配置键
     * @param defaultValue 默认值
     * @return std::string 配置值
     */
    std::string getConfig(const std::string& key, const std::string& defaultValue = "");

    /**
     * @brief 获取菜单管理器
     * 
     * @return SeventeenLiveMenuManager* 菜单管理器指针
     */
    SeventeenLiveMenuManager* getMenuManager() const;

    bool handleLoginClicked();

    // 禁止拷贝构造和赋值操作
    SeventeenLiveCoreManager(const SeventeenLiveCoreManager&) = delete;
    SeventeenLiveCoreManager& operator=(const SeventeenLiveCoreManager&) = delete;

    /**
     * @brief 处理登录成功的槽函数
     * 
     * @param userData 登录成功后返回的用户数据
     */
    void handleLoginSuccess(const SeventeenLiveLoginData& userData);

    void handleLogoutClicked();

private:
    // 私有构造函数，确保只能通过getInstance方法获取实例
    explicit SeventeenLiveCoreManager(QMainWindow* mainWindow);
    
    // 私有析构函数
    ~SeventeenLiveCoreManager();

    // 单例实例
    static SeventeenLiveCoreManager* instance;
    
    // 互斥锁，用于线程安全的单例访问
    static std::mutex instanceMutex;

    // OBS主窗体
    QMainWindow* mainWindow;
    
    // 配置存储
    std::map<std::string, std::string> configMap;
    
    // 初始化标志
    bool initialized;

    // 菜单管理器
    std::unique_ptr<SeventeenLiveMenuManager> menuManager;

    std::unique_ptr<SeventeenLiveApiWrappers> apiWrapper;

    // 检查登录状态是否有效的函数
    bool checkLoginStatus();
};

} // namespace seventeenlive
