#include "sl-browser-app.hpp"

#ifdef ENABLE_BROWSER_QT_LOOP
#include <util/base.h>
#include <util/platform.h>
#include <util/threading.h>
#include <QTimer>
#endif
BrowserApp::BrowserApp() = default;

void BrowserApp::OnContextInitialized() {
    // 浏览器进程初始化完成后的回调
    // 这里可以执行一些初始化操作
}

// CefRefPtr<CefClient> BrowserApp::GetDefaultClient() {
//     // Called when a new browser window is created via Chrome style UI.
//     return SimpleHandler::GetInstance();
// }

#ifdef ENABLE_BROWSER_QT_LOOP
Q_DECLARE_METATYPE(MessageTask);
MessageObject messageObject;

void QueueBrowserTask(CefRefPtr<CefBrowser> browser, BrowserFunc func)
{
	std::lock_guard<std::mutex> lock(messageObject.browserTaskMutex);
	messageObject.browserTasks.emplace_back(browser, func);

	QMetaObject::invokeMethod(&messageObject, "ExecuteNextBrowserTask", Qt::QueuedConnection);
}

bool MessageObject::ExecuteNextBrowserTask()
{
	Task nextTask;
	{
		std::lock_guard<std::mutex> lock(browserTaskMutex);
		if (!browserTasks.size())
			return false;

		nextTask = browserTasks[0];
		browserTasks.pop_front();
	}

	nextTask.func(nextTask.browser);
	return true;
}

void MessageObject::ExecuteTask(MessageTask task)
{
	task();
}

void MessageObject::DoCefMessageLoop(int ms)
{
	if (ms)
		QTimer::singleShot((int)ms + 2, []() { CefDoMessageLoopWork(); });
	else
		CefDoMessageLoopWork();
}

void MessageObject::Process()
{
	CefDoMessageLoopWork();
}

void ProcessCef()
{
	QMetaObject::invokeMethod(&messageObject, "DoCefMessageLoop", Qt::QueuedConnection, Q_ARG(int, (int)0));
}

#define MAX_DELAY (1000 / 30)

#if CHROME_VERSION_BUILD < 5938
void BrowserApp::OnScheduleMessagePumpWork(int64 delay_ms)
#else
void BrowserApp::OnScheduleMessagePumpWork(int64_t delay_ms)
#endif
{
	if (delay_ms < 0)
		delay_ms = 0;
	else if (delay_ms > MAX_DELAY)
		delay_ms = MAX_DELAY;

	if (!frameTimer.isActive()) {
		QObject::connect(&frameTimer, &QTimer::timeout, &messageObject, &MessageObject::Process);
		frameTimer.setSingleShot(false);
		frameTimer.start(33);
	}

	QMetaObject::invokeMethod(&messageObject, "DoCefMessageLoop", Qt::QueuedConnection, Q_ARG(int, (int)delay_ms));
}
#endif
