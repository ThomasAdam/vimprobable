/*
    (c) 2009 by Leon Winter
    (c) 2009 by Hannes Schueller
    see LICENSE file
*/

function clearfocus() {
    if(document.activeElement && document.activeElement.blur)
        document.activeElement.blur();
}

function v(e, y) {
    t = e.nodeName.toLowerCase();
    if((t == 'input' && /^(text|password)$/.test(e.type))
    || /^(select|textarea)$/.test(t)
    || e.contentEditable == 'true')
        console.log('insertmode_'+(y=='focus'?'on':'off'));
}

if(document.activeElement)
    v(document.activeElement,'focus');

m=['focus','blur'];

for(i in m)
    document.getElementsByTagName('body')[0].addEventListener(m[i], function(x) {
        v(x.target,x.type);
    }, true);

document.getElementsByTagName("body")[0].appendChild(document.createElement("style"));
document.styleSheets[0].addRule('.hinting_mode_hint', 'color: #000; background: #ff0');
document.styleSheets[0].addRule('.hinting_mode_hint_focus', 'color: #000; background: #8f0');

self.onunload = function() {
    v(document.activeElement, '');
};

function show_hints() {
    var height = window.innerHeight;
    var width = window.innerWidth;
    var scrollX = document.defaultView.scrollX;
    var scrollY = document.defaultView.scrollY;
    /* prefixing html: will result in namespace error */
    var hinttags = "//*[@onclick or @onmouseover or @onmousedown or @onmouseup or @oncommand or @class='lk' or @role='link' or @href] | //input[not(@type='hidden')] | //a | //area | //iframe | //textarea | //button | //select";

    /* iterator type isn't suitable here, because: "DOMException NVALID_STATE_ERR: The document has been mutated since the result was returned." */
    var r = document.evaluate(hinttags, document,
        function(p) {
            return 'http://www.w3.org/1999/xhtml';
        }, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);
    div = document.createElement("div");
    /* due to the different XPath result type, we will need two counter variables */
    j = 1;
    var i;
    a = [];
    for (i = 0; i < r.snapshotLength; i++)
    {
        var elem = r.snapshotItem(i);
        rect = elem.getBoundingClientRect();
        if (!rect || rect.top > height || rect.bottom < 0 || rect.left > width || rect.right < 0 || !(elem.getClientRects()[0]))
            continue;
        var computedStyle = document.defaultView.getComputedStyle(elem, null);
        if (computedStyle.getPropertyValue("visibility") != "visible" || computedStyle.getPropertyValue("display") == "none")
            continue;
        var leftpos = Math.max((rect.left + scrollX), scrollX);
        var toppos = Math.max((rect.top + scrollY), scrollY);
        a.push(elem);
        /* making this block DOM compliant */
        var hint = document.createElement("span");
        hint.setAttribute("class", "hinting_mode_hint");
        hint.setAttribute("id", "vimprobablehint" + j);
        hint.style.position = "absolute";
        hint.style.left = leftpos + "px";
        hint.style.top =  toppos + "px";
        hint.style.background = "red";
        hint.style.color = "#fff";
        hint.style.font = "bold 10px monospace";
        hint.style.zIndex = "99";
        var text = document.createTextNode(j);
        hint.appendChild(text);
        div.appendChild(hint);
        j++;
    }
    i = 0;
    while (typeof(a[i]) != "undefined") {
        a[i].className += " hinting_mode_hint";
        i++;
    }
    document.getElementsByTagName("body")[0].appendChild(div);
    clearfocus();
    h = null;
}
function fire(n)
{
    if (typeof(a[n - 1]) != "undefined") {
        el = a[n - 1];
        tag = el.nodeName.toLowerCase();
        clear();
        if(tag == "iframe" || tag == "frame" || tag == "textarea" || (tag == "input" && el.type == "text"))
            el.focus();
        else {
            var evObj = document.createEvent('MouseEvents');
            evObj.initMouseEvent('click', true, true, window, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, null);
            el.dispatchEvent(evObj);
        }
    }
}
function cleanup()
{
    for(e in a)
        a[e].className = a[e].className.replace(/hinting_mode_hint/,'');
    div.parentNode.removeChild(div);
    window.onkeyup = null;
}
function clear()
{
    cleanup();
    console.log("hintmode_off")
}

function update_hints(n) 
{
    if(h != null)
        h.className = h.className.replace("_focus","");
    if (j - 1 < n * 10 && typeof(a[n - 1]) != "undefined") {
        fire(n);
        return "fired";
    } else
        if (typeof(a[n - 1]) != "undefined")
            (h = a[n - 1]).className = a[n - 1].className.replace("hinting_mode_hint", "hinting_mode_hint_focus");
}

