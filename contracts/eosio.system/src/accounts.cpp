#include <eosio.system/eosio.system.hpp>

#include <eosio/check.hpp>

#include "abieos/abieos_numeric.hpp"

namespace eosiosystem {

   std::string format_public_key(const eosio::public_key& key) {
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

   void record_account_keys(const authority& auth, const name& account, const name& contract) {
      std::string accstr = account.to_string();
      for (auto& key: auth.keys) {
         auto pubkey = format_public_key(key.key);
         // existing value of this key first
         auto acclist = get_key_accounts(pubkey, contract);
         bool exist = false;
         if (acclist == "") {
            acclist = accstr;
         } else {
            // check already existing or not first
            // avoid sstrem warnings, using find_first_not_of and find
            size_t start = 0;
            size_t end = 0;
            char delim = ';';
            while ((start = acclist.find_first_not_of(delim, end)) != std::string::npos)
            {
               end = acclist.find(delim, start);
               if (acclist.substr(start, end - start) == accstr) {
                  exist = true;
                  break;
               }
            }
            if (!exist) {
               acclist += ";" + accstr;
            }
         }

         // new value
         if (!exist) {
            kv_set(name{"eosio.kvdisk"}.value, contract.value, pubkey.c_str(), pubkey.length(), acclist.c_str(), acclist.length());
         }
      }
   }

   std::string get_key_accounts(const std::string& key, const name& contract) {
      uint32_t valsize = 0;
      auto db = name{"eosio.kvdisk"};
      if (kv_get(db.value, contract.value, key.c_str(), key.length(), valsize)) {
         std::vector<char> buf(valsize);
         kv_get_data(db.value, 0, &(buf[0]), valsize);
         return std::string(std::begin(buf), std::end(buf));
      }
      return std::string{};
   }
}

