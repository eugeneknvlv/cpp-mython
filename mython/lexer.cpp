#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std;

namespace parse {

    bool operator==(const Token& lhs, const Token& rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token& lhs, const Token& rhs) {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& os, const Token& rhs) {
        using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    Lexer::Lexer(std::istream& input)
        : input_(input)
        , curr_indent_(0)
        , curr_line_(""s)
        , first_line_(true)
        , new_line_(true)
        , dedented_before_eof_(false)
    {
        // Реализуйте конструктор самостоятельно
        curr_token_ = NextToken();
    }

    const Token& Lexer::CurrentToken() const {
        // Заглушка. Реализуйте метод самостоятельно
        return curr_token_;
        throw std::logic_error("Not implemented"s);
    }

    Token Lexer::NextToken() {
        if (curr_line_.size() == 0) {
            if (!getline(input_, curr_line_)) {
                if (curr_indent_ == 0) {
                    dedented_before_eof_ = true;
                }

                if (curr_token_ != token_type::Eof() &&
                    curr_token_ != token_type::Newline() &&
                    curr_token_ != token_type::Dedent() &&
                    !first_line_)
                {
                    curr_token_ = token_type::Newline();
                }
                else if (!dedented_before_eof_) {
                    curr_indent_ -= 2;
                    if (curr_indent_ == 0) {
                        dedented_before_eof_ = true;
                    }
                    curr_token_ = token_type::Dedent();
                }
                else {
                    curr_token_ = token_type::Eof();
                }
                return curr_token_;
            }
            else {
                if (curr_line_.find_first_not_of(' ') == curr_line_.npos) {
                    curr_line_.clear();
                    return NextToken();
                }
                if (curr_line_[0] == '#') {
                    curr_line_.clear();
                    return NextToken();
                }
                if (!first_line_) {
                    curr_token_ = token_type::Newline();
                    new_line_ = true;
                    return curr_token_;
                }
                else {
                    first_line_ = false;
                }
                new_line_ = true;
                curr_line_ += ' ';
            }
        }

        size_t first_symb_pos = curr_line_.find_first_not_of(' ');
        if (first_symb_pos == curr_line_.npos) {
            curr_line_.clear();
            return NextToken();
        }
        if (new_line_) {
            if (first_symb_pos > curr_indent_) {
                curr_token_ = token_type::Indent();
                curr_indent_ += 2;
                return curr_token_;
            }
            if (first_symb_pos < curr_indent_) {
                curr_token_ = token_type::Dedent();
                curr_indent_ -= 2;
                return curr_token_;
            }
            new_line_ = false;
        }

        curr_line_ = curr_line_.substr(curr_line_.find_first_not_of(' '));
        if (curr_line_[0] == '\'' || curr_line_[0] == '\"') {
            string str = ParseString();
            curr_token_ = token_type::String(str);
            curr_line_.erase(0, str.size() + 1/*quote*/ + 1/*space*/);
            return curr_token_;
        }

        size_t lexem_end =
            min({
                curr_line_.find(' '),
                curr_line_.find('('),
                curr_line_.find(')'),
                curr_line_.find(','),
                curr_line_.find(':'),
                curr_line_.find('.'),
                curr_line_.find('#'),
                curr_line_.find('+'),
                curr_line_.find('-'),
                curr_line_.find('*'),
                curr_line_.find('/')
                });
        string lexem = curr_line_.substr(0, lexem_end == 0 ? 1 : lexem_end);
        bool lexem_is_char = false;
        if (lexem == "class"s) {
            curr_token_ = token_type::Class();
        }
        else if (lexem == "return"s) {
            curr_token_ = token_type::Return();
        }
        else if (lexem == "if"s) {
            curr_token_ = token_type::If();
        }
        else if (lexem == "else"s) {
            curr_token_ = token_type::Else();
        }
        else if (lexem == "def"s) {
            curr_token_ = token_type::Def();
        }
        else if (lexem == "print"s) {
            curr_token_ = token_type::Print();
        }
        else if (lexem == "and"s) {
            curr_token_ = token_type::And();
        }
        else if (lexem == "or"s) {
            curr_token_ = token_type::Or();
        }
        else if (lexem == "not"s) {
            curr_token_ = token_type::Not();
        }
        else if (lexem == "<="s) {
            curr_token_ = token_type::LessOrEq();
        }
        else if (lexem == ">="s) {
            curr_token_ = token_type::GreaterOrEq();
        }
        else if (lexem == "=="s) {
            curr_token_ = token_type::Eq();
        }
        else if (lexem == "!=") {
            curr_token_ = token_type::NotEq();
        }
        else if (lexem == "None"s) {
            curr_token_ = token_type::None();
        }
        else if (lexem == "True"s) {
            curr_token_ = token_type::True();
        }
        else if (lexem == "False"s) {
            curr_token_ = token_type::False();
        }
        else if (lexem.find_first_not_of("0123456789"s) == lexem.npos) {
            curr_token_ = token_type::Number(stoi(lexem));
        }
        else if (lexem.find_first_not_of(admissible_id_symbols) == lexem.npos && string("0123456789").find(lexem[0]) == string::npos) {
            curr_token_ = token_type::Id(lexem);
        }
        else if (lexem[0] == '#') {
            curr_line_.clear();
            return NextToken();
        }
        else {
            curr_token_ = token_type::Char(lexem[0]);
            lexem_is_char = true;
        }

        if (lexem_is_char) {
            curr_line_.erase(0, 1);
        }
        else {
            curr_line_.erase(0, lexem.size());
        }

        return curr_token_;
    }

    std::string Lexer::ParseString() {
        using namespace std::literals;
        std::string result = ""s;
        char opening_quote = curr_line_[0];
        curr_line_ = curr_line_.substr(1);
        bool escape_seq = false;
        for (char c : curr_line_) {
            if (c == '\\') {
                escape_seq = true;
                continue;
            }

            if (c == opening_quote) {
                if (escape_seq) {
                    result += opening_quote;
                }
                else {
                    return result;
                }
            }
            else if (c == 't' && escape_seq) {
                result += '\t';
            }
            else if (c == 'n' && escape_seq) {
                result += '\n';
            }
            else {
                result += c;
            }
            escape_seq = false;
        }
        return result;
    }

}  // namespace parse