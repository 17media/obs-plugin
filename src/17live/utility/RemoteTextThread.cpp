/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "RemoteTextThread.hpp"

#include <obs.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QUrl>

#include <cctype>
#include <cstring>
#include <functional>

#include "curl-helper.h"
#include "moc_RemoteTextThread.cpp"
#include "plugin-support.h"

using namespace std;

static auto curl_deleter = [](CURL *curl) { curl_easy_cleanup(curl); };

using Curl = unique_ptr<CURL, decltype(curl_deleter)>;

static size_t string_write(char *ptr, size_t size, size_t nmemb, string &str) {
    size_t total = size * nmemb;
    if (total)
        str.append(ptr, total);

    return total;
}

static size_t binary_write(char *ptr, size_t size, size_t nmemb, std::vector<char> &data) {
    size_t total = size * nmemb;
    if (total) {
        size_t current_size = data.size();
        data.resize(current_size + total);
        memcpy(data.data() + current_size, ptr, total);
    }
    return total;
}

static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                             curl_off_t ultotal, curl_off_t ulnow) {
    (void) dltotal;
    (void) dlnow;
    (void) ultotal;
    (void) ulnow;

    if (clientp) {
        std::atomic<bool> *cancelled = static_cast<std::atomic<bool> *>(clientp);
        if (cancelled->load()) {
            return 1;  // Return non-zero to abort transfer
        }
    }
    return 0;
}

struct UploadProgressContext {
    std::atomic<bool> *cancelled = nullptr;
    UploadProgressCallback cb;
};

static int progress_callback_upload(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                                    curl_off_t ultotal, curl_off_t ulnow) {
    (void) dltotal;
    (void) dlnow;

    if (!clientp) {
        return 0;
    }

    auto *ctx = static_cast<UploadProgressContext *>(clientp);
    if (ctx->cancelled && ctx->cancelled->load()) {
        return 1;
    }

    if (ctx->cb && ultotal > 0) {
        ctx->cb(static_cast<int64_t>(ultotal), static_cast<int64_t>(ulnow));
    }

    return 0;
}

void RemoteTextThread::run() {
    char error[CURL_ERROR_SIZE];
    CURLcode code;
    error[0] = 0;

    string versionString("User-Agent: obs-basic ");
    versionString += obs_get_version_string();

    string contentTypeString;
    if (!contentType.empty()) {
        contentTypeString += "Content-Type: ";
        contentTypeString += contentType;
    }

    Curl curl{curl_easy_init(), curl_deleter};
    if (curl) {
        struct curl_slist *header = nullptr;
        string str;
        std::vector<char> binary_data;

        header = curl_slist_append(header, versionString.c_str());

        if (!contentTypeString.empty()) {
            header = curl_slist_append(header, contentTypeString.c_str());
        }

        for (std::string &h : extraHeaders)
            header = curl_slist_append(header, h.c_str());

        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, error);
        curl_easy_setopt(curl.get(), CURLOPT_FAILONERROR, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);

        if (isImageRequest) {
            curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, binary_write);
            curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &binary_data);
        } else {
            curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, string_write);
            curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &str);
        }

        curl_obs_set_revoke_setting(curl.get());

        if (timeoutSec)
            curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, timeoutSec);
        if (connectTimeoutSec)
            curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, connectTimeoutSec);

        if (!postData.empty()) {
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, postData.c_str());
        }

        curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, progress_callback);
        if (externalCancel) {
            curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, externalCancel);
        } else {
            curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &m_isCancelled);
        }
        curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);

        code = curl_easy_perform(curl.get());
        long httpCode = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &httpCode);

        if (m_isCancelled.load()) {
            // If cancelled, don't emit results
            curl_slist_free_all(header);
            return;
        }

        if (code != CURLE_OK) {
            obs_log(LOG_WARNING, "RemoteTextThread request failed: %s [url: %s, http: %ld]",
                    strlen(error) ? error : curl_easy_strerror(code), url.c_str(), httpCode);
            if (isImageRequest) {
                emit ImageResult(QByteArray(), QString::fromUtf8(error));
            } else {
                emit Result(QString(), QString::fromUtf8(error));
            }
        } else {
            if (httpCode >= 400) {
                const QString httpError = QString("HTTP %1").arg(httpCode);
                obs_log(LOG_WARNING, "RemoteTextThread image request returned %ld [url: %s]",
                        httpCode, url.c_str());
                if (isImageRequest) {
                    emit ImageResult(QByteArray(), httpError);
                } else {
                    emit Result(QString(), httpError);
                }
                curl_slist_free_all(header);
                return;
            }
            if (isImageRequest) {
                if (binary_data.empty()) {
                    obs_log(LOG_WARNING, "RemoteTextThread image request returned empty body [url: %s]",
                            url.c_str());
                    emit ImageResult(QByteArray(), QStringLiteral("Empty image response"));
                    curl_slist_free_all(header);
                    return;
                }
                QByteArray imageData(binary_data.data(), binary_data.size());
                emit ImageResult(imageData, QString());
            } else {
                emit Result(QString::fromUtf8(str.c_str()), QString());
            }
        }

        curl_slist_free_all(header);
    }
}

static size_t header_write(char *ptr, size_t size, size_t nmemb, vector<string> &list) {
    string str;

    size_t total = size * nmemb;
    if (total)
        str.append(ptr, total);

    if (str.back() == '\n')
        str.resize(str.size() - 1);
    if (str.back() == '\r')
        str.resize(str.size() - 1);

    list.push_back(std::move(str));
    return total;
}

static std::string find_header_value(const vector<string> &headers, const char *header_name) {
    if (!header_name) {
        return {};
    }

    const std::string prefix = std::string(header_name) + ":";
    for (const auto &header : headers) {
        if (header.size() < prefix.size()) {
            continue;
        }

        bool matches = true;
        for (size_t i = 0; i < prefix.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(header[i])) !=
                std::tolower(static_cast<unsigned char>(prefix[i]))) {
                matches = false;
                break;
            }
        }

        if (!matches) {
            continue;
        }

        size_t value_pos = prefix.size();
        while (value_pos < header.size() &&
               std::isspace(static_cast<unsigned char>(header[value_pos]))) {
            ++value_pos;
        }
        return header.substr(value_pos);
    }

    return {};
}

namespace {
struct CurlRequest {
    const char *url = nullptr;
    const char *contentType = nullptr;
    std::string requestType;
    const char *postData = nullptr;
    std::vector<std::string> extraHeaders;
    std::string *signature = nullptr;
    int timeoutSec = 0;
    int connectTimeoutSec = 0;
    bool failOnError = true;
    int postDataSize = 0;
    std::atomic<bool> *cancelFlag = nullptr;
};

struct CurlResponse {
    bool ok = false;
    std::string body;
    std::string error;
    long httpCode = 0;
    std::string signature;
    CURLcode curlCode = CURLE_FAILED_INIT;
};

enum class CurlWorkerBucket {
    SeventeenLive,
    ThirdParty,
};

QString GetUrlHost(const char *url) {
    if (!url) {
        return QString();
    }

    return QUrl(QString::fromUtf8(url)).host().toLower();
}

bool IsSeventeenLiveHost(const QString &host) {
    return host.endsWith(".17app.co") || host == "17app.co" || host.endsWith(".17.live") ||
           host == "17.live";
}

CurlWorkerBucket ClassifyWorkerBucket(const char *url) {
    return IsSeventeenLiveHost(GetUrlHost(url)) ? CurlWorkerBucket::SeventeenLive
                                                : CurlWorkerBucket::ThirdParty;
}

class CurlSingleThreadWorker : public QObject {
   public:
    CurlSingleThreadWorker() : curl(curl_easy_init()) {}
    ~CurlSingleThreadWorker() override {
        if (curl) {
            curl_easy_cleanup(curl);
            curl = nullptr;
        }
    }

    void perform(const CurlRequest &req, CurlResponse &resp) {
        resp = CurlResponse{};
        if (!curl || !req.url) {
            resp.ok = false;
            resp.curlCode = CURLE_FAILED_INIT;
            return;
        }

        curl_easy_reset(curl);

        const bool isUserNoteEndpoint =
            req.url && std::strstr(req.url, "/users/") && std::strstr(req.url, "/note");
        vector<string> header_in_list;
        char error_in[CURL_ERROR_SIZE];
        error_in[0] = 0;

        string versionString("User-Agent: obs-basic ");
        versionString += obs_get_version_string();

        string contentTypeString;
        if (req.contentType) {
            contentTypeString += "Content-Type: ";
            contentTypeString += req.contentType;
        }

        struct curl_slist *header = nullptr;
        header = curl_slist_append(header, versionString.c_str());

        if (!contentTypeString.empty()) {
            header = curl_slist_append(header, contentTypeString.c_str());
        }

        for (const std::string &h : req.extraHeaders)
            header = curl_slist_append(header, h.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, req.url);
        curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, isUserNoteEndpoint ? "identity" : "");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_in);
        if (req.failOnError)
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, string_write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
        curl_obs_set_revoke_setting(curl);

        if (req.signature || isUserNoteEndpoint) {
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_write);
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_in_list);
        }

        if (req.timeoutSec)
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, req.timeoutSec);
        if (req.connectTimeoutSec)
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, req.connectTimeoutSec);

        if (!req.requestType.empty()) {
            if (req.requestType != "GET")
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req.requestType.c_str());

            if (req.requestType == "POST") {
                curl_easy_setopt(curl, CURLOPT_POST, 1);
                if (!req.postData)
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{}");
            }
        }
        if (req.postData) {
            if (req.postDataSize > 0) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) req.postDataSize);
            }
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.postData);
        }

        if (req.cancelFlag) {
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, req.cancelFlag);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        }

        resp.curlCode = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.httpCode);

        if (isUserNoteEndpoint) {
            double dnsTimeSec = 0;
            double connectTimeSec = 0;
            double appConnectTimeSec = 0;
            double startTransferTimeSec = 0;
            double totalTimeSec = 0;
            const char *method = req.requestType.empty() ? "GET" : req.requestType.c_str();
            const std::string responseEncoding = find_header_value(header_in_list, "Content-Encoding");
            const std::string responseType = find_header_value(header_in_list, "Content-Type");

            curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME, &dnsTimeSec);
            curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME, &connectTimeSec);
            curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME, &appConnectTimeSec);
            curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &startTransferTimeSec);
            curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &totalTimeSec);

            obs_log(LOG_INFO,
                    "[UserMemo][HTTP] %s /note http=%ld curl=%d accept_encoding=identity "
                    "response_encoding=%s content_type=%s dns=%.0fms connect=%.0fms "
                    "tls=%.0fms ttfb=%.0fms total=%.0fms",
                    method, resp.httpCode, static_cast<int>(resp.curlCode),
                    responseEncoding.empty() ? "(none)" : responseEncoding.c_str(),
                    responseType.empty() ? "(unknown)" : responseType.c_str(), dnsTimeSec * 1000,
                    connectTimeSec * 1000, appConnectTimeSec * 1000, startTransferTimeSec * 1000,
                    totalTimeSec * 1000);
        }

        if (resp.curlCode != CURLE_OK) {
            resp.error = strlen(error_in) ? error_in : curl_easy_strerror(resp.curlCode);
        } else if (req.signature) {
            for (string &h : header_in_list) {
                string name = h.substr(0, 13);
                if (name == "X-Signature: " || name == "x-signature: ") {
                    resp.signature = h.substr(13);
                    break;
                }
            }
        }

        curl_slist_free_all(header);

        resp.ok = resp.curlCode == CURLE_OK;
    }

   private:
    CURL *curl = nullptr;
};

struct CurlWorkerPair {
    QThread *thread = nullptr;
    CurlSingleThreadWorker *worker = nullptr;
};

CurlSingleThreadWorker *GetCurlWorkerForBucket(CurlWorkerBucket bucket) {
    static QMutex mutex;
    static CurlWorkerPair seventeenLive;
    static CurlWorkerPair thirdParty;

    QMutexLocker locker(&mutex);

    auto &pair = bucket == CurlWorkerBucket::SeventeenLive ? seventeenLive : thirdParty;
    const char *threadName =
        bucket == CurlWorkerBucket::SeventeenLive ? "curl-worker-17live"
                                                  : "curl-worker-third-party";

    if (pair.worker) {
        return pair.worker;
    }

    pair.thread = new QThread();
    pair.thread->setObjectName(QString::fromUtf8(threadName));
    pair.worker = new CurlSingleThreadWorker();
    pair.worker->moveToThread(pair.thread);

    QObject::connect(pair.thread, &QThread::finished, pair.worker, &QObject::deleteLater);
    QObject::connect(pair.thread, &QThread::finished, pair.thread, &QObject::deleteLater);

    if (QCoreApplication::instance()) {
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, pair.thread,
                         [thread = pair.thread]() {
                             thread->quit();
                             thread->wait();
                         });
    }

    pair.thread->start();
    return pair.worker;
}

CurlSingleThreadWorker *GetCurlWorkerForUrl(const char *url) {
    return GetCurlWorkerForBucket(ClassifyWorkerBucket(url));
}
}  // namespace

bool GetRemoteFile(const char *url, std::string &str, std::string &error, long *responseCode,
                   const char *contentType, std::string request_type, const char *postData,
                   std::vector<std::string> extraHeaders, std::string *signature, int timeoutSec,
                   bool fail_on_error, int postDataSize, std::atomic<bool> *cancelFlag,
                   int connectTimeoutSec) {
    CurlRequest req;
    req.url = url;
    req.contentType = contentType;
    req.requestType = std::move(request_type);
    req.postData = postData;
    req.extraHeaders = std::move(extraHeaders);
    req.signature = signature;
    req.timeoutSec = timeoutSec;
    req.connectTimeoutSec = connectTimeoutSec;
    req.failOnError = fail_on_error;
    req.postDataSize = postDataSize;
    req.cancelFlag = cancelFlag;

    CurlResponse resp;

    CurlSingleThreadWorker *worker = GetCurlWorkerForUrl(url);
    if (!worker) {
        return false;
    }

    if (QThread::currentThread() == worker->thread()) {
        worker->perform(req, resp);
    } else {
        QMetaObject::invokeMethod(
            worker, [&]() { worker->perform(req, resp); }, Qt::BlockingQueuedConnection);
    }

    if (responseCode)
        *responseCode = resp.httpCode;
    str = std::move(resp.body);
    error = std::move(resp.error);
    if (signature)
        *signature = std::move(resp.signature);

    return resp.ok;
}

bool UploadMultipartFile(const char *url, const char *fieldName, const std::string &filePath,
                         std::string &str, std::string &error, long *responseCode,
                         std::vector<std::string> extraHeaders, int timeoutSec,
                         std::atomic<bool> *cancelFlag) {
    char error_in[CURL_ERROR_SIZE];
    CURLcode code = CURLE_FAILED_INIT;

    error_in[0] = 0;

    string versionString("User-Agent: obs-basic ");
    versionString += obs_get_version_string();

    Curl curl{curl_easy_init(), curl_deleter};
    if (curl) {
        struct curl_slist *header = nullptr;
        header = curl_slist_append(header, versionString.c_str());

        for (std::string &h : extraHeaders)
            header = curl_slist_append(header, h.c_str());

        curl_easy_setopt(curl.get(), CURLOPT_URL, url);
        curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, error_in);
        curl_easy_setopt(curl.get(), CURLOPT_FAILONERROR, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, string_write);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &str);
        curl_obs_set_revoke_setting(curl.get());

        if (timeoutSec)
            curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, timeoutSec);

        curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "POST");

        curl_mime *mime = curl_mime_init(curl.get());
        curl_mimepart *part = curl_mime_addpart(mime);
        curl_mime_name(part, fieldName);
        curl_mime_filedata(part, filePath.c_str());

        curl_easy_setopt(curl.get(), CURLOPT_MIMEPOST, mime);

        if (cancelFlag) {
            curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, progress_callback);
            curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, cancelFlag);
            curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
        }

        code = curl_easy_perform(curl.get());
        if (responseCode)
            curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, responseCode);

        if (code != CURLE_OK) {
            error = strlen(error_in) ? error_in : curl_easy_strerror(code);
        }

        curl_mime_free(mime);
        curl_slist_free_all(header);
    }

    return code == CURLE_OK;
}

bool UploadMultipartFileWithProgress(const char *url, const char *fieldName,
                                     const std::string &filePath, std::string &str,
                                     std::string &error, long *responseCode,
                                     std::vector<std::string> extraHeaders, int timeoutSec,
                                     std::atomic<bool> *cancelFlag,
                                     UploadProgressCallback uploadProgress) {
    char error_in[CURL_ERROR_SIZE];
    CURLcode code = CURLE_FAILED_INIT;

    error_in[0] = 0;

    string versionString("User-Agent: obs-basic ");
    versionString += obs_get_version_string();

    Curl curl{curl_easy_init(), curl_deleter};
    if (curl) {
        struct curl_slist *header = nullptr;
        header = curl_slist_append(header, versionString.c_str());

        for (std::string &h : extraHeaders)
            header = curl_slist_append(header, h.c_str());

        curl_easy_setopt(curl.get(), CURLOPT_URL, url);
        curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, error_in);
        curl_easy_setopt(curl.get(), CURLOPT_FAILONERROR, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, string_write);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &str);
        curl_obs_set_revoke_setting(curl.get());

        if (timeoutSec)
            curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, timeoutSec);

        curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "POST");

        curl_mime *mime = curl_mime_init(curl.get());
        curl_mimepart *part = curl_mime_addpart(mime);
        curl_mime_name(part, fieldName);
        curl_mime_filedata(part, filePath.c_str());

        curl_easy_setopt(curl.get(), CURLOPT_MIMEPOST, mime);

        UploadProgressContext ctx;
        ctx.cancelled = cancelFlag;
        ctx.cb = std::move(uploadProgress);

        curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, progress_callback_upload);
        curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &ctx);
        curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);

        code = curl_easy_perform(curl.get());
        if (responseCode)
            curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, responseCode);

        if (code != CURLE_OK) {
            error = strlen(error_in) ? error_in : curl_easy_strerror(code);
        }

        curl_mime_free(mime);
        curl_slist_free_all(header);
    }

    return code == CURLE_OK;
}
