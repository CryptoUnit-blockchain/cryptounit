#include "limiter.hpp"
#include "tm.h"

//using namespace eosio;

[[eosio::action]] void limiter::addlock( eosio::name account, std::string note )
{
	eosio::name ram_payer = _debtadmin;
	if( has_auth( _self ) ) {
		ram_payer = _self;
	} else if( ! has_auth( _debtadmin ) ) {
		eosio::check( false, "missing authority either of debtadmin or limiter" );
	}

	lock_index locks( _self, _self.value );
	auto it = locks.find( account.value ) ;
	if( it == locks.end() ) {
		eosio::check( note.size() > 0, "empty note string" );
		locks.emplace( ram_payer, [&](auto &a) {
			a.account = account;
			a.ts = eosio::time_point_sec( eosio::current_time_point() );
			a.note = note;
		});
	} else {
		eosio::check( note != it->note, "account already locked" );
		locks.modify( it, ram_payer, [&](auto &c) {
			//c.ts = eosio::time_point_sec(eosio::current_time_point());
			c.note = note;
		});
	}
}

[[eosio::action]] void limiter::rmlock( eosio::name account )
{
	eosio::check( has_auth( _debtadmin ) || has_auth( _self ),
			"missing authority either of debtadmin or limiter" );

	lock_index lockstable( _self, _self.value );
	const auto &it = lockstable.find( account.value );
	eosio::check( it != lockstable.end(), "Account not locked" );
	lockstable.erase( it );
}

eosio::checksum256 limiter::calcDebtHash( eosio::name debtor, eosio::name target, eosio::asset debt, std::string memo )
{
	std::string key =
		debtor.to_string() +
		target.to_string() +
		debt.to_string() +
		memo;
	return eosio::sha256( key.c_str(), key.size() );
}

[[eosio::action]] void limiter::adddebt( eosio::name debtor, eosio::name target, eosio::asset debt, std::string memo )
{
	eosio::name ram_payer = _debtadmin;
	if( has_auth( _self ) ) {
		ram_payer = _self;
	} else if( ! has_auth( _debtadmin ) ) {
		eosio::check( false, "missing authority either of debtadmin or limiter" );
	}

	eosio::check( debt.amount > 0, "valid positive debt amount only" );
	debt_index debts( _self, debtor.value );

	eosio::check( memo.length() > 0, "empty memo string" );
	eosio::checksum256 hash = calcDebtHash ( debtor, target, debt, memo );
	auto idx = debts.template get_index<"byhash"_n>();
	auto itr = idx.find( hash );
	eosio::check( itr == idx.end(), "Debt hash is not unique" );
	debts.emplace( ram_payer, [&](auto &a) {
		a.id = debts.available_primary_key();
		a.target = target;
		a.sum = debt;
		a.ts = eosio::time_point_sec( eosio::current_time_point() );
		a.memo = memo;
		a.hash = hash;
	});
}

[[eosio::action]] void limiter::deldebt( eosio::name debtor, eosio::name target, eosio::asset debt, std::string memo )
{
	eosio::check( has_auth( _debtadmin ) || has_auth( _self ),
			"missing authority either of debtadmin or limiter" );

	debt_index debts( _self, debtor.value );
	eosio::checksum256 hash = calcDebtHash ( debtor, target, debt, memo );
	auto idx = debts.template get_index<"byhash"_n>();
	auto itr = idx.find( hash );
	eosio::check( itr != idx.end(), "Debt not found" );

	idx.erase( itr );
}

[[eosio::action]] void limiter::rmdebt( eosio::name debtor, uint64_t debt_id )
{
	eosio::check( has_auth( _debtadmin ) || has_auth( _self ), "missing authority either of debtadmin or limiter" );

	debt_index debts( _self, debtor.value );
	const auto &it = debts.find( debt_id );
	eosio::check( it != debts.end(), "debt not found" );

	debts.erase( it );
}

eosio::name limiter::get_issuer ( eosio::symbol_code currency_code )
{
	stats statstable( "eosio.token"_n, currency_code.raw() );
	auto existing = statstable.find( currency_code.raw() );
	if( existing == statstable.end()) {
		eosio::check( false,  "Unknown token " + currency_code.to_string() );
	}
	return existing->issuer;
}

[[eosio::action]] void limiter::addlimit( eosio::asset limit )
{
	eosio::symbol_code currency_code( limit.symbol.code() );
	eosio::name ram_payer = get_issuer( currency_code );
	if( has_auth( _self ) ) {
		ram_payer = _self;
	} else if( ! has_auth( ram_payer )) {
		eosio::check( false, "missing authority either of token issuer or limiter" );
	}

	eosio::check( currency_code != _untb_symbol_code, "UNTB limit is not supported" );

	tokenlimit_index limits_table(_self, _self.value);
	auto token_limit = limits_table.find( currency_code.raw() );

	eosio::time_point_sec ct( eosio::current_time_point() );

	if ( token_limit == limits_table.end() ) {
		limits_table.emplace( ram_payer, [&](auto &c) {
			c.month_limit = limit;
			c.ts = ct;
		});
	} else {
		limits_table.modify( token_limit, ram_payer, [&](auto &c) {
			c.month_limit = limit;
			c.ts = ct;
		});
	}
}

[[eosio::action]] void limiter::rmlimit(eosio::symbol_code currency_code)
{
	eosio::check( has_auth( get_issuer( currency_code ) ) || has_auth( _self ),
			"missing authority either of token issuer or limiter" );

	tokenlimit_index table( _self, _self.value );
	auto pos = table.find( currency_code.raw() );

	eosio::check( pos != table.end(), "Limit does not set" );
	table.erase (pos);
}

[[eosio::action]] void limiter::rmusedlimit( eosio::symbol_code currency_code, eosio::name user )
{
	eosio::check( has_auth( user ) || has_auth( get_issuer(currency_code) ) || has_auth( _self ),
			"missing authority either of user, token issuer or limiter" );

	tokenlimit_index tokenlimits( _self, _self.value );
	auto iter_tokenlimit = tokenlimits.find( currency_code.raw() );
	eosio::check( iter_tokenlimit == tokenlimits.end(), "Limit is active" );

	usedlimit_index usedlimits_table( _self, currency_code.raw() );
	auto itr = usedlimits_table.find( user.value );
	eosio::check( itr != usedlimits_table.end(), "Used limit not found" );
	usedlimits_table.erase( itr );
}

[[eosio::action]] void limiter::rmusedlimits( eosio::symbol_code currency_code, uint32_t user_count )
{
	eosio::check(  has_auth( _self ) || has_auth( get_issuer(currency_code) ),
			"missing authority either of token issuer or limiter" );

	usedlimit_index usedlimits_table( _self, currency_code.raw() );
	auto itr = usedlimits_table.begin();
	eosio::check( itr != usedlimits_table.end(), "Used limit(s) not found" );

	uint32_t i = 0;
	do {
		itr = usedlimits_table.erase( itr );
		++i;
	} while( itr != usedlimits_table.end() && i < user_count );
}

[[eosio::action]] void limiter::addwhitelist(eosio::name username, eosio::symbol_code currency_code)
{
	eosio::name ram_payer = get_issuer( currency_code );
	if( has_auth( _self ) ) {
		ram_payer = _self;
	} else if( ! has_auth( ram_payer )) {
		eosio::check( false, "missing authority either of token issuer or limiter" );
	}

	eosio::check( currency_code != _untb_symbol_code, "UNTB is not supported" );

	whitelist_index table(_self, currency_code.raw());
	auto pos = table.find( username.value );

	eosio::check( pos==table.end(), "User already in whitelist" );

	table.emplace( ram_payer, [&](auto &c ) {
		c.username  = username;
		c.ts = eosio::time_point_sec(eosio::current_time_point());
	});
}

[[eosio::action]] void limiter::addwhiteto(eosio::name username, eosio::symbol_code currency_code)
{
	eosio::name ram_payer = get_issuer( currency_code );
	if( has_auth( _self ) ) {
		ram_payer = _self;
	} else if( ! has_auth( ram_payer )) {
		eosio::check( false, "missing authority either of token issuer or limiter" );
	}

	eosio::check( currency_code != _untb_symbol_code, "UNTB is not supported" );

	whitelistto_index table(_self, currency_code.raw());
	auto pos = table.find( username.value );

	eosio::check( pos==table.end(), "User already in whitelist" );

	table.emplace( ram_payer, [&](auto &c ) {
		c.username  = username;
		c.ts = eosio::time_point_sec(eosio::current_time_point());
	});
}

[[eosio::action]] void limiter::rmwhitelist(eosio::name username, eosio::symbol_code currency_code)
{
	eosio::check( has_auth( get_issuer (currency_code) ) || has_auth( _self ),
			"missing authority either of token issuer or limiter" );

	whitelist_index table(_self, currency_code.raw());
	auto pos = table.find(username.value);

	eosio::check( pos!=table.end(), "User not in whitelist" );
	table.erase (pos);
}

[[eosio::action]] void limiter::rmwhiteto(eosio::name username, eosio::symbol_code currency_code)
{
	eosio::check( has_auth( get_issuer (currency_code) ) || has_auth( _self ),
			"missing authority either of token issuer or limiter" );

	whitelistto_index table(_self, currency_code.raw());
	auto pos = table.find(username.value);

	eosio::check( pos!=table.end(), "User not in whitelistto" );
	table.erase (pos);
}

bool limiter::is_current_month (uint32_t last_utc, uint32_t current_utc)
{
	tm last_tm;
	if (! __secs_to_tm(last_utc ,&last_tm) ) {
		return true;
	}

	tm current_tm;
	if (! __secs_to_tm(current_utc,&current_tm) ) {
		return true;
	}
#ifdef TEST_CONTRACT
	//check that hour is matching
	return (current_tm.tm_hour == last_tm.tm_hour);
#else
	//check that year and months are matching
	return (current_tm.tm_year==last_tm.tm_year && current_tm.tm_mon==last_tm.tm_mon);
#endif
}


[[eosio::action]] void limiter::checklimit(eosio::name username, eosio::name to, eosio::asset sum, std::string memo)
{
#ifdef TEST_CONTRACT
	eosio::check( has_auth( _self ) || has_auth( _eosiotoken ), "missing authority either of limiter or eosio.token" );
#else
	require_auth( _eosiotoken );
#endif

	bool debt_return_only = false;

    lock_index lockstable( _self, _self.value );
    const auto& lock_iter = lockstable.find( username.value );
    if (lock_iter != lockstable.end()) {
    	debt_return_only = true;
    }

	debt_index debts( _self, username.value );
	if( debts.begin() != debts.end() ) {

		eosio::checksum256 hash = calcDebtHash ( username, to, sum, memo );
		auto idx = debts.template get_index<"byhash"_n>();
		auto debt_iter = idx.find( hash );

		eosio::check( debt_iter != idx.end(), "Transfer valid for debt return only" );

		idx.erase( debt_iter );

	} else {
		eosio::check( ! debt_return_only, "Account is locked" );
	}

	eosio::symbol_code currency_code = sum.symbol.code();
	if( currency_code == _untb_symbol_code ) {
		// Limit for UNTB not set
		return;
	}

	tokenlimit_index limits(_self, _self.value);
	auto token_limit = limits.find(currency_code.raw());
	if( token_limit == limits.end() ) {
		// Limit for the currency not set
		return;
	}

	whitelistto_index whiteliststo_table(_self, currency_code.raw());
	if( whiteliststo_table.find( to.value ) != whiteliststo_table.end() ) {
		// target account in white list for the currency
		return;
	}

	whitelist_index whitelists_table(_self, currency_code.raw());
	if( whitelists_table.find( username.value ) != whitelists_table.end() ) {
		// User in white list for the currency
		return;
	}

	usedlimit_index usedlimits( _self, currency_code.raw() );
	auto used_limit = usedlimits.find( username.value );

	int64_t used_amount = 0;
	bool same_month = false;
	eosio::time_point_sec ct(eosio::current_time_point());

	if( used_limit != usedlimits.end() ) {
		same_month = is_current_month ( used_limit->ts.utc_seconds, ct.utc_seconds );
		if( same_month ) {
			used_amount = used_limit->used_limit.amount;
		}
	}

	if( used_amount + sum.amount > token_limit->month_limit.amount ) {
		eosio::asset limit_asset = token_limit->month_limit;
		std::string msg = "Token transfer limit exceeded, max month transfer ";
		msg += limit_asset.to_string();
		if( used_amount > 0 ) {
			msg += ", used ";
			limit_asset.amount = used_amount;
			msg += limit_asset.to_string();

			msg += ", possible ";
			limit_asset.amount = token_limit->month_limit.amount - used_amount;
			msg += limit_asset.to_string();

		}
		eosio::check( false, msg );
	}

	if( used_limit != usedlimits.end() ) {
		usedlimits.modify( used_limit, _self, [&](auto &c) {
			if( same_month ) {
				c.used_limit.amount += sum.amount;
			} else {
				c.used_limit.amount = sum.amount;
			}
			c.ts = ct;
		});
	} else {
		usedlimits.emplace( _self, [&](auto &c) {
			c.account = username;
			c.used_limit = sum;
			c.ts = ct;
		});
	}
}

extern "C" void apply( uint64_t receiver, uint64_t code, uint64_t action )
{
	if (code == limiter::_self.value) {
		if (action == "checklimit"_n.value) {
			execute_action(eosio::name(receiver), eosio::name(code), &limiter::checklimit);

		} else if (action == "adddebt"_n.value) {
			execute_action(eosio::name(receiver), eosio::name(code), &limiter::adddebt);
		} else if (action == "addlimit"_n.value) {
			execute_action(eosio::name(receiver), eosio::name(code), &limiter::addlimit);
		} else if (action == "addlock"_n.value) {
			execute_action(eosio::name(receiver), eosio::name(code), &limiter::addlock);
		} else if (action == "addwhitelist"_n.value) {
			execute_action(eosio::name(receiver), eosio::name(code), &limiter::addwhitelist);
		} else if (action == "addwhiteto"_n.value) {
			execute_action(eosio::name(receiver), eosio::name(code), &limiter::addwhiteto);

		} else if (action == "deldebt"_n.value) {
			execute_action(eosio::name(receiver), eosio::name(code), &limiter::deldebt);
		} else if (action == "rmdebt"_n.value) {
			execute_action(eosio::name(receiver), eosio::name(code), &limiter::rmdebt);
		} else if (action == "rmlimit"_n.value) {
			execute_action(eosio::name(receiver), eosio::name(code), &limiter::rmlimit);
		} else if (action == "rmlock"_n.value) {
			execute_action(eosio::name(receiver), eosio::name(code), &limiter::rmlock);
		} else if (action == "rmwhitelist"_n.value) {
			execute_action(eosio::name(receiver), eosio::name(code), &limiter::rmwhitelist);
		} else if (action == "rmwhiteto"_n.value) {
			execute_action(eosio::name(receiver), eosio::name(code), &limiter::rmwhiteto);
		} else if (action == "rmusedlimits"_n.value) {
			execute_action(eosio::name(receiver), eosio::name(code), &limiter::rmusedlimits);
		} else if (action == "rmusedlimit"_n.value) {
			execute_action(eosio::name(receiver), eosio::name(code), &limiter::rmusedlimit);
		}
	}
};
