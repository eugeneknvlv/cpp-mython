#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>
#include <algorithm>

using namespace std;

namespace runtime {

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
        : data_(std::move(data)) {
    }

    void ObjectHolder::AssertIsValid() const {
        assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object& object) {
        // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
        return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
    }

    ObjectHolder ObjectHolder::None() {
        return ObjectHolder();
    }

    Object& ObjectHolder::operator*() const {
        AssertIsValid();
        return *Get();
    }

    Object* ObjectHolder::operator->() const {
        AssertIsValid();
        return Get();
    }

    Object* ObjectHolder::Get() const {
        return data_.get();
    }

    ObjectHolder::operator bool() const {
        return Get() != nullptr;
    }

    bool IsTrue(const ObjectHolder& object) {
        if (!object) {
            return false;
        }
        DummyContext context;

        Number* obj_num = object.TryAs<Number>();
        String* obj_str = object.TryAs<String>();
        Bool* obj_boolean = object.TryAs<Bool>();
        if (obj_num) {
            return obj_num->GetValue() != 0;
        }
        else if (obj_str) {
            return obj_str->GetValue() != ""s;
        }
        else if (obj_boolean) {
            return obj_boolean->GetValue() != false;
        }

        return false;
    }

    void ClassInstance::Print(std::ostream& os, Context& context) {
        if (HasMethod("__str__"s, 0)) {
            ObjectHolder str_result = Call("__str__"s, {}, context);
            str_result->Print(os, context);
        }
        else {
            os << this;
        }
    }

    bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
        const Method* needed = cls_.GetMethod(method);
        if (needed) {
            if ((needed->formal_params).size() == argument_count) {
                return true;
            }
        }
        return false;
    }

    Closure& ClassInstance::Fields() {
        return closure_;
    }

    const Closure& ClassInstance::Fields() const {
        return closure_;
    }

    ClassInstance::ClassInstance(const Class& cls)
        : cls_(cls)
        , closure_({})
    {
        closure_["self"s] = ObjectHolder::Share(*this);
    }

    ObjectHolder ClassInstance::Call(const std::string& method,
        const std::vector<ObjectHolder>& actual_args,
        Context& context)
    {
        if (!HasMethod(method, actual_args.size())) {
            throw std::runtime_error("Not implemented"s);
        }

        Closure method_closure{};
        method_closure["self"s] = ObjectHolder::Share(*this);
        const Method* m = cls_.GetMethod(method);
        size_t idx = 0;
        for (const string& arg_name : m->formal_params) {
            method_closure[arg_name] = actual_args[idx++];
        }

        ObjectHolder result = m->body->Execute(method_closure, context);
        return result;
    }

    Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
        : name_(move(name))
        , methods_(move(methods))
        , parent_ptr_(parent)
    {
    }

    optional<vector<Method>::const_iterator> Class::FindMethod(const string& name) const {
        auto needed = find_if(methods_.begin(), methods_.end(),
            [&name](const Method& method) {
                return method.name == name;
            });
        if (needed == methods_.end()) {
            return nullopt;
        }
        return needed;
    }

    const Class* Class::GetParent() const {
        return parent_ptr_;
    }

    const Method* Class::GetMethod(const std::string& name) const {
        auto needed = FindMethod(name);
        if (!needed.has_value()) {
            const Class* parent_ptr = parent_ptr_;
            while (parent_ptr) {
                needed = parent_ptr->FindMethod(name);
                if (needed.has_value()) {
                    vector<Method>::const_iterator it = *needed;
                    return &(*it);
                }
                parent_ptr = parent_ptr->GetParent();
            }
        }
        else {
            vector<Method>::const_iterator it = *needed;
            return &(*it);
        }
        return nullptr;
    }

    [[nodiscard]] const std::string& Class::GetName() const {
        return name_;
    }

    void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
        os << "Class "s << name_;
    }

    void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
        os << (GetValue() ? "True"sv : "False"sv);
    }

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        if (!lhs) {
            if (!rhs) {
                return true;
            }
            throw std::runtime_error("Cannot compare objects for equality"s);
        }
        if (!rhs) {
            if (!lhs) {
                return true;
            }
            throw std::runtime_error("Cannot compare objects for equality"s);
        }

        Number* lhs_num = lhs.TryAs<Number>();
        String* lhs_str = lhs.TryAs<String>();
        Bool* lhs_boolean = lhs.TryAs<Bool>();
        ClassInstance* lhs_class_inst = lhs.TryAs<ClassInstance>();
        if (lhs_num) {
            Number* rhs_num = rhs.TryAs<Number>();
            if (!rhs_num) {
                throw std::runtime_error("Cannot compare objects for equality"s);
            }
            return lhs_num->GetValue() == rhs_num->GetValue();
        }
        else if (lhs_str) {
            String* rhs_str = rhs.TryAs<String>();
            if (!rhs_str) {
                throw std::runtime_error("Cannot compare objects for equality"s);
            }
            return lhs_str->GetValue() == rhs_str->GetValue();
        }
        else if (lhs_boolean) {
            Bool* rhs_boolean = rhs.TryAs<Bool>();
            if (!rhs_boolean) {
                throw std::runtime_error("Cannot compare objects for equality"s);
            }
            return lhs_boolean->GetValue() == rhs_boolean->GetValue();
        }
        else if (lhs_class_inst) {
            if (!lhs_class_inst->HasMethod("__eq__"s, 1)) {
                throw std::runtime_error("Cannot compare objects for equality"s);
            }

            ClassInstance* rhs_class_inst = rhs.TryAs<ClassInstance>();
            if (!rhs_class_inst) {
                throw std::runtime_error("Cannot compare objects for equality"s);
            }

            DummyContext context;
            return IsTrue(lhs_class_inst->Call("__eq__"s, { ObjectHolder::Share(*rhs_class_inst) }, context));
        }
        return false;
    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, [[maybe_unused]] Context& context) {
        if (!lhs || !rhs) {
            throw std::runtime_error("Cannot compare objects for equality"s);
        }
        Number* lhs_num = lhs.TryAs<Number>();
        String* lhs_str = lhs.TryAs<String>();
        Bool* lhs_boolean = lhs.TryAs<Bool>();
        ClassInstance* lhs_class_inst = lhs.TryAs<ClassInstance>();
        if (lhs_num) {
            Number* rhs_num = rhs.TryAs<Number>();
            if (!rhs_num) {
                throw std::runtime_error("Cannot compare objects for equality"s);
            }
            return lhs_num->GetValue() < rhs_num->GetValue();
        }
        else if (lhs_str) {
            String* rhs_str = rhs.TryAs<String>();
            if (!rhs_str) {
                throw std::runtime_error("Cannot compare objects for equality"s);
            }
            return lhs_str->GetValue() < rhs_str->GetValue();
        }
        else if (lhs_boolean) {
            Bool* rhs_boolean = rhs.TryAs<Bool>();
            if (!rhs_boolean) {
                throw std::runtime_error("Cannot compare objects for equality"s);
            }
            return lhs_boolean->GetValue() < rhs_boolean->GetValue();
        }
        else if (lhs_class_inst) {
            if (!lhs_class_inst->HasMethod("__eq__"s, 1)) {
                throw std::runtime_error("Cannot compare objects for equality"s);
            }

            ClassInstance* rhs_class_inst = rhs.TryAs<ClassInstance>();
            if (!rhs_class_inst) {
                throw std::runtime_error("Cannot compare objects for equality"s);
            }

            DummyContext context;
            return IsTrue(lhs_class_inst->Call("__lt__"s, { ObjectHolder::Share(*rhs_class_inst) }, context));
        }
        return false;
    }

    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
        throw std::runtime_error("Cannot compare objects for equality"s);
    }

    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Greater(lhs, rhs, context);
        throw std::runtime_error("Cannot compare objects for equality"s);
    }

    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Less(lhs, rhs, context);
        throw std::runtime_error("Cannot compare objects for equality"s);
    }

}  // namespace runtime