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
 * @file memory.cpp
 * @brief Class for representing memory information
 *
 *
 * @author Christian Pilato <pilato@elet.polimi.it>
 * @author Fabrizio Ferrandi <fabrizio.ferrandi@polimi.it>
 * $Revision$
 * $Date$
 * Last modified by $Author$
 *
*/
#include "memory.hpp"

#include "memory_symbol.hpp"

#include "application_manager.hpp"
#include "call_graph_manager.hpp"

#include "tree_helper.hpp"
#include "tree_node.hpp"
#include "tree_manager.hpp"
#include "funit_obj.hpp"

#include "structural_manager.hpp"
#include "structural_objects.hpp"

#include "exceptions.hpp"
#include "utility.hpp"

#include "xml_helper.hpp"
#include "polixml.hpp"

#include "math_function.hpp"

/// we start to allocate from internal_base_address_alignment byte to align address to internal_base_address_alignment bits
/// we can use address 0 in some cases but it is not safe in general.

memory::memory(const tree_managerRef _TreeM, unsigned int _off_base_address, unsigned int max_bram, bool _null_pointer_check, bool initial_internal_address_p, unsigned int initial_internal_address, unsigned int &_address_bitsize) :
   TreeM(_TreeM),
   maximum_private_memory_size(0),
   total_amount_of_private_memory(0),
   total_amount_of_parameter_memory(0),
   off_base_address(_off_base_address),
   next_off_base_address(_off_base_address),
   bus_data_bitsize(0),
   bus_addr_bitsize(_address_bitsize),
   bus_size_bitsize(0),
   aligned_bitsize(0),
   bram_bitsize(0),
   maxbram_bitsize(0),
   intern_shared_data(false),
   use_unknown_addresses(false),
   pointer_conversion(false),
   all_pointers_resolved(false),
   implicit_memcpy(false),
   parameter_alignment(16),
   null_pointer_check(_null_pointer_check),
   packed_vars(false)
{
   unsigned int max_bus_size = 2*max_bram;
   external_base_address_alignment= internal_base_address_alignment= max_bus_size/8;
   if(null_pointer_check && !initial_internal_address_p)
      next_base_address = internal_base_address_start = max_bus_size/8;
   else if (initial_internal_address_p && initial_internal_address != 0)
   {
      align(initial_internal_address, internal_base_address_alignment);
      next_base_address = internal_base_address_start = initial_internal_address;
   }
   else
      next_base_address = internal_base_address_start = 0;
}

memory::~memory()
{

}

std::map<unsigned int, memory_symbolRef> memory::get_ext_memory_variables() const
{
   return external;
}

void memory::compute_next_base_address(unsigned int& address, unsigned int var, unsigned int alignment)
{
   const tree_nodeRef node = TreeM->get_tree_node_const(var);
   unsigned int size = 0;

   // The __builtin_wait_call associate an address to the call site to
   // identify it.  For this case we are allocating a word.
   if(GetPointer<gimple_call>(node))
   {
      size = (bus_addr_bitsize % 8 == 0) ? bus_addr_bitsize / 8 : (bus_addr_bitsize / 8) + 1;
   } else {
      ///compute the next base address
      size = compute_n_bytes(tree_helper::size(TreeM, tree_helper::get_type_index(TreeM, var)));
   }
   address += size;
   ///align the memory address
   align(address, alignment);
}

void memory::add_internal_variable(unsigned int funID_scope, unsigned int var)
{
   memory_symbolRef m_sym;
   if(in_vars.find(var) != in_vars.end())
      m_sym = in_vars[var];
   else if(is_private_memory(var))
      m_sym = memory_symbolRef(new memory_symbol(var, null_pointer_check ? internal_base_address_start : 0, funID_scope));
   else
      m_sym = memory_symbolRef(new memory_symbol(var, next_base_address, funID_scope));
   add_internal_symbol(funID_scope, var, m_sym);
}

void memory::add_internal_variable_proxy(unsigned int funID_scope, unsigned int var)
{
   internal_variable_proxy[funID_scope].insert(var);
   proxied_variables.insert(var);
}

const std::set<unsigned int>& memory::get_proxied_internal_variables(unsigned int funID_scope) const
{
   THROW_ASSERT(has_proxied_internal_variables(funID_scope), "No proxy variables for " + STR(funID_scope));
   return internal_variable_proxy.find(funID_scope)->second;
}

bool memory::has_proxied_internal_variables(unsigned int funID_scope) const
{
   return internal_variable_proxy.find(funID_scope) != internal_variable_proxy.end();
}

bool memory::is_a_proxied_variable(unsigned int var) const
{
   return proxied_variables.find(var) != proxied_variables.end();
}

void memory::add_read_only_variable(unsigned var)
{
   read_only_vars.insert(var);
}

bool memory::is_read_only_variable(unsigned var) const
{
   return read_only_vars.find(var) != read_only_vars.end();
}

void memory::add_internal_symbol(unsigned int funID_scope, unsigned int var, const memory_symbolRef m_sym)
{
   if (external.find(var) != external.end())
      THROW_WARNING("The variable " + STR(var) + " has been already allocated out of the module");
   if (parameter.find(var) != parameter.end())
      THROW_WARNING("The variable " + STR(var) + " has been already set as a parameter");
   THROW_ASSERT(in_vars.find(var) == in_vars.end() || is_private_memory(var), "variable already allocated inside this module");

   internal[funID_scope][var] = m_sym;
   if (GetPointer<gimple_call>(TreeM->get_tree_node_const(var)))
      callSites[var] = m_sym;
   else
      in_vars[var] = m_sym;

   if(is_private_memory(var))
   {
      unsigned int allocated_memory =compute_n_bytes(tree_helper::size(TreeM, var));
      rangesize[var] = allocated_memory;
      align(rangesize[var], internal_base_address_alignment);
      total_amount_of_private_memory += allocated_memory;
      maximum_private_memory_size = std::max(maximum_private_memory_size, allocated_memory);
   }
   else
   {
      unsigned int address = m_sym->get_address();
      unsigned int address_orig = address;
      compute_next_base_address(address, var, internal_base_address_alignment);
      next_base_address = std::max(next_base_address, address);
      rangesize[var] = next_base_address-address_orig;
   }
}

unsigned int memory::count_non_private_internal_symbols() const
{
   unsigned int n_non_private = 0;
   for(auto iv_pair: in_vars)
      if(!is_private_memory(iv_pair.first))
         ++n_non_private;
   return n_non_private;
}

void memory::add_external_variable(unsigned int var)
{
   memory_symbolRef m_sym = memory_symbolRef(new memory_symbol(var, next_off_base_address, 0));
   add_external_symbol(var, m_sym);
}

void memory::add_external_symbol(unsigned int var, const memory_symbolRef m_sym)
{
   if (in_vars.find(var) != in_vars.end())
      THROW_WARNING("The variable " + STR(var) + " has been already internally allocated");
   if (parameter.find(var) != parameter.end())
      THROW_WARNING("The variable " + STR(var) + " has been already set as a parameter");
   external[var] = m_sym;
   unsigned int address = m_sym->get_address();
   compute_next_base_address(address, var, external_base_address_alignment);
   next_off_base_address = std::max(next_off_base_address, address);
}

void memory::add_private_memory(unsigned int var)
{
   private_memories.insert(var);
}

void memory::set_sds_var(unsigned int var, bool value)
{
   same_data_size_accesses[var] = value;
}

void memory::add_source_value(unsigned int var, unsigned int value)
{
   source_values[var].insert(value);
}

void memory::add_parameter(unsigned int funID_scope, unsigned int var, bool is_last)
{
   memory_symbolRef m_sym = memory_symbolRef(new memory_symbol(var, next_base_address, funID_scope));
   add_parameter_symbol(funID_scope, var, m_sym);
   if(is_last)
   {
      unsigned int address = next_base_address;
      ///align in case is not aligned
      align(next_base_address, internal_base_address_alignment);
      total_amount_of_parameter_memory += next_base_address-address;
   }
}

void memory::add_parameter_symbol(unsigned int funID_scope, unsigned int var, const memory_symbolRef m_sym)
{
   if (external.find(var) != external.end())
      THROW_WARNING("The variable " + STR(var) + " has been already allocated out of the module");
   if (in_vars.find(var) != in_vars.end())
      THROW_WARNING("The variable " + STR(var) + " has been already internally allocated");
   ///allocation of the parameters
   params[var] = parameter[funID_scope][var] = m_sym;
   unsigned int address = m_sym->get_address();
   compute_next_base_address(next_base_address, var, parameter_alignment);
   next_base_address = std::max(next_base_address, address);
   total_amount_of_parameter_memory += next_base_address-address;
}

unsigned int memory::get_memory_address() const
{
   return next_off_base_address;
}

bool memory::is_internal_variable(unsigned int funID_scope, unsigned int var) const
{
   return internal.find(funID_scope) != internal.end() && internal.find(funID_scope)->second.find(var) != internal.find(funID_scope)->second.end();
}

bool memory::is_external_variable(unsigned int var) const
{
   return external.find(var) != external.end();
}

bool memory::is_private_memory(unsigned int var) const
{
   return private_memories.find(var) != private_memories.end();
}

bool memory::is_sds_var(unsigned int var) const
{
   THROW_ASSERT(has_sds_var(var), "variable not classified" + STR(var));
   return same_data_size_accesses.find(var)->second;
}

bool memory::has_sds_var(unsigned int var) const
{
   return same_data_size_accesses.find(var) != same_data_size_accesses.end();
}

bool memory::is_parameter(unsigned int funID_scope, unsigned int var) const
{
   return parameter.find(funID_scope) != parameter.end() && parameter.find(funID_scope)->second.find(var) != parameter.find(funID_scope)->second.end();
}

unsigned int memory::get_callSite_base_address(unsigned int var) const
{
   THROW_ASSERT(callSites.find(var) != callSites.end(), "Variable not yet allocated");
   return callSites.find(var)->second->get_address();
}

unsigned int memory::get_internal_base_address(unsigned int var) const
{
   THROW_ASSERT(in_vars.find(var) != in_vars.end(), "Variable not yet allocated");
   return in_vars.find(var)->second->get_address();
}

unsigned int memory::get_external_base_address(unsigned int var) const
{
   THROW_ASSERT(external.find(var) != external.end(), "Variable not yet allocated");
   return external.find(var)->second->get_address();
}

unsigned int memory::get_parameter_base_address(unsigned int funId, unsigned int var) const
{
   THROW_ASSERT(parameter.find(funId) != parameter.end(), "Function not yet allocated");
   THROW_ASSERT(parameter.find(funId)->second.find(var) != parameter.find(funId)->second.end(), "Function not yet allocated");
   return parameter.find(funId)->second.find(var)->second->get_address();
}

std::map<unsigned int, memory_symbolRef> memory::get_function_vars(unsigned int funID_scope) const
{
   if (internal.find(funID_scope) == internal.end())
      return std::map<unsigned int, memory_symbolRef>();
   return internal.find(funID_scope)->second;
}

std::map<unsigned int, memory_symbolRef> memory::get_function_parameters(unsigned int funID_scope) const
{
   if (parameter.find(funID_scope) == parameter.end())
      return std::map<unsigned int, memory_symbolRef>();
   return parameter.find(funID_scope)->second;
}

bool memory::has_callSite_base_address(unsigned int var) const
{
   return callSites.find(var) != callSites.end();
}

bool memory::has_internal_base_address(unsigned int var) const
{
   return in_vars.find(var) != in_vars.end();
}

bool memory::has_external_base_address(unsigned int var) const
{
   return external.find(var) != external.end();
}

bool memory::has_parameter_base_address(unsigned int var, unsigned int funId) const
{
   std::map<unsigned int, std::map<unsigned int, memory_symbolRef> >::const_iterator itr =
       parameter.find(funId);
   if (itr == parameter.end()) return false;
   return itr->second.find(var) != itr->second.end();
}

bool memory::has_base_address(unsigned int var) const
{
   return external.find(var) != external.end() ||
     in_vars.find(var) != in_vars.end() ||
     params.find(var) != params.end() ||
     callSites.find(var) != callSites.end();
}

unsigned int memory::get_base_address(unsigned int var, unsigned int funId) const
{
   THROW_ASSERT(has_base_address(var), "Variable not yet allocated: @" + STR(var));
   if (has_callSite_base_address(var)) return get_callSite_base_address(var);
   if (has_internal_base_address(var)) return get_internal_base_address(var);
   if (funId && has_parameter_base_address(var, funId)) return get_parameter_base_address(var, funId);
   return get_external_base_address(var);
}

unsigned int memory::get_first_address(unsigned int funId) const
{
   const std::map<unsigned int, memory_symbolRef> & internalVars = (internal.find(funId)->second);
   unsigned int minAddress = UINT_MAX;
   for (std::map<unsigned int, memory_symbolRef>::const_iterator
              itr = internalVars.begin(), end = internalVars.end(); itr != end; ++itr)
   {
      unsigned int var = itr->first;
      if (itr->second && !is_private_memory(var) && !has_parameter_base_address(var, funId))
         minAddress = std::min(minAddress, itr->second->get_address());
   }
   if (minAddress == UINT_MAX)
   {
      const std::map<unsigned int, memory_symbolRef> & paramsVar = (parameter.find(funId)->second);
      for (std::map<unsigned int, memory_symbolRef>::const_iterator
                 itr = paramsVar.begin(), end = paramsVar.end(); itr != end; ++itr)
      {
         unsigned int var = itr->first;
         if (!is_private_memory(var))
            minAddress = std::min(minAddress, itr->second->get_address());
      }
   }
   return minAddress;
}

unsigned int memory::get_last_address(unsigned int funId, const application_managerRef AppMgr) const
{
   const std::set<unsigned int> calledSet = AppMgr->CGetCallGraphManager()->get_called_by(funId);
   const std::map<unsigned int, memory_symbolRef> & internalVars = (internal.find(funId)->second);
   unsigned int maxAddress = 0;
   for (std::map<unsigned int, memory_symbolRef>::const_iterator
              itr = internalVars.begin(), end = internalVars.end(); itr != end; ++itr)
   {
      unsigned int var = itr->first;
      if (!is_private_memory(var) && !has_parameter_base_address(var, funId) && has_base_address(var))
      {
         maxAddress = std::max(maxAddress, itr->second->get_address() + tree_helper::size(TreeM,  var) / 8);
      }
   }
   if (AppMgr->hasToBeInterfaced(funId))
   {
      const std::map<unsigned int, memory_symbolRef> & paramsVar = get_function_parameters(funId);
      for (std::map<unsigned int, memory_symbolRef>::const_iterator itr = paramsVar.begin(),
                                                                    end = paramsVar.end(); itr != end; ++itr)
      {
         unsigned int var = itr->first;
         maxAddress = std::max(maxAddress, itr->second->get_address() + tree_helper::size(TreeM,  var) / 8);
      }
   }
   for (std::set<unsigned int>::const_iterator
              Itr = calledSet.begin(), End = calledSet.end(); Itr != End; ++Itr)
   {
      if (not AppMgr->hasToBeInterfaced(*Itr))
      {
         maxAddress = std::max(get_last_address(*Itr, AppMgr), maxAddress);
      }
   }

   return maxAddress;
}

memory_symbolRef memory::get_symbol(unsigned int var, unsigned int funId) const
{
   THROW_ASSERT(has_base_address(var), "Variable not yet allocated: @" + STR(var));
   if (has_callSite_base_address(var)) return callSites.find(var)->second;
   if (has_internal_base_address(var)) return in_vars.find(var)->second;
   if (funId && has_parameter_base_address(var, funId)){
     std::map<unsigned int, std::map<unsigned int, memory_symbolRef> >::const_iterator itr =
         parameter.find(funId);
     return itr->second.find(var)->second;
   }
   return external.find(var)->second;
}

unsigned int memory::get_rangesize(unsigned int var) const
{
   THROW_ASSERT(has_base_address(var), "Variable not yet allocated: @" + STR(var));
   return rangesize.find(var)->second;
}

void memory::reserve_space(unsigned int space)
{
   next_off_base_address += space;
   align(next_off_base_address, internal_base_address_alignment);
}

unsigned int memory::get_allocated_space() const
{
   return total_amount_of_private_memory+next_base_address-internal_base_address_start;
}

unsigned int memory::get_allocated_parameters_memory() const
{
   return total_amount_of_parameter_memory;
}

unsigned int memory::get_max_address() const
{
   return std::max(next_base_address, maximum_private_memory_size+internal_base_address_start);
}

bool memory::is_parm_decl_copied(unsigned int var) const
{
   return parm_decl_copied.find(var) != parm_decl_copied.end();
}

void memory::add_parm_decl_copied(unsigned int var)
{
   parm_decl_copied.insert(var);
}

bool memory::is_parm_decl_stored(unsigned int var) const
{
   return parm_decl_stored.find(var) != parm_decl_stored.end();
}

void memory::add_parm_decl_stored(unsigned int var)
{
   parm_decl_stored.insert(var);
}

bool memory::is_actual_parm_loaded(unsigned int var) const
{
   return actual_parm_loaded.find(var) != actual_parm_loaded.end();
}

void memory::add_actual_parm_loaded(unsigned int var)
{
   actual_parm_loaded.insert(var);
}

void memory::set_internal_base_address_alignment(unsigned int _internal_base_address_alignment)
{
   THROW_ASSERT(_internal_base_address_alignment && !(_internal_base_address_alignment & (_internal_base_address_alignment-1)), "alignment must be a power of two");
   internal_base_address_alignment = _internal_base_address_alignment;
   align(internal_base_address_start, internal_base_address_alignment);
   next_base_address = internal_base_address_start;
}

void memory::propagate_memory_parameters(const structural_objectRef src, const structural_managerRef tgt)
{
   std::map<std::string, std::string> res_parameters;

   if (src->is_parameter(MEMORY_PARAMETER))
   {
      std::vector<std::string> current_src_parameters = convert_string_to_vector<std::string>(src->get_parameter(MEMORY_PARAMETER), ";");
      for(unsigned int l = 0; l < current_src_parameters.size(); l++)
      {
         std::vector<std::string> current_parameter = convert_string_to_vector<std::string>(current_src_parameters[l], "=");
         res_parameters[current_parameter[0]] = current_parameter[1];
      }
   }

   module * srcModule = GetPointer<module>(src);
   //std::cout << srcModule->get_id() << std::endl;
   if (srcModule)
   {
      for (unsigned int i = 0; i < srcModule->get_internal_objects_size(); ++i)
      {
         structural_objectRef subModule = srcModule->get_internal_object(i);
         if (subModule->is_parameter(MEMORY_PARAMETER))
         {
            std::vector<std::string> current_src_parameters = convert_string_to_vector<std::string>(subModule->get_parameter(MEMORY_PARAMETER), ";");
            for(unsigned int l = 0; l < current_src_parameters.size(); l++)
            {
               std::vector<std::string> current_parameter = convert_string_to_vector<std::string>(current_src_parameters[l], "=");
               res_parameters[current_parameter[0]] = current_parameter[1];
            }
         }
      }
   }

   if (tgt->get_circ()->is_parameter(MEMORY_PARAMETER))
   {
      std::vector<std::string> current_tgt_parameters = convert_string_to_vector<std::string>(tgt->get_circ()->get_parameter(MEMORY_PARAMETER), ";");
      for(unsigned int l = 0; l < current_tgt_parameters.size(); l++)
      {
         std::vector<std::string> current_parameter = convert_string_to_vector<std::string>(current_tgt_parameters[l], "=");
         if (res_parameters.find(current_parameter[0]) != res_parameters.end() && res_parameters[current_parameter[0]] != current_parameter[1])
            THROW_ERROR("The parameter \"" + current_parameter[0] + "\" has been set with (at least) two different values");
         res_parameters[current_parameter[0]] = current_parameter[1];
      }
   }

   if (res_parameters.size() == 0)
      return;

   std::string memory_parameters;
   for(std::map<std::string, std::string>::iterator it = res_parameters.begin(); it != res_parameters.end(); it++)
   {
      if (memory_parameters.size()) memory_parameters += ";";
      memory_parameters += it->first +"="+it->second;
   }
   tgt->get_circ()->set_parameter(MEMORY_PARAMETER, memory_parameters);
}

void memory::add_memory_parameter(const structural_managerRef SM, const std::string& name, const std::string& value)
{
   std::string memory_parameters;
   if (SM->get_circ()->is_parameter(MEMORY_PARAMETER))
   {
      memory_parameters = SM->get_circ()->get_parameter(MEMORY_PARAMETER) + ";";
   }
   std::vector<std::string> current_parameters = convert_string_to_vector<std::string>(memory_parameters, ";");
   for(unsigned int l = 0; l < current_parameters.size(); l++)
   {
      std::vector<std::string> current_parameter = convert_string_to_vector<std::string>(current_parameters[l], "=");
      THROW_ASSERT(current_parameter.size() == 2, "expected two elements");
      if (current_parameter[0] == name)
      {
         if (value == current_parameter[1])
            return;
         THROW_ERROR("The parameter \"" + name + "\" has been set with (at least) two different values: " + value + " != " + current_parameter[1]);
      }
   }
   memory_parameters += name + "=" + value;
   SM->get_circ()->set_parameter(MEMORY_PARAMETER, memory_parameters);
}

void memory::xwrite(xml_element* node)
{
   xml_element* Enode = node->add_child_element("HLS_memory");
   unsigned int base_address = off_base_address;
   WRITE_XVM(base_address, node);
   if (internal.size() or parameter.size())
   {
      xml_element* IntNode = Enode->add_child_element("internal_memory");
      for(std::map<unsigned int, std::map<unsigned int, memory_symbolRef> >::iterator iIt = internal.begin(); iIt != internal.end(); iIt++)
      {
         xml_element* ScopeNode = IntNode->add_child_element("scope");
         std::string id = "@" + STR(iIt->first);
         WRITE_XVM(id, ScopeNode);
         std::string name = tree_helper::name_function(TreeM, iIt->first);
         WRITE_XVM(name, ScopeNode);
         for(std::map<unsigned int, memory_symbolRef>::iterator vIt = iIt->second.begin(); vIt != iIt->second.end(); vIt++)
         {
            xml_element* VarNode = ScopeNode->add_child_element("variable");
            std::string variable = "@" + STR(vIt->second->get_variable());
            WRITE_XNVM(id, variable, VarNode);
            unsigned int address = vIt->second->get_address();
            WRITE_XVM(address, VarNode);
            std::string var_name = vIt->second->get_symbol_name();
            WRITE_XNVM(name, var_name, VarNode);
         }
         if (parameter.find(iIt->first) != parameter.end())
         {
            for(std::map<unsigned int, memory_symbolRef>::iterator vIt = parameter.find(iIt->first)->second.begin(); vIt != parameter.find(iIt->first)->second.end(); vIt++)
            {
               xml_element* VarNode = ScopeNode->add_child_element("parameter");
               std::string variable = "@" + STR(vIt->second->get_variable());
               WRITE_XNVM(id, variable, VarNode);
               unsigned int address = vIt->second->get_address();
               WRITE_XVM(address, VarNode);
               std::string var_name = vIt->second->get_symbol_name();
               WRITE_XNVM(symbol, var_name, VarNode);
            }
         }
      }
      if (parameter.size())
      {
         for(std::map<unsigned int, std::map<unsigned int, memory_symbolRef> >::iterator iIt = internal.begin(); iIt != internal.end(); iIt++)
         {
            if (internal.find(iIt->first) != internal.end()) continue;
            xml_element* ScopeNode = IntNode->add_child_element("scope");
            std::string id = "@" + STR(iIt->first);
            WRITE_XVM(id, ScopeNode);
            std::string name = tree_helper::name_function(TreeM, iIt->first);
            WRITE_XVM(name, ScopeNode);
            for(std::map<unsigned int, memory_symbolRef>::iterator vIt = parameter.find(iIt->first)->second.begin(); vIt != parameter.find(iIt->first)->second.end(); vIt++)
            {
               xml_element* VarNode = ScopeNode->add_child_element("parameter");
               std::string variable = "@" + STR(vIt->second->get_variable());
               WRITE_XNVM(id, variable, VarNode);
               unsigned int address = vIt->second->get_address();
               WRITE_XVM(address, VarNode);
               std::string var_name = vIt->second->get_symbol_name();
               WRITE_XNVM(symbol, var_name, VarNode);
            }
         }
      }
   }
   if (external.size())
   {
      xml_element* ExtNode = Enode->add_child_element("external_memory");
      for(std::map<unsigned int, memory_symbolRef>::iterator eIt = external.begin(); eIt != external.end(); eIt++)
      {
         xml_element* VarNode = ExtNode->add_child_element("variable");
         std::string variable = "@" + STR(eIt->second->get_variable());
         WRITE_XNVM(id, variable, VarNode);
         unsigned int address = eIt->second->get_address();
         WRITE_XVM(address, VarNode);
         std::string var_name = eIt->second->get_symbol_name();
         WRITE_XNVM(symbol, var_name, VarNode);
      }
   }
}
