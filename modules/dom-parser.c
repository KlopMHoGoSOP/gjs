/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2013  Nikita Churaev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "dom-parser.h"
#include "dom-node.h"

static void finalize_stub(JSContext *cx, JSObject *obj) {}

static JSObject *gjs_dom_parser_prototype = NULL;

static JSClass gjs_dom_parser_class = {
    "DOMParser", 0,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    finalize_stub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

GJS_NATIVE_CONSTRUCTOR_DECLARE(dom_parser)
{
    GJS_NATIVE_CONSTRUCTOR_VARIABLES(dom_parser);
    GJS_NATIVE_CONSTRUCTOR_PRELUDE(dom_parser);
    GJS_NATIVE_CONSTRUCTOR_FINISH(dom_parser);
    return JS_TRUE;
}

static JSBool
parse_from_string_func(JSContext *cx, unsigned argc, jsval *vp)
{
    JSBool result = JS_TRUE;
    JSString *text;
    JSString *type;
    char *u_text = NULL;
    char *u_type = NULL;
    xmlDocPtr doc = NULL;
    JSObject *doc_object = NULL;
    
    if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "SS", &text, &type))
        return JS_FALSE;

    u_text = JS_EncodeString(cx, text);
    u_type = JS_EncodeString(cx, type);
    
    if (u_text && u_type) {
        if (!strcmp(u_type, "text/xml") 
              || !strcmp(u_type, "application/xml") 
              || !strcmp(u_type, "application/xhtml+xml")
              || !strcmp(u_type, "image/svg+xml")) {
            doc = xmlReadDoc(u_text, NULL, NULL, 0);
        
            if (doc == NULL)
            {
                gjs_throw(cx, "Failed to parse document");
                result = JS_FALSE;
                goto finish;
            }

            doc_object = gjs_dom_wrap_xml_node (cx, (xmlNodePtr)doc);
            
            if (doc_object == NULL)
            {
                gjs_throw(cx, "Failed to wrap the document object");
                result = JS_FALSE;
                goto finish;
            }

            JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(doc_object));
            goto finish;
        } else {
            gjs_throw(cx, "Unsupported type %s", u_type);
            result = JS_FALSE;
            goto finish;
        }
    }
    else
    {
        JS_ReportOutOfMemory(cx);
        result = JS_FALSE;
        goto finish;
    }

finish:
    if (u_text) JS_free(cx, u_text);
    if (u_type) JS_free(cx, u_type);
    if (result == JS_FALSE && doc) xmlFreeDoc(doc);

    return result;
}

static JSFunctionSpec gjs_dom_parser_proto_funcs[] = {
    { "parseFromString", JSOP_WRAPPER ((JSNative) parse_from_string_func), 0, 0 },
    { NULL }
};

JSBool
gjs_js_define_dom_parser_stuff (JSContext *cx, JSObject *module)
{
    gjs_dom_parser_prototype = JS_InitClass(
        cx, /* context */
        module, /* global object */
        NULL, /* parent prototype */
        &gjs_dom_parser_class,
        gjs_dom_parser_constructor, /* constructor */
        0, /* constructor number of arguments */
        NULL, /* property spec */
        gjs_dom_parser_proto_funcs, /* function spec */
        NULL, /* static property spec */
        NULL  /* static function spec */
    );

    if (gjs_dom_parser_prototype == NULL)
        return JS_FALSE;

    return JS_TRUE;
}
