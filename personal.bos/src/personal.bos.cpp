/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <personal.bos/personal.bos.hpp>

namespace eosio {

void personal_contract::sethomepage( name account, std::string url){
   eosio_assert( url.size() <= 256, "url is too long");
   eosio_assert( url.find("http") == 0, "illegal url");
   setpersonal( account, "homepage", url);
}

void personal_contract::setpersonal(name account,std::string key,std::string value){
   require_auth( account );
   eosio_assert( key.size() <= 12 , "key should be less than 12" );
   //not support too long value, for safety.
   eosio_assert( value.size() <= 1024 , "value should be less than 1024" );
   name key_name{ std::string_view(key) };
   personal_table personal( _self , account.value );
   auto ite = personal.find( key_name.value );
   if( ite != personal.end() ){
	  personal.modify( ite, account , [&](auto& data){
		 data.value = value;
	  });
   }else{
	  personal.emplace( account , [&](auto& data){
		 data.key = key_name.value;
		 data.readablekey = key;
		 data.value = value;
	  });
   }
}
} 

EOSIO_DISPATCH( eosio::personal_contract, (sethomepage)(setpersonal) )
