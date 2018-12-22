/**
 *  @file
 *  @copyright defined in bos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/singleton.hpp>

#include <string>

#define MULTI_INDEX(table) \
 using table##s = eosio::multi_index<#table "s"##_n, table##_ts>;


constexpr uint64_t DELAY_SEC = 7 *24 * 60 *60;

namespace eosio {
   typedef capi_checksum256   transaction_id_type;
   using std::string;

   class [[eosio::contract("bos.pegtoken")]] pegtoken : public contract {
      public:
         using contract::contract;

         [[eosio::action]]
         void create( name    issuer,
                      name    auditor,
                      asset   maximum_supply,
                      asset   large_asset,
                      asset   min_withdraw,
                      name    address_style,
                      string  organization,
                      string  website,
                      string  miner_fee,
                      string  service_fee,
                      string  unified_recharge_address,
                      bool    active );
         
         [[eosio::action]]
         void setwithdraw( asset min_withdraw );

         [[eosio::action]]
         void setmaxsupply( asset maximum_supply );

         [[eosio::action]]
         void setlargeast( asset large_asset );

         [[eosio::action]]
         void setdelay( symbol_code sym_code, uint64_t delaysec );

         [[eosio::action]]
         void lockall( symbol_code sym_code );

         [[eosio::action]]
         void unlockall( symbol_code sym_code );

         [[eosio::action]]
         void update( symbol_code sym_code,
                      string  parameter,
                      string  value );

         [[eosio::action]]
         void applicant( symbol_code   sym_code,
                         name          action,
                         name          applicant );

         [[eosio::action]]
         void applyaddr( name          applicant,
                         name          to,
                         symbol_code   sym_code );

         [[eosio::action]]
         void assignaddr( symbol_code  sym_code,
                          name         to,
                          string       address );

         [[eosio::action]]
         void issue( uint64_t seq_num, name to, asset quantity, string memo );

         [[eosio::action]]
         void approve( symbol_code  sym_code ,
                       uint64_t     issue_seq_num );

         [[eosio::action]]
         void unapprove( symbol_code  sym_code ,
                         uint64_t     issue_seq_num );

         [[eosio::action]]
         void retire( asset quantity, string memo );

         [[eosio::action]]
         void transfer( name    from,
                        name    to,
                        asset   quantity,
                        string  memo );



         [[eosio::action]]
         void withdraw( name    from,
                        string  to_address,
                        asset   quantity,
                        string  memo );

         [[eosio::action]]
         void feedback( symbol_code          sym_code,
                        transaction_id_type  trx_id,
                        uint64_t             state,
                        string               remote_trx_id,
                        string               memo );
         
         [[eosio::action]]
         void rollback( symbol_code          sym_code,
                        transaction_id_type  trx_id,
                        string               memo );

         [[eosio::action]]
         void rmwithdraw( uint64_t           id,
                          symbol_code        sym_code);
         
         [[eosio::action]]
         void rmwithdraws( uint64_t          n,
                          symbol_code        sym_code);

         [[eosio::action]]
         void open( name owner, const symbol& symbol, name ram_payer );

         [[eosio::action]]
         void close( name owner, const symbol& symbol );

         static asset get_supply( name token_contract_account, symbol_code sym_code )
         {
            stats statstable( token_contract_account, sym_code.raw() );
            const auto& st = statstable.get( sym_code.raw() );
            return st.supply;
         }

         static asset get_balance( name token_contract_account, name owner, symbol_code sym_code )
         {
            accounts accountstable( token_contract_account, owner.value );
            const auto& ac = accountstable.get( sym_code.raw() );
            return ac.balance;
         }

      private:

         struct [[eosio::table]] symbol_ts {
            symbol  sym;

            uint64_t primary_key()const { return sym.code().raw(); }
         };

         struct [[eosio::table]] applicant_ts {
            name  applicant;

            uint64_t primary_key()const { return applicant.value; }
         };

         struct [[eosio::table]] recharge_address_ts {
            name           owner;
            string         address;
            time_point_sec assign_time;
            uint64_t       state;

            time_point_sec create_time;

            uint64_t primary_key()const { return owner.value; }
            uint64_t by_address()const { return hash64( address ); }
            uint64_t by_state()const { return state; }

            recharge_address_ts():create_time(time_point_sec()){}
         };

         struct [[eosio::table]] issue_ts {
            uint64_t seq_num;
            name     to;
            asset    quantity;
            string   memo;

            uint64_t primary_key()const { return seq_num; }
         };

         struct [[eosio::table]] withdraw_ts {
            uint64_t             id;
            transaction_id_type  trx_id;
            name                 from;
            string               to;
            asset                quantity;
            time_point_sec       create_time;
            uint64_t             state;
            string               feedback_trx_id;
            string               feedback_msg;
            time_point_sec       feedback_time;

            uint64_t  primary_key()const { return id; }
            fixed_bytes<32> by_trxid()const { return fixed_bytes<32>(trx_id.hash); }
            uint64_t by_state()const { return state; }
            withdraw_ts():create_time(time_point_sec()),feedback_time(time_point_sec()){};

            uint128_t by_state_and_feedback_time() const 
            {
               uint128_t index = 0;
               index |= feedback_time.utc_seconds;
               index <<= 64;
               uint64_t mask = (state == 5 || state == 2) ? 0 : 1;
               mask <<= 63;
               // amount should always be positive
               uint64_t amount = 0x7fffffffffffffff;
               amount += quantity.amount;
               index |= amount;
               return index;
            }
         };

         struct [[eosio::table]] account {
            asset    balance;

            uint64_t primary_key()const { return balance.symbol.code().raw(); }
         };

         struct [[eosio::table]] currency_stats {
            asset    supply;
            asset    max_supply;
            asset    large_asset;
            asset    min_withdraw;
            name     issuer;
            name     auditor;
            name     address_style;
            string   organization;
            string   website;
            string   miner_fee;
            string   service_fee;
            string   unified_recharge_address;
            bool     active;
            uint64_t issue_seq_num;

            uint32_t delay_sec; 

            uint64_t primary_key()const { return supply.symbol.code().raw(); }

            currency_stats():delay_sec(DELAY_SEC){}
         };

         struct [[eosio::table]] deposit_ts {
            uint64_t             id;
            transaction_id_type  trx_id;
            name                 from;
            string               to;
            asset                quantity;
            time_point_sec       create_time;

            uint64_t primary_key() const { return id; }
         };

         MULTI_INDEX(applicant)
         // typedef eosio::multi_index< "applicants"_n, applicant_ts > applicants;
         typedef eosio::multi_index< "symbols"_n, symbol_ts > symbols;
         typedef eosio::multi_index< "issues"_n, issue_ts > issues;
         typedef eosio::multi_index< "rchrgaddr"_n, recharge_address_ts ,
            indexed_by<"address"_n, const_mem_fun<recharge_address_ts, uint64_t, &recharge_address_ts::by_address> >,
            indexed_by<"state"_n, const_mem_fun<recharge_address_ts, uint64_t, &recharge_address_ts::by_state> >
         > addresses;
         typedef eosio::multi_index< "withdraws"_n, withdraw_ts,
               indexed_by<"trxid"_n, const_mem_fun<withdraw_ts, fixed_bytes<32>, &withdraw_ts::by_trxid>  >,
               indexed_by<"state"_n, const_mem_fun<withdraw_ts, uint64_t, &withdraw_ts::by_state>  >,
               indexed_by<"delindex"_n, const_mem_fun<withdraw_ts, uint128_t, &withdraw_ts::by_state_and_feedback_time>  >
         > withdraws;
         typedef eosio::multi_index< "accounts"_n, account > accounts;
         typedef eosio::multi_index< "stat"_n, currency_stats > stats;
         typedef eosio::multi_index< "deposit"_n, deposit_ts > deposits;

         void sub_balance( name owner, asset value );
         void add_balance( name owner, asset value, name ram_payer );

         void verify_maximum_supply(asset maximum_supply);
         void verify_address( name style, string addr);
         void issue_handle( symbol_code sym_code, uint64_t issue_seq_num, bool pass);
         static uint64_t hash64( string str );

         uint32_t get_delay_sec(symbol_code sym_code) const;
   };

} /// namespace eosio