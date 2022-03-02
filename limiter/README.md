eosio-cpp -abigen limiter.cpp  -I./ -o ./limiter.wasm
cleos -u https://api-uatbc.otcdesk.ch set contract limiter limiter/ -p limiter

cleos -u https://api-uatbc.otcdesk.ch  push action limiter transfer '[ "CRU"]' -p limiter

UAT key:
limiter:EOS7JQU2NWpYLzYweVncr9xZ6w4feKkqZtyti2bRg7EAEKik2bK61:5JLV2DGQ6gbFybag9bM8xdSNMnjBGRVTKnjJvPQkRs68NRTZTyN

###locks control

If a user is added to the contract lock table, transfer to any account is prohibited.

###debts control

debtadmin user can registrate debtors and restrict any transfers except debt return.

###tokenlimit control

token issuer can set monthly limit for user transfers.

Users whitelist for unlimited transfers also under control of the token issuer.
