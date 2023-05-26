#include <map>
#include <array>
#include <vector>
#include <ranges>
#include <random>
#include <iostream>
#include <algorithm>

namespace utility
{
    constexpr int NUM_PLAYERS = 2;
    constexpr int NUM_SIDES = 6;
    constexpr int NUM_ACTIONS = (2 * NUM_SIDES) + 1;
    constexpr int DUDO = NUM_ACTIONS - 1;
    constexpr std::array<int, NUM_ACTIONS - 1> CLAIM_NUM =  { 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2 };
    constexpr std::array<int, NUM_ACTIONS - 1> CLAIM_RANK = { 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6, 1 };

    int roll() 
    {
        static std::uniform_int_distribution<int> dist{ 1, 6 };
        static std::random_device seed;
        static std::mt19937_64 engine{seed()};
        return dist(engine);
    }

    std::vector<int> get_3history(std::array<bool, NUM_ACTIONS> const& is_claimed)
    {
        std::vector<int> history;
        for (int i = NUM_ACTIONS - 1, s = 0; i >= 0 && s < 3; i--)
            if (is_claimed[i])
            {
            history.insert(history.begin(), i--);
            s++;
            }
        return history;
    }

    unsigned int generate_key(std::vector<int> const& history,
                            int const& player, int const& player_roll)
    {
        unsigned int node_key = (player << 3 | player_roll);

        for (int i = history.size() - 1, s = 1; i >= 0; i--)
            node_key |= (history[i] + 1) << (4 * s++);

        return node_key;        
    }
};

struct node
{
    public:
        std::array<double, utility::NUM_PLAYERS> reach_probability_sum;

        node() {};
        node(int const first_available_a, int const roll, int const player_number, std::vector<int> const& history)
        {
            player = player_number;
            opponent = (player + 1) % 2;

            player_roll = roll;
            
            int num_available_actions;
            first_available_action = first_available_a;
            is_starting_node = (first_available_action == 0);
            if (is_starting_node) 
            {
                num_available_actions = utility::NUM_ACTIONS - first_available_action - 1;
                reach_probability_sum = { 1.0, 1.0 };
            }
            else 
            {
                num_available_actions = utility::NUM_ACTIONS - first_available_action;
                reach_probability_sum = { 0.0, 0.0 };
            }

            regret_sum.resize(num_available_actions, 0.0);
            strategy.resize(num_available_actions, 0.0);
            strategy_sum.resize(num_available_actions, 0.0);
            utility.resize(num_available_actions, 0.0);

            node_history = history;
        }
        std::vector<double>& get_strategy() 
        {   
            double normalizing_sum = 0.0;
            for (int a = 0; a < regret_sum.size(); a++) 
            {
                strategy[a] = std::max(regret_sum[a], 0.0);
                normalizing_sum += strategy[a];
            }
            for (int a = 0; a < regret_sum.size(); a++)
            {
                if (normalizing_sum > 0)
                    strategy[a] /= normalizing_sum;
                else
                    strategy[a] = 1.0 / strategy.size();
                strategy_sum[a] += strategy[a] * reach_probability_sum[player];
            }
            return strategy;
        }
        void update_reach_probability(
            std::array<double, utility::NUM_PLAYERS> const& predecessor_reach_probability_sum,
            std::vector<double> const& predecessor_strategy, int const predecessor_first_available_action)
        {
            int const action_index = first_available_action - predecessor_first_available_action -1;

            reach_probability_sum[opponent] += predecessor_reach_probability_sum[opponent] * predecessor_strategy[action_index];
            reach_probability_sum[player] += predecessor_reach_probability_sum[player];
        }
        double get_utility(std::array<int, utility::NUM_PLAYERS> const& dice)
        {
            // compute the payoff for playing dudo, if it's a valid move
            if (!is_starting_node) 
            {
                const int previous_a = first_available_action - 1;
                int actual_rank_count = std::count_if(dice.begin(), dice.end(), 
                                    [previous_a](int die){return die == utility::CLAIM_RANK[previous_a] || die == 1;});
                
                bool player_wins = utility::CLAIM_NUM[previous_a] > actual_rank_count;
                utility.back() = player_wins ? 1.0 : -1.0;
            }

            // accumulate regret
            double total_utility = std::inner_product(utility.begin(), utility.end(), strategy.begin(), 0.0);
            for (int a = 0; a < utility.size(); a++) 
            {
                double regret_a = strategy[a] * utility[a] - total_utility;
                regret_sum[a] += reach_probability_sum[opponent] * regret_a;
            }
            if (is_starting_node)
                reach_probability_sum = { 1.0, 1.0 };
            else
                reach_probability_sum = { 0.0, 0.0 };

            return total_utility;
        }  
        void update_utility(double const& successor_total_utility, int const successor_first_available_action)
        {
            int const action_index = successor_first_available_action - first_available_action - 1;

            utility[action_index] = -successor_total_utility;
        }

        void reset_strategy_sum() {std::fill(strategy_sum.begin(), strategy_sum.end(), 0.0);}
        void print_optimal_strategy()
        {
            std::cout << "-------------------------------------------------" << std::endl;
            std::cout << "player: " << player << std::endl;
            std::cout << "roll: " << player_roll << std::endl;
            
            std::cout << "history: ";
            for (auto const& h : node_history)
                std::cout << "(" << utility::CLAIM_NUM[h] << "*" << utility::CLAIM_RANK[h] << ") ";
            std::cout << std::endl;
            std::cout << std::endl;

            // normalize strategy sum
            double const normalizing_sum = std::accumulate(strategy_sum.begin(), strategy_sum.end(), 0.0);
            if (normalizing_sum != 0)
                std::for_each(strategy_sum.begin(), strategy_sum.end(), [normalizing_sum](double& x){x = x / normalizing_sum;});
            else
                std::fill(strategy_sum.begin(), strategy_sum.end(), 1.0 / strategy_sum.size());

            for (int a = 0; a < strategy_sum.size(); a++)
            {
                int action_index = first_available_action + a;
                if (action_index != utility::DUDO)
                    std::cout << "(" << utility::CLAIM_NUM[action_index] << "*" << utility::CLAIM_RANK[action_index] << ")" 
                            << "    " << strategy_sum[a] << std::endl;
                else
                    std::cout << "DUDO" << "    " << strategy_sum[a] << std::endl;
            }
        }

    private:
        int player, opponent;
        int player_roll;
        int first_available_action;
        bool is_starting_node;

        std::vector<double> regret_sum;
        std::vector<double> strategy;
        std::vector<double> strategy_sum;
        std::vector<double> utility;

        std::vector<int> node_history;
};

class dudo3_trainer
{
    public:
        dudo3_trainer()
        {
            std::array<bool, utility::NUM_ACTIONS> claim_history = { false };
            std::array<int, utility::NUM_PLAYERS> dice;
            // construct the node map
            for (int d0 = 1; d0 <= 6; d0++)
                for (int d1 = 1; d1 <= 6; d1++)
                {
                    dice = { d0, d1 };
                    compute_tree(claim_history, dice, 0, nullptr);
                }
        }

        void train(int const num_iterations)
        {
            std::array<int,utility::NUM_PLAYERS> dice;

            for (int i = 1; i <= num_iterations; i++)
            {
                std::generate(dice.begin(), dice.end(), utility::roll);  

                unsigned int const player_0_node_identifier = 0 << 3 | dice[0];
                unsigned int const player_1_node_identifier = 1 << 3 | dice[1];

                for (auto& [node_key, node_holder] : node_map)
                {
                    unsigned int const node_key_identifier = node_key&0xf;
                    
                    if (node_key_identifier == player_0_node_identifier
                    || node_key_identifier == player_1_node_identifier)
                    {
                        node_holder.propagate_reach_probability();
                    }
                }
                for (auto& [node_key, node_holder] : std::ranges::reverse_view(node_map))
                {
                    unsigned int const node_key_identifier = node_key&0xf;
                    
                    if (node_key_identifier == player_0_node_identifier
                    || node_key_identifier == player_1_node_identifier)
                    {
                        node_holder.backpropagate_utility(dice);
                    }
                }
                
                // reset strategy sum half-way through 
                if (i == num_iterations / 2)
                {
                    auto reset_strategy_sum = [](auto& it){it.second.held_node.reset_strategy_sum();};
                    std::for_each(node_map.begin(), node_map.end(), reset_strategy_sum);
                }
                    
            }
        }
    
        void print_results()
        {
            for (auto& [node_key, node_holder] : node_map)
            {
                node_holder.held_node.print_optimal_strategy();
            }
        }
    
    private:
        struct node_holder
        {
            int first_available_action;
            bool is_terminal;

            node held_node;
            std::vector<node*> predecessors;
            std::vector<node*> successors;

            node_holder() {};
            node_holder(std::array<bool, utility::NUM_ACTIONS> const& is_claimed, 
                        int const& player_number, int const& roll, std::vector<int> const& history)         
                        : 
                        first_available_action (std::distance(std::find(is_claimed.rbegin(), is_claimed.rend(), 1), is_claimed.rend())),
                        held_node(first_available_action, roll, player_number, history),
                        is_terminal (first_available_action == utility::DUDO) 
                        {}
            
            void propagate_reach_probability()
            {
                std::vector<double> const strategy = held_node.get_strategy(); // node 'learns' from the computed strategy //

                for (node* successor_ptr : successors) 
                    successor_ptr -> update_reach_probability(held_node.reach_probability_sum, strategy, first_available_action);
            }

            void backpropagate_utility(std::array<int, utility::NUM_PLAYERS> const& dice)
            {
                double total_utility = held_node.get_utility(dice); // node 'learns' from the computed utility //

                for (node* const predecessor_ptr : predecessors) 
                    predecessor_ptr -> update_utility(total_utility, first_available_action);
            }            
        };

        std::map<unsigned int, node_holder> node_map;

        node* compute_tree(std::array<bool, utility::NUM_ACTIONS>& is_claimed,
                    std::array<int, utility::NUM_PLAYERS> const& dice, 
                    int const player, node* previous_node_ptr)
        {
            std::vector<int> history = utility::get_3history(is_claimed);
            unsigned int node_key = utility::generate_key(history, player, dice[player]);
            node_map.insert({node_key, node_holder(is_claimed, player, dice[player], history)});
            node_holder* current_node_ptr = &node_map[node_key];

            if (previous_node_ptr)
                current_node_ptr -> predecessors.push_back(previous_node_ptr);

            if (current_node_ptr -> is_terminal)
                return &current_node_ptr -> held_node;

            // recursively visit the current nodes' successors
            for (int a = current_node_ptr -> first_available_action; a < utility::DUDO; a++) 
            {
                is_claimed[a] = true;
                current_node_ptr -> successors.push_back(
                                compute_tree(is_claimed, dice, (player + 1) % 2, &current_node_ptr -> held_node));
                is_claimed[a] = false;
            }
            return &current_node_ptr -> held_node;
        }
};

int main()
{
    dudo3_trainer trainer;
    trainer.train(20000);
    trainer.print_results();
    
    return 0;
}
