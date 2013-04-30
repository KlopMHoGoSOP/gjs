#include "xml.h"
#include "dom-node.h"
#include "dom-parser.h"

JSBool
gjs_js_define_xml_stuff (JSContext *context, JSObject *module)
{
    if (!gjs_js_define_dom_node_stuff (context, module))
        return JS_FALSE;

    if (!gjs_js_define_dom_parser_stuff (context, module))
        return JS_FALSE;

    return JS_TRUE;
}
