/* nfa.cc -- operations for NFA
 *
 * Copyright (c) 2018 Ondrej Lengal <ondra.lengal@gmail.com>
 *
 * This file is a part of libvata2.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <algorithm>
#include <list>
#include <unordered_set>

// VATA headers
#include <vata2/nfa.hh>
#include <vata2/util.hh>
#include <vata2/vm-dispatch.hh>

using std::tie;

using namespace Vata2::util;
using namespace Vata2::Nfa;
using Vata2::Nfa::Symbol;

const std::string Vata2::Nfa::TYPE_NFA = "NFA";

std::ostream& std::operator<<(std::ostream& os, const Vata2::Nfa::Trans& trans)
{ // {{{
	std::string result = "(" + std::to_string(trans.src) + ", " +
		std::to_string(trans.symb) + ", " + std::to_string(trans.tgt) + ")";
	return os << result;
} // operator<<(ostream, Trans) }}}


Symbol OnTheFlyAlphabet::translate_symb(const std::string& str)
{ // {{{
	auto it_insert_pair = symbol_map->insert({str, cnt_symbol});
	if (it_insert_pair.second) { return cnt_symbol++; }
	else { return it_insert_pair.first->second; }
} // OnTheFlyAlphabet::translate_symb }}}

std::list<Symbol> OnTheFlyAlphabet::get_symbols() const
{ // {{{
	std::list<Symbol> result;
	for (const auto& str_sym_pair : *(this->symbol_map))
	{
		result.push_back(str_sym_pair.second);
	}

	return result;
} // OnTheFlyAlphabet::get_symbols }}}

std::list<Symbol> OnTheFlyAlphabet::get_complement(
	const std::set<Symbol>& syms) const
{ // {{{
	std::list<Symbol> result;

	// TODO: could be optimized
	std::set<Symbol> symbols_alphabet;
	for (const auto& str_sym_pair : *(this->symbol_map))
	{
		symbols_alphabet.insert(str_sym_pair.second);
	}

	std::set_difference(
		symbols_alphabet.begin(), symbols_alphabet.end(),
		syms.begin(), syms.end(),
		std::inserter(result, result.end()));

	return result;
} // OnTheFlyAlphabet::get_complement }}}

std::list<Symbol> EnumAlphabet::get_symbols() const
{ // {{{
	std::list<Symbol> result;
	for (const auto& str_sym_pair : this->symbol_map)
	{
		result.push_back(str_sym_pair.second);
	}

	return result;
} // EnumAlphabet::get_symbols }}}

std::list<Symbol> EnumAlphabet::get_complement(
	const std::set<Symbol>& syms) const
{ // {{{
	std::list<Symbol> result;

	// TODO: could be optimized
	std::set<Symbol> symbols_alphabet;
	for (const auto& str_sym_pair : this->symbol_map)
	{
		symbols_alphabet.insert(str_sym_pair.second);
	}

	std::set_difference(
		symbols_alphabet.begin(), symbols_alphabet.end(),
		syms.begin(), syms.end(),
		std::inserter(result, result.end()));

	return result;
} // EnumAlphabet::get_complement }}}


std::list<Symbol> CharAlphabet::get_symbols() const
{ // {{{
	std::list<Symbol> result;
	for (size_t i = 0; i < 256; ++i)
	{
		result.push_back(i);
	}

	return result;
} // CharAlphabet::get_symbols }}}

std::list<Symbol> CharAlphabet::get_complement(
	const std::set<Symbol>& syms) const
{ // {{{
	std::list<Symbol> result;

	std::list<Symbol> symb_list = this->get_symbols();

	// TODO: could be optimized
	std::set<Symbol> symbols_alphabet(symb_list.begin(), symb_list.end());

	std::set_difference(
		symbols_alphabet.begin(), symbols_alphabet.end(),
		syms.begin(), syms.end(),
		std::inserter(result, result.end()));

	return result;
} // CharAlphabet::get_complement }}}


void Nfa::add_trans(const Trans& trans)
{ // {{{
	auto it = this->transitions.find(trans.src);
	if (it != this->transitions.end())
	{
		PostSymb& post = it->second;
		auto jt = post.find(trans.symb);
		if (jt != post.end())
		{
			jt->second.insert(trans.tgt);
		}
		else
		{
			post.insert({trans.symb, StateSet({trans.tgt})});
		}
	}
	else
	{
		this->transitions.insert(
			{trans.src, PostSymb({{trans.symb, StateSet({trans.tgt})}})});
	}
} // add_trans }}}

bool Nfa::has_trans(const Trans& trans) const
{ // {{{
	auto it = this->transitions.find(trans.src);
	if (it == this->transitions.end())
	{
		return false;
	}

	const PostSymb& post = it->second;
	auto jt = post.find(trans.symb);
	if (jt == post.end())
	{
		return false;
	}

	return haskey(jt->second, trans.tgt);
} // has_trans }}}


size_t Nfa::trans_size() const
{ // {{{
	size_t cnt = 0;
	for (const auto& state_post_symb_map_pair : this->transitions)
	{
		const PostSymb& symb_set_map = state_post_symb_map_pair.second;
		for (const auto& symb_state_set_pair : symb_set_map)
		{
			cnt += symb_state_set_pair.second.size();
		}
	}

	return cnt;
} // trans_size() }}}


Nfa::const_iterator Nfa::const_iterator::for_begin(const Nfa* nfa)
{ // {{{
	assert(nullptr != nfa);

	const_iterator result;
	if (nfa->transitions.empty())
	{
		result.is_end = true;
		return result;
	}

	result.nfa = nfa;
	result.stpmIt = nfa->transitions.begin();
	const PostSymb& post = result.stpmIt->second;
	assert(!post.empty());
	result.psIt = post.begin();
	const StateSet& state_set = result.psIt->second;
	assert(!state_set.empty());
	result.ssIt = state_set.begin();

	result.refresh_trans();

	return result;
} // for_begin }}}

Nfa::const_iterator Nfa::const_iterator::for_end(const Nfa* /* nfa*/)
{ // {{{
	const_iterator result;
	result.is_end = true;
	return result;
} // for_end }}}

Nfa::const_iterator& Nfa::const_iterator::operator++()
{ // {{{
	assert(nullptr != nfa);

	++(this->ssIt);
	const StateSet& state_set = this->psIt->second;
	assert(!state_set.empty());
	if (this->ssIt != state_set.end())
	{
		this->refresh_trans();
		return *this;
	}

	// out of state set
	++(this->psIt);
	const PostSymb& post_map = this->stpmIt->second;
	assert(!post_map.empty());
	if (this->psIt != post_map.end())
	{
		const StateSet& new_state_set = this->psIt->second;
		assert(!new_state_set.empty());
		this->ssIt = new_state_set.begin();

		this->refresh_trans();
		return *this;
	}

	// out of post map
	++(this->stpmIt);
	assert(!this->nfa->transitions.empty());
	if (this->stpmIt != this->nfa->transitions.end())
	{
		const PostSymb& new_post_map = this->stpmIt->second;
		assert(!new_post_map.empty());
		this->psIt = new_post_map.begin();
		const StateSet& new_state_set = this->psIt->second;
		assert(!new_state_set.empty());
		this->ssIt = new_state_set.begin();

		this->refresh_trans();
		return *this;
	}

	// out of transitions
	this->is_end = true;

	return *this;
} // operator++ }}}


StateSet Nfa::post(
	const StateSet&  macrostate,
	Symbol           sym) const
{ // {{{
	StateSet result;
	for (State state : macrostate)
	{
		const PostSymb* post_s = this->post(state);
		if (nullptr != post_s)
		{
			auto it = post_s->find(sym);
			if (post_s->end() != it)
			{
				result.insert(it->second.begin(), it->second.end());
			}
		}
	}

	return result;
} // post }}}


std::ostream& Vata2::Nfa::operator<<(std::ostream& os, const Nfa& nfa)
{ // {{{
	return os << std::to_string(serialize(nfa));
} // Nfa::operator<<(ostream) }}}


bool Vata2::Nfa::are_state_disjoint(const Nfa& lhs, const Nfa& rhs)
{ // {{{
	// fill lhs_states with all states of lhs
	std::unordered_set<State> lhs_states;
	lhs_states.insert(lhs.initialstates.begin(), lhs.initialstates.end());
	lhs_states.insert(lhs.finalstates.begin(), lhs.finalstates.end());

	for (const auto& trans : lhs) {
		lhs_states.insert({trans.src, trans.tgt});
	}

	// for every state found in rhs, check its presence in lhs_states
	for (const auto& rhs_st : rhs.initialstates) {
		if (haskey(lhs_states, rhs_st)) { return false; }
	}

	for (const auto& rhs_st : rhs.finalstates) {
		if (haskey(lhs_states, rhs_st)) { return false; }
	}

	for (const auto& trans : rhs) {
		if (haskey(lhs_states, trans.src) || haskey(lhs_states, trans.tgt)) {
			return false;
		}
	}

	// no common state found
	return true;
} // are_disjoint }}}


void Vata2::Nfa::union_norename(
	Nfa*        result,
	const Nfa&  lhs,
	const Nfa&  rhs)
{ // {{{
	assert(nullptr != result);

	result->initialstates.insert(
		lhs.initialstates.cbegin(),
		lhs.initialstates.cend());

	result->initialstates.insert(
		rhs.initialstates.cbegin(),
		rhs.initialstates.cend());

	result->finalstates.insert(
		lhs.finalstates.cbegin(),
		lhs.finalstates.cend());

	result->finalstates.insert(
		rhs.finalstates.cbegin(),
		rhs.finalstates.cend());

	for (const auto& trans : lhs) { result->add_trans(trans); }
	for (const auto& trans : rhs) { result->add_trans(trans); }
} // union_norename }}}


namespace {
/// Copies @p src to @p result while peforming a (Haskell-like) map @p f on states
template <class Func>
void copy_state_map(Nfa* result, const Nfa& src, Func f)
{ // {{{
	assert(nullptr != result);

	for (State st : src.initialstates) { result->initialstates.insert(f(st)); }
	for (State st : src.finalstates) { result->finalstates.insert(f(st)); }

	for (const Trans& tr : src) {
		result->add_trans(f(tr.src), tr.symb, f(tr.tgt));
	}
} // copy_state_map }}}
}


Nfa Vata2::Nfa::union_rename(
	const Nfa&  lhs,
	const Nfa&  rhs)
{ // {{{
	Nfa result;
	State cnt = 0;
	std::unordered_map<State, State> dict;

	// function used to encapsulate translation of states
	auto transl = [&cnt, &dict](State st) {
		auto it_bool = dict.insert({ st, cnt });
		if (it_bool.second) { // if insertion happened
			++cnt;
		}

		return it_bool.first->second;
	};

	copy_state_map(&result, lhs, transl);
	dict.clear();
	copy_state_map(&result, rhs, transl);

	return result;
} // union_rename }}}


void Vata2::Nfa::intersection(
	Nfa*         result,
	const Nfa&   lhs,
	const Nfa&   rhs,
	ProductMap*  prod_map)
{ // {{{
	bool remove_prod_map = false;
	if (nullptr == prod_map)
	{
		remove_prod_map = true;
		prod_map = new ProductMap();
	}

	// counter for names of new states
	State cnt_state = 0;
	// list of elements the form <lhs_state, rhs_state, result_state>
	std::list<std::tuple<State, State, State>> worklist;

	// translate initial states and initialize worklist
	for (const auto& lhs_st : lhs.initialstates)
	{
		for (const auto& rhs_st : rhs.initialstates)
		{
			prod_map->insert({{lhs_st, rhs_st}, cnt_state});
			result->initialstates.insert(cnt_state);
			worklist.push_back(std::make_tuple(lhs_st, rhs_st, cnt_state));
			++cnt_state;
		}
	}

	while (!worklist.empty())
	{
		State lhs_st, rhs_st, res_st;
		tie(lhs_st, rhs_st, res_st) = worklist.front();
		worklist.pop_front();

		if (haskey(lhs.finalstates, lhs_st) && haskey(rhs.finalstates, rhs_st))
		{
			result->finalstates.insert(res_st);
		}

		// TODO: a very inefficient implementation
		for (const auto& lhs_tr : lhs)
		{
			if (lhs_tr.src == lhs_st)
			{
				for (const auto& rhs_tr : rhs)
				{
					if (rhs_tr.src == rhs_st)
					{
						if (lhs_tr.symb == rhs_tr.symb)
						{
							// add a new transition
							State tgt_state;
							ProductMap::iterator it;
							bool ins;
							tie(it, ins) = prod_map->insert(
								{{lhs_tr.tgt, rhs_tr.tgt}, cnt_state});
							if (ins)
							{
								tgt_state = cnt_state;
								++cnt_state;

								worklist.push_back({lhs_tr.tgt, rhs_tr.tgt, tgt_state});
							}
							else
							{
								tgt_state = it->second;
							}

							result->add_trans(res_st, lhs_tr.symb, tgt_state);
						}
					}
				}
			}
		}
	}

	if (remove_prod_map)
	{
		delete prod_map;
	}
} // intersection }}}


bool Vata2::Nfa::is_lang_empty(const Nfa& aut, Path* cex)
{ // {{{
	std::list<State> worklist(
		aut.initialstates.begin(), aut.initialstates.end());
	std::unordered_set<State> processed(
		aut.initialstates.begin(), aut.initialstates.end());

	// 'paths[s] == t' denotes that state 's' was accessed from state 't',
	// 'paths[s] == s' means that 's' is an initial state
	std::map<State, State> paths;
	for (State s : worklist)
	{	// initialize
		paths[s] = s;
	}

	while (!worklist.empty())
	{
		State state = worklist.front();
		worklist.pop_front();

		if (haskey(aut.finalstates, state))
		{
			// TODO process the CEX
			if (nullptr != cex)
			{
				cex->clear();
				cex->push_back(state);
				while (paths[state] != state)
				{
					state = paths[state];
					cex->push_back(state);
				}

				std::reverse(cex->begin(), cex->end());
			}

			return false;
		}

		for (const auto& symb_stateset : aut[state])
		{
			const StateSet& stateset = symb_stateset.second;
			for (const auto& tgt_state : stateset)
			{
				bool inserted;
				tie(std::ignore, inserted) = processed.insert(tgt_state);
				if (inserted)
				{
					worklist.push_back(tgt_state);
					// also set that tgt_state was accessed from state
					paths[tgt_state] = state;
				}
				else
				{
					// the invariant
					assert(haskey(paths, tgt_state));
				}
			}
		}
	}

	return true;
} // is_lang_empty }}}


bool Vata2::Nfa::is_lang_empty_cex(const Nfa& aut, Word* cex)
{ // {{{
	assert(nullptr != cex);

	Path path = { };
	bool result = is_lang_empty(aut, &path);
	if (result) { return true; }
	bool consistent;
	tie(*cex, consistent) = get_word_for_path(aut, path);
	assert(consistent);

	return false;
} // is_lang_empty_cex }}}

std::unordered_set<State> Vata2::Nfa::get_fwd_reach_states(const Nfa& aut)
{
	std::list<State> worklist(
		aut.initialstates.begin(), aut.initialstates.end());
	std::unordered_set<State> processed(
		aut.initialstates.begin(), aut.initialstates.end());

	while (!worklist.empty())
	{
		State state = worklist.front();
		worklist.pop_front();

		for (const auto& symb_stateset : aut[state])
		{
			const StateSet& stateset = symb_stateset.second;
			for (const auto& tgt_state : stateset)
			{
				bool inserted;
				tie(std::ignore, inserted) = processed.insert(tgt_state);
				if (inserted)
				{
					worklist.push_back(tgt_state);
				}
			}
		}
	}

	return processed;
}


void Vata2::Nfa::determinize(
	Nfa*        result,
	const Nfa&  aut,
	SubsetMap*  subset_map,
	State*      last_state_num)
{ // {{{
	assert(nullptr != result);

	bool delete_map = false;
	if (nullptr == subset_map)
	{
		subset_map = new SubsetMap();
		delete_map = true;
	}

	State cnt_state = 0;
	std::list<std::pair<const StateSet*, State>> worklist;

	auto it_bool_pair = subset_map->insert({aut.initialstates, cnt_state});
	result->initialstates = {cnt_state};
	worklist.push_back({&it_bool_pair.first->first, cnt_state});
	++cnt_state;

	while (!worklist.empty())
	{
		const StateSet* state_set;
		State new_state;
		tie(state_set, new_state) = worklist.front();
		worklist.pop_front();
		assert(nullptr != state_set);

		// set the state final
		if (!are_disjoint(*state_set, aut.finalstates))
		{
			result->finalstates.insert(new_state);
		}

		// create the post of new_state
		PostSymb post_symb;
		for (State s : *state_set)
		{
			for (const auto& symb_post_pair : aut[s])
			{
				Symbol symb = symb_post_pair.first;
				const StateSet& post = symb_post_pair.second;
				post_symb[symb].insert(post.begin(), post.end());
				// TODO: consider using post() instead
			}
		}

		for (const auto& it : post_symb)
		{
			Symbol symb = it.first;
			const StateSet& post = it.second;

			// insert the new state in the map
			auto it_bool_pair = subset_map->insert({post, cnt_state});
			if (it_bool_pair.second)
			{ // if not processed yet, add to the queue
				worklist.push_back({&it_bool_pair.first->first, cnt_state});
				++cnt_state;
			}

			State post_state = it_bool_pair.first->second;
			result->add_trans(new_state, symb, post_state);
		}
	}

	if (delete_map)
	{
		delete subset_map;
	}

	if (nullptr != last_state_num)
	{
		*last_state_num = cnt_state - 1;
	}
} // determinize }}}


void Vata2::Nfa::make_complete(
	Nfa*             aut,
	const Alphabet&  alphabet,
	State            sink_state)
{ // {{{
	assert(nullptr != aut);

	std::list<State> worklist(aut->initialstates.begin(),
		aut->initialstates.end());
	std::unordered_set<State> processed(aut->initialstates.begin(),
		aut->initialstates.end());
	worklist.push_back(sink_state);
	processed.insert(sink_state);

	while (!worklist.empty())
	{
		State state = *worklist.begin();
		worklist.pop_front();

		std::set<Symbol> used_symbols;
		for (const auto& symb_stateset : (*aut)[state])
		{
			used_symbols.insert(symb_stateset.first);

			const StateSet& stateset = symb_stateset.second;
			for (const auto& tgt_state : stateset)
			{
				bool inserted;
				tie(std::ignore, inserted) = processed.insert(tgt_state);
				if (inserted) { worklist.push_back(tgt_state); }
			}
		}

		auto unused_symbols = alphabet.get_complement(used_symbols);
		for (Symbol symb : unused_symbols)
		{
			aut->add_trans(state, symb, sink_state);
		}
	}
} // make_complete }}}


Vata2::Parser::ParsedSection Vata2::Nfa::serialize(
	const Nfa&                aut,
	const SymbolToStringMap*  symbol_map,
	const StateToStringMap*   state_map)
{ // {{{
	Vata2::Parser::ParsedSection parsec;
	parsec.type = Vata2::Nfa::TYPE_NFA;

	using bool_str_pair = std::pair<bool, std::string>;

	std::function<bool_str_pair(State)> state_namer = nullptr;
	if (nullptr == state_map)
	{
		state_namer = [](State st) -> auto {
			return bool_str_pair(true, "q" + std::to_string(st));
		};
	}
	else
	{
		state_namer = [&state_map](State st) -> auto {
			auto it = state_map->find(st);
			if (it != state_map->end()) { return bool_str_pair(true, it->second); }
			else { return bool_str_pair(false, ""); }
		};
	}

	std::function<bool_str_pair(Symbol)> symbol_namer = nullptr;
	if (nullptr == symbol_map)
	{
		symbol_namer = [](Symbol sym) -> auto {
			return bool_str_pair(true, "a" + std::to_string(sym));
		};
	}
	else
	{
		symbol_namer = [&symbol_map](Symbol sym) -> auto {
			auto it = symbol_map->find(sym);
			if (it != symbol_map->end()) { return bool_str_pair(true, it->second); }
			else { return bool_str_pair(false, ""); }
		};
	}

	// construct initial states
	std::vector<std::string> init_states;
	for (State s : aut.initialstates)
	{
		bool_str_pair bsp = state_namer(s);
		if (!bsp.first) { throw std::runtime_error("cannot translate state " + std::to_string(s)); }
		init_states.push_back(bsp.second);
	}
	parsec.dict["Initial"] = init_states;

	// construct final states
	std::vector<std::string> fin_states;
	for (State s : aut.finalstates)
	{
		bool_str_pair bsp = state_namer(s);
		if (!bsp.first) { throw std::runtime_error("cannot translate state " + std::to_string(s)); }
		fin_states.push_back(bsp.second);
	}
	parsec.dict["Final"] = fin_states;

	for (const auto& trans : aut)
	{
		bool_str_pair src_bsp = state_namer(trans.src);
		if (!src_bsp.first) { throw std::runtime_error("cannot translate state " + std::to_string(trans.src)); }
		bool_str_pair tgt_bsp = state_namer(trans.tgt);
		if (!tgt_bsp.first) { throw std::runtime_error("cannot translate state " + std::to_string(trans.tgt)); }
		bool_str_pair sym_bsp = symbol_namer(trans.symb);
		if (!sym_bsp.first) { throw std::runtime_error("cannot translate symbol " + std::to_string(trans.symb)); }

		parsec.body.push_back({ src_bsp.second, sym_bsp.second, tgt_bsp.second });
	}

	return parsec;
} // serialize }}}


std::pair<Word, bool> Vata2::Nfa::get_word_for_path(
	const Nfa&   aut,
	const Path&  path)
{ // {{{
	if (path.empty())
	{
		return {{}, true};
	}

	Word word;
	State cur = path[0];
	for (size_t i = 1; i < path.size(); ++i)
	{
		State newSt = path[i];
		bool found = false;

		const auto& postCur = aut.post(cur);
		for (const auto& symbolMap : *postCur)
		{
			for (State st : symbolMap.second)
			{
				if (st == newSt)
				{
					word.push_back(symbolMap.first);
					found = true;
					break;
				}
			}

			if (found) { break; }
		}

		if (!found)
		{
			return {{}, false};
		}

		cur = newSt;    // update current state
	}

	return {word, true};
} // get_word_for_path }}}


void Vata2::Nfa::revert(Nfa* result, const Nfa& aut)
{ // {{{
	assert(nullptr != result);

	result->initialstates = aut.finalstates;
	result->finalstates = aut.initialstates;

	for (auto trans : aut)
	{
		result->add_trans(trans.tgt, trans.symb, trans.src);
	}
} // revert }}}


void Vata2::Nfa::remove_epsilon(Nfa* result, const Nfa& aut, Symbol epsilon)
{ // {{{
	assert(nullptr != result);

	// cannot use multimap, because it can contain multiple occurrences of (a -> a), (a -> a)
	std::unordered_map<State, StateSet> eps_closure;

	// TODO: grossly inefficient
	// first we compute the epsilon closure
	for (auto trans : aut) { // initialize
		auto it_ins_pair = eps_closure.insert({ trans.src, {trans.src} });
		if (trans.symb == epsilon) {
			StateSet& closure = it_ins_pair.first->second;
			closure.insert(trans.tgt);
		}
	}

	bool changed = true;
	while (changed) { // compute the fixpoint
		changed = false;
		for (auto trans : aut) {
			if (trans.symb == epsilon) {
				StateSet& src_eps_cl = eps_closure[trans.src];
				const StateSet& tgt_eps_cl = eps_closure[trans.tgt];

				for (State st : tgt_eps_cl) {
					auto it_ins_pair = src_eps_cl.insert(st);
					if (it_ins_pair.second) changed = true;
				}
			}
		}
	}

	// now we construct the automaton without epsilon transitions
	result->initialstates.insert(aut.initialstates.begin(), aut.initialstates.end());
	result->finalstates.insert(aut.finalstates.begin(), aut.finalstates.end());
	for (auto state_closure_pair : eps_closure) { // for every state
		State src_state = state_closure_pair.first;
		for (State eps_cl_state : state_closure_pair.second) { // for every state in its eps cl
			if (aut.has_final(eps_cl_state)) result->add_final(src_state);
			for (auto symb_set : aut[eps_cl_state]) {
				if (symb_set.first == epsilon) continue;

				// TODO: this could be done more efficiently if we had a better add_trans method
				for (State tgt_state : symb_set.second) {
					result->add_trans(src_state, symb_set.first, tgt_state);
				}
			}
		}
	}
} // remove_epsilon }}}


void Vata2::Nfa::minimize(
	Nfa*               result,
	const Nfa&         aut,
	const StringDict&  params)
{ // {{{
	assert(nullptr != result);

	DEBUG_PRINT("ignoring parameters of minimization and using default");
	assert(&params);

	// TODO: remove useless states before we start doing anything

	// TODO: make controllable using 'params', so far using Brzozowski's
	// minimization (revert, determinize, revert, determinize)
	Nfa tmp = revert(aut);
	tmp = determinize(tmp);
	tmp = revert(tmp);
	*result = determinize(tmp);
} // minimize }}}


void Vata2::Nfa::construct(
	Nfa*                                 aut,
	const Vata2::Parser::ParsedSection&  parsec,
	Alphabet*                            alphabet,
	StringToStateMap*                    state_map)
{ // {{{
	assert(nullptr != aut);
	assert(nullptr != alphabet);

	if (parsec.type != Vata2::Nfa::TYPE_NFA) {
		throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"" +
			Vata2::Nfa::TYPE_NFA + "\"");
	}

	bool remove_state_map = false;
	if (nullptr == state_map) {
		state_map = new StringToStateMap();
		remove_state_map = true;
	}

	State cnt_state = 0;

	// a lambda for translating state names to identifiers
	auto get_state_name = [state_map, &cnt_state](const std::string& str) {
		auto it_insert_pair = state_map->insert({str, cnt_state});
		if (it_insert_pair.second) { return cnt_state++; }
		else { return it_insert_pair.first->second; }
	};

	// a lambda for cleanup
	auto clean_up = [&]() {
		if (remove_state_map) { delete state_map; }
	};


	auto it = parsec.dict.find("Initial");
	if (parsec.dict.end() != it)
	{
		for (const auto& str : it->second)
		{
			State state = get_state_name(str);
			aut->initialstates.insert(state);
		}
	}


	it = parsec.dict.find("Final");
	if (parsec.dict.end() != it)
	{
		for (const auto& str : it->second)
		{
			State state = get_state_name(str);
			aut->finalstates.insert(state);
		}
	}

	for (const auto& body_line : parsec.body)
	{
		if (body_line.size() != 3)
		{
			// clean up
			clean_up();

			if (body_line.size() == 2)
			{
				throw std::runtime_error("Epsilon transitions not supported: " +
					std::to_string(body_line));
			}
			else
			{
				throw std::runtime_error("Invalid transition: " +
					std::to_string(body_line));
			}
		}

		State src_state = get_state_name(body_line[0]);
		Symbol symbol = alphabet->translate_symb(body_line[1]);
		State tgt_state = get_state_name(body_line[2]);

		aut->add_trans(src_state, symbol, tgt_state);
	}

	// do the dishes and take out garbage
	clean_up();
} // construct }}}


void Vata2::Nfa::construct(
	Nfa*                                 aut,
	const Vata2::Parser::ParsedSection&  parsec,
	StringToSymbolMap*                   symbol_map,
	StringToStateMap*                    state_map)
{ // {{{
	assert(nullptr != aut);

	bool remove_symbol_map = false;
	if (nullptr == symbol_map)
	{
		symbol_map = new StringToSymbolMap();
		remove_symbol_map = true;
	}

	auto release_res = [&](){ if (remove_symbol_map) delete symbol_map; };

	OnTheFlyAlphabet alphabet(symbol_map);

	try
	{
		construct(aut, parsec, &alphabet, state_map);
	}
	catch (std::exception&)
	{
		release_res();
		throw;
	}

	release_res();
} // construct(StringToSymbolMap) }}}


bool Vata2::Nfa::is_in_lang(const Nfa& aut, const Word& word)
{ // {{{
	StateSet cur = aut.initialstates;

	for (Symbol sym : word)
	{
		cur = aut.post(cur, sym);
		if (cur.empty()) { return false; }
	}

	return !are_disjoint(cur, aut.finalstates);
} // is_in_lang }}}


bool Vata2::Nfa::is_prfx_in_lang(const Nfa& aut, const Word& word)
{ // {{{
	StateSet cur = aut.initialstates;

	for (Symbol sym : word)
	{
	  if (!are_disjoint(cur, aut.finalstates)) { return true; }
		cur = aut.post(cur, sym);
		if (cur.empty()) { return false; }
	}

	return !are_disjoint(cur, aut.finalstates);
} // is_prfx_in_lang }}}


bool Vata2::Nfa::is_deterministic(const Nfa& aut)
{ // {{{
	if (aut.initialstates.size() != 1) { return false; }

	for (const auto& trans : aut)
	{
		const PostSymb& post = aut[trans.src];
		if (post.at(trans.symb).size() != 1) { return false; }
	}

	return true;
} // is_deterministic }}}


bool Vata2::Nfa::is_complete(const Nfa& aut, const Alphabet& alphabet)
{ // {{{
	std::list<Symbol> symbs_ls = alphabet.get_symbols();
	std::unordered_set<Symbol> symbs(symbs_ls.cbegin(), symbs_ls.cend());

	// TODO: make a general function for traversal over reachable states that can
	// be shared by other functions?
	std::list<State> worklist(aut.initialstates.begin(),
		aut.initialstates.end());
	std::unordered_set<State> processed(aut.initialstates.begin(),
		aut.initialstates.end());

	while (!worklist.empty())
	{
		State state = *worklist.begin();
		worklist.pop_front();

		size_t n = 0;      // counter of symbols
		for (const auto& symb_stateset : aut[state])
		{
			++n;
			if (!haskey(symbs, symb_stateset.first))
			{
				throw std::runtime_error(std::to_string(__func__) +
					": encountered a symbol that is not in the provided alphabet");
			}

			const StateSet& stateset = symb_stateset.second;
			for (const auto& tgt_state : stateset)
			{
				bool inserted;
				tie(std::ignore, inserted) = processed.insert(tgt_state);
				if (inserted) { worklist.push_back(tgt_state); }
			}
		}

		if (symbs.size() != n) { return false; }
	}

	return true;
} // is_complete }}}

bool Vata2::Nfa::accepts_epsilon(const Nfa& aut)
{ // {{{
	for (State st : aut.initialstates) {
		if (haskey(aut.finalstates, st)) return true;
	}

	return false;
} // accepts_epsilon }}}

std::ostream& std::operator<<(std::ostream& os, const Vata2::Nfa::NfaWrapper& nfa_wrap)
{ // {{{
	os << "{NFA wrapper|NFA: " << nfa_wrap.nfa << "|alphabet: " << nfa_wrap.alphabet <<
		"|state_dict: " << std::to_string(nfa_wrap.state_dict) << "}";
	return os;
} // operator<<(NfaWrapper) }}}
