# BOS Airdrop Unactivated Account Burning Scheme

[中文](./README_CN.md)

## Process
1. Generate a list of accounts to be burned
2. Import the list of accounts to be burned into `burn.bos` contract
3. Involve community to verifies burning accounts and contract codes
4. Update burn contract `eosio.system`, `eosio.token` contract
5. Execute Burning
6. End


# Create ready-to-burn account list

## Obtain snapshot airdrop accounts

Put airdrop files into `dataset` directory

```
mkdir dataset && cd dataset
git clone  https://github.com/boscore/bos-airdrop-snapshots
cp ./bos-airdrop-snapshots/accounts_info_bos_snapshot.airdrop.msig.json 
cp ./bos-airdrop-snapshots/accounts_info_bos_snapshot.airdrop.normal.csv 
```

Use [bos.burn](https://github.com/boscore/bos/releases/tag/burn-v1.0.0) `nodeos` to export `auth_sequence=0||auth_sequence=2` accounts;
Burning deadline is `2019-11-27 03:26:27 UTC-0`, `54,171,828`,you can use previous `snapshot` before the deadline to start `bos.burn` nodeos , and specify ` --netruncate-at-block=54171828`

docker-compose.yml Example:
```
version: "3"

services:
  burnnode:
    image: boscore/burnbos:v1.0.2
    command: /opt/eosio/bin/nodeosd.sh --data-dir /opt/eosio/bin/data-dir --max-irreversible-block-age=5000000 --max-transaction-time=100000 --wasm-runtime wabt --netruncate-at-block=321899 --snapshot /opt/eosio/bin/data-dir/snapshots/snapshot-2019112702.bin
    hostname: burnnode
    ports:
      - 280:80
      - 643:443
    environment:
      - NODEOSPORT=80
      - WALLETPORT=8888
    volumes:
      - /data/bos/fullnode:/opt/eosio/bin/data-dir
```

Note:
* `bos.burn` corresponding Docker Image is: boscore/burnbos:v1.0.0
* `snapshot` we can use EOSNation provide snapshot-2019-11-26-15-bos.bin.bz2
* [BlockGo Download address](https://osshkbk01.oss-cn-hongkong.aliyuncs.com/bos/mainnet/snapshot-2019-11-26-15-bos.bin.bz2)
* [EOSNation Download address](https://eosn.sfo2.digitaloceanspaces.com/snapshots/snapshot-2019-11-26-15-bos.bin.bz2)


You can use `get_info` API to verify whether `head` has reached the particular height:

```
curl 127.0.0.1:280/v1/chain/get_info
```

Call the following interface to generate an intermediate file:

```
curl 127.0.0.1:280/v1/chain/get_unused_accounts 
#OR
curl -X POST --url http://127.0.0.1:8888/v1/chain/get_unused_accounts  -d '{
  "file_path": "/xxx/xxx/nonactivated_bos_accounts.txt"  
}'
```

Output Files:
* nonactivated_bos_accounts.txt
* nonactivated_bos_accounts.msig

Move the output files into `dataset` directory, and move [unionset.py](https://github.com/boscore/bos.contracts/blob/bos.burn/contracts/bos.burn/scripts/unionset.py) into `dataset`, and execute:

```
python3 unionset.py
```

Output Files
* unactive_airdrop_accounts.csv


# Burn

## Create Burning Account

Burning contract will be deployed on `burn.bos`, any account can trigger burning after initialization

Smart Contract with burning function:[bos.contracts/bos.burn](https://github.com/boscore/bos.contracts/tree/bos.burn/contracts/bos.burn), burn-v1.0.0
Corresponding Compiled Code:[bos.contract-prebuild/bos.burn](https://github.com/boscore/bos.contract-prebuild/tree/bos.burn), burn-v1.0.0

## Deploy Contract

### Upgrade `burn.bos` contract

```
git clone https://github.com/boscore/bos.contract-prebuild.git

CONTRACTS_FOLDER='./bos.contract-prebuild' 
cd ${CONTRACTS_FOLDER}
git checkout bos.burn && git pull origin bos.burn

cleos set contract burn.bos bos.burn -p burn.bos
```

### burn.bos import unactivated accounts

Use [burntool.sh](https://github.com/boscore/bos.contracts/blob/bos.burn/contracts/bos.burn/scripts/burntool.sh) to import accounts.

Adjust the variable in `burntool.sh`:`http_endpoint`, put `burntool.sh` and  `unactive_airdrop_accounts.csv` under same directory and execute

```
bash burntool.sh imp
```

_Beware: before importing, make sure `burn.bos` has sufficient resource:_
* RAM, number of unactivated accounts is less than, we have to purchase 130M RAM;
* CPU 40s;
* NET 16MB;
* It should takes 30 minutes to finish whole importing process

### Open to Community for 24 hours

Alter the permission of `burn.bos` to `eosio`, and start `eosio.code` permission for community verification.

```
# resign active
cleos set account permission burn.bos active '{"threshold": 1,"keys": [],"accounts": [{"permission":{"actor":"burn.bos","permission":"eosio.code"},"weight":1},{"permission":{"actor":"eosio","permission":"active"},"weight":1}]}' owner -p burn.bos@owner
# resign owner
cleos set account permission burn.bos owner '{"threshold": 1,"keys": [],"accounts": [{"permission":{"actor":"eosio","permission":"active"},"weight":1}]}' -p burn.bos@owner
```

 Compare `unactive_airdrop_accounts.csv` (created by community member) with [burnboschecktool.py](https://github.com/boscore/bos.contracts/blob/bos.burn/contracts/bos.burn/scripts/burnboschecktool.py) `burn.bos`:

```
python3 burnboschecktool.py
```

Account to be burned [unactive_airdrop_accounts.csv](https://github.com/boscore/bos-airdrop-snapshots/blob/master/unactive_airdrop_accounts.csv) of block height 54171828, you can use this file to compare with the result.


### Upgrade eosio.system 

After community has verified [bos.contract-prebuild/bos.burn](https://github.com/boscore/bos.contract-prebuild/tree/bos.burn), use compiled version to upgraded `eosio.system` and `eosio.token` 

Prepare `bp.json` for top 30 Block Producers:
```
[
    {"actor":"bponeoneonee","permission":"active"},
    ....
]
```

Multisig to upgrade the system contract

```
# burnbosooooo is a common acount

CONTRACTS_FOLDER='./bos.contract-prebuild' 
cd ${CONTRACTS_FOLDER}
git checkout bos.burn && git pull origin bos.burn

# upgrade eosio contract
cleos set contract eosio eosio.system -p eosio -s -j -d > updatesys.json
# update updatesys.json expire time
cleos multisig propose_trx updatesys ../bp.json updatesys.json burnbosooooo
# let BPs approve
cleos multisig approve burnbosooooo updatesys '{"actor":"bponeoneonee","permission":"active"}' -p bponeoneonee@active
cleos multisig exec burnbosooooo updatesys -p burnbosooooo@active

# upgrade eosio.token contract
cleos set contract eosio.token eosio.token -p eosio.token -s -j -d > updatetoken.json
# update updatetoken.json expire time
cleos multisig propose_trx updatetoken ../bp.json updatetoken.json burnbosooooo
# let BPs approve
cleos multisig approve burnbosooooo updatetoken '{"actor":"bponeoneonee","permission":"active"}' -p bponeoneonee@active
cleos multisig exec burnbosooooo updatetoken -p burnbosooooo@active
```

Trigger multisig upgrade for `eosio.system` and `eosio.token`, and wait for Block Producers to approve 

### Start `burn.bos` burn

In order to ensure more transparency and openness, after BP multi-signature allows burning, any account can initiate burning actions.

```
# set burning account: burn.bos
cleos multisig propose enablebrun ../bp.json '[{"actor": "burn.bos", "permission": "active"}]' burn.bos setparameter '{"version":1,"executer":"burn.bos"}' burnbosooooo 336 -p  burnbosooooo@active
# review proposal
cleos multisig review burnbosooooo enablebrun
# approve proposal
cleos multisig approve burnbosooooo enablebrun  '{"actor":"bponeoneonee","permission":"active"}' -p bponeoneonee@active 
# exec proposal
cleos multisig exec burnbosooooo enablebrun -p burnbosooooo@active
```

After setting up the burning account, you can burn:

```
bash burntool.sh air
```

_Note: Before performing the import, make sure that `burn.bos` has sufficient resources:_
* Modify `burn_trigger_act` to specify the burning initialization account
* RAM,`burn.bos` need to purchase 140M RAM;
* CPU 10min;
* NET 200MB;
* The entire import process should be completed within 3 hours

### Check Burning Result

```
bash burntool.sh chk
```

To save the unactive account into `unactive_airdrop_accounts_result.csv`, we can start to burn ths list of accounts until success:

```
mv ./unactive_airdrop_accounts.csv ./unactive_airdrop_accounts.csv.old
mv ./unactive_airdrop_accounts_result.csv ./unactive_airdrop_accounts.csv
bash burntool.sh air
```

### Burn the remaining balance of `hole.bos`, and adjust the `quantity` in the proposal:


```
# burn hole.bos
cleos multisig propose holebos ../bp.json '[{"actor": "burn.bos", "permission": "active"}]' burn.bos burn '{"quantity":"6896959.3921 BOS"}' burnbosooooo 336 -p  burnbosooooo@active
# review proposal
cleos multisig review burnbosooooo holebos
# approve proposal
cleos multisig approve burnbosooooo holebos  '{"actor":"bponeoneonee","permission":"active"}' -p bponeoneonee@active 
# exec proposal
cleos multisig exec burnbosooooo holebos -p burnbosooooo@active
```

# Summary

### Recover System Contract

Use compiled version [bos.contract-prebuild/master](https://github.com/boscore/bos.contract-prebuild/tree/master) to upgrade `eosio.system` and `eosio.token` contract.


```
CONTRACTS_FOLDER='./bos.contract-prebuild' 
cd ${CONTRACTS_FOLDER}
git checkout master && git pull origin master

# upgrade eosio contract
cleos set contract eosio eosio.system -p eosio -s -j -d > updatesys.json
# update updatesys.json expire time
cleos multisig propose_trx updatesys ../bp.json updatesys.json burnbosooooo
# let BPs approve
cleos multisig approve burnbosooooo updatesys '{"actor":"bponeoneonee","permission":"active"}' -p bponeoneonee@active
cleos multisig exec burnbosooooo updatesys -p burnbosooooo@active

# upgrade eosio.token contract
cleos set contract eosio.token eosio.token -p eosio.token -s -j -d > updatetoken.json
# update updatetoken.json expire time
cleos multisig propose_trx updatetoken ../bp.json updatetoken.json burnbosooooo
# let BPs approve
cleos multisig approve burnbosooooo updatetoken '{"actor":"bponeoneonee","permission":"active"}' -p bponeoneonee@active
cleos multisig exec burnbosooooo updatetoken -p burnbosooooo@active
```

### Release burn.bos Resource

```
bash burntool.sh clr
```

To launch BP multisig to recover `active` permission of `burn.bos`, so as to restore resource of `burn.bos`:

```
cleos multisig propose upactive bp.json '[{"actor": "burn.bos", "permission": "owner"}]' eosio updateauth '{"account":"burn.bos","permission":"active","parent":"owner","auth":{"threshold":1,"keys":[{"key":"EOS8FsuQAe7vXzYnGWoDXtdMgTXc2Hv9ctqAMvtRPrYAvn17nCftR","weight":"1"}],"accounts":[],"waits":[]}}' burnbosooooo 144 -p burnbosooooo@active

cleos multisig approve burnbosooooo upactive '{"actor":"eosio","permission":"active"}' -p eosio@active

cleos multisig exec burnbosooooo upactive -p burnbosooooo@active
```
