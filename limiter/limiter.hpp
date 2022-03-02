#include <eosio/multi_index.hpp>
#include <eosio/contract.hpp>
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/time.hpp>
#include <eosio/print.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>
#include <string>

class [[eosio::contract]] limiter : public eosio::contract
{

public:
	limiter( eosio::name receiver, eosio::name code, eosio::datastream<const char*> ds ): eosio::contract(receiver, code, ds)
	{}
	[[eosio::action]] void adddebt( eosio::name debtor, eosio::name target, eosio::asset debt, std::string memo );
	[[eosio::action]] void addlimit( eosio::asset limit );
	[[eosio::action]] void addlock( eosio::name account, std::string note );
	[[eosio::action]] void addwhitelist( eosio::name username, eosio::symbol_code currency_code );
	[[eosio::action]] void addwhiteto( eosio::name username, eosio::symbol_code currency_code );

	[[eosio::action]] void deldebt( eosio::name debtor, eosio::name target, eosio::asset debt, std::string memo );
	[[eosio::action]] void rmdebt( eosio::name debtor, uint64_t debt_id);
	[[eosio::action]] void rmlimit( eosio::symbol_code currency_code );
	[[eosio::action]] void rmlock( eosio::name account);
	[[eosio::action]] void rmwhitelist( eosio::name username, eosio::symbol_code currency_code );
	[[eosio::action]] void rmwhiteto( eosio::name username, eosio::symbol_code currency_code );

	[[eosio::action]] void rmusedlimit( eosio::symbol_code currency_code, eosio::name user );
	[[eosio::action]] void rmusedlimits( eosio::symbol_code currency_code, uint32_t user_count );
	[[eosio::action]] void checklimit( eosio::name username, eosio::name to, eosio::asset sum, std::string memo );

	static constexpr eosio::name _self = "limiter"_n;
	static constexpr eosio::symbol_code _untb_symbol_code = eosio::symbol_code("UNTB");
	static constexpr eosio::name _eosiotoken = "eosio.token"_n;
	static constexpr eosio::name _debtadmin = "debtadmin"_n;

	struct [[eosio::table]] currency_stats {
		eosio::asset supply;
		eosio::asset max_supply;
		eosio::name issuer;

		uint64_t primary_key() const {
			return supply.symbol.code().raw();
		}
	};

    struct [[eosio::table]] lock {
		eosio::name account;
		eosio::time_point_sec ts;
		std::string note;

		uint64_t primary_key() const {
			return account.value;
		}
		EOSLIB_SERIALIZE(lock, (account)(ts)(note))
	};

	struct [[eosio::table]] debt {
		uint64_t id;
		eosio::name target;
		eosio::asset sum;
		eosio::time_point_sec ts;
		std::string memo;
		eosio::checksum256 hash;

		uint64_t primary_key() const {
			return id;
		}
		eosio::checksum256 byhash() const {
			return hash;
		}
		EOSLIB_SERIALIZE(debt, (id)(target)(sum)(ts)(memo)(hash))
	};

	struct [[eosio::table]] tokenlimit {
		eosio::asset month_limit;
		eosio::time_point_sec ts;

		uint64_t primary_key() const {
			return month_limit.symbol.code().raw();
		}
		EOSLIB_SERIALIZE(tokenlimit, (month_limit)(ts))
	};

	struct [[eosio::table]] whitelist {
		eosio::name username;
		eosio::time_point_sec ts;

		uint64_t primary_key() const {
			return username.value;
		}
		EOSLIB_SERIALIZE(whitelist, (username)(ts))
	};

	struct [[eosio::table]] usedlimit {
		eosio::name account;
		eosio::asset used_limit;
		eosio::time_point_sec ts;

		uint64_t primary_key() const {
			return account.value;
		}
		EOSLIB_SERIALIZE(usedlimit, (account)(used_limit)(ts))
	};

	struct [[eosio::table]] whitelistto {
		eosio::name username;
		eosio::time_point_sec ts;

		uint64_t primary_key() const {
			return username.value;
		}
		EOSLIB_SERIALIZE(whitelistto, (username)(ts))
	};


	typedef eosio::multi_index< "stat"_n, currency_stats > stats;
	typedef eosio::multi_index< "lock"_n, lock > lock_index;
	typedef eosio::multi_index< "debt"_n, debt
			,eosio::indexed_by<"byhash"_n, eosio::const_mem_fun<debt, eosio::checksum256, &debt::byhash>>
	> debt_index;
	typedef eosio::multi_index< "tokenlimit"_n, tokenlimit > tokenlimit_index;
	typedef eosio::multi_index< "whitelist"_n, whitelist > whitelist_index;
	typedef eosio::multi_index< "usedlimit"_n, usedlimit > usedlimit_index;
	typedef eosio::multi_index< "whitelistto"_n, whitelistto > whitelistto_index;


private:
	eosio::name get_issuer (eosio::symbol_code currency_code);
	bool is_current_month (uint32_t last_utc, uint32_t current_utc);
	eosio::checksum256 calcDebtHash( eosio::name debtor, eosio::name target, eosio::asset debt, std::string memo );
};
