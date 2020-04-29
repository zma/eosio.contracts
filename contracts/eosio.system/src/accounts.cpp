#include <eosio.system/eosio.system.hpp>

#include <eosio/check.hpp>

#include "abieos/abieos_numeric.hpp"

namespace eosiosystem {

   std::string format_public_key(const eosio::public_key key) {
      abieos::public_key pkey;
      pkey.type = abieos::key_type::k1;

      auto idx = key.index();
      if (idx == 0) {
         auto thekey = std::get<0>(key);
         for (int i = 0; i < thekey.size(); ++i) {
            pkey.data[i] = thekey[i];
         }
      } else if (idx == 1) {
         auto thekey = std::get<1>(key);
         for (int i = 0; i < thekey.size(); ++i) {
            pkey.data[i] = thekey[i];
         }
      } else {
         // not supported yet
         return "";
      }

      return abieos::public_key_to_string(pkey);
   }
}
