// Copyright (c) 2018 Karl-Johan Alm
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef included_tiny_parser_h_
#define included_tiny_parser_h_

namespace tiny {

enum token_type {
    tok_undef,
    tok_symbol,    // variable, function name, ...
    tok_number,
    tok_equal,
    tok_lparen,
    tok_rparen,
    tok_string,
    tok_mul,
    tok_plus,
    tok_minus,
    tok_div,
    tok_concat,
    tok_comma,
    tok_hex,
    tok_bin,
    tok_consumable, // consumed by fulfilled token sequences
    tok_ws,
};

static const char* token_type_str[] = {
    "???",
    "symbol",
    "number",
    "equal",
    "lparen",
    "rparen",
    "string",
    "mul",
    "plus",
    "minus",
    "div",
    "concat",
    "comma",
    "hex",
    "bin",
    "consumable",
    "ws",
};

struct token_t {
    token_type token = tok_undef;
    char* value = nullptr;
    token_t* next = nullptr;
    token_t(token_type token_in, token_t* prev) : token(token_in) {
        if (prev) prev->next = this;
    }
    ~token_t() { if (value) free(value); if (next) delete next; }
    void print() {
        printf("[%s %s]\n", token_type_str[token], value ?: "<null>");
        if (next) next->print();
    }
};

inline token_type determine_token(const char c, const char p, token_type restrict_type, token_type current) {
    if (c == '|') return p == '|' ? tok_concat : tok_consumable;
    if (c == '+') return tok_plus;
    if (c == '-') return tok_minus;
    if (c == '*') return tok_mul;
    if (c == '/') return tok_div;
    if (c == ',') return tok_comma;
    if (c == '=') return tok_equal;
    if (c == ')') return tok_rparen;
    if (c == ' ' || c == '\t' || c == '\n') return tok_ws;
    if (restrict_type != tok_undef) {
        switch (restrict_type) {
        case tok_hex:
            if ((c >= '0' && c <= '9') ||
                (c >= 'a' && c <= 'f') ||
                (c >= 'A' && c <= 'F')) return tok_number;
            break;
        case tok_bin:
            if (c == '0' || c == '1') return tok_number;
            break;
        default: break;
        }
        return tok_undef;
    }

    if (c == 'x' && p == '0' && current == tok_number) return tok_hex;
    if (c == 'b' && p == '0' && current == tok_number) return tok_bin;
    if (c >= '0' && c <= '9') return current == tok_symbol ? tok_symbol : tok_number;
    if (current == tok_number &&
        ((c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F'))) return tok_number; // hexadecimal
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '_') return tok_symbol;
    if (c == '"') return tok_string;
    if (c == '(') return tok_lparen;
    return tok_undef;
}

token_t* tokenize(const char* s) {
    token_t* head = nullptr;
    token_t* tail = nullptr;
    token_t* prev = nullptr;
    bool open = false;
    bool finalized = true;
    char finding = 0;
    token_type restrict_type = tok_undef;
    size_t token_start = 0;
    size_t i;
    for (i = 0; s[i]; ++i) {
        // if we are finding a character, keep reading until we find it
        if (finding) {
            if (s[i] != finding) continue;
            finding = 0;
            open = false;
            continue; // we move one extra step, or "foo" will be read in as "foo
        }
        auto token = determine_token(s[i], i ? s[i-1] : 0, restrict_type, tail ? tail->token : tok_undef);
        // printf("token = %s\n", token_type_str[token]);
        if (token == tok_consumable && tail->token == tok_consumable) {
            throw std::runtime_error(strprintf("tokenization failure at character '%c'", s[i]));
            delete head;
            return nullptr;
        }
        if ((token == tok_hex || token == tok_bin) && tail->token == tok_number) tail->token = tok_consumable;
        // if whitespace, close
        if (token == tok_ws) {
            open = false;
            restrict_type = tok_undef;
        }
        // if open, see if it stays open
        if (open) {
            open = token == tail->token;
        }
        if (!open) {
            if (tail && tail->token == tok_consumable) {
                if (token == tok_hex || token == tok_bin) {
                    restrict_type = token;
                    delete tail;
                    if (head == tail) head = prev;
                    tail = prev;
                }
            } else if (!finalized) {
                tail->value = strndup(&s[token_start], i-token_start);
                finalized = true;
            }
            switch (token) {
            case tok_string:
                finding = '"';
            case tok_symbol:
            case tok_number:
            case tok_consumable:
                prev = tail;
                finalized = false;
                token_start = i;
                tail = new token_t(token, tail);
                if (!head) head = tail;
                open = true;
                break;
            case tok_equal:
            case tok_lparen:
            case tok_rparen:
            case tok_mul:
            case tok_plus:
            case tok_minus:
            case tok_concat:
            case tok_comma:
            case tok_div:
            case tok_hex:
            case tok_bin:
                if (tail->token == tok_consumable) {
                    delete tail;
                    if (head == tail) head = prev;
                    tail = prev;
                }
                tail = new token_t(token, tail);
                tail->value = strndup(&s[i], 1 /* misses 1 char for concat/hex/bin, but irrelevant */);
                if (!head) head = tail;
                break;
            case tok_ws:
                break;
            case tok_undef:
                throw std::runtime_error(strprintf("tokenization failure at character '%c'", s[i]));
                delete head;
                return nullptr;
            }
        }
        // for (auto x = head; x; x = x->next) printf(" %s", token_type_str[x->token]); printf("\n");
    }
    if (!finalized) {
        tail->value = strndup(&s[token_start], i-token_start);
        finalized = true;
    }
    return head;
}

typedef size_t ref;
static const ref nullref = 0;

struct st_callback_table {
    virtual ref  load(const std::string& variable) = 0;
    virtual void save(const std::string& variable, ref value) = 0;
    virtual ref  bin(token_type op, ref lhs, ref rhs) = 0;
    virtual ref  unary(token_type op, ref val) = 0;
    virtual ref  fcall(const std::string& fname, int argc, ref* argv) = 0;
    virtual ref  convert(const std::string& value, token_type type, token_type restriction) = 0;
};

struct st_t {
    virtual void print() {
        printf("????");
    }
    virtual ref eval(st_callback_table* ct) {
        return nullref;
    }
};

struct var_t: public st_t {
    std::string varname;
    var_t(const std::string& varname_in) : varname(varname_in) {}
    void print() override {
        printf("%s", varname.c_str());
    }
    virtual ref eval(st_callback_table* ct) override {
        return ct->load(varname);
    }
};

struct value_t: public st_t {
    token_type type; // tok_number, tok_string, tok_symbol
    token_type restriction; // tok_hex, tok_bin, tok_undef
    std::string value;
    value_t(token_type type_in, const std::string& value_in, token_type restriction_in) : type(type_in), restriction(restriction_in), value(value_in) {
        if (type == tok_string) {
            // get rid of quotes
            value = value.substr(1, value.length() - 2);
        }
    }
    void print() override {
        printf("%s:%s", token_type_str[type], value.c_str());
    }
    virtual ref eval(st_callback_table* ct) override {
        return ct->convert(value, type, restriction);
    }
};

struct set_t: public st_t {
    std::string varname;
    st_t* value;
    set_t(const std::string& varname_in, st_t* value_in) : varname(varname_in), value(value_in) {}
    void print() override {
        printf("%s = ", varname.c_str());
        value->print();
    }
    virtual ref eval(st_callback_table* ct) override {
        ct->save(varname, value->eval(ct));
        return nullref;
    }
};

struct list_t: public st_t {
    ref* listref;
    std::vector<st_t*> values;
    list_t(const std::vector<st_t*>& values_in) : values(values_in) {
        listref = (ref*)malloc(sizeof(ref) * values.size());
    }
    ~list_t() {
        free(listref);
    }
    void print() override {
        printf("[");
        for (size_t i = 0; i < values.size(); ++i) {
            printf("%s", i ? ", " : "");
            values[i]->print();
        }
        printf("]");
    }
    virtual ref eval(st_callback_table* ct) override {
        throw std::runtime_error("list_t cannot be evaluated directly; use the list_eval method");
    }
    ref* list_eval(st_callback_table* ct) {
        for (size_t i = 0; i < values.size(); ++i) {
            listref[i] = values[i]->eval(ct);
        }
        return listref;
    }
};

struct call_t: public st_t {
    std::string fname;
    list_t* args;
    call_t(const std::string& fname_in, list_t* args_in) : fname(fname_in), args(args_in) {}
    void print() override {
        printf("%s(", fname.c_str());
        args->print();
        printf(")");
    }
    virtual ref eval(st_callback_table* ct) override {
        ref* list = (ref*)args->list_eval(ct);
        // ref ca[1];
        // ca[0] = args->eval(ct);
        return ct->fcall(fname, args->values.size(), list);
    }
};

struct bin_t: public st_t {
    token_type op_token;
    st_t* lhs;
    st_t* rhs;
    bin_t(token_type op_token_in, st_t* lhs_in, st_t* rhs_in) : op_token(op_token_in), lhs(lhs_in), rhs(rhs_in) {}
    ~bin_t() {
        delete lhs;
        delete rhs;
    }
    void print() override {
        printf("(tok_bin %s ", token_type_str[op_token]);
        lhs->print();
        printf(" ");
        rhs->print();
        printf(")");
    }
    virtual ref eval(st_callback_table* ct) override {
        return ct->bin(op_token, lhs->eval(ct), rhs->eval(ct));
    }
};

st_t* x;
#define try(parser) x = parser(s); if (x) return x

st_t* parse_variable(token_t** s) {
    if ((*s)->token == tok_symbol) {
        var_t* t = new var_t((*s)->value);
        *s = (*s)->next;
        return t;
    }
    return nullptr;
}

st_t* parse_value(token_t** s, token_type restriction = tok_undef) {
    if ((*s)->token == tok_symbol || (*s)->token == tok_number || (*s)->token == tok_string) {
        value_t* t = new value_t((*s)->token, (*s)->value, restriction);
        *s = (*s)->next;
        return t;
    }
    return nullptr;
}

st_t* parse_restricted(token_t** s) {
    if (((*s)->token == tok_hex || (*s)->token == tok_bin)) {
        token_t* r = (*s)->next;
        st_t* t = r ? parse_value(&r, (*s)->token) : nullptr;
        if (!t) {
            if ((*s)->token == tok_hex) {
                // we allow '0x'(null)
                t = new value_t(tok_number, "", (*s)->token);
            } else {
                // we do not allow '0b'(null)
                return nullptr;
            }
        }
        *s = r;
        return t;
    }
    return nullptr;
}

st_t* parse_expr(token_t** s, bool allow_tok_binary = true, bool allow_set = false);

st_t* parse_set(token_t** s) {
    // tok_symbol tok_equal [expr]
    token_t* r = *s;
    var_t* var = (var_t*)parse_variable(&r);
    if (!var) return nullptr;
    if (!r || !r->next || r->token != tok_equal) { delete var; return nullptr; }
    r = r->next;
    st_t* val = parse_expr(&r);
    if (!val) { delete var; return nullptr; }
    *s = r;
    st_t* rv = new set_t(var->varname, val);
    delete var;
    return rv;
}

st_t* parse_parenthesized(token_t** s) {
    // tok_lparen expr tok_rparen
    token_t* r = *s;
    if (r->token != tok_lparen || !r->next) return nullptr;
    r = r->next;
    st_t* v = parse_expr(&r);
    if (!v) return nullptr;
    if (!r || r->token != tok_rparen) { delete v; return nullptr; }
    *s = r->next;
    return v;
}

st_t* parse_fcall(token_t** s);
st_t* parse_tok_binary_expr(token_t** s);

st_t* parse_expr(token_t** s, bool allow_tok_binary, bool allow_set) {
    if (allow_tok_binary) { try(parse_tok_binary_expr); }
    if (allow_set) { try(parse_set); }
    try(parse_fcall);
    try(parse_parenthesized);
    try(parse_variable);
    try(parse_restricted);
    try(parse_value);
    return nullptr;
}

st_t* parse_tok_binary_expr_post_lhs(token_t** s, st_t* lhs) {
    // tok_plus|tok_minus|tok_mul|tok_div [expr]
    token_t* r = *s;
    switch (r->token) {
    case tok_plus:
    case tok_minus:
    case tok_mul:
    case tok_div:
    case tok_concat:
        break;
    default:
        return nullptr;
    }
    token_type op_token = r->token;
    r = r->next;
    if (!r) return nullptr;
    st_t* rhs = parse_expr(&r);
    if (!rhs) { delete lhs; return nullptr; }
    *s = r;
    return new bin_t(op_token, lhs, rhs);
}

st_t* parse_tok_binary_expr(token_t** s) {
    // [expr] tok_plus|tok_minus|tok_mul|tok_div [expr]
    token_t* r = *s;
    st_t* lhs = parse_expr(&r, false);
    if (!lhs) return nullptr;
    if (!r) { delete lhs; return nullptr; }
    st_t* res = parse_tok_binary_expr_post_lhs(&r, lhs);
    if (!res) return nullptr;
    *s = r;
    return res;
}

st_t* parse_csv(token_t** s) {
    // [expr] [tok_comma [expr] [tok_comma [expr] [...]]
    std::vector<st_t*> values;
    token_t* r = *s;

    while (r) {
        st_t* next = parse_expr(&r);
        if (!next) break;
        values.push_back(next);
        if (!r || r->token != tok_comma) {
            break;
        } else {
            r = r->next;
        }
    }

    if (values.size() == 0) return nullptr;
    *s = r;

    return new list_t(values);
}

st_t* parse_fcall(token_t** s) {
    // tok_symbol tok_lparen [arg1 [tok_comma arg2 [tok_comma arg3 [...]]] tok_rparen
    token_t* r = *s;
    if (r->token != tok_symbol) return nullptr;
    std::string fname = r->value;
    r = r->next;
    if (!r || !r->next || r->token != tok_lparen) return nullptr;
    r = r->next;
    list_t* args = (list_t*)parse_csv(&r); // may be null, for case function() (0 args)
    if (!r) { if (args) delete args; return nullptr; }
    if (!r || r->token != tok_rparen) { delete args; return nullptr; }
    *s = r->next;
    return new call_t(fname, args);
}

st_t* treeify(token_t* tokens) {
    token_t* s = tokens;
    st_t* value = parse_expr(&s, true, true);
    if (s) {
        throw std::runtime_error(strprintf("failed to treeify tokens around token %s", s->value));
        return nullptr;
    }
    return value;
}

} // namespace tiny

#endif // included_tiny_parser_h_
