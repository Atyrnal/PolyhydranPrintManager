// ftpsclient.cpp
#include "ftpsclient.h"
#include <QUrl>

FtpsClient::FtpsClient(QObject *parent) : QObject(parent) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    m_worker = new FtpsWorker(); // no parent — required for moveToThread
    m_thread = new QThread(this);
    m_worker->moveToThread(m_thread); // worker now lives on the thread

    // Wire worker signals to client signals (auto queued across thread boundary)
    connect(m_worker, &FtpsWorker::finished, this, &FtpsClient::finished);
    connect(m_worker, &FtpsWorker::progress, this, &FtpsClient::progress);

    // Wire client signal to worker slot — queued because they're on different threads
    connect(this, &FtpsClient::startUpload, m_worker, &FtpsWorker::doUpload);

    // Clean up worker when thread finishes
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_thread->start(); // thread runs, worker sits idle waiting for signals
}

FtpsClient::~FtpsClient() {
    m_thread->quit();
    m_thread->wait();
    curl_global_cleanup();
}

void FtpsClient::uploadFile(const QString &localFile, const QString &host,
                            const QString &username, const QString &password,
                            const QString &remotePath) {
    // Emitting this signal invokes doUpload on the worker thread via queued connection
    emit startUpload(localFile, host, username, password, remotePath);
}

size_t FtpsWorker::readCallback(void *ptr, size_t size, size_t nmemb, void *stream) {
    QFile *file = static_cast<QFile*>(stream);
    qint64 bytesRead = file->read(static_cast<char*>(ptr), size * nmemb);
    return bytesRead < 0 ? 0 : static_cast<size_t>(bytesRead);
}

int FtpsWorker::progressCallback(void *clientp, curl_off_t, curl_off_t,
                                 curl_off_t ultotal, curl_off_t ulnow) {
    FtpsWorker *worker = static_cast<FtpsWorker*>(clientp);
    emit worker->progress(ulnow, ultotal);
    return 0;
}

void FtpsWorker::doUpload(const QString &localFile, const QString &host,
                          const QString &username, const QString &password,
                          const QString &remotePath) {
    QFile file(localFile);
    if (!file.open(QIODevice::ReadOnly)) {
        emit finished(false, "Cannot open file: " + localFile);
        return;
    }

    m_curl = curl_easy_init();
    if (!m_curl) {
        emit finished(false, "Failed to init libcurl");
        return;
    }

    QByteArray encodedPath = QUrl::toPercentEncoding(remotePath, "/");

    QString url = QString("ftps://%1:990/%2").arg(host, QString::fromLatin1(encodedPath));

    curl_easy_setopt(m_curl, CURLOPT_URL, url.toUtf8().constData());
    curl_easy_setopt(m_curl, CURLOPT_USERNAME, username.toUtf8().constData());
    curl_easy_setopt(m_curl, CURLOPT_PASSWORD, password.toUtf8().constData());
    curl_easy_setopt(m_curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    curl_easy_setopt(m_curl, CURLOPT_FTP_SSL_CCC, CURLFTPSSL_CCC_NONE);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(m_curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(m_curl, CURLOPT_READDATA, &file);
    curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, &FtpsWorker::readCallback);
    curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, &FtpsWorker::progressCallback);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFODATA, this);

    CURLcode res = curl_easy_perform(m_curl);

    if (res != CURLE_OK) {
        emit finished(false, QString(curl_easy_strerror(res)) + " url: " +url);
    } else {
        emit finished(true, "");
    }

    file.close();
    curl_easy_cleanup(m_curl);
    m_curl = nullptr;
}