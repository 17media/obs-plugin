#include "SeventeenLiveApiWrappers.hpp"

#include "../utility/RemoteTextThread.hpp"

#include <QFile>
#include <QMimeDatabase>

#include <obs-module.h>
#include "plugin-support.h"

#include <QCryptographicHash>

using namespace json11;

namespace seventeenlive {

#define SEVENTEENLIVE_API_URL "https://sta-wap-api.17app.co"

// 登录接口: SEVENTEENLIVE_API_URL + "/api/v1/auth/loginAction"
#define SEVENTEENLIVE_LOGIN_URL SEVENTEENLIVE_API_URL "/api/v1/auth/loginAction"

SeventeenLiveApiWrappers::SeventeenLiveApiWrappers() {}

bool SeventeenLiveApiWrappers::TryInsertCommand(const char *url, const char *content_type, std::string request_type,
  const char *data, Json &json_out, long *error_code, int data_size, bool token_required)
{
  long httpStatusCode = 0;

#ifdef _DEBUG
  obs_log(LOG_DEBUG, "17Live API command URL: %s", url);
  
  if (data && data[0] == '{') // only log JSON data
    obs_log(LOG_DEBUG, "17Live API command data: %s", data);
#endif

  if (token_required && token.empty())
    return false;

  std::string output;
  std::string error;
  // Increase timeout by the time it takes to transfer `data_size` at 1 Mbps
  int timeout = 60 + data_size / 125000;
  bool success = GetRemoteFile(url, output, error, &httpStatusCode, content_type,request_type, data, {"Authorization: Bearer " + token}, nullptr, timeout, false, data_size);
  if (error_code)
    *error_code = httpStatusCode;

  if (!success || output.empty()) {
    if (!error.empty())
      obs_log(LOG_WARNING, "17Live API request failed: %s", error.c_str());
    return false;
  }

  json_out = Json::parse(output, error);
#ifdef _DEBUG
  obs_log(LOG_DEBUG, "17Live API command answer: %s", json_out.dump().c_str());
#endif
  if (!error.empty()) {
    return false;
  }
  return httpStatusCode < 400;
}

bool SeventeenLiveApiWrappers::UpdateAccessToken()
{
  obs_log(LOG_INFO, "Updating access token");
  // TODO: implement
  return false;
}

bool SeventeenLiveApiWrappers::InsertCommand(const char *url, const char *content_type, std::string request_type, const char *data, Json &json_out, int data_size, bool token_required)
{
  long error_code;
  std::string error;
  bool success = TryInsertCommand(url, content_type, request_type, data, json_out, &error_code, data_size, token_required);

  if (error_code == 401) {
    // Attempt to update access token and try again
    if (!UpdateAccessToken())
      return false;
    success = TryInsertCommand(url, content_type, request_type, data, json_out, &error_code, data_size);
  }

  if (json_out.object_items().find("error") != json_out.object_items().end()) {
    obs_log(LOG_ERROR, "17Live API error:\n\tHTTP status: %ld\n\tURL: %s\n\tJSON: %s", error_code, url, json_out.dump().c_str());

    Json json_out_data = Json::parse(json_out["data"].string_value(), error);
    lastErrorMessage = QString::fromStdString(json_out_data["message"].string_value());
    
    // The existence of an error implies non-success even if the HTTP status code disagrees.
    success = false;
  }
  return success;
}

bool SeventeenLiveApiWrappers::Login(const QString &username, const QString &password, SeventeenLiveLoginData &loginData)
{
	lastErrorMessage.clear();

	const QByteArray url = SEVENTEENLIVE_LOGIN_URL;
  // TODO: language
  // const char *obs_get_locale(void)
	const Json data = Json::object{
    {"language", "TW"},
    {"openID", username.toStdString()},
    {"password", md5(password).toStdString()},
  };
  std::string error;
	Json json_out;
	if (!InsertCommand(url, "application/json", "", data.dump().c_str(), json_out, 0, false)) {
		return false;
	}
  obs_log(LOG_INFO, "Login success");

  // transform string json_out["data"] to Json
  Json json_out_data = Json::parse(json_out["data"].string_value(), error);


  // check if json_out_data contains "result" key
  auto items = json_out_data.object_items();
  if (items.find("result") == items.end()) {
    // check if json_out_data "result" equal to "fail"
  	if (json_out_data["result"].string_value() == "fail") {
  		lastErrorMessage = QString(json_out_data["message"].string_value().c_str());
  		return false;
  	}

    // TODO: handle other cases
    obs_log(LOG_WARNING, "Unknown login result: %s", json_out_data.dump().c_str());
    return false;
  }

  loginData.accessToken = QString(json_out_data["accessToken"].string_value().c_str());
  // TODO: handle other information

	return loginData.accessToken.isEmpty() ? false : true;
}

QString SeventeenLiveApiWrappers::md5(const QString& str)
{
  QByteArray input = str.toUtf8();
  QByteArray hash = QCryptographicHash::hash(input, QCryptographicHash::Md5);
  return QString(hash.toHex());
}

} // namespace seventeenlive
