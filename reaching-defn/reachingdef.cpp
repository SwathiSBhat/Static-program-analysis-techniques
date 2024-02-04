#include <fstream>
#include <vector>
#include <unordered_set>
#include <set>
#include<deque>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>

#include "../headers/datatypes.h"
#include "execute_rdef.hpp"
#include "rtype.hpp"

using json = nlohmann::json;

/*
    Class that contains methods to perform constant analysis on function
*/
class ReachingDef {
public:

    /*
     * This data structure holds a list of all basic blocks that have ever been
     * on the worklist. At the end of our analysis, we will only print out the
     * basic blocks that are on this list.
     */
    std::set<std::string> bbs_to_output;

    ReachingDef(Program program) : program(program) {};

    /*
    Method to get all pointer typed globals, parameters, locals of the function
    */
    std::unordered_set<Variable*> get_ptrs() {
        std::unordered_set<Variable*> PTRS;
        for (auto global_var : program.globals) {
            Variable *global_var_ptr = global_var->globalVar;
            if (global_var_ptr->type->indirection > 0) {
                PTRS.insert(global_var_ptr);
            }  
        }
        for (auto param : program.funcs[funcname]->params) {
            if (param->type->indirection > 0) {
                PTRS.insert(param);
            }
        }
        for (auto local : program.funcs[funcname]->locals) {
            if (local.second->type->indirection > 0) {
                PTRS.insert(local.second);
            }
        }
        return PTRS;
    }

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
     * Get all reachable types from the set given in arg
    */
   std::vector<ReachableType*> get_reachable_types(std::unordered_set<Variable*> PTRS)
   {
      // TODO - Add logic here
        std::vector<ReachableType*> reachable_types;
        return reachable_types;
   } 

    /*
     * Get all address taken local variable + function parameters + global variables
    */
    void get_addr_taken(std::unordered_set<Variable*> &addr_taken) {  
        //program.funcs[func_name]->bbs["entry"]->pretty_print(json::parse("{\"structs\": \"false\",\"globals\": \"false\",\"functions\": {\"bbs\": {\"instructions\" : \"true\"}},\"externs\": \"false\"}"));
        
        // TODO: Handle scenario where there are variables with same name in different scopes
        for (auto basic_block : program.funcs[funcname]->bbs) {
            for (auto instruction = basic_block.second->instructions.begin(); instruction != basic_block.second->instructions.end(); ++instruction) {
                if ((*instruction)->instrType == InstructionType::AddrofInstrType) {
                        if (program.funcs[funcname]->locals.count(dynamic_cast<AddrofInstruction*>(*instruction)->rhs->name) != 0)
                        {
                            addr_taken.insert(dynamic_cast<AddrofInstruction*>(*instruction)->rhs);
                        }
                        // TODO: Convert globals to ordered set in datatypes.h
                        else
                        {
                            for (auto global: program.globals)
                            {
                                if (global->globalVar->name == dynamic_cast<AddrofInstruction*>(*instruction)->rhs->name)
                                {
                                    addr_taken.insert(dynamic_cast<AddrofInstruction*>(*instruction)->rhs);
                                }
                            }

                            for (auto param : program.funcs[funcname]->params) {
                                if (param && param->name == dynamic_cast<AddrofInstruction*>(*instruction)->rhs->name) {
                                    addr_taken.insert(dynamic_cast<AddrofInstruction*>(*instruction)->rhs);
                                }
                            }
                        }
                }
            }
        }

        return; 
    }

    /*
        Uber level method to run the analysis on a function
    */
    void AnalyzeFunc(const std::string &func_name) {

        Function *func = program.funcs[func_name];
        if (!func) {
            std::cout << "Function not found" << std::endl;
            return;
        }

        funcname = func_name;


        // data structures required for prep stage
        std::unordered_set<Variable*> addr_taken; // contains all variables that are address taken i.e addrof is used on them
        // TODO: This can be converted to an unordered set of type ReachableType with it's own hash function. Need to figure out how to do it
        std::vector<ReachableType*> reachable_types; // reachable data types from all pointers in the function
        
        // Prep steps:
        // 1. Compute set of variables that are address taken
        get_addr_taken(addr_taken);
        // 2. Compute reachable types from all pointers in the function
        std::unordered_set<Variable*> PTRS = get_ptrs(); // Get all pointer typed globals, parameters, locals of the function
        reachable_types = get_reachable_types(PTRS);
        // 3. Put all fake variables in the address taken set
        // TODO - Yet to write logic

        /*
            Setup steps
            1. Initialize the abstract store for 'entry' basic block
            2. Add 'entry' basic block to worklist
        */
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
            /*std::cout << "Abstract store of " << current_bb << " before transfer function: " << std::endl;
            for (auto def : bb2store[current_bb]) {
                std::cout << def.first << " -> {";
                for(auto def : def.second)
                {
                    std::cout << def << ",";
                }
                std::cout << "}" << std::endl;
            }*/
            
            execute(&program,
                    func->bbs[current_bb],
                    bb2store,
                    worklist,
                    addr_taken,
                    bbs_to_output,
                    soln
                    );

            /*std::cout << "Abstract store of " << current_bb << " after transfer function: " << std::endl;
            for (auto def : bb2store[current_bb]) {
                std::cout << def.first << " -> {";
                for(auto def : def.second)
                {
                    std::cout << def << ",";
                }
                std::cout << "}" << std::endl;
            }*/

            //std::cout << "This is the worklist now:" << std::endl;
            for (const auto &i: worklist) {
                //std::cout << i << " ";
                bbs_to_output.insert(i);
            }
            //std::cout << std::endl;
        }

        /*std::cout << " bbs to output: " << std::endl;
        for (const auto &i: bbs_to_output) {
            std::cout << i << " ";
        }*/

        /*
         * Once we've completed the worklist algorithm, let's execute our
         * transfer function once more on each basic block to get their exit
         * abstract stores.
         */

        for (const auto &it : bbs_to_output) {
            execute(&program,
                    func->bbs[it],
                    bb2store,
                    worklist,
                    addr_taken,
                    bbs_to_output,
                    soln,
                    true
                    );
        }

        /*
         * Finally, let's print out the exit abstract stores of each program point in
         * alphabetical order.
         */
        /* Workaround to print bb1.11 after bb1.9 */
        std::vector<std::string>sorted_pps;
        for (auto it = soln.begin(); it != soln.end(); it++) {
            sorted_pps.push_back(it->first);
        }
        std::sort(sorted_pps.begin(), sorted_pps.end(), [](const std::string &a, const std::string &b) {
            {
                std::string a1 = a.substr(0, a.rfind("."));
                std::string a2 = a.substr(a.rfind(".") + 1);
                std::string b1 = b.substr(0, b.rfind("."));
                std::string b2 = b.substr(b.rfind(".") + 1);
                // std::cout << "a1: " << a1 << " a2: " << a2 << " b1: " << b1 << " b2: " << b2 << std::endl;
                if (a1 == b1 && (a2 == "term" || b2 == "term"))
                    return false;
                if (a1 == b1) {
                    return std::stoi(a2) < std::stoi(b2);
                }
                return a1 < b1;
            }
        });
        /*std::cout <<" Sorted vector " << std::endl;
        for (auto it = sorted_pps.begin(); it != sorted_pps.end(); it++) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;*/

        for (auto it = sorted_pps.begin(); it != sorted_pps.end(); it++) {
            if (soln[*it].size() == 0) {
                continue;
            }
            std::cout << *it << " -> {";
            int indx = 0;
            int size = soln[*it].size();
            for (auto def = soln[*it].begin(); def != soln[*it].end(); def++, indx++) {
                if (indx == size - 1) {
                    std::cout << *def << "}" << std::endl;
                } else {
                    std::cout << *def << ", ";
                }
            }
        }
    }

    Program program;
    /*
     * Our bb2store is a map from a basic block label to a map of var name and a set of its definitions.
     */
    std::map<std::string, std::map<std::string, std::set<std::string>>> bb2store;
    /*
     * Our worklist is a queue containing BasicBlock labels.
     */
    std::deque<std::string> worklist;
    /*
     * This is the final solution which we get by running through all the basic blocks one last time after the worklist algorithm has completed.
     * The solution is map of program points and their definitions
    */
    std::map<std::string, std::set<std::string>> soln;

private:
    std::string funcname;
};

int main(int argc, char* argv[]) 
{
    if (argc != 4) {
        std::cerr << "Usage: reachingdef <lir file path> <lir json filepath> <funcname>" << std::endl;
        return EXIT_FAILURE;
    }

    std::ifstream f(argv[2]);
    json lir_json = json::parse(f);

    std::string func_name = argv[3];

    Program program = Program(lir_json);
    ReachingDef reaching_def = ReachingDef(program);
    reaching_def.AnalyzeFunc(func_name);

    return 0;
}