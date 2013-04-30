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

#include "dom-node.h"
#include <config.h>
#include <gjs/gjs-module.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>

/* --------------------------------------------------------------- */

/* _private for xmlDomNode and friends */
typedef struct {
    JSObject *obj;
    xmlNodePtr node;
} DOMNodePrivate;

/* _private for xmlDomDoc, can be safely cast into DOMNodePrivate */
typedef struct {
    DOMNodePrivate parent;
    
    /* Number of nodes that have a JavaScript object associated with them. When
       this number reaches zero, the document is deleted. */
    guint64 num_nodes_with_obj;
} DOMDocPrivate;

#define DOM_NODE_PRIVATE(p) ((DOMNodePrivate *)(p))
#define DOM_DOC_PRIVATE(p)  ((DOMDocPrivate *)(p))
#define IS_NODE_DOCUMENT(n) ((n)->type == XML_DOCUMENT_NODE)

/* --------------------------------------------------------------- */

#define REPORT_OUT_OF_MEMORY \
    { JS_ReportOutOfMemory(cx); \
      result = JS_FALSE; \
      goto finish; }

/* --------------------------------------------------------------- */

static JSObject *gjs_dom_node_prototype = NULL;
static JSObject *gjs_dom_document_prototype = NULL;
static JSObject *gjs_dom_element_prototype = NULL;

/* --------------------------------------------------------------- */

static void node_finalize(JSContext *cx, JSObject *obj);

/* --------------------------------------------------------------- */

static JSClass gjs_dom_node_class = {
    "DOMNode", 
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    node_finalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

GJS_DEFINE_PRIV_FROM_JS(DOMNodePrivate, gjs_dom_node_class)

/* ========================================================================= */
/* Node                                                                      */
/* ========================================================================= */

static JSBool
node_type_getter(JSContext *cx, JSObject **obj, jsid *id, jsval *vp)
{
    DOMNodePrivate *priv;
    priv = priv_from_js(cx, *obj);

    if (priv == NULL)
        return JS_TRUE; /* prototype, not instance */

    *vp = INT_TO_JSVAL (priv->node->type);
    return JS_TRUE;
}

/* --------------------------------------------------------------- */

static JSBool
node_name_getter(JSContext *cx, JSObject **obj, jsid *id, jsval *vp)
{
    DOMNodePrivate *priv;
    const xmlChar *name = NULL;
    JSString *str;
    
    priv = priv_from_js(cx, *obj);

    if (priv == NULL)
        return JS_TRUE; /* prototype, not instance */

    switch (priv->node->type) {
    case XML_ATTRIBUTE_NODE:
    case XML_ELEMENT_NODE:
    case XML_ENTITY_NODE:
    case XML_ENTITY_REF_NODE:
    case XML_NOTATION_NODE:
    case XML_PI_NODE:
        name = priv->node->name;
        break;
        
    case XML_CDATA_SECTION_NODE:
        name = (const xmlChar *)"#cdata-section";
        break;
    
    case XML_COMMENT_NODE:
        name = (const xmlChar *)"#comment";
        break;
    
    case XML_DOCUMENT_NODE:
        name = (const xmlChar *)"#document";
        break;
        
    case XML_DOCUMENT_FRAG_NODE:
        name = (const xmlChar *)"#document-fragment";
        break;
        
    case XML_TEXT_NODE:
        name = (const xmlChar *)"#text";
        break;
    }

    if (name == NULL)
        name = (const xmlChar *)"#unknown";

    str = JS_NewStringCopyZ(cx, name);
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

/* --------------------------------------------------------------- */

static JSBool
node_value_getter(JSContext *cx, JSObject **obj, jsid *id, jsval *vp)
{
    DOMNodePrivate *priv;
    const xmlChar *value;
    JSString *str;
    priv = priv_from_js(cx, *obj);

    if (priv == NULL)
        return JS_TRUE; /* prototype, not instance */

    switch (priv->node->type) {
    case XML_TEXT_NODE:
    case XML_CDATA_SECTION_NODE:
        value = priv->node->content;
        break;
    
    default:
        value = NULL;
    }

    if (value != NULL) {
        str = JS_NewStringCopyZ(cx, value);
        *vp = STRING_TO_JSVAL(str);
    } else {
        *vp = JSVAL_NULL;
    }

    return JS_TRUE;
}

/* --------------------------------------------------------------- */

static JSBool
node_value_setter(JSContext *cx, JSObject **obj, jsid *id, jsval *vp)
{
    DOMNodePrivate *priv;
    JSString *str;
    xmlChar *new_value;
    priv = priv_from_js(cx, *obj);

    if (priv == NULL)
        return JS_TRUE; /* prototype, not instance */

    if (!JSVAL_IS_STRING(*vp)) {
        gjs_throw(cx, "Must be a string");
        return JS_FALSE;
    }

    str = JSVAL_TO_STRING(*vp);
    
    new_value = JS_EncodeString(cx, str);
    
    if (!new_value) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }

    switch (priv->node->type) {
    case XML_TEXT_NODE:
    case XML_CDATA_SECTION_NODE:
        xmlNodeSetContent(priv->node, new_value);
        break;

    default:
        return JS_FALSE;
    }

    return JS_TRUE;
}

/* --------------------------------------------------------------- */

#define DEFINE_RELATED_NODE_GETTER(c_name, node_member)                                 \
    static JSBool                                                                       \
    c_name##_getter(JSContext *cx, JSObject **obj, jsid *id, jsval *vp)                 \
    {                                                                                   \
        DOMNodePrivate *priv;                                                           \
        JSObject *retobj;                                                               \
        priv = priv_from_js(cx, *obj);                                                  \
                                                                                        \
        if (priv == NULL)                                                               \
            return JS_TRUE; /* prototype, not instance */                               \
                                                                                        \
        if (priv->node->node_member == NULL) {                                          \
            *vp = JSVAL_NULL;                                                           \
            return JS_TRUE;                                                             \
        }                                                                               \
                                                                                        \
        retobj = gjs_dom_wrap_xml_node (cx, (xmlNodePtr)priv->node->node_member);       \
                                                                                        \
        if (!retobj) {                                                                  \
            JS_ReportOutOfMemory(cx);                                                   \
            return JS_FALSE;                                                            \
        }                                                                               \
                                                                                        \
        *vp = OBJECT_TO_JSVAL(retobj);                                                  \
        return JS_TRUE;                                                                 \
    }

DEFINE_RELATED_NODE_GETTER (owner_document, doc)
DEFINE_RELATED_NODE_GETTER (parent_node, parent)
DEFINE_RELATED_NODE_GETTER (first_child, children)
DEFINE_RELATED_NODE_GETTER (last_child, last)
DEFINE_RELATED_NODE_GETTER (previous_sibling, prev)
DEFINE_RELATED_NODE_GETTER (next_sibling, next)

/* --------------------------------------------------------------- */

static JSPropertySpec gjs_dom_node_proto_props[] = {
    { "nodeType", 0, JSPROP_READONLY | JSPROP_PERMANENT, JSOP_WRAPPER((JSPropertyOp) node_type_getter), JSOP_NULLWRAPPER },
    { "nodeName", 0, JSPROP_READONLY | JSPROP_PERMANENT, JSOP_WRAPPER((JSPropertyOp) node_name_getter), JSOP_NULLWRAPPER },
    { "nodeValue", 0, JSPROP_PERMANENT, JSOP_WRAPPER((JSPropertyOp) node_value_getter), JSOP_NULLWRAPPER },
    { "ownerDocument", 0, JSPROP_READONLY | JSPROP_PERMANENT, JSOP_WRAPPER((JSPropertyOp) owner_document_getter), JSOP_NULLWRAPPER },
    { "parentNode", 0, JSPROP_READONLY | JSPROP_PERMANENT, JSOP_WRAPPER((JSPropertyOp) parent_node_getter), JSOP_NULLWRAPPER },
    { "firstChild", 0, JSPROP_READONLY | JSPROP_PERMANENT, JSOP_WRAPPER((JSPropertyOp) first_child_getter), JSOP_NULLWRAPPER },
    { "lastChild", 0, JSPROP_READONLY | JSPROP_PERMANENT, JSOP_WRAPPER((JSPropertyOp) last_child_getter), JSOP_NULLWRAPPER },
    { "previousSibling", 0, JSPROP_READONLY | JSPROP_PERMANENT, JSOP_WRAPPER((JSPropertyOp) previous_sibling_getter), JSOP_NULLWRAPPER },
    { "nextSibling", 0, JSPROP_READONLY | JSPROP_PERMANENT, JSOP_WRAPPER((JSPropertyOp) next_sibling_getter), JSOP_NULLWRAPPER },
    { NULL }
};

/* --------------------------------------------------------------- */

static JSBool
to_string_func(JSContext *cx, unsigned argc, jsval *vp)
{
    JSBool result = JS_TRUE;
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    DOMNodePrivate *priv = NULL;
    const gchar *name = NULL;
    gchar *u_str = NULL;
    JSString *str = NULL;

    priv = priv_from_js(cx, obj);

    if (!priv)
        return JS_TRUE; /* prototype, not instance */

    switch (priv->node->type) {
    case XML_ELEMENT_NODE: 
        name = "Element"; 
        break;

    case XML_TEXT_NODE: 
        name = "Text"; 
        break;

    case XML_CDATA_SECTION_NODE: 
        name = "CDATASection"; 
        break;

    case XML_DOCUMENT_NODE: 
        name = "Document"; 
        break;

    default: name = "Node";
    }

    u_str = g_strdup_printf ("[object %s]", name);
    if (!u_str) {
        result = JS_FALSE;
        JS_ReportOutOfMemory(cx);
        goto finish;
    }

    str = JS_NewStringCopyZ(cx, u_str);
    if (!str) {
        result = JS_FALSE;
        JS_ReportOutOfMemory(cx);
        goto finish;
    }

    JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(str));

finish:
    if (u_str) g_free(u_str);
    return JS_TRUE;
}

/* --------------------------------------------------------------- */

static JSBool
is_same_node_func(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    JSObject *other = NULL;
    DOMNodePrivate *priv = NULL;
    DOMNodePrivate *other_priv = NULL;
    
    priv = priv_from_js(cx, obj);
    
    if (!priv)
        return JS_TRUE; /* prototype, not instance */
    
    if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "o", &other))
        return JS_FALSE;
    
    other_priv = priv_from_js(cx, other);
    
    if (!other_priv) {
        gjs_throw(cx, "Not a DOMNode");
        return JS_FALSE;
    }

    JS_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(priv->node == other_priv->node ? JS_TRUE : JS_FALSE));
    return JS_TRUE;
}

/* --------------------------------------------------------------- */

static JSFunctionSpec gjs_dom_node_proto_funcs[] = {
    { "toString", JSOP_WRAPPER((JSNative) to_string_func), 0, 0 },
    { "isSameNode", JSOP_WRAPPER((JSNative) is_same_node_func), 1, 0 },
    { NULL }
};

/* --------------------------------------------------------------- */

static void 
node_finalize(JSContext *cx, JSObject *obj) {
    DOMNodePrivate *priv;
    priv = priv_from_js(cx, obj);

    g_print("Finalizer called");

    if (priv == NULL)
        return; /* prototype, not instance */

    priv->obj = NULL;
    DOM_DOC_PRIVATE(priv->node->doc->_private)->num_nodes_with_obj -= 1;

    /* If there are no more JavaScript objects referencing any nodes in the
       document, free it. */
    if (DOM_DOC_PRIVATE(priv->node->doc->_private)->num_nodes_with_obj == 0) {
        g_print("Freeing the document");
        xmlFreeDoc(priv->node->doc);
    }

    g_slice_free(DOMNodePrivate, priv);
}

/* --------------------------------------------------------------- */

static JSBool
gjs_create_dom_node_prototype (JSContext *cx)
{
    gjs_dom_node_prototype = JS_NewObject (cx, &gjs_dom_node_class, NULL, NULL);

    if (!gjs_dom_node_prototype)
        return JS_FALSE;

    if (!JS_DefineProperties(cx, gjs_dom_node_prototype, 
                             gjs_dom_node_proto_props)) 
        return JS_FALSE;

    if (!JS_DefineFunctions(cx, gjs_dom_node_prototype,
                            gjs_dom_node_proto_funcs))
        return JS_FALSE;

    return JS_TRUE;
}

/* ========================================================================= */
/* Element                                                                   */
/* ========================================================================= */

static JSBool
tag_name_getter(JSContext *cx, JSObject **obj, jsid *id, jsval *vp)
{
    DOMNodePrivate *priv;
    JSString *str;
    priv = priv_from_js(cx, *obj);

    if (priv == NULL)
        return JS_TRUE; /* prototype, not instance */

    if (priv->node->type != XML_ELEMENT_NODE) {
        gjs_throw(cx, "Must be an element node");
        return JS_FALSE;
    }

    str = JS_NewStringCopyZ(cx, priv->node->name);
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSPropertySpec gjs_dom_element_proto_props[] = {
    { "tagName", 0, JSPROP_READONLY | JSPROP_PERMANENT, JSOP_WRAPPER((JSPropertyOp) tag_name_getter), JSOP_NULLWRAPPER },
    { NULL }
};

/* --------------------------------------------------------------- */

static JSBool
get_attribute_func(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    JSString *name = NULL;
    JSString *out = NULL;
    char *u_name = NULL;
    xmlChar *value;
    DOMNodePrivate *priv = NULL;
    JSBool result = JS_TRUE;

    priv = priv_from_js(cx, obj);

    if (!priv)
        goto finish; /* prototype, not instance */

    if (priv->node->type != XML_ELEMENT_NODE)
        goto finish;

    if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "S", &name)) {
        result = JS_FALSE;
        goto finish;
    }

    u_name = JS_EncodeString(cx, name);
    
    if (u_name == NULL) {
        JS_ReportOutOfMemory(cx);
        result = JS_FALSE;
        goto finish;
    }

    value = xmlGetNoNsProp(priv->node, (const xmlChar *)u_name);

    if (!value) {
        *vp = JSVAL_NULL;
        goto finish;
    }

    out = JS_NewStringCopyZ(cx, value);
    
    if (!out) {
        JS_ReportOutOfMemory(cx);
        result = JS_FALSE;
        goto finish;
    }
    
    *vp = STRING_TO_JSVAL(out);

finish:
    if (u_name) JS_free(cx, u_name);
    return result;
}

/* --------------------------------------------------------------- */

static JSBool
get_attribute_ns_func(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    JSString *name = NULL;
    JSString *ns = NULL;
    JSString *out = NULL;
    char *u_name = NULL;
    char *u_ns = NULL;
    xmlChar *value;
    DOMNodePrivate *priv = NULL;
    JSBool result = JS_TRUE;

    priv = priv_from_js(cx, obj);

    if (!priv)
        goto finish; /* prototype, not instance */

    if (priv->node->type != XML_ELEMENT_NODE)
        goto finish;

    if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "SS", &name, &ns)) {
        result = JS_FALSE;
        goto finish;
    }

    u_name = JS_EncodeString(cx, name);
    u_ns = JS_EncodeString(cx, ns);
    
    if (!u_name || !u_ns) {
        JS_ReportOutOfMemory(cx);
        result = JS_FALSE;
        goto finish;
    }

    value = xmlGetNsProp(priv->node, (const xmlChar *)u_name, (const xmlChar *)u_ns);

    if (!value) {
        *vp = JSVAL_NULL;
        goto finish;
    }

    out = JS_NewStringCopyZ(cx, value);
    
    if (!out) {
        JS_ReportOutOfMemory(cx);
        result = JS_FALSE;
        goto finish;
    }
    
    *vp = STRING_TO_JSVAL(out);

finish:
    if (u_name) JS_free(cx, u_name);
    if (u_ns) JS_free(cx, u_ns);
    return result;
}

/* --------------------------------------------------------------- */

static JSFunctionSpec gjs_dom_element_proto_funcs[] = {
    { "getAttribute", JSOP_WRAPPER((JSNative) get_attribute_func), 1, 0 },
    { "getAttributeNS", JSOP_WRAPPER((JSNative) get_attribute_ns_func), 1, 0 },
    { NULL }
};

/* --------------------------------------------------------------- */

static JSBool
gjs_create_dom_element_prototype (JSContext *cx)
{
    gjs_dom_element_prototype = JS_NewObject (cx, &gjs_dom_node_class, 
                                              gjs_dom_node_prototype, NULL);

    if (!gjs_dom_node_prototype)
        return JS_FALSE;

    if (!JS_DefineProperties(cx, gjs_dom_element_prototype, 
                             gjs_dom_element_proto_props)) 
        return JS_FALSE;

    if (!JS_DefineFunctions(cx, gjs_dom_element_prototype,
                            gjs_dom_element_proto_funcs))
        return JS_FALSE;

    return JS_TRUE;
}

/* ========================================================================= */

JSBool
gjs_js_define_dom_node_stuff (JSContext *cx, JSObject *module)
{
    jsval v;

    if (!gjs_create_dom_node_prototype (cx))
        return JS_FALSE;

    if (!gjs_create_dom_element_prototype (cx))
        return JS_FALSE;

    #define DEFINE_NUM(name, n) \
        v = INT_TO_JSVAL(n); \
        if (!JS_SetProperty(cx, module, #name, &v)) \
            return JS_FALSE;

    DEFINE_NUM(ELEMENT_NODE, 1)
    DEFINE_NUM(ATTRIBUTE_NODE, 2)
    DEFINE_NUM(TEXT_NODE, 3)
    DEFINE_NUM(CDATA_SECTION_NODE, 4)
    DEFINE_NUM(ENTITY_REFERENCE_NODE, 5)
    DEFINE_NUM(ENTITY_NODE, 5)
    DEFINE_NUM(PROCESSING_INSTRUCTION_NODE, 7)
    DEFINE_NUM(COMMENT_NODE, 8)
    DEFINE_NUM(DOCUMENT_NODE, 9)
    DEFINE_NUM(DOCUMENT_TYPE_NODE, 10)
    DEFINE_NUM(DOCUMENT_FRAGMENT_NODE, 11)
    DEFINE_NUM(NOTATION_NODE, 12)

    return JS_TRUE;
}

JSObject *
gjs_dom_wrap_xml_node (JSContext *cx, xmlNodePtr node)
{
    JSObject *obj = NULL;
    JSObject *proto = gjs_dom_node_prototype;
    DOMNodePrivate *priv;

    /* If it's already wrapped, return the existing wrapper */
    if (node->_private && DOM_NODE_PRIVATE(node->_private)->obj != NULL)
        return DOM_NODE_PRIVATE(node->_private)->obj;

    /* Pick the right prototype */
    if (node->type == XML_ELEMENT_NODE)
        proto = gjs_dom_element_prototype;

    /* Create the object */
    obj = JS_NewObject(cx, &gjs_dom_node_class, proto, NULL);
    if (obj == NULL)
        return NULL;

    /* Create private for the node if it doesn't have one already */
    if (node->_private == NULL) {
        if (IS_NODE_DOCUMENT(node)) {
            priv = g_slice_new(DOMDocPrivate);
            DOM_DOC_PRIVATE(priv)->num_nodes_with_obj = 1;
        } else {
            priv = g_slice_new(DOMNodePrivate);
        }

        priv->node = node;
        node->_private = priv;
    } else {
        priv = node->_private;
    }

    priv->obj = obj;
    JS_SetPrivate(obj, priv);

    return obj;
}

