#include "inexor/gluegen/SharedVarDatatypes.hpp"
#include "inexor/gluegen/SharedVariables.hpp"
#include "inexor/gluegen/parse_helpers.hpp"

#include <vector>
#include <string>
#include <hash_set>

using namespace pugi;
using namespace std;

namespace inexor { namespace gluegen {

shared_class_definition new_shared_class_definition(const xml_node &compound_xml)
{
    shared_class_definition def;

    def->refid = compound_xml.attribute("id").value();
    string full_name =  get_complete_xml_text(compound_xml.child("compoundname")); // includes the namespace e.g. inexor::rendering::screen

    vector<string> ns_and_name(split_by_delimiter(full_name, "::"));

    def->class_name = ns_and_name.back();
    ns_and_name.pop_back();
    def->definition_namespace = join(ns_and_name, "::");
    def->definition_header = compound_xml.child("location").attribute("file").value();

    if(contains(def->definition_header, ".c"))
    {
        std::cerr << "ERROR: SharedClasses can only be defined in cleanly include-able **header**-files"
                  << std::endl << "Class in question is " << full_name << std::endl;
        std::exit(1);
    }

    for(const xml_node &var_xml : find_class_member_vars(compound_xml))
    {
        string type = get_complete_xml_text(var_xml.child("type"));
        if(is_marked_variable(var_xml))
            def.elements.push_back(SharedVariable{var_xml});
    }
    return def;
}

/// Return true if this class' name is contained in the shared_var_type_literals hashset.
bool is_relevant_class(const xml_node &compound_xml, const std::set<std::string> &relevant_classes)
{
    // TODO control case that both are in the same namespace or not.
    if(relevant_classes.find(get_complete_xml_text(compound_xml.child("compoundname"))))
        return true;
    return false;
}

const std::set<std::string> shared_var_type_literals(const std::vector<std::string> &class_vec)
{
    std::set<std::string> buf;
    for(const string &c : class_vec)
        buf.emplace(c);
    return buf;
}

std::vector<shared_class_definition>
        find_class_definitions(const std::vector<std::unique_ptr<pugi::xml_document>> &AST_class_xmls,
                                const std::vector<std::string> &shared_var_type_literals)
{
    std::vector<shared_class_definition> buf;
    for(const auto &class_xml : AST_class_xmls)
    {
        const xml_node &compound_xml = class_xml->child("doxygen").child("compounddef");
        if(is_relevant_class(compound_xml))
            buf.push_back(new_shared_class_definition(compound_xml));
    }
    return buf;
}

/// Create a shared class definition which the
/// \param def
/// \param ctx
/// \param add_instances
/// \return
TemplateData get_shared_class_templatedata(shared_class_definition *def, ParserContext &ctx, bool add_instances = true)
{
    TemplateData cur_definition{TemplateData::type::object};
    // The class needs to be defined in a cleanly includeable header file.
    cur_definition.set("definition_header_file", def->containing_header);

    // TODO: recognize the innerclass case!
    // The name of the class with leading namespace.
    cur_definition.set("definition_name_cpp", def->get_name_cpp_full());
    cur_definition.set("definition_name_unique", def->get_name_unique());

    add_options_templatedata(cur_definition, def->attached_options, ctx);

    TemplateData all_instances{TemplateData::type::list};

    if(add_instances) for(ShTreeNode *inst_node : def->instances)
        {
            TemplateData cur_instance{TemplateData::type::object};
            add_namespace_seps_templatedata(cur_instance, inst_node->get_namespace());

            // The first parents name, e.g. of inexor::game::player1.weapons.ammo its player1.
            cur_instance.set("name_parent_cpp_short", inst_node->get_name_cpp_short());
            cur_instance.set("name_parent_cpp_full", inst_node->get_name_cpp_full());
            cur_instance.set("instance_name_unique", inst_node->get_name_unique());
            cur_instance.set("path", inst_node->get_path());
            cur_instance.set("index", get_index_incrementer());

            // were doing this for sharedlists, where the first template is relevant.
            if(inst_node->template_type_definition)
            {
                TemplateData dummy_list(TemplateData::type::list);
                dummy_list << get_shared_class_templatedata(inst_node->template_type_definition, ctx, false);
                cur_instance.set("first_template_type", std::move(dummy_list));
            }

            add_options_templatedata(cur_instance, inst_node->attached_options, ctx);

            all_instances << cur_instance;
        }
    cur_definition.set("instances", all_instances);

    TemplateData members{TemplateData::type::list};

    int local_index = 2;
    for(ShTreeNode *child : def->nodes)
    {
        members << get_shared_var_templatedata(*child, local_index++, ctx);
    }
    cur_definition.set("members", members);
    return cur_definition;
}

} } // namespace inexor::gluegen