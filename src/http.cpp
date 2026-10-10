#include "http.hpp"

#include <curl/curl.h>

static size_t onWrite(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	auto *buf = static_cast<QByteArray *>(userdata);
	buf->append(ptr, int(size * nmemb));
	return size * nmemb;
}

static int onProgress(void *clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
	const auto *cancel = static_cast<const std::atomic<bool> *>(clientp);
	return (cancel && cancel->load()) ? 1 : 0; // non-zero aborts the transfer
}

HttpResult httpsGet(const QString &url, const QByteArray &accept, const std::atomic<bool> *cancel, long timeoutSec)
{
	HttpResult r;
	CURL *curl = curl_easy_init();
	if (!curl) {
		r.error = "curl init failed";
		return r;
	}

	char errbuf[CURL_ERROR_SIZE] = {0};
	curl_slist *headers = nullptr;
	if (!accept.isEmpty())
		headers = curl_slist_append(headers, (QByteArray("Accept: ") + accept).constData());

	const QByteArray u = url.toUtf8();
	curl_easy_setopt(curl, CURLOPT_URL, u.constData());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "obs-elgato-lights-updater");
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
	curl_easy_setopt(curl, CURLOPT_PROTOCOLS, (long)CURLPROTO_HTTPS);
	curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, (long)CURLPROTO_HTTPS);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSec);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, (long)CURLSSLOPT_NATIVE_CA);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, onWrite);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &r.body);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, onProgress);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, const_cast<std::atomic<bool> *>(cancel));
	if (headers)
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	const CURLcode rc = curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &r.status);
	if (rc != CURLE_OK) {
		r.error = errbuf[0] ? QString::fromUtf8(errbuf) : QString::fromUtf8(curl_easy_strerror(rc));
	} else if (r.status < 200 || r.status >= 300) {
		r.error = QString("HTTP %1").arg(r.status);
	} else {
		r.ok = true;
	}

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return r;
}
