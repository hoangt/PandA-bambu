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
 *              Copyright (c) 2015-2017 Politecnico di Milano
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
 * @file hdl_var_decl_fix.cpp
 * @brief Pre-analysis step fixing var_decl duplications and hdl name conflicts.
 *
 * @author Marco Lattuada <marco.lattuada@polimi.it>
 *
*/

///Header include
#include "hdl_var_decl_fix.hpp"

///. include
#include "Parameter.hpp"

///behavior includes
#include "application_manager.hpp"
#include "function_behavior.hpp"

///boost include
#include <boost/algorithm/string.hpp>

///design_flows/backend/ToHDL include
#include "language_writer.hpp"

///HLS includes
#include "hls_manager.hpp"
#include "hls_target.hpp"

///tree includes
#include "behavioral_helper.hpp"
#include "tree_manager.hpp"
#include "tree_node.hpp"
#include "tree_reindex.hpp"

///utility include
#include "utility.hpp"

HDLVarDeclFix::HDLVarDeclFix(const application_managerRef _AppM, unsigned int _function_id, const DesignFlowManagerConstRef _design_flow_manager, const ParameterConstRef _parameters) :
   VarDeclFix(_AppM, _function_id, _design_flow_manager, _parameters, HDL_VAR_DECL_FIX),
   hdl_writer_type(static_cast<HDLWriter_Language>(parameters->getOption<unsigned int>(OPT_writer_language)))
{
   debug_level = parameters->get_class_debug_level(GET_CLASS(*this), DEBUG_LEVEL_NONE);
}

HDLVarDeclFix::~HDLVarDeclFix()
{}

DesignFlowStep_Status HDLVarDeclFix::InternalExec()
{
   const tree_managerRef TM = AppM->get_tree_manager();

   ///Preload backend names
   const auto hdl_writer = language_writer::create_writer(hdl_writer_type, GetPointer<HLS_manager>(AppM)->get_HLS_target()->get_technology_manager(), parameters);
   const auto hdl_reserved_names = hdl_writer->GetHDLReservedNames();
   already_examinated_names.insert(hdl_reserved_names.begin(), hdl_reserved_names.end());

   /// a parameter cannot be named "this"
   already_examinated_names.insert(std::string("this"));

   ///Add function name
   already_examinated_names.insert(Normalize(function_behavior->CGetBehavioralHelper()->get_function_name()));

   ///Fixing names of parameters
   const tree_nodeRef curr_tn = TM->GetTreeNode(function_id);
   function_decl * fd = GetPointer<function_decl>(curr_tn);

   for(auto arg : fd->list_of_args)
      recursive_examinate(arg);

   VarDeclFix::InternalExec();

   return DesignFlowStep_Status::SUCCESS;
}

const std::string HDLVarDeclFix::Normalize(const std::string identifier) const
{
   return hdl_writer_type == HDLWriter_Language::VHDL  ? boost::to_upper_copy<std::string>(identifier) : identifier;
}

