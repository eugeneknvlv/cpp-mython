#pragma once

#include <iosfwd>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>

namespace parse {

    using namespace std::literals;
    const std::string admissible_id_symbols =
        "abcdefghijklmnopqrstuvwxyz"s +
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"s +
        "0123456789"s +
        "_"s;

    namespace token_type {
        struct Number {  // ������� ������
            Number() = default;
            Number(int value)
                : value(value)
            {}

            int value;   // �����
        };

        struct Id {             // ������� ��������������
            Id() = default;
            Id(const std::string& value)
                : value(value)
            {}

            std::string value;  // ��� ��������������
        };

        struct Char {    // ������� �������
            Char() = default;
            Char(char value)
                : value(value)
            {}
            char value;  // ��� �������
        };

        struct String {  // ������� ���������� ���������
            String() = default;
            String(const std::string& value)
                : value(value)
            {}
            std::string value;
        };

        struct Class {};    // ������� �class�
        struct Return {};   // ������� �return�
        struct If {};       // ������� �if�
        struct Else {};     // ������� �else�
        struct Def {};      // ������� �def�
        struct Newline {};  // ������� ������ ������
        struct Print {};    // ������� �print�
        struct Indent {};  // ������� ����������� �������, ������������� ���� ��������
        struct Dedent {};  // ������� ����������� �������
        struct Eof {};     // ������� ������ �����
        struct And {};     // ������� �and�
        struct Or {};      // ������� �or�
        struct Not {};     // ������� �not�
        struct Eq {};      // ������� �==�
        struct NotEq {};   // ������� �!=�
        struct LessOrEq {};     // ������� �<=�
        struct GreaterOrEq {};  // ������� �>=�
        struct None {};         // ������� �None�
        struct True {};         // ������� �True�
        struct False {};        // ������� �False�
    }  // namespace token_type

    using TokenBase
        = std::variant<token_type::Number, token_type::Id, token_type::Char, token_type::String,
        token_type::Class, token_type::Return, token_type::If, token_type::Else,
        token_type::Def, token_type::Newline, token_type::Print, token_type::Indent,
        token_type::Dedent, token_type::And, token_type::Or, token_type::Not,
        token_type::Eq, token_type::NotEq, token_type::LessOrEq, token_type::GreaterOrEq,
        token_type::None, token_type::True, token_type::False, token_type::Eof>;

    struct Token : TokenBase {
        using TokenBase::TokenBase;

        template <typename T>
        [[nodiscard]] bool Is() const {
            return std::holds_alternative<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T& As() const {
            return std::get<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T* TryAs() const {
            return std::get_if<T>(this);
        }
    };

    bool operator==(const Token& lhs, const Token& rhs);
    bool operator!=(const Token& lhs, const Token& rhs);

    std::ostream& operator<<(std::ostream& os, const Token& rhs);

    class LexerError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    const std::unordered_map<std::string, Token> str_to_token =
    {
        {"class"s, token_type::Class()},
        {"return"s, token_type::Return()},
        {"if"s, token_type::If()},
        {"else"s, token_type::Else()},
        {"def"s, token_type::Def()},
        {"print"s, token_type::Print()},
        {"and"s, token_type::And()},
        {"or"s, token_type::Or()},
        {"not"s, token_type::Not()},
        {"<="s, token_type::LessOrEq()},
        {">="s, token_type::GreaterOrEq()},
        {"=="s, token_type::Eq()},
        {"!="s, token_type::NotEq()},
        {"None", token_type::None()},
        {"True"s, token_type::True()},
        {"False"s, token_type::False()}
    };

    class Lexer {
    public:
        explicit Lexer(std::istream& input);

        // ���������� ������ �� ������� ����� ��� token_type::Eof, ���� ����� ������� ����������
        [[nodiscard]] const Token& CurrentToken() const;

        // ���������� ��������� �����, ���� token_type::Eof, ���� ����� ������� ����������
        Token NextToken();

        // ���� ������� ����� ����� ��� T, ����� ���������� ������ �� ����.
        // � ��������� ������ ����� ����������� ���������� LexerError
        template <typename T>
        const T& Expect() const {
            if (!curr_token_.Is<T>()) {
                throw LexerError("Not implemented"s);
            }
            return curr_token_.As<T>();
        }

        // ����� ���������, ��� ������� ����� ����� ��� T, � ��� ����� �������� �������� value.
        // � ��������� ������ ����� ����������� ���������� LexerError
        template <typename T, typename U>
        void Expect(const U& value) const {
            if (!curr_token_.Is<T>()) {
                throw LexerError("Not implemented"s);
            }
            else if (curr_token_.As<T>().value != value) {
                throw LexerError("Not implemented"s);
            }
        }

        // ���� ��������� ����� ����� ��� T, ����� ���������� ������ �� ����.
        // � ��������� ������ ����� ����������� ���������� LexerError
        template <typename T>
        const T& ExpectNext() {
            curr_token_ = NextToken();
            if (!curr_token_.Is<T>()) {
                throw LexerError("Not implemented"s);
            }
            return curr_token_.As<T>();
        }

        // ����� ���������, ��� ��������� ����� ����� ��� T, � ��� ����� �������� �������� value.
        // � ��������� ������ ����� ����������� ���������� LexerError
        template <typename T, typename U>
        void ExpectNext(const U& value) {
            curr_token_ = NextToken();
            if (!curr_token_.Is<T>()) {
                throw LexerError("Not implemented"s);
            }
            else if (curr_token_.As<T>().value != value) {
                throw LexerError("Not implemented"s);
            }
        }

    private:
        // ���������� ��������� ����� ��������������
        std::istream& input_;
        Token curr_token_;
        size_t curr_indent_;
        std::string curr_line_;
        bool first_line_;
        bool processed_indents_dedents_;
        bool dedented_before_eof_;

        void ProcessNoMoreInput();
        bool CurrLineIsCommentedOrEmpty();
        void ProcessEmptyOrCommentedLine();
        std::string GetNewLexem();
        void ProcessString();
        void ProcessSingleIndentDedent();
        bool LexemIsId(const std::string& lexem);
        bool LexemIsNumber(const std::string& lexem);
        bool LexemIsCommented(const std::string& lexem);
        std::string ParseString();
    };
}  // namespace parse