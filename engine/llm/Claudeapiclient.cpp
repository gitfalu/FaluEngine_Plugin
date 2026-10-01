#include "Claudeapiclient.h"
#include "core/Logger.h"

#include <nlohmann/json.hpp>
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib,"winhttp.lib")

namespace FaluEngine
{
	namespace 
	{
		constexpr wchar_t kHost[] = L"api.anthropic.com";
		constexpr wchar_t kPath[] = L"/v1/messages";
		constexpr wchar_t kAnthropicVersion[] = L"2023-06-01";

		std::wstring toWide(const std::string& s)
		{
			if (s.empty()) return {};
			int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
			std::wstring result(len, L'\0');
			MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len);
			return result;
		}

		std::string toUtf8(const std::wstring& s)
		{
			if (s.empty()) return {};
			int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
			std::string result(len, '\0');
			WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len, nullptr, nullptr);
			return result;
		}

		std::string base64Encode(const std::vector<uint8_t>& data)
		{
			static const char table[] =
				"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

			std::string out;
			out.reserve(((data.size() + 2) / 3) * 4);

			size_t i = 0;
			while (i + 3 <= data.size())
			{
				uint32_t chunk = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
				out.push_back(table[(chunk >> 18) & 0x3F]);
				out.push_back(table[(chunk >> 12) & 0x3F]);
				out.push_back(table[(chunk >> 6) & 0x3F]);
				out.push_back(table[chunk & 0x3F]);
				i += 3;
			}

			const size_t remain = data.size() - i;
			if (remain == 1)
			{
				uint32_t chunk = data[i] << 16;
				out.push_back(table[(chunk >> 18) & 0x3F]);
				out.push_back(table[(chunk >> 12) & 0x3F]);
				out.push_back('=');
				out.push_back('=');
			}
			else if (remain == 2)
			{
				uint32_t chunk = (data[i] << 16 | data[i + 1] << 8);
				out.push_back(table[(chunk >> 18) & 0x3F]);
				out.push_back(table[(chunk >> 12) & 0x3F]);
				out.push_back(table[(chunk >> 6) & 0x3F]);
				out.push_back('=');
			}

			return out;
		}
	}


	ClaudeApiClient::ClaudeApiClient(std::string apikey, std::string model)
		: m_apiKey(std::move(apikey)),m_model(std::move(model))
	{

	}

	LLMDebugResponse ClaudeApiClient::sendDebugRequest(const LLMDebugRequest& request, const std::string& systemPrompt)
	{
		LLMDebugResponse response;

		if (m_apiKey.empty())
		{
			response.error = "ClaudeApiClient: API key is empty";
			LOG_ERROR("{}", response.error);
			return response;
		}

		nlohmann::json contentArray = nlohmann::json::array();

		for (auto& img : request.images)
		{
			contentArray.push_back(
				{
					{"type","image"},
				{"source",{
					{"type","base64"},
					{"media_type","image/png"},
					{"data",base64Encode(img.pngData)}
				}}
				});
		}

		contentArray.push_back({
			{"type","text"},
			{"text",request.prompt}
			});

		nlohmann::json body = {
			{"model",m_model},
			{"max_tokens",2048},
			{"messages",nlohmann::json::array({
				{
				{"role","user"},
				{"content",contentArray}
				}
			}) }
		};

		if (!systemPrompt.empty())
		{
			body["system"] = systemPrompt;
		}

		const std::string bodyStr = body.dump();

		HINTERNET hSession = WinHttpOpen(
			L"FaluEngine-LLMDebugAssistant/1.0",
			WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS,
			0
		);

		if (!hSession)
		{
			response.error = "ClaudeApiClient: WinHttpOpen failed.";
			LOG_ERROR("{}", response.error);
			return response;
		}

		HINTERNET hConnect = WinHttpConnect(hSession, kHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
		if (!hConnect)
		{
			response.error = "ClaudeApiClient: WinHttpConnect failed.";
			LOG_ERROR("{}", response.error);
			WinHttpCloseHandle(hSession);
			return response;
		}

		HINTERNET hRequest = WinHttpOpenRequest(
			hConnect, L"POST", kPath, nullptr,
			WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			WINHTTP_FLAG_SECURE
		);

		if (!hRequest)
		{
			response.error = "ClaudeApiClient: WinHttpOpenRequest failed.";
			LOG_ERROR("{}", response.error);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return response;
		}

		std::wstring headers;
		headers += L"Content-Type: application/json\r\n";
		headers += L"x-api-key: " + toWide(m_apiKey) + L"\r\n";
		headers += kAnthropicVersion;
		headers += L"\r\n";

		BOOL sent = WinHttpSendRequest(
			hRequest,
			headers.c_str(), static_cast<DWORD>(headers.size()),
			(LPVOID)bodyStr.data(), static_cast<DWORD>(bodyStr.size()),
			static_cast<DWORD>(bodyStr.size()), 0);

		bool ok = sent && WinHttpReceiveResponse(hRequest, nullptr);

		std::string responseBody;
		if (ok)
		{
			DWORD available = 0;
			do
			{
				available = 0;
				if (!WinHttpQueryDataAvailable(hRequest, &available)) break;
				if (available == 0) break;

				std::string chunk(available, '\0');
				DWORD read = 0;
				if (!WinHttpReadData(hRequest, chunk.data(), available, &read)) break;

				chunk.resize(read);
				responseBody += chunk;
			} while (available > 0);
		}

		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);

		if (ok)
		{
			response.error = "ClaudeApiClient: HTTP request failed(network error).";
			LOG_ERROR("{}", response.error);
			return response;
		}

		nlohmann::json parsed;
		try
		{
			parsed = nlohmann::json::parse(responseBody);
		}
		catch(const std::exception& e)
		{
			response.error = std::string("CaludeApiClient: failed to parse response JSON: ") + e.what();
			LOG_ERROR("{}", response.error);
			return response;
		}

		if (parsed.contains("error"))
		{
			response.error = "ClaudeApiClient: API error: " +
				parsed["error"].value("message", "unknown error");
			LOG_ERROR("{}", response.error);
			return response;
		}

		if (!parsed.contains("content") || !parsed["content"].is_array())
		{
			response.error = "CaludeApiClient: unexpected response shape (no content array).";
			LOG_ERROR("{}", response.error);
			return response;
		}

		std::string text;
		for (auto& block : parsed["content"])
		{
			if (block.value("type", "") == text)
			{
				text += block.value("text", "");
			}
		}

		response.success = true;
		response.text = text;
		return response;	
	}

}
