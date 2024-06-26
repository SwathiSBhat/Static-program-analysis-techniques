#include <fstream>
#include <stack>

#include "interval_analysis.hpp"
#include "interval_execute.hpp"
#include "../headers/datatypes.h"

/*
 * Class to perform integer interval analysis on a function.
 */
class IntervalAnalysis {
public:

    /*
     * This is the final solution which we get by running through all the basic
     * blocks one last time after the worklist algorithm has completed.
     */
    std::map<std::string, interval_abstract_store> solution;

    /*
     * This data structure holds a list of all basic blocks that have ever been
     * on the worklist. At the end of our analysis, we will only print out the
     * basic blocks that are on this list.
     */
    std::set<std::string> bbs_to_output;

    /*
     * The underlying program data structure.
     */
    Program program;

    /*
     * Our bb2store is a map from a basic block label to an
     * interval_abstract_store.
     */
    std::map<std::string, interval_abstract_store> bb2store;

    /*
     * Our worklist is a deque of basic block labels.
     */
    std::deque<std::string> worklist;

    /*
     * We also need to maintain the list of loop headers.
     */
    std::unordered_set<std::string> loop_headers;

    /*
     * The name of the function to analyze.
     */
    std::string func_name;

    /*
     * Constructor.
     */
    IntervalAnalysis(Program p) : program(p) {};

    /*
     * Get the set of int-typed global variables.
     */
    void get_int_type_globals(std::unordered_set<std::string> &int_type_globals) {
        for (const auto &global_var : program.globals) {
            Variable *global_var_ptr = global_var->globalVar;
            if (global_var_ptr->isIntType()) {
                int_type_globals.insert(global_var_ptr->name);
            }
        }
    }

    /*
     * Get the set of all the int-typed local variables and function parameters
     * whose addresses were taken using the $addrof command.
     */
    void get_addrof_ints(std::unordered_set<std::string> &addrof_ints, const std::string &func_name) {
        for (const auto &basic_block : program.funcs[func_name]->bbs) {
            for (auto instruction = basic_block.second->instructions.begin(); instruction != basic_block.second->instructions.end(); ++instruction) {
                if ((*instruction)->instrType == InstructionType::AddrofInstrType) {
                    if (dynamic_cast<AddrofInstruction *>(*instruction)->rhs->isIntType()) {
                        if (program.funcs[func_name]->locals.count(dynamic_cast<AddrofInstruction *>(*instruction)->rhs->name) != 0) {
                            addrof_ints.insert(dynamic_cast<AddrofInstruction *>(*instruction)->rhs->name);
                        } else {
                            for (auto param : program.funcs[func_name]->params) {
                                if (param && param->name == dynamic_cast<AddrofInstruction *>(*instruction)->rhs->name) {
                                    addrof_ints.insert(dynamic_cast<AddrofInstruction *>(*instruction)->rhs->name);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /*
     * Get the list of loop headers through post-order traversal of all basic
     * blocks in the function.
     */
    void get_loop_headers(std::unordered_set<std::string> &loop_headers,
                          const std::string &func_name) {
        std::unordered_set<std::string> visited;
        std::vector<std::string> post_order;
        std::string entry_bb = "entry";
        std::string current_bb = entry_bb;
        std::stack<std::string> stack;

        stack.push(current_bb);

        while (!stack.empty()) {
            current_bb = stack.top();
            stack.pop();

            if (visited.count(current_bb) == 0) {
                visited.insert(current_bb);
                post_order.push_back(current_bb);
                Instruction *instr = program.funcs[func_name]->bbs[current_bb]->terminal;
                // Check instruction type to get next basic block
                if (instr->instrType == InstructionType::BranchInstrType) {
                    BranchInstruction *branch_instr = dynamic_cast<BranchInstruction*>(instr);
                    stack.push(branch_instr->tt);
                    stack.push(branch_instr->ff);
                } else if (instr->instrType == InstructionType::JumpInstrType) {
                    JumpInstruction *jump_instr = dynamic_cast<JumpInstruction*>(instr);
                    stack.push(jump_instr->label);
                } else if (instr->instrType == InstructionType::CallDirInstrType) {
                    CallDirInstruction *call_dir_instr = dynamic_cast<CallDirInstruction*>(instr);
                    stack.push(call_dir_instr->next_bb);
                } else if (instr->instrType == InstructionType::CallIdrInstrType) {
                    CallIdrInstruction *call_idr_instr = dynamic_cast<CallIdrInstruction*>(instr);
                    stack.push(call_idr_instr->next_bb);
                } else if (instr->instrType == InstructionType::RetInstrType) {
                    // No-op
                }
                else {
                    //std::cout << "Terminal instruction type not found" << std::endl;
                }
            }
            else {
                loop_headers.insert(current_bb);
            }
        }
        std::reverse(post_order.begin(), post_order.end());
        /*std::cout << "Printing post order" << std::endl;
        for (auto bb : post_order) {
            std::cout << bb << " ";
        }
        std::cout << std::endl;*/
        return;
    }

    /*
     * Initialize the abstract store for the entry basic block.
     */
    void InitEntryStore() {
        for (auto param : program.funcs[func_name]->params) {
            if (param->isIntType()) {
                bb2store["entry"][param->name] = AbstractVals::TOP;
            }
        }
    }

    /*
     * Run interval analysis on a given function.
     */
    void AnalyzeFunc() {
        Function *func = program.funcs[func_name];

        /*
         * Data structures required for the prep stage.
         */
        std::unordered_set<std::string> addrof_ints;

        /*
         * Populate the set of loop headers for which widening will be
         * performed.
         */
        get_loop_headers(loop_headers, func_name);

        for (const auto &loop_header : loop_headers) {
            //std::cout << loop_header << "is a loop header " << __FILE_NAME__ << ":" << __LINE__ << std::endl;
        }

        /*
         * First initialize the abstract store for the entry basic block. Then
         * add the entry basic block to the worklist.
         */
        InitEntryStore();
        worklist.push_back("entry");

        /*
         * Worklist algorithm.
         */
        while (!worklist.empty()) {
            std::string current_bb = worklist.front();
            worklist.pop_front();
            //std::cout << "About to execute " << current_bb << " " << __FILE_NAME__ << ":" << __LINE__ << std::endl;

            //std::cout << "Before transfer function" << std::endl;
            //print(bb2store[current_bb]);

            /*
             * Perform our transfer function on the current basic block.
             */
            execute(&program,
                    func->bbs[current_bb],
                    bb2store[current_bb],
                    bb2store,
                    worklist,
                    addrof_ints,
                    bbs_to_output,
                    false,
                    loop_headers);

            //std::cout << "After transfer function" << std::endl;
            //print(bb2store[current_bb]);

            /*
             * Keep track of all the basic blocks we add to the worklist.
             */
            for (const auto &i : worklist) {
                //std::cout << "Adding " << i << " to bbs_to_output " << __FILE_NAME__ << ":" << __LINE__ << std::endl;
                bbs_to_output.insert(i);
            }

            //std::cout << "This is the worklist now:" << std::endl;
            for (const auto &i: worklist) {
                //std::cout << i << " ";
                //bbs_to_output.insert(i);
            }
        }

        //std::cout << "Exited loop " << __FILE_NAME__ << ":" << __LINE__ << std::endl;

        /*
         * Once we've completed the worklist algorithm, let's execute our
         * transfer function once more on each basic block to get their exit
         * abstract stores.
         *
         * TODO Let's just try this.
         */
        //for (const auto &bb_label : bbs_to_output) {
        for (const auto &[bb_label, bb_store] : bb2store) {
            //std::cout << "Computing the solution for " << bb_label << " " << __FILE_NAME__ << ":" << __LINE__ << std::endl;
            solution[bb_label] = execute(&program,
                                         func->bbs[bb_label],
                                         bb2store[bb_label],
                                         bb2store,
                                         worklist,
                                         addrof_ints,
                                         bbs_to_output,
                                         true,
                                         loop_headers);
        }

        //std::cout << "Computed exit abstract stores " << __FILE_NAME__ << ":" << __LINE__ << std::endl;
        //std::cout << bbs_to_output.size() << " " << __FILE_NAME__ << ":" << __LINE__ << std::endl;

        /*
         * Finally, let's print out the exit abstract stores of each basic block
         * in alphabetical order.
         */
        for (const auto &[bb_label, bb_store] : solution) {
            std::cout << bb_label << ":" << std::endl;
            print(solution[bb_label]);
            std::cout << std::endl;
        }
    }
};

/*
 * This is the entry point for our interval analysis.
 */
int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cout << __FILE__ << ":" << __LINE__ << std::endl;
        return EXIT_FAILURE;
    }
    std::ifstream f(argv[2]);
    //std::ifstream f("/Users/vinayakgajjewar/PhD/CS260/CS-260-Program-Analysis/interval-analysis/interval-analysis-tests/intervals-noptr-nocall.lir.json");
    Program p = Program(json::parse(f));
    IntervalAnalysis i = IntervalAnalysis(p);
    i.func_name = argv[3];
    //i.func_name = "test";
    i.AnalyzeFunc();
}