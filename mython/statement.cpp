#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        const string ADD_METHOD = "__add__"s;
        const string INIT_METHOD = "__init__"s;
    }  // namespace

    ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
        closure[var_] = rv_->Execute(closure, context);
        return closure[var_];
    }

    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
        : var_(move(var))
        , rv_(move(rv))
    {}

    VariableValue::VariableValue(const std::string& var_name)
        : var_name_(var_name)
        , dotted_ids_(0)
    {}

    VariableValue::VariableValue(std::vector<std::string> dotted_ids)
        : var_name_(""s)
        , dotted_ids_(move(dotted_ids))
    {}

    ObjectHolder VariableValue::Execute(Closure& closure, [[maybe_unused]] Context& context) {
        if (!var_name_.empty() && closure.count(var_name_)) {
            return closure.at(var_name_);
        }
        else if (!dotted_ids_.empty()) {
            if (dotted_ids_.size() == 1) {
                return closure.at(dotted_ids_[0]);
            }
            Closure tmp_closure;
            for (size_t i = 0; i < dotted_ids_.size() - 1; ++i) {
                string id = dotted_ids_[i];
                tmp_closure = closure.at(id).TryAs<runtime::ClassInstance>()->Fields();
            }
            return tmp_closure[dotted_ids_.back()];
        }
        else {
            throw runtime_error("Variable error"s);
        }
    }

    unique_ptr<Print> Print::Variable(const std::string& name) {
        return make_unique<Print>(make_unique<VariableValue>(VariableValue(name)));
    }

    Print::Print(unique_ptr<Statement> argument)
        : statements_(1)
    {
        statements_[0] = move(argument);
    }

    Print::Print(vector<unique_ptr<Statement>> args)
        : statements_(move(args))
    {}

    ObjectHolder Print::Execute(Closure& closure, Context& context) {
        ostream& os = context.GetOutputStream();
        ObjectHolder result;
        bool first = true;
        for (const auto& st : statements_) {
            if (!first) {
                os << ' ';
            }
            first = false;

            result = st->Execute(closure, context);
            if (!result.Get()) {
                os << "None"s;
            }
            else {
                result.Get()->Print(os, context);
            }
        }
        os << '\n';
        return result;
    }

    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
        std::vector<std::unique_ptr<Statement>> args)
        : object_(move(object))
        , method_(move(method))
        , args_(move(args))
    {}

    ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
        ObjectHolder obj = object_->Execute(closure, context);
        runtime::ClassInstance* cls_inst_ptr = obj.TryAs<runtime::ClassInstance>();

        vector<ObjectHolder> actual_args;
        for (const auto& arg : args_) {
            actual_args.push_back(move(arg->Execute(closure, context)));
        }

        return cls_inst_ptr->Call(method_, actual_args, context);
    }

    ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
        ObjectHolder obj = arg_->Execute(closure, context);
        if (!obj) {
            return ObjectHolder::Own(runtime::String{ "None"s });
        }

        stringstream ss;
        runtime::SimpleContext ctx(ss);

        obj->Print(ss, context);
        return ObjectHolder::Own(runtime::String{ ss.str() });
    }

    ObjectHolder Add::Execute(Closure& closure, Context& context) {
        ObjectHolder lhs = lhs_->Execute(closure, context);
        ObjectHolder rhs = rhs_->Execute(closure, context);

        runtime::Number* lhs_num = lhs.TryAs<runtime::Number>();
        runtime::String* lhs_string = lhs.TryAs<runtime::String>();
        runtime::ClassInstance* lhs_cls_inst = lhs.TryAs<runtime::ClassInstance>();

        if (lhs_num) {
            runtime::Number* rhs_num = rhs.TryAs<runtime::Number>();
            if (!rhs_num) {
                throw runtime_error("Can't Add different types"s);
            }
            return ObjectHolder::Own(runtime::Number{ lhs_num->GetValue() + rhs_num->GetValue() });
        }
        else if (lhs_string) {
            runtime::String* rhs_string = rhs.TryAs<runtime::String>();
            if (!rhs_string) {
                throw runtime_error("Can't Add different types"s);
            }
            return ObjectHolder::Own(runtime::String{ string(lhs_string->GetValue()) + string(rhs_string->GetValue()) });
        }
        else if (lhs_cls_inst) {
            return lhs_cls_inst->Call(ADD_METHOD, { rhs_->Execute(closure, context) }, context);
        }

        throw runtime_error("Addition error"s);
    }

    ObjectHolder Sub::Execute(Closure& closure, Context& context) {
        ObjectHolder lhs = lhs_->Execute(closure, context);
        ObjectHolder rhs = rhs_->Execute(closure, context);

        runtime::Number* lhs_num = lhs.TryAs<runtime::Number>();
        runtime::Number* rhs_num = rhs.TryAs<runtime::Number>();
        if (!lhs_num || !rhs_num) {
            throw runtime_error("Only numbers can be substracted"s);
        }

        return ObjectHolder::Own(runtime::Number{ lhs_num->GetValue() - rhs_num->GetValue() });
    }

    ObjectHolder Mult::Execute(Closure& closure, Context& context) {
        ObjectHolder lhs = lhs_->Execute(closure, context);
        ObjectHolder rhs = rhs_->Execute(closure, context);

        runtime::Number* lhs_num = lhs.TryAs<runtime::Number>();
        runtime::Number* rhs_num = rhs.TryAs<runtime::Number>();
        if (!lhs_num || !rhs_num) {
            throw runtime_error("Only numbers can be multiplied"s);
        }

        return ObjectHolder::Own(runtime::Number{ lhs_num->GetValue() * rhs_num->GetValue() });
    }

    ObjectHolder Div::Execute(Closure& closure, Context& context) {
        ObjectHolder lhs = lhs_->Execute(closure, context);
        ObjectHolder rhs = rhs_->Execute(closure, context);

        runtime::Number* lhs_num = lhs.TryAs<runtime::Number>();
        runtime::Number* rhs_num = rhs.TryAs<runtime::Number>();
        if (!lhs_num || !rhs_num) {
            throw runtime_error("Only numbers can be divided"s);
        }
        if (rhs_num->GetValue() == 0) {
            throw runtime_error("Zero division"s);
        }

        return ObjectHolder::Own(runtime::Number{ lhs_num->GetValue() / rhs_num->GetValue() });
    }

    ObjectHolder Compound::Execute(Closure& closure, Context& context) {
        for (const auto& stmt : statements_) {
            stmt->Execute(closure, context);
        }
        return {};
    }

    ObjectHolder Return::Execute(Closure& closure, Context& context) { // TODO
        ObjectHolder res = statement_->Execute(closure, context);
        closure["return"s] = res;
        throw runtime_error("executing return statement"s);
        return {};
    }

    ClassDefinition::ClassDefinition(ObjectHolder cls)
        : cls_(cls)
    {}

    ObjectHolder ClassDefinition::Execute(Closure& closure, [[maybe_unused]] Context& context) {
        closure[cls_.TryAs<runtime::Class>()->GetName()] = cls_;
        return {};
    }

    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
        std::unique_ptr<Statement> rv)
        : obj_(move(object))
        , field_name_(move(field_name))
        , rv_(move(rv))
    {}

    ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
        ObjectHolder obj_holder = obj_.Execute(closure, context);
        runtime::ClassInstance* cls_inst = obj_holder.TryAs<runtime::ClassInstance>();
        ObjectHolder rv_res = rv_->Execute(closure, context);
        cls_inst->Fields()[field_name_] = rv_res;
        return cls_inst->Fields()[field_name_];
    }

    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
        std::unique_ptr<Statement> else_body)
        : cond_(move(condition))
        , if_(move(if_body))
        , else_(move(else_body))
    {}

    ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
        if (runtime::IsTrue(cond_->Execute(closure, context))) {
            return if_->Execute(closure, context);
        }
        else if (else_.get()) {
            return else_->Execute(closure, context);
        }
        return {};
    }

    ObjectHolder Or::Execute(Closure& closure, Context& context) {
        if (!runtime::IsTrue(lhs_->Execute(closure, context))) {
            return ObjectHolder::Own(runtime::Bool{ runtime::IsTrue(rhs_->Execute(closure, context)) });
        }
        return ObjectHolder::Own(runtime::Bool{ true });
    }

    ObjectHolder And::Execute(Closure& closure, Context& context) {
        if (runtime::IsTrue(lhs_->Execute(closure, context))) {
            return ObjectHolder::Own(runtime::Bool{ runtime::IsTrue(rhs_->Execute(closure, context)) });
        }
        return ObjectHolder::Own(runtime::Bool{ false });
    }

    ObjectHolder Not::Execute(Closure& closure, Context& context) {
        return ObjectHolder::Own(runtime::Bool{ !IsTrue(arg_->Execute(closure, context)) });
    }

    Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
        : BinaryOperation(std::move(lhs), std::move(rhs))
        , cmp_(cmp)
    {}

    ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
        ObjectHolder lhs_res = lhs_->Execute(closure, context);
        ObjectHolder rhs_res = rhs_->Execute(closure, context);
        return ObjectHolder::Own(runtime::Bool{ cmp_(lhs_res, rhs_res, context) });
    }

    NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args)
        : cls_(class_)
        , args_(move(args))
    {}

    NewInstance::NewInstance(const runtime::Class& class_) 
        : cls_(class_)
        , args_(0)
    {}

    ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
        ObjectHolder cls_inst_OH = ObjectHolder::Own(runtime::ClassInstance(cls_));
        runtime::ClassInstance* cls_inst_ptr_ = cls_inst_OH.TryAs<runtime::ClassInstance>();
        if (cls_inst_ptr_->HasMethod(INIT_METHOD, args_.size())) {
            vector<ObjectHolder> actual_args;
            for (const auto& stmt : args_) {
                actual_args.push_back(stmt->Execute(closure, context));
            }
            cls_inst_ptr_->Call(INIT_METHOD, actual_args, context);
        }
        return cls_inst_OH;
    }

    MethodBody::MethodBody(std::unique_ptr<Statement>&& body) 
        : body_(move(body))
    {}

    ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
        try {
            ObjectHolder res = body_->Execute(closure, context);
            return {};
        }
        catch (const runtime_error& e) {
            if (string(e.what()) == "executing return statement"s) {
                ObjectHolder value_to_ret = closure.at("return"s);
                closure.erase("return"s);
                return value_to_ret;
            }
            else {
                throw;
            }
        }
    }

}  // namespace ast