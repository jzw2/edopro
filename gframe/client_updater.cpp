#include "client_updater.h"
#ifdef UPDATE_URL
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#endif // _WIN32
#if defined(__MINGW32__) && defined(UNICODE)
#include <fcntl.h>
#include <ext/stdio_filebuf.h>
#endif
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <fstream>
#include <atomic>
#include <openssl/md5.h>
#include "logging.h"
#include "config.h"
#include "utils.h"
#ifdef __ANDROID__
#include "Android/porting_android.h"
#endif
#include "game_config.h"

#define LOCKFILE EPRO_TEXT("./.edopro_lock")

using md5array = std::array<uint8_t, MD5_DIGEST_LENGTH>;

struct WritePayload {
	std::vector<char>* outbuffer = nullptr;
	std::ostream* outfstream = nullptr;
	MD5_CTX* md5context = nullptr;
};

struct Payload {
	update_callback callback = nullptr;
	int current = 1;
	int total = 1;
	bool is_new = true;
	int previous_percent = 0;
	void* payload = nullptr;
	const char* filename = nullptr;
};

static int progress_callback(void* ptr, curl_off_t TotalToDownload, curl_off_t NowDownloaded, curl_off_t TotalToUpload, curl_off_t NowUploaded) {
	Payload* payload = static_cast<Payload*>(ptr);
	if(payload && payload->callback) {
		int percentage = 0;
		if(TotalToDownload > 0.0) {
			double fractiondownloaded = (double)NowDownloaded / (double)TotalToDownload;
			percentage = (int)std::round(fractiondownloaded * 100);
		}
		if(percentage != payload->previous_percent) {
			payload->callback(percentage, payload->current, payload->total, payload->filename, payload->is_new, payload->payload);
			payload->is_new = false;
			payload->previous_percent = percentage;
		}
	}
	return 0;
}

static size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	auto payload = static_cast<WritePayload*>(userp);
	auto buff = payload->outbuffer;
	if(buff) {
		size_t prev_size = buff->size();
		buff->resize(prev_size + realsize);
		memcpy((char*)buff->data() + prev_size, contents, realsize);
	}
	if(payload->outfstream)
		payload->outfstream->write(contents, realsize);
	if(payload->md5context)
		MD5_Update(payload->md5context, contents, realsize);
	return realsize;
}

static CURLcode curlPerform(const char* url, void* payload, void* payload2 = nullptr) {
	char curl_error_buffer[CURL_ERROR_SIZE];
	CURL* curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, curl_error_buffer);
	curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl_handle, CURLOPT_URL, url);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 60L);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, payload);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, ygo::Utils::GetUserAgent().data());
	curl_easy_setopt(curl_handle, CURLOPT_NOPROXY, "*");
	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl_handle, CURLOPT_DNS_CACHE_TIMEOUT, 0);
	curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, progress_callback);
	curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, payload2);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
	if(ygo::gGameConfig->ssl_certificate_path.size()
	   && ygo::Utils::FileExists(ygo::Utils::ToPathString(ygo::gGameConfig->ssl_certificate_path)))
		curl_easy_setopt(curl_handle, CURLOPT_CAINFO, ygo::gGameConfig->ssl_certificate_path.data());
#ifdef _WIN32
	else
		curl_easy_setopt(curl_handle, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
#endif
	CURLcode res = curl_easy_perform(curl_handle);
	curl_easy_cleanup(curl_handle);
	if(res != CURLE_OK && ygo::gGameConfig->logDownloadErrors)
		ygo::ErrorLog("Curl error: ({}) {} ({})", res, curl_easy_strerror(res), curl_error_buffer);
	return res;
}

static bool CheckMd5(std::istream& instream, const md5array& md5) {
	MD5_CTX context{};
	MD5_Init(&context);
	std::array<char, 512> buff;
	while(!instream.eof()) {
		instream.read(buff.data(), buff.size());
		MD5_Update(&context, buff.data(), static_cast<size_t>(instream.gcount()));
	}
	md5array result;
	MD5_Final(result.data(), &context);
	return result == md5;
}

#ifndef __ANDROID__
static inline void DeleteOld() {
#if defined(_WIN32) || (defined(__linux__) && !defined(__ANDROID__))
	ygo::Utils::FileDelete(fmt::format(EPRO_TEXT("{}.old"), ygo::Utils::GetExePath()));
#if !defined(__linux__)
	ygo::Utils::FileDelete(fmt::format(EPRO_TEXT("{}.old"), ygo::Utils::GetCorePath()));
#endif
#endif
}

static inline ygo::ClientUpdater::lock_type GetLock() {
	ygo::ClientUpdater::lock_type file{};
#ifdef _WIN32
	file = CreateFile(LOCKFILE, GENERIC_READ,
					  0, nullptr, CREATE_ALWAYS,
					  FILE_ATTRIBUTE_HIDDEN, nullptr);
	if(file == INVALID_HANDLE_VALUE)
		return nullptr;
#else
	file = open(LOCKFILE, O_CREAT | O_CLOEXEC, S_IRWXU);
	if(file < 0 || flock(file, LOCK_EX | LOCK_NB) != 0) {
		close(file);
		return 0;
	}
#endif
	DeleteOld();
	return file;
}

static inline void FreeLock(ygo::ClientUpdater::lock_type lock) {
	if(!lock)
		return;
#ifdef _WIN32
	CloseHandle(lock);
#else
	flock(lock, LOCK_UN);
	close(lock);
#endif
	ygo::Utils::FileDelete(LOCKFILE);
}
#endif //__ANDROID__
#endif //UPDATE_URL

namespace ygo {

void ClientUpdater::StartUnzipper(unzip_callback callback, void* payload, const epro::path_string& src) {
#ifdef UPDATE_URL
#ifdef __ANDROID__
	porting::installUpdate(fmt::format("{}{}{}.apk", Utils::working_dir, src, update_urls.front().name));
#else
	if(Lock)
		std::thread(&ClientUpdater::Unzip, this, src, payload, callback).detach();
#endif
#endif
}

void ClientUpdater::CheckUpdates() {
#ifdef UPDATE_URL
	if(Lock)
		std::thread(&ClientUpdater::CheckUpdate, this).detach();
#endif
}

bool ClientUpdater::StartUpdate(update_callback callback, void* payload, const epro::path_string& dest) {
#ifdef UPDATE_URL
	if(!has_update || downloading || !Lock)
		return false;
	std::thread(&ClientUpdater::DownloadUpdate, this, dest, payload, callback).detach();
	return true;
#else
	return false;
#endif
}
#ifdef UPDATE_URL
void ClientUpdater::Unzip(epro::path_string src, void* payload, unzip_callback callback) {
	Utils::SetThreadName("Unzip");
#if defined(_WIN32) || (defined(__linux__) && !defined(__ANDROID__))
	const auto& path = ygo::Utils::GetExePath();
	ygo::Utils::FileMove(path, fmt::format(EPRO_TEXT("{}.old"), path));
#if !defined(__linux__)
	const auto& corepath = ygo::Utils::GetCorePath();
	ygo::Utils::FileMove(corepath, fmt::format(EPRO_TEXT("{}.old"), corepath));
#endif
#endif
	unzip_payload cbpayload{};
	UnzipperPayload uzpl;
	uzpl.payload = payload;
	uzpl.cur = -1;
	uzpl.tot = static_cast<int>(update_urls.size());
	cbpayload.payload = &uzpl;
	int i = 1;
	for(auto& file : update_urls) {
		uzpl.cur = i++;
		auto name = src + ygo::Utils::ToPathString(file.name);
		uzpl.filename = name.data();
		ygo::Utils::UnzipArchive(name, callback, &cbpayload);
	}
	Utils::Reboot();
}

#ifdef __ANDROID__
#define formatstr EPRO_TEXT("{}/{}.apk")
#else
#define formatstr EPRO_TEXT("{}/{}")
#endif

void ClientUpdater::DownloadUpdate(epro::path_string dest_path, void* payload, update_callback callback) {
	Utils::SetThreadName("Updater");
	downloading = true;
	Payload cbpayload{};
	cbpayload.callback = callback;
	cbpayload.total = static_cast<int>(update_urls.size());
	cbpayload.payload = payload;
	int i = 1;
	for(auto& file : update_urls) {
		auto name = fmt::format(formatstr, dest_path, ygo::Utils::ToPathString(file.name));
		cbpayload.current = i++;
		cbpayload.filename = file.name.data();
		cbpayload.is_new = true;
		cbpayload.previous_percent = -1;
		md5array binmd5;
		if(file.md5.size() != binmd5.size() * 2) {
			failed = true;
			continue;
		}
		try {
			for(size_t i = 0; i < binmd5.size(); i++) {
				uint8_t b = static_cast<uint8_t>(std::stoul(file.md5.substr(i * 2, 2), nullptr, 16));
				binmd5[i] = b;
			}
		} catch(...) {
			failed = true;
			continue;
		}
		{
#if defined(__MINGW32__) && defined(UNICODE)
			auto fd = _wopen(name.data(), _O_RDONLY | _O_BINARY);
			if(fd != -1) {
				__gnu_cxx::stdio_filebuf<char> b(fd, std::ios::in);
				std::istream stream(&b);
				if(!stream.fail() && CheckMd5(stream, binmd5))
					continue;
			}
#else
			std::ifstream stream(name, std::ifstream::binary);
			if(!stream.fail() && CheckMd5(stream, binmd5))
				continue;
#endif
		}
		if(!ygo::Utils::CreatePath(name)) {
			failed = true;
			continue;
		}
#if defined(__MINGW32__) && defined(UNICODE)
		auto fd = _wopen(name.data(), _O_WRONLY | _O_TRUNC | _O_CREAT | _O_BINARY);
		if(fd == -1) {
			failed = true;
			continue;
		}
		__gnu_cxx::stdio_filebuf<char> b(fd, std::ios::out);
		std::ostream stream(&b);
#else
		std::ofstream stream(name, std::ofstream::trunc | std::ofstream::binary);
#endif
		if(stream.fail()) {
			failed = true;
			continue;
		}
		WritePayload wpayload;
		wpayload.outfstream = &stream;
		MD5_CTX context{};
		wpayload.md5context = &context;
		MD5_Init(wpayload.md5context);
		if(curlPerform(file.url.data(), &wpayload, &cbpayload) != CURLE_OK) {
			failed = true;
			continue;
		}
		md5array md5;
		MD5_Final(md5.data(), &context);
		if(md5 != binmd5) {
			stream.close();
			Utils::FileDelete(name);
			failed = true;
			continue;
		}
	}
	downloaded = true;
}

void ClientUpdater::CheckUpdate() {
	Utils::SetThreadName("CheckUpdate");
	WritePayload payload{};
	std::vector<char> retrieved_data;
	payload.outbuffer = &retrieved_data;
	if(curlPerform(update_url.data(), &payload) != CURLE_OK)
		return;
	try {
		const auto j = nlohmann::json::parse(retrieved_data);
		if(!j.is_array())
			return;
		for(const auto& asset : j) {
			try {
				const auto& url = asset.at("url").get_ref<const std::string&>();
				const auto& name = asset.at("name").get_ref<const std::string&>();
				const auto& md5 = asset.at("md5").get_ref<const std::string&>();
				update_urls.emplace_back(DownloadInfo{ name, url, md5 });
			} catch(...) {}
		}
	}
	catch(...) { update_urls.clear(); }
	has_update = !!update_urls.size();
}
#endif

ClientUpdater::ClientUpdater(epro::path_stringview override_url) {
	(void)override_url;
#if defined(UPDATE_URL)
	if(override_url.size())
		update_url = Utils::ToUTF8IfNeeded(override_url);
#if !defined(__ANDROID__)
	Lock = GetLock();
#endif
#endif
}

ClientUpdater::~ClientUpdater() {
#if defined(UPDATE_URL) && !defined(__ANDROID__)
	FreeLock(Lock);
#endif
}

};
