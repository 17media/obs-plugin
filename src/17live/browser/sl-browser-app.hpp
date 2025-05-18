#pragma once

#include "cef-headers.hpp"

typedef std::function<void(CefRefPtr<CefBrowser>)> BrowserFunc;

#ifdef ENABLE_BROWSER_QT_LOOP
#include <QObject>
#include <QTimer>
#include <mutex>
#include <deque>

typedef std::function<void()> MessageTask;

    class MessageObject : public QObject {
        Q_OBJECT
    
        friend void QueueBrowserTask(CefRefPtr<CefBrowser> browser, BrowserFunc func);
    
        struct Task {
            CefRefPtr<CefBrowser> browser;
            BrowserFunc func;
    
            inline Task() {}
            inline Task(CefRefPtr<CefBrowser> browser_, BrowserFunc func_) : browser(browser_), func(func_) {}
        };
    
        std::mutex browserTaskMutex;
        std::deque<Task> browserTasks;
    
    public slots:
	bool ExecuteNextBrowserTask();
	void ExecuteTask(MessageTask task);
	void DoCefMessageLoop(int ms);
	void Process();
};

extern void QueueBrowserTask(CefRefPtr<CefBrowser> browser, BrowserFunc func);
#endif

class BrowserApp : public CefApp, public CefBrowserProcessHandler {
public:
    BrowserApp();
    // CefApp 接口实现
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }
    // CefBrowserProcessHandler 接口实现
    virtual void OnContextInitialized() override;
    // CefRefPtr<CefClient> GetDefaultClient() override;

    #ifdef ENABLE_BROWSER_QT_LOOP
    #if CHROME_VERSION_BUILD < 5938
        virtual void OnScheduleMessagePumpWork(int64 delay_ms) override;
    #else
        virtual void OnScheduleMessagePumpWork(int64_t delay_ms) override;
    #endif
        QTimer frameTimer;
    #endif
    
    IMPLEMENT_REFCOUNTING(BrowserApp);
};
