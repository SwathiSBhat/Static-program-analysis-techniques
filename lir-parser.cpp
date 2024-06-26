/*
 * CS260 winter 2024 assignment 0.
 */

#include <fstream>
#include <iostream>
#include "headers/datatypes.h"

/*
 * https://github.com/nlohmann/json
 */
#include "headers/json.hpp"

using json = nlohmann::json;

int main(int argc, char *argv[]) {

    if (argc != 2) {
        std::cerr << "Usage: assn0 <filepath>" << std::endl;
        return EXIT_FAILURE;
    }
    /*
     * Open and parse our JSON.
     */
    std::ifstream f(argv[1]);
    json lir_json = json::parse(f);

    /*
     * Initialize accumulators for the statistics we need.
     */
    uint64_t num_struct_fields = 0;
    uint64_t num_return_funcs = 0;
    uint64_t num_func_params = 0;
    uint64_t num_local_vars = 0;
    uint64_t num_basic_blocks = 0;
    uint64_t num_instructions = 0;
    uint64_t num_terminals = 0;
    uint64_t num_int_locals_globals = 0;
    uint64_t num_struct_locals_globals = 0;
    uint64_t num_ptr_int_locals_globals = 0;
    uint64_t num_ptr_struct_locals_globals = 0;
    uint64_t num_ptr_func_locals_globals = 0;
    uint64_t num_ptr_ptr_locals_globals = 0;


    Program *p = new Program(lir_json);
    p->print_pretty(json::parse("{\"structs\": \"false\",\"globals\": \"false\",\"functions\": {\"bbs\": {\"instructions\" : \"true\"}},\"externs\": \"false\"}"));

    /*
     * Scan through our top-level LIR JSON object.
     */
    for (auto &[lir_key, lir_value] : lir_json.items()) {

        /*
         * Let's look at our structs.
         */
        if (lir_key == "structs") {

            /*
             * Loop through each struct.
             */
            for (auto structs: lir_value.items()) {

                /*
                 * Loop through all fields within that struct and count up the
                 * number of fields.
                 */
                //Struct *s = new Struct(structs);
                for (auto &[struct_key, struct_val] : structs.value().items()) {
                    num_struct_fields++;
                }
            }
        }

        /*
         * Let's look at our functions.
         */
        if (lir_key == "functions") {

            /*
             * Loop through all functions.
             */
            for (auto &[func_key, func_value] : lir_value.items()) {
                //Function *f = new Function(func_value);
                /*
                 * Only increment our accumulator if the function actually
                 * returns something.
                 */
                if (func_value["ret_ty"] != nullptr) {
                    num_return_funcs++;
                }

                /*
                 * Count up the function parameters, local vars, basic blocks,
                 * and terminals.
                 */
                num_func_params += func_value["params"].size();
                num_local_vars += func_value["locals"].size();
                num_basic_blocks += func_value["body"].size();
                num_terminals += func_value["body"].size();

                /*
                 * Count up the types of each local var.
                 */
                for (auto &local : func_value["locals"]) {
                    if (local["typ"] == "Int") {
                        num_int_locals_globals++;
                        continue;
                    }
                    if (local["typ"].contains("Struct")) {
                        num_struct_locals_globals++;
                        continue;
                    }

                    /*
                     * If we're here, local is definitely a pointer. We just
                     * have to figure out what kind.
                     *
                     * TODO This is better as a switch/case statement.
                     */
                    if (local["typ"]["Pointer"] == "Int") {
                        num_ptr_int_locals_globals++;
                    } else if (local["typ"]["Pointer"].contains("Struct")) {
                        num_ptr_struct_locals_globals++;
                    } else if (local["typ"]["Pointer"].contains("Function")) {
                        num_ptr_func_locals_globals++;
                    } else {

                        /*
                         * If we're here, our local variable is a pointer to a
                         * pointer.
                         */
                        num_ptr_ptr_locals_globals++;
                    }
                }

                /*
                 * Count up the number of instructions by going through each
                 * block.
                 */
                for (auto &[func_body_key, func_body_val] : func_value["body"].items()) {

                    num_instructions += func_body_val["insts"].size();
                    //BasicBlock *bb = new BasicBlock(func_body_val);
                }
            }
        }

        /*
         * Count up global variables.
         */
        if (lir_key == "globals") {
            for (auto &[globals_key, globals_val] : lir_value.items()) {

                //Global *g = new Global(globals_val);
                /*
                 * Now we need to find the type of each global so we can
                 * increment the right accumulator.
                 */

                if (globals_val["typ"] == "Int") {
                    num_int_locals_globals++;
                    continue;
                }

                if (globals_val["typ"].contains("Struct")) {
                    num_struct_locals_globals++;
                    continue;
                }

                /*
                 * If it's a pointer, we have to figure out what kind of pointer
                 * it is.
                 */
                if (globals_val["typ"]["Pointer"] != nullptr) {
                    if (globals_val["typ"]["Pointer"].contains("Struct")) {
                        num_ptr_struct_locals_globals++;
                        continue;
                    }
                    if (globals_val["typ"]["Pointer"].contains("Function")) {
                        num_ptr_func_locals_globals++;
                        continue;
                    }
                    if (globals_val["typ"]["Pointer"] == "Int") {
                        num_ptr_int_locals_globals++;
                        continue;
                    }
                    if (globals_val["typ"]["Pointer"].contains("Pointer")) {
                        num_ptr_ptr_locals_globals++;
                        continue;
                    }
                }
            }
        }
        /*
        * Parse externs in the program
        */
        if (lir_key == "externs") {
            for (auto &[externs_key, externs_val] : lir_value.items()) {
                //ExternalFunction *e = new ExternalFunction(externs_val);
            }
        }
    }

    /*
     * Now let's print out our statistics.
     */
    std::cout << "Number of fields across all struct types: " << num_struct_fields << std::endl;
    std::cout << "Number of functions that return a value: " << num_return_funcs << std::endl;
    std::cout << "Number of function parameters: " << num_func_params << std::endl;
    std::cout << "Number of local variables: " << num_local_vars << std::endl;
    std::cout << "Number of basic blocks: " << num_basic_blocks << std::endl;
    std::cout << "Number of instructions: " << num_instructions << std::endl;
    std::cout << "Number of terminals: " << num_terminals << std::endl;
    std::cout << "Number of int locals/globals: " << num_int_locals_globals << std::endl;
    std::cout << "Number of struct locals/globals: " << num_struct_locals_globals << std::endl;
    std::cout << "Number of int pointer locals/globals: " << num_ptr_int_locals_globals << std::endl;
    std::cout << "Number of struct pointer locals/globals: " << num_ptr_struct_locals_globals << std::endl;
    std::cout << "Number of function pointer locals/globals: " << num_ptr_func_locals_globals << std::endl;
    std::cout << "Number of pointer pointer locals/globals: " << num_ptr_ptr_locals_globals << std::endl;

    return EXIT_SUCCESS;
}