// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#ifndef XXUTILS__JSON_OBJECT_H
#define XXUTILS__JSON_OBJECT_H

#include <json-c/json.h>
#include <string>
#include <stdexcept>
#include <boost/lexical_cast.hpp>

#include <util/data_types.h>

class JsonError: public Yb::RunTimeError
{
public:
    JsonError(const std::string &s): RunTimeError(s) {}
};

class JsonKeyError: public JsonError
{
public:
    JsonKeyError(const std::string &s): JsonError(s) {}
};

// A thinniest wrapper for libjson-c
class JsonObject
{
    // Each object can keep either a strong or a weak reference.
    // Strong one is returned on parsing a document or
    // on explicit node creation.
    // Copy/assignment transfers ownership a-la std::auto_ptr

    mutable json_object *jobj_;
    mutable bool owns_;

public:
    JsonObject()
        : jobj_(NULL)
        , owns_(false)
    {}

    explicit JsonObject(json_object *jobj, bool owns)
        : jobj_(jobj)
        , owns_(owns)
    {
        if (owns_ && !jobj_)
            throw JsonError("can't allocate a JSON object");
    }

    void put_object()
    {
        if (owns_ && jobj_)
            json_object_put(jobj_);
        jobj_ = NULL;
        owns_ = false;
    }

    JsonObject(const JsonObject &other)
        : jobj_(other.jobj_)
        , owns_(other.owns_)
    {
        other.owns_ = false;
    }

    ~JsonObject()
    {
        put_object();
    }

    JsonObject &operator= (const JsonObject &other)
    {
        if (&other != this)
        {
            put_object();
            jobj_ = other.jobj_;
            owns_ = other.owns_;
            other.owns_ = false;
        }
        return *this;
    }

    json_object *get() { return jobj_; }
    json_object *release() { owns_ = false; return jobj_; }
    bool owns() const { return owns_; }

    static JsonObject new_object()
    {
        json_object *jobj = json_object_new_object();
        if (!jobj)
            throw JsonError("can't allocate a JSON object");
        return JsonObject(jobj, true);
    }

    static JsonObject parse(const std::string &s)
    {
        json_object *jobj = json_tokener_parse(s.c_str());
        if (!jobj)
            throw JsonError("failed to parse JSON");
        return JsonObject(jobj, true);
    }

    const std::string serialize()
    {
        return std::string(json_object_to_json_string(jobj_));
    }

    bool has_field(const std::string &name)
    {
        json_object *field_jobj = json_object_object_get(jobj_, name.c_str());
        return field_jobj != NULL;
    }

    JsonObject get_field(const std::string &name)
    {
        json_object *field_jobj = json_object_object_get(jobj_, name.c_str());
        if (!field_jobj)
            throw JsonKeyError("JSON has no '" + name + "' key");
        return JsonObject(field_jobj, false);
    }

    const std::string get_str_field(const std::string &name)
    {
        JsonObject field_obj = get_field(name);
        if (json_object_get_type(field_obj.get()) != json_type_string)
            throw JsonKeyError("JSON key '" + name
                    + "' expected to contain a string");
        return std::string(json_object_get_string(field_obj.get()));
    }

    const std::string get_typed_field(const std::string &name)
    {
        JsonObject field_obj = get_field(name);
        enum json_type jtype = json_object_get_type(field_obj.get());
        switch (jtype) {
            case json_type_boolean:
                return "b:" + std::string(
                        json_object_get_boolean(field_obj.get())?
                        "true": "false");
            case json_type_int:
                return "i:" + boost::lexical_cast<std::string>(
                        json_object_get_int(field_obj.get()));
            case json_type_double:
                return "f:" + boost::lexical_cast<std::string>(
                        json_object_get_double(field_obj.get()));
            case json_type_string:
                return "s:" + std::string(
                        json_object_get_string(field_obj.get()));
            default:
                break;
        }
        throw JsonKeyError("JSON key '" + name
                + "' has unsupported type " +
                boost::lexical_cast<std::string>((int)jtype));
    }

    void add_field(const std::string &name, JsonObject &child)
    {
        json_object_object_add(jobj_, name.c_str(), child.release());
    }

    void add_str_field(const std::string &name, const std::string &value)
    {
        JsonObject str(json_object_new_string(value.c_str()), true);
        add_field(name, str);
    }

    void add_typed_field(const std::string &name, const std::string &value)
    {
        json_object *field_jobj = NULL;
        if (value.size() >= 2 && value[1] == ':')
            switch (value[0]) {
                case 'b':
                    field_jobj = json_object_new_boolean(value.substr(2) == "true");
                    break;
                case 'i':
                    field_jobj = json_object_new_int(
                            boost::lexical_cast<int>(value.substr(2)));
                    break;
                case 'f':
                    field_jobj = json_object_new_double(
                            boost::lexical_cast<double>(value.substr(2)));
                    break;
                case 's':
                    field_jobj = json_object_new_string(value.substr(2).c_str());
                    break;
                default:
                    break;
            }
        if (!field_jobj)
            throw JsonError("can't allocate a JSON object or wrong type");
        JsonObject typed(field_jobj, true);
        add_field(name, typed);
    }

    void delete_field(const std::string &name)
    {
        json_object_object_del(jobj_, name.c_str());
    }

    const std::string pop_str_field(const std::string &name)
    {
        const std::string result = get_str_field(name);
        delete_field(name);
        return result;
    }

    const std::string pop_typed_field(const std::string &name)
    {
        const std::string result = get_typed_field(name);
        delete_field(name);
        return result;
    }
};

#endif // XXUTILS__JSON_OBJECT_H
// vim:ts=4:sts=4:sw=4:et:
