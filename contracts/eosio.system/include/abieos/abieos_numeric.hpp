// copyright defined in abieos/LICENSE.txt

#include <algorithm>
#include <array>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>

#include <eosio/check.hpp>

#include "ripemd160.hpp"

namespace abieos {

   const char base58_chars[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

   bool map_initialized = false;
   std::array<int8_t, 256> base58_map{{0}};
   auto get_base58_map() {
      if(!map_initialized) {
         for (unsigned i = 0; i < base58_map.size(); ++i)
            base58_map[i] = -1;
         for (unsigned i = 0; i < sizeof(base58_chars); ++i)
            base58_map[base58_chars[i]] = i;
         map_initialized = true;
      }
      return base58_map;
   }

   template <size_t size>
      bool is_negative(const std::array<uint8_t, size>& a) {
         return a[size - 1] & 0x80;
      }

   template <size_t size>
      void negate(std::array<uint8_t, size>& a) {
         uint8_t carry = 1;
         for (auto& byte : a) {
            int x = uint8_t(~byte) + carry;
            byte = x;
            carry = x >> 8;
         }
      }

   template <size_t size>
      std::array<uint8_t, size> decimal_to_binary(std::string_view s) {
         std::array<uint8_t, size> result{{0}};
         for (auto& src_digit : s) {
            if (src_digit < '0' || src_digit > '9')
               eosio::check(0, "invalid number");
            uint8_t carry = src_digit - '0';
            for (auto& result_byte : result) {
               int x = result_byte * 10 + carry;
               result_byte = x;
               carry = x >> 8;
            }
            if (carry)
               eosio::check(0, "number is out of range");
         }
         return result;
      }

   template <size_t size>
      std::string binary_to_decimal(const std::array<uint8_t, size>& bin) {
         std::string result("0");
         for (auto byte_it = bin.rbegin(); byte_it != bin.rend(); ++byte_it) {
            int carry = *byte_it;
            for (auto& result_digit : result) {
               int x = ((result_digit - '0') << 8) + carry;
               result_digit = '0' + x % 10;
               carry = x / 10;
            }
            while (carry) {
               result.push_back('0' + carry % 10);
               carry = carry / 10;
            }
         }
         std::reverse(result.begin(), result.end());
         return result;
      }

   template <size_t size>
      std::array<uint8_t, size> base58_to_binary(std::string_view s) {
         std::array<uint8_t, size> result{{0}};
         for (auto& src_digit : s) {
            int carry = get_base58_map()[src_digit];
            if (carry < 0)
               eosio::check(0, "invalid base-58 value");
            for (auto& result_byte : result) {
               int x = result_byte * 58 + carry;
               result_byte = x;
               carry = x >> 8;
            }
            if (carry)
               eosio::check(0, "base-58 value is out of range");
         }
         std::reverse(result.begin(), result.end());
         return result;
      }

   template <size_t size>
      std::string binary_to_base58(const std::array<uint8_t, size>& bin) {
         std::string result("");
         for (auto byte : bin) {
            int carry = byte;
            for (auto& result_digit : result) {
               int x = (get_base58_map()[result_digit] << 8) + carry;
               result_digit = base58_chars[x % 58];
               carry = x / 58;
            }
            while (carry) {
               result.push_back(base58_chars[carry % 58]);
               carry = carry / 58;
            }
         }
         for (auto byte : bin)
            if (byte)
               break;
            else
               result.push_back('1');
         std::reverse(result.begin(), result.end());
         return result;
      }

   enum class key_type : uint8_t {
      k1 = 0,
      r1 = 1,
   };

   struct public_key {
      key_type type{};
      std::array<uint8_t, 33> data{};
   };

   struct private_key {
      key_type type{};
      std::array<uint8_t, 32> data{};
   };

   struct signature {
      key_type type{};
      std::array<uint8_t, 65> data{};
   };

   auto digest_message_ripemd160(const unsigned char* message, size_t message_len) {
      std::array<unsigned char, 20> digest;
      ripemd160::ripemd160_state self;
      ripemd160::ripemd160_init(&self);
      ripemd160::ripemd160_update(&self, message, message_len);
      eosio::check (ripemd160::ripemd160_digest(&self, digest.data()), "ripemd failed");
      return digest;
   }

   template <size_t size, int suffix_size>
      auto digest_suffix_ripemd160(const std::array<uint8_t, size>& data, const char (&suffix)[suffix_size]) {
         std::array<unsigned char, 20> digest;
         ripemd160::ripemd160_state self;
         ripemd160::ripemd160_init(&self);
         ripemd160::ripemd160_update(&self, data.data(), data.size());
         ripemd160::ripemd160_update(&self, (uint8_t*)suffix, suffix_size - 1);
         eosio::check (ripemd160::ripemd160_digest(&self, digest.data()), "ripemd failed");
         return digest;
      }

   template <typename Key, int suffix_size>
      Key string_to_key(std::string_view s, key_type type, const char (&suffix)[suffix_size]) {
         static const auto size = std::tuple_size<decltype(Key::data)>::value;
         auto whole = base58_to_binary<size + 4>(s);
         Key result{type};
         memcpy(result.data.data(), whole.data(), result.data.size());
         auto ripe_digest = digest_suffix_ripemd160(result.data, suffix);
         eosio::check (memcmp(ripe_digest.data(), whole.data() + result.data.size(), 4) == 0, "checksum doesn't match");
         return result;
      }

   template <typename Key, int suffix_size>
      std::string key_to_string(const Key& key, const char (&suffix)[suffix_size], const char* prefix) {
         static const auto size = std::tuple_size<decltype(Key::data)>::value;
         auto ripe_digest = digest_suffix_ripemd160(key.data, suffix);
         std::array<uint8_t, size + 4> whole;
         memcpy(whole.data(), key.data.data(), size);
         memcpy(whole.data() + size, ripe_digest.data(), 4);
         return prefix + binary_to_base58(whole);
      }

   public_key string_to_public_key(std::string_view s) {
      if (s.size() >= 3 && s.substr(0, 3) == "EOS") {
         auto whole = base58_to_binary<37>(s.substr(3));
         public_key key{key_type::k1};
         static_assert(whole.size() == key.data.size() + 4, "Error: whole.size() != key.data.size() + 4");
         memcpy(key.data.data(), whole.data(), key.data.size());
         auto ripe_digest = digest_message_ripemd160(key.data.data(), key.data.size());
         eosio::check (memcmp(ripe_digest.data(), whole.data() + key.data.size(), 4) == 0, "Key checksum doesn't match");
         return key;
      } else if (s.size() >= 7 && s.substr(0, 7) == "PUB_R1_") {
         return string_to_key<public_key>(s.substr(7), key_type::r1, "R1");
      } else {
         eosio::check(0, "unrecognized public key format");
         // never reach here
         return public_key{};
      }
   }

   std::string public_key_to_string(const public_key& key) {
      if (key.type == key_type::k1) {
         auto ripe_digest = digest_message_ripemd160(key.data.data(), key.data.size());
         std::array<uint8_t, 37> whole;
         memcpy(whole.data(), key.data.data(), key.data.size());
         memcpy(whole.data() + key.data.size(), ripe_digest.data(), 4);
         return "EOS" + binary_to_base58(whole);
      } else if (key.type == key_type::r1) {
         return key_to_string(key, "R1", "PUB_R1_");
      } else {
         eosio::check(0, "unrecognized public key format");
         // never reach here
         return "";
      }
   }

   private_key string_to_private_key(std::string_view s) {
      if (s.size() >= 7 && s.substr(0, 7) == "PVT_R1_")
         return string_to_key<private_key>(s.substr(7), key_type::r1, "R1");
      else
         eosio::check(0, "unrecognized private key format");

      // never reach here
      return private_key{};
   }

   std::string private_key_to_string(const private_key& private_key) {
      if (private_key.type == key_type::r1)
         return key_to_string(private_key, "R1", "PVT_R1_");
      else
         eosio::check(0, "unrecognized private key format");

      // never reach here
      return "";
   }

   signature string_to_signature(std::string_view s) {
      if (s.size() >= 7 && s.substr(0, 7) == "SIG_K1_")
         return string_to_key<signature>(s.substr(7), key_type::k1, "K1");
      else if (s.size() >= 7 && s.substr(0, 7) == "SIG_R1_")
         return string_to_key<signature>(s.substr(7), key_type::r1, "R1");
      else
         eosio::check(0, "unrecognized signature format");

      // never reach here
      return signature{};
   }

   std::string signature_to_string(const signature& signature) {
      if (signature.type == key_type::k1)
         return key_to_string(signature, "K1", "SIG_K1_");
      else if (signature.type == key_type::r1)
         return key_to_string(signature, "R1", "SIG_R1_");
      else
         eosio::check(0, "unrecognized signature format");

      // never reach here
      return "";
   }

} // namespace abieos
