#include "../headers/tokenizer.hpp"
#include <fstream>
#include <string>
#include "./set_constraint_util.cpp"
#include <map>
#include <queue>
#include<variant>


std::map<std::string, Node*> set_var_map;

std::deque<Node*> worklist;

/*
* AddEdge adds an edge between two nodes in the graph
* The rules for determining whether it should be stored as a successor or predecessor edge are:
* 1. Any edge where the lhs is a constructor call is a predecessor edge
    1.a. If the rhs is a constructor call, add an edge between each corresponding argument
* 2. Any edge where the rhs is a projection is a predecessor edge
* 3. Any other edge is a successor edge
*/
void AddEdge(Node* lhs, Node* rhs, bool is_init = false) {
    if (((lhs->IsConstructor() && rhs->IsConstructor()) || (lhs->IsLam() && rhs->IsLam())) 
        && lhs->Name() == rhs->Name())
    {
        if (lhs->Name() == "ref")
        {
            // Add edge between second argument which is a set variable. This argument is covariant so the edge will be from lhs -> rhs
            if (std::holds_alternative<Node*>(lhs->GetArgAt(1)) && std::holds_alternative<Node*>(rhs->GetArgAt(1)))
            {
                AddEdge(std::get<Node*>(lhs->CallArgs().at(1)), std::get<Node*>(rhs->CallArgs().at(1)));
            }
        }
        else if (lhs->Name() == "lam_")
        {
            int start_idx = 2;
            // If ret value is present, all arguments from index 2 are contravariant, else all arguments from index 1 are contravariant
            if (!lhs->HasRetVal())
                start_idx = 1;
            else
            {
                if (std::holds_alternative<Node*>(lhs->CallArgs().at(1)) && std::holds_alternative<Node*>(rhs->CallArgs().at(1)))
                {
                    Node* lhs_retval = std::get<Node*>(lhs->CallArgs().at(1));
                    Node* rhs_retval = std::get<Node*>(rhs->CallArgs().at(1));
                    AddEdge(lhs_retval, rhs_retval);
                }
            }

            for (int i = start_idx; i < lhs->CallArgs().size(); i++)
            {
                if (std::holds_alternative<Node*>(lhs->GetArgAt(i)) && std::holds_alternative<Node*>(rhs->GetArgAt(i)))
                {
                    AddEdge(std::get<Node*>(rhs->CallArgs().at(i)), std::get<Node*>(lhs->CallArgs().at(i)));
                }
            }
        }
    }
    else if ((lhs->IsConstructor() || lhs->IsLam()) || rhs->IsProjection())
    {
        if (!rhs->HasPredecessor(lhs))
        {
            rhs->predecessor_nodes.insert(lhs);
            if (rhs->IsSetVar() && !is_init)
            {
                worklist.push_back(rhs);
            }
        }
    }
    else
    {
        if (!lhs->HasSuccessor(rhs))
        {
            lhs->successor_nodes.insert(rhs);
            if (lhs->IsSetVar() && !is_init)
            {
                worklist.push_back(lhs);
            }
        }
    }
}

/*
* Gets set variable if already present, otherwise creates a new set variable and returns it
*/
Node* get_sv(std::string sv_name) {
    if (set_var_map.count(sv_name)) {
        return set_var_map[sv_name];
    }
    Node *sv = new Node(sv_name);
    set_var_map[sv_name] = sv;
    return sv;
}

Node* parseExpression(vector<string>& tokens) {
    
    std::string type = util::Tokenizer::Consume(tokens);

    if (type == "ref") {
        util::Tokenizer::ConsumeToken(tokens, "(");
        std::string const_name = util::Tokenizer::Consume(tokens);
        util::Tokenizer::ConsumeToken(tokens, ",");
        std::string sv_name = util::Tokenizer::Consume(tokens);
        util::Tokenizer::ConsumeToken(tokens, ")");
        
        std::vector<std::variant<std::string, Node*>> args;
        // We know that ref has two arguments, so we just hardcode it here 
        // the first argument is the constant name which refers to the corresponding program variable
        // the second argument is the set variable 
        args.push_back(const_name);
        args.push_back(get_sv(sv_name));

        Node* newRef = new Node("ref", args);
        return newRef;
    }
    else if (type == "proj") {

        util::Tokenizer::ConsumeToken(tokens, "(");
        std::string ref_name = util::Tokenizer::Consume(tokens);
        util::Tokenizer::ConsumeToken(tokens, ",");
        int proj_idx = std::stoi(util::Tokenizer::Consume(tokens));
        util::Tokenizer::ConsumeToken(tokens, ",");
        std::string sv_name = util::Tokenizer::Consume(tokens);
        util::Tokenizer::ConsumeToken(tokens, ")");

        Node *proj = new Node(ref_name, sv_name, proj_idx);
        // Add projection reference to set variable whose projection it is
        get_sv(sv_name)->proj_sv_refs.insert(proj);
        return proj;
    }
    else if (type == "lam_") {
        util::Tokenizer::ConsumeToken(tokens, "[");
        util::Tokenizer::ConsumeToken(tokens, "(");

        std::vector<std::string> param_types;
        while(tokens.back() != ")") {
            std::string param_type = util::Tokenizer::Consume(tokens);
            param_types.push_back(param_type);
            if (tokens.back() != ")") {
                util::Tokenizer::ConsumeToken(tokens, ",");
            }
        }
        util::Tokenizer::ConsumeToken(tokens, ")");
        util::Tokenizer::ConsumeToken(tokens, "->");

        std::string retval_type = util::Tokenizer::Consume(tokens);

        util::Tokenizer::ConsumeToken(tokens, "]");
        util::Tokenizer::ConsumeToken(tokens, "(");

        std::vector<std::variant<std::string, Node*>> args;
        int i = 0;
        while(tokens.back() != ")") {
            std::string arg = util::Tokenizer::Consume(tokens);

            if (tokens.back() != ")") {
                util::Tokenizer::ConsumeToken(tokens, ",");
            }
            
            // Push string to arg only for function name which is the first argument, the rest will be set variables
            if (i == 0)
                args.push_back(arg);
            else
                args.push_back(get_sv(arg));
            i += 1;
        }
        util::Tokenizer::ConsumeToken(tokens, ")");

        Node *lam = new Node("lam_", args, retval_type, param_types);
        return lam;
    }
    else {
        std::string sv_name = type;
        return get_sv(sv_name);
    }
}

/*
* Solver algorithm
* 1. Worklist is initialized with all set variables that have a predecessor edge 
* 2. While worklist is not empty, 
    2.a. Pop a set variable X from the worklist
    2.b. Add edges from X's predecessor edges to it's successor edges
        2.b.1 If destination node's predecessor edges change, put dest on worklist (if it's a set variable)
    2.c For each projection node P of X
        2.c.1 Let Y = value of P
        2.c.2 For each predecessor Pi of P and successor Si of P and each yi in Y
            2.c.2.1 Add edge Pi -> yi
            2.c.2.2 Add edge yi -> Si
            2.c.2.3 If Pi's successor edges change, put Pi on worklist (if it's a set variable)
            2.c.2.4 If Si's predecessor edges change, put Si on worklist (if it's a set variable)
            2.c.2.5 If yi has new edges, add yi to the worklist (if it's a set variable)
*/
void Solve() {

    // Step 1 - Initialoze worklist with all set variables that have a predecessor edge
    for (auto const& [name, node] : set_var_map) {
        if (!node->predecessor_nodes.empty()) {
            worklist.push_back(node);
        }
    }

    while(!worklist.empty()) {
        Node* sv_node = worklist.front();
        worklist.pop_front();

        // Step 2.b
        for (auto pred : sv_node->predecessor_nodes) {
            for (auto succ : sv_node->successor_nodes) {
                AddEdge(pred, succ);
            }
        }

        // Step 2.c
        for (auto proj_sv_ref : sv_node->proj_sv_refs) {
            std::set<Node*> Y;
            // Get the set variable for the projection
            Node *sv_for_proj = set_var_map[proj_sv_ref->ProjSV()];
            // For every predecessor of the set variable that is a constructor and the name matches the projection name,
            // compute the value of the projection
            // We don't consider lams here because projections are only on ref constructor calls
            for (auto pred : sv_for_proj->predecessor_nodes) {
                if (pred->IsConstructor() && pred->Name() == proj_sv_ref->Name()) {

                    // Projections are always only on ref constructor calls with position 1 => this will always be a set variable
                    std::variant<std::string, Node*> arg = pred->GetArgAt(proj_sv_ref->ProjIdx());
                    if (std::holds_alternative<Node*>(arg)) {
                        Node* arg_node = std::get<Node*>(arg);
                        Y.insert(arg_node);
                    }
                }
            }

            for (auto yi : Y)
            {
                int num_of_edges_yi = yi->predecessor_nodes.size() + yi->successor_nodes.size();
                for (auto pred : proj_sv_ref->predecessor_nodes) {
                    int num_of_edges_pred = pred->predecessor_nodes.size() + pred->successor_nodes.size();
                    AddEdge(pred, yi, true);
                    if (pred->IsSetVar() && pred->predecessor_nodes.size() + pred->successor_nodes.size() > num_of_edges_pred) {
                        worklist.push_back(pred);
                    }
                }
                for (auto succ : proj_sv_ref->successor_nodes) {
                    int num_of_edges_succ = succ->predecessor_nodes.size() + succ->successor_nodes.size();
                    // Pass is_init as true since we don't want this function to control adding to the worklist
                    AddEdge(yi, succ, true);
                    if (succ->IsSetVar() && succ->predecessor_nodes.size() + succ->successor_nodes.size() > num_of_edges_succ) {
                        worklist.push_back(succ);
                    }
                }
                // Add yi to worklist if the edges have changed
                if (yi->predecessor_nodes.size() + yi->successor_nodes.size() > num_of_edges_yi) {
                    worklist.push_back(yi);
                }
            }
        }
    }
}


int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: constraint-solver <file path>" << std::endl;
        return EXIT_FAILURE;
    }

    std::ifstream f(argv[1]);
    std::string input_str(std::istreambuf_iterator<char>{f}, {});
    // The input tokenizer to parse the input string.
    util::Tokenizer tk(input_str, {' '}, {"(", ")", "<=", ",", "->", "[", "]"}, {});
    vector<string> tokens = tk.Tokens();

    while(!tokens.empty()) {
        Node* lhs_expr = parseExpression(tokens);
        util::Tokenizer::ConsumeToken(tokens, "<=");
        Node* rhs_expr = parseExpression(tokens);
        util::Tokenizer::ConsumeToken(tokens, "\n");

        // Add edge between lhs and rhs
        AddEdge(lhs_expr, rhs_expr, true);
    }

    // Solve the constraints
    Solve();

    // Solution is the predecessor edges of all set variable which are constructors
    // for a ref constructor, it is the first argument and for lam constructor, it is the first argument as well
    std::map<std::string, std::set<std::string>> solution;
    for (auto const& [name, node] : set_var_map) {
        for (auto pred : node->predecessor_nodes) {
            if (pred->IsConstructor() || pred->IsLam()) {
                if (std::holds_alternative<std::string>(pred->GetArgAt(0))) {
                    std::string arg_name = std::get<std::string>(pred->GetArgAt(0));
                    solution[name].insert(arg_name);
                }
            }
        }
    }

    // Print solution
    for (auto const& [name, preds] : solution) {
        std::cout << name << " -> {";
        int i = 0;
        for (auto pred : preds) {
            if (i != preds.size() - 1) {
                std::cout << pred << ", ";
            } else {
                std::cout << pred;
            }
            i += 1;
        }
        std::cout << "}" << std::endl;
    }
    std::cout << std::endl;
}