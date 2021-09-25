#include <stdlib.h>
#include <string>
#include <vector>
#include <map>

#include "json_lite.h"

namespace json {
    node *parse (char *stream, int& offset);

    bool extractLiteral (char *stream, std::string& extraction, int& offset);
    node *extractLiteral (char *stream, int& offset);
    node *extractNumber (char *stream, int& offset);
    node *extractHash (char *stream, int& offset);
    node *extractArray (char *stream, int& offset);
    node *extractBoolean (char *stream, int& offset);

    node _nothing;
    node *nothing = & _nothing;
}

void json::removeWhiteSpaces (char *source, std::string& result) {
    bool insideString = false;

    result.clear ();

    for (auto chr = source; *chr; ++ chr) {
        if (*chr == '"') insideString = !insideString;
        
        if (insideString || *chr != ' ' && *chr != '\t' && *chr != '\r' && *chr != '\n')
            result += *chr;
    }
}

void json::removeWhiteSpaces (char *begin, char *end, std::string& result) {
    bool insideString = false;

    result.clear ();

    for (auto chr = begin; chr < end; ++ chr) {
        if (*chr == '"') insideString = !insideString;
        
        if (insideString || *chr != ' ' && *chr != '\t' && *chr != '\r' && *chr != '\n')
            result.append (1, *chr);
    }
}

bool json::extractLiteral (char *stream, std::string& extraction, int& offset) {
    extraction.clear ();

    if (stream [offset] != '"') return false;

    for (auto i = offset + 1; stream [i] && stream [i] != '"'; ++ i) extraction += stream [i];

    offset += (int) extraction.length () + 2;

    return true;
}

json::node *json::extractLiteral (char *stream, int& offset) {
    std::string extraction;

    if (!extractLiteral (stream, extraction, offset)) return 0;

    return new stringNode (extraction.c_str ());
}

json::node *json::extractNumber (char *stream, int& offset) {
    if (!isdigit (stream [offset]) && stream [offset] != '-') return 0;

    std::string extraction;
    bool dotPassed = false;
    auto start= offset;

    if (stream [offset] == '-') {
        extraction += '-';
        start ++;
    }

    for (auto i = start; isdigit (stream [i]) || stream [i] == '.'; ++ i) {
        if (stream [i] == '.') {
            if (dotPassed) return 0;

            dotPassed = true;
        }

        extraction += stream [i];
    }

    offset += (int) extraction.length ();

    return new numberNode (extraction.c_str ());
}

json::node *json::extractBoolean (char *stream, int& offset) {
    if (!isalpha (stream [offset])) return 0;

    std::string extraction;

    while (isalpha (stream [offset])) {
        extraction += stream [offset++];
    }

    return new booleanNode (extraction.c_str ());
}

json::node *json::parse (char *stream, int& nextChar) {
    std::string streamHolder;

    removeWhiteSpaces (stream, streamHolder);
    
    return parseTrusted ((char *) streamHolder.data (), nextChar);
}

json::node *json::parse (char *begin, char *end, int& nextChar) {
    std::string streamHolder;

    removeWhiteSpaces (begin, end, streamHolder);
    
    return parseTrusted ((char *) streamHolder.data (), nextChar);
}

json::node *json::parseTrusted (char *stream, int& offset) {
    json::node *item;

    switch (stream [offset]) {
        case '{': {
            item = extractHash (stream, offset);

            if (!item) return 0;

            break;
        }
        case '[': {
            item = extractArray (stream, offset);

            if (!item) return 0;

            break;
        }
        case '"': {
            item = extractLiteral (stream, offset);

            if (!item) return 0;

            break;
        }
        default: {
            if (isdigit (stream [offset]) || stream [offset] == '.' || stream [offset] == '-') {
                item = extractNumber (stream, offset);
            } else if (memcmp (stream + offset, "true", 4) == 0 || memcmp (stream + offset, "false", 5) == 0) {
                item = extractBoolean (stream, offset);
            } else if (memcmp (stream + offset, "null", 4) == 0) {
                item = new node ();
                offset += 4;
            } else {
                item = 0;
            }

            break;
        }
    }

    return item;
}

json::node *json::extractHash (char *stream, int& offset) {
    hashNode *result = new hashNode ();
    
    if (stream [offset] != '{') return  0;

    ++ offset;

    while (stream [offset] == '"') {
        std::string key;

        if (!extractLiteral (stream, key, offset)) return 0;
        if (stream [offset] != ':') return 0;

        ++ offset;

        json::node *item = parseTrusted (stream, offset);

        if (!item) return 0;

        result->add (key.data (), item);

        if (stream [offset] == ',') ++ offset;
    }

    if (stream [offset] != '}') return 0;

    ++ offset;

    return result;
}

json::node *json::extractArray (char *stream, int& offset) {
    arrayNode *result = new arrayNode ();
    
    if (stream [offset] != '[') return  0;

    ++ offset;

    while (stream [offset] != ']') {
        json::node *item = parseTrusted (stream, offset);

        if (!item) return 0;

        result->add (item);

        if (stream [offset] == ',') ++ offset;
    }

    if (stream [offset] != ']') return 0;

    ++ offset;

    return result;
}

void json::getValue (node *_node, nodeValue& value) {
    switch ((*_node).type) {
        case nodeType::number: {
            value.numericValue = ((numberNode *)_node)->getValue (); break;
        }
        case nodeType::boolean: {
            value.booleanValue = ((booleanNode *)_node)->getValue (); break;
        }
        case nodeType::string: {
            value.stringValue = ((stringNode *)_node)->getValue (); break;
        }
        case nodeType::hash: {
            hashNode *hash = (hashNode *) _node;
            value.hashValue.insert ((*hash).begin (), (*hash).end ()); break;
        }
        case nodeType::array: {
            arrayNode *array = (arrayNode *) _node;
            value.arrayValue.insert (value.arrayValue.begin (), (*array).begin (), (*array).end ()); break;
        }
    }
}