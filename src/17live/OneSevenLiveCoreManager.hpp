#pragma once

#include <QObject>
#include <memory>
#include <string>
#include <map>
#include <mutex>

// 前向声明
class QMainWindow;

class BrowserApp;

// 前向声明 OneSevenLiveMenuManager 类
class OneSevenLiveMenuManager;

class OneSevenLiveApiWrappers;

class OneSevenLiveConfigManager;

struct OneSevenLiveLoginData;

class OneSevenLiveStreamingDock;

class OneSevenLiveStreamListDock;

struct OneSevenLiveRtmpRequest;

class OneSevenLiveHttpServer;

/**
 * @brief OneSevenLiveCoreManager 类是17live插件的核心管理类
 * 
 * 该类采用单例模式设计，作为管理全部17live插件的控制中心。
 * 负责插件的初始化、配置管理、资源分配等核心功能。
 */
class OneSevenLiveCoreManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取OneSevenLiveCoreManager的单例实例
     * 
     * @param mainWindow OBS主窗体，仅在首次调用时需要提供
     * @return OneSevenLiveCoreManager& 单例实例的引用
     */
    static OneSevenLiveCoreManager& getInstance(QMainWindow* mainWindow = nullptr);

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
     * @brief 获取菜单管理器
     * 
     * @return OneSevenLiveMenuManager* 菜单管理器指针
     */
    OneSevenLiveMenuManager* getMenuManager() const;
    
    /**
     * @brief 获取API包装器
     * 
     * @return OneSevenLiveApiWrappers* API包装器指针
     */
    OneSevenLiveApiWrappers* getApiWrapper() const;

    OneSevenLiveConfigManager* getConfigManager() const;

    bool handleLoginClicked();

    // 禁止拷贝构造和赋值操作
    OneSevenLiveCoreManager(const OneSevenLiveCoreManager&) = delete;
    OneSevenLiveCoreManager& operator=(const OneSevenLiveCoreManager&) = delete;

private:
    // 私有构造函数，确保只能通过getInstance方法获取实例
    explicit OneSevenLiveCoreManager(QMainWindow* mainWindow);
    
    // 私有析构函数
    ~OneSevenLiveCoreManager();

    // 单例实例
    static OneSevenLiveCoreManager* instance;
    
    // 互斥锁，用于线程安全的单例访问
    static std::mutex instanceMutex;

    // OBS主窗体
    QMainWindow* mainWindow;
    
    // 配置存储
    std::map<std::string, std::string> configMap;
    
    // 初始化标志
    bool initialized;

    std::unique_ptr<OneSevenLiveConfigManager> configManager;

    // 菜单管理器
    std::unique_ptr<OneSevenLiveMenuManager> menuManager;

    std::unique_ptr<OneSevenLiveApiWrappers> apiWrapper;

    std::unique_ptr<OneSevenLiveHttpServer> httpServer_;

    /**
     * @brief 处理登录成功的槽函数
     * 
     * @param userData 登录成功后返回的用户数据
     */
    void handleLoginSuccess(const OneSevenLiveLoginData& userData);

    void handleLogoutClicked();

    // 检查登录状态是否有效的函数
    bool checkLoginStatus();
    
    // Streaming Dock load status
    bool streamingDockFirstLoad = true;
    OneSevenLiveStreamingDock* streamingDock{nullptr};
    void handleStreamingClicked();

    void handleChatRoomClicked();

    bool liveListDockFirstLoad = true;
    OneSevenLiveStreamListDock* liveListDock{nullptr};
    void handleLiveListClicked();

    void saveDockState();

    void load17LiveConfig();
};
