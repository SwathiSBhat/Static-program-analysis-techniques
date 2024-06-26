#pragma once

#include "../headers/datatypes.h"
#include <unordered_set>
#include <iostream>

class ReachableType: public Type {
    public:
    ReachableType(DataType type, void* ptr_type, int indirection) {
        this->type = type;
        this->ptr_type = ptr_type;
        this->indirection = indirection;
    }

    ReachableType(Type *var_type) {
        this->type = var_type->type;
        this->ptr_type = var_type->ptr_type;
        this->indirection = var_type->indirection;
    }

    static bool isPresentInSet(std::unordered_set<ReachableType*> &rset, ReachableType *rtype)
    {
        for (auto it = rset.begin(); it != rset.end(); it++)
        {
            if ((*it)->type == DataType::IntType && (*it)->type == rtype->type && (*it)->ptr_type == rtype->ptr_type && (*it)->indirection == rtype->indirection)
            {
                return true;
            }
            else if ((*it)->type == DataType::FuncType && (*it)->type == rtype->type && (*it)->indirection == rtype->indirection)
            {
                FunctionType *f1 = (FunctionType*)(*it)->ptr_type;
                FunctionType *f2 = (FunctionType*)rtype->ptr_type;
                
                if ((f1->ret && !f2->ret) || (!f1->ret && f2->ret))
                {
                    continue;
                }
                if ((!f1->ret && !f2->ret) || (f1->ret->type == f2->ret->type && f1->ret->indirection == f2->ret->indirection))
                {
                    if (f1->ret->type == DataType::StructType)
                    {
                        if (((StructType*)f1->ret->ptr_type)->name != ((StructType*)f2->ret->ptr_type)->name)
                        {
                            continue;
                        }
                    }

                    if (f1->params.size() == f2->params.size())
                    {
                        bool areParamsEqual = true;
                        for (int i = 0; i < f1->params.size(); i++)
                        {
                            if (!isEqualType((f1->params)[i], (f2->params)[i]))
                            {
                                areParamsEqual = false;
                                break;
                            }
                        }
                        if (areParamsEqual) {
                            return true;
                        }
                    }
                }
            }
            else if ((*it)->type == DataType::StructType && (*it)->type == rtype->type && (*it)->indirection == rtype->indirection)
            {
                StructType *s1 = (StructType*)(*it)->ptr_type;
                StructType *s2 = (StructType*)rtype->ptr_type;
                if (s1->name == s2->name)
                {
                    return true;
                }
            }
        }
        return false;
    }

    static void GetReachableType(Program *program, ReachableType *var_type, std::unordered_set<ReachableType*> &rset)
    {
        /*
         * Ensuring that the type is a pointer i.e type = &type' and type' is not a function type
        */

        if (var_type->indirection > 0 && !(var_type->indirection == 1 && var_type->type == DataType::FuncType))
        {
            /*
             * If the type is a pointer, then we need to get the reachable type of the pointer
             * and add it to the set
            */
            void* ptr_type = var_type->ptr_type;

            if (!(var_type->indirection == 1 && var_type->type == DataType::StructType))
            {
                if (var_type->indirection == 1)
                {
                    ptr_type = nullptr;
                }
                ReachableType *rtype = new ReachableType(var_type->type, ptr_type, var_type->indirection - 1);
                if (!isPresentInSet(rset, rtype)) {
                    rset.insert(rtype);
                }
            }
            /*
            * Get reachable type for next level pointer type
            */
            GetReachableType(program, new ReachableType(var_type->type, ptr_type, var_type->indirection - 1), rset);
        } 
        else if (var_type->type == DataType::StructType)
        {
            /*
             * If the type is a struct, then we need to get the reachable types of the struct
             * and add them to the set
            */
            std::string struct_name = ((StructType*)var_type->ptr_type)->name;
            for (auto field = program->structs[struct_name]->fields.begin(); field != program->structs[struct_name]->fields.end(); field++)
            {
                ReachableType *rtype = new ReachableType(new ReachableType((*field)->type->type, (*field)->type->ptr_type, (*field)->type->indirection));
                if (!isPresentInSet(rset,rtype) && !((*field)->type->type == DataType::StructType && (*field)->type->indirection == 0))
                {
                    rset.insert(rtype);
                }
                GetReachableType(program, new ReachableType((*field)->type->type,  (*field)->type->ptr_type, (*field)->type->indirection), rset);
            }
        }
    }
};