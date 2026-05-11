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

#pragma once

#include <QThread>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

class RemoteTextThread : public QThread {
    Q_OBJECT

    std::string url;
    std::string contentType;
    std::string postData;

    std::vector<std::string> extraHeaders;

    int timeoutSec = 0;
    bool isImageRequest = false;
    std::atomic<bool> *externalCancel = nullptr;

    void run() override;

   signals:
    void Result(const QString &text, const QString &error);
    void ImageResult(const QByteArray &imageData, const QString &error);

   public:
    inline RemoteTextThread(std::string url_, std::string contentType_ = std::string(),
                            std::string postData_ = std::string(), int timeoutSec_ = 0,
                            bool isImageRequest_ = false)
        : url(url_),
          contentType(contentType_),
          postData(postData_),
          timeoutSec(timeoutSec_),
          isImageRequest(isImageRequest_) {}

    inline RemoteTextThread(std::string url_, std::vector<std::string> &&extraHeaders_,
                            std::string contentType_ = std::string(),
                            std::string postData_ = std::string(), int timeoutSec_ = 0,
                            bool isImageRequest_ = false)
        : url(url_),
          contentType(contentType_),
          postData(postData_),
          extraHeaders(std::move(extraHeaders_)),
          timeoutSec(timeoutSec_),
          isImageRequest(isImageRequest_) {}

    inline RemoteTextThread(std::string url_, std::vector<std::string> &&extraHeaders_,
                            std::string contentType_, std::string postData_, int timeoutSec_,
                            bool isImageRequest_, std::atomic<bool> *externalCancel_)
        : url(url_),
          contentType(contentType_),
          postData(postData_),
          extraHeaders(std::move(extraHeaders_)),
          timeoutSec(timeoutSec_),
          isImageRequest(isImageRequest_),
          externalCancel(externalCancel_) {}

    inline RemoteTextThread(std::string url_, std::string contentType_, std::string postData_,
                            int timeoutSec_, bool isImageRequest_,
                            std::atomic<bool> *externalCancel_)
        : url(url_),
          contentType(contentType_),
          postData(postData_),
          timeoutSec(timeoutSec_),
          isImageRequest(isImageRequest_),
          externalCancel(externalCancel_) {}

    void cancel() {
        m_isCancelled.store(true);
    }

   private:
    std::atomic<bool> m_isCancelled{false};
};

bool GetRemoteFile(const char *url, std::string &str, std::string &error,
                   long *responseCode = nullptr, const char *contentType = nullptr,
                   std::string request_type = "", const char *postData = nullptr,
                   std::vector<std::string> extraHeaders = std::vector<std::string>(),
                   std::string *signature = nullptr, int timeoutSec = 0, bool fail_on_error = true,
                   int postDataSize = 0, std::atomic<bool> *cancelFlag = nullptr);

bool UploadMultipartFile(const char *url, const char *fieldName, const std::string &filePath,
                         std::string &str, std::string &error, long *responseCode = nullptr,
                         std::vector<std::string> extraHeaders = std::vector<std::string>(),
                         int timeoutSec = 0, std::atomic<bool> *cancelFlag = nullptr);

using UploadProgressCallback = std::function<void(int64_t totalBytes, int64_t uploadedBytes)>;

bool UploadMultipartFileWithProgress(const char *url, const char *fieldName,
                                     const std::string &filePath, std::string &str,
                                     std::string &error, long *responseCode,
                                     std::vector<std::string> extraHeaders, int timeoutSec,
                                     std::atomic<bool> *cancelFlag,
                                     UploadProgressCallback uploadProgress);
