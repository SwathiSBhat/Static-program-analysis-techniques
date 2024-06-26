#include <fstream>
#include <vector>
#include <unordered_set>
#include <set>

#include "../headers/datatypes.h"
#include "./execute.hpp"

using json = nlohmann::json;

/*
    Class that contains methods to perform constant analysis on function
*/
class ConstantAnalysis {
public:

    /*
     * This data structure holds a list of all basic blocks that have ever been
     * on the worklist. At the end of our analysis, we will only print out the
     * basic blocks that are on this list.
     */
    std::set<std::string> bbs_to_output;

    ConstantAnalysis(Program program) : program(program) {};

    /*
    Method to get the set of int-typed global variables
    */
    void get_int_type_globals(std::unordered_set<std::string> &int_type_globals) {
        for (auto global_var : program.globals) {
            Variable *global_var_ptr = global_var->globalVar;
            if (global_var_ptr->isIntType()) {
                int_type_globals.insert(global_var_ptr->name);
            }  
        }
        return;
    }

    /*
     * Get the list of names of all the int-typed local variables + function parameters whose addresses were
     * taken using the $addrof command.
     * TODO: This should also include addrof of global variables but we are not doing it for assignment 1
    */
    void get_addr_of_int_types(std::unordered_set<std::string> &addr_of_int_types, const std::string &func_name) {  
        //program.funcs[func_name]->bbs["entry"]->pretty_print(json::parse("{\"structs\": \"false\",\"globals\": \"false\",\"functions\": {\"bbs\": {\"instructions\" : \"true\"}},\"externs\": \"false\"}"));
        for(auto it = program.funcs[func_name]->bbs.begin(); it != program.funcs[func_name]->bbs.end(); ++it) {
            std::cout<<it->first<<std::endl;
        }
        for (auto basic_block : program.funcs[func_name]->bbs) {
            for (auto instruction = basic_block.second->instructions.begin(); instruction != basic_block.second->instructions.end(); ++instruction) {
                if ((*instruction)->instrType == InstructionType::AddrofInstrType) {
                    if (dynamic_cast<AddrofInstruction*>(*instruction)->rhs->isIntType()) { 
                        if (program.funcs[func_name]->locals.count(dynamic_cast<AddrofInstruction*>(*instruction)->rhs->name) != 0)
                        {
                            addr_of_int_types.insert(dynamic_cast<AddrofInstruction*>(*instruction)->rhs->name);
                        }
                        else
                        {
                            for (auto param : program.funcs[func_name]->params) {
                                if (param && param->name == dynamic_cast<AddrofInstruction*>(*instruction)->rhs->name) {
                                    addr_of_int_types.insert(dynamic_cast<AddrofInstruction*>(*instruction)->rhs->name);
                                }
                            }
                        }
                    }
                }
            }
        }

        return; 
    }

    /*
        Initialize the abstract store for 'entry' basic block
    */
    void InitEntryStore() {
        
        std::string bb_name = "entry";
        AbstractStore store = AbstractStore();

        // Initialize all globals and parameters in function to TOP

        // TODO: Ignoring global variables for assignment 1
        /*for (auto global_var : program.globals) {
            Variable *global_var_ptr = global_var->globalVar;
            if (global_var_ptr->isIntType()) {
                // std::cout << "Setting global: " << global_var_ptr->name << " to TOP" << std::endl;
                store.abstract_store[global_var_ptr->name] = AbstractVal::TOP;
            }  
        }*/

        for (auto param : program.funcs[funcname]->params) {
            if (param && param->isIntType()) {
                // std::cout << "Setting parameter: " << param->name << " to TOP" << std::endl;
                store.abstract_store[param->name] = AbstractVal::TOP;
            }  
        }
        bb2store[bb_name] = store;
        return;
    }

    /*
        Uber level method to run the analysis on a function
    */
    void AnalyzeFunc(const std::string &func_name) {

        Function *func = program.funcs[func_name];
        if (!func) {
            std::cout << "Func not found" << std::endl;
            return;
        }

        funcname = func_name;
        
        // data structures required for prep stage
        std::unordered_set<std::string> int_type_globals; // contains names of all global variables of type int
        std::unordered_set<std::string> addr_of_int_types; // contains names of all variables that are addresses of int types
        
        // Prep steps:
        // 1. Compute set of int-typed global variables
        get_int_type_globals(int_type_globals);
        // 2. Compute set of variables that are addresses of int-typed variables
        get_addr_of_int_types(addr_of_int_types, func_name);

        /*
         * We also need to initialize bb2store entries for all the basic blocks
         * in the function (I think.)
         */
        for (const auto &[bb_label, bb] : program.funcs[func_name]->bbs) {
            //std::cout << "Initializing empty abstract store for " << bb_label << " basic block" << std::endl;
            bb2store[bb_label] = AbstractStore();
        }

        /*
            Setup steps
            1. Initialize the abstract store for 'entry' basic block
            2. Add 'entry' basic block to worklist
        */
        InitEntryStore();
        worklist.push_back("entry");
        bbs_to_output.insert("entry");


        /*
            Worklist algorithm
            1. Pop a basic block from the worklist
            2. Perform the transfer function on the basic block
            3. For each successor of the basic block, join the abstract store of the successor with the abstract store of the current basic block
            4. If the abstract store of the successor has changed, add the successor to the worklist
        */
        
        while (!worklist.empty()) {
            std::string current_bb = worklist.front();
            worklist.pop_front();

            // Perform the transfer function on the current basic block
            //std::cout << "Abstract store of " << current_bb << " before transfer function: " << std::endl;
            //bb2store[current_bb].print();
            
            execute(&program,
                    func->bbs[current_bb],
                    bb2store[current_bb],
                    bb2store,
                    worklist,
                    addr_of_int_types,
                    bbs_to_output);

            //std::cout << "Abstract store of " << current_bb << " after transfer function: " << std::endl;
            //bb2store[current_bb].print();

            //std::cout << "This is the worklist now:" << std::endl;
            for (const auto &i: worklist) {
                //std::cout << i << " ";
                bbs_to_output.insert(i);
            }
            //std::cout << std::endl;
        }

        /*
         * Once we've completed the worklist algorithm, let's execute our
         * transfer function once more on each basic block to get their exit
         * abstract stores.
         */
        for (const auto &it : bbs_to_output) {
            soln[it] = execute(&program,
                                func->bbs[it],
                                bb2store[it],
                                bb2store,
                                worklist,
                                addr_of_int_types,
                                bbs_to_output,
                                true);
        }

        /*
         * Finally, let's print out the exit abstract stores of each basic block in
         * alphabetical order.
         */
        for (const auto &bb_label : bbs_to_output) {
            std::cout << bb_label << ":" << std::endl;
            soln[bb_label].print();
            std::cout << std::endl;
        }
    }

    Program program;
    /*
     * Our bb2store is a map from a basic block label to an AbstractStore.
     */
    std::map<std::string, AbstractStore> bb2store;
    /*
     * Our worklist is a queue containing BasicBlock labels.
     */
    std::deque<std::string> worklist;
    /*
     * This is the final solution which we get by running through all the basic blocks one last time after the worklist algorithm has completed.
    */
    std::map<std::string, AbstractStore> soln;

private:
    std::string funcname;
};

int main(int argc, char* argv[]) 
{
    if (argc != 4) {
        std::cerr << "Usage: constant-analysis <lir file path> <lir json filepath> <funcname>" << std::endl;
        return EXIT_FAILURE;
    }

    std::ifstream f(argv[2]);
    json lir_json = json::parse(f);

    std::string func_name = argv[3];

    Program program = Program(lir_json);
    ConstantAnalysis constant_analysis = ConstantAnalysis(program);
    constant_analysis.AnalyzeFunc(func_name);

    return 0;
}