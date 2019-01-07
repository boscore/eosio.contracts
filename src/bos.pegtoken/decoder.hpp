#pragma once

#include <algorithm>
#include <cstdio>
#include <eosiolib/crypto.h>
#include <eosiolib/eosio.hpp>
#include <eosiolib/transaction.h>
#include <string>

namespace eosio {

using std::string;

constexpr char hex_map[] = "0123456789abcdef";

uint8_t hex_to_digit(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'z')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A' + 10;
    eosio_assert(false, "invalid hex type");
    return 0xff;
}

string decode_hex(const void* data, uint64_t len)
{
    auto src = static_cast<const uint8_t*>(data);
    string str(len * 2, 0);
    for (int i = 0; i < len; ++i) {
        str[2 * i] = hex_map[(src[i] & 0xf0) >> 4];
        str[2 * i + 1] = hex_map[src[i] & 0x0f];
    }
    return str;
}

bool valid_bitcoin_addr(string addr)
{
    if (addr.size() < 26 || addr.size() > 35)
        return false;

#ifdef TESTNET
    if (addr[0] != '2' && addr[0] != '9' && addr[0] != 'm' && addr[0] != 'n')
        return false;
#else
    if (addr[0] != '1' && addr[0] != '3')
        return false;
#endif

    auto npos = string::npos;
    if (addr.find('O') != npos || addr.find('I') != npos || addr.find('l') != npos || addr.find('0') != npos)
        return false;

    return true;
}

bool valid_ethereum_addr(string addr)
{
    eosio_assert(addr.size() % 2 == 0, "invalid eth addr");

    auto prefix_index = addr.find("0x");
    if (prefix_index == 0)
        addr = addr.substr(prefix_index + 2);

    eosio_assert(addr.size() == 40, "invalid eth adr len, expected: 40");

    for (auto& r : addr) {
        if ((r >= '0' && r <= '9') || (r >= 'a' && r <= 'f') || (r >= 'A' && r <= 'F'))
            continue;
        return false;
    }
    return true;
}

capi_checksum256 get_trx_id()
{
    capi_checksum256 trx_id;
    std::vector<char> trx_bytes;
    size_t trx_size = transaction_size();
    trx_bytes.resize(trx_size);
    read_transaction(trx_bytes.data(), trx_size);
    sha256(trx_bytes.data(), trx_size, &trx_id);
    return trx_id;
}

string checksum256_to_string(capi_checksum256 src)
{
    return decode_hex(src.hash, 32);
}

uint64_t hash64(const string s)
{
    capi_checksum256 hash256;
    uint64_t res = 0;
    sha256(s.c_str(), s.length(), &hash256);
    std::memcpy(&res, hash256.hash, 8);
    return res;
}

} // namespace eosio