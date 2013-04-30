// application/javascript;version=1.8
const Xml = imports.xml;

var JSUnit = {
    assertEquals: function(a, b) {
        print(a + ' == ' + b + ' : ' + (a === b));
    }
}

function testDOMBasic() {
    let parser = new Xml.DOMParser();
    let document = parser.parseFromString(
        '<doctor xmlns:ultramed="http://ultramed.com/xml/500BC/ultramed">'+
        '  prescribed  '+
        '<![CDATA[ extra ]]>'+
        '<medicine name="Mega Pills" type="pills" ultramed:effectiveness="mega"/>'+
        '</doctor>',
        
        'text/xml');

    JSUnit.assertEquals('#document', document.nodeName);
    JSUnit.assertEquals(null, document.nodeValue);

    let doctor = document.firstChild;
    
    JSUnit.assertEquals(Xml.ELEMENT_NODE, doctor.nodeType);
    JSUnit.assertEquals(true, document.isSameNode(doctor.parentNode));
    JSUnit.assertEquals(document, doctor.parentNode);
    JSUnit.assertEquals('doctor', doctor.nodeName);
    JSUnit.assertEquals('doctor', doctor.tagName);

    let prescribed = doctor.firstChild;

    JSUnit.assertEquals(Xml.TEXT_NODE, prescribed.nodeType);
    JSUnit.assertEquals(doctor, prescribed.parentNode);
    JSUnit.assertEquals('#text', prescribed.nodeName);
    JSUnit.assertEquals('  prescribed  ', prescribed.nodeValue);

    let extra = prescribed.nextSibling;

    JSUnit.assertEquals(Xml.CDATA_SECTION_NODE, extra.nodeType);
    JSUnit.assertEquals(doctor, extra.parentNode);
    JSUnit.assertEquals('#cdata-section', extra.nodeName);
    JSUnit.assertEquals(' extra ', extra.nodeValue);

    let medicine = extra.nextSibling;

    JSUnit.assertEquals(Xml.ELEMENT_NODE, medicine.nodeType);
    JSUnit.assertEquals(doctor, medicine.parentNode);
    JSUnit.assertEquals('medicine', medicine.nodeName);
    JSUnit.assertEquals('medicine', medicine.tagName);
    JSUnit.assertEquals('Mega Pills', medicine.getAttribute('name'));
    JSUnit.assertEquals('pills', medicine.getAttribute('type'));
    JSUnit.assertEquals('mega', medicine.getAttributeNS('effectiveness', 'http://ultramed.com/xml/500BC/ultramed'));
}

testDOMBasic();
