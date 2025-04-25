#include "SeventeenLiveCoreManager.hpp"
#include <QMainWindow>

#include "SeventeenLiveMenuManager.hpp"

namespace seventeenlive {

// 初始化静态成员变量
SeventeenLiveCoreManager* SeventeenLiveCoreManager::instance = nullptr;
std::mutex SeventeenLiveCoreManager::instanceMutex;

SeventeenLiveCoreManager& SeventeenLiveCoreManager::getInstance(QMainWindow* mainWindow)
{
    // 使用双重检查锁定模式确保线程安全
    if (instance == nullptr) {
        std::lock_guard<std::mutex> lock(instanceMutex);
        if (instance == nullptr) {
            if (mainWindow == nullptr) {
                throw std::runtime_error("首次调用getInstance时必须提供mainWindow参数");
            }
            instance = new SeventeenLiveCoreManager(mainWindow);
        }
    }
    return *instance;
}

SeventeenLiveCoreManager::SeventeenLiveCoreManager(QMainWindow* mainWindow)
    : mainWindow(mainWindow), initialized(false)
{
    // 构造函数中初始化 menuManager
    menuManager = std::make_unique<SeventeenLiveMenuManager>(mainWindow);
}

SeventeenLiveCoreManager::~SeventeenLiveCoreManager()
{
    // 确保在析构前调用shutdown
    if (initialized) {
        shutdown();
    }
}

bool SeventeenLiveCoreManager::initialize()
{
    // 防止重复初始化
    if (initialized) {
        return true;
    }

    // // 初始化菜单管理器
    // if (menuManager && !menuManager->initialize()) {
    //     return false;
    // }

    initialized = true;
    return true;
}

void SeventeenLiveCoreManager::shutdown()
{
    if (!initialized) {
        return;
    }

    // 清理菜单管理器资源
    if (menuManager) {
        menuManager->cleanup();
    }

    initialized = false;
}

QMainWindow* SeventeenLiveCoreManager::getMainWindow() const
{
    return mainWindow;
}

void SeventeenLiveCoreManager::setConfig(const std::string& key, const std::string& value)
{
    configMap[key] = value;
}

std::string SeventeenLiveCoreManager::getConfig(const std::string& key, const std::string& defaultValue)
{
    auto it = configMap.find(key);
    if (it != configMap.end()) {
        return it->second;
    }
    return defaultValue;
}

SeventeenLiveMenuManager* SeventeenLiveCoreManager::getMenuManager() const
{
    return menuManager.get();
}

} // namespace seventeenlive
