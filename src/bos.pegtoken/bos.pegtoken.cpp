#include "bos.pegtoken.hpp"
#include <eosiolib/transaction.hpp>

#define STRING_LEN_CHECK(str, len) \
    eosio_assert(str.size() <= len, "param " #str " too long, maxmium length is " #len);

#define ACCOUNT_CHECK(account) \
    eosio_assert(is_account(account), "invalid account " #account);

#define ASSIGN_STATS_AND_ITER(sym_raw)             \
    auto stats_table = stats(get_self(), sym_raw); \
    auto iter = stats_table.find(sym_raw);         \
    eosio_assert(iter != stats_table.end(), "token not exist");

#define NEED_ISSUER_AUTH(sym_raw)  \
    ASSIGN_STATS_AND_ITER(sym_raw) \
    require_auth(iter->issuer);

#define NEED_ACCEPTOR_AUTH(sym_raw) \
    ASSIGN_STATS_AND_ITER(sym_raw)  \
    require_auth(iter->acceptor);

#define NEED_AUDITOR_AUTH(sym_raw, auditor)                                        \
    require_auth(auditor);                                                         \
    ASSIGN_STATS_AND_ITER(sym_raw)                                                 \
    {                                                                              \
        auto auds = auditors(get_self(), sym_raw);                                 \
        eosio_assert(auds.find(auditor.value) != auds.end(), "auditor not exist"); \
    }

#define ACCOUNT_EXCLUDE(account, sym_raw)                                                      \
    eosio_assert(account != get_self(), #account " can't be contract");                        \
    ASSIGN_STATS_AND_ITER(sym_raw)                                                             \
    {                                                                                          \
        eosio_assert(account != iter->issuer, #account " can't be issuer");                    \
        eosio_assert(account != iter->acceptor, #account " can't be acceptor");                \
        auto appl = applicants(get_self(), sym_raw);                                           \
        eosio_assert(appl.find(account.value) == appl.end(), #account " can't be applicant "); \
    }

#define NIL_ACCOUNT "nil"_n

namespace eosio {

constexpr uint32_t ONE_DAY = 24 * 60 * 60;

////////////////////////
// private funcs
////////////////////////
void pegtoken::verify_address(symbol_code sym_code, string addr)
{
    if (sym_code == symbol_code("BTC")) {
        eosio_assert(valid_bitcoin_addr(addr), "invalid bitcoin addr");
    } else if (sym_code == symbol_code("ETH")) {
        eosio_assert(valid_ethereum_addr(addr), "invalid ethereum addr");
    } else if (sym_code == symbol_code("EOS")) {
        auto _ = name(addr);
    } else {
        eosio_assert(false, "only EOS, BTC and ETH supported");
    }
}

void pegtoken::sub_balance(name owner, asset value)
{
    auto acct = accounts(get_self(), owner.value);
    auto from = acct.find(value.symbol.code().raw());
    eosio_assert(from != acct.end(), "no balance object found");
    eosio_assert(from->balance.amount >= value.amount, "overdrawn balance");
    if (from->balance.amount == value.amount) {
        acct.erase(from);
    } else {
        acct.modify(from, same_payer, [&](auto& p) {
            p.balance -= value;
        });
    }
}

void pegtoken::add_balance(name owner, asset value, name ram_payer)
{
    auto acct = accounts(get_self(), owner.value);
    auto to = acct.find(value.symbol.code().raw());
    if (to == acct.end()) {
        acct.emplace(ram_payer, [&](auto& p) {
            p.balance = value;
        });
    } else {
        acct.modify(to, same_payer, [&](auto& p) {
            p.balance += value;
        });
    }
}

////////////////////////
// actions
////////////////////////

void pegtoken::create(name issuer, symbol sym)
{
    require_auth(get_self());

    ACCOUNT_CHECK(issuer);
    eosio_assert(sym.is_valid(), "invalid symbol");

    auto stats_table = stats(get_self(), sym.code().raw());
    eosio_assert(stats_table.find(sym.code().raw()) == stats_table.end(), "token with symbol already exists");

    stats_table.emplace(get_self(), [&](auto& p) {
        p.supply = asset(0, sym);
        p.max_limit = p.supply;
        p.min_limit = p.supply;
        p.min_service_fee = p.supply;
        p.miner_fee = p.supply;
        p.total_limit = p.supply;
        p.frequency_limit = 0;
        p.interval_limit = 300;
        p.delayday = 7;
        p.service_fee_rate = 0;
        p.issuer = issuer;
        p.acceptor = issuer;
        p.active = false;
    });

    auto syms = symbols(get_self(), get_self().value);
    syms.emplace(get_self(), [&](auto& p) { p.sym = sym; });
}

void pegtoken::init(symbol_code sym_code, string organization, string website, name acceptor)
{
    STRING_LEN_CHECK(organization, 256)
    STRING_LEN_CHECK(website, 256)

    ACCOUNT_CHECK(acceptor)

    NEED_ISSUER_AUTH(sym_code.raw())

    stats_table.modify(iter, same_payer, [&](auto& p) {
        p.organization = organization;
        p.website = website;
        p.acceptor = acceptor;
        p.active = true;
    });
}

void pegtoken::update(symbol_code sym_code, string organization, string website)
{
    STRING_LEN_CHECK(organization, 256)
    STRING_LEN_CHECK(website, 256)

    NEED_ISSUER_AUTH(sym_code.raw())

    stats_table.modify(iter, same_payer, [&](auto& p) {
        p.organization = organization;
        p.website = website;
    });
}

void pegtoken::setlimit(asset max_limit, asset min_limit, asset total_limit, uint64_t frequency_limit, uint64_t interval_limit)
{
    eosio_assert(max_limit >= min_limit && total_limit >= max_limit, "constrict mismatch: total_limit >= max_limit >= min_limit");

    NEED_ISSUER_AUTH(max_limit.symbol.code().raw())

    stats_table.modify(iter, same_payer, [&](auto& p) {
        p.max_limit = max_limit;
        p.min_limit = min_limit;
        p.total_limit = total_limit;
        p.frequency_limit = frequency_limit;
        p.interval_limit = interval_limit;
    });
}

void pegtoken::setauditor(symbol_code sym_code, name auditor, string action)
{
    { ACCOUNT_CHECK(auditor) };

    NEED_ISSUER_AUTH(sym_code.raw())

    auto auds = auditors(_self, sym_code.raw());
    if (action == "add") {
        eosio_assert(auds.find(auditor.value) == auds.end(), "auditor already exist");
        auds.emplace(iter->issuer, [&](auto& p) {
            p.auditor = auditor;
        });
    } else if (action == "remove") {
        auto iter = auds.find(auditor.value);
        eosio_assert(iter != auds.end(), ("auditor " + auditor.to_string() + " not exist").c_str());
        auds.erase(auds.find(auditor.value));
    } else {
        eosio_assert(false, ("invalid action: " + action).c_str());
    }
}

void pegtoken::setfee(double service_fee_rate, asset min_service_fee, asset miner_fee)
{
    eosio_assert(service_fee_rate >= 0 && service_fee_rate < 1 && min_service_fee.amount >= 0 && miner_fee.amount >= 0, "invalid service_fee_rate or min_service_fee or miner_fee");
    eosio_assert(min_service_fee.symbol == miner_fee.symbol, "min_service_fee and miner_fee are not the same symbol");

    NEED_ISSUER_AUTH(min_service_fee.symbol.code().raw())

    stats_table.modify(iter, same_payer, [&](auto& p) {
        p.service_fee_rate = service_fee_rate;
        p.min_service_fee = min_service_fee;
        p.miner_fee = miner_fee;
    });
}

void pegtoken::issue(asset quantity, string memo)
{
    STRING_LEN_CHECK(memo, 256)

    eosio_assert(quantity.is_valid() && quantity.amount > 0, "invalid quantity");

    NEED_ISSUER_AUTH(quantity.symbol.code().raw())
    ACCOUNT_CHECK(iter->acceptor)
    eosio_assert(iter->active, "token is not active");

    add_balance(iter->acceptor, quantity, iter->issuer);
    stats_table.modify(iter, same_payer, [&](auto& p) {
        p.supply += quantity;
        eosio_assert(p.supply.amount > 0, "supply overflow");
    });

    auto oper = operates(get_self(), quantity.symbol.code().raw());
    oper.emplace(iter->issuer, [&](auto& p) {
        p.id = oper.available_primary_key();
        p.to = iter->acceptor;
        p.quantity = quantity;
        p.memo = memo;
    });
}

void pegtoken::retire(asset quantity, string memo)
{
    STRING_LEN_CHECK(memo, 256)

    eosio_assert(quantity.is_valid() && quantity.amount > 0, "invalid quantity");

    NEED_ISSUER_AUTH(quantity.symbol.code().raw())
    ACCOUNT_CHECK(iter->acceptor)
    eosio_assert(iter->active, "token is not active");
    eosio_assert(iter->supply >= quantity, "invalid quantity");

    sub_balance(iter->acceptor, quantity);
    stats_table.modify(iter, same_payer, [&](auto& p) {
        p.supply -= quantity;
    });

    auto oper = operates(get_self(), quantity.symbol.code().raw());
    oper.emplace(iter->issuer, [&](auto& p) {
        p.id = oper.available_primary_key();
        p.to = iter->acceptor;
        p.quantity = quantity;
        p.memo = memo;
    });
}

void pegtoken::applicant(symbol_code sym_code, string action, name applicant)
{

    { ACCOUNT_CHECK(applicant) };

    NEED_ISSUER_AUTH(sym_code.raw())
    eosio_assert(applicant != get_self(), "applicant can`t be contract");
    eosio_assert(applicant != iter->issuer, "applicant can`t be issuer");
    eosio_assert(applicant != iter->acceptor, "applicant can`t be acceptor");

    auto appl = applicants(get_self(), sym_code.raw());
    if (action == "add") {
        eosio_assert(appl.find(applicant.value) == appl.end(), "applicant already exist");
        appl.emplace(iter->issuer, [&](auto& p) {
            p.applicant = applicant;
        });
    } else if (action == "remove") {
        auto iter = appl.find(applicant.value);
        eosio_assert(iter != appl.end(), "applicant not exist");
        appl.erase(iter);
    } else {
        eosio_assert(false, "action must be add or remove");
    }
}

void pegtoken::applyaddr(name applicant, symbol_code sym_code, name to)
{
    require_auth(applicant);

    ACCOUNT_CHECK(to)

    auto appl = applicants(get_self(), sym_code.raw());
    eosio_assert(appl.find(applicant.value) != appl.end(), "applicant dose not exist");

    { ACCOUNT_EXCLUDE(to, sym_code.raw()) };

    auto addresses = rchrgaddrs(get_self(), sym_code.raw());
    eosio_assert(addresses.find(to.value) == addresses.end(), "to account has applied for address already");

    addresses.emplace(applicant, [&](auto& p) {
        p.owner = to;
        p.state = to.value;
    });
}

void pegtoken::assignaddr(symbol_code sym_code, name to, string address)
{
    ACCOUNT_CHECK(to)

    STRING_LEN_CHECK(address, 64)

    name payer;
    {
        ACCOUNT_EXCLUDE(to, sym_code.raw())
        require_auth(iter->issuer);
        payer = iter->issuer;
    }

    verify_address(sym_code, address);

    auto addresses = rchrgaddrs(get_self(), sym_code.raw());
    auto addr = addresses.template get_index<"address"_n>();
    auto iter = addr.find(hash64(address));

    eosio_assert(iter == addr.end(), ("this address " + address + " has been assigned to " + iter->owner.to_string()).c_str());

    auto iter2 = addr.find(to.value);
    if (iter2 == addr.end()) {
        addresses.emplace(payer, [&](auto& p) {
            p.owner = to;
            p.address = address;
            p.assign_time = time_point_sec(now());
            p.state = 0;
        });
    } else {
        addr.modify(iter2, same_payer, [&](auto& p) {
            p.address = address;
            p.assign_time = time_point_sec(now());
            p.state = 0;
        });
    }
}

void pegtoken::withdraw(name from, string to, asset quantity, string memo)
{
    require_auth(from);

    STRING_LEN_CHECK(memo, 256)

    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount > 0, "must withdraw positive quantity");

    auto sym = quantity.symbol;

    ACCOUNT_EXCLUDE(from, quantity.symbol.code().raw())

    eosio_assert(iter->active, "underwriter is not active");

    eosio_assert(quantity >= iter->min_limit, "quantity less than min_limit");

    verify_address(quantity.symbol.code(), to);

    bool need_check = false;

    auto stt = statistics(get_self(), quantity.symbol.code().raw());
    auto iter2 = stt.find(from.value);

    if (quantity > iter->max_limit) {
        need_check = true;
    }

    if (iter2 == stt.end()) {
        stt.emplace(from, [&](auto& p) {
            p.owner = from;
            p.last_time = time_point_sec(now());
            p.frequency = 1;
            p.total = quantity;
            p.update_time = p.last_time;
        });
    } else {
        eosio_assert(iter2->last_time.utc_seconds < now() - iter->interval_limit, "exceed interval_limit");
        if (iter2->last_time.utc_seconds / ONE_DAY != now() / ONE_DAY) {
            stt.modify(iter2, same_payer, [&](auto& p) {
                p.last_time = time_point_sec(now());
                p.frequency = 1;
                p.total = quantity;
                p.update_time = p.last_time;
            });
        } else {
            if (iter2->frequency >= iter->frequency_limit || iter2->total + quantity > iter->total_limit) {
                need_check = true;
            }
            stt.modify(iter2, same_payer, [&](auto& p) {
                p.last_time = time_point_sec(now());
                p.frequency += 1;
                p.total += quantity;
            });
        }
    }

    auto accs = accounts(get_self(), from.value);
    auto& owner = accs.get(quantity.symbol.code().raw(), "no balance object found");
    eosio_assert(owner.balance >= quantity, "overdrawn balance");

    SEND_INLINE_ACTION(*this, transfer, { { from, "active"_n } }, { from, iter->acceptor, quantity, "withdraw address:" + iter->issuer.to_string() + " memo: " + memo });

    auto wds = withdraws(get_self(), quantity.symbol.code().raw());
    wds.emplace(get_self(), [&](auto& p) {
        p.id = wds.available_primary_key();
        p.trx_id = get_trx_id();
        p.from = from;
        p.to = to;
        p.quantity = quantity;
        p.create_time = time_point_sec(now());
        // FIXME: state ?
        p.state = 0;
        p.enable = !need_check;
        p.auditor = NIL_ACCOUNT;
    });
}

void pegtoken::deposit(name to, asset quantity, string memo)
{

    STRING_LEN_CHECK(memo, 256)

    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount > 0, "must issue positive quantity");

    { ACCOUNT_CHECK(to) };
    { ACCOUNT_EXCLUDE(to, quantity.symbol.code().raw()) };

    NEED_ACCEPTOR_AUTH(quantity.symbol.code().raw())
    SEND_INLINE_ACTION(*this, transfer, { { iter->acceptor, "active"_n } }, { iter->acceptor, to, quantity, "deposit account:" + to.to_string() + " memo:" + memo });

    auto depo = deposits(get_self(), quantity.symbol.code().raw());
    depo.emplace(iter->acceptor, [&](auto& p) {
        p.id = depo.available_primary_key();
        p.from = iter->acceptor;
        p.to = to.to_string();
        p.quantity = quantity;
        p.msg = memo;
    });
}

void pegtoken::transfer(name from, name to, asset quantity, string memo)
{
    STRING_LEN_CHECK(memo, 256)

    eosio_assert(from != to, "cannot transfer to self");
    require_auth(from);

    { ACCOUNT_CHECK(to) };

    eosio_assert(quantity.is_valid(), "invalid quantity");
    eosio_assert(quantity.amount > 0, "must transfer positive quantity");

    {
        ASSIGN_STATS_AND_ITER(quantity.symbol.code().raw())
        eosio_assert(iter->active, "underwriter is not active");
    };

    auto payer = has_auth(to) ? to : from;
    sub_balance(from, quantity);
    add_balance(to, quantity, payer);

    require_recipient(from);
    require_recipient(to);
}

void pegtoken::clear(symbol_code sym_code, uint64_t num)
{
    NEED_ACCEPTOR_AUTH(sym_code.raw())

    {
        auto depo = deposits(get_self(), sym_code.raw());
        auto delindex = depo.template get_index<"delindex"_n>();
        for (auto i = 0; i < num; ++i) {
            auto to_del = delindex.begin();
            if (to_del == delindex.end()
                || to_del->create_time.utc_seconds + iter->delayday * ONE_DAY > now()) {
                break;
            }
            delindex.erase(to_del);
        }
    }
    {
        auto withd = withdraws(get_self(), sym_code.raw());
        auto delindex = withd.template get_index<"delindex"_n>();
        for (auto i = 0; i < num; ++i) {
            auto to_del = delindex.begin();
            if (to_del == delindex.end()
                || to_del->create_time.utc_seconds + iter->delayday * ONE_DAY > now()
                || (to_del->state != 2 && to_del->state != 3)
                || to_del->quantity >= iter->min_limit) {
                break;
            }
            delindex.erase(to_del);
        }
    }
}

void pegtoken::feedback(symbol_code sym_code, transaction_id_type trx_id, string remote_trx_id, string memo)
{
    auto state = 2;

    STRING_LEN_CHECK(memo, 256)

    NEED_ACCEPTOR_AUTH(sym_code.raw())

    // TODO: check remote_trx_id

    auto withd = withdraws(get_self(), sym_code.raw());
    auto trxids = withd.template get_index<"trxid"_n>();
    auto iter2 = trxids.find(withdraw_ts::trxid(trx_id));
    eosio_assert(iter2 != trxids.end(), "this trx id does not exist");

    // defer delete
    uint128_t sender_id = iter2->id;
    cancel_deferred(sender_id);
    transaction tsn;
    tsn.actions.push_back({ { get_self(), "active"_n }, get_self(), "rmwithdraw"_n,
        std::make_tuple(iter2->id, iter2->quantity.symbol.code()) });
    tsn.delay_sec = iter->delayday * ONE_DAY;
    tsn.send(sender_id, get_self(), true);

    trxids.modify(iter2, same_payer, [&](auto& p) {
        p.state = state;
        p.remote_trx_id = remote_trx_id;
    });
}

void pegtoken::rollback(symbol_code sym_code, transaction_id_type trx_id, string memo)
{
    auto state = 5;
    STRING_LEN_CHECK(memo, 256)

    NEED_ACCEPTOR_AUTH(sym_code.raw())

    auto withd = withdraws(get_self(), sym_code.raw());
    auto trxids = withd.template get_index<"trxid"_n>();
    auto iter2 = trxids.find(withdraw_ts::trxid(trx_id));
    eosio_assert(iter2 != trxids.end(), "this trx id does not exist");

    auto acct = accounts(get_self(), iter->acceptor.value);
    auto const& owner = acct.get(sym_code.raw(), "no balance object found");
    eosio_assert(owner.balance >= iter2->quantity, "acceptor has not enough balance");

    // defer delete
    uint128_t sender_id = iter2->id;
    cancel_deferred(sender_id);
    transaction tsn;
    tsn.actions.push_back({ { get_self(), "active"_n }, get_self(), "rmwithdraw"_n,
        std::make_tuple(iter2->id, iter2->quantity.symbol.code()) });
    tsn.delay_sec = iter->delayday * ONE_DAY;
    tsn.send(sender_id, get_self(), true);

    trxids.modify(iter2, same_payer, [&](auto& p) {
        p.state = state;
    });
}

void pegtoken::setacceptor(symbol_code sym_code, name acceptor)
{
    { ACCOUNT_CHECK(acceptor) };

    NEED_ISSUER_AUTH(sym_code.raw())
    stats_table.modify(iter, same_payer, [&](auto& p) {
        p.acceptor = acceptor;
    });
}

void pegtoken::setdelay(symbol_code sym_code, uint64_t delayday)
{
    NEED_ISSUER_AUTH(sym_code.raw())
    stats_table.modify(iter, same_payer, [&](auto& p) {
        p.delayday = delayday;
    });
}

void pegtoken::lockall(symbol_code sym_code, name auditor)
{
    // FIXME: auth check
    NEED_AUDITOR_AUTH(sym_code.raw(), auditor)
    eosio_assert(iter->active == true, "this token has been locked already");
    stats_table.modify(iter, same_payer, [&](auto& p) { p.active = false; });
}

void pegtoken::unlockall(symbol_code sym_code, name auditor)
{
    // FIXME: auth check
    NEED_AUDITOR_AUTH(sym_code.raw(), auditor)
    eosio_assert(iter->active == false, "this token is not being locked");
    stats_table.modify(iter, same_payer, [&](auto& p) { p.active = true; });
}

void pegtoken::approve(symbol_code sym_code, name auditor, transaction_id_type trx_id, string memo)
{
    STRING_LEN_CHECK(memo, 256)

    {
        NEED_AUDITOR_AUTH(sym_code.raw(), auditor);
        eosio_assert(iter->active, "underwriter is not active");
    };
    auto withd = withdraws(get_self(), sym_code.raw());
    auto trxids = withd.template get_index<"trxid"_n>();
    auto iter2 = trxids.find(withdraw_ts::trxid(trx_id));
    eosio_assert(iter2 != trxids.end(), "invalid trx_id");
    trxids.modify(iter2, same_payer, [&](auto& p) {
        if (iter2->auditor == NIL_ACCOUNT) {
            p.auditor = auditor;
            p.enable = true;
        }
        p.msg = (memo == "" ? p.msg : memo);
    });
}

void pegtoken::unapprove(symbol_code sym_code, name auditor, transaction_id_type trx_id, string memo)
{
    {
        STRING_LEN_CHECK(memo, 256);
        NEED_AUDITOR_AUTH(sym_code.raw(), auditor);
        eosio_assert(iter->active, "underwriter is not active");
    };
    auto withd = withdraws(get_self(), sym_code.raw());
    auto trxids = withd.template get_index<"trxid"_n>();
    auto iter2 = trxids.find(withdraw_ts::trxid(trx_id));
    eosio_assert(iter2 != trxids.end(), "invalid trx_id");
    trxids.modify(iter2, same_payer, [&](auto& p) {
        if (iter2->auditor != NIL_ACCOUNT) {
            p.auditor = auditor;
            p.enable = false;
        }
        p.msg = (memo == "" ? p.msg : memo);
    });
}

void pegtoken::sendback(name auditor, transaction_id_type trx_id, name to, asset quantity, string memo)
{
    {
        STRING_LEN_CHECK(memo, 256);
        ACCOUNT_CHECK(to);
        NEED_AUDITOR_AUTH(quantity.symbol.code().raw(), auditor);
    };

    ACCOUNT_EXCLUDE(to, quantity.symbol.code().raw())

    eosio_assert(quantity.amount > 0, "invalid quantity amount");

    auto withd = withdraws(get_self(), quantity.symbol.code().raw());
    auto trxids = withd.template get_index<"trxid"_n>();
    auto iter2 = trxids.find(withdraw_ts::trxid(trx_id));
    eosio_assert(iter2 != trxids.end(), "invalid trx_id");

    // defer delete
    uint128_t sender_id = iter2->id;
    cancel_deferred(sender_id);
    transaction tsn;
    tsn.actions.push_back({ { get_self(), "active"_n }, get_self(), "rmwithdraw"_n,
        std::make_tuple(iter2->id, iter2->quantity.symbol.code()) });
    tsn.delay_sec = iter->delayday * ONE_DAY;
    tsn.send(sender_id, get_self(), true);

    trxids.modify(iter2, same_payer, [&](auto& p) {
        p.state = 3;
        p.msg = (memo == "" ? p.msg : memo);
    });
}

void pegtoken::rmwithdraw(uint64_t id, symbol_code sym_code)
{
    require_auth(get_self());
    uint128_t sender_id = id;
    cancel_deferred(id);
    auto withd = withdraws(get_self(), sym_code.raw());
    withd.erase(withd.find(id));
}

} // namespace eosio

EOSIO_DISPATCH(eosio::pegtoken, (create)(init)(update)(setlimit)(setauditor)(setfee)(issue)(retire)(applicant)(applyaddr)(assignaddr)(withdraw)(deposit)(transfer)(clear)(feedback)(rollback)(setacceptor)(setdelay)(lockall)(unlockall)(approve)(unapprove)(sendback)(rmwithdraw));