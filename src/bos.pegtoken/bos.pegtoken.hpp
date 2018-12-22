#pragma once

#include "decoder.hpp"
#include <eosiolib/asset.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/time.hpp>
#include <string>

#define MULTI_INDEX_1(table) \
    using table##s = eosio::multi_index<#table "s"##_n, table##_ts>;

#define MULTI_INDEX_2(table, index_type, index_name)                \
    using table##s = eosio::multi_index<#table "s"##_n, table##_ts, \
        indexed_by<#index_name##_n, const_mem_fun<table##_ts, index_type, &table##_ts::by_##index_name>>>;

#define MULTI_INDEX_4(table,                                                                                 \
    index_type1, index_name1,                                                                                \
    index_type2, index_name2,                                                                                \
    index_type3, index_name3)                                                                                \
    using table##s = eosio::multi_index<#table "s"##_n, table##_ts,                                          \
        indexed_by<#index_name1##_n, const_mem_fun<table##_ts, index_type1, &table##_ts::by_##index_name1>>, \
        indexed_by<#index_name2##_n, const_mem_fun<table##_ts, index_type2, &table##_ts::by_##index_name2>>, \
        indexed_by<#index_name3##_n, const_mem_fun<table##_ts, index_type3, &table##_ts::by_##index_name3>>>;

#define PRIMARY_KEY(pk) \
    uint64_t primary_key() const { return pk; }

namespace eosio {

using transaction_id_type = capi_checksum256;

class[[eosio::contract("bos.pegtoken")]] pegtoken : public contract
{
public:
    using contract::contract;

    [[eosio::action]] void create(name issuer, symbol sym);

    [[eosio::action]] void init(symbol_code sym_code, string organization, string website, name acceptor);

    [[eosio::action]] void update(symbol_code sym_code, string organization, string website);

    [[eosio::action]] void setlimit(asset max_limit, asset min_limit, asset total_limit, uint64_t frequency_limit, uint64_t interval_limit);

    [[eosio::action]] void setauditor(symbol_code sym_code, name auditor, string action);

    [[eosio::action]] void setfee(double service_fee_rate, asset min_service_fee, asset miner_fee);

    [[eosio::action]] void issue(asset quantity, string memo);

    [[eosio::action]] void retire(asset quantity, string memo);

    [[eosio::action]] void applicant(symbol_code sym_code, string action, name applicant);

    [[eosio::action]] void applyaddr(name applicant, symbol_code sym_code, name to);

    [[eosio::action]] void assignaddr(symbol_code sym_code, name to, string address);

    [[eosio::action]] void withdraw(name from, string to, asset quantity, string memo);

    [[eosio::action]] void deposit(name to, asset quantity, string memo);

    [[eosio::action]] void transfer(name from, name to, asset quantity, string memo);

    [[eosio::action]] void clear(symbol_code sym_code, uint64_t num);

    [[eosio::action]] void feedback(symbol_code sym_code, transaction_id_type trx_id, string remote_trx_id, string memo);

    [[eosio::action]] void rollback(symbol_code sym_code, transaction_id_type trx_id, string memo);

    [[eosio::action]] void setacceptor(symbol_code sym_code, name acceptor);

    [[eosio::action]] void setdelay(symbol_code sym_code, uint64_t delayday);

    [[eosio::action]] void lockall(symbol_code sym_code, name auditor);

    [[eosio::action]] void unlockall(symbol_code sym_code, name auditor);

    [[eosio::action]] void approve(symbol_code sym_code, name auditor, transaction_id_type trx_id, string memo);

    [[eosio::action]] void unapprove(symbol_code sym_code, name auditor, transaction_id_type trx_id, string memo);

    [[eosio::action]] void sendback(name auditor, transaction_id_type trx_id, name to, asset quantity, string memo);

    [[eosio::action]] void rmwithdraw(uint64_t id, symbol_code sym_code);

private:
    void verify_address(symbol_code sym_code, string address);
    void add_balance(name owner, asset value, name ram_payer);
    void sub_balance(name owner, asset value);

    struct [[eosio::table("symbols")]] symbol_ts {
        symbol sym;

        PRIMARY_KEY(sym.code().raw())
    };

    struct [[eosio::table("applicants")]] applicant_ts {
        name applicant;

        uint64_t primary_key() const { return applicant.value; }
    };

    struct [[eosio::table("rchrgaddrs")]] rchrgaddr_ts {
        name owner;
        string address;
        uint64_t state;

        time_point_sec assign_time;
        time_point_sec create_time;

        PRIMARY_KEY(owner.value)

        uint64_t by_address() const { return hash64(address); }
        uint64_t by_state() const { return state; }

        rchrgaddr_ts()
            : create_time(now())
        {
        }
    };

    struct [[eosio::table("operates")]] operate_ts {
        uint64_t id;
        name to;
        asset quantity;
        string memo;
        time_point_sec operate_time;

        PRIMARY_KEY(id)

        operate_ts()
            : operate_time(now())
        {
        }
    };

    struct [[eosio::table("withdraws")]] withdraw_ts {
        uint64_t id;
        transaction_id_type trx_id;
        name from;
        string to;
        asset quantity;
        uint64_t state;

        bool enable;
        name auditor;
        string remote_trx_id;
        string msg;

        time_point_sec create_time;
        time_point_sec update_time;

        PRIMARY_KEY(id)

        fixed_bytes<32> by_trxid() const { return trxid(trx_id); }

        uint128_t by_delindex() const
        {
            uint128_t index = (state == 2 || state == 3) ? 1 : 2;
            index <<= 64;
            index += quantity.amount;
            return index;
        }

        uint128_t by_queindex() const
        {
            uint128_t index = state;
            index <<= 64;
            index += id;
            return index;
        }

        withdraw_ts()
            : create_time(now())
            , update_time(now())
        {
        }

        static fixed_bytes<32> trxid(transaction_id_type trx_id) { return fixed_bytes<32>(trx_id.hash); }
    };

    struct [[eosio::table("deposits")]] deposit_ts {
        uint64_t id;
        transaction_id_type trx_id;
        name from;
        string to;
        asset quantity;
        uint64_t state;
        string remote_trx_id;
        string msg;

        time_point_sec create_time;
        time_point_sec update_time;

        PRIMARY_KEY(id)

        uint64_t by_delindex() const { return create_time.utc_seconds; }

        deposit_ts()
            : create_time(now())
            , update_time(now())
        {
        }
    };

    struct [[eosio::table("statistics")]] statistic_ts {
        name owner;
        time_point_sec last_time;
        uint64_t frequency;
        asset total;
        time_point_sec update_time;

        PRIMARY_KEY(owner.value);

        statistic_ts()
            : update_time(now())
        {
        }
    };

    struct [[eosio::table("accounts")]] account_ts {
        asset balance;

        PRIMARY_KEY(balance.symbol.code().raw())
    };

    struct [[eosio::table("stats")]] stat_ts {
        asset supply;
        asset max_limit;
        asset min_limit;
        asset total_limit;
        uint64_t frequency_limit;
        uint64_t interval_limit;
        uint64_t delayday;
        name issuer;
        name acceptor;
        string organization;
        string website;
        double service_fee_rate;
        asset min_service_fee;
        asset miner_fee;
        bool active;

        PRIMARY_KEY(supply.symbol.code().raw())
    };

    struct [[eosio::table("auditors")]] auditor_ts {
        name auditor;

        PRIMARY_KEY(auditor.value)
    };

    MULTI_INDEX_1(symbol)
    MULTI_INDEX_1(applicant)
    MULTI_INDEX_2(rchrgaddr, uint64_t, address)
    MULTI_INDEX_1(operate)
    MULTI_INDEX_4(withdraw, fixed_bytes<32>, trxid, uint128_t, delindex, uint128_t, queindex)
    MULTI_INDEX_2(deposit, uint64_t, delindex)
    MULTI_INDEX_1(statistic)
    MULTI_INDEX_1(account)
    MULTI_INDEX_1(stat)
    MULTI_INDEX_1(auditor)
};

} // namespace eosio