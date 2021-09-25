#pragma once

#include <stdlib.h>
#include <string>
#include <vector>
#include <map>
#include <string>

#ifdef __linux__
#define _strdup strdup
#endif

namespace json {
    enum nodeType {
        null = 0,
        number,
        string,
        array,
        hash,
        boolean,
    };

    struct node {
        nodeType type;

        node (nodeType _type = nodeType::null) : type (_type) {}
        virtual ~node () {}

        virtual void *get () { return 0; }
        virtual std::string serialize () { return "null"; }
    };

    extern node _nothing;
    extern node *nothing;

    struct stringNode: node {
        std::string value;

        stringNode (): node (nodeType::string) {}
        stringNode (const char *src): node (nodeType::string), value (src) {}

        virtual void *get () { return (void *) value.c_str (); }
        inline const char *getValue () { return value.c_str (); }

        virtual std::string serialize () {
            std::string result = "\"";

            result += value;
            result += '"';

            return result;
        }
    };

    struct booleanNode: node {
        bool value;

        bool isTrue (const char *src) {
            if (!src) return false;

            static constexpr const char *__TRUE = "true";

            for (int i = 0; __TRUE [i]; ++ i) {
                if (!src [i] || (tolower (src [i]) != __TRUE [i])) return false;
            }

            return true;
        }

        booleanNode (): node (nodeType::boolean), value (false) {}
        booleanNode (const bool src): node (nodeType::boolean), value (src) {}
        booleanNode (const char *src): node (nodeType::boolean), value (isTrue (src)) {}

        virtual void *get () { return (void *) & value; }
        inline bool getValue () { return value; }

        virtual std::string serialize () {
            return std::string (value ? "true" : "false");
        }
    };

    struct numberNode: node {
        double value;

        // redone atof version independent on the locale settings
        static double char2dbl (const char *src) {
            uint32_t integral = 0;
            uint32_t fractal = 0;
            uint32_t fractalDivider = 1;
            bool negative = false;
            bool sepFound = false;
            bool nonBlankFound = false;
            for (; *src; ++ src) {
                if (*src == '-') {
                    if (negative) break; // second minus found
                    nonBlankFound = true;
                    negative = true;
                } else if (*src == ' ' || *src == '\t') {
                    if (nonBlankFound) break; // an unexpected char found
                    continue;
                } else if (*src == ',' || *src == '.') {
                    if (sepFound) break; // second decimal point or comma found
                    sepFound = true;
                } else if (isdigit (*src)) {
                    if (sepFound) {
                        fractal = fractal * 10 + *src - '0';
                        fractalDivider *= 10;
                    } else {
                        integral = integral * 10 + *src - '0';
                    }
                } else {
                    break; // non-number char found (neither minus nor dot, comma or digit)
                }
            }
            double result = (double) integral + (double) fractal / (double) fractalDivider;
            return negative ? - result : result;
        }

        numberNode (): node (nodeType::number) {}
        numberNode (const char *src): node (nodeType::number), value (char2dbl (src)) {}
        numberNode (const double src): node (nodeType::number), value (src) {}

        virtual void *get () { return (void *) & value; }
        inline double getValue () { return value; }

        virtual std::string serialize () {
            char buffer [200];
            sprintf (buffer, "%f", value);

            // Make sure that dot is used but not comma
            char *dotPos = strchr (buffer, ',');

            if (dotPos) {
                *dotPos = '.';
            } else {
                dotPos = strchr (buffer, '.');
            }
            
            // Remove trailing zeros (if any)
            if (dotPos) {
                for (auto pos = buffer + strlen (buffer) - 1; pos > dotPos; -- pos) {
                    if (*pos == '0')
                        *pos = '\0';
                    else
                        break;
                }
                // if the value ended by "." so it is an integer
                if (dotPos [1] == '\0') *dotPos = '\0';
            }

            return std::string (buffer);
        }
    };

    struct arrayNode: node {
        std::vector<node *> value;

        arrayNode (): node (nodeType::array) {}
        virtual ~arrayNode () {
            for (auto& item: value) {
                delete item;
            }
            value.clear ();
        }

        virtual void *get () { return (void *) & value; }

        inline size_t size () {
            return value.size ();
        }

        inline void add (node *val) {
            value.push_back (val);
        }

        inline node *&operator [] (size_t index) {
            return index < value.size () ? value [index] : json::nothing;
        }

        inline node *at (size_t index) {
            return index < value.size () ? value [index] : json::nothing;
        }

        inline void setAt (size_t index, node *nodeValue) {
            if (index < value.size ()) value [index] = nodeValue;
        }

        inline auto begin () { return value.begin (); }
        inline auto end () { return value.end (); }

        virtual std::string serialize () {
            std::string result = "[";

            for (auto i = 0; i < value.size (); ++ i) {
                result += value [i]->serialize ();

                if (i < (value.size () - 1)) result += ',';
            }

            result += ']';

            return result;
        }
    };

    struct hashNode: node {
        std::map<std::string, node *> value;

        hashNode (): node (nodeType::hash) {}
        virtual ~hashNode () {
            for (auto& item: value) {
                if (item.second) {
                    delete item.second;
                }
            }
            value.clear ();
        }

        virtual void *get () { return (void *) & value; }

        inline void add (const char *key, node *val) {
            value.insert (value.end (), std::pair<std::string, node *> (std::string (key), val));
        }

        inline node *&operator [] (char *key) {
            for (auto& item: value) {
                auto result = strcmp (item.first.c_str (), key);
                
                if (result == 0) return item.second;
            }

            return nothing;
        }

        inline node *&operator [] (const char *key) {
            if (!key || !*key) return nothing;

            for (auto& item: value) {
                auto result = strcmp (item.first.c_str (), key);
                
                if (result == 0) return item.second;
            }

            return nothing;
        }

        inline node *at (char *key) {
            if (!key || !*key) return nothing;

            for (auto& item: value) {
                auto result = strcmp (item.first.c_str (), key);
                
                if (result == 0) return item.second;
            }

            return 0;
        }

        inline void setAt (char *key, node *nodeValue) {
            if (!key || !*key || !nodeValue) return;

            for (auto& item: value) {
                auto result = strcmp (item.first.c_str (), key);
                
                if (result == 0) {
                    item.second = nodeValue; break;
                }
            }
        }

        inline auto begin () { return value.begin (); }
        inline auto end () { return value.end (); }

        virtual std::string serialize () {
            std::string result = "{";

            for (auto& element: value) {
                result += "\"";
                result += element.first;
                result += "\":";

                if (element.second) {
                    result += element.second->serialize ();
                } else {
                    result += "null";
                }

                result += ',';
            }

            if (result.length () > 2)
                result.back () = '}';

            return result;
        }
    };

    struct valueKey {
        size_t arrayIndex;
        std::string hashKey;

        static const size_t noIndex = 0xFFFFFFFF;

        valueKey (): arrayIndex (noIndex) {}
    };

    struct nodeValue {
        std::string stringValue;
        double numericValue;
        bool booleanValue;
        std::vector<node *> arrayValue;
        std::map<std::string, node *> hashValue;
    };

    node *parse (char *sourceString, int& nextChar);
    node *parse (char *begin, char *end, int& nextChar);
    node *parseTrusted (char *stream, int& offset);
    void removeWhiteSpaces (char *source, std::string& result);
    void removeWhiteSpaces (char *begin, char *end, std::string& result);
    void getValue (node *_node, nodeValue& value);

    template<typename Cb> void walkThrough (node *_node, Cb cb, valueKey& key, uint16_t level) {
        nodeValue val;

        if (!_node) {
            cb (_node, val, key, level); return;
        }

        // populate node value and make a first, very general call of the callback
        // for hashes and arrays we will call callback recursively later
        getValue (_node, val);
        cb (_node, val, key, level); 

        switch ((*(_node)).type) {
            case nodeType::array: {
                valueKey itemKey;

                // go through all array elements
                for (itemKey.arrayIndex = 0; itemKey.arrayIndex < val.arrayValue.size (); ++ itemKey.arrayIndex) {
                    walkThrough (val.arrayValue [itemKey.arrayIndex], cb, itemKey, level + 1);
                }
                break;
            }
            case nodeType::hash: {
                valueKey itemKey;

                // go through all hash elements
                for (auto& element: val.hashValue) {
                    itemKey.hashKey = element.first;
                    walkThrough (element.second, cb, itemKey, level + 1);
                }
                break;
            }
        }
    }
}
