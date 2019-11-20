#pragma once


#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <eosio/action.hpp>
using namespace eosio;
using namespace std;

struct [[eosio::table, eosio::contract("eosio.system")]] user_resources {
   name owner;
   asset net_weight;
   asset cpu_weight;
   int64_t ram_bytes = 0;

   bool is_empty() const { return net_weight.amount == 0 && cpu_weight.amount == 0 && ram_bytes == 0; }
   uint64_t primary_key() const { return owner.value; }

   // explicit serialization macro is not necessary, used here only to improve compilation time
   EOSLIB_SERIALIZE(user_resources, (owner)(net_weight)(cpu_weight)(ram_bytes))
};
typedef eosio::multi_index< "userres"_n, user_resources >      user_resources_table;


struct [[eosio::table, eosio::contract("bos.burn")]] unactivated_airdrop_account {
  name account;
  asset quantity;
  uint8_t is_burned;
  uint64_t primary_key() const { return account.value; }
};

typedef eosio::multi_index<"accounts"_n, unactivated_airdrop_account> accounts;


struct [[eosio::table("metaparams"), eosio::contract("bos.burn")]] meta_parameters {
   meta_parameters() {}
   uint8_t version;
   name executer;

   EOSLIB_SERIALIZE(meta_parameters, (version)(executer))
};

typedef eosio::singleton<"metaparams"_n, meta_parameters> meta_parameters_singleton;

static const uint8_t current_version = 1;
class [[eosio::contract("bos.burn")]] bos_burn : public eosio::contract {
 private:
   meta_parameters_singleton _meta_parameters_singleton;
   meta_parameters _meta_parameters;

 public:
   static constexpr eosio::name token_account{"eosio.token"_n};
   static constexpr eosio::name active_permission{"active"_n};
   using contract::contract;
   bos_burn(name receiver, name code, datastream<const char*> ds) : contract(receiver, code, ds), _meta_parameters_singleton(_self, _self.value) {
      _meta_parameters = _meta_parameters_singleton.exists() ? _meta_parameters_singleton.get() : meta_parameters{};
   }
   ~bos_burn() { _meta_parameters_singleton.set(_meta_parameters, _self); }

   [[eosio::action]] void importacnts(std::vector<std::pair<name,asset>> unactivated_airdrop_accounts);
   [[eosio::action]] void setparameter(uint8_t version,name executer);
   [[eosio::action]] void burn(asset quantity);
   [[eosio::action]] void transferairs(name account);
   [[eosio::action]] void transferair(name account);
   [[eosio::action]] void clear(vector<name> clear_accounts);
   using importacnts_action = eosio::action_wrapper<"importacnts"_n, &bos_burn::importacnts>;
   using setparameter_action = eosio::action_wrapper<"setparameter"_n, &bos_burn::setparameter>;
   using burn_action = eosio::action_wrapper<"burn"_n, &bos_burn::burn>;
   using transferairs_action = eosio::action_wrapper<"transferairs"_n, &bos_burn::transferairs>;
   using transferair_action = eosio::action_wrapper<"transferair"_n, &bos_burn::transferair>;
   using clear_action = eosio::action_wrapper<"clear"_n, &bos_burn::clear>;

 private:
  static  constexpr  name hole_account="hole.bos"_n;
   /// common
   symbol core_symbol() const {
      symbol _core_symbol = symbol(symbol_code("BOS"), 4);
      return _core_symbol;
   };
};