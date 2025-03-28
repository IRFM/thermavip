#include "VipHash.h"

/// @brief Read 16 bits integer from src in little endian order
VIP_ALWAYS_INLINE auto read_LE_16(const void* src) -> std::uint16_t
{
	std::uint16_t value = 0;
	memcpy(&value, src, sizeof(std::uint16_t));

#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
	value = qToLittleEndian(value);
#endif
	return value;
}
/// @brief Read 32 bits integer from src in little endian order
VIP_ALWAYS_INLINE auto read_LE_32(const void* src) -> std::uint32_t
{
	std::uint32_t value = 0;
	memcpy(&value, src, sizeof(std::uint32_t));
#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
	value = qToLittleEndian(value);
#endif
	return value;
}
/// @brief Read 64 bits integer from src in little endian order
VIP_ALWAYS_INLINE auto read_LE_64(const void* src) -> std::uint64_t
{
	std::uint64_t value = 0;
	memcpy(&value, src, sizeof(std::uint64_t));
#if Q_BYTE_ORDER != Q_LITTLE_ENDIAN
	value = qToLittleEndian(value);
#endif
	return value;
}

namespace detail
{

	/**
	 * Function builds an unsigned 64-bit value out of remaining bytes in a
	 * message, and pads it with the "final byte". This function can only be
	 * called if less than 8 bytes are left to read. The message should be "long",
	 * permitting Msg[ -3 ] reads.
	 *
	 * @param Msg Message pointer, alignment is unimportant.
	 * @param MsgLen Message's remaining length, in bytes; can be 0.
	 * @return Final byte-padded value from the message.
	 */

#define VIP_HU64(v) static_cast<uint64_t>(v)

	static VIP_ALWAYS_INLINE uint64_t kh_lpu64ec_l3(const uint8_t* const Msg, const size_t MsgLen)
	{
		const int ml8 = static_cast<int>(MsgLen * 8);
		if (MsgLen < 4) {
			const uint8_t* const Msg3 = Msg + MsgLen - 3;
			const uint64_t m = VIP_HU64(Msg3[0]) | VIP_HU64(Msg3[1]) << 8 | VIP_HU64(Msg3[2]) << 16;
			return (VIP_HU64(1) << ml8 | m >> (24 - ml8));
		}

		const uint64_t mh = read_LE_32(Msg + MsgLen - 4);
		const uint64_t ml = read_LE_32(Msg);
		return (VIP_HU64(1) << ml8 | ml | (mh >> (64 - ml8)) << 32);
	}

	/**
	 * Function builds an unsigned 64-bit value out of remaining bytes in a
	 * message, and pads it with the "final byte". This function can only be
	 * called if less than 8 bytes are left to read. Can be used on "short"
	 * messages, but MsgLen should be greater than 0.
	 *
	 * @param Msg Message pointer, alignment is unimportant.
	 * @param MsgLen Message's remaining length, in bytes; cannot be 0.
	 * @return Final byte-padded value from the message.
	 */

	static VIP_ALWAYS_INLINE uint64_t kh_lpu64ec_nz(const uint8_t* const Msg, const size_t MsgLen)
	{
		const int ml8 = static_cast<int>(MsgLen * 8);
		if (MsgLen < 4) {
			uint64_t m = Msg[0];
			if (MsgLen > 1) {
				m |= VIP_HU64(Msg[1]) << 8;
				if (MsgLen > 2)
					m |= VIP_HU64(Msg[2]) << 16;
			}
			return (VIP_HU64(1) << ml8 | m);
		}

		const uint64_t mh = read_LE_32(Msg + MsgLen - 4);
		const uint64_t ml = read_LE_32(Msg);
		return (VIP_HU64(1) << ml8 | ml | (mh >> (64 - ml8)) << 32);
	}

	/**
	 * Function builds an unsigned 64-bit value out of remaining bytes in a
	 * message, and pads it with the "final byte". This function can only be
	 * called if less than 8 bytes are left to read. The message should be "long",
	 * permitting Msg[ -4 ] reads.
	 *
	 * @param Msg Message pointer, alignment is unimportant.
	 * @param MsgLen Message's remaining length, in bytes; can be 0.
	 * @return Final byte-padded value from the message.
	 */

	static VIP_ALWAYS_INLINE uint64_t kh_lpu64ec_l4(const uint8_t* const Msg, const size_t MsgLen)
	{
		const int ml8 = static_cast<int>(MsgLen * 8);
		if (MsgLen < 5) {
			const uint64_t m = read_LE_32(Msg + MsgLen - 4);
			return (VIP_HU64(1) << ml8 | m >> (32 - ml8));
		}
		const uint64_t m = read_LE_64(Msg + MsgLen - 8);
		return (VIP_HU64(1) << ml8 | m >> (64 - ml8));
	}

#define KOMIHASH_HASH16(m)                                                                                                                                                                             \
	vipUmul128(Seed1 ^ read_LE_64(m), Seed5 ^ read_LE_64(m + 8), &Seed1, &r1h);                                                                                                                    \
	Seed5 += r1h;                                                                                                                                                                                  \
	Seed1 ^= Seed5

#define KOMIHASH_HASHROUND()                                                                                                                                                                           \
	vipUmul128(Seed1, Seed5, &Seed1, &r2h);                                                                                                                                                        \
	Seed5 += r2h;                                                                                                                                                                                  \
	Seed1 ^= Seed5

#define KOMIHASH_HASHFIN()                                                                                                                                                                             \
	vipUmul128(r1h, r2h, &Seed1, &r1h);                                                                                                                                                            \
	Seed5 += r1h;                                                                                                                                                                                  \
	Seed1 ^= Seed5;                                                                                                                                                                                \
	KOMIHASH_HASHROUND();                                                                                                                                                                          \
	return static_cast<size_t>(Seed1)

	/**
	 * The hashing epilogue function (for internal use).
	 *
	 * @param Msg Pointer to the remaining part of the message.
	 * @param MsgLen Remaining part's length, can be zero.
	 * @param Seed1 Latest Seed1 value.
	 * @param Seed5 Latest Seed5 value.
	 * @return 64-bit hash value.
	 */

	static VIP_ALWAYS_INLINE uint64_t komihash_epi(const uint8_t* Msg, size_t MsgLen, uint64_t Seed1, uint64_t Seed5)
	{
		uint64_t r1h, r2h;

		if (VIP_LIKELY(MsgLen > 31)) {
			KOMIHASH_HASH16(Msg);
			KOMIHASH_HASH16(Msg + 16);
			Msg += 32;
			MsgLen -= 32;
		}
		if (MsgLen > 15) {
			KOMIHASH_HASH16(Msg);
			Msg += 16;
			MsgLen -= 16;
		}
		if (MsgLen > 7) {
			r2h = Seed5 ^ kh_lpu64ec_l4(Msg + 8, MsgLen - 8);
			r1h = Seed1 ^ read_LE_64(Msg);
		}
		else {
			r1h = Seed1 ^ kh_lpu64ec_l4(Msg, MsgLen);
			r2h = Seed5;
		}
		KOMIHASH_HASHFIN();
	}

	static VIP_ALWAYS_INLINE uint64_t komihash_long(const uint8_t* Msg, size_t MsgLen, uint64_t Seed1, uint64_t Seed5)
	{
		if (MsgLen > 63) {
			uint64_t Seed2 = 1354286222620113816ull;
			uint64_t Seed3 = 11951381506893904140ull;
			uint64_t Seed4 = 719472657908900949ull;
			uint64_t Seed6 = 17340704221724641189ull;
			uint64_t Seed7 = 10258850193283144468ull;
			uint64_t Seed8 = 8175790239553258206ull;
			uint64_t r1h, r2h, r3h, r4h;

			do {
				VIP_PREFETCH(Msg);
				vipUmul128(Seed1 ^ read_LE_64(Msg), Seed5 ^ read_LE_64(Msg + 32), &Seed1, &r1h);
				vipUmul128(Seed2 ^ read_LE_64(Msg + 8), Seed6 ^ read_LE_64(Msg + 40), &Seed2, &r2h);
				vipUmul128(Seed3 ^ read_LE_64(Msg + 16), Seed7 ^ read_LE_64(Msg + 48), &Seed3, &r3h);
				vipUmul128(Seed4 ^ read_LE_64(Msg + 24), Seed8 ^ read_LE_64(Msg + 56), &Seed4, &r4h);

				Msg += 64;
				MsgLen -= 64;
				Seed5 += r1h;
				Seed6 += r2h;
				Seed7 += r3h;
				Seed8 += r4h;
				Seed2 ^= Seed5;
				Seed3 ^= Seed6;
				Seed4 ^= Seed7;
				Seed1 ^= Seed8;
			} while (VIP_LIKELY(MsgLen > 63));

			Seed5 ^= Seed6 ^ Seed7 ^ Seed8;
			Seed1 ^= Seed2 ^ Seed3 ^ Seed4;
		}
		return static_cast<size_t>(komihash_epi(Msg, MsgLen, Seed1, Seed5));
	}
}

/**
 Strip down version of KOMIHASH hash function.
 See https://github.com/avaneev/komihash/tree/main for more details.
 */
size_t vipHashBytesKomihash(const void* Msg0, size_t MsgLen) noexcept
{
	const uint8_t* Msg = reinterpret_cast<const uint8_t*>(Msg0);
	uint64_t Seed1 = 131429069690128604ull;
	uint64_t Seed5 = 5688864720084962249ull;
	uint64_t r1h, r2h;

	VIP_PREFETCH(Msg);

	if ((MsgLen < 16)) {
		r1h = Seed1;
		r2h = Seed5;
		if (MsgLen > 7) {
			r2h ^= detail::kh_lpu64ec_l3(Msg + 8, MsgLen - 8);
			r1h ^= read_LE_64(Msg);
		}
		else if (VIP_LIKELY(MsgLen != 0))
			r1h ^= detail::kh_lpu64ec_nz(Msg, MsgLen);
		KOMIHASH_HASHFIN();
	}

	if ((MsgLen < 32)) {
		KOMIHASH_HASH16(Msg);

		if (MsgLen > 23) {
			r2h = Seed5 ^ detail::kh_lpu64ec_l4(Msg + 24, MsgLen - 24);
			r1h = Seed1 ^ read_LE_64(Msg + 16);
		}
		else {
			r1h = Seed1 ^ detail::kh_lpu64ec_l4(Msg + 16, MsgLen - 16);
			r2h = Seed5;
		}
		KOMIHASH_HASHFIN();
	}

	return static_cast<size_t>(detail::komihash_long(Msg, MsgLen, Seed1, Seed5));
}

// fallthrough
#ifndef __has_cpp_attribute
#define __has_cpp_attribute(x) 0
#endif
#if __has_cpp_attribute(clang::fallthrough)
#define SEQ_FALLTHROUGH() [[clang::fallthrough]]
#elif __has_cpp_attribute(gnu::fallthrough)
#define SEQ_FALLTHROUGH() [[gnu::fallthrough]]
#else
#define SEQ_FALLTHROUGH()
#endif

size_t vipHashBytesMurmur64(const void* _ptr, size_t len) noexcept
{
	static constexpr std::uint64_t m = 14313749767032793493ULL;
	static constexpr std::uint64_t seed = 3782874213ULL;
	static constexpr std::uint64_t r = 47ULL;

	const unsigned char* ptr = static_cast<const unsigned char*>(_ptr);
	std::uint64_t h = seed ^ (len * m);
	const std::uint8_t* end = ptr + len - (sizeof(std::uint64_t) - 1);
	while (ptr < end) {
		auto k = read_LE_64(ptr);

		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;

		ptr += sizeof(std::uint64_t);
	}

	switch (len & 7U) {
		case 7U:
			h ^= static_cast<std::uint64_t>(ptr[6U]) << 48U;
			SEQ_FALLTHROUGH();
		case 6U:
			h ^= static_cast<std::uint64_t>(ptr[5U]) << 40U;
			SEQ_FALLTHROUGH();
		case 5U:
			h ^= static_cast<std::uint64_t>(ptr[4U]) << 32U;
			SEQ_FALLTHROUGH();
		case 4U:
			h ^= static_cast<std::uint64_t>(ptr[3U]) << 24U;
			SEQ_FALLTHROUGH();
		case 3U:
			h ^= static_cast<std::uint64_t>(ptr[2U]) << 16U;
			SEQ_FALLTHROUGH();
		case 2U:
			h ^= static_cast<std::uint64_t>(ptr[1U]) << 8U;
			SEQ_FALLTHROUGH();
		case 1U:
			h ^= static_cast<std::uint64_t>(ptr[0U]);
			h *= m;
			SEQ_FALLTHROUGH();
		default:
			break;
	}

	h ^= h >> r;
	h *= m;
	h ^= h >> r;
	return static_cast<size_t>(h);
}