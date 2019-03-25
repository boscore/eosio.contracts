/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/eosio.hpp>

#include <string>

namespace eosio {

   using std::string;

   class [[eosio::contract("personal.bos")]] personal_contract : public contract {
      public:
         using contract::contract;

         [[eosio::action]]
         void setpersonal( name account, string key, string value);

         [[eosio::action]]
         void sethomepage( name account, string url);

         string get_personal( name account, string key ){
            if(key.size() > 12){
               return "";
            }
            name key_name{ std::string_view(key) };
            personal_table personal( _self , account.value );
            auto ite = personal.find( key_name.value );
            if(ite != personal.end()){
               return ite->value;
            }else{
               return "";
            }
         }

      private:
		 /*For accounts to store their personal specific key-value data.
		 key needs to be string and follow rules: only contain a-z,0-5,not longer than 12.
		 for the key shorter than 12,will add 0 at higher bits.
		 by doing this we can use key as uint64_t,to save space and easy use.
		 usage: ex. dapp owner can call sethomepage to set its homepage,guiding users to their dapp*/
		 struct [[eosio::table]] personal{
			uint64_t key;
			std::string readablekey;
			std::string value;
			EOSLIB_SERIALIZE(personal,(key)(readablekey)(value))
				
			uint64_t primary_key()const { return key; }
		 };
		 typedef eosio::multi_index< "personaldata"_n, personal > personal_table;
   };

} /// namespace eosio
