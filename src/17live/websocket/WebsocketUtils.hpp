#pragma once

#include <QString>
#include <nlohmann/json.hpp>
#include <string>

class OneSevenLiveWebsocketServer;
class OneSevenLiveCoreManager;
struct WsMessage;

void wsBroadcast(OneSevenLiveWebsocketServer* server, const WsMessage& msg);
void wsBroadcast(const QString& type, const nlohmann::json& payload);
std::string generateWebSocketKey();