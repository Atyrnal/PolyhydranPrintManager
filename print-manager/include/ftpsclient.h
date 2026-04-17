#ifndef FTPSCLIENT_H
#define FTPSCLIENT_H

#include <QObject>
#include <curl/curl.h>
#include <QFile>

class FtpsClient : public QObject {
    Q_OBJECT
public:
    explicit FtpsClient(QObject* parent = 0);
    ~FtpsClient();
    void uploadFile(const QString &localFile, const QString &host, const QString &username, const QString &password, const QString &remotePath);
signals:
    void progress(qint64 bytesSent, qint64 totalBytes);
    void finished(bool success, const QString &errorString);
private:
    size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream);
    static size_t readCallback(void *ptr, size_t size, size_t nmemb, void *stream);
    static int progressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

    QFile* m_file;
    CURL* m_curl = nullptr;
    QString m_errorString;
};

#endif // FTPSCLIENT_H
