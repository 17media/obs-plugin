#include "LocalGatewayService.hpp"
#include "../OneSevenLiveCoreManager.hpp"
#include "../websocket/OneSevenLiveWebsocketServer.hpp"
#include "../websocket/OneSevenLiveHttpServer.hpp"
#include "../utility/NetworkDiagnostics.hpp"
#include <obs-module.h>

LocalGatewayService::LocalGatewayService(OneSevenLiveCoreManager* coreManager, QObject* parent)
    : QObject(parent), coreManager_(coreManager) {}

LocalGatewayService::~LocalGatewayService() {
    shutdownLocalServers();
}

bool LocalGatewayService::initLocalServers() {
    // Run network diagnostics to check API connectivity
    obs_log(LOG_INFO, "[17Live Core] Running startup network diagnostics...");
    NetworkDiagnostics::runStartupDiagnostics(ONESEVENLIVE_API_URL);

    // Initialize and start HTTP server
    // "html" is the path relative to obs_get_module_data_path()
    httpServer_ =
        std::make_unique<OneSevenLiveHttpServer>("localhost", 0, "html/chat", "17Live HTTP Server");
    if (!httpServer_) {
        obs_log(LOG_ERROR, "[17Live Core] Failed to create HTTP server instance");
        return false;
    }

    if (!httpServer_->start()) {
        obs_log(LOG_ERROR, "[17Live Core] Failed to start HTTP server");
        // Decide whether to interrupt the entire initialization due to HTTP server startup
        // failure based on requirements return false;
    } else {
        obs_log(LOG_INFO, "[17Live Core] HTTP server started successfully");
    }

    // Initialize and start WebSocket server
    websocketServer_ = std::make_shared<OneSevenLiveWebsocketServer>("localhost", 0);
    if (!websocketServer_) {
        obs_log(LOG_ERROR, "[17Live Core] Failed to create WebSocket server instance");
        return false;
    }

    if (!websocketServer_->start()) {
        obs_log(LOG_ERROR, "[17Live Core] Failed to start WebSocket server");
        // Continue initialization even if WebSocket server fails
    } else {
        obs_log(LOG_INFO, "[17Live Core] WebSocket server started successfully on port %d",
                websocketServer_->getPort());
    }

    // Note: Callbacks will be set up by CoreManager after this returns true,
    // to avoid coupling ChatBridge specifics into this class.
    return true;
}

void LocalGatewayService::shutdownLocalServers() {
    // Stop WebSocket server
    if (websocketServer_) {
        websocketServer_->stop();
        obs_log(LOG_INFO, "[17Live Core] WebSocket server stopped");
    }

    // Stop HTTP server
    if (httpServer_) {
        // Stop synchronous to ensure clean shutdown before destroying other resources
        httpServer_->stop();
        obs_log(LOG_INFO, "[17Live Core] HTTP server stopped");
    }
}

OneSevenLiveHttpServer* LocalGatewayService::getHttpServer() const {
    return httpServer_.get();
}

OneSevenLiveWebsocketServer* LocalGatewayService::getWebsocketServer() const {
    return websocketServer_.get();
}
