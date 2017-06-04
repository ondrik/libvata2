// TODO: add header

#include <vata2/nfa.hh>
#include <vata2/util.hh>

#include <algorithm>
#include <list>
#include <unordered_set>

using std::tie;
using namespace Vata2::util;

void Vata2::Nfa::Nfa::add_trans(const Trans* trans)
{ // {{{
	assert(nullptr != trans);

	auto it = this->transitions.find(trans->src);
	if (it != this->transitions.end())
	{
		PostSymb& post = it->second;
		auto jt = post.find(trans->symb);
		if (jt != post.end())
		{
			jt->second.insert(trans->tgt);
		}
		else
		{
			post.insert({trans->symb, StateSet({trans->tgt})});
		}
	}
	else
	{
		this->transitions.insert(
			{trans->src, PostSymb({{trans->symb, StateSet({trans->tgt})}})});
	}
} // add_trans }}}

bool Vata2::Nfa::Nfa::has_trans(const Trans* trans) const
{ // {{{
	assert(nullptr != trans);

	auto it = this->transitions.find(trans->src);
	if (it == this->transitions.end())
	{
		return false;
	}

	const PostSymb& post = it->second;
	auto jt = post.find(trans->symb);
	if (jt == post.end())
	{
		return false;
	}

	return jt->second.find(trans->tgt) != jt->second.end();
} // has_trans }}}

Vata2::Nfa::Nfa::const_iterator Vata2::Nfa::Nfa::const_iterator::for_begin(const Nfa* nfa)
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

Vata2::Nfa::Nfa::const_iterator& Vata2::Nfa::Nfa::const_iterator::operator++()
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

bool Vata2::Nfa::are_state_disjoint(const Nfa* lhs, const Nfa* rhs)
{ // {{{
	assert(nullptr != lhs);
	assert(nullptr != rhs);

	// fill lhs_states with all states of lhs
	std::unordered_set<State> lhs_states;
	lhs_states.insert(lhs->initialstates.begin(), lhs->initialstates.end());
	lhs_states.insert(lhs->finalstates.begin(), lhs->finalstates.end());

	for (auto trans : *lhs)
	{
		lhs_states.insert({trans.src, trans.tgt});
	}

	// for every state found in rhs, check its presence in lhs_states
	for (auto rhs_st : rhs->initialstates)
	{
		if (lhs_states.find(rhs_st) != lhs_states.end())
		{
			return false;
		}
	}

	for (auto rhs_st : rhs->finalstates)
	{
		if (lhs_states.find(rhs_st) != lhs_states.end())
		{
			return false;
		}
	}

	for (auto trans : *rhs)
	{
		if (lhs_states.find(trans.src) != lhs_states.end()
				|| lhs_states.find(trans.tgt) != lhs_states.end())
		{
			return false;
		}
	}

	// no common state found
	return true;
} // are_disjoint }}}

void Vata2::Nfa::intersection(
	Nfa* result,
	const Nfa* lhs,
	const Nfa* rhs,
	ProductMap* prod_map)
{ // {{{
	assert(nullptr != result);
	assert(nullptr != lhs);
	assert(nullptr != rhs);

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
	for (const auto lhs_st : lhs->initialstates)
	{
		for (const auto rhs_st : rhs->initialstates)
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

		if (lhs->finalstates.find(lhs_st) != lhs->finalstates.end() &&
			rhs->finalstates.find(rhs_st) != rhs->finalstates.end())
		{
			result->finalstates.insert(res_st);
		}

		// TODO: a very inefficient implementation
		for (const auto lhs_tr : *lhs)
		{
			if (lhs_tr.src == lhs_st)
			{
				for (const auto rhs_tr : *rhs)
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

bool Vata2::Nfa::is_lang_empty(const Nfa* aut, Path* cex)
{ // {{{
	assert(nullptr != aut);

	std::list<State> worklist(
		aut->initialstates.begin(), aut->initialstates.end());
	std::unordered_set<State> processed(
		aut->initialstates.begin(), aut->initialstates.end());

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

		if (aut->finalstates.find(state) != aut->finalstates.end())
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

		const PostSymb* post_map = aut->get_post(state);
		if (nullptr == post_map) { continue; }
		for (const auto& symb_stateset : *post_map)
		{
			const StateSet& stateset = symb_stateset.second;
			for (auto tgt_state : stateset)
			{
				bool inserted;
				std::tie(std::ignore, inserted) = processed.insert(tgt_state);
				if (inserted)
				{
					worklist.push_back(tgt_state);
					// also set that tgt_state was accessed from state
					paths[tgt_state] = state;
				}
				else
				{
					// the invariant
					assert(paths.end() != paths.find(tgt_state));
				}
			}
		}
	}

	return true;
} // is_lang_empty }}}

bool Vata2::Nfa::is_lang_universal(const Nfa* aut, Path* cex)
{ // {{{
	assert(nullptr != aut);

	assert(false);

	*cex = {1,2};

	return true;

} // is_lang_universal }}}

void Vata2::Nfa::determinize(
	Nfa* result,
	const Nfa* aut,
	SubsetMap* subset_map)
{ // {{{
	assert(nullptr != result);
	assert(nullptr != aut);

	bool delete_map = false;
	if (nullptr == subset_map)
	{
		subset_map = new SubsetMap();
		delete_map = true;
	}

	State cnt_state = 0;
	std::list<std::pair<const StateSet*, State>> worklist;

	auto it_bool_pair = subset_map->insert({aut->initialstates, cnt_state});
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
		if (!are_disjoint(state_set, &aut->finalstates))
		{
			result->finalstates.insert(new_state);
		}

		// create the post of new_state
		PostSymb post_symb;
		for (State s : *state_set)
		{
			for (auto symb_post_pair : (*aut)[s])
			{
				Symbol symb = symb_post_pair.first;
				const StateSet& post = symb_post_pair.second;
				post_symb[symb].insert(post.begin(), post.end());
			}
		}

		for (auto it : post_symb)
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
} // determinize }}}

std::string Vata2::Nfa::serialize_vtf(const Nfa* aut)
{ // {{{
	assert(nullptr != aut);

	std::string result;
	result += "@NFA\n";
	result += "%Initial";
	for (State s : aut->initialstates)
	{
		result += " q" + std::to_string(s);
	}

	result += "\n";
	result += "%Final";
	for (State s : aut->finalstates)
	{
		result += " q" + std::to_string(s);
	}

	result += "\n";
	result += "%Transitions   # the format is <src> <symbol> <tgt>\n";
	for (auto trans : *aut)
	{
		result +=
			"q" + std::to_string(trans.src) +
			" a" + std::to_string(trans.symb) +
			" q" + std::to_string(trans.tgt) +
			"\n";
	}

	return result;
} // serialize_vtf }}}

std::pair<Vata2::Nfa::Word, bool> Vata2::Nfa::get_word_for_path(
	const Nfa& aut,
	const Path& path)
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

		auto postCur = aut.get_post(cur);
		for (auto symbolMap : *postCur)
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

void Vata2::Nfa::construct(
	Nfa* aut,
	const Vata2::Parser::Parsed* parsed,
	StringToSymbolMap* symbol_map,
	StringToStateMap* state_map)
{ // {{{
	assert(nullptr != aut);
	assert(nullptr != parsed);

	if (parsed->type != "NFA")
	{
		throw std::runtime_error(std::string(__FUNCTION__) + ": expecting type \"NFA\"");
	}

	bool remove_state_map = false;
	if (nullptr == state_map)
	{
		state_map = new StringToStateMap();
		remove_state_map = true;
	}

	bool remove_symbol_map = false;
	if (nullptr == symbol_map)
	{
		symbol_map = new StringToSymbolMap();
		remove_symbol_map = true;
	}

	State cnt_state = 0;
	State cnt_symbol = 0;

	// a lambda for translating state names to identifiers
	auto get_state_name = [state_map, &cnt_state](const std::string& str) {
		auto it_insert_pair = state_map->insert({str, cnt_state});
		if (it_insert_pair.second) { return cnt_state++; }
		else { return it_insert_pair.first->second; }
	};

	// a lambda for translating symbol names to identifiers
	auto get_symbol_name = [symbol_map, &cnt_symbol](const std::string& str) {
		auto it_insert_pair = symbol_map->insert({str, cnt_symbol});
		if (it_insert_pair.second) { return cnt_symbol++; }
		else { return it_insert_pair.first->second; }
	};

	// a lambda for cleanup
	auto clean_up = [&]() {
		if (remove_state_map) { delete state_map; }
		if (remove_symbol_map) { delete symbol_map; }
	};


	auto first_last_it = parsed->dict.equal_range("Initial");
	auto it = first_last_it.first;
	while (it != first_last_it.second)
	{
		State state = get_state_name(it->second);
		aut->initialstates.insert(state);
		++it;
	}

	first_last_it = parsed->dict.equal_range("Final");
	it = first_last_it.first;
	while (it != first_last_it.second)
	{
		State state = get_state_name(it->second);
		aut->finalstates.insert(state);
		++it;
	}

	for (auto parsed_trans : parsed->trans_list)
	{
		if (parsed_trans.size() != 3)
		{
			// clean up
			clean_up();

			if (parsed_trans.size() == 2)
			{
				throw std::runtime_error("Epsilon transitions not supported: " +
					std::to_string(parsed_trans));
			}
			else
			{
				throw std::runtime_error("Invalid transition: " +
					std::to_string(parsed_trans));
			}
		}

		State src_state = get_state_name(parsed_trans[0]);
		Symbol symbol = get_symbol_name(parsed_trans[1]);
		State tgt_state = get_state_name(parsed_trans[2]);

		aut->add_trans(src_state, symbol, tgt_state);
	}

	// do the dishes and take out garbage
	clean_up();
} // construct }}}