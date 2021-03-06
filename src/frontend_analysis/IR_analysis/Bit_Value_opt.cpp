/*
 *
 *                   _/_/_/    _/_/   _/    _/ _/_/_/    _/_/
 *                  _/   _/ _/    _/ _/_/  _/ _/   _/ _/    _/
 *                 _/_/_/  _/_/_/_/ _/  _/_/ _/   _/ _/_/_/_/
 *                _/      _/    _/ _/    _/ _/   _/ _/    _/
 *               _/      _/    _/ _/    _/ _/_/_/  _/    _/
 *
 *             ***********************************************
 *                              PandA Project
 *                     URL: http://panda.dei.polimi.it
 *                       Politecnico di Milano - DEIB
 *                        System Architectures Group
 *             ***********************************************
 *              Copyright (c) 2004-2017 Politecnico di Milano
 *
 *   This file is part of the PandA framework.
 *
 *   The PandA framework is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * @file Bit_Value_opt.cpp
 * @brief
 *
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 * $Revision$
 * $Date$
 * Last modified by $Author$
 *
*/

//Header include
#include "Bit_Value_opt.hpp"

///Autoheader include
#include "config_HAVE_FROM_DISCREPANCY_BUILT.hpp"

//Behavior include
#include "application_manager.hpp"
#include "behavioral_helper.hpp"
#include "function_behavior.hpp"
#include "call_graph_manager.hpp"
#include "call_graph.hpp"

///HLS/vcd include
#include "Discrepancy.hpp"

//Parameter include
#include "Parameter.hpp"

//STD include
#include <fstream>
#include <string>
#include <math.h>
#include <boost/range/adaptor/reversed.hpp>

//Tree include
#include "tree_manager.hpp"
#include "tree_manipulation.hpp"
#include "ext_tree_node.hpp"
#include "tree_basic_block.hpp"
#include "tree_node.hpp"
#include "tree_reindex.hpp"
#include "tree_helper.hpp"
#include "math_function.hpp"         // for resize_to_1_8_16_32_64_128_256_512


Bit_Value_opt::Bit_Value_opt (const ParameterConstRef _parameters, const application_managerRef _AppM, unsigned int _function_id, const DesignFlowManagerConstRef _design_flow_manager) :
   FunctionFrontendFlowStep (_AppM, _function_id, BIT_VALUE_OPT, _design_flow_manager, _parameters),
   modified(false),
   restart_dead_code(false)
{
   debug_level = parameters->get_class_debug_level (GET_CLASS(*this),
                                                    DEBUG_LEVEL_NONE);
}

Bit_Value_opt::~Bit_Value_opt ()
{
}

const std::unordered_set<std::pair<FrontendFlowStepType, FrontendFlowStep::FunctionRelationship> > Bit_Value_opt::ComputeFrontendRelationships (const DesignFlowStep::RelationshipType relationship_type) const
{
   std::unordered_set<std::pair<FrontendFlowStepType, FunctionRelationship> > relationships;
   switch (relationship_type)
   {
      case (PRECEDENCE_RELATIONSHIP):
      {
         relationships.insert(std::make_pair(EXTRACT_PATTERNS, SAME_FUNCTION));
         relationships.insert(std::make_pair(EXTRACT_GIMPLE_COND_OP, SAME_FUNCTION));
#if HAVE_ILP_BUILT && HAVE_BAMBU_BUILT
         relationships.insert(std::make_pair(SDC_CODE_MOTION, SAME_FUNCTION));
#endif
         break;
      }
      case DEPENDENCE_RELATIONSHIP:
      {
         relationships.insert(std::make_pair(BIT_VALUE, SAME_FUNCTION));
         if (parameters->isOption(OPT_bitvalue_ipa) and
               parameters->getOption<bool>(OPT_bitvalue_ipa))
         {
            relationships.insert (std::make_pair(BIT_VALUE_IPA, WHOLE_APPLICATION));
         }
         relationships.insert(std::make_pair(FUNCTION_CALL_TYPE_CLEANUP, SAME_FUNCTION));
         relationships.insert(std::make_pair(COMPLETE_CALL_GRAPH, WHOLE_APPLICATION));
         break;
      }
      case(INVALIDATION_RELATIONSHIP) :
      {
         switch(GetStatus())
         {
            case DesignFlowStep_Status::SUCCESS:
               {
                  if(restart_dead_code)
                     relationships.insert(std::make_pair(DEAD_CODE_ELIMINATION, SAME_FUNCTION));
                  break;
               }
            case DesignFlowStep_Status::SKIPPED:
            case DesignFlowStep_Status::UNCHANGED:
            case DesignFlowStep_Status::UNEXECUTED:
            case DesignFlowStep_Status::UNNECESSARY:
               {
                  break;
               }
            case DesignFlowStep_Status::ABORTED:
            case DesignFlowStep_Status::EMPTY:
            case DesignFlowStep_Status::NONEXISTENT:
            default:
               THROW_UNREACHABLE("");
         }
         break;
      }
      default:
         THROW_UNREACHABLE("");
   }
   return relationships;
}

static tree_nodeRef
create_ga(const tree_manipulationRef IRman, tree_nodeRef, tree_nodeRef &type, tree_nodeRef& op, unsigned int bb_index, const std::string & srcp_default)
{
   tree_nodeRef ssa_vd = IRman->create_ssa_name(tree_nodeRef(), type);
   return IRman->create_gimple_modify_stmt(ssa_vd, op, srcp_default, bb_index);
}


void Bit_Value_opt::optimize(statement_list* sl, tree_managerRef TM)
{
   for(auto bb_pair : sl->list_of_bloc)
   {
      blocRef B = bb_pair.second;
      unsigned int B_id = B->number;
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Examining BB" + STR(B_id));
      const auto list_of_stmt = B->CGetStmtList();
      for (const auto stmt : list_of_stmt)
      {
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "-->Examining statement " + GET_NODE(stmt)->ToString());
#ifndef NDEBUG
         if(not AppM->ApplyNewTransformation())
         {
            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Skipped because reached limit of cfg transformations");
            continue;
         }
#endif
         if(GET_NODE(stmt)->get_kind() == gimple_assign_K)
         {
            gimple_assign * ga =  GetPointer<gimple_assign>(GET_NODE(stmt));
            unsigned int output_uid = GET_INDEX_NODE(ga->op0);
            ssa_name *ssa = GetPointer<ssa_name> (GET_NODE(ga->op0));
            if (ssa)
            {
               if(tree_helper::is_real(TM, output_uid))
               {
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---real variables not considered: "+ STR(GET_INDEX_NODE(ga->op0)));
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Examined statement " + GET_NODE(stmt)->ToString());
                  continue;
               }
               if(tree_helper::is_a_complex(TM, output_uid))
               {
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---complex variables not considered: "+ STR(GET_INDEX_NODE(ga->op0)));
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Examined statement " + GET_NODE(stmt)->ToString());
                  continue;
               }
               if(tree_helper::is_a_vector(TM, output_uid))
               {
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---vector variables not considered: "+ STR(GET_INDEX_NODE(ga->op0)));
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Examined statement " + GET_NODE(stmt)->ToString());
                  continue;
               }
               if(GetPointer<integer_cst>(GET_NODE(ga->op1)) && tree_helper::is_a_pointer(TM, GET_INDEX_NODE(ga->op1)))
               {
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---constant pointer value assignements not considered: "+ STR(GET_INDEX_NODE(ga->op0)));
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Examined statement " + GET_NODE(stmt)->ToString());
                  continue;
               }
               if(GetPointer<call_expr>(GET_NODE(ga->op1)) and ga->vdef)
               {
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---calls with side effects cannot be optimized" + STR(GET_INDEX_NODE(ga->op1)));
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Examined statement " + GET_NODE(stmt)->ToString());
                  continue;
               }
               unsigned int type_index = tree_helper::get_type_index(TM, GET_INDEX_NODE(ga->op0));
               tree_nodeRef ga_op_type = TM->GetTreeReindex(type_index);
               tree_nodeRef Scpe = TM->GetTreeReindex(function_id);
               const std::string & bit_values = ssa->bit_values;
               bool is_constant=bit_values.size()!=0;
               for(auto current_el : bit_values)
               {
                  if(current_el == 'U')
                  {
                     is_constant = false;
                     break;
                  }
               }
               if(is_constant)
               {
                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level,"---Left part is constant");
                  unsigned long long int  const_value = 0;
                  unsigned int index_val = 0;
                  for(auto current_el : boost::adaptors::reverse(bit_values))
                  {
                     if(current_el == '1')
                        const_value |= 1ULL << index_val;
                     ++index_val;
                  }
                  ///in case do sign extension
                  if (tree_helper::is_int(TM, output_uid) && bit_values[0]=='1')
                  {
                     for(; index_val < 64;++index_val)
                        const_value |= 1ULL << index_val;
                  }
                  tree_nodeRef val;
                  if(GetPointer<integer_cst>(GET_NODE(ga->op1)))
                     val = ga->op1;
                  else
                     val = TM->CreateUniqueIntegerCst(static_cast<long long int>(const_value), type_index);
                  const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                  for(const auto use : StmtUses)
                  {
                     INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage before: "+ use.first->ToString());
                     TM->ReplaceTreeNode(use.first, ga->op0, val);
                     modified = true;
                     INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage after: "+ use.first->ToString());
                  }
                  if(GET_NODE(ga->op0)->get_kind() == ssa_name_K and ga->predicate)
                  {
                     ga->predicate = tree_nodeRef();
                  }
                  if(not StmtUses.empty())
                     restart_dead_code = true;
#ifndef NDEBUG
                  AppM->RegisterTransformation(GetName(), stmt);
#endif
               }
               else if(GetPointer<cst_node>(GET_NODE(ga->op1)))
               {
                  const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                  for(const auto use : StmtUses)
                  {
                     INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage before: "+ use.first->ToString());
                     TM->ReplaceTreeNode(use.first, ga->op0, ga->op1);
                     INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage after: "+ use.first->ToString());
                     modified = true;
                  }
                  if(not StmtUses.empty())
                     restart_dead_code = true;
#ifndef NDEBUG
                  AppM->RegisterTransformation(GetName(), stmt);
#endif

               }
               else if(GetPointer<ssa_name>(GET_NODE(ga->op1)))
               {
                  const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                  for(const auto use : StmtUses)
                  {
                     INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace ssa usage before: "+ use.first->ToString());
                     TM->ReplaceTreeNode(use.first, ga->op0, ga->op1);
                     INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace ssa usage after: "+ use.first->ToString());
                     modified = true;
                  }
                  if(not StmtUses.empty())
                     restart_dead_code = true;
#ifndef NDEBUG
                  AppM->RegisterTransformation(GetName(), stmt);
#endif
               }
               else if(GET_NODE(ga->op1)->get_kind() == mult_expr_K||GET_NODE(ga->op1)->get_kind() == widen_mult_expr_K)
               {
                  binary_expr * me = GetPointer<binary_expr>(GET_NODE(ga->op1));
                  tree_nodeRef op0 = GET_NODE(me->op0);
                  tree_nodeRef op1 = GET_NODE(me->op1);
                  /// first check if we have to change a mult_expr in a widen_mult_expr
                  unsigned int data_bitsize_out = resize_to_1_8_16_32_64_128_256_512(tree_helper::Size(GET_NODE(ga->op0)));
                  unsigned int data_bitsize_in0 = resize_to_1_8_16_32_64_128_256_512(tree_helper::Size(op0));
                  unsigned int data_bitsize_in1 = resize_to_1_8_16_32_64_128_256_512(tree_helper::Size(op1));
                  bool realp = tree_helper::is_real(TM, GET_INDEX_NODE(GetPointer<binary_expr>(GET_NODE(ga->op1))->type));
                  if(GET_NODE(ga->op1)->get_kind() == mult_expr_K && !realp && std::max(data_bitsize_in0, data_bitsize_in1)*2== data_bitsize_out)
                  {
                     tree_nodeRef op0_type= TM->GetTreeReindex(type_index);
                     const std::string srcp_default = ga->include_name + ":" + STR(ga->line_number) + ":" + STR(ga->column_number);
                     tree_nodeRef new_widen_expr = IRman->create_binary_operation(op0_type, GetPointer<binary_expr>(GET_NODE(ga->op1))->op0, GetPointer<binary_expr>(GET_NODE(ga->op1))->op1, srcp_default, widen_mult_expr_K);
                     INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Replacing " + STR(ga->op1) + " with " + STR(new_widen_expr) + " in " + STR(stmt));
                     modified = true;
                     TM->ReplaceTreeNode(stmt, ga->op1, new_widen_expr);
                     INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace expression with a widen mult_expr: "+ stmt->ToString());
                  }
                  else if(GET_NODE(ga->op1)->get_kind() == widen_mult_expr_K && !realp && std::max(data_bitsize_in0, data_bitsize_in1)== data_bitsize_out)
                  {
                     tree_nodeRef op0_type= TM->GetTreeReindex(type_index);
                     const std::string srcp_default = ga->include_name + ":" + STR(ga->line_number) + ":" + STR(ga->column_number);
                     tree_nodeRef new_expr = IRman->create_binary_operation(op0_type, GetPointer<binary_expr>(GET_NODE(ga->op1))->op0, GetPointer<binary_expr>(GET_NODE(ga->op1))->op1, srcp_default, mult_expr_K);
                     INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Replacing " + STR(ga->op1) + " with " + STR(new_expr) + " in " + STR(stmt));
                     modified = true;
                     TM->ReplaceTreeNode(stmt, ga->op1, new_expr);
                     INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace expression with a mult_expr: "+ stmt->ToString());
                  }
                  unsigned int trailing_zero_op0 = 0;
                  unsigned int trailing_zero_op1 = 0;
                  if(GetPointer<ssa_name>(op0))
                  {
                     const std::string & bit_values_op0 = GetPointer<ssa_name>(op0)->bit_values;
                     for(auto current_el : boost::adaptors::reverse(bit_values_op0))
                     {
                        if(current_el == '0' || current_el == 'X')
                           ++trailing_zero_op0;
                        else
                           break;
                     }
                  }
                  else if(GetPointer<integer_cst>(op0))
                  {
                     integer_cst *int_const= GetPointer<integer_cst>(op0);
                     unsigned long long int value_int = static_cast<unsigned long long int>(int_const->value);
                     for(unsigned int index=0; index<64 && value_int != 0;++index)
                     {
                        if(value_int&(1ULL<<index))
                           break;
                        else
                           ++trailing_zero_op0;
                     }
                  }
                  if(GetPointer<ssa_name>(op1))
                  {
                     const std::string & bit_values_op1 = GetPointer<ssa_name>(op1)->bit_values;
                     for(auto current_el : boost::adaptors::reverse(bit_values_op1))
                     {
                        if(current_el == '0' || current_el == 'X')
                           ++trailing_zero_op1;
                        else
                           break;
                     }
                  }
                  else if(GetPointer<integer_cst>(op1))
                  {
                     integer_cst *int_const= GetPointer<integer_cst>(op1);
                     unsigned long long int value_int = static_cast<unsigned long long int>(int_const->value);
                     for(unsigned int index=0; index<64 && value_int != 0;++index)
                     {
                        if(value_int&(1ULL<<index))
                           break;
                        else
                           ++trailing_zero_op1;
                     }
                  }
                  if(trailing_zero_op0 != 0 || trailing_zero_op1 != 0)
                  {
                     modified = true;
#ifndef NDEBUG
                     AppM->RegisterTransformation(GetName(), stmt);
#endif
                     INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "-->Bit Value Opt: mult_expr/widen_mult_expr optimized, nbits = " + STR(trailing_zero_op0+trailing_zero_op1));
                     INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "<--");
                     const std::string srcp_default = ga->include_name + ":" + STR(ga->line_number) + ":" + STR(ga->column_number);
                     if(trailing_zero_op0 != 0)
                     {
                        const unsigned int op0_type_id = tree_helper::get_type_index(TM, GET_INDEX_NODE(me->op0));
                        tree_nodeRef op0_type = TM->CGetTreeReindex(op0_type_id);
                        tree_nodeRef op0_const_node = TM->CreateUniqueIntegerCst(static_cast<long long int>(trailing_zero_op0), op0_type_id);
                        tree_nodeRef op0_expr = IRman->create_binary_operation(op0_type, me->op0, op0_const_node, srcp_default, rshift_expr_K);
                        tree_nodeRef op0_ga = create_ga(IRman, Scpe, op0_type, op0_expr, B_id, srcp_default);
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(op0_ga));
                        B->PushBefore(op0_ga, stmt);
                        tree_nodeRef op0_ga_var = GetPointer<gimple_assign>(GET_NODE(op0_ga))->op0;
                        TM->ReplaceTreeNode(stmt, me->op0, op0_ga_var);
                        ///set the bit_values to the ssa var
                        if(GetPointer<ssa_name>(op0))
                        {
                           ssa_name* op0_ssa = GetPointer<ssa_name>(GET_NODE(op0_ga_var));
                           op0_ssa->bit_values = GetPointer<ssa_name>(op0)->bit_values.substr(0, GetPointer<ssa_name>(op0)->bit_values.size()-trailing_zero_op0);
                        }
                     }
                     if(trailing_zero_op1 != 0 and op0->index != op1->index)
                     {
                        const unsigned int op1_type_id = tree_helper::get_type_index(TM, GET_INDEX_NODE(me->op1));
                        tree_nodeRef op1_type = TM->CGetTreeReindex(op1_type_id);
                        tree_nodeRef op1_const_node = TM->CreateUniqueIntegerCst(static_cast<long long int>(trailing_zero_op1), op1_type_id);
                        tree_nodeRef op1_expr = IRman->create_binary_operation(op1_type, me->op1, op1_const_node, srcp_default, rshift_expr_K);
                        tree_nodeRef op1_ga = create_ga(IRman, Scpe, op1_type, op1_expr, B_id, srcp_default);
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(op1_ga));
                        B->PushBefore(op1_ga, stmt);
                        tree_nodeRef op1_ga_var = GetPointer<gimple_assign>(GET_NODE(op1_ga))->op0;
                        TM->ReplaceTreeNode(stmt, me->op1, op1_ga_var);
                        ///set the bit_values to the ssa var
                        if( GetPointer<ssa_name>(op1))
                        {
                           ssa_name* op1_ssa = GetPointer<ssa_name>(GET_NODE(op1_ga_var));
                           op1_ssa->bit_values = GetPointer<ssa_name>(op1)->bit_values.substr(0, GetPointer<ssa_name>(op1)->bit_values.size()-trailing_zero_op1);
                        }
                     }

                     tree_nodeRef ssa_vd = IRman->create_ssa_name(tree_nodeRef(), ga_op_type);
                     ssa_name * sn = GetPointer<ssa_name> (GET_NODE(ssa_vd));
                     ///set the bit_values to the ssa var
                     sn->bit_values = ssa->bit_values.substr(0, ssa->bit_values.size()-trailing_zero_op0-trailing_zero_op1);
                     tree_nodeRef op_const_node = TM->CreateUniqueIntegerCst(static_cast<long long int>(trailing_zero_op0+trailing_zero_op1), type_index);
                     tree_nodeRef op_expr = IRman->create_binary_operation(ga_op_type, ssa_vd, op_const_node, srcp_default, lshift_expr_K);
                     tree_nodeRef curr_ga = create_ga(IRman, Scpe, ga_op_type, op_expr, B_id, srcp_default);
                     INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(curr_ga));
                     TM->ReplaceTreeNode(curr_ga, GetPointer<gimple_assign>(GET_NODE(curr_ga))->op0, ga->op0);
                     TM->ReplaceTreeNode(stmt, ga->op0, ssa_vd);
                     B->PushAfter(curr_ga, stmt);
                     INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "pushed");
                  }
               }
               else if(GET_NODE(ga->op1)->get_kind() == plus_expr_K || GET_NODE(ga->op1)->get_kind() == minus_expr_K)
               {
                  binary_expr * me = GetPointer<binary_expr>(GET_NODE(ga->op1));
                  if(me->op0->index == me->op1->index)
                  {
                     INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Skipped plus expr with same operands " + GET_NODE(stmt)->ToString());
                     continue;
                  }
                  tree_nodeRef op0 = GET_NODE(me->op0);
                  tree_nodeRef op1 = GET_NODE(me->op1);
                  PRINT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "Var_uid: "+ AppM->CGetFunctionBehavior(function_id)->CGetBehavioralHelper()->PrintVariable(output_uid) +" bitstring: " + bit_values);
                  unsigned int trailing_zero_op0 = 0;
                  unsigned int trailing_zero_op1 = 0;
                  bool is_op0_null = false;
                  bool is_op1_null = false;

                  if(GetPointer<ssa_name>(op0) && GET_NODE(ga->op1)->get_kind() == plus_expr_K)
                  {
                     const std::string & bit_values_op0 = GetPointer<ssa_name>(op0)->bit_values;
                     PRINT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "Var_uid: "+ AppM->CGetFunctionBehavior(function_id)->CGetBehavioralHelper()->PrintVariable(GET_INDEX_NODE(me->op0)) +" bitstring: " + bit_values_op0);
                     for(auto current_el : boost::adaptors::reverse(bit_values_op0))
                     {
                        if(current_el == '0' || current_el == 'X')
                           ++trailing_zero_op0;
                        else
                           break;
                     }
                     if(bit_values_op0=="0")
                        is_op0_null = true;
                  }
                  else if(GetPointer<integer_cst>(op0) && GET_NODE(ga->op1)->get_kind() == plus_expr_K)
                  {
                     integer_cst *int_const= GetPointer<integer_cst>(op0);
                     unsigned long long int value_int = static_cast<unsigned long long int>(int_const->value);
                     for(unsigned int index=0; index<64 && value_int != 0;++index)
                     {
                        if(value_int&(1ULL<<index))
                           break;
                        else
                           ++trailing_zero_op0;
                     }
                     if(int_const->value==0)
                        is_op0_null = true;

                  }

                  if(GetPointer<ssa_name>(op1))
                  {
                     const std::string & bit_values_op1 = GetPointer<ssa_name>(op1)->bit_values;
                     PRINT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "Var_uid: "+ AppM->CGetFunctionBehavior(function_id)->CGetBehavioralHelper()->PrintVariable(GET_INDEX_NODE(me->op1)) +" bitstring: " + bit_values_op1);
                     for(auto current_el : boost::adaptors::reverse(bit_values_op1))
                     {
                        if(current_el == '0' || current_el == 'X')
                           ++trailing_zero_op1;
                        else
                           break;
                     }
                     if(bit_values_op1=="0")
                        is_op1_null = true;
                  }
                  else if(GetPointer<integer_cst>(op1))
                  {
                     integer_cst *int_const= GetPointer<integer_cst>(op1);
                     unsigned long long int value_int = static_cast<unsigned long long int>(int_const->value);
                     for(unsigned int index=0; index<64 && value_int != 0;++index)
                     {
                        if(value_int&(1ULL<<index))
                           break;
                        else
                           ++trailing_zero_op1;
                     }
                     if(int_const->value==0)
                        is_op1_null = true;
                  }

                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Trailing zeros op0=" + STR(trailing_zero_op0) + ", trailing zeros op1=" + STR(trailing_zero_op1));
                  if(is_op0_null)
                  {
                     const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                     for(const auto use : StmtUses)
                     {
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage before: "+ use.first->ToString());
                        TM->ReplaceTreeNode(use.first, ga->op0, me->op1);
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage after: "+ use.first->ToString());
                        modified = true;
                     }
                     if(not StmtUses.empty())
                        restart_dead_code = true;
#ifndef NDEBUG
                     AppM->RegisterTransformation(GetName(), stmt);
#endif
                  }
                  else if(is_op1_null)
                  {
                     const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                     for(const auto use : StmtUses)
                     {
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage before: "+ use.first->ToString());
                        TM->ReplaceTreeNode(use.first, ga->op0, me->op0);
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage after: "+ use.first->ToString());
                        modified = true;
                     }
                     if(not StmtUses.empty())
                        restart_dead_code = true;
#ifndef NDEBUG
                     AppM->RegisterTransformation(GetName(), stmt);
#endif
                  }
                  else if(trailing_zero_op0 != 0 || trailing_zero_op1 != 0)
                  {
                     modified = true;
#ifndef NDEBUG
                     AppM->RegisterTransformation(GetName(), stmt);
#endif
                     const std::string srcp_default = ga->include_name + ":" + STR(ga->line_number) + ":" + STR(ga->column_number);
                     const bool is_first_max = trailing_zero_op0 > trailing_zero_op1;
                     const unsigned int shift_const = is_first_max ? trailing_zero_op0 : trailing_zero_op1;
                     INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "---Bit Value Opt: " + (GET_NODE(ga->op1)->get_kind() == plus_expr_K?std::string("plus_expr"):std::string("minus_expr")) + " optimized, nbits = " + STR(shift_const));
                     const tree_nodeRef shift_constant_node = TM->CreateUniqueIntegerCst(static_cast<long long int>(shift_const), type_index);
                     bool is_op0_const = GetPointer<integer_cst>(op0);
                     bool is_op1_const = GetPointer<integer_cst>(op1);
                     const unsigned int op0_type_id = tree_helper::get_type_index(TM, GET_INDEX_NODE(me->op0));
                     tree_nodeRef op0_type = TM->GetTreeReindex(op0_type_id);
                     const unsigned int op1_type_id = tree_helper::get_type_index(TM, GET_INDEX_NODE(me->op1));
                     tree_nodeRef op1_type = TM->GetTreeReindex(op1_type_id);
                     tree_nodeRef b_node = is_first_max ? me->op1 : me->op0;
                     unsigned int b_type_id = is_first_max ? op1_type_id : op0_type_id;
                     tree_nodeRef b_type = TM->GetTreeReindex(b_type_id);

                     if(is_op0_const)
                     {
                        integer_cst *int_const= GetPointer<integer_cst>(op0);
                        if(tree_helper::is_int(TM, GET_INDEX_NODE(me->op0)))
                        {
                           if(static_cast<long long int>(int_const->value>>shift_const) == 0)
                              is_op0_null = GET_NODE(ga->op1)->get_kind() == plus_expr_K; // TODO: true?
                           TM->ReplaceTreeNode(stmt, me->op0, TM->CreateUniqueIntegerCst(static_cast<long long int>(int_const->value>>shift_const), op0_type_id));
                        }
                        else
                        {
                           if(static_cast<unsigned long long int>(int_const->value>>shift_const) == 0)
                              is_op0_null = GET_NODE(ga->op1)->get_kind() == plus_expr_K; // TODO: true?
                           TM->ReplaceTreeNode(stmt, me->op0, TM->CreateUniqueIntegerCst(static_cast<long long int>(static_cast<unsigned long long int>(int_const->value)>>shift_const), op0_type_id));
                        }
                     }
                     else
                     {
                        std::string resulting_bit_values;
                        THROW_ASSERT(GetPointer<ssa_name>(op0), "expected an SSA name");

                        if((GetPointer<ssa_name>(op0)->bit_values.size() - shift_const) > 0)
                           resulting_bit_values = GetPointer<ssa_name>(op0)->bit_values.substr(0, GetPointer<ssa_name>(op0)->bit_values.size() - shift_const);
                        else if(tree_helper::is_int(TM, GET_INDEX_NODE(me->op0)))
                           resulting_bit_values = GetPointer<ssa_name>(op0)->bit_values.substr(0, 1);
                        else
                           resulting_bit_values = "0";

                        if(resulting_bit_values == "0" && GET_NODE(ga->op1)->get_kind() == plus_expr_K)
                        {
                           is_op0_null = true;
                        }
                        else
                        {
                           tree_nodeRef op0_expr = IRman->create_binary_operation(op0_type, me->op0, shift_constant_node, srcp_default, rshift_expr_K);
                           tree_nodeRef op0_ga = create_ga(IRman, Scpe, op0_type, op0_expr, B_id, srcp_default);
                           INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(op0_ga));
                           B->PushBefore(op0_ga, stmt);
                           tree_nodeRef op0_ga_var = GetPointer<gimple_assign>(GET_NODE(op0_ga))->op0;
                           INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Replacing " + me->op0->ToString() + " with " + op0_ga_var->ToString() + " in " + stmt->ToString());
                           TM->ReplaceTreeNode(stmt, me->op0, op0_ga_var);
#if HAVE_FROM_DISCREPANCY_BUILT
                           /*
                            * for discrepancy analysis, the ssa assigned by this
                            * bitshift must not be checked if it was applied to
                            * a variable marked as address.
                            */
                           if (parameters->isOption(OPT_discrepancy) and
                                 parameters->getOption<bool>(OPT_discrepancy))
                           {
                              AppM->RDiscr->ssa_to_skip_if_address.insert(GET_NODE(op0_ga_var));
                           }
#endif
                           ///set the bit_values to the ssa var
                           ssa_name* op0_ssa = GetPointer<ssa_name>(GET_NODE(op0_ga_var));
                           op0_ssa->bit_values = resulting_bit_values;
                           PRINT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "Var_uid: "+ AppM->CGetFunctionBehavior(function_id)->CGetBehavioralHelper()->PrintVariable(GET_INDEX_NODE(op0_ga_var)) +" bitstring: " + STR(op0_ssa->bit_values));
                        }
                     }

                     if(is_op1_const)
                     {
                        integer_cst *int_const= GetPointer<integer_cst>(op1);
                        if(tree_helper::is_int(TM, GET_INDEX_NODE(me->op1)))
                        {
                           if(static_cast<long long int>(int_const->value>>shift_const) == 0)
                              is_op1_null = true;
                           TM->ReplaceTreeNode(stmt, me->op1, TM->CreateUniqueIntegerCst(static_cast<long long int>(int_const->value>>shift_const), op1_type_id));
                        }
                        else
                        {
                           if(static_cast<unsigned long long int>(int_const->value>>shift_const) == 0)
                              is_op1_null = true;
                           TM->ReplaceTreeNode(stmt, me->op1, TM->CreateUniqueIntegerCst(static_cast<long long int>(static_cast<unsigned long long int>(int_const->value)>>shift_const), op1_type_id));
                        }
                     }
                     else
                     {
                        std::string resulting_bit_values;
                        THROW_ASSERT(GetPointer<ssa_name>(op1), "expected an SSA name");

                        if((GetPointer<ssa_name>(op1)->bit_values.size() - shift_const) > 0)
                           resulting_bit_values = GetPointer<ssa_name>(op1)->bit_values.substr(0, GetPointer<ssa_name>(op1)->bit_values.size() - shift_const);
                        else if(tree_helper::is_int(TM, GET_INDEX_NODE(me->op1)))
                           resulting_bit_values = GetPointer<ssa_name>(op1)->bit_values.substr(0, 1);
                        else
                           resulting_bit_values = "0";

                        if(resulting_bit_values == "0")
                        {
                           is_op1_null = true;
                        }
                        else
                        {
                           tree_nodeRef op1_expr = IRman->create_binary_operation(op1_type, me->op1, shift_constant_node, srcp_default, rshift_expr_K);
                           tree_nodeRef op1_ga = create_ga(IRman, Scpe, op1_type, op1_expr, B_id, srcp_default);
                           INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(op1_ga));
                           B->PushBefore(op1_ga, stmt);
                           tree_nodeRef op1_ga_var = GetPointer<gimple_assign>(GET_NODE(op1_ga))->op0;
                           INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Replacing " + me->op1->ToString() + " with " + op1_ga_var->ToString() + " in " + stmt->ToString());
                           TM->ReplaceTreeNode(stmt, me->op1, op1_ga_var);
#if HAVE_FROM_DISCREPANCY_BUILT
                           /*
                            * for discrepancy analysis, the ssa assigned by this
                            * bitshift must not be checked if it was applied to
                            * a variable marked as address.
                            */
                           if (parameters->isOption(OPT_discrepancy) and
                                 parameters->getOption<bool>(OPT_discrepancy))
                           {
                              AppM->RDiscr->ssa_to_skip_if_address.insert(GET_NODE(op1_ga_var));
                           }
#endif
                           ///set the bit_values to the ssa var
                           ssa_name* op1_ssa = GetPointer<ssa_name>(GET_NODE(op1_ga_var));
                           op1_ssa->bit_values = resulting_bit_values;
                           PRINT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "Var_uid: "+ AppM->CGetFunctionBehavior(function_id)->CGetBehavioralHelper()->PrintVariable(GET_INDEX_NODE(op1_ga_var)) +" bitstring: " + STR(op1_ssa->bit_values));
                        }
                     }

                     tree_nodeRef curr_ga;
                     if(is_op0_null)
                     {
                        curr_ga = create_ga(IRman, Scpe, op1_type, me->op1, B_id, srcp_default);
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(curr_ga));
                     }
                     else if(is_op1_null)
                     {
                        curr_ga = create_ga(IRman, Scpe, op0_type, me->op0, B_id, srcp_default);
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(curr_ga));
                     }
                     else
                     {
                        curr_ga = create_ga(IRman, Scpe, ga_op_type, ga->op1, B_id, srcp_default);
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(curr_ga));
                     }
                     B->PushBefore(curr_ga, stmt);
                     GetPointer<gimple_assign>(GET_NODE(curr_ga))->orig = stmt;
                     tree_nodeRef curr_ga_var = GetPointer<gimple_assign>(GET_NODE(curr_ga))->op0;
#if HAVE_FROM_DISCREPANCY_BUILT
                     /*
                      * for discrepancy analysis, the ssa assigned by this
                      * bitshift must not be checked if it was applied to
                      * a variable marked as address.
                      */
                     if (parameters->isOption(OPT_discrepancy) and
                           parameters->getOption<bool>(OPT_discrepancy))
                     {
                        AppM->RDiscr->ssa_to_skip_if_address.insert(GET_NODE(curr_ga_var));
                     }
#endif
                     ///set the bit_values to the ssa var
                     ssa_name * op_ssa = GetPointer<ssa_name>(GET_NODE(curr_ga_var));
                     op_ssa->bit_values = ssa->bit_values.substr(0, ssa->bit_values.size() - shift_const);
                     PRINT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "Var_uid: "+ AppM->CGetFunctionBehavior(function_id)->CGetBehavioralHelper()->PrintVariable(GET_INDEX_NODE(curr_ga_var)) +" bitstring: " + STR(op_ssa->bit_values));

                     tree_nodeRef op_expr = IRman->create_binary_operation(ga_op_type, curr_ga_var, shift_constant_node, srcp_default, lshift_expr_K);
                     tree_nodeRef lshift_ga = create_ga(IRman, Scpe, ga_op_type, op_expr, B_id, srcp_default);
                     INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(lshift_ga));
                     B->PushBefore(lshift_ga, stmt);
                     tree_nodeRef lshift_ga_var = GetPointer<gimple_assign>(GET_NODE(lshift_ga))->op0;
                     ///set the bit_values to the ssa var
                     ssa_name * lshift_ssa = GetPointer<ssa_name>(GET_NODE(lshift_ga_var));
                     lshift_ssa->bit_values = ssa->bit_values.substr(0, ssa->bit_values.size() - shift_const);
                     while (lshift_ssa->bit_values.size() < ssa->bit_values.size())
                        lshift_ssa->bit_values.push_back('0');
                     PRINT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "Var_uid: "+ AppM->CGetFunctionBehavior(function_id)->CGetBehavioralHelper()->PrintVariable(GET_INDEX_NODE(lshift_ga_var)) +" bitstring: " + STR(lshift_ssa->bit_values));

                     bool do_final_or = false;
                     unsigned int n_iter = 0;
                     for(auto cur_bit : boost::adaptors::reverse(ssa->bit_values))
                     {
                        if(cur_bit == '1' || cur_bit == 'U')
                        {
                           do_final_or = true;
                           break;
                        }
                        n_iter++;
                        if(n_iter==shift_const)
                           break;
                     }

                     if(do_final_or)
                     {
#if HAVE_FROM_DISCREPANCY_BUILT
                        /*
                         * for discrepancy analysis, the ssa assigned by this
                         * bitshift must not be checked if it was applied to
                         * a variable marked as address.
                         */
                        if (parameters->isOption(OPT_discrepancy) and
                              parameters->getOption<bool>(OPT_discrepancy))
                        {
                           AppM->RDiscr->ssa_to_skip_if_address.insert(GET_NODE(lshift_ga_var));
                        }
#endif
                        if(GetPointer<integer_cst>(GET_NODE(b_node)))
                        {
                           integer_cst *int_const= GetPointer<integer_cst>(GET_NODE(b_node));
                           tree_nodeRef b_node_val = TM->CreateUniqueIntegerCst(static_cast<long long int>(static_cast<unsigned long long int>(int_const->value)&((1ULL<<shift_const)-1)), b_type_id);
                           TM->ReplaceTreeNode(stmt, ga->op1, IRman->create_ternary_operation(ga_op_type, lshift_ga_var, b_node_val, shift_constant_node, srcp_default, bit_ior_concat_expr_K));
                        }
                        else
                        {
                           tree_nodeRef bit_mask_constant_node = TM->CreateUniqueIntegerCst(static_cast<long long int>((1ULL<<shift_const)-1), b_type_id);
                           tree_nodeRef band_expr = IRman->create_binary_operation(b_type, b_node, bit_mask_constant_node, srcp_default, bit_and_expr_K);
                           tree_nodeRef band_ga = create_ga(IRman, Scpe, b_type, band_expr, B_id, srcp_default);
                           INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(band_ga));
                           B->PushBefore(band_ga, stmt);
                           tree_nodeRef band_ga_var = GetPointer<gimple_assign>(GET_NODE(band_ga))->op0;
#if HAVE_FROM_DISCREPANCY_BUILT
                           /*
                            * for discrepancy analysis, the ssa assigned by this
                            * bitshift must not be checked if it was applied to
                            * a variable marked as address.
                            */
                           if (parameters->isOption(OPT_discrepancy) and
                                 parameters->getOption<bool>(OPT_discrepancy))
                           {
                              AppM->RDiscr->ssa_to_skip_if_address.insert(GET_NODE(band_ga_var));
                           }
#endif
                           ///set the bit_values to the ssa var
                           ssa_name * band_ssa = GetPointer<ssa_name>(GET_NODE(band_ga_var));
                           for(auto cur_bit : boost::adaptors::reverse(ssa->bit_values))
                           {
                              band_ssa->bit_values = cur_bit + band_ssa->bit_values;
                              if(band_ssa->bit_values.size() == shift_const)
                                 break;
                           }
                           band_ssa->bit_values = "0" + band_ssa->bit_values;
                           PRINT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "Var_uid: "+ AppM->CGetFunctionBehavior(function_id)->CGetBehavioralHelper()->PrintVariable(GET_INDEX_NODE(band_ga_var)) +" bitstring: " + STR(band_ssa->bit_values));

                           tree_nodeRef res_expr = IRman->create_ternary_operation(ga_op_type, lshift_ga_var, band_ga_var, shift_constant_node, srcp_default, bit_ior_concat_expr_K);
                           TM->ReplaceTreeNode(stmt, ga->op1, res_expr);
                        }
                     }
                     else
                     {
                        PRINT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, "Final or not performed: ");
                        TM->ReplaceTreeNode(stmt, ga->op1, lshift_ga_var);
                     }
                     ///set uses of stmt
                  }
               }
               else if(GET_NODE(ga->op1)->get_kind() == eq_expr_K || GET_NODE(ga->op1)->get_kind() == ne_expr_K)
               {
                   binary_expr * me = GetPointer<binary_expr>(GET_NODE(ga->op1));
                   tree_nodeRef op0 = GET_NODE(me->op0);
                   tree_nodeRef op1 = GET_NODE(me->op1);
                   unsigned int op0_size = tree_helper::size(TM, GET_INDEX_NODE(me->op0));
                   bool is_op1_zero = false;
                   if(GetPointer<integer_cst>(op1))
                   {
                      integer_cst *ic = GetPointer<integer_cst>(op1);
                      unsigned long long int ull_value = static_cast<unsigned long long int>(tree_helper::get_integer_cst_value(ic));
                      if(ull_value==0)
                         is_op1_zero = true;
                   }

                   if(op0 == op1)
                   {
                      long long int  const_value = GET_NODE(ga->op1)->get_kind() == eq_expr_K ? 1LL : 0LL;
                      tree_nodeRef val = TM->CreateUniqueIntegerCst(const_value, type_index);
                      const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                      for(const auto use : StmtUses)
                      {
                         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage before: "+ use.first->ToString());
                         TM->ReplaceTreeNode(use.first, ga->op0, val);
                         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage after: "+ use.first->ToString());
                         modified = true;
                      }
                      if(not StmtUses.empty())
                         restart_dead_code = true;
#ifndef NDEBUG
                      AppM->RegisterTransformation(GetName(), stmt);
#endif

                   }
                   else if(is_op1_zero && GET_NODE(ga->op1)->get_kind() == ne_expr_K && op0_size == 1)
                   {
                      unsigned int op0_type_index = tree_helper::get_type_index(TM,GET_INDEX_NODE(me->op0));
                      tree_nodeRef op0_op_type = TM->GetTreeReindex(op0_type_index);
                      unsigned data_bitsize = tree_helper::size(TM, op0_type_index);
                      if(data_bitsize==1)
                      {
                         const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                         for(const auto use : StmtUses)
                         {
                            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage before: "+ use.first->ToString());
                            if(GET_CONST_NODE(use.first)->get_kind() != gimple_cond_K)
                            {
                               TM->ReplaceTreeNode(use.first, ga->op0, me->op0);
                               modified = true;
                            }
                            INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage after: "+ use.first->ToString());
                         }
                         if(ssa->CGetUseStmts().size())
                         {
                            const std::string srcp_default = ga->include_name + ":" + STR(ga->line_number) + ":" + STR(ga->column_number);
                            ga->op1 = IRman->create_unary_operation(ga_op_type, me->op0, srcp_default, nop_expr_K);
                         }
                         else
                         {
                            restart_dead_code = true;
                         }

#ifndef NDEBUG
                         AppM->RegisterTransformation(GetName(), stmt);
#endif
                      }
                      else
                      {
                         const std::string srcp_default = ga->include_name + ":" + STR(ga->line_number) + ":" + STR(ga->column_number);
                         tree_nodeRef one_const_node = TM->CreateUniqueIntegerCst(1, op0_type_index);
                         tree_nodeRef bitwise_masked = IRman->create_binary_operation(op0_op_type, me->op0, one_const_node, srcp_default, bit_and_expr_K);
                         tree_nodeRef op0_ga = create_ga(IRman, Scpe, op0_op_type, bitwise_masked, B_id, srcp_default);
                         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(op0_ga));
                         B->PushBefore(op0_ga, stmt);
                         tree_nodeRef op0_ga_var = GetPointer<gimple_assign>(GET_NODE(op0_ga))->op0;

                         const tree_nodeConstRef type_node = tree_helper::CGetType(GET_NODE(ga->op0));
                         const auto type_id = type_node->index;
                         tree_nodeRef ga_nop = IRman->CreateNopExpr(op0_ga_var, TM->CGetTreeReindex(type_id));
                         B->PushBefore(ga_nop, stmt);
                         modified = true;
#ifndef NDEBUG
                         AppM->RegisterTransformation(GetName(), ga_nop);
#endif
                         tree_nodeRef nop_ga_var = GetPointer<gimple_assign>(GET_NODE(ga_nop))->op0;
                         TM->ReplaceTreeNode(stmt, ga->op1, nop_ga_var);
                         restart_dead_code = true;
                      }
                   }
                   else
                   {
                      std::string s0, s1;
                      if(GetPointer<ssa_name>(op0))
                         s0 = GetPointer<ssa_name>(op0)->bit_values;
                      if(GetPointer<ssa_name>(op1))
                         s1 = GetPointer<ssa_name>(op1)->bit_values;
                      unsigned int precision;
                      if(s0.size() && s1.size())
                         precision = static_cast<unsigned int>(std::min(s0.size(), s1.size()));
                      else
                         precision = static_cast<unsigned int>(std::max(s0.size(), s1.size()));

                      if(precision)
                      {
                         unsigned int trailing_eq = 0;
                         if(GetPointer<integer_cst>(op0))
                         {
                            integer_cst *ic = GetPointer<integer_cst>(op0);
                            unsigned long long int ull_value = static_cast<unsigned long long int>(tree_helper::get_integer_cst_value(ic));
                            s0 = convert_to_binary(ull_value, precision);
                         }
                         if(GetPointer<integer_cst>(op1))
                         {
                            integer_cst *ic = GetPointer<integer_cst>(op1);
                            unsigned long long int ull_value = static_cast<unsigned long long int>(tree_helper::get_integer_cst_value(ic));
                            s1 = convert_to_binary(ull_value, precision);
                         }
                         precision = static_cast<unsigned int>(std::min(s0.size(), s1.size()));
                         if(precision==0)
                            precision = 1;
                         for(unsigned int index=0; index < (precision-1); ++index)
                         {
                            if(s0[precision-index-1] == s1[precision-index-1] && (s0[precision-index-1] == '0' || s0[precision-index-1] == '1'))
                               ++trailing_eq;
                            else
                               break;
                         }
                         if(trailing_eq)
                         {
                            INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "-->Bit Value Opt: " + (GET_NODE(ga->op1)->get_kind() == eq_expr_K?std::string("eq_expr"):std::string("ne_expr")) + " optimized, nbits = " + STR(trailing_eq));
                            INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "<--");
                            modified = true;
#ifndef NDEBUG
                            AppM->RegisterTransformation(GetName(), stmt);
#endif
                            const std::string srcp_default = ga->include_name + ":" + STR(ga->line_number) + ":" + STR(ga->column_number);
                            unsigned int type_index0 = tree_helper::get_type_index(TM, GET_INDEX_NODE(me->op0));
                            tree_nodeRef op0_op_type = TM->GetTreeReindex(type_index0);
                            unsigned int type_index1 = tree_helper::get_type_index(TM, GET_INDEX_NODE(me->op1));
                            tree_nodeRef op1_op_type = TM->GetTreeReindex(type_index1);

                            if(GetPointer<ssa_name>(op0))
                            {
                               tree_nodeRef op0_const_node = TM->CreateUniqueIntegerCst(static_cast<long long int>(trailing_eq), type_index0);
                               tree_nodeRef op0_expr = IRman->create_binary_operation(op0_op_type, me->op0, op0_const_node, srcp_default, rshift_expr_K);
                               tree_nodeRef op0_ga = create_ga(IRman, Scpe, op0_op_type, op0_expr, B_id, srcp_default);
                               INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(op0_ga));
                               B->PushBefore(op0_ga, stmt);
                               tree_nodeRef op0_ga_var = GetPointer<gimple_assign>(GET_NODE(op0_ga))->op0;
                               TM->ReplaceTreeNode(stmt, me->op0, op0_ga_var);
                               ///set the bit_values to the ssa var
                               ssa_name* op0_ssa = GetPointer<ssa_name>(GET_NODE(op0_ga_var));
                               op0_ssa->bit_values = GetPointer<ssa_name>(op0)->bit_values.substr(0, GetPointer<ssa_name>(op0)->bit_values.size()-trailing_eq);
                            }
                            else
                            {
                               integer_cst *int_const= GetPointer<integer_cst>(op0);
                               if(tree_helper::is_int(TM, GET_INDEX_NODE(me->op0)))
                                  TM->ReplaceTreeNode(stmt, me->op0, TM->CreateUniqueIntegerCst(static_cast<long long int>(int_const->value>>trailing_eq), type_index0));
                               else
                                  TM->ReplaceTreeNode(stmt, me->op0, TM->CreateUniqueIntegerCst(static_cast<long long int>(static_cast<unsigned long long int>(int_const->value)>>trailing_eq), type_index0));
                            }
                            if(GetPointer<ssa_name>(op1))
                            {
                               tree_nodeRef op1_const_node = TM->CreateUniqueIntegerCst(static_cast<long long int>(trailing_eq), type_index1);
                               tree_nodeRef op1_expr = IRman->create_binary_operation(op1_op_type, me->op1, op1_const_node, srcp_default, rshift_expr_K);
                               tree_nodeRef op1_ga = create_ga(IRman, Scpe, op1_op_type, op1_expr, B_id, srcp_default);
                               INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(op1_ga));
                               B->PushBefore(op1_ga, stmt);
                               tree_nodeRef op1_ga_var = GetPointer<gimple_assign>(GET_NODE(op1_ga))->op0;
                               TM->ReplaceTreeNode(stmt, me->op1, op1_ga_var);
                               ///set the bit_values to the ssa var
                               ssa_name* op1_ssa = GetPointer<ssa_name>(GET_NODE(op1_ga_var));
                               op1_ssa->bit_values = GetPointer<ssa_name>(op1)->bit_values.substr(0, GetPointer<ssa_name>(op1)->bit_values.size()-trailing_eq);
                            }
                            else
                            {
                               integer_cst *int_const= GetPointer<integer_cst>(op1);
                               if(tree_helper::is_int(TM, GET_INDEX_NODE(me->op1)))
                                  TM->ReplaceTreeNode(stmt, me->op1, TM->CreateUniqueIntegerCst(static_cast<long long int>(int_const->value>>trailing_eq), type_index1));
                               else
                                  TM->ReplaceTreeNode(stmt, me->op1, TM->CreateUniqueIntegerCst(static_cast<long long int>(static_cast<unsigned long long int>(int_const->value)>>trailing_eq), type_index1));
                            }
                         }
                      }
                   }
               }
               else if(GET_NODE(ga->op1)->get_kind() == cond_expr_K)
               {
                   cond_expr * me = GetPointer<cond_expr>(GET_NODE(ga->op1));
                   tree_nodeRef op0 = GET_NODE(me->op1);
                   tree_nodeRef op1 = GET_NODE(me->op2);
                   tree_nodeRef condition = GET_NODE(me->op0);
                   if(op0==op1)
                   {
                       INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Cond expr with equal operands");
                       const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                       for(const auto use : StmtUses)
                       {
                          INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace var usage before: "+ use.first->ToString());
                          TM->ReplaceTreeNode(use.first, ga->op0, me->op1);
                          INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace var usage after: "+ use.first->ToString());
                          modified = true;
                       }
                       if(not StmtUses.empty())
                          restart_dead_code = true;
#ifndef NDEBUG
                       AppM->RegisterTransformation(GetName(), stmt);
#endif
                   }
                   else if(condition->get_kind() == integer_cst_K)
                   {
                      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Cond expr with constant condition");
                      integer_cst *ic = GetPointer<integer_cst>(condition);
                      unsigned long long int ull_value = static_cast<unsigned long long int>(tree_helper::get_integer_cst_value(ic));
                      const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                      for(const auto use : StmtUses)
                      {
                         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace var usage before: "+ use.first->ToString());
                         if(ull_value)
                            TM->ReplaceTreeNode(use.first, ga->op0, me->op1);
                         else
                            TM->ReplaceTreeNode(use.first, ga->op0, me->op2);
                         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace var usage after: "+ use.first->ToString());
                         modified = true;
                      }
                      if(not StmtUses.empty())
                         restart_dead_code = true;
#ifndef NDEBUG
                      AppM->RegisterTransformation(GetName(), stmt);
#endif
                   }
                   else
                   {
                      THROW_ASSERT(op0 != op1, "unexpected condition");
                      std::string s0, s1;
                      if(GetPointer<ssa_name>(op0))
                          s0 = GetPointer<ssa_name>(op0)->bit_values;
                      if(GetPointer<ssa_name>(op1))
                          s1 = GetPointer<ssa_name>(op1)->bit_values;
                      unsigned int precision;
                      precision = static_cast<unsigned int>(std::max(s0.size(), s1.size()));
                      if(GetPointer<integer_cst>(op0))
                      {
                         integer_cst *ic = GetPointer<integer_cst>(op0);
                         unsigned long long int ull_value = static_cast<unsigned long long int>(tree_helper::get_integer_cst_value(ic));
                         s0 = convert_to_binary(ull_value, std::max(precision, tree_helper::Size(op0)));
                      }
                      if(GetPointer<integer_cst>(op1))
                      {
                         integer_cst *ic = GetPointer<integer_cst>(op1);
                         unsigned long long int ull_value = static_cast<unsigned long long int>(tree_helper::get_integer_cst_value(ic));
                         s1 = convert_to_binary(ull_value, std::max(precision, tree_helper::Size(op1)));
                      }
                      precision = static_cast<unsigned int>(std::max(s0.size(), s1.size()));

                      if(precision)
                      {
                          unsigned int trailing_eq = 0;
                          unsigned int minimum_precision = static_cast<unsigned int>(std::min(s0.size(), s1.size()));
                          INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Bit_value strings are " + s0 + " and " + s1);
                          if(precision==0)
                             precision = 1;
                          for(unsigned int index=0; index < (minimum_precision-1); ++index)
                          {
                              if((s0[s0.size()-index-1] == '0' || s0[s0.size()-index-1] == 'X') && (s1[s1.size()-index-1] == '0' || s1[s1.size()-index-1] == 'X'))
                                  ++trailing_eq;
                              else
                                  break;
                          }
                          if(trailing_eq)
                          {
                              INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "-->Bit Value Opt: cond_expr optimized, nbits = " + STR(trailing_eq));
                              INDENT_OUT_MEX(OUTPUT_LEVEL_VERBOSE, output_level, "<--");
                              modified = true;
                              const std::string srcp_default = ga->include_name + ":" + STR(ga->line_number) + ":" + STR(ga->column_number);
                              unsigned int type_index0 = tree_helper::get_type_index(TM, GET_INDEX_NODE(me->op1));
                              tree_nodeRef op0_op_type = TM->GetTreeReindex(type_index0);
                              unsigned int type_index1 = tree_helper::get_type_index(TM, GET_INDEX_NODE(me->op2));
                              tree_nodeRef op1_op_type = TM->GetTreeReindex(type_index1);

                              if(GetPointer<ssa_name>(op0))
                              {
                                  tree_nodeRef op0_const_node = TM->CreateUniqueIntegerCst(static_cast<long long int>(trailing_eq), type_index0);
                                  tree_nodeRef op0_expr = IRman->create_binary_operation(op0_op_type, me->op1, op0_const_node, srcp_default, rshift_expr_K);
                                  tree_nodeRef op0_ga = create_ga(IRman, Scpe, op0_op_type, op0_expr, B_id, srcp_default);
                                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(op0_ga));
                                  B->PushBefore(op0_ga, stmt);
                                  tree_nodeRef op0_ga_var = GetPointer<gimple_assign>(GET_NODE(op0_ga))->op0;
                                  TM->ReplaceTreeNode(stmt, me->op1, op0_ga_var);
                                  ///set the bit_values to the ssa var
                                  ssa_name* op0_ssa = GetPointer<ssa_name>(GET_NODE(op0_ga_var));
                                  op0_ssa->bit_values = GetPointer<ssa_name>(op0)->bit_values.substr(0, GetPointer<ssa_name>(op0)->bit_values.size()-trailing_eq);
                              }
                              else
                              {
                                  integer_cst *int_const= GetPointer<integer_cst>(op0);
                                  if(tree_helper::is_int(TM, GET_INDEX_NODE(me->op0)))
                                      TM->ReplaceTreeNode(stmt, me->op1, TM->CreateUniqueIntegerCst(static_cast<long long int>(int_const->value>>trailing_eq), type_index0));
                                  else
                                      TM->ReplaceTreeNode(stmt, me->op1, TM->CreateUniqueIntegerCst(static_cast<long long int>(static_cast<unsigned long long int>(int_const->value)>>trailing_eq), type_index0));
                              }
                              if(GetPointer<ssa_name>(op1))
                              {
                                  tree_nodeRef op1_const_node = TM->CreateUniqueIntegerCst(static_cast<long long int>(trailing_eq), type_index1);
                                  tree_nodeRef op1_expr = IRman->create_binary_operation(op1_op_type, me->op2, op1_const_node, srcp_default, rshift_expr_K);
                                  tree_nodeRef op1_ga = create_ga(IRman, Scpe, op1_op_type, op1_expr, B_id, srcp_default);
                                  INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(op1_ga));
                                  B->PushBefore(op1_ga, stmt);
                                  tree_nodeRef op1_ga_var = GetPointer<gimple_assign>(GET_NODE(op1_ga))->op0;
                                  TM->ReplaceTreeNode(stmt, me->op2, op1_ga_var);
                                  ///set the bit_values to the ssa var
                                  ssa_name* op1_ssa = GetPointer<ssa_name>(GET_NODE(op1_ga_var));
                                  op1_ssa->bit_values = GetPointer<ssa_name>(op1)->bit_values.substr(0, GetPointer<ssa_name>(op1)->bit_values.size()-trailing_eq);
                              }
                              else
                              {
                                  integer_cst *int_const= GetPointer<integer_cst>(op1);
                                  if(tree_helper::is_int(TM, GET_INDEX_NODE(me->op2)))
                                      TM->ReplaceTreeNode(stmt, me->op2, TM->CreateUniqueIntegerCst(static_cast<long long int>(int_const->value>>trailing_eq), type_index1));
                                  else
                                      TM->ReplaceTreeNode(stmt, me->op2, TM->CreateUniqueIntegerCst(static_cast<long long int>(static_cast<unsigned long long int>(int_const->value)>>trailing_eq), type_index1));
                              }
                              tree_nodeRef ssa_vd = IRman->create_ssa_name(tree_nodeRef(), ga_op_type);
                              ssa_name * sn = GetPointer<ssa_name> (GET_NODE(ssa_vd));
                              ///set the bit_values to the ssa var
                              if(ssa->bit_values.size())
                                  sn->bit_values = ssa->bit_values.substr(0, ssa->bit_values.size()-trailing_eq);
                              tree_nodeRef op_const_node = TM->CreateUniqueIntegerCst(static_cast<long long int>(trailing_eq), type_index);
                              tree_nodeRef op_expr = IRman->create_binary_operation(ga_op_type, ssa_vd, op_const_node, srcp_default, lshift_expr_K);
                              tree_nodeRef curr_ga = create_ga(IRman, Scpe, ga_op_type, op_expr, B_id, srcp_default);
                              INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "Created " + STR(curr_ga));
                              TM->ReplaceTreeNode(curr_ga, GetPointer<gimple_assign>(GET_NODE(curr_ga))->op0, ga->op0);
                              TM->ReplaceTreeNode(stmt, ga->op0, ssa_vd);
                              B->PushAfter(curr_ga, stmt);
                              modified = true;
#ifndef NDEBUG
                              AppM->RegisterTransformation(GetName(), stmt);
#endif
                          }
                          else if(precision==1 && s0=="1" && s1=="0")
                          {
                             INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Cond expr with true and false");
                             const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                             for(const auto use : StmtUses)
                             {
                                INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace var usage before: "+ use.first->ToString());
                                TM->ReplaceTreeNode(use.first, ga->op0, me->op0);
                                INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace var usage after: "+ use.first->ToString());
                                modified = true;
                             }
                             if(not StmtUses.empty())
                                restart_dead_code = true;
#ifndef NDEBUG
                             AppM->RegisterTransformation(GetName(), stmt);
#endif

                          }
                          else if(precision == 1 and s0 == "0" and s1 == "1")
                          {
                             INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---Cond expr with false and true");
                             ///second argument is null since we cannot add the new statement at the end of the current BB
                             const auto new_ssa = IRman->CreateNotExpr(me->op0, blocRef());
                             const auto new_stmt = GetPointer<const ssa_name>(GET_CONST_NODE(new_ssa))->CGetDefStmt();
                             B->PushBefore(new_stmt, stmt);
                             const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                             for(const auto use : StmtUses)
                             {
                                INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace var usage before: "+ use.first->ToString());
                                TM->ReplaceTreeNode(use.first, ga->op0, new_ssa);
                                INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace var usage after: "+ use.first->ToString());
                                modified = true;
                             }
                             if(not StmtUses.empty())
                                restart_dead_code = true;
#ifndef NDEBUG
                             AppM->RegisterTransformation(GetName(), stmt);
#endif

                          }
                      }
                      else if(GetPointer<integer_cst>(op0) && GetPointer<integer_cst>(op1))
                      {
                         integer_cst *ic = GetPointer<integer_cst>(op0);
                         unsigned long long int ull_value0 = static_cast<unsigned long long int>(tree_helper::get_integer_cst_value(ic));
                         ic = GetPointer<integer_cst>(op1);
                         unsigned long long int ull_value1 = static_cast<unsigned long long int>(tree_helper::get_integer_cst_value(ic));
                         if(ull_value0==1 && ull_value1==0)
                         {
                            tree_nodeRef ga_nop = IRman->CreateNopExpr(me->op0, GetPointer<ternary_expr>(GET_NODE(ga->op1))->type);
                            B->PushBefore(ga_nop, stmt);
                            modified = true;
#ifndef NDEBUG
                            AppM->RegisterTransformation(GetName(), ga_nop);
#endif
                            tree_nodeRef nop_ga_var = GetPointer<gimple_assign>(GET_NODE(ga_nop))->op0;
                            TM->ReplaceTreeNode(stmt, ga->op1, nop_ga_var);
                         }
                      }
                   }
               }
               else if(GET_NODE(ga->op1)->get_kind() == truth_not_expr_K)
               {
                  truth_not_expr * tne = GetPointer<truth_not_expr>(GET_NODE(ga->op1));
                  if(GET_NODE(tne->op)->get_kind() == integer_cst_K)
                  {
                     integer_cst *int_const= GetPointer<integer_cst>(GET_NODE(tne->op));
                     long long int  const_value = int_const->value == 0 ? 1LL : 0LL;
                     tree_nodeRef val = TM->CreateUniqueIntegerCst(const_value, type_index);
                     const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                     for(const auto use : StmtUses)
                     {
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage before: "+ use.first->ToString());
                        TM->ReplaceTreeNode(use.first, ga->op0, val);
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage after: "+ use.first->ToString());
                        modified = true;
                     }
                     if(not StmtUses.empty())
                        restart_dead_code = true;
   #ifndef NDEBUG
                     AppM->RegisterTransformation(GetName(), stmt);
   #endif
                  }
               }
               else if(GET_NODE(ga->op1)->get_kind() == truth_and_expr_K)
               {
                  truth_and_expr * tae = GetPointer<truth_and_expr>(GET_NODE(ga->op1));
                  if(GET_NODE(tae->op0)->get_kind() == integer_cst_K || GET_NODE(tae->op1)->get_kind() == integer_cst_K)
                  {
                     tree_nodeRef val;
                     if(GET_NODE(tae->op0)->get_kind() == integer_cst_K)
                     {
                        integer_cst *int_const= GetPointer<integer_cst>(GET_NODE(tae->op0));
                        if(int_const->value == 0)
                           val = tae->op0;
                        else
                           val = tae->op1;
                     }
                     else
                     {
                        integer_cst *int_const= GetPointer<integer_cst>(GET_NODE(tae->op1));
                        if(int_const->value == 0)
                           val = tae->op1;
                        else
                           val = tae->op0;
                     }
                     const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                     for(const auto use : StmtUses)
                     {
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage before: "+ use.first->ToString());
                        TM->ReplaceTreeNode(use.first, ga->op0, val);
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage after: "+ use.first->ToString());
                        modified = true;
                     }
                     if(not StmtUses.empty())
                        restart_dead_code = true;
   #ifndef NDEBUG
                     AppM->RegisterTransformation(GetName(), stmt);
   #endif
                  }
               }
               else if(GET_NODE(ga->op1)->get_kind() == truth_or_expr_K)
               {
                  truth_or_expr * toe = GetPointer<truth_or_expr>(GET_NODE(ga->op1));
                  if(GET_NODE(toe->op0)->get_kind() == integer_cst_K || GET_NODE(toe->op1)->get_kind() == integer_cst_K)
                  {
                     tree_nodeRef val;
                     if(GET_NODE(toe->op0)->get_kind() == integer_cst_K)
                     {
                        integer_cst *int_const= GetPointer<integer_cst>(GET_NODE(toe->op0));
                        if(int_const->value == 0)
                           val = toe->op1;
                        else
                           val = toe->op0;
                     }
                     else
                     {
                        integer_cst *int_const= GetPointer<integer_cst>(GET_NODE(toe->op1));
                        if(int_const->value == 0)
                           val = toe->op0;
                        else
                           val = toe->op1;
                     }
                     const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                     for(const auto use : StmtUses)
                     {
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage before: "+ use.first->ToString());
                        TM->ReplaceTreeNode(use.first, ga->op0, val);
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage after: "+ use.first->ToString());
                        modified = true;
                     }
                     if(not StmtUses.empty())
                        restart_dead_code = true;
   #ifndef NDEBUG
                     AppM->RegisterTransformation(GetName(), stmt);
   #endif
                  }
               }
               else if(GET_NODE(ga->op1)->get_kind() == bit_ior_expr_K)
               {
                  bit_ior_expr * bie = GetPointer<bit_ior_expr>(GET_NODE(ga->op1));
                  if(GET_NODE(bie->op0)->get_kind() == integer_cst_K || GET_NODE(bie->op1)->get_kind() == integer_cst_K)
                  {
                     tree_nodeRef val;
                     if(GET_NODE(bie->op0)->get_kind() == integer_cst_K)
                     {
                        integer_cst *int_const= GetPointer<integer_cst>(GET_NODE(bie->op0));
                        if(int_const->value == 0)
                           val = bie->op1;
                     }
                     else
                     {
                        integer_cst *int_const= GetPointer<integer_cst>(GET_NODE(bie->op1));
                        if(int_const->value == 0)
                           val = bie->op0;
                     }
                     if(val)
                     {
                        const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                        for(const auto use : StmtUses)
                        {
                           INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage before: "+ use.first->ToString());
                           TM->ReplaceTreeNode(use.first, ga->op0, val);
                           INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage after: "+ use.first->ToString());
                           modified = true;
                        }
                        if(not StmtUses.empty())
                           restart_dead_code = true;
#ifndef NDEBUG
                        AppM->RegisterTransformation(GetName(), stmt);
#endif
                     }
                  }
               }
               else if(GET_NODE(ga->op1)->get_kind() == pointer_plus_expr_K)
               {
                  pointer_plus_expr * ppe = GetPointer<pointer_plus_expr>(GET_NODE(ga->op1));
                  if(GET_NODE(ppe->op1)->get_kind() == integer_cst_K)
                  {
                     integer_cst *int_const= GetPointer<integer_cst>(GET_NODE(ppe->op1));
                     if(int_const->value == 0)
                     {
                        const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                        for(const auto use : StmtUses)
                        {
                           INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage before: "+ use.first->ToString());
                           TM->ReplaceTreeNode(use.first, ga->op0, ppe->op0);
                           INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage after: "+ use.first->ToString());
                           modified = true;
                        }
                        if(not StmtUses.empty())
                           restart_dead_code = true;
#ifndef NDEBUG
                        AppM->RegisterTransformation(GetName(), stmt);
#endif
                     }
                     else if(GetPointer<ssa_name>(GET_NODE(ppe->op0)))
                     {
                        auto temp_def = GET_NODE(GetPointer<const ssa_name>(GET_NODE(ppe->op0))->CGetDefStmt());
                        if(temp_def->get_kind() == gimple_assign_K)
                        {
                           const auto prev_ga = GetPointer<const gimple_assign>(temp_def);
                           if(GET_NODE(prev_ga->op1)->get_kind() == pointer_plus_expr_K)
                           {
                              const auto prev_ppe = GetPointer<const pointer_plus_expr>(GET_NODE(prev_ga->op1));
                              if(GetPointer<ssa_name>(GET_NODE(prev_ppe->op0))&& GetPointer<integer_cst>(GET_NODE(prev_ppe->op1)))
                              {
                                 ssa_name *ssa_ppe_op0 = GetPointer<ssa_name> (GET_NODE(ppe->op0));

                                 size_t prev_val = static_cast<size_t>(tree_helper::get_integer_cst_value(GetPointer<integer_cst>(GET_NODE(prev_ppe->op1))));
                                 size_t curr_val = static_cast<size_t>(tree_helper::get_integer_cst_value(GetPointer<integer_cst>(GET_NODE(ppe->op1))));
                                 unsigned int type_ppe_op1_index = tree_helper::get_type_index(TM, GET_INDEX_NODE(ppe->op1));
                                 ppe->op1 = TM->CreateUniqueIntegerCst(static_cast<long long int>(prev_val+curr_val), type_ppe_op1_index);
                                 INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage before: "+ stmt->ToString());
                                 TM->ReplaceTreeNode(stmt, ppe->op0, prev_ppe->op0);
                                 INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage after: "+ stmt->ToString());
                                 const TreeNodeMap<size_t> StmtUses = ssa_ppe_op0->CGetUseStmts();
                                 if(not StmtUses.empty())
                                    restart_dead_code = true;
                                 modified = true;
#ifndef NDEBUG
                                 AppM->RegisterTransformation(GetName(), stmt);
#endif
                              }
                           }
                        }
                     }
                  }
               }
               else if(GET_NODE(ga->op1)->get_kind() == addr_expr_K)
               {
                  addr_expr * ae =  GetPointer<addr_expr>(GET_NODE(ga->op1));
                  enum kind ae_code = GET_NODE(ae->op)->get_kind();
                  if(ae_code == mem_ref_K)
                  {
                     mem_ref * MR =  GetPointer<mem_ref>(GET_NODE(ae->op));
                     tree_nodeRef op1 = MR->op1;
                     long long int op1_val = tree_helper::get_integer_cst_value(GetPointer<integer_cst>(GET_NODE(op1)));
                     if(op1_val == 0 && GET_NODE(MR->op0)->get_kind() == ssa_name_K)
                     {
                        auto temp_def = GET_NODE(GetPointer<const ssa_name>(GET_NODE(MR->op0))->CGetDefStmt());
                        if(temp_def->get_kind() == gimple_assign_K)
                        {
                           const auto prev_ga = GetPointer<const gimple_assign>(temp_def);
                           if(GET_NODE(prev_ga->op1)->get_kind() == addr_expr_K)
                           {
                              const TreeNodeMap<size_t> StmtUses = ssa->CGetUseStmts();
                              for(const auto use : StmtUses)
                              {
                                 INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage before: "+ use.first->ToString());
                                 TM->ReplaceTreeNode(use.first, ga->op0, prev_ga->op0);
                                 INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage after: "+ use.first->ToString());
                                 modified = true;
                              }
                              if(not StmtUses.empty())
                                 restart_dead_code = true;
      #ifndef NDEBUG
                              AppM->RegisterTransformation(GetName(), stmt);
      #endif
                           }
                        }
                     }
                     else if (op1_val == 0 && GET_NODE(MR->op0)->get_kind() == integer_cst_K)
                     {
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage before: "+ stmt->ToString());
                        TM->ReplaceTreeNode(stmt, ga->op1, MR->op0);
                        INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "---replace constant usage after: "+ stmt->ToString());
                     }
                  }
               }
               else if(GET_NODE(ga->op1)->get_kind() == lut_expr_K)
               {
                  const auto le = GetPointer<const lut_expr>(GET_CONST_NODE(ga->op1));
                  if(GET_CONST_NODE(le->op0)->get_kind() == integer_cst_K and GET_CONST_NODE(le->op1)->get_kind() == integer_cst_K)
                  {
                     const auto op0 = tree_helper::get_integer_cst_value(GetPointer<const integer_cst>(GET_CONST_NODE(le->op0)));
                     const auto op1 = tree_helper::get_integer_cst_value(GetPointer<const integer_cst>(GET_CONST_NODE(le->op1)));
                     const long long int value = ((op1 >> op0) & 1);
                     ga->op1 = IRman->CreateIntegerCst(TM->GetTreeReindex(tree_helper::CGetType(GET_CONST_NODE(ga->op0))->index), value, TM->new_tree_node_id());
                  }
               }
            }
         }
         INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--Statement analyzed " + GET_NODE(stmt)->ToString());
      }
      INDENT_DBG_MEX(DEBUG_LEVEL_VERY_PEDANTIC, debug_level, "<--BB analyzed " + STR(B_id));
   }
}

DesignFlowStep_Status Bit_Value_opt::InternalExec ()
{
   PRINT_DBG_MEX(DEBUG_LEVEL_PEDANTIC, debug_level, " --------- BIT_VALUE_OPT ----------");
   tree_managerRef TM = AppM->get_tree_manager();

   tree_nodeRef tn = TM->get_tree_node_const(function_id);
   //tree_nodeRef Scpe = TM->GetTreeReindex(function_id);
   function_decl * fd = GetPointer<function_decl>(tn);
   THROW_ASSERT(fd && fd->body, "Node is not a function or it hasn't a body");
   statement_list * sl = GetPointer<statement_list>(GET_NODE(fd->body));
   THROW_ASSERT(sl, "Body is not a statement_list");
   /// for each basic block B in CFG do > Consider all blocks successively
   restart_dead_code = false;
   modified = false;
   optimize(sl, TM);
   THROW_ASSERT(not restart_dead_code or modified, "");
   modified ? function_behavior->UpdateBBVersion() : 0;
   return modified ? DesignFlowStep_Status::SUCCESS : DesignFlowStep_Status::UNCHANGED;

}

bool Bit_Value_opt::HasToBeExecuted() const
{
   return (bitvalue_version != function_behavior->GetBitValueVersion()) or FunctionFrontendFlowStep::HasToBeExecuted();
}

void Bit_Value_opt::Initialize()
{
   tree_managerRef TM = AppM->get_tree_manager();
   IRman = tree_manipulationRef(new tree_manipulation(TM,parameters));
}
