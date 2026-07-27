#pragma once

#include "REX/BASE.h"
#include "REX/TScopeExit.h"
#include "REX/W32/BCRYPT.h"

namespace REX
{
	inline std::optional<std::string> SHA512(std::span<const std::byte> a_data)
	{
		REX::W32::BCRYPT_ALG_HANDLE algorithm;
		if (!REX::W32::NT_SUCCESS(REX::W32::BCryptOpenAlgorithmProvider(&algorithm, REX::W32::BCRYPT_SHA512_ALGORITHM)))
			return std::nullopt;

		const TScopeExit cleanup_algo([&]() {
			[[maybe_unused]] const auto success = REX::W32::NT_SUCCESS(REX::W32::BCryptCloseAlgorithmProvider(algorithm));
			assert(success);
		});

		REX::W32::BCRYPT_HASH_HANDLE hash;
		if (!REX::W32::NT_SUCCESS(REX::W32::BCryptCreateHash(algorithm, &hash)))
			return std::nullopt;

		const TScopeExit cleanup_hash([&]() {
			[[maybe_unused]] const auto success = REX::W32::NT_SUCCESS(REX::W32::BCryptDestroyHash(hash));
			assert(success);
		});

		if (!REX::W32::NT_SUCCESS(REX::W32::BCryptHashData(hash, reinterpret_cast<std::uint8_t*>(const_cast<std::byte*>(a_data.data())), static_cast<std::uint32_t>(a_data.size()))))
			return std::nullopt;

		std::uint32_t length{ 0 };
		std::uint32_t output{ 0 };
		if (!REX::W32::NT_SUCCESS(REX::W32::BCryptGetProperty(hash, REX::W32::BCRYPT_HASH_LENGTH, reinterpret_cast<std::uint8_t*>(&length), sizeof(length), &output)))
			return std::nullopt;

		std::vector<std::uint8_t> buffer(static_cast<std::size_t>(length));
		if (!REX::W32::NT_SUCCESS(REX::W32::BCryptFinishHash(hash, buffer.data(), static_cast<std::uint32_t>(buffer.size()))))
			return std::nullopt;

		std::string result;
		result.reserve(buffer.size() * 2);
		for (const auto byte : buffer) {
			result += std::format("{:02X}", byte);
		}

		return { std::move(result) };
	}
}
