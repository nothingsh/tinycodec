#include "doctest.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "tinycodec/document.h"
#include "tinycodec/value.h"

using namespace tinycodec;

TEST_CASE("each New function creates a value of the matching type") {
    Document document;
    CHECK(document.NewNull()->GetType() == Type::Null);
    CHECK(document.NewBool(true)->GetType() == Type::Bool);
    CHECK(document.NewInt(-1)->GetType() == Type::Int);
    CHECK(document.NewUint(1)->GetType() == Type::Uint);
    CHECK(document.NewDouble(1.5)->GetType() == Type::Double);
    CHECK(document.NewString("x")->GetType() == Type::String);
    CHECK(document.NewBytes("x")->GetType() == Type::Bytes);
    CHECK(document.NewArray()->GetType() == Type::Array);
    CHECK(document.NewObject()->GetType() == Type::Object);
}

TEST_CASE("QueryBool reads a Bool and rejects everything else") {
    Document document;
    bool out = false;
    CHECK(document.NewBool(true)->QueryBool(&out));
    CHECK(out == true);

    out = true;
    CHECK_FALSE(document.NewInt(0)->QueryBool(&out));
    CHECK(out == true);  // Untouched on failure.
    CHECK_FALSE(document.NewNull()->QueryBool(&out));
}

TEST_CASE("QueryInt accepts Int, and Uint only up to INT64_MAX") {
    Document document;
    int64_t out = 0;

    CHECK(document.NewInt(INT64_MIN)->QueryInt(&out));
    CHECK(out == INT64_MIN);

    CHECK(document.NewUint(static_cast<uint64_t>(INT64_MAX))->QueryInt(&out));
    CHECK(out == INT64_MAX);

    out = 7;
    CHECK_FALSE(document.NewUint(static_cast<uint64_t>(INT64_MAX) + 1)->QueryInt(&out));
    CHECK(out == 7);
    CHECK_FALSE(document.NewDouble(1.0)->QueryInt(&out));
    CHECK_FALSE(document.NewString("1")->QueryInt(&out));
}

TEST_CASE("QueryUint accepts Uint, and Int only when non-negative") {
    Document document;
    uint64_t out = 0;

    CHECK(document.NewUint(UINT64_MAX)->QueryUint(&out));
    CHECK(out == UINT64_MAX);

    CHECK(document.NewInt(0)->QueryUint(&out));
    CHECK(out == 0);

    out = 7;
    CHECK_FALSE(document.NewInt(-1)->QueryUint(&out));
    CHECK(out == 7);
    CHECK_FALSE(document.NewDouble(1.0)->QueryUint(&out));
}

TEST_CASE("QueryDouble accepts every numeric type") {
    Document document;
    double out = 0.0;

    CHECK(document.NewDouble(1.5)->QueryDouble(&out));
    CHECK(out == 1.5);
    CHECK(document.NewInt(-3)->QueryDouble(&out));
    CHECK(out == -3.0);
    CHECK(document.NewUint(4)->QueryDouble(&out));
    CHECK(out == 4.0);

    out = 7.0;
    CHECK_FALSE(document.NewBool(true)->QueryDouble(&out));
    CHECK(out == 7.0);
}

TEST_CASE("QueryString reads a String, including empty text and NUL bytes") {
    Document document;
    std::string_view out = "unchanged";

    CHECK(document.NewString("hello")->QueryString(&out));
    CHECK(out == "hello");

    CHECK(document.NewString("")->QueryString(&out));
    CHECK(out.empty());

    std::string withNul("a\0b", 3);
    CHECK(document.NewString(withNul)->QueryString(&out));
    CHECK(out.size() == 3);
    CHECK(out == std::string_view(withNul));

    out = "unchanged";
    CHECK_FALSE(document.NewInt(1)->QueryString(&out));
    CHECK(out == "unchanged");
}

TEST_CASE("NewString copies its argument") {
    Document document;
    std::string source = "original";
    Value* value = document.NewString(source);
    source = "XXXXXXXX";

    std::string_view out;
    CHECK(value->QueryString(&out));
    CHECK(out == "original");
}

TEST_CASE("a string longer than an arena block survives intact") {
    Document document;
    std::string big(10000, 'q');
    std::string_view out;
    CHECK(document.NewString(big)->QueryString(&out));
    CHECK(out == std::string_view(big));
}

TEST_CASE("Append adds children to an Array in order") {
    Document document;
    Value* array = document.NewArray();
    CHECK(array->Size() == 0);
    CHECK(array->FirstChild() == nullptr);

    Value* first = document.NewInt(1);
    Value* second = document.NewInt(2);
    CHECK(array->Append(first));
    CHECK(array->Append(second));

    CHECK(array->Size() == 2);
    CHECK(array->FirstChild() == first);
    CHECK(first->Next() == second);
    CHECK(second->Next() == nullptr);
    CHECK(array->At(0) == first);
    CHECK(array->At(1) == second);
    CHECK(array->At(2) == nullptr);
    CHECK(first->Key().empty());
}

TEST_CASE("Set adds members to an Object in order and Find looks them up") {
    Document document;
    Value* object = document.NewObject();
    Value* a = document.NewInt(1);
    Value* b = document.NewInt(2);
    CHECK(object->Set("a", a));
    CHECK(object->Set("b", b));

    CHECK(object->Size() == 2);
    CHECK(object->FirstChild() == a);
    CHECK(a->Next() == b);
    CHECK(a->Key() == "a");
    CHECK(b->Key() == "b");
    CHECK(object->Find("a") == a);
    CHECK(object->Find("b") == b);
    CHECK(object->Find("c") == nullptr);
}

TEST_CASE("Set copies the key and accepts an empty key") {
    Document document;
    Value* object = document.NewObject();
    std::string key = "name";
    Value* named = document.NewInt(1);
    Value* unnamed = document.NewInt(2);
    CHECK(object->Set(key, named));
    key = "XXXX";
    CHECK(object->Set("", unnamed));

    CHECK(named->Key() == "name");
    CHECK(object->Find("name") == named);
    CHECK(object->Find("") == unnamed);
}

TEST_CASE("Set replaces an existing member in place") {
    Document document;
    Value* object = document.NewObject();
    Value* a = document.NewInt(1);
    Value* b = document.NewInt(2);
    Value* c = document.NewInt(3);
    object->Set("a", a);
    object->Set("b", b);
    object->Set("c", c);

    SUBCASE("in the middle") {
        Value* replacement = document.NewString("new");
        CHECK(object->Set("b", replacement));
        CHECK(object->Size() == 3);
        CHECK(a->Next() == replacement);
        CHECK(replacement->Next() == c);
        CHECK(replacement->Key() == "b");
        CHECK(object->Find("b") == replacement);
    }
    SUBCASE("at the front") {
        Value* replacement = document.NewString("new");
        CHECK(object->Set("a", replacement));
        CHECK(object->FirstChild() == replacement);
        CHECK(replacement->Next() == b);
    }
    SUBCASE("at the end, and later members still go after it") {
        Value* replacement = document.NewString("new");
        CHECK(object->Set("c", replacement));
        CHECK(b->Next() == replacement);
        CHECK(replacement->Next() == nullptr);

        Value* d = document.NewInt(4);
        CHECK(object->Set("d", d));
        CHECK(replacement->Next() == d);
        CHECK(object->Size() == 4);
    }
    SUBCASE("the replaced value is detached and can be attached again") {
        CHECK(object->Set("b", document.NewNull()));
        CHECK(b->Key().empty());
        CHECK(b->Next() == nullptr);
        CHECK(object->Set("again", b));
        CHECK(object->Find("again") == b);
    }
    SUBCASE("using the key of the member being replaced") {
        Value* replacement = document.NewString("new");
        CHECK(object->Set(b->Key(), replacement));
        CHECK(replacement->Key() == "b");
    }
}

TEST_CASE("Append and Set fail without changing anything") {
    Document document;
    Value* array = document.NewArray();
    Value* object = document.NewObject();

    SUBCASE("on the wrong container type") {
        CHECK_FALSE(object->Append(document.NewNull()));
        CHECK_FALSE(array->Set("k", document.NewNull()));
        CHECK_FALSE(document.NewInt(1)->Append(document.NewNull()));
        CHECK(object->Size() == 0);
        CHECK(array->Size() == 0);
    }
    SUBCASE("for a null child") {
        CHECK_FALSE(array->Append(nullptr));
        CHECK_FALSE(object->Set("k", nullptr));
    }
    SUBCASE("for a child from another document") {
        Document other;
        CHECK_FALSE(array->Append(other.NewNull()));
        CHECK_FALSE(object->Set("k", other.NewNull()));
        CHECK(array->Size() == 0);
    }
    SUBCASE("for a child that already has a parent") {
        Value* child = document.NewNull();
        CHECK(array->Append(child));
        CHECK_FALSE(array->Append(child));
        CHECK_FALSE(object->Set("k", child));
        CHECK(array->Size() == 1);
        CHECK(object->Size() == 0);
    }
    SUBCASE("for a child that is the root") {
        Value* root = document.NewArray();
        CHECK(document.SetRoot(root));
        CHECK_FALSE(array->Append(root));
    }
    SUBCASE("for the container itself") {
        CHECK_FALSE(array->Append(array));
        CHECK_FALSE(object->Set("self", object));
        CHECK(array->Size() == 0);
    }
    SUBCASE("for an ancestor of the container") {
        Value* outer = document.NewArray();
        Value* middle = document.NewObject();
        Value* inner = document.NewArray();
        CHECK(outer->Append(middle));
        CHECK(middle->Set("inner", inner));
        CHECK_FALSE(inner->Append(outer));
        CHECK(inner->Size() == 0);
    }
}

TEST_CASE("container accessors return nothing on scalars") {
    Document document;
    Value* scalar = document.NewInt(1);
    CHECK(scalar->Size() == 0);
    CHECK(scalar->FirstChild() == nullptr);
    CHECK(scalar->Find("a") == nullptr);
    CHECK(scalar->At(0) == nullptr);
    CHECK(document.NewObject()->At(0) == nullptr);
    CHECK(document.NewArray()->Find("a") == nullptr);
}

TEST_CASE("SetRoot installs and replaces the root") {
    Document document;
    CHECK(document.Root() == nullptr);

    Value* first = document.NewInt(1);
    CHECK(document.SetRoot(first));
    CHECK(document.Root() == first);

    Value* second = document.NewInt(2);
    CHECK(document.SetRoot(second));
    CHECK(document.Root() == second);

    // The old root is detached, so it can be used again.
    Value* array = document.NewArray();
    CHECK(array->Append(first));
}

TEST_CASE("SetRoot fails without changing the root") {
    Document document;
    Value* root = document.NewArray();
    CHECK(document.SetRoot(root));

    CHECK_FALSE(document.SetRoot(nullptr));
    CHECK_FALSE(document.SetRoot(root));  // Already the root.

    Document other;
    CHECK_FALSE(document.SetRoot(other.NewNull()));

    Value* child = document.NewNull();
    CHECK(root->Append(child));
    CHECK_FALSE(document.SetRoot(child));

    CHECK(document.Root() == root);
}

TEST_CASE("Clear empties the document and it can be filled again") {
    Document document;
    document.SetRoot(document.NewString("old"));
    document.Clear();
    CHECK(document.Root() == nullptr);

    Value* fresh = document.NewInt(1);
    CHECK(document.SetRoot(fresh));
    CHECK(document.Root() == fresh);
}

TEST_CASE("Equals compares scalars by type and value") {
    Document document;
    CHECK(document.NewNull()->Equals(*document.NewNull()));
    CHECK(document.NewBool(true)->Equals(*document.NewBool(true)));
    CHECK_FALSE(document.NewBool(true)->Equals(*document.NewBool(false)));
    CHECK(document.NewString("a")->Equals(*document.NewString("a")));
    CHECK_FALSE(document.NewString("a")->Equals(*document.NewString("b")));
    CHECK(document.NewDouble(1.5)->Equals(*document.NewDouble(1.5)));
    CHECK_FALSE(document.NewDouble(1.5)->Equals(*document.NewDouble(2.5)));
    CHECK_FALSE(document.NewNull()->Equals(*document.NewBool(false)));
    CHECK_FALSE(document.NewString("1")->Equals(*document.NewInt(1)));
}

TEST_CASE("Equals compares Int and Uint by numeric value, but keeps Double apart") {
    Document document;
    CHECK(document.NewInt(5)->Equals(*document.NewInt(5)));
    CHECK_FALSE(document.NewInt(5)->Equals(*document.NewInt(6)));
    CHECK(document.NewUint(5)->Equals(*document.NewUint(5)));
    CHECK(document.NewInt(5)->Equals(*document.NewUint(5)));
    CHECK(document.NewUint(5)->Equals(*document.NewInt(5)));
    CHECK_FALSE(document.NewInt(-1)->Equals(*document.NewUint(UINT64_MAX)));
    CHECK_FALSE(document.NewUint(UINT64_MAX)->Equals(*document.NewInt(-1)));
    CHECK_FALSE(document.NewInt(1)->Equals(*document.NewDouble(1.0)));
    CHECK_FALSE(document.NewDouble(1.0)->Equals(*document.NewInt(1)));
}

TEST_CASE("Equals compares containers deeply and in order") {
    Document document;
    auto makeObject = [&document](const char* firstKey, const char* secondKey) {
        Value* object = document.NewObject();
        Value* list = document.NewArray();
        list->Append(document.NewInt(1));
        list->Append(document.NewString("x"));
        object->Set(firstKey, list);
        object->Set(secondKey, document.NewNull());
        return object;
    };

    CHECK(makeObject("a", "b")->Equals(*makeObject("a", "b")));
    CHECK_FALSE(makeObject("a", "b")->Equals(*makeObject("a", "c")));

    Value* shorter = document.NewArray();
    shorter->Append(document.NewInt(1));
    Value* longer = document.NewArray();
    longer->Append(document.NewInt(1));
    longer->Append(document.NewInt(2));
    CHECK_FALSE(shorter->Equals(*longer));
    CHECK_FALSE(document.NewArray()->Equals(*document.NewObject()));

    // Same members, different order.
    Value* ab = document.NewObject();
    ab->Set("a", document.NewInt(1));
    ab->Set("b", document.NewInt(2));
    Value* ba = document.NewObject();
    ba->Set("b", document.NewInt(2));
    ba->Set("a", document.NewInt(1));
    CHECK_FALSE(ab->Equals(*ba));
}

TEST_CASE("QueryBytes reads Bytes, including empty content and NUL bytes") {
    Document document;
    std::string_view out = "unchanged";

    std::string binary("\x00\xFF\x80" "a", 4);
    CHECK(document.NewBytes(binary)->QueryBytes(&out));
    CHECK(out.size() == 4);
    CHECK(out == std::string_view(binary));

    CHECK(document.NewBytes("")->QueryBytes(&out));
    CHECK(out.empty());

    out = "unchanged";
    CHECK_FALSE(document.NewInt(1)->QueryBytes(&out));
    CHECK(out == "unchanged");
}

TEST_CASE("String and Bytes do not convert into each other") {
    Document document;
    std::string_view out = "unchanged";
    CHECK_FALSE(document.NewBytes("x")->QueryString(&out));
    CHECK(out == "unchanged");
    CHECK_FALSE(document.NewString("x")->QueryBytes(&out));
    CHECK(out == "unchanged");
}

TEST_CASE("NewBytes copies its argument, also when longer than an arena block") {
    Document document;
    std::string source = "original";
    Value* small = document.NewBytes(source);
    source = "XXXXXXXX";

    std::string big(10000, '\xAB');
    Value* large = document.NewBytes(big);

    std::string_view out;
    CHECK(small->QueryBytes(&out));
    CHECK(out == "original");
    CHECK(large->QueryBytes(&out));
    CHECK(out == std::string_view(big));
}

TEST_CASE("Equals compares Bytes byte by byte and keeps them apart from String") {
    Document document;
    CHECK(document.NewBytes("ab")->Equals(*document.NewBytes("ab")));
    CHECK(document.NewBytes("")->Equals(*document.NewBytes("")));
    CHECK_FALSE(document.NewBytes("ab")->Equals(*document.NewBytes("ac")));
    CHECK_FALSE(document.NewBytes("ab")->Equals(*document.NewBytes("abc")));
    CHECK_FALSE(document.NewBytes("ab")->Equals(*document.NewString("ab")));
    CHECK_FALSE(document.NewString("ab")->Equals(*document.NewBytes("ab")));
}
