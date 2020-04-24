#include <eosio.system/eosio.system.hpp>
#include <eosio.token/eosio.token.hpp>
#include <eosio.system/rex.results.hpp>

namespace eosiosystem {

   using eosio::current_time_point;
   using eosio::token;
   using eosio::seconds;

   void new_accounts_counter_bookkeep(const eosio::name& self) {
      // update statistic counters, initialize them if they are not initialized
      // yet
      new_accounts_counter_meta_table accountcntrm( self, name("eosio").value );
      auto meta_it = accountcntrm.find(0);
      // after the meta table is initialized, check whether need to move the
      // tracking window
      auto cur_epoch = eosio::current_time_point().sec_since_epoch();
      auto cur_interval = cur_epoch / STATISTICS_NEW_ACCOUNTS_INTERVAL * STATISTICS_NEW_ACCOUNTS_INTERVAL;
      if (meta_it != accountcntrm.end()) {
         // clean up old counters, if they are now out of the sliding window
         if (cur_interval - meta_it->first_interval_start_time >= STATISTICS_NEW_ACCOUNTS_ALL_INTERVALS) {
            // move the window, and update the accumulated latest new account counters
            auto latest_accounts = meta_it->accumulated_latest_accounts_count;
            auto new_first_interval_start_time = cur_interval - (STATISTICS_NEW_ACCOUNTS_INTERVAL_COUNT - 1) * STATISTICS_NEW_ACCOUNTS_INTERVAL;
            // erease counters for intervals outside of the sliding window now
            auto erase_end = std::min(new_first_interval_start_time, meta_it->first_interval_start_time + STATISTICS_NEW_ACCOUNTS_ALL_INTERVALS);
            for (auto time = meta_it->first_interval_start_time;
                 time < erase_end;
                 time += STATISTICS_NEW_ACCOUNTS_INTERVAL) {

               new_accounts_counter_table accountcntr(self, name("eosio").value);
               auto counter_it = accountcntr.find(time);
               if (counter_it != accountcntr.end()) {
                  accountcntr.erase(counter_it);
                  latest_accounts -= counter_it->count;
               }
            }

            // emplace new counters
            auto emplace_begin = std::max(new_first_interval_start_time, meta_it->first_interval_start_time + STATISTICS_NEW_ACCOUNTS_ALL_INTERVALS);
            for (auto time = emplace_begin;
                 time < new_first_interval_start_time + STATISTICS_NEW_ACCOUNTS_ALL_INTERVALS;
                 time += STATISTICS_NEW_ACCOUNTS_INTERVAL) {
               new_accounts_counter_table accountcntr(self, name("eosio").value);
               accountcntr.emplace(self, [&](auto& row) {
                  row.start_time = time;
                  row.count = 0;
               });
            }

            // update accountcntrm
            accountcntrm.modify(meta_it, self, [&](auto& row) {
               row.first_interval_start_time = new_first_interval_start_time;
               row.accumulated_latest_accounts_count = latest_accounts;
            });
         }
      } else {
         // initialized the counters
         // this make it works even if the system contract is upgraded
         // to have this function
         accountcntrm.emplace(self, [&](auto& row) {
            row.first_interval_start_time = cur_interval;
            row.accumulated_accounts_count = 0;
            row.accumulated_latest_accounts_count = 0;
         });

         for (int64_t cnt = 0; cnt < STATISTICS_NEW_ACCOUNTS_INTERVAL_COUNT; ++cnt) {
            auto start_time = cur_interval + cnt * STATISTICS_NEW_ACCOUNTS_INTERVAL;
            new_accounts_counter_table accountcntr(self, start_time);
            auto counter_it = accountcntr.find(start_time);
            accountcntr.emplace(self, [&](auto& row) {
               row.start_time = start_time;
               row.count = 0;
            });
         }
      }

   }
}; /// namespace eosiosystem
