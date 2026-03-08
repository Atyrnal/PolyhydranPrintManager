#include "headers/ftpsclient.h"

size_t FtpsClient::write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    return fwrite(ptr, size, nmemb, static_cast<FILE*>(stream));
}

FtpsClient::FtpsClient(QObject *parent) : QObject(parent) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

FtpsClient::~FtpsClient() {
    curl_global_cleanup();
}

size_t FtpsClient::readCallback(void *ptr, size_t size, size_t nmemb, void *stream) {
    QFile *file = static_cast<QFile*>(stream);
    qint64 bytesRead = file->read(static_cast<char*>(ptr), size * nmemb);
    return bytesRead < 0 ? 0 : static_cast<size_t>(bytesRead);
}

int FtpsClient::progressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    FtpsClient *uploader = static_cast<FtpsClient*>(clientp);
    emit uploader->progress(ulnow, ultotal);
    return 0;
}

void FtpsClient::uploadFile(const QString &localFile, const QString &host, const QString &username, const QString &password, const QString &remotePath) {
    m_file = new QFile(localFile);
    if (!m_file->open(QIODevice::ReadOnly)) {
        emit finished(false, "Cannot open file: " + localFile);
        return;
    }

    m_curl = curl_easy_init();
    if (!m_curl) {
        emit finished(false, "Failed to init libcurl");
        m_file->close();
        return;
    }

    QString url = QString("ftps://%1:990/%2").arg(host, remotePath); //Maybe specify magic number port 990?

    curl_easy_setopt(m_curl, CURLOPT_URL, url.toUtf8().constData());
    curl_easy_setopt(m_curl, CURLOPT_USERNAME, username.toUtf8().constData());
    curl_easy_setopt(m_curl, CURLOPT_PASSWORD, password.toUtf8().constData());

    // Implicit TLS
    curl_easy_setopt(m_curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    curl_easy_setopt(m_curl, CURLOPT_FTP_SSL_CCC, CURLFTPSSL_CCC_NONE);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // Read data from file
    curl_easy_setopt(m_curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(m_curl, CURLOPT_READDATA, m_file);
    curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, &FtpsClient::readCallback);

    // Progress
    curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, &FtpsClient::progressCallback);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFODATA, this);

    // Enable verbose for debug
    // curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);

    // Perform upload
    CURLcode res = curl_easy_perform(m_curl);
    if (res != CURLE_OK) {
        m_errorString = curl_easy_strerror(res);
        emit finished(false, m_errorString);
    } else {
        emit finished(true, "");
    }

    m_file->close();
    curl_easy_cleanup(m_curl);
    m_curl = nullptr;
}
