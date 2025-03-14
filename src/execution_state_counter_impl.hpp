/*
	Never include directly!
	This is included by execution_state_counter.hpp and only exists to split
	interface and implementation despite the template.
*/

#include <cassert>

namespace suse {

template <typename underlying>
execution_state_counter<underlying> &execution_state_counter<underlying>::operator+=(const execution_state_counter<underlying> &other) {
    assert(size() == other.size());

    for (std::size_t i = 0; i < counters_.size(); ++i)
        counters_[i] += other.counters_[i];

    return *this;
}

template <typename underlying>
execution_state_counter<underlying> &execution_state_counter<underlying>::operator-=(const execution_state_counter<underlying> &other) {
    assert(size() == other.size());

    for (std::size_t i = 0; i < counters_.size(); ++i)
        counters_[i] -= other.counters_[i];

    return *this;
}

template <typename underlying>
execution_state_counter<underlying> &execution_state_counter<underlying>::operator*=(const execution_state_counter<underlying> &other) {
    for (std::size_t i = 0; i < counters_.size(); ++i)
        counters_[i] *= other.counters_[i];

    return *this;
}

template <typename underlying>
execution_state_counter<underlying> &execution_state_counter<underlying>::operator*=(const underlying &other) {
    for (std::size_t i = 0; i < counters_.size(); ++i)
        counters_[i] *= other;

    return *this;
}

template <typename underlying>
execution_state_counter<underlying> advance(const execution_state_counter<underlying> &counter, const nfa &automaton, char symbol) {
    assert(counter.size() == automaton.number_of_states());

    auto followup = execution_state_counter<underlying>{counter.size()};

    for (std::size_t source_id = 0; source_id < automaton.number_of_states(); ++source_id) {
        const auto add_for = [&](auto s) {
            const auto &state = automaton.states()[source_id];
            if (auto it = state.transitions.find(s); it != state.transitions.end()) {
                auto &destination_state_ids = it->second;
                for (auto &destination_id : destination_state_ids)
                    followup[destination_id] += counter[source_id];
            }
        };

        add_for(symbol);
        add_for(nfa::wildcard_symbol);
    }

    return followup;
}

template <typename underlying>
execution_state_counter<underlying> advance(const execution_state_counter<underlying> &counter, const edgelist &per_character_edges, char symbol) {
    auto followup = execution_state_counter<underlying>{counter.size()};

    const auto add_for = [&](auto s) {
        for (const auto &e : per_character_edges.edges_for(s))
            followup[e.to] += counter[e.from];
    };

    add_for(symbol);
    add_for(nfa::wildcard_symbol);

    return followup;
}

template <typename underlying>
execution_state_counter<underlying> advance_sum(
    const execution_state_counter<underlying> &count_counter, 
    const execution_state_counter<underlying> &sum_counter,
    const edgelist &per_character_edges, 
    const event &event) {

    auto followup = execution_state_counter<underlying>{count_counter.size()};
    const auto sum_for = [&](auto s) {
        for (const auto &e : per_character_edges.edges_for(s)) 
            followup[e.to] += sum_counter[e.from] + count_counter[e.from] * event.value;
    };

    sum_for(event.type);
    sum_for(nfa::wildcard_symbol);

    return followup;
}

template <typename underlying>
execution_state_counter<underlying> advance_prod(
    const execution_state_counter<underlying> &count_counter,
    const execution_state_counter<underlying> &mult_counter,
    const edgelist &per_character_edges,
    const event &event) {

    auto followup = execution_state_counter<underlying>{count_counter.size()};
    std::fill(followup.begin(), followup.end(), 1);
    const auto mult_for = [&](auto s) {
        for (const auto &e : per_character_edges.edges_for(s)) {
            followup[e.to] *= mult_counter[e.from] * pow(event.value, count_counter[e.from]);
        }

    };

    mult_for(event.type);
    mult_for(nfa::wildcard_symbol);

    return followup;
}
} // namespace suse
